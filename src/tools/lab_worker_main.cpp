#include "faultmine/lab_worker.hpp"
#include "faultmine/wic_io.hpp"

#include <windows.h>
#include <winternl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using faultmine::laboratory::WorkerMode;

void append_u32(std::vector<std::uint8_t>& output, const std::uint32_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U) output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
}

void append_u64(std::vector<std::uint8_t>& output, const std::uint64_t value) {
    for (unsigned shift = 0U; shift < 64U; shift += 8U) output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
}

[[nodiscard]] bool read_u32(const std::vector<std::uint8_t>& input, std::size_t& offset, std::uint32_t& output) noexcept {
    if (offset > input.size() || input.size() - offset < 4U) return false;
    output = 0U;
    for (unsigned index = 0U; index < 4U; ++index) output |= static_cast<std::uint32_t>(input[offset + index]) << (index * 8U);
    offset += 4U;
    return true;
}

[[nodiscard]] bool read_u64(const std::vector<std::uint8_t>& input, std::size_t& offset, std::uint64_t& output) noexcept {
    if (offset > input.size() || input.size() - offset < 8U) return false;
    output = 0U;
    for (unsigned index = 0U; index < 8U; ++index) output |= static_cast<std::uint64_t>(input[offset + index]) << (index * 8U);
    offset += 8U;
    return true;
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> read_bytes(
    const std::filesystem::path& path,
    const std::uint64_t maximum_bytes) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size > maximum_bytes || size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) return std::nullopt;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return std::nullopt;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty()) return std::nullopt;
    return bytes;
}

[[nodiscard]] bool write_bytes(const std::filesystem::path& path, const std::span<const std::uint8_t> bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    if (!bytes.empty()) stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] std::string os_build_text() {
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) return "windows-build-unavailable";
    using RtlGetVersionFunction = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto function = reinterpret_cast<RtlGetVersionFunction>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (function == nullptr) return "windows-build-unavailable";
    RTL_OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (function(&info) != 0) return "windows-build-unavailable";
    return "Windows " + std::to_string(info.dwMajorVersion) + "." + std::to_string(info.dwMinorVersion) +
        " build " + std::to_string(info.dwBuildNumber);
}

[[nodiscard]] bool write_response(
    const std::filesystem::path& response_path,
    const std::filesystem::path& pixel_path,
    const std::uint32_t outcome,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint64_t stride,
    const std::uint64_t pixel_bytes,
    const std::string_view diagnostic,
    const std::string_view decoder,
    const std::string_view os,
    const std::span<const std::uint8_t> pixels) {
    if (!pixels.empty() && !write_bytes(pixel_path, pixels)) return false;
    std::vector<std::uint8_t> response{'F','M','L','A','B','R','S','1'};
    append_u32(response, faultmine::laboratory::kLabWorkerProtocolVersion);
    append_u32(response, 2U);
    append_u32(response, outcome);
    append_u32(response, width);
    append_u32(response, height);
    append_u64(response, stride);
    append_u64(response, pixel_bytes);
    append_u32(response, static_cast<std::uint32_t>(diagnostic.size()));
    append_u32(response, static_cast<std::uint32_t>(decoder.size()));
    append_u32(response, static_cast<std::uint32_t>(os.size()));
    response.insert(response.end(), diagnostic.begin(), diagnostic.end());
    response.insert(response.end(), decoder.begin(), decoder.end());
    response.insert(response.end(), os.begin(), os.end());
    return write_bytes(response_path, response);
}

struct Arguments {
    std::filesystem::path request;
    std::filesystem::path input;
    std::filesystem::path response;
    std::filesystem::path pixels;
};

[[nodiscard]] std::optional<Arguments> parse_arguments(const int argc, wchar_t** argv) {
    Arguments result;
    for (int index = 1; index + 1 < argc; index += 2) {
        const std::wstring_view name{argv[index]};
        if (name == L"--request") result.request = argv[index + 1];
        else if (name == L"--input") result.input = argv[index + 1];
        else if (name == L"--response") result.response = argv[index + 1];
        else if (name == L"--pixels") result.pixels = argv[index + 1];
        else return std::nullopt;
    }
    if (result.request.empty() || result.input.empty() || result.response.empty() || result.pixels.empty()) return std::nullopt;
    return result;
}

}  // namespace

int wmain(const int argc, wchar_t** argv) {
    const auto arguments = parse_arguments(argc, argv);
    if (!arguments.has_value()) return 2;
    auto request = read_bytes(arguments->request, 4096U);
    if (!request.has_value()) return 3;
    constexpr std::array<std::uint8_t, 8> kMagic{'F','M','L','A','B','R','Q','1'};
    if (request->size() < kMagic.size() || !std::equal(kMagic.begin(), kMagic.end(), request->begin())) return 4;
    std::size_t offset = kMagic.size();
    std::uint32_t version{};
    std::uint32_t type{};
    std::uint32_t raw_mode{};
    std::uint64_t encoded_length{};
    if (!read_u32(*request, offset, version) || !read_u32(*request, offset, type) || !read_u32(*request, offset, raw_mode) ||
        !read_u64(*request, offset, encoded_length) || offset != request->size() ||
        version != faultmine::laboratory::kLabWorkerProtocolVersion || type != 1U) {
        return 5;
    }
    auto encoded = read_bytes(arguments->input, faultmine::laboratory::kLabWorkerMaximumEncodedBytes);
    if (!encoded.has_value() || encoded->size() != encoded_length) return 6;
    const WorkerMode mode = static_cast<WorkerMode>(raw_mode);
    const std::string os = os_build_text();
    const std::string decoder = "Windows Imaging Component (external decoder; pixels non-canonical until frozen)";

    if (mode == WorkerMode::synthetic_crash) return 73;
    if (mode == WorkerMode::synthetic_hang) {
        Sleep(INFINITE);
        return 74;
    }
    if (mode == WorkerMode::synthetic_malformed_header) {
        const std::array<std::uint8_t, 5> bad{'B','A','D','!','!'};
        return write_bytes(arguments->response, bad) ? 0 : 7;
    }
    if (mode == WorkerMode::synthetic_oversized_metadata) {
        std::vector<std::uint8_t> response{'F','M','L','A','B','R','S','1'};
        append_u32(response, faultmine::laboratory::kLabWorkerProtocolVersion);
        append_u32(response, 2U);
        append_u32(response, 0U);
        append_u32(response, 2U);
        append_u32(response, 2U);
        append_u64(response, 8U);
        append_u64(response, 16U);
        append_u32(response, faultmine::laboratory::kLabWorkerMaximumDiagnosticBytes + 1U);
        append_u32(response, 0U);
        append_u32(response, 0U);
        return write_bytes(arguments->response, response) ? 0 : 8;
    }
    if (mode == WorkerMode::synthetic_decode_failure) {
        return write_response(arguments->response, arguments->pixels, 1U, 0U, 0U, 0U, 0U,
            "synthetic decoder failure", "synthetic-decoder", os, {}) ? 0 : 9;
    }

    if (mode == WorkerMode::synthetic_success || mode == WorkerMode::synthetic_invalid_dimensions ||
        mode == WorkerMode::synthetic_invalid_stride || mode == WorkerMode::synthetic_invalid_byte_count) {
        std::array<std::uint8_t, 16> pixels{};
        for (std::size_t index = 0U; index < pixels.size(); ++index) {
            pixels[index] = (*encoded)[index % encoded->size()];
        }
        const std::uint32_t width = mode == WorkerMode::synthetic_invalid_dimensions ? 0U : 2U;
        const std::uint32_t height = 2U;
        const std::uint64_t stride = mode == WorkerMode::synthetic_invalid_stride ? 7U : 8U;
        const std::uint64_t declared = mode == WorkerMode::synthetic_invalid_byte_count ? 15U : 16U;
        return write_response(arguments->response, arguments->pixels, 0U, width, height, stride, declared,
            "synthetic success path", "synthetic-decoder", os, pixels) ? 0 : 10;
    }

    if (mode != WorkerMode::decode_wic) return 11;
    faultmine::io::WicLoadResult loaded = faultmine::io::load_wic_image(arguments->input);
    if (!loaded.ok()) {
        const std::string diagnostic = loaded.error.has_value() ? loaded.error->message : "external WIC decode failed";
        return write_response(arguments->response, arguments->pixels, 1U, 0U, 0U, 0U, 0U,
            diagnostic, decoder, os, {}) ? 0 : 12;
    }
    const faultmine::core::ImageBuffer& image = loaded.source->image;
    return write_response(arguments->response, arguments->pixels, 0U, image.width, image.height, image.row_stride,
        image.bytes.size(), "decode completed; result remains external-decoder dependent until materialized",
        decoder, os, image.bytes) ? 0 : 13;
}
