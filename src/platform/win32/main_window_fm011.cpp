#include <windows.h>

namespace faultmine::platform::win32 {
LRESULT temporal_call_window_proc(WNDPROC previous, HWND window, UINT message, WPARAM w_param, LPARAM l_param);
}

// FM-010 already subclasses the accepted native editor window. Interpose only
// its parent-proc forwarding call so FM-011 can add a thin presentation
// transport without copying or forking the large accepted editor/explorer UI.
#define CallWindowProcW ::faultmine::platform::win32::temporal_call_window_proc
#include "main_window_fm009.cpp"
#undef CallWindowProcW

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace faultmine::platform::win32 {
namespace {

#ifdef FAULTMINE_FM012_LAYER
void fm012_install_export_ui(MainWindow& owner);
#endif

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

struct TemporalUiState {
    HWND window{};
    bool installed{};
    bool playing{};
};

TemporalUiState g_temporal_ui;

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
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalSeekBackward);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalSeekForward);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalRateDown);
    UnregisterHotKey(owner.hwnd_, kHotkeyTemporalRateUp);
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
#ifdef FAULTMINE_FM012_LAYER
        const wchar_t* command_line = GetCommandLineW();
        const bool smoke_test = command_line != nullptr &&
            std::wstring_view{command_line}.find(L"--smoke-test") != std::wstring_view::npos;
        if (!smoke_test) fm012_install_export_ui(*owner);
#endif
        if (message == WM_COMMAND && handle_temporal_command(*owner, static_cast<UINT>(LOWORD(w_param)))) return 0;
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
