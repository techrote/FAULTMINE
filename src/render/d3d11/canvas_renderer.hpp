#pragma once

#include "faultmine/image.hpp"
#include "faultmine/session.hpp"

#include <windows.h>
#include <wrl/client.h>

#include <d3d11.h>
#include <dxgi.h>

#include <cstdint>
#include <optional>
#include <string>

namespace faultmine::render::d3d11 {

class CanvasRenderer {
public:
    CanvasRenderer() = default;
    CanvasRenderer(const CanvasRenderer&) = delete;
    CanvasRenderer& operator=(const CanvasRenderer&) = delete;

    [[nodiscard]] std::optional<std::string> initialize(HWND window);
    [[nodiscard]] std::optional<std::string> resize(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] std::optional<std::string> upload_image(const core::ImageBuffer& image);
    [[nodiscard]] std::optional<std::string> draw(const app::CanvasViewState& view);

    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;
    [[nodiscard]] bool using_warp() const noexcept;

private:
    struct Constants {
        float scale_x{1.0F};
        float scale_y{1.0F};
        float offset_x{};
        float offset_y{};
    };

    [[nodiscard]] std::optional<std::string> create_device_and_swap_chain();
    [[nodiscard]] std::optional<std::string> create_render_target();
    [[nodiscard]] std::optional<std::string> create_pipeline_objects();
    [[nodiscard]] std::optional<std::string> recreate_device();
    [[nodiscard]] std::optional<std::string> create_texture_from_cached_image();
    [[nodiscard]] std::optional<std::string> draw_once(const app::CanvasViewState& view, HRESULT* present_result);

    HWND window_{};
    std::uint32_t width_{1U};
    std::uint32_t height_{1U};
    bool using_warp_{};

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_view_;

    std::optional<core::ImageBuffer> cached_image_;
};

}  // namespace faultmine::render::d3d11
