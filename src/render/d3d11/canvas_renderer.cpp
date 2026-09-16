#include "render/d3d11/canvas_renderer.hpp"

#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>

namespace faultmine::render::d3d11 {
namespace {

using Microsoft::WRL::ComPtr;

constexpr const char* kVertexShaderSource = R"(
cbuffer CanvasConstants : register(b0) {
    float4 transform;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput main(uint vertex_id : SV_VertexID) {
    const float2 positions[4] = {
        float2(-1.0,  1.0),
        float2( 1.0,  1.0),
        float2(-1.0, -1.0),
        float2( 1.0, -1.0)
    };
    const float2 uvs[4] = {
        float2(0.0, 0.0),
        float2(1.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 1.0)
    };

    VertexOutput output;
    float2 position = positions[vertex_id];
    position.x = position.x * transform.x + transform.z;
    position.y = position.y * transform.y + transform.w;
    output.position = float4(position, 0.0, 1.0);
    output.uv = uvs[vertex_id];
    return output;
}
)";

constexpr const char* kPixelShaderSource = R"(
Texture2D source_texture : register(t0);
SamplerState source_sampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    return source_texture.Sample(source_sampler, uv);
}
)";

std::string hresult_text(const char* operation, const HRESULT result) {
    std::ostringstream stream;
    stream << operation << " failed (HRESULT 0x" << std::hex << static_cast<std::uint32_t>(result) << ')';
    return stream.str();
}

std::optional<std::string> compile_shader(
    const char* source,
    const char* target,
    ComPtr<ID3DBlob>& bytecode) {
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompile(
        source,
        std::strlen(source),
        nullptr,
        nullptr,
        nullptr,
        "main",
        target,
        D3DCOMPILE_ENABLE_STRICTNESS,
        0U,
        bytecode.GetAddressOf(),
        errors.GetAddressOf());
    if (SUCCEEDED(result)) {
        return std::nullopt;
    }

    std::string message = hresult_text("D3DCompile", result);
    if (errors != nullptr && errors->GetBufferPointer() != nullptr && errors->GetBufferSize() != 0U) {
        message += ": ";
        message.append(
            static_cast<const char*>(errors->GetBufferPointer()),
            errors->GetBufferSize());
    }
    return message;
}

bool is_device_loss(const HRESULT result) noexcept {
    return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET;
}

}  // namespace

std::optional<std::string> CanvasRenderer::initialize(const HWND window) {
    if (window == nullptr) {
        return std::string{"cannot initialize D3D11 canvas without a window"};
    }
    window_ = window;
    RECT client{};
    if (GetClientRect(window_, &client) != 0) {
        width_ = std::max<std::uint32_t>(1U, static_cast<std::uint32_t>(std::max<LONG>(1, client.right - client.left)));
        height_ = std::max<std::uint32_t>(1U, static_cast<std::uint32_t>(std::max<LONG>(1, client.bottom - client.top)));
    }
    return recreate_device();
}

std::optional<std::string> CanvasRenderer::create_device_and_swap_chain() {
    device_.Reset();
    context_.Reset();
    swap_chain_.Reset();

    DXGI_SWAP_CHAIN_DESC swap_desc{};
    swap_desc.BufferDesc.Width = width_;
    swap_desc.BufferDesc.Height = height_;
    swap_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.SampleDesc.Count = 1U;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount = 2U;
    swap_desc.OutputWindow = window_;
    swap_desc.Windowed = TRUE;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr std::array<D3D_FEATURE_LEVEL, 3> feature_levels{
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL selected_level{};

    auto create = [&](const D3D_DRIVER_TYPE driver_type) {
        return D3D11CreateDeviceAndSwapChain(
            nullptr,
            driver_type,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            feature_levels.data(),
            static_cast<UINT>(feature_levels.size()),
            D3D11_SDK_VERSION,
            &swap_desc,
            swap_chain_.ReleaseAndGetAddressOf(),
            device_.ReleaseAndGetAddressOf(),
            &selected_level,
            context_.ReleaseAndGetAddressOf());
    };

    HRESULT result = create(D3D_DRIVER_TYPE_HARDWARE);
    using_warp_ = false;
    if (FAILED(result)) {
        result = create(D3D_DRIVER_TYPE_WARP);
        using_warp_ = SUCCEEDED(result);
    }
    if (FAILED(result)) {
        return hresult_text("D3D11CreateDeviceAndSwapChain", result);
    }
    return std::nullopt;
}

std::optional<std::string> CanvasRenderer::create_render_target() {
    render_target_.Reset();
    ComPtr<ID3D11Texture2D> back_buffer;
    const HRESULT result = swap_chain_->GetBuffer(
        0U,
        IID_PPV_ARGS(back_buffer.GetAddressOf()));
    if (FAILED(result)) {
        return hresult_text("IDXGISwapChain::GetBuffer", result);
    }
    const HRESULT view_result = device_->CreateRenderTargetView(
        back_buffer.Get(),
        nullptr,
        render_target_.GetAddressOf());
    if (FAILED(view_result)) {
        return hresult_text("ID3D11Device::CreateRenderTargetView", view_result);
    }
    return std::nullopt;
}

std::optional<std::string> CanvasRenderer::create_pipeline_objects() {
    vertex_shader_.Reset();
    pixel_shader_.Reset();
    sampler_.Reset();
    constants_.Reset();
    blend_state_.Reset();

    ComPtr<ID3DBlob> vertex_bytecode;
    if (auto error = compile_shader(kVertexShaderSource, "vs_5_0", vertex_bytecode); error.has_value()) {
        return error;
    }
    HRESULT result = device_->CreateVertexShader(
        vertex_bytecode->GetBufferPointer(),
        vertex_bytecode->GetBufferSize(),
        nullptr,
        vertex_shader_.GetAddressOf());
    if (FAILED(result)) {
        return hresult_text("ID3D11Device::CreateVertexShader", result);
    }

    ComPtr<ID3DBlob> pixel_bytecode;
    if (auto error = compile_shader(kPixelShaderSource, "ps_5_0", pixel_bytecode); error.has_value()) {
        return error;
    }
    result = device_->CreatePixelShader(
        pixel_bytecode->GetBufferPointer(),
        pixel_bytecode->GetBufferSize(),
        nullptr,
        pixel_shader_.GetAddressOf());
    if (FAILED(result)) {
        return hresult_text("ID3D11Device::CreatePixelShader", result);
    }

    D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    result = device_->CreateSamplerState(&sampler_desc, sampler_.GetAddressOf());
    if (FAILED(result)) {
        return hresult_text("ID3D11Device::CreateSamplerState", result);
    }

    D3D11_BUFFER_DESC constant_desc{};
    constant_desc.ByteWidth = static_cast<UINT>(sizeof(Constants));
    constant_desc.Usage = D3D11_USAGE_DEFAULT;
    constant_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    result = device_->CreateBuffer(&constant_desc, nullptr, constants_.GetAddressOf());
    if (FAILED(result)) {
        return hresult_text("ID3D11Device::CreateBuffer(constants)", result);
    }

    D3D11_BLEND_DESC blend_desc{};
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    result = device_->CreateBlendState(&blend_desc, blend_state_.GetAddressOf());
    if (FAILED(result)) {
        return hresult_text("ID3D11Device::CreateBlendState", result);
    }

    return std::nullopt;
}

std::optional<std::string> CanvasRenderer::recreate_device() {
    texture_.Reset();
    texture_view_.Reset();
    render_target_.Reset();
    vertex_shader_.Reset();
    pixel_shader_.Reset();
    sampler_.Reset();
    constants_.Reset();
    blend_state_.Reset();

    if (auto error = create_device_and_swap_chain(); error.has_value()) {
        return error;
    }
    if (auto error = create_render_target(); error.has_value()) {
        return error;
    }
    if (auto error = create_pipeline_objects(); error.has_value()) {
        return error;
    }
    return create_texture_from_cached_image();
}

std::optional<std::string> CanvasRenderer::resize(
    const std::uint32_t width,
    const std::uint32_t height) {
    if (width == 0U || height == 0U || swap_chain_ == nullptr) {
        return std::nullopt;
    }
    width_ = width;
    height_ = height;

    context_->OMSetRenderTargets(0U, nullptr, nullptr);
    render_target_.Reset();
    const HRESULT result = swap_chain_->ResizeBuffers(
        0U,
        width_,
        height_,
        DXGI_FORMAT_UNKNOWN,
        0U);
    if (is_device_loss(result)) {
        return recreate_device();
    }
    if (FAILED(result)) {
        return hresult_text("IDXGISwapChain::ResizeBuffers", result);
    }
    return create_render_target();
}

std::optional<std::string> CanvasRenderer::create_texture_from_cached_image() {
    texture_.Reset();
    texture_view_.Reset();
    if (!cached_image_.has_value()) {
        return std::nullopt;
    }

    const core::ImageBuffer& image = *cached_image_;
    if (image.row_stride > static_cast<std::uint64_t>(std::numeric_limits<UINT>::max())) {
        return std::string{"canonical image row stride exceeds D3D11 upload pitch range"};
    }

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = image.width;
    texture_desc.Height = image.height;
    texture_desc.MipLevels = 1U;
    texture_desc.ArraySize = 1U;
    texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_desc.SampleDesc.Count = 1U;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initial_data{};
    initial_data.pSysMem = image.bytes.data();
    initial_data.SysMemPitch = static_cast<UINT>(image.row_stride);

    HRESULT result = device_->CreateTexture2D(
        &texture_desc,
        &initial_data,
        texture_.GetAddressOf());
    if (FAILED(result)) {
        return hresult_text("ID3D11Device::CreateTexture2D", result);
    }
    result = device_->CreateShaderResourceView(
        texture_.Get(),
        nullptr,
        texture_view_.GetAddressOf());
    if (FAILED(result)) {
        return hresult_text("ID3D11Device::CreateShaderResourceView", result);
    }
    return std::nullopt;
}

std::optional<std::string> CanvasRenderer::upload_image(const core::ImageBuffer& image) {
    if (const auto validation = core::validate_canonical_image(image); validation.has_value()) {
        return validation->message;
    }
    cached_image_ = image;
    return create_texture_from_cached_image();
}

std::optional<std::string> CanvasRenderer::draw_once(
    const app::CanvasViewState& view,
    HRESULT* present_result) {
    if (render_target_ == nullptr || context_ == nullptr || swap_chain_ == nullptr) {
        return std::string{"D3D11 canvas is not initialized"};
    }

    constexpr float clear_colour[4]{0.055F, 0.055F, 0.065F, 1.0F};
    context_->OMSetRenderTargets(1U, render_target_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(render_target_.Get(), clear_colour);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0F;
    viewport.MaxDepth = 1.0F;
    context_->RSSetViewports(1U, &viewport);

    if (cached_image_.has_value() && texture_view_ != nullptr) {
        const core::ImageBuffer& image = *cached_image_;
        const double fit_zoom = std::min(
            static_cast<double>(width_) / static_cast<double>(image.width),
            static_cast<double>(height_) / static_cast<double>(image.height));
        double zoom = fit_zoom;
        if (view.mode == app::ViewMode::actual_size) {
            zoom = 1.0;
        } else if (view.mode == app::ViewMode::custom) {
            zoom = fit_zoom * view.zoom;
        }

        Constants constants;
        constants.scale_x = static_cast<float>(
            static_cast<double>(image.width) * zoom / static_cast<double>(width_));
        constants.scale_y = static_cast<float>(
            static_cast<double>(image.height) * zoom / static_cast<double>(height_));
        constants.offset_x = static_cast<float>(2.0 * view.pan_x / static_cast<double>(width_));
        constants.offset_y = static_cast<float>(-2.0 * view.pan_y / static_cast<double>(height_));
        context_->UpdateSubresource(constants_.Get(), 0U, nullptr, &constants, 0U, 0U);

        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context_->VSSetShader(vertex_shader_.Get(), nullptr, 0U);
        context_->VSSetConstantBuffers(0U, 1U, constants_.GetAddressOf());
        context_->PSSetShader(pixel_shader_.Get(), nullptr, 0U);
        context_->PSSetShaderResources(0U, 1U, texture_view_.GetAddressOf());
        context_->PSSetSamplers(0U, 1U, sampler_.GetAddressOf());
        constexpr float blend_factor[4]{0.0F, 0.0F, 0.0F, 0.0F};
        context_->OMSetBlendState(blend_state_.Get(), blend_factor, 0xffffffffU);
        context_->Draw(4U, 0U);
    }

    const HRESULT result = swap_chain_->Present(0U, 0U);
    if (present_result != nullptr) {
        *present_result = result;
    }
    if (FAILED(result) && !is_device_loss(result)) {
        return hresult_text("IDXGISwapChain::Present", result);
    }
    return std::nullopt;
}

std::optional<std::string> CanvasRenderer::draw(const app::CanvasViewState& view) {
    HRESULT present_result = S_OK;
    if (auto error = draw_once(view, &present_result); error.has_value()) {
        return error;
    }
    if (!is_device_loss(present_result)) {
        return std::nullopt;
    }

    if (auto error = recreate_device(); error.has_value()) {
        return error;
    }
    present_result = S_OK;
    if (auto error = draw_once(view, &present_result); error.has_value()) {
        return error;
    }
    if (FAILED(present_result)) {
        return hresult_text("IDXGISwapChain::Present after device recreation", present_result);
    }
    return std::nullopt;
}

std::uint32_t CanvasRenderer::width() const noexcept {
    return width_;
}

std::uint32_t CanvasRenderer::height() const noexcept {
    return height_;
}

bool CanvasRenderer::using_warp() const noexcept {
    return using_warp_;
}

}  // namespace faultmine::render::d3d11
