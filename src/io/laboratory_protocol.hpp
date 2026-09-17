#pragma once

#include "faultmine/laboratory.hpp"
#include "faultmine/laboratory_worker.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace faultmine::laboratory::protocol {

inline constexpr std::array<std::uint8_t, 4> kRequestMagic{'F', 'M', 'L', 'Q'};
inline constexpr std::array<std::uint8_t, 4> kResponseMagic{'F', 'M', 'L', 'R'};
inline constexpr std::uint32_t kRequestType = 1U;
inline constexpr std::uint32_t kResponseType = 2U;
inline constexpr std::size_t kRequestHeaderBytes = 24U;
inline constexpr std::size_t kResponseHeaderBytes = 52U;

struct RequestRecord {
    WorkerSyntheticMode synthetic_mode{WorkerSyntheticMode::none};
    std::vector<std::uint8_t> encoded_bytes;
};

struct ResponseRecord {
    Outcome outcome{Outcome::decode_failure};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint64_t row_stride{};
    std::uint64_t pixel_payload_length{};
    std::string diagnostic;
    std::string os_metadata;
    std::string decoder_metadata;
};

inline void append_u32(std::vector<std::uint8_t>& output, const std::uint32_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U) output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
}

inline void append_u64(std::vector<std::uint8_t>& output, const std::uint64_t value) {
    for (unsigned shift = 0U; shift < 64U; shift += 8U) output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
}

inline bool read_u32(const std::span<const std::uint8_t> bytes, std::size_t& offset, std::uint32_t& value) noexcept {
    if (offset > bytes.size() || bytes.size() - offset < 4U) return false;
    value = 0U;
    for (unsigned shift = 0U; shift < 32U; shift += 8U) value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
    return true;
}

inline bool read_u64(const std::span<const std::uint8_t> bytes, std::size_t& offset, std::uint64_t& value) noexcept {
    if (offset > bytes.size() || bytes.size() - offset < 8U) return false;
    value = 0U;
    for (unsigned shift = 0U; shift < 64U; shift += 8U) value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
    return true;
}

inline std::vector<std::uint8_t> encode_request(
    const std::span<const std::uint8_t> payload,
    const WorkerSyntheticMode synthetic_mode) {
    std::vector<std::uint8_t> result;
    result.reserve(kRequestHeaderBytes + payload.size());
    result.insert(result.end(), kRequestMagic.begin(), kRequestMagic.end());
    append_u32(result, kLaboratoryProtocolVersion);
    append_u32(result, kRequestType);
    append_u32(result, static_cast<std::uint32_t>(synthetic_mode));
    append_u64(result, static_cast<std::uint64_t>(payload.size()));
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

inline bool decode_request(
    const std::span<const std::uint8_t> bytes,
    RequestRecord& output,
    std::string& error) {
    if (bytes.size() < kRequestHeaderBytes || !std::equal(kRequestMagic.begin(), kRequestMagic.end(), bytes.begin())) {
        error = "invalid worker request magic/header";
        return false;
    }
    std::size_t offset = 4U;
    std::uint32_t version{};
    std::uint32_t type{};
    std::uint32_t synthetic{};
    std::uint64_t payload_length{};
    if (!read_u32(bytes, offset, version) || !read_u32(bytes, offset, type) || !read_u32(bytes, offset, synthetic) ||
        !read_u64(bytes, offset, payload_length) || version != kLaboratoryProtocolVersion || type != kRequestType ||
        synthetic > static_cast<std::uint32_t>(WorkerSyntheticMode::invalid_byte_count)) {
        error = "unsupported or malformed worker request header";
        return false;
    }
    if (payload_length > kMaxLaboratoryEncodedBytes || payload_length != bytes.size() - kRequestHeaderBytes) {
        error = "worker request payload length is invalid or oversized";
        return false;
    }
    output.synthetic_mode = static_cast<WorkerSyntheticMode>(synthetic);
    output.encoded_bytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(kRequestHeaderBytes), bytes.end());
    return true;
}

inline std::vector<std::uint8_t> encode_response(const ResponseRecord& response) {
    const std::string diagnostic = response.diagnostic.substr(0U, kMaxLaboratoryDiagnosticBytes);
    const std::string os = response.os_metadata.substr(0U, kMaxLaboratoryDiagnosticBytes);
    const std::string decoder = response.decoder_metadata.substr(0U, kMaxLaboratoryDiagnosticBytes);
    std::vector<std::uint8_t> result;
    result.reserve(kResponseHeaderBytes + diagnostic.size() + os.size() + decoder.size());
    result.insert(result.end(), kResponseMagic.begin(), kResponseMagic.end());
    append_u32(result, kLaboratoryProtocolVersion);
    append_u32(result, kResponseType);
    append_u32(result, static_cast<std::uint32_t>(response.outcome));
    append_u32(result, response.width);
    append_u32(result, response.height);
    append_u64(result, response.row_stride);
    append_u64(result, response.pixel_payload_length);
    append_u32(result, static_cast<std::uint32_t>(diagnostic.size()));
    append_u32(result, static_cast<std::uint32_t>(os.size()));
    append_u32(result, static_cast<std::uint32_t>(decoder.size()));
    result.insert(result.end(), diagnostic.begin(), diagnostic.end());
    result.insert(result.end(), os.begin(), os.end());
    result.insert(result.end(), decoder.begin(), decoder.end());
    return result;
}

inline bool decode_response(
    const std::span<const std::uint8_t> bytes,
    ResponseRecord& output,
    std::string& error) {
    if (bytes.size() < kResponseHeaderBytes || !std::equal(kResponseMagic.begin(), kResponseMagic.end(), bytes.begin())) {
        error = "invalid worker response magic/header";
        return false;
    }
    std::size_t offset = 4U;
    std::uint32_t version{};
    std::uint32_t type{};
    std::uint32_t outcome{};
    std::uint32_t diagnostic_length{};
    std::uint32_t os_length{};
    std::uint32_t decoder_length{};
    if (!read_u32(bytes, offset, version) || !read_u32(bytes, offset, type) || !read_u32(bytes, offset, outcome) ||
        !read_u32(bytes, offset, output.width) || !read_u32(bytes, offset, output.height) ||
        !read_u64(bytes, offset, output.row_stride) || !read_u64(bytes, offset, output.pixel_payload_length) ||
        !read_u32(bytes, offset, diagnostic_length) || !read_u32(bytes, offset, os_length) || !read_u32(bytes, offset, decoder_length) ||
        version != kLaboratoryProtocolVersion || type != kResponseType ||
        outcome > static_cast<std::uint32_t>(Outcome::rejected_response)) {
        error = "unsupported or malformed worker response header";
        return false;
    }
    if (diagnostic_length > kMaxLaboratoryDiagnosticBytes || os_length > kMaxLaboratoryDiagnosticBytes ||
        decoder_length > kMaxLaboratoryDiagnosticBytes) {
        error = "worker response diagnostic metadata exceeds its bounded contract";
        return false;
    }
    const std::uint64_t string_bytes = static_cast<std::uint64_t>(diagnostic_length) + os_length + decoder_length;
    if (string_bytes != bytes.size() - kResponseHeaderBytes) {
        error = "worker response control-message length is inconsistent";
        return false;
    }
    output.outcome = static_cast<Outcome>(outcome);
    output.diagnostic.assign(reinterpret_cast<const char*>(bytes.data() + offset), diagnostic_length);
    offset += diagnostic_length;
    output.os_metadata.assign(reinterpret_cast<const char*>(bytes.data() + offset), os_length);
    offset += os_length;
    output.decoder_metadata.assign(reinterpret_cast<const char*>(bytes.data() + offset), decoder_length);
    return true;
}

}  // namespace faultmine::laboratory::protocol
