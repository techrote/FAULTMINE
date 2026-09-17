#include "faultmine/lab_worker.hpp"

#include "faultmine/image.hpp"

#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace faultmine::laboratory {
namespace {

class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~UniqueHandle() { reset(); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }
    [[nodiscard]] HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] explicit operator bool() const noexcept { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }
    void reset(HANDLE replacement = nullptr) noexcept {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
        handle_ = replacement;
    }
private:
    HANDLE handle_{};
};

struct TempFiles {
    std::filesystem::path request;
    std::filesystem::path input;
    std::filesystem::path response;
    std::filesystem::path pixels;
    ~TempFiles() {
        std::error_code ignored;
        std::filesystem::remove(request, ignored);
        std::filesystem::remove(input, ignored);
        std::filesystem::remove(response, ignored);
        std::filesystem::remove(pixels, ignored);
    }
};

[[nodiscard]] std::filesystem::path unique_temp_stem() {
    std::array<wchar_t, MAX_PATH + 1U> buffer{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    std::filesystem::path directory = length != 0U && length < buffer.size()
        ? std::filesystem::path{buffer.data()}
        : std::filesystem::temp_directory_path();
    static std::atomic<std::uint64_t> counter{0U};
    const std::uint64_t ordinal = counter.fetch_add(1U, std::memory_order_relaxed);
    return directory / (L"faultmine-lab-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(ordinal));
}

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

[[nodiscard]] bool write_bytes(const std::filesystem::path& path, const std::span<const std::uint8_t> bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    if (!bytes.empty()) stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
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

[[nodiscard]] std::wstring quote(const std::filesystem::path& path) {
    return L"\"" + path.wstring() + L"\"";
}

[[nodiscard]] WorkerResult rejected(std::string diagnostic) {
    WorkerResult result;
    result.outcome = core::LaboratoryOutcome::rejected_response;
    result.diagnostic = std::move(diagnostic);
    return result;
}

[[nodiscard]] WorkerResult parse_response(const TempFiles& paths, const DWORD exit_code) {
    auto response = read_bytes(paths.response, 128ULL * 1024ULL);
    if (!response.has_value()) return rejected("worker response is missing, unreadable, or exceeds the metadata bound");
    constexpr std::array<std::uint8_t, 8> kMagic{'F','M','L','A','B','R','S','1'};
    if (response->size() < kMagic.size() || !std::equal(kMagic.begin(), kMagic.end(), response->begin())) {
        return rejected("worker response header magic is invalid");
    }
    std::size_t offset = kMagic.size();
    std::uint32_t version{};
    std::uint32_t type{};
    std::uint32_t outcome{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint64_t stride{};
    std::uint64_t pixel_bytes{};
    std::uint32_t diagnostic_length{};
    std::uint32_t decoder_length{};
    std::uint32_t os_length{};
    if (!read_u32(*response, offset, version) || !read_u32(*response, offset, type) || !read_u32(*response, offset, outcome) ||
        !read_u32(*response, offset, width) || !read_u32(*response, offset, height) || !read_u64(*response, offset, stride) ||
        !read_u64(*response, offset, pixel_bytes) || !read_u32(*response, offset, diagnostic_length) ||
        !read_u32(*response, offset, decoder_length) || !read_u32(*response, offset, os_length)) {
        return rejected("worker response header is truncated");
    }
    if (version != kLabWorkerProtocolVersion || type != 2U) return rejected("worker response protocol version/type is unsupported");
    if (diagnostic_length > kLabWorkerMaximumDiagnosticBytes || decoder_length > 4096U || os_length > 4096U) {
        return rejected("worker response metadata length exceeds policy bounds");
    }
    const std::uint64_t metadata_total = static_cast<std::uint64_t>(diagnostic_length) + decoder_length + os_length;
    if (metadata_total > response->size() - offset || offset + static_cast<std::size_t>(metadata_total) != response->size()) {
        return rejected("worker response declared metadata lengths do not match the message length");
    }
    WorkerResult result;
    result.exit_code = static_cast<std::uint32_t>(exit_code);
    const auto take = [&](const std::uint32_t length) {
        std::string text(reinterpret_cast<const char*>(response->data() + offset), length);
        offset += length;
        return text;
    };
    result.diagnostic = take(diagnostic_length);
    result.decoder_identifier = take(decoder_length);
    result.os_build = take(os_length);

    if (outcome == 1U) {
        result.outcome = core::LaboratoryOutcome::decode_failure;
        return result;
    }
    if (outcome != 0U) return rejected("worker response outcome code is invalid");
    if (pixel_bytes > kLabWorkerMaximumPixelBytes) return rejected("worker pixel payload exceeds the host bound");
    if (width == 0U || height == 0U) return rejected("worker returned zero image dimensions");
    const auto expected = core::canonical_rgba8_byte_size(width, height);
    if (!expected.has_value() || pixel_bytes != *expected || stride != static_cast<std::uint64_t>(width) * 4U) {
        return rejected("worker returned invalid dimensions, stride, or canonical pixel byte count");
    }
    auto pixels = read_bytes(paths.pixels, kLabWorkerMaximumPixelBytes);
    if (!pixels.has_value() || pixels->size() != *expected) return rejected("worker pixel file length does not match validated response metadata");
    auto image = core::make_rgba8_image(width, height, *pixels);
    if (!image.ok()) return rejected("worker pixel payload could not be normalized into a canonical image");
    result.image = std::move(*image.image);
    result.outcome = core::LaboratoryOutcome::success;
    return result;
}

}  // namespace

WorkerResult run_lab_worker(const WorkerRequest& request) {
    if (request.worker_executable.empty() || request.encoded_bytes.empty()) return rejected("worker executable and encoded input are required");
    if (request.encoded_bytes.size() > kLabWorkerMaximumEncodedBytes) return rejected("encoded input exceeds the worker request bound");
    if (request.timeout_ms == 0U) return rejected("worker timeout must be positive");
    if (request.process_memory_limit_bytes == 0U || request.process_memory_limit_bytes > static_cast<std::uint64_t>(std::numeric_limits<SIZE_T>::max())) {
        return rejected("worker memory limit is invalid for this host");
    }

    const std::filesystem::path stem = unique_temp_stem();
    TempFiles paths{stem.wstring() + L".request", stem.wstring() + L".input", stem.wstring() + L".response", stem.wstring() + L".pixels"};
    if (!write_bytes(paths.input, request.encoded_bytes)) return rejected("could not materialize bounded worker input file");

    std::vector<std::uint8_t> message{'F','M','L','A','B','R','Q','1'};
    append_u32(message, kLabWorkerProtocolVersion);
    append_u32(message, 1U);
    append_u32(message, static_cast<std::uint32_t>(request.mode));
    append_u64(message, request.encoded_bytes.size());
    if (!write_bytes(paths.request, message)) return rejected("could not write worker request control message");

    std::wstring command = quote(request.worker_executable) + L" --request " + quote(paths.request) +
        L" --input " + quote(paths.input) + L" --response " + quote(paths.response) + L" --pixels " + quote(paths.pixels);
    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');

    UniqueHandle job{CreateJobObjectW(nullptr, nullptr)};
    if (!job) return rejected("CreateJobObjectW failed: " + std::to_string(GetLastError()));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.BasicLimitInformation.ActiveProcessLimit = 1U;
    limits.ProcessMemoryLimit = static_cast<SIZE_T>(request.process_memory_limit_bytes);
    if (SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)) == FALSE) {
        return rejected("SetInformationJobObject failed: " + std::to_string(GetLastError()));
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process_info{};
    if (CreateProcessW(
            request.worker_executable.c_str(), command_buffer.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &process_info) == FALSE) {
        return rejected("CreateProcessW failed: " + std::to_string(GetLastError()));
    }
    UniqueHandle process{process_info.hProcess};
    UniqueHandle thread{process_info.hThread};
    if (AssignProcessToJobObject(job.get(), process.get()) == FALSE) {
        TerminateProcess(process.get(), 100U);
        return rejected("AssignProcessToJobObject failed: " + std::to_string(GetLastError()));
    }
    if (ResumeThread(thread.get()) == static_cast<DWORD>(-1)) {
        TerminateJobObject(job.get(), 101U);
        return rejected("ResumeThread failed: " + std::to_string(GetLastError()));
    }

    const auto started = std::chrono::steady_clock::now();
    while (true) {
        const DWORD wait = WaitForSingleObject(process.get(), 20U);
        if (wait == WAIT_OBJECT_0) break;
        if (wait == WAIT_FAILED) {
            TerminateJobObject(job.get(), 102U);
            return rejected("WaitForSingleObject failed: " + std::to_string(GetLastError()));
        }
        if (request.should_cancel && request.should_cancel()) {
            TerminateJobObject(job.get(), 103U);
            WaitForSingleObject(process.get(), 1000U);
            WorkerResult result;
            result.outcome = core::LaboratoryOutcome::cancelled;
            result.diagnostic = "laboratory worker was cancelled by the host";
            return result;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
        if (elapsed.count() >= request.timeout_ms) {
            TerminateJobObject(job.get(), 104U);
            WaitForSingleObject(process.get(), 1000U);
            WorkerResult result;
            result.outcome = core::LaboratoryOutcome::timeout;
            result.diagnostic = "laboratory worker exceeded its explicit deadline";
            return result;
        }
    }

    DWORD exit_code{};
    if (GetExitCodeProcess(process.get(), &exit_code) == FALSE) return rejected("GetExitCodeProcess failed: " + std::to_string(GetLastError()));
    if (exit_code != 0U) {
        WorkerResult result;
        result.outcome = core::LaboratoryOutcome::worker_crash;
        result.exit_code = exit_code;
        result.diagnostic = "laboratory worker exited nonzero before a trusted success response";
        return result;
    }
    return parse_response(paths, exit_code);
}

}  // namespace faultmine::laboratory
