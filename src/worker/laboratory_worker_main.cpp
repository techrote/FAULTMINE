#include "faultmine/laboratory.hpp"
#include "faultmine/wic_io.hpp"
#include "../io/laboratory_protocol.hpp"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using faultmine::laboratory::Outcome;
using faultmine::laboratory::WorkerSyntheticMode;
namespace protocol = faultmine::laboratory::protocol;

[[nodiscard]] bool read_bytes(
    const std::filesystem::path& path,
    const std::size_t maximum,
    std::vector<std::uint8_t>& output) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size > maximum) return false;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    output.resize(static_cast<std::size_t>(size));
    if (!output.empty()) stream.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(output.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] bool write_bytes(
    const std::filesystem::path& path,
    const std::span<const std::uint8_t> bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    if (!bytes.empty()) stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.flush();
    return static_cast<bool>(stream);
}

[[nodiscard]] std::string windows_metadata() {
    using RtlGetVersionFunction = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE module = GetModuleHandleW(L"ntdll.dll");
    if (module == nullptr) return "Windows";
    const auto function = reinterpret_cast<RtlGetVersionFunction>(GetProcAddress(module, "RtlGetVersion"));
    if (function == nullptr) return "Windows";
    OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (function(&version) != 0) return "Windows";
    return "Windows " + std::to_string(version.dwMajorVersion) + "." +
        std::to_string(version.dwMinorVersion) + " build " + std::to_string(version.dwBuildNumber);
}

[[nodiscard]] bool emit_response(
    const std::filesystem::path& response_path,
    const protocol::ResponseRecord& response) {
    const std::vector<std::uint8_t> bytes = protocol::encode_response(response);
    return write_bytes(response_path, bytes);
}

[[nodiscard]] std::filesystem::path argument_path(
    const int argc,
    wchar_t** argv,
    const std::wstring_view name) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (argv[index] == name) return argv[index + 1];
    }
    return {};
}

int run_synthetic(
    const WorkerSyntheticMode mode,
    const std::filesystem::path& response_path,
    const std::filesystem::path& pixels_path) {
    protocol::ResponseRecord response;
    response.os_metadata = windows_metadata();
    response.decoder_metadata = "FAULTMINE synthetic worker mode";
    switch (mode) {
        case WorkerSyntheticMode::success: {
            const std::vector<std::uint8_t> pixels{
                1U, 2U, 3U, 255U, 4U, 5U, 6U, 255U,
                7U, 8U, 9U, 255U, 10U, 11U, 12U, 255U};
            if (!write_bytes(pixels_path, pixels)) return 31;
            response.outcome = Outcome::success;
            response.width = 2U;
            response.height = 2U;
            response.row_stride = 8U;
            response.pixel_payload_length = pixels.size();
            response.diagnostic = "synthetic success";
            return emit_response(response_path, response) ? 0 : 32;
        }
        case WorkerSyntheticMode::decode_failure:
            response.outcome = Outcome::decode_failure;
            response.diagnostic = "synthetic decode failure";
            return emit_response(response_path, response) ? 0 : 33;
        case WorkerSyntheticMode::crash:
            return 73;
        case WorkerSyntheticMode::hang:
            Sleep(INFINITE);
            return 74;
        case WorkerSyntheticMode::oversized_payload:
            response.outcome = Outcome::success;
            response.width = 1U;
            response.height = 1U;
            response.row_stride = 4U;
            response.pixel_payload_length = faultmine::laboratory::kMaxLaboratoryPixelBytes + 1ULL;
            response.diagnostic = "synthetic oversized payload metadata";
            return emit_response(response_path, response) ? 0 : 35;
        case WorkerSyntheticMode::malformed_response: {
            const std::vector<std::uint8_t> malformed{'b', 'a', 'd'};
            return write_bytes(response_path, malformed) ? 0 : 36;
        }
        case WorkerSyntheticMode::invalid_dimensions:
            response.outcome = Outcome::success;
            response.width = 0U;
            response.height = 2U;
            response.row_stride = 0U;
            response.pixel_payload_length = 0U;
            response.diagnostic = "synthetic invalid dimensions";
            return emit_response(response_path, response) ? 0 : 37;
        case WorkerSyntheticMode::invalid_stride:
            response.outcome = Outcome::success;
            response.width = 2U;
            response.height = 1U;
            response.row_stride = 3U;
            response.pixel_payload_length = 8U;
            response.diagnostic = "synthetic invalid stride";
            return emit_response(response_path, response) ? 0 : 38;
        case WorkerSyntheticMode::invalid_byte_count:
            response.outcome = Outcome::success;
            response.width = 2U;
            response.height = 2U;
            response.row_stride = 8U;
            response.pixel_payload_length = 15U;
            response.diagnostic = "synthetic invalid byte count";
            return emit_response(response_path, response) ? 0 : 39;
        case WorkerSyntheticMode::none:
            return 40;
    }
    return 41;
}

}  // namespace

int wmain(const int argc, wchar_t** argv) {
    const std::filesystem::path request_path = argument_path(argc, argv, L"--request");
    const std::filesystem::path response_path = argument_path(argc, argv, L"--response");
    const std::filesystem::path pixels_path = argument_path(argc, argv, L"--pixels");
    if (request_path.empty() || response_path.empty() || pixels_path.empty()) {
        std::wcerr << L"FAULTMINE-lab-worker requires --request, --response and --pixels.\n";
        return 2;
    }

    std::vector<std::uint8_t> request_bytes;
    if (!read_bytes(
            request_path,
            protocol::kRequestHeaderBytes + faultmine::laboratory::kMaxLaboratoryEncodedBytes,
            request_bytes)) {
        return 3;
    }
    protocol::RequestRecord request;
    std::string protocol_error;
    if (!protocol::decode_request(request_bytes, request, protocol_error)) {
        protocol::ResponseRecord rejected;
        rejected.outcome = Outcome::rejected_response;
        rejected.diagnostic = "request rejected: " + protocol_error;
        rejected.os_metadata = windows_metadata();
        rejected.decoder_metadata = "not invoked";
        return emit_response(response_path, rejected) ? 0 : 4;
    }

    if (request.synthetic_mode != WorkerSyntheticMode::none) {
        return run_synthetic(request.synthetic_mode, response_path, pixels_path);
    }

    const std::filesystem::path encoded_path = request_path.parent_path() / L"encoded-input.bin";
    if (!write_bytes(encoded_path, request.encoded_bytes)) return 5;

    protocol::ResponseRecord response;
    response.os_metadata = windows_metadata();
    response.decoder_metadata = "Windows Imaging Component (WIC)";
    faultmine::io::WicLoadResult decoded = faultmine::io::load_wic_image(encoded_path);
    if (!decoded.ok()) {
        response.outcome = Outcome::decode_failure;
        response.diagnostic = decoded.error.has_value() ? decoded.error->message : "WIC decode failed without detail";
        return emit_response(response_path, response) ? 0 : 6;
    }
    if (const auto validation = faultmine::core::validate_canonical_image(decoded.source->image); validation.has_value()) {
        response.outcome = Outcome::rejected_response;
        response.diagnostic = "decoded image failed canonical validation: " + validation->message;
        return emit_response(response_path, response) ? 0 : 7;
    }
    if (decoded.source->image.bytes.size() > faultmine::laboratory::kMaxLaboratoryPixelBytes) {
        response.outcome = Outcome::rejected_response;
        response.diagnostic = "decoded image exceeds laboratory pixel byte limit";
        return emit_response(response_path, response) ? 0 : 8;
    }
    if (!write_bytes(pixels_path, decoded.source->image.bytes)) return 9;
    response.outcome = Outcome::success;
    response.width = decoded.source->image.width;
    response.height = decoded.source->image.height;
    response.row_stride = decoded.source->image.row_stride;
    response.pixel_payload_length = decoded.source->image.bytes.size();
    response.diagnostic = "external decoder returned normalized pixels; result is decoder-dependent until materialized";
    return emit_response(response_path, response) ? 0 : 10;
}
