#include "faultmine/export.hpp"

#define FAULTMINE_FM012_LAYER 1
#include "main_window_fm011.cpp"
#undef FAULTMINE_FM012_LAYER

#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::platform::win32 {
namespace {

constexpr wchar_t kExportPropertyName[] = L"FAULTMINE.FM012.Export";
constexpr wchar_t kSequenceDialogClass[] = L"FAULTMINE.FM012.SequenceRange";
constexpr wchar_t kProgressClass[] = L"FAULTMINE.FM012.ExportProgress";

constexpr UINT kCommandExportStill = 1801U;
constexpr UINT kCommandExportContact = 1802U;
constexpr UINT kCommandExportSequence = 1803U;
constexpr UINT kCommandExportCancel = 1804U;

constexpr int kHotkeyExportContact = 1901;
constexpr int kHotkeyExportSequence = 1902;

constexpr UINT kRangeStartEdit = 2001U;
constexpr UINT kRangeEndEdit = 2002U;
constexpr UINT kRangeOk = 2003U;
constexpr UINT kRangeCancel = 2004U;
constexpr UINT kProgressCancel = 2010U;

[[nodiscard]] bool smoke_command_line() noexcept {
    const wchar_t* command_line = GetCommandLineW();
    return command_line != nullptr && std::wstring_view{command_line}.find(L"--smoke-test") != std::wstring_view::npos;
}

struct RangeDialogState {
    HWND window{};
    HWND start_edit{};
    HWND end_edit{};
    std::uint64_t start{};
    std::uint64_t end{};
    bool accepted{};
    bool finished{};
};

[[nodiscard]] bool parse_u64_text(const HWND edit, std::uint64_t& value) {
    wchar_t buffer[64]{};
    const int length = GetWindowTextW(edit, buffer, static_cast<int>(std::size(buffer)));
    if (length <= 0) return false;
    const std::string text = wide_to_utf8(std::wstring_view{buffer, static_cast<std::size_t>(length)});
    std::uint64_t parsed{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return false;
    value = parsed;
    return true;
}

LRESULT CALLBACK range_dialog_proc(const HWND window, const UINT message, const WPARAM w_param, const LPARAM l_param) {
    auto* state = reinterpret_cast<RangeDialogState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        state = static_cast<RangeDialogState*>(create->lpCreateParams);
        if (state != nullptr) {
            state->window = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }
    }
    if (state == nullptr) return DefWindowProcW(window, message, w_param, l_param);

    switch (message) {
        case WM_CREATE: {
            const auto make = [&](const wchar_t* klass, const wchar_t* text, const DWORD style, const UINT id,
                                  const int x, const int y, const int width, const int height) {
                HWND control = CreateWindowExW(
                    0, klass, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, window,
                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
                if (control != nullptr) {
                    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
                }
                return control;
            };
            (void)make(L"STATIC", L"Start frame (inclusive)", SS_LEFT, 0U, 12, 14, 150, 20);
            state->start_edit = make(L"EDIT", std::to_wstring(state->start).c_str(), ES_AUTOHSCROLL | WS_BORDER,
                kRangeStartEdit, 170, 10, 180, 24);
            (void)make(L"STATIC", L"End frame (exclusive)", SS_LEFT, 0U, 12, 48, 150, 20);
            state->end_edit = make(L"EDIT", std::to_wstring(state->end).c_str(), ES_AUTOHSCROLL | WS_BORDER,
                kRangeEndEdit, 170, 44, 180, 24);
            (void)make(L"STATIC", L"Deterministic -f000000 PNG names; provenance manifest on by default.",
                SS_LEFT, 0U, 12, 80, 338, 38);
            (void)make(L"BUTTON", L"Export", BS_DEFPUSHBUTTON, kRangeOk, 190, 126, 76, 26);
            (void)make(L"BUTTON", L"Cancel", BS_PUSHBUTTON, kRangeCancel, 274, 126, 76, 26);
            return 0;
        }
        case WM_COMMAND: {
            const UINT command = static_cast<UINT>(LOWORD(w_param));
            if (command == kRangeOk) {
                std::uint64_t start{};
                std::uint64_t end{};
                if (!parse_u64_text(state->start_edit, start) || !parse_u64_text(state->end_edit, end) || end <= start) {
                    MessageBoxW(window, L"Enter a valid non-empty [start,end) unsigned frame range.", kWindowTitle, MB_OK | MB_ICONWARNING);
                    return 0;
                }
                if (end - start > 100000U) {
                    MessageBoxW(window, L"FM-012 limits one sequence export to 100000 frames.", kWindowTitle, MB_OK | MB_ICONWARNING);
                    return 0;
                }
                state->start = start;
                state->end = end;
                state->accepted = true;
                state->finished = true;
                DestroyWindow(window);
                return 0;
            }
            if (command == kRangeCancel) {
                state->finished = true;
                DestroyWindow(window);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            state->finished = true;
            DestroyWindow(window);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

[[nodiscard]] bool prompt_sequence_range(
    const HWND owner,
    const std::uint64_t current_frame,
    std::uint64_t& start,
    std::uint64_t& end) {
    if (current_frame == std::numeric_limits<std::uint64_t>::max()) {
        MessageBoxW(owner, L"The maximum uint64 frame cannot start a non-empty end-exclusive range. Seek to an earlier frame first.",
            kWindowTitle, MB_OK | MB_ICONWARNING);
        return false;
    }

    WNDCLASSEXW klass{};
    klass.cbSize = static_cast<UINT>(sizeof(klass));
    klass.lpfnWndProc = range_dialog_proc;
    klass.hInstance = GetModuleHandleW(nullptr);
    klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    klass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    klass.lpszClassName = kSequenceDialogClass;
    const ATOM atom = RegisterClassExW(&klass);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    RangeDialogState state;
    state.start = current_frame;
    state.end = current_frame > std::numeric_limits<std::uint64_t>::max() - 60U
        ? std::numeric_limits<std::uint64_t>::max() : current_frame + 60U;
    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME, kSequenceDialogClass, L"FAULTMINE frame sequence",
        WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 378, 200,
        owner, nullptr, GetModuleHandleW(nullptr), &state);
    if (dialog == nullptr) return false;

    EnableWindow(owner, FALSE);
    MSG message{};
    while (!state.finished) {
        const BOOL status = GetMessageW(&message, nullptr, 0U, 0U);
        if (status <= 0) {
            state.finished = true;
            break;
        }
        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (!state.accepted) return false;
    start = state.start;
    end = state.end;
    return true;
}

struct ProgressWindowState {
    HWND window{};
    HWND label{};
    bool cancelled{};
};

LRESULT CALLBACK progress_proc(const HWND window, const UINT message, const WPARAM w_param, const LPARAM l_param) {
    auto* state = reinterpret_cast<ProgressWindowState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        state = static_cast<ProgressWindowState*>(create->lpCreateParams);
        if (state != nullptr) {
            state->window = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }
    }
    if (state == nullptr) return DefWindowProcW(window, message, w_param, l_param);

    if (message == WM_CREATE) {
        state->label = CreateWindowExW(0, L"STATIC", L"Preparing export...", WS_CHILD | WS_VISIBLE | SS_LEFT,
            12, 14, 346, 24, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            282, 48, 76, 26, window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kProgressCancel)),
            GetModuleHandleW(nullptr), nullptr);
        if (state->label != nullptr) SendMessageW(state->label, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        if (cancel != nullptr) SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        return 0;
    }
    if (message == WM_COMMAND && static_cast<UINT>(LOWORD(w_param)) == kProgressCancel) {
        state->cancelled = true;
        if (state->label != nullptr) SetWindowTextW(state->label, L"Cancelling after the current atomic frame write...");
        return 0;
    }
    if (message == WM_CLOSE) {
        state->cancelled = true;
        ShowWindow(window, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

class ExportUi {
public:
    explicit ExportUi(MainWindow& owner) : owner_(owner) {}

    [[nodiscard]] bool attach(std::string* error) {
        HMENU export_menu = CreatePopupMenu();
        if (export_menu == nullptr) {
            if (error != nullptr) *error = "could not create FM-012 Export menu";
            return false;
        }
        AppendMenuW(export_menu, MF_STRING, kCommandExportStill, L"Canonical &still + manifest\tCtrl+E");
        AppendMenuW(export_menu, MF_STRING, kCommandExportContact, L"Retained specimen &contact sheet\tCtrl+Shift+E");
        AppendMenuW(export_menu, MF_STRING, kCommandExportSequence, L"Temporal frame &sequence...\tCtrl+Alt+E");
        AppendMenuW(export_menu, MF_SEPARATOR, 0U, nullptr);
        AppendMenuW(export_menu, MF_STRING | MF_GRAYED, kCommandExportCancel, L"Cancel active export\tEsc");
        AppendMenuW(owner_.menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(export_menu), L"E&xport");
        DrawMenuBar(owner_.hwnd_);

        (void)RegisterHotKey(owner_.hwnd_, kHotkeyExportContact, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'E');
        (void)RegisterHotKey(owner_.hwnd_, kHotkeyExportSequence, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'E');
        SetPropW(owner_.hwnd_, kExportPropertyName, reinterpret_cast<HANDLE>(this));
        SetLastError(ERROR_SUCCESS);
        old_proc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            owner_.hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ExportUi::parent_proc)));
        if (old_proc_ == nullptr && GetLastError() != ERROR_SUCCESS) {
            RemovePropW(owner_.hwnd_, kExportPropertyName);
            if (error != nullptr) *error = "could not subclass main window for FM-012 export controls";
            return false;
        }
        return true;
    }

    [[nodiscard]] bool maybe_run_smoke(std::string* error) {
        if (!smoke_command_line() || smoke_attempted_ || !owner_.session_.has_source()) return true;
        smoke_attempted_ = true;
        return smoke_export(error);
    }

private:
    static LRESULT CALLBACK parent_proc(
        const HWND window,
        const UINT message,
        const WPARAM w_param,
        const LPARAM l_param) {
        auto* self = reinterpret_cast<ExportUi*>(GetPropW(window, kExportPropertyName));
        if (self == nullptr || self->old_proc_ == nullptr) return DefWindowProcW(window, message, w_param, l_param);

        std::string smoke_error;
        if (!self->maybe_run_smoke(&smoke_error)) {
            show_error_box(window, L"FAULTMINE canonical export smoke failed:\n" + utf8_to_wide(smoke_error));
            PostQuitMessage(EXIT_FAILURE);
            return 0;
        }

        if (message == WM_COMMAND) {
            const UINT command = static_cast<UINT>(LOWORD(w_param));
            if (command == command_export || command == kCommandExportStill) {
                self->export_still();
                return 0;
            }
            if (command == kCommandExportContact) {
                self->export_contact();
                return 0;
            }
            if (command == kCommandExportSequence) {
                self->export_sequence();
                return 0;
            }
            if (command == kCommandExportCancel && self->progress_ != nullptr) {
                self->progress_->cancelled = true;
                return 0;
            }
        }
        if (message == WM_HOTKEY) {
            if (static_cast<int>(w_param) == kHotkeyExportContact) {
                self->export_contact();
                return 0;
            }
            if (static_cast<int>(w_param) == kHotkeyExportSequence) {
                self->export_sequence();
                return 0;
            }
        }
        if (message == WM_KEYDOWN) {
            const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            if (control && !shift && !alt && (w_param == 'E' || w_param == 'e')) {
                self->export_still();
                return 0;
            }
            if (w_param == VK_ESCAPE && self->progress_ != nullptr) {
                self->progress_->cancelled = true;
                return 0;
            }
        }

        if (message == WM_DESTROY) {
            UnregisterHotKey(window, kHotkeyExportContact);
            UnregisterHotKey(window, kHotkeyExportSequence);
        }
        const LRESULT result = CallWindowProcW(self->old_proc_, window, message, w_param, l_param);
        if (message == WM_NCDESTROY) {
            RemovePropW(window, kExportPropertyName);
            delete self;
        }
        return result;
    }

    [[nodiscard]] std::optional<std::filesystem::path> choose_png_path(const std::wstring& suggested) const {
        std::array<wchar_t, 32768> filename{};
        std::copy_n(suggested.c_str(), std::min<std::size_t>(suggested.size(), filename.size() - 1U), filename.data());
        OPENFILENAMEW dialog{};
        dialog.lStructSize = static_cast<DWORD>(sizeof(dialog));
        dialog.hwndOwner = owner_.hwnd_;
        dialog.lpstrFilter = L"PNG image\0*.png\0All files\0*.*\0\0";
        dialog.lpstrDefExt = L"png";
        dialog.lpstrFile = filename.data();
        dialog.nMaxFile = static_cast<DWORD>(filename.size());
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetSaveFileNameW(&dialog) == FALSE) return std::nullopt;
        return std::filesystem::path{filename.data()};
    }

    [[nodiscard]] std::optional<app::ExportOverwritePolicy> confirm_overwrite_policy(
        const std::filesystem::path& path,
        const bool sequence_wide) const {
        const bool image_or_base_exists = std::filesystem::exists(path);
        const bool manifest_exists = std::filesystem::exists(app::companion_manifest_path(path));
        if (!image_or_base_exists && !manifest_exists) return app::ExportOverwritePolicy::fail_if_exists;

        const wchar_t* text = sequence_wide
            ? L"One or more export names may already exist. Replace colliding sequence files and the companion manifest atomically?"
            : L"The selected PNG or its companion provenance manifest already exists. Replace the existing export artefact(s) atomically?";
        const int answer = MessageBoxW(owner_.hwnd_, text, kWindowTitle, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (answer != IDYES) return std::nullopt;
        return app::ExportOverwritePolicy::replace_existing;
    }

    void report_result(const app::ExportResult& result, const std::string_view success_text) {
        if (result.success) {
            owner_.transient_status_ = std::string{success_text};
            owner_.update_status();
            return;
        }
        if (result.cancelled) {
            owner_.transient_status_ = "Export cancelled; completed atomic files and partial manifest were preserved.";
            owner_.update_status();
            return;
        }
        show_error_box(owner_.hwnd_, L"Export failed:\n" + utf8_to_wide(result.error));
    }

    void export_still() {
        if (!owner_.session_.has_source()) {
            show_error_box(owner_.hwnd_, L"Load an image before exporting.");
            return;
        }
        if (owner_.session_.current_frame() == std::numeric_limits<std::uint64_t>::max()) {
            show_error_box(owner_.hwnd_, L"The maximum uint64 frame cannot be represented as a one-frame [start,end) manifest range. Seek to an earlier frame before exporting.");
            return;
        }
        const auto path = choose_png_path(owner_.session_.source_path().stem().wstring() + L"-faultmine.png");
        if (!path.has_value()) return;
        const auto policy = confirm_overwrite_policy(*path, false);
        if (!policy.has_value()) return;
        app::ExportOptions options;
        options.overwrite_policy = *policy;
        report_result(app::export_canonical_still(owner_.session_, *path, options),
            "Exported full-resolution canonical PNG plus provenance manifest.");
    }

    [[nodiscard]] std::vector<app::SpecimenTrayItem> retained_mutation_items() const {
        std::vector<app::SpecimenTrayItem> items;
        for (const app::SpecimenRecord& record : owner_.session_.lineage().records()) {
            if (record.derivation.kind != app::DerivationKind::mutation ||
                record.derivation.parent_genome_identities.size() != 1U ||
                !record.derivation.seed.has_value() || !record.derivation.descendant_index.has_value() ||
                !record.derivation.mutation_radius.has_value()) {
                continue;
            }
            app::SpecimenTrayItem item;
            item.genome = record.genome;
            item.pinned = record.favourite;
            item.provenance.mutation_policy_version = record.derivation.policy_version;
            item.provenance.parent_genome_identity = record.derivation.parent_genome_identities.front();
            item.provenance.mutation_seed = *record.derivation.seed;
            item.provenance.descendant_index = *record.derivation.descendant_index;
            item.provenance.radius = *record.derivation.mutation_radius;
            items.push_back(std::move(item));
        }
        return items;
    }

    void export_contact() {
        if (!owner_.session_.has_source()) {
            show_error_box(owner_.hwnd_, L"Load an image before exporting a contact sheet.");
            return;
        }
        if (owner_.session_.current_frame() == std::numeric_limits<std::uint64_t>::max()) {
            show_error_box(owner_.hwnd_, L"The maximum uint64 frame cannot be represented as a one-frame [start,end) manifest range. Seek to an earlier frame before exporting.");
            return;
        }
        std::vector<app::SpecimenTrayItem> items = retained_mutation_items();
        if (items.empty()) {
            show_error_box(owner_.hwnd_,
                L"No retained mutation specimens are available. Favourite or promote tray specimens first; the contact-sheet command exports that retained subset in lineage order.");
            return;
        }
        const auto path = choose_png_path(owner_.session_.source_path().stem().wstring() + L"-faultmine-contact.png");
        if (!path.has_value()) return;
        const auto policy = confirm_overwrite_policy(*path, false);
        if (!policy.has_value()) return;
        app::ExportOptions options;
        options.overwrite_policy = *policy;
        report_result(app::export_contact_sheet(owner_.session_, items, *path, {}, options),
            "Exported deterministic retained-specimen contact sheet plus cell provenance manifest.");
    }

    [[nodiscard]] bool create_progress_window(ProgressWindowState& state) const {
        WNDCLASSEXW klass{};
        klass.cbSize = static_cast<UINT>(sizeof(klass));
        klass.lpfnWndProc = progress_proc;
        klass.hInstance = owner_.instance_;
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        klass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        klass.lpszClassName = kProgressClass;
        const ATOM atom = RegisterClassExW(&klass);
        if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        state.window = CreateWindowExW(WS_EX_TOOLWINDOW, kProgressClass, L"FAULTMINE export progress",
            WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, 386, 118,
            owner_.hwnd_, nullptr, owner_.instance_, &state);
        return state.window != nullptr;
    }

    static void pump_progress(ProgressWindowState& state, const app::ExportProgress& progress) {
        if (state.label != nullptr) {
            const std::wstring text = L"Frame " + std::to_wstring(progress.current_frame) + L" | " +
                std::to_wstring(progress.completed) + L" / " + std::to_wstring(progress.total) + L" committed";
            SetWindowTextW(state.label, text.c_str());
        }
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE) != FALSE) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    void export_sequence() {
        if (!owner_.session_.has_source()) {
            show_error_box(owner_.hwnd_, L"Load an image before exporting a frame sequence.");
            return;
        }
        std::uint64_t start{};
        std::uint64_t end{};
        if (!prompt_sequence_range(owner_.hwnd_, owner_.session_.current_frame(), start, end)) return;
        const auto base = choose_png_path(owner_.session_.source_path().stem().wstring() + L"-faultmine-sequence.png");
        if (!base.has_value()) return;

        app::SequenceOptions sequence;
        sequence.frame_start_inclusive = start;
        sequence.frame_end_exclusive = end;
        app::ExportOptions options;
        options.overwrite_policy = app::ExportOverwritePolicy::fail_if_exists;

        ProgressWindowState progress;
        if (!create_progress_window(progress)) {
            show_error_box(owner_.hwnd_, L"Could not create export progress/cancellation window.");
            return;
        }
        progress_ = &progress;
        const auto run = [&](const app::ExportOverwritePolicy policy) {
            options.overwrite_policy = policy;
            return app::export_frame_sequence(owner_.session_, *base, sequence, options,
                [&](const app::ExportProgress& state) {
                    pump_progress(progress, state);
                    return !progress.cancelled;
                });
        };

        app::ExportResult result = run(app::ExportOverwritePolicy::fail_if_exists);
        if (!result.success && !result.cancelled && result.error.find("collision") != std::string::npos) {
            const auto replacement = confirm_overwrite_policy(*base, true);
            if (replacement.has_value()) {
                progress.cancelled = false;
                result = run(*replacement);
            }
        }
        progress_ = nullptr;
        if (progress.window != nullptr && IsWindow(progress.window) != FALSE) DestroyWindow(progress.window);
        report_result(result, "Exported canonical temporal PNG sequence plus per-frame audit manifest.");
    }

    [[nodiscard]] bool smoke_export(std::string* error) {
        const std::filesystem::path root = std::filesystem::temp_directory_path() /
            (L"faultmine-fm012-smoke-" + std::to_wstring(static_cast<unsigned long>(GetCurrentProcessId())));
        std::error_code fs_error;
        std::filesystem::remove_all(root, fs_error);
        fs_error.clear();
        std::filesystem::create_directories(root, fs_error);
        if (fs_error) {
            if (error != nullptr) *error = "could not create FM-012 native smoke export directory";
            return false;
        }
        const app::ExportResult still = app::export_canonical_still(owner_.session_, root / L"still.png");
        if (!still.success) {
            if (error != nullptr) *error = still.error;
            return false;
        }
        app::SequenceOptions sequence;
        sequence.frame_start_inclusive = 0U;
        sequence.frame_end_exclusive = 2U;
        const app::ExportResult frames = app::export_frame_sequence(owner_.session_, root / L"sequence.png", sequence);
        if (!frames.success) {
            if (error != nullptr) *error = frames.error;
            return false;
        }
        std::filesystem::remove_all(root, fs_error);
        return true;
    }

    MainWindow& owner_;
    WNDPROC old_proc_{};
    ProgressWindowState* progress_{};
    bool smoke_attempted_{};
};

void fm012_install_export_ui(MainWindow& owner) {
    if (owner.hwnd_ == nullptr || GetPropW(owner.hwnd_, kExportPropertyName) != nullptr) return;
    auto* ui = new ExportUi(owner);
    std::string error;
    if (!ui->attach(&error)) {
        delete ui;
        show_error_box(owner.hwnd_, L"FAULTMINE export UI initialization failed:\n" + utf8_to_wide(error));
        if (smoke_command_line()) PostQuitMessage(EXIT_FAILURE);
    }
}

}  // namespace
}  // namespace faultmine::platform::win32
