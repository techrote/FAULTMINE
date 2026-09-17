#include "faultmine/laboratory_worker.hpp"

#include "../../io/laboratory_protocol.hpp"
#include "faultmine/sha256.hpp"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
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
    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) reset(other.release());
        return *this;
    }
    [[nodiscard]] HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] explicit operator bool() const noexcept { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }
    [[nodiscard]] HANDLE release() noexcept {
        HANDLE result = handle_;
        handle_ = nullptr;
        return result;
    }
    void reset(HANDLE handle = nullptr) noexcept {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
        handle_ = handle;
    }
private:
    HANDLE handle_{};
};

class TempDirectory {
public:
    explicit TempDirectory(std::filesystem::path path) : path_(std::move(path)) {}
    ~TempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
private:
    std::filesystem::path path_;
};

[[nodiscard]] std::string win32_error(const DWORD code) {
    return "Win32 error " + std::to_string(code);
}

[[nodiscard]] std::filesystem::path make_temp_directory(std::string& error) {
    std::vector<wchar_t> buffer(32768U);
    const DWORD length = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (length == 0U || length >= buffer.size()) {
        error = "GetTempPathW failed: " + win32_error(GetLastError());
        return {};
    }
    const std::filesystem::path root(buffer.data());
    for (std::uint32_t attempt = 0U; attempt < 1024U; ++attempt) {
        const std::wstring name = L"faultmine-lab-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(attempt);
        const std::filesystem::path candidate = root / name;
        std::error_code filesystem_error;
        if (std::filesystem::create_directory(candidate, filesystem_error)) return candidate;
        if (filesystem_error && filesystem_error != std::errc::file_exists) {
            error = "could not create laboratory temporary directory: " + filesystem_error.message();
            return {};
        }
    }
    error = "could not reserve a laboratory temporary directory";
    return {};
}

[[nodiscard]] bool write_bytes(
    const std::filesystem::path& path,
    const std::span<const std::uint8_t> bytes,
    std::string& error) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "could not open bounded worker request file for writing";
        return false;
    }
    if (!bytes.empty()) {
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    stream.flush();
    if (!stream) {
        error = "could not commit bounded worker request bytes";
        return false;
    }
    return true;
}

[[nodiscard]] bool read_bytes_bounded(
    const std::filesystem::path& path,
    const std::size_t maximum,
    std::vector<std::uint8_t>& bytes,
    std::string& error) {
    std::error_code filesystem_error;
    const std::uintmax_t size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        error = "could not inspect worker response file: " + filesystem_error.message();
        return false;
    }
    if (size > maximum) {
        error = "worker response file exceeds its bounded contract";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "could not open worker response file";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    if (!bytes.empty()) stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        error = "could not read complete worker response file";
        return false;
    }
    return true;
}

[[nodiscard]] std::wstring quote_argument(const std::filesystem::path& path) {
    std::wstring text = path.wstring();
    std::wstring quoted{L"\""};
    for (const wchar_t character : text) {
        if (character == L'\"') quoted += L'\\';
        quoted += character;
    }
    quoted += L'\"';
    return quoted;
}

[[nodiscard]] WorkerResult fail_result(
    LaboratoryProvenance provenance,
    const Outcome outcome,
    std::string message) {
    provenance.outcome = outcome;
    provenance.diagnostic = message.substr(0U, kMaxLaboratoryDiagnosticBytes);
    WorkerResult result;
    result.provenance = std::move(provenance);
    result.host_error = std::move(message);
    return result;
}

}  // namespace

WorkerResult run_decoder_worker(
    const std::span<const std::uint8_t> mutated_encoded_bytes,
    std::string original_encoded_identity,
    std::string mutation_recipe,
    const core::RootSeed seed,
    const WorkerOptions& options) {
    LaboratoryProvenance provenance;
    provenance.encoded_input_identity = std::move(original_encoded_identity);
    provenance.mutated_input_identity = core::sha256_hex(mutated_encoded_bytes);
    provenance.mutation_recipe = std::move(mutation_recipe);
    provenance.root_seed = seed.to_string();
    provenance.worker_application_version = "0.13.0";
    provenance.outcome = Outcome::rejected_response;

    if (mutated_encoded_bytes.size() > kMaxLaboratoryEncodedBytes) {
        return fail_result(std::move(provenance), Outcome::rejected_response, "mutated encoded input exceeds laboratory byte limit");
    }
    if (options.executable.empty()) {
        return fail_result(std::move(provenance), Outcome::rejected_response, "worker executable path is empty");
    }

    std::string error;
    const std::filesystem::path temporary = make_temp_directory(error);
    if (temporary.empty()) return fail_result(std::move(provenance), Outcome::rejected_response, std::move(error));
    TempDirectory cleanup(temporary);
    const std::filesystem::path request_path = cleanup.path() / L"request.bin";
    const std::filesystem::path response_path = cleanup.path() / L"response.bin";
    const std::filesystem::path pixels_path = cleanup.path() / L"pixels.rgba";
    const std::vector<std::uint8_t> request = protocol::encode_request(mutated_encoded_bytes, options.synthetic_mode);
    if (!write_bytes(request_path, request, error)) {
        return fail_result(std::move(provenance), Outcome::rejected_response, std::move(error));
    }

    UniqueHandle job(CreateJobObjectW(nullptr, nullptr));
    if (!job) return fail_result(std::move(provenance), Outcome::rejected_response, "CreateJobObjectW failed: " + win32_error(GetLastError()));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    limits.BasicLimitInformation.ActiveProcessLimit = 1U;
    if (options.job_memory_limit_bytes != 0U) {
        limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
        const std::uint64_t bounded = std::min<std::uint64_t>(
            options.job_memory_limit_bytes,
            static_cast<std::uint64_t>(std::numeric_limits<SIZE_T>::max()));
        limits.JobMemoryLimit = static_cast<SIZE_T>(bounded);
    }
    if (SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)) == FALSE) {
        return fail_result(std::move(provenance), Outcome::rejected_response, "SetInformationJobObject failed: " + win32_error(GetLastError()));
    }

    std::wstring command = quote_argument(options.executable) + L" --request " + quote_argument(request_path) +
        L" --response " + quote_argument(response_path) + L" --pixels " + quote_argument(pixels_path);
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process_info{};
    if (CreateProcessW(
            options.executable.c_str(), mutable_command.data(), nullptr, nullptr, FALSE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, cleanup.path().c_str(), &startup, &process_info) == FALSE) {
        return fail_result(std::move(provenance), Outcome::worker_crash, "CreateProcessW failed: " + win32_error(GetLastError()));
    }
    UniqueHandle process(process_info.hProcess);
    UniqueHandle thread(process_info.hThread);
    if (AssignProcessToJobObject(job.get(), process.get()) == FALSE) {
        TerminateProcess(process.get(), 0xDEADU);
        return fail_result(std::move(provenance), Outcome::rejected_response, "AssignProcessToJobObject failed: " + win32_error(GetLastError()));
    }
    if (ResumeThread(thread.get()) == static_cast<DWORD>(-1)) {
        TerminateJobObject(job.get(), 0xDEADU);
        return fail_result(std::move(provenance), Outcome::worker_crash, "ResumeThread failed: " + win32_error(GetLastError()));
    }

    const ULONGLONG start = GetTickCount64();
    bool cancelled = false;
    bool timed_out = false;
    for (;;) {
        const DWORD wait = WaitForSingleObject(process.get(), 20U);
        if (wait == WAIT_OBJECT_0) break;
        if (wait == WAIT_FAILED) {
            TerminateJobObject(job.get(), 0xDEADU);
            return fail_result(std::move(provenance), Outcome::worker_crash, "WaitForSingleObject failed: " + win32_error(GetLastError()));
        }
        if (options.should_cancel && options.should_cancel()) {
            cancelled = true;
            break;
        }
        if (GetTickCount64() - start >= options.timeout_ms) {
            timed_out = true;
            break;
        }
    }
    if (cancelled || timed_out) {
        TerminateJobObject(job.get(), cancelled ? 0xCA11U : 0x710EU);
        WaitForSingleObject(process.get(), 1000U);
        provenance.outcome = cancelled ? Outcome::cancelled : Outcome::timeout;
        provenance.diagnostic = cancelled ? "laboratory experiment cancelled" : "laboratory worker exceeded its deadline";
        WorkerResult result;
        result.provenance = std::move(provenance);
        return result;
    }

    DWORD exit_code = 0U;
    if (GetExitCodeProcess(process.get(), &exit_code) == FALSE) {
        return fail_result(std::move(provenance), Outcome::worker_crash, "GetExitCodeProcess failed: " + win32_error(GetLastError()));
    }
    if (exit_code != 0U) {
        WorkerResult result = fail_result(std::move(provenance), Outcome::worker_crash,
            "laboratory worker exited with code " + std::to_string(exit_code));
        result.exit_code = exit_code;
        return result;
    }

    std::vector<std::uint8_t> control;
    constexpr std::size_t kMaxControlBytes = protocol::kResponseHeaderBytes + 3U * kMaxLaboratoryDiagnosticBytes;
    if (!read_bytes_bounded(response_path, kMaxControlBytes, control, error)) {
        return fail_result(std::move(provenance), Outcome::rejected_response, std::move(error));
    }
    protocol::ResponseRecord response;
    if (!protocol::decode_response(control, response, error)) {
        return fail_result(std::move(provenance), Outcome::rejected_response, std::move(error));
    }
    provenance.outcome = response.outcome;
    provenance.diagnostic = response.diagnostic;
    provenance.os_metadata = response.os_metadata;
    provenance.decoder_metadata = response.decoder_metadata;

    WorkerResult result;
    result.exit_code = exit_code;
    if (response.outcome != Outcome::success) {
        if (response.pixel_payload_length != 0U || response.width != 0U || response.height != 0U || response.row_stride != 0U) {
            return fail_result(std::move(provenance), Outcome::rejected_response, "non-success worker response carried pixel metadata");
        }
        result.provenance = std::move(provenance);
        return result;
    }

    if (response.width == 0U || response.height == 0U || response.width > std::numeric_limits<std::uint32_t>::max() / 4U) {
        return fail_result(std::move(provenance), Outcome::rejected_response, "worker returned invalid image dimensions");
    }
    const auto expected_size = core::canonical_rgba8_byte_size(response.width, response.height);
    const std::uint64_t expected_stride = static_cast<std::uint64_t>(response.width) * 4U;
    if (!expected_size.has_value() || *expected_size > kMaxLaboratoryPixelBytes || response.row_stride != expected_stride ||
        response.pixel_payload_length != *expected_size) {
        return fail_result(std::move(provenance), Outcome::rejected_response, "worker returned inconsistent or oversized pixel metadata");
    }
    std::vector<std::uint8_t> pixels;
    if (!read_bytes_bounded(pixels_path, kMaxLaboratoryPixelBytes, pixels, error)) {
        return fail_result(std::move(provenance), Outcome::rejected_response, std::move(error));
    }
    if (pixels.size() != *expected_size) {
        return fail_result(std::move(provenance), Outcome::rejected_response, "worker pixel file length does not match validated metadata");
    }
    auto created = core::make_rgba8_image(response.width, response.height, pixels);
    if (!created.ok()) {
        return fail_result(std::move(provenance), Outcome::rejected_response, created.error->message);
    }
    provenance.materialized_source_identity = core::source_identity_hex(*created.image);
    MaterializedSource materialized{std::move(*created.image), provenance};
    if (const auto validation = validate_materialized_source(materialized); validation.has_value()) {
        return fail_result(std::move(provenance), Outcome::rejected_response, *validation);
    }
    result.provenance = provenance;
    result.materialized = std::move(materialized);
    return result;
}

}  // namespace faultmine::laboratory
