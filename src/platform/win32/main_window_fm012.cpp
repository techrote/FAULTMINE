#include <windows.h>

namespace faultmine::platform::win32 {
LRESULT temporal_call_window_proc(WNDPROC previous, HWND window, UINT message, WPARAM w_param, LPARAM l_param);
}

// FM-012 retains FM-011's temporal transport and interposes export commands at
// the same thin Win32 presentation boundary. Canonical export work lives in the
// dedicated export service; this file only collects explicit user intent and
// reports progress/cancellation.
#define CallWindowProcW ::faultmine::platform::win32::temporal_call_window_proc
#include "main_window_fm009.cpp"
#undef CallWindowProcW

#include "faultmine/export.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace faultmine::platform::win32 {
namespace {

constexpr UINT kTemporalTimerId = 0xFA11U;
constexpr UINT kCommandTemporalPlay = 1601U;
constexpr UINT kCommandTemporalStepForward = 1602U;
constexpr UINT kCommandTemporalStepBackward = 1603U;
constexpr UINT kCommandTemporalReset = 1604U;
constexpr UINT kCommandTemporalSeekForward = 1605U;
constexpr UINT kCommandTemporalSeekBackward = 1606U;
constexpr UINT kCommandTemporalRateUp = 1607U;
constexpr UINT kCommandTemporalRateDown = 1608U;

constexpr int kHotkeyTemporalPlay = 1701;
constexpr int kHotkeyTemporalStepForward = 1702;
constexpr int kHotkeyTemporalStepBackward = 1703;
constexpr int kHotkeyTemporalReset = 1704;
constexpr int kHotkeyTemporalSeekForward = 1705;
constexpr int kHotkeyTemporalSeekBackward = 1706;
constexpr int kHotkeyTemporalRateUp = 1707;
constexpr int kHotkeyTemporalRateDown = 1708;

constexpr UINT kCommandExportStill = 1801U;
constexpr UINT kCommandExportContact = 1802U;
constexpr UINT kCommandExportSequence = 1803U;
constexpr UINT kCommandExportManifest = 1804U;
constexpr UINT kCommandExportOverwrite = 1805U;

constexpr wchar_t kRangeWindowClass[] = L"FAULTMINE.ExportFrameRange";
constexpr UINT kRangeBeginEdit = 1901U;
constexpr UINT kRangeEndEdit = 1902U;

struct TemporalUiState {
    HWND window{};
    bool installed{};
    bool playing{};
    bool write_manifest{true};
    bool overwrite{};
    HMENU export_menu{};
};

TemporalUiState g_temporal_ui;

struct FrameRangePromptState {
    bool accepted{};
    std::uint64_t begin{};
    std::uint64_t end_exclusive{};
};

[[nodiscard]] bool parse_u64_edit(const HWND edit, std::uint64_t& output) {
    std::array<wchar_t, 64> buffer{};
    GetWindowTextW(edit, buffer.data(), static_cast<int>(buffer.size()));
    wchar_t* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::wcstoull(buffer.data(), &end, 10);
    if (errno == ERANGE || end == buffer.data() || end == nullptr || *end != L'\0') return false;
    output = static_cast<std::uint64_t>(parsed);
    return true;
}

LRESULT CALLBACK frame_range_proc(
    const HWND window,
    const UINT message,
    const WPARAM w_param,
    const LPARAM l_param) {
    auto* state = reinterpret_cast<FrameRangePromptState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        state = static_cast<FrameRangePromptState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    switch (message) {
        case WM_COMMAND:
            if (LOWORD(w_param) == IDOK && state != nullptr) {
                std::uint64_t begin{};
                std::uint64_t end{};
                if (!parse_u64_edit(GetDlgItem(window, kRangeBeginEdit), begin) ||
                    !parse_u64_edit(GetDlgItem(window, kRangeEndEdit), end) || begin >= end) {
                    MessageBoxW(window, L"Enter an unsigned begin frame and a larger exclusive end frame.", L"FAULTMINE frame range", MB_OK | MB_ICONERROR);
                    return 0;
                }
                state->begin = begin;
                state->end_exclusive = end;
                state->accepted = true;
                DestroyWindow(window);
                return 0;
            }
            if (LOWORD(w_param) == IDCANCEL) {
                DestroyWindow(window);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

[[nodiscard]] bool prompt_frame_range(
    MainWindow& owner,
    const std::uint64_t initial_begin,
    std::uint64_t& begin,
    std::uint64_t& end_exclusive) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = static_cast<UINT>(sizeof(window_class));
    window_class.lpfnWndProc = frame_range_proc;
    window_class.hInstance = owner.instance_;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    window_class.lpszClassName = kRangeWindowClass;
    const ATOM atom = RegisterClassExW(&window_class);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    FrameRangePromptState state;
    state.begin = initial_begin;
    state.end_exclusive = initial_begin > std::numeric_limits<std::uint64_t>::max() - 60U
        ? std::numeric_limits<std::uint64_t>::max()
        : initial_begin + 60U;
    if (state.end_exclusive <= state.begin) return false;

    RECT owner_rect{};
    GetWindowRect(owner.hwnd_, &owner_rect);
    const int width = 420;
    const int height = 190;
    const LONG owner_width = owner_rect.right - owner_rect.left;
    const LONG owner_height = owner_rect.bottom - owner_rect.top;
    const int x = static_cast<int>(owner_rect.left + std::max<LONG>(0L, (owner_width - static_cast<LONG>(width)) / 2L));
    const int y = static_cast<int>(owner_rect.top + std::max<LONG>(0L, (owner_height - static_cast<LONG>(height)) / 2L));
    HWND prompt = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kRangeWindowClass,
        L"Export frame sequence",
        WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
        x, y, width, height,
        owner.hwnd_, nullptr, owner.instance_, &state);
    if (prompt == nullptr) return false;

    const auto make = [&](const wchar_t* klass, const wchar_t* text, const DWORD style, const int cx, const int cy, const int cw, const int ch, const UINT id) {
        HWND control = CreateWindowExW(
            0, klass, text, WS_CHILD | WS_VISIBLE | style,
            cx, cy, cw, ch, prompt,
            id == 0U ? nullptr : reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), owner.instance_, nullptr);
        owner.set_default_font(control);
        return control;
    };
    make(L"STATIC", L"Begin frame (inclusive)", SS_LEFT, 18, 20, 160, 22, 0U);
    make(L"STATIC", L"End frame (exclusive)", SS_LEFT, 18, 58, 160, 22, 0U);
    HWND begin_edit = make(L"EDIT", std::to_wstring(state.begin).c_str(), ES_AUTOHSCROLL | WS_BORDER | ES_NUMBER, 184, 18, 200, 24, kRangeBeginEdit);
    make(L"EDIT", std::to_wstring(state.end_exclusive).c_str(), ES_AUTOHSCROLL | WS_BORDER | ES_NUMBER, 184, 56, 200, 24, kRangeEndEdit);
    make(L"STATIC", L"Sequence uses exact canonical frames. Press Esc during export to cancel after the current completed file.", SS_LEFT, 18, 92, 366, 36, 0U);
    make(L"BUTTON", L"Export", BS_DEFPUSHBUTTON, 222, 130, 78, 26, IDOK);
    make(L"BUTTON", L"Cancel", BS_PUSHBUTTON, 306, 130, 78, 26, IDCANCEL);
    SetFocus(begin_edit);

    EnableWindow(owner.hwnd_, FALSE);
    MSG message{};
    while (IsWindow(prompt) != FALSE) {
        const BOOL status = GetMessageW(&message, nullptr, 0U, 0U);
        if (status <= 0) {
            if (status == 0) PostQuitMessage(static_cast<int>(message.wParam));
            break;
        }
        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
            DestroyWindow(prompt);
            continue;
        }
        if (IsDialogMessageW(prompt, &message) == FALSE) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    EnableWindow(owner.hwnd_, TRUE);
    SetForegroundWindow(owner.hwnd_);
    if (!state.accepted) return false;
    begin = state.begin;
    end_exclusive = state.end_exclusive;
    return true;
}

[[nodiscard]] UINT timer_interval_ms(const app::SessionModel& session) noexcept {
    const app::TimelineRate rate = session.semantic_timeline_rate();
    const std::uint64_t preview_milli = session.preview_rate_milli();
    const std::uint64_t denominator = rate.numerator * preview_milli;
    if (denominator == 0U) return 1000U;
    const std::uint64_t numerator = 1000000ULL * rate.denominator;
    const std::uint64_t rounded = (numerator + denominator / 2U) / denominator;
    return static_cast<UINT>(std::clamp<std::uint64_t>(rounded, 1U, 60000U));
}

void update_timer(MainWindow& owner) {
    KillTimer(owner.hwnd_, kTemporalTimerId);
    if (g_temporal_ui.playing) {
        SetTimer(owner.hwnd_, kTemporalTimerId, timer_interval_ms(owner.session_), nullptr);
    }
}

void append_timeline_title(MainWindow& owner) {
    std::wstring title(1024U, L'\0');
    const int length = GetWindowTextW(owner.hwnd_, title.data(), static_cast<int>(title.size()));
    title.resize(length > 0 ? static_cast<std::size_t>(length) : 0U);
    const std::wstring marker = L" | frame ";
    if (const std::size_t found = title.find(marker); found != std::wstring::npos) title.resize(found);
    const app::TimelineRate semantic = owner.session_.semantic_timeline_rate();
    title += marker + std::to_wstring(owner.session_.current_frame());
    title += L" @ " + std::to_wstring(semantic.numerator) + L"/" + std::to_wstring(semantic.denominator) + L" fps";
    title += L" | preview " + std::to_wstring(owner.session_.preview_rate_milli()) + L"/1000x";
    title += g_temporal_ui.playing ? L" | PLAY" : L" | PAUSE";
    SetWindowTextW(owner.hwnd_, title.c_str());
}

void sync_export_menu_checks() {
    if (g_temporal_ui.export_menu == nullptr) return;
    CheckMenuItem(
        g_temporal_ui.export_menu,
        kCommandExportManifest,
        MF_BYCOMMAND | (g_temporal_ui.write_manifest ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(
        g_temporal_ui.export_menu,
        kCommandExportOverwrite,
        MF_BYCOMMAND | (g_temporal_ui.overwrite ? MF_CHECKED : MF_UNCHECKED));
}

void install_temporal_ui(MainWindow& owner) {
    if (g_temporal_ui.installed && g_temporal_ui.window == owner.hwnd_) return;
    g_temporal_ui = TemporalUiState{owner.hwnd_, true, false};

    HMENU timeline = CreatePopupMenu();
    AppendMenuW(timeline, MF_STRING, kCommandTemporalPlay, L"&Play / pause\tCtrl+Alt+Space");
    AppendMenuW(timeline, MF_STRING, kCommandTemporalStepBackward, L"Step &back one frame\tCtrl+Alt+,");
    AppendMenuW(timeline, MF_STRING, kCommandTemporalStepForward, L"Step &forward one frame\tCtrl+Alt+.");
    AppendMenuW(timeline, MF_STRING, kCommandTemporalReset, L"&Reset to frame 0\tCtrl+Alt+Home");
    AppendMenuW(timeline, MF_STRING, kCommandTemporalSeekBackward, L"Seek -10 frames\tCtrl+Alt+PageUp");
    AppendMenuW(timeline, MF_STRING, kCommandTemporalSeekForward, L"Seek +10 frames\tCtrl+Alt+PageDown");
    AppendMenuW(timeline, MF_SEPARATOR, 0U, nullptr);
    AppendMenuW(timeline, MF_STRING, kCommandTemporalRateDown, L"Preview rate /2\tCtrl+Alt+Down");
    AppendMenuW(timeline, MF_STRING, kCommandTemporalRateUp, L"Preview rate x2\tCtrl+Alt+Up");
    AppendMenuW(owner.menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(timeline), L"&Timeline");

    g_temporal_ui.export_menu = CreatePopupMenu();
    AppendMenuW(g_temporal_ui.export_menu, MF_STRING, kCommandExportStill, L"Canonical &still + provenance...\tCtrl+E");
    AppendMenuW(g_temporal_ui.export_menu, MF_STRING, kCommandExportContact, L"Retained exploration &contact sheet...");
    AppendMenuW(g_temporal_ui.export_menu, MF_STRING, kCommandExportSequence, L"Canonical frame &sequence...");
    AppendMenuW(g_temporal_ui.export_menu, MF_SEPARATOR, 0U, nullptr);
    AppendMenuW(g_temporal_ui.export_menu, MF_STRING | MF_CHECKED, kCommandExportManifest, L"Write provenance &manifest");
    AppendMenuW(g_temporal_ui.export_menu, MF_STRING, kCommandExportOverwrite, L"Allow explicit &overwrite");
    AppendMenuW(owner.menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(g_temporal_ui.export_menu), L"E&xport");
    sync_export_menu_checks();
    DrawMenuBar(owner.hwnd_);

    constexpr UINT modifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
    RegisterHotKey(owner.hwnd_, kHotkeyTemporalPlay, modifiers, VK_SPACE);
    RegisterHotKey(owner.hwnd_, kHotkeyTemporalStepBackward, modifiers, VK_OEM_COMMA);
    RegisterHotKey(owner.hwnd_, kHotkeyTemporalStepForward, modifiers, VK_OEM_PERIOD);
    RegisterHotKey(owner.hwnd_, kHotkeyTemporalReset, modifiers, VK_HOME);
    RegisterHotKey(owner.hwnd_, kHotkeyTemporalSeekBackward, modifiers, VK_PRIOR);
    RegisterHotKey(owner.hwnd_, kHotkeyTemporalSeekForward, modifiers, VK_NEXT);
    RegisterHotKey(owner.hwnd_, kHotkeyTemporalRateDown, modifiers, VK_DOWN);
    RegisterHotKey(owner.hwnd_, kHotkeyTemporalRateUp, modifiers, VK_UP);
    append_timeline_title(owner);
}

void uninstall_temporal_ui(MainWindow& owner) noexcept {
    KillTimer(owner.hwnd_, kTemporalTimerId);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalPlay);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalStepBackward);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalStepForward);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalReset);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalSeekForward);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalSeekBackward);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalRateUp);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalRateDown);
    g_temporal_ui = {};
}

void schedule_frame(MainWindow& owner) {
    owner.schedule_render();
    append_timeline_title(owner);
}

void toggle_play(MainWindow& owner) {
    g_temporal_ui.playing = !g_temporal_ui.playing;
    update_timer(owner);
    append_timeline_title(owner);
}

void change_preview_rate(MainWindow& owner, const bool increase) {
    const std::uint32_t current = owner.session_.preview_rate_milli();
    const std::uint32_t next = increase
        ? std::min<std::uint32_t>(8000U, current > 4000U ? 8000U : current * 2U)
        : std::max<std::uint32_t>(125U, current / 2U);
    owner.session_.set_preview_rate_milli(next);
    update_timer(owner);
    append_timeline_title(owner);
}

[[nodiscard]] bool choose_export_png(
    MainWindow& owner,
    const std::wstring& default_name,
    std::filesystem::path& destination) {
    std::array<wchar_t, 32768> filename{};
    std::copy_n(default_name.c_str(), std::min<std::size_t>(default_name.size(), filename.size() - 1U), filename.data());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = static_cast<DWORD>(sizeof(dialog));
    dialog.hwndOwner = owner.hwnd_;
    dialog.lpstrFilter = L"PNG image\0*.png\0All files\0*.*\0\0";
    dialog.lpstrDefExt = L"png";
    dialog.lpstrFile = filename.data();
    dialog.nMaxFile = static_cast<DWORD>(filename.size());
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (g_temporal_ui.overwrite) dialog.Flags |= OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&dialog) == FALSE) return false;
    destination = filename.data();
    return true;
}

void report_export_error(MainWindow& owner, const exporting::ExportError& error) {
    std::wstring message = L"Export failed during " + utf8_to_wide(error.operation) + L".\n\n";
    if (!error.path.empty()) message += error.path.wstring() + L"\n\n";
    message += utf8_to_wide(error.message);
    show_error_box(owner.hwnd_, message);
}

void export_still_ui(MainWindow& owner) {
    if (!owner.session_.has_source()) {
        show_error_box(owner.hwnd_, L"Load an image before exporting.");
        return;
    }
    std::filesystem::path destination;
    const std::wstring default_name = owner.session_.source_path().stem().wstring() + L"-faultmine.png";
    if (!choose_export_png(owner, default_name, destination)) return;

    exporting::StillExportRequest request;
    request.destination = destination;
    request.collision = g_temporal_ui.overwrite
        ? exporting::CollisionPolicy::overwrite
        : exporting::CollisionPolicy::fail_if_exists;
    request.write_manifest = g_temporal_ui.write_manifest;
    request.frame_index = owner.session_.current_frame();
    const exporting::ExportResult result = exporting::export_still(owner.session_, request);
    if (!result.ok()) {
        report_export_error(owner, *result.error);
        return;
    }
    owner.transient_status_ = "Exported canonical full-resolution still";
    owner.transient_status_ += request.write_manifest ? " + provenance manifest." : ".";
    owner.update_status();
}

[[nodiscard]] std::vector<exporting::ContactSheetSpecimen> retained_contact_specimens(MainWindow& owner) {
    std::vector<exporting::ContactSheetSpecimen> specimens;
    const auto& records = owner.session_.lineage().records();
    specimens.reserve(std::max<std::size_t>(records.size(), 1U));
    for (const app::SpecimenRecord& record : records) {
        specimens.push_back(exporting::ContactSheetSpecimen{
            record.creation_ordinal,
            record.genome,
            record.genome_identity});
    }
    if (specimens.empty()) {
        specimens.push_back(exporting::ContactSheetSpecimen{
            0U,
            owner.session_.genome(),
            owner.session_.genome_identity()});
    }
    return specimens;
}

void export_contact_ui(MainWindow& owner) {
    if (!owner.session_.has_source()) {
        show_error_box(owner.hwnd_, L"Load an image before exporting a contact sheet.");
        return;
    }
    std::filesystem::path destination;
    const std::wstring default_name = owner.session_.source_path().stem().wstring() + L"-faultmine-contact.png";
    if (!choose_export_png(owner, default_name, destination)) return;

    exporting::ContactSheetRequest request;
    request.destination = destination;
    request.specimens = retained_contact_specimens(owner);
    request.columns = 4U;
    request.cell_width = 320U;
    request.cell_height = 240U;
    request.frame_index = owner.session_.current_frame();
    request.collision = g_temporal_ui.overwrite
        ? exporting::CollisionPolicy::overwrite
        : exporting::CollisionPolicy::fail_if_exists;
    request.write_manifest = g_temporal_ui.write_manifest;

    (void)GetAsyncKeyState(VK_ESCAPE);
    const exporting::ExportResult result = exporting::export_contact_sheet(
        owner.session_,
        request,
        [] { return (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0; },
        [&](const std::size_t completed, const std::size_t total, const std::filesystem::path&) {
            owner.transient_status_ = "Contact sheet: " + std::to_string(completed) + "/" + std::to_string(total) + " specimens. Esc cancels.";
            owner.update_status();
            UpdateWindow(owner.status_);
        });
    if (!result.ok()) {
        report_export_error(owner, *result.error);
        return;
    }
    if (result.cancelled) {
        owner.transient_status_ = "Contact-sheet export cancelled before final commit; no incomplete final PNG was left.";
    } else {
        owner.transient_status_ = "Exported deterministic retained-exploration contact sheet";
        owner.transient_status_ += request.write_manifest ? " + cell mapping manifest." : ".";
    }
    owner.update_status();
}

void export_sequence_ui(MainWindow& owner) {
    if (!owner.session_.has_source()) {
        show_error_box(owner.hwnd_, L"Load an image before exporting a frame sequence.");
        return;
    }
    std::uint64_t begin{};
    std::uint64_t end_exclusive{};
    if (!prompt_frame_range(owner, owner.session_.current_frame(), begin, end_exclusive)) return;

    std::filesystem::path base_path;
    const std::wstring default_name = owner.session_.source_path().stem().wstring() + L"-faultmine-frame.png";
    if (!choose_export_png(owner, default_name, base_path)) return;
    const std::wstring stem_wide = base_path.stem().wstring();
    const std::string stem = wide_to_utf8(stem_wide);
    if (stem.empty()) {
        show_error_box(owner.hwnd_, L"The selected sequence naming stem could not be represented as UTF-8.");
        return;
    }

    exporting::FrameSequenceRequest request;
    request.directory = base_path.parent_path();
    if (request.directory.empty()) request.directory = std::filesystem::current_path();
    request.stem = stem;
    request.frame_begin = begin;
    request.frame_end_exclusive = end_exclusive;
    request.minimum_padding = 6U;
    request.collision = g_temporal_ui.overwrite
        ? exporting::CollisionPolicy::overwrite
        : exporting::CollisionPolicy::fail_if_exists;
    request.write_manifest = g_temporal_ui.write_manifest;

    g_temporal_ui.playing = false;
    update_timer(owner);
    (void)GetAsyncKeyState(VK_ESCAPE);
    const exporting::ExportResult result = exporting::export_frame_sequence(
        owner.session_,
        request,
        [] { return (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0; },
        [&](const std::size_t completed, const std::size_t total, const std::filesystem::path& path) {
            owner.transient_status_ = "Sequence: " + std::to_string(completed) + "/" + std::to_string(total) + " | " + wide_to_utf8(path.filename().wstring()) + " | Esc cancels.";
            owner.update_status();
            UpdateWindow(owner.status_);
        });
    if (!result.ok()) {
        report_export_error(owner, *result.error);
        return;
    }
    if (result.cancelled) {
        const std::size_t frame_files = result.manifest.has_value() ? result.manifest->frames.size() : 0U;
        owner.transient_status_ = "Sequence cancelled after " + std::to_string(frame_files) + " completed frame(s); completed files were retained atomically.";
    } else {
        const std::size_t frame_files = result.manifest.has_value() ? result.manifest->frames.size() : 0U;
        owner.transient_status_ = "Exported " + std::to_string(frame_files) + " canonical frame(s)";
        owner.transient_status_ += request.write_manifest ? " + sequence provenance manifest." : ".";
    }
    owner.update_status();
}

[[nodiscard]] bool handle_export_command(MainWindow& owner, const UINT command) {
    switch (command) {
        case command_export:
        case kCommandExportStill:
            export_still_ui(owner);
            return true;
        case kCommandExportContact:
            export_contact_ui(owner);
            return true;
        case kCommandExportSequence:
            export_sequence_ui(owner);
            return true;
        case kCommandExportManifest:
            g_temporal_ui.write_manifest = !g_temporal_ui.write_manifest;
            sync_export_menu_checks();
            return true;
        case kCommandExportOverwrite:
            g_temporal_ui.overwrite = !g_temporal_ui.overwrite;
            sync_export_menu_checks();
            return true;
        default:
            return false;
    }
}

[[nodiscard]] bool handle_temporal_command(MainWindow& owner, const UINT command) {
    switch (command) {
        case kCommandTemporalPlay: toggle_play(owner); return true;
        case kCommandTemporalStepForward:
            g_temporal_ui.playing = false; update_timer(owner); (void)owner.session_.step_frame_forward(); schedule_frame(owner); return true;
        case kCommandTemporalStepBackward:
            g_temporal_ui.playing = false; update_timer(owner); (void)owner.session_.step_frame_backward(); schedule_frame(owner); return true;
        case kCommandTemporalReset:
            g_temporal_ui.playing = false; update_timer(owner); owner.session_.reset_timeline(); schedule_frame(owner); return true;
        case kCommandTemporalSeekForward: {
            const std::uint64_t frame = owner.session_.current_frame();
            owner.session_.seek_frame(frame > std::numeric_limits<std::uint64_t>::max() - 10U
                ? std::numeric_limits<std::uint64_t>::max() : frame + 10U);
            schedule_frame(owner); return true;
        }
        case kCommandTemporalSeekBackward: {
            const std::uint64_t frame = owner.session_.current_frame();
            owner.session_.seek_frame(frame >= 10U ? frame - 10U : 0U);
            schedule_frame(owner); return true;
        }
        case kCommandTemporalRateUp: change_preview_rate(owner, true); return true;
        case kCommandTemporalRateDown: change_preview_rate(owner, false); return true;
        default: return false;
    }
}

[[nodiscard]] bool handle_temporal_hotkey(MainWindow& owner, const int id) {
    switch (id) {
        case kHotkeyTemporalPlay: return handle_temporal_command(owner, kCommandTemporalPlay);
        case kHotkeyTemporalStepForward: return handle_temporal_command(owner, kCommandTemporalStepForward);
        case kHotkeyTemporalStepBackward: return handle_temporal_command(owner, kCommandTemporalStepBackward);
        case kHotkeyTemporalReset: return handle_temporal_command(owner, kCommandTemporalReset);
        case kHotkeyTemporalSeekForward: return handle_temporal_command(owner, kCommandTemporalSeekForward);
        case kHotkeyTemporalSeekBackward: return handle_temporal_command(owner, kCommandTemporalSeekBackward);
        case kHotkeyTemporalRateUp: return handle_temporal_command(owner, kCommandTemporalRateUp);
        case kHotkeyTemporalRateDown: return handle_temporal_command(owner, kCommandTemporalRateDown);
        default: return false;
    }
}

}  // namespace

LRESULT temporal_call_window_proc(
    const WNDPROC previous,
    const HWND window,
    const UINT message,
    const WPARAM w_param,
    const LPARAM l_param) {
    auto* owner = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (owner != nullptr) {
        install_temporal_ui(*owner);
        if (message == WM_COMMAND) {
            const UINT command = static_cast<UINT>(LOWORD(w_param));
            if (handle_export_command(*owner, command) || handle_temporal_command(*owner, command)) return 0;
        }
        if (message == WM_KEYDOWN &&
            (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
            (w_param == 'E' || w_param == 'e')) {
            export_still_ui(*owner);
            return 0;
        }
        if (message == WM_HOTKEY && handle_temporal_hotkey(*owner, static_cast<int>(w_param))) return 0;
        if (message == WM_TIMER && w_param == kTemporalTimerId) {
            if (g_temporal_ui.playing && owner->session_.step_frame_forward()) schedule_frame(*owner);
            return 0;
        }
        if (message == WM_DESTROY) uninstall_temporal_ui(*owner);
    }

    const LRESULT result = ::CallWindowProcW(previous, window, message, w_param, l_param);
    if (owner != nullptr) {
        if (message == kRenderMessage) append_timeline_title(*owner);
        if (message == WM_COMMAND) {
            const UINT command = static_cast<UINT>(LOWORD(w_param));
            if (command == command_open_image || command == command_open_project) {
                g_temporal_ui.playing = false;
                update_timer(*owner);
                owner->session_.reset_timeline();
                owner->schedule_render();
            }
        }
    }
    return result;
}

}  // namespace faultmine::platform::win32
