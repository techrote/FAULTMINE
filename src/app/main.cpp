#include "app/release_self_test.hpp"
#include "platform/win32/main_window.hpp"

#include "faultmine/batch.hpp"
#include "faultmine/export.hpp"
#include "faultmine/genome.hpp"
#include "faultmine/mutation.hpp"
#include "faultmine/project.hpp"
#include "faultmine/version.hpp"

#include <windows.h>

#include <string>
#include <string_view>

namespace {

[[nodiscard]] std::wstring ascii_to_wide(const std::string_view text) {
    return std::wstring{text.begin(), text.end()};
}

void show_diagnostics() {
    std::wstring text = L"FAULTMINE ";
    text += ascii_to_wide(faultmine::kApplicationVersion);
    text += L"\nWindows x64 portable build\n\n";
    text += L"Engine contract: " + std::to_wstring(faultmine::core::kEngineContractVersion);
    text += L"\nGenome schema: " + std::to_wstring(faultmine::core::kGenomeSchemaVersion);
    text += L"\nProject schema: " + std::to_wstring(faultmine::app::kProjectSchemaVersion);
    text += L"\nExport manifest: " + std::to_wstring(faultmine::exporting::kExportManifestSchemaVersion);
    text += L"\nMutation policy: " + std::to_wstring(faultmine::core::kMutationPolicyVersion);
    text += L"\nBatch manifest: " + std::to_wstring(faultmine::core::kBatchManifestVersion);
    text += L"\nDescriptor: " + std::to_wstring(faultmine::core::kVisualDescriptorVersion);
    text += L"\n\nCanonical rendering is CPU-owned. Proxy previews and external-decoder experiments are explicitly non-canonical until materialized as documented.";
    MessageBoxW(nullptr, text.c_str(), L"FAULTMINE diagnostics", MB_OK | MB_ICONINFORMATION);
}

}  // namespace

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR command_line,
    int show_command) {
    const std::wstring_view arguments =
        command_line != nullptr ? std::wstring_view{command_line} : std::wstring_view{};

    if (arguments == L"--diagnostics" || arguments == L"--about") {
        show_diagnostics();
        return 0;
    }
    if (arguments == L"--release-self-test") {
        return faultmine::app::run_release_self_test();
    }

    const bool smoke_test = arguments == L"--smoke-test";
    return faultmine::platform::win32::run_application(instance, show_command, smoke_test);
}
