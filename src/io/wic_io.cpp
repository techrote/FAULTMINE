#include "faultmine/wic_io.hpp"

#include <windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace faultmine::io {
namespace {

using Microsoft::WRL::ComPtr;

class ComScope {
public:
    ComScope() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), uninitialize_(SUCCEEDED(result_)) {}

    ~ComScope() {
        if (uninitialize_) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool usable() const noexcept {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

    [[nodiscard]] HRESULT result() const noexcept {
        return result_;
    }

private:
    HRESULT result_{};
    bool uninitialize_{};
};

[[nodiscard]] WicError make_wic_error(
    const WicErrorCode code,
    std::string operation,
    const HRESULT result,
    std::string message) {
    std::ostringstream stream;
    stream << message << " (HRESULT 0x" << std::hex << static_cast<std::uint32_t>(result) << ')';
    return WicError{code, std::move(operation), static_cast<std::int32_t>(result), stream.str()};
}

[[nodiscard]] std::optional<WicError> create_factory(ComPtr<IWICImagingFactory>& factory) {
    const HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(result)) {
        return make_wic_error(
            WicErrorCode::factory_creation_failed,
            "CoCreateInstance(CLSID_WICImagingFactory)",
            result,
            "failed to create the WIC imaging factory");
    }
    return std::nullopt;
}

[[nodiscard]] bool dimensions_supported_by_wic_rows(
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
    return width > 0U && height > 0U &&
        width <= static_cast<std::uint32_t>(std::numeric_limits<INT>::max()) &&
        height <= static_cast<std::uint32_t>(std::numeric_limits<INT>::max()) &&
        width <= std::numeric_limits<UINT>::max() / 4U;
}

}  // namespace

WicLoadResult load_wic_image(const std::filesystem::path& path) {
    const ComScope com;
    if (!com.usable()) {
        return WicLoadResult{
            std::nullopt,
            make_wic_error(
                WicErrorCode::com_initialization_failed,
                "CoInitializeEx",
                com.result(),
                "COM initialization failed before WIC decode")};
    }

    ComPtr<IWICImagingFactory> factory;
    if (auto error = create_factory(factory); error.has_value()) {
        return WicLoadResult{std::nullopt, std::move(error)};
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT result = factory->CreateDecoderFromFilename(
        path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        decoder.GetAddressOf());
    if (FAILED(result)) {
        return WicLoadResult{
            std::nullopt,
            make_wic_error(WicErrorCode::open_failed, "CreateDecoderFromFilename", result, "failed to open image source")};
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0U, frame.GetAddressOf());
    if (FAILED(result)) {
        return WicLoadResult{
            std::nullopt,
            make_wic_error(WicErrorCode::decode_failed, "IWICBitmapDecoder::GetFrame", result, "failed to read the first image frame")};
    }

    UINT width = 0U;
    UINT height = 0U;
    result = frame->GetSize(&width, &height);
    if (FAILED(result)) {
        return WicLoadResult{
            std::nullopt,
            make_wic_error(WicErrorCode::decode_failed, "IWICBitmapFrameDecode::GetSize", result, "failed to query source dimensions")};
    }
    if (!dimensions_supported_by_wic_rows(width, height)) {
        return WicLoadResult{
            std::nullopt,
            make_wic_error(WicErrorCode::invalid_dimensions, "WIC dimensions", E_INVALIDARG, "source dimensions exceed the supported canonical row-copy contract")};
    }

    auto created = core::make_rgba8_image(width, height);
    if (!created.ok()) {
        return WicLoadResult{
            std::nullopt,
            make_wic_error(WicErrorCode::invalid_dimensions, "make_rgba8_image", E_INVALIDARG, created.error->message)};
    }

    ComPtr<IWICFormatConverter> converter;
    result = factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(result)) {
        return WicLoadResult{
            std::nullopt,
            make_wic_error(WicErrorCode::normalization_failed, "CreateFormatConverter", result, "failed to create WIC format converter")};
    }
    result = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(result)) {
        return WicLoadResult{
            std::nullopt,
            make_wic_error(WicErrorCode::normalization_failed, "IWICFormatConverter::Initialize", result, "failed to normalize source to straight RGBA8")};
    }

    core::ImageBuffer image = std::move(*created.image);
    const UINT stride = static_cast<UINT>(image.row_stride);
    for (UINT y = 0U; y < height; ++y) {
        WICRect rectangle{
            0,
            static_cast<INT>(y),
            static_cast<INT>(width),
            1};
        BYTE* row = reinterpret_cast<BYTE*>(image.bytes.data() + static_cast<std::size_t>(y) * stride);
        result = converter->CopyPixels(&rectangle, stride, stride, row);
        if (FAILED(result)) {
            return WicLoadResult{
                std::nullopt,
                make_wic_error(WicErrorCode::normalization_failed, "IWICFormatConverter::CopyPixels", result, "failed to copy normalized RGBA8 row")};
        }
    }

    LoadedSource source;
    source.source_identity = core::source_identity_hex(image);
    source.path = path;
    source.image = std::move(image);
    return WicLoadResult{std::move(source), std::nullopt};
}

std::optional<WicError> save_wic_png(
    const core::ImageBuffer& image,
    const std::filesystem::path& path) {
    if (const auto image_error = core::validate_canonical_image(image); image_error.has_value()) {
        return make_wic_error(WicErrorCode::encode_failed, "validate_canonical_image", E_INVALIDARG, image_error->message);
    }
    if (!dimensions_supported_by_wic_rows(image.width, image.height)) {
        return make_wic_error(WicErrorCode::invalid_dimensions, "WIC dimensions", E_INVALIDARG, "image dimensions exceed the supported PNG row-write contract");
    }

    const ComScope com;
    if (!com.usable()) {
        return make_wic_error(
            WicErrorCode::com_initialization_failed,
            "CoInitializeEx",
            com.result(),
            "COM initialization failed before WIC encode");
    }

    ComPtr<IWICImagingFactory> factory;
    if (auto error = create_factory(factory); error.has_value()) {
        return error;
    }

    ComPtr<IWICStream> stream;
    HRESULT result = factory->CreateStream(stream.GetAddressOf());
    if (FAILED(result)) {
        return make_wic_error(WicErrorCode::encode_failed, "IWICImagingFactory::CreateStream", result, "failed to create WIC output stream");
    }
    result = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (FAILED(result)) {
        return make_wic_error(WicErrorCode::encode_failed, "IWICStream::InitializeFromFilename", result, "failed to open PNG destination") ;
    }

    ComPtr<IWICBitmapEncoder> encoder;
    result = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf());
    if (FAILED(result)) {
        return make_wic_error(WicErrorCode::encode_failed, "IWICImagingFactory::CreateEncoder", result, "failed to create PNG encoder");
    }
    result = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(result)) {
        return make_wic_error(WicErrorCode::encode_failed, "IWICBitmapEncoder::Initialize", result, "failed to initialize PNG encoder");
    }

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    result = encoder->CreateNewFrame(frame.GetAddressOf(), properties.GetAddressOf());
    if (FAILED(result)) {
        return make_wic_error(WicErrorCode::encode_failed, "IWICBitmapEncoder::CreateNewFrame", result, "failed to create PNG frame");
    }
    result = frame->Initialize(properties.Get());
    if (FAILED(result)) {
        return make_wic_error(WicErrorCode::encode_failed, "IWICBitmapFrameEncode::Initialize", result, "failed to initialize PNG frame");
    }
    result = frame->SetSize(image.width, image.height);
    if (FAILED(result)) {
        return make_wic_error(WicErrorCode::encode_failed, "IWICBitmapFrameEncode::SetSize", result, "failed to set PNG dimensions");
    }

    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppRGBA;
    result = frame->SetPixelFormat(&pixel_format);
    if (FAILED(result) || !IsEqualGUID(pixel_format, GUID_WICPixelFormat32bppRGBA)) {
        return make_wic_error(WicErrorCode::encode_failed, "IWICBitmapFrameEncode::SetPixelFormat", FAILED(result) ? result : E_FAIL, "PNG encoder did not accept canonical RGBA8") ;
    }

    const UINT stride = static_cast<UINT>(image.row_stride);
    for (std::uint32_t y = 0U; y < image.height; ++y) {
        const BYTE* row = reinterpret_cast<const BYTE*>(
            image.bytes.data() + static_cast<std::size_t>(y) * stride);
        result = frame->WritePixels(1U, stride, stride, const_cast<BYTE*>(row));
        if (FAILED(result)) {
            return make_wic_error(WicErrorCode::encode_failed, "IWICBitmapFrameEncode::WritePixels", result, "failed to encode canonical RGBA8 row");
        }
    }

    result = frame->Commit();
    if (FAILED(result)) {
        return make_wic_error(WicErrorCode::encode_failed, "IWICBitmapFrameEncode::Commit", result, "failed to commit PNG frame");
    }
    result = encoder->Commit();
    if (FAILED(result)) {
        return make_wic_error(WicErrorCode::encode_failed, "IWICBitmapEncoder::Commit", result, "failed to commit PNG output");
    }
    return std::nullopt;
}

}  // namespace faultmine::io
