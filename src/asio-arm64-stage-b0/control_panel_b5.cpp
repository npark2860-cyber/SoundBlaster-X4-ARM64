#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <cwchar>

#include "control_panel_b5.h"

namespace {

constexpr wchar_t kPrefsKey[] = L"Software\\SoundBlaster-X4-ARM64\\ASIO B5";
constexpr wchar_t kPrefs48_96[] = L"PreferredBufferFrames48_96";
constexpr wchar_t kPrefs192[] = L"PreferredBufferFrames192";
constexpr long kMinFrames48_96 = 96;
constexpr long kMinFrames192 = 384;
constexpr long kMaxFrames = 4800;
constexpr long kDefaultFrames48_96 = 240;
constexpr long kDefaultFrames192 = 384;
constexpr long kGranularity = 48;
constexpr long kCompatibilityFrames = 512;

constexpr int kIdBufferCombo = 1201;
constexpr int kIdApply = 1202;
constexpr int kIdCopyDiagnostics = 1203;

bool rate_is_192(double sample_rate) {
    return sample_rate == 192000.0;
}

long default_frames(double sample_rate) {
    return rate_is_192(sample_rate) ? kDefaultFrames192 : kDefaultFrames48_96;
}

long min_frames(double sample_rate) {
    return rate_is_192(sample_rate) ? kMinFrames192 : kMinFrames48_96;
}

bool valid_preference(double sample_rate, long frames) {
    if (frames == kCompatibilityFrames) return true;
    return frames >= min_frames(sample_rate) && frames <= kMaxFrames &&
           (frames % kGranularity) == 0;
}

const wchar_t* preference_value_name(double sample_rate) {
    return rate_is_192(sample_rate) ? kPrefs192 : kPrefs48_96;
}

bool save_preference(double sample_rate, long frames) {
    if (!valid_preference(sample_rate, frames)) return false;

    HKEY key = nullptr;
    const LONG open_result = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kPrefsKey,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);
    if (open_result != ERROR_SUCCESS) return false;

    const DWORD value = static_cast<DWORD>(frames);
    const LONG set_result = RegSetValueExW(
        key,
        preference_value_name(sample_rate),
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        sizeof(value));
    RegCloseKey(key);
    return set_result == ERROR_SUCCESS;
}

int scale_px(UINT dpi, int value) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

void set_font(HWND control, HFONT font) {
    if (control && font) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

HWND make_control(
    HWND parent,
    const wchar_t* class_name,
    const wchar_t* text,
    DWORD style,
    int x,
    int y,
    int width,
    int height,
    int id,
    HFONT font) {

    HWND control = CreateWindowExW(
        0,
        class_name,
        text,
        WS_CHILD | WS_VISIBLE | style,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        nullptr,
        nullptr);
    set_font(control, font);
    return control;
}

void format_rate(double sample_rate, wchar_t* text, size_t count) {
    const long rate = static_cast<long>(sample_rate);
    if (rate == 48000) wcscpy_s(text, count, L"48,000 Hz");
    else if (rate == 96000) wcscpy_s(text, count, L"96,000 Hz");
    else if (rate == 192000) wcscpy_s(text, count, L"192,000 Hz");
    else swprintf_s(text, count, L"%ld Hz", rate);
}

void format_frames(long frames, wchar_t* text, size_t count) {
    if (frames > 0) swprintf_s(text, count, L"%ld samples", frames);
    else wcscpy_s(text, count, L"Not active");
}

void format_frames_ms(long frames, double sample_rate, wchar_t* text, size_t count) {
    if (frames <= 0 || sample_rate <= 0.0) {
        wcscpy_s(text, count, L"Not active");
        return;
    }
    const double ms = (static_cast<double>(frames) * 1000.0) / sample_rate;
    swprintf_s(text, count, L"%ld samples / %.2f ms", frames, ms);
}

void status_to_wide(const char* source, wchar_t* destination, int count) {
    if (!source || !*source) {
        wcscpy_s(destination, count, L"None");
        return;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, source, -1, destination, count) == 0) {
        if (MultiByteToWideChar(CP_ACP, 0, source, -1, destination, count) == 0) {
            wcscpy_s(destination, count, L"Unavailable");
        }
    }
}

void failure_log_path(wchar_t* path, size_t count) {
    if (!path || count == 0) return;
    path[0] = L'\0';
    wchar_t temp[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, temp);
    if (length == 0 || length >= MAX_PATH) {
        wcscpy_s(path, count, L"%TEMP%\\B5_RUNTIME_FAILURE.txt");
        return;
    }
    wcscpy_s(path, count, temp);
    wcscat_s(path, count, L"B5_RUNTIME_FAILURE.txt");
}

const wchar_t* architecture_name() {
#if defined(_M_ARM64EC)
    return L"ARM64EC";
#elif defined(_M_ARM64)
    return L"Classic ARM64";
#else
    return L"Windows 64-bit";
#endif
}

struct PanelContext {
    B5ControlPanelState state{};
    long applied_frames = 0;
    HWND combo = nullptr;
    HWND apply = nullptr;
    HWND note = nullptr;
    HWND diagnostics_edit = nullptr;
    HFONT font = nullptr;
    HFONT title_font = nullptr;
    wchar_t diagnostics[2048]{};
};

long selected_frames(const PanelContext& context) {
    if (!context.combo) return 0;
    const LRESULT index = SendMessageW(context.combo, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR) return 0;
    const LRESULT item = SendMessageW(context.combo, CB_GETITEMDATA, static_cast<WPARAM>(index), 0);
    if (item == CB_ERR) return 0;
    return static_cast<long>(item);
}

void update_apply_enabled(PanelContext& context) {
    if (!context.apply) return;
    EnableWindow(context.apply, selected_frames(context) != context.applied_frames);
}

void add_buffer_item(HWND combo, long frames, double sample_rate, long selected) {
    wchar_t text[80]{};
    format_frames_ms(frames, sample_rate, text, _countof(text));
    const LRESULT index = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    if (index == CB_ERR || index == CB_ERRSPACE) return;
    SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(index), static_cast<LPARAM>(frames));
    if (frames == selected) {
        SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
    }
}

void populate_buffer_combo(PanelContext& context) {
    const long selected = context.applied_frames;
    bool compatibility_added = false;
    for (long frames = min_frames(context.state.sample_rate);
         frames <= kMaxFrames;
         frames += kGranularity) {
        if (!compatibility_added && kCompatibilityFrames < frames) {
            add_buffer_item(context.combo, kCompatibilityFrames, context.state.sample_rate, selected);
            compatibility_added = true;
        }
        add_buffer_item(context.combo, frames, context.state.sample_rate, selected);
    }
    if (!compatibility_added) {
        add_buffer_item(context.combo, kCompatibilityFrames, context.state.sample_rate, selected);
    }

    if (SendMessageW(context.combo, CB_GETCURSEL, 0, 0) == CB_ERR) {
        const long fallback = default_frames(context.state.sample_rate);
        const LRESULT count = SendMessageW(context.combo, CB_GETCOUNT, 0, 0);
        for (LRESULT index = 0; index < count; ++index) {
            if (static_cast<long>(SendMessageW(context.combo, CB_GETITEMDATA, index, 0)) == fallback) {
                SendMessageW(context.combo, CB_SETCURSEL, index, 0);
                context.applied_frames = fallback;
                break;
            }
        }
    }
}

void build_diagnostics(PanelContext& context) {
    wchar_t rate[64]{};
    wchar_t active[96]{};
    wchar_t preferred[96]{};
    wchar_t last_status[256]{};
    wchar_t log_path[MAX_PATH + 64]{};

    format_rate(context.state.sample_rate, rate, _countof(rate));
    format_frames_ms(
        context.state.buffers_created ? context.state.active_buffer_frames : 0,
        context.state.sample_rate,
        active,
        _countof(active));
    format_frames_ms(context.applied_frames, context.state.sample_rate, preferred, _countof(preferred));
    status_to_wide(context.state.last_status, last_status, _countof(last_status));
    failure_log_path(log_path, _countof(log_path));

    swprintf_s(
        context.diagnostics,
        _countof(context.diagnostics),
        L"Sound Blaster X4 ARM64 ASIO B5\r\n"
        L"Driver: 2.00 (%ls)\r\n"
        L"Sample rate: %ls\r\n"
        L"Active/effective buffer: %ls\r\n"
        L"Preferred next-open buffer: %ls\r\n"
        L"Buffers created: %ls\r\n"
        L"Worker running: %ls\r\n"
        L"Last driver status: %ls\r\n"
        L"Fatal runtime log: %ls",
        architecture_name(),
        rate,
        active,
        preferred,
        context.state.buffers_created ? L"Yes" : L"No",
        context.state.worker_running ? L"Yes" : L"No",
        last_status,
        log_path);
}

void refresh_diagnostics(PanelContext& context) {
    build_diagnostics(context);
    if (context.diagnostics_edit) SetWindowTextW(context.diagnostics_edit, context.diagnostics);
}

bool copy_to_clipboard(HWND owner, const wchar_t* text) {
    if (!text || !OpenClipboard(owner)) return false;
    EmptyClipboard();

    const SIZE_T bytes = (wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) {
        CloseClipboard();
        return false;
    }

    void* destination = GlobalLock(memory);
    if (!destination) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    memcpy(destination, text, bytes);
    GlobalUnlock(memory);

    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

void center_dialog(HWND dialog, HWND owner) {
    RECT dialog_rect{};
    GetWindowRect(dialog, &dialog_rect);
    const int width = dialog_rect.right - dialog_rect.left;
    const int height = dialog_rect.bottom - dialog_rect.top;

    RECT target{};
    if (owner && IsWindow(owner)) {
        GetWindowRect(owner, &target);
    } else {
        const HMONITOR monitor = MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{sizeof(info)};
        if (GetMonitorInfoW(monitor, &info)) target = info.rcWork;
        else SystemParametersInfoW(SPI_GETWORKAREA, 0, &target, 0);
    }

    const int x = target.left + ((target.right - target.left) - width) / 2;
    const int y = target.top + ((target.bottom - target.top) - height) / 2;
    SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool apply_selected_preference(HWND dialog, PanelContext& context) {
    const long frames = selected_frames(context);
    if (!valid_preference(context.state.sample_rate, frames)) {
        MessageBoxW(dialog, L"The selected buffer size is not valid for this sample rate.",
                    L"Sound Blaster X4 ARM64 ASIO B5", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (!save_preference(context.state.sample_rate, frames)) {
        MessageBoxW(dialog, L"The buffer preference could not be saved for this user.",
                    L"Sound Blaster X4 ARM64 ASIO B5", MB_OK | MB_ICONERROR);
        return false;
    }

    context.applied_frames = frames;
    refresh_diagnostics(context);
    update_apply_enabled(context);
    if (context.note) {
        SetWindowTextW(
            context.note,
            context.state.buffers_created || context.state.worker_running
                ? L"Saved. The current stream is unchanged; reopen the ASIO device/host to use this preference."
                : L"Saved for the next buffer creation. The ASIO host still chooses the buffer passed to createBuffers().");
    }
    return true;
}

INT_PTR CALLBACK panel_proc(HWND dialog, UINT message, WPARAM w_param, LPARAM l_param) {
    auto* context = reinterpret_cast<PanelContext*>(GetWindowLongPtrW(dialog, DWLP_USER));

    switch (message) {
    case WM_INITDIALOG: {
        context = reinterpret_cast<PanelContext*>(l_param);
        if (!context) return FALSE;
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
        SetWindowTextW(dialog, L"Sound Blaster X4 ARM64 ASIO B5");

        const UINT dpi = GetDpiForWindow(dialog);
        RECT desired_client{0, 0, scale_px(dpi, 520), scale_px(dpi, 482)};
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(dialog, GWL_STYLE));
        const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(dialog, GWL_EXSTYLE));
        AdjustWindowRectExForDpi(&desired_client, style, FALSE, ex_style, dpi);
        const int outer_width = desired_client.right - desired_client.left;
        const int outer_height = desired_client.bottom - desired_client.top;
        SetWindowPos(dialog, nullptr, 0, 0, outer_width, outer_height,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        center_dialog(dialog, GetParent(dialog));

        RECT client{};
        GetClientRect(dialog, &client);
        const int margin = scale_px(dpi, 18);
        const int content_width = (client.right - client.left) - (margin * 2);

        context->font = CreateFontW(
            -MulDiv(9, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        context->title_font = CreateFontW(
            -MulDiv(13, static_cast<int>(dpi), 72), 0, 0, 0, FW_SEMIBOLD,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        set_font(dialog, context->font);

        int y = scale_px(dpi, 16);
        make_control(
            dialog, L"STATIC", L"Sound Blaster X4 ARM64 ASIO B5",
            SS_LEFT, margin, y, content_width, scale_px(dpi, 28), 0, context->title_font);
        y += scale_px(dpi, 27);

        wchar_t version_line[96]{};
        swprintf_s(version_line, _countof(version_line), L"Driver 2.00  •  %ls", architecture_name());
        make_control(dialog, L"STATIC", version_line, SS_LEFT,
                     margin, y, content_width, scale_px(dpi, 20), 0, context->font);
        y += scale_px(dpi, 28);

        const int group_height = scale_px(dpi, 112);
        make_control(dialog, L"BUTTON", L"Audio Status", BS_GROUPBOX,
                     margin, y, content_width, group_height, 0, context->font);

        wchar_t rate_text[64]{};
        wchar_t buffer_text[64]{};
        wchar_t latency_text[96]{};
        format_rate(context->state.sample_rate, rate_text, _countof(rate_text));
        format_frames(
            context->state.buffers_created ? context->state.active_buffer_frames : 0,
            buffer_text,
            _countof(buffer_text));
        format_frames_ms(
            context->state.buffers_created ? context->state.active_buffer_frames : 0,
            context->state.sample_rate,
            latency_text,
            _countof(latency_text));

        const int label_x = margin + scale_px(dpi, 16);
        const int value_x = margin + scale_px(dpi, 176);
        const int value_width = content_width - scale_px(dpi, 192);
        int row_y = y + scale_px(dpi, 25);
        make_control(dialog, L"STATIC", L"Sample Rate", SS_LEFT,
                     label_x, row_y, scale_px(dpi, 150), scale_px(dpi, 20), 0, context->font);
        make_control(dialog, L"STATIC", rate_text, SS_LEFT,
                     value_x, row_y, value_width, scale_px(dpi, 20), 0, context->font);
        row_y += scale_px(dpi, 27);
        make_control(dialog, L"STATIC", L"Current Buffer", SS_LEFT,
                     label_x, row_y, scale_px(dpi, 150), scale_px(dpi, 20), 0, context->font);
        make_control(dialog, L"STATIC", buffer_text, SS_LEFT,
                     value_x, row_y, value_width, scale_px(dpi, 20), 0, context->font);
        row_y += scale_px(dpi, 27);
        make_control(dialog, L"STATIC", L"Effective Latency", SS_LEFT,
                     label_x, row_y, scale_px(dpi, 150), scale_px(dpi, 20), 0, context->font);
        make_control(dialog, L"STATIC", latency_text, SS_LEFT,
                     value_x, row_y, value_width, scale_px(dpi, 20), 0, context->font);

        y += group_height + scale_px(dpi, 10);
        const int latency_group_height = scale_px(dpi, 112);
        make_control(dialog, L"BUTTON", L"Latency / Buffer Size", BS_GROUPBOX,
                     margin, y, content_width, latency_group_height, 0, context->font);
        make_control(dialog, L"STATIC", L"Preferred for next open", SS_LEFT,
                     label_x, y + scale_px(dpi, 28), scale_px(dpi, 155), scale_px(dpi, 20), 0, context->font);

        context->combo = CreateWindowExW(
            0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            value_x, y + scale_px(dpi, 23), value_width, scale_px(dpi, 260),
            dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBufferCombo)), nullptr, nullptr);
        set_font(context->combo, context->font);
        populate_buffer_combo(*context);

        context->note = make_control(
            dialog,
            L"STATIC",
            context->state.buffers_created || context->state.worker_running
                ? L"The current stream will not be resized. Apply/OK saves this for the next ASIO reopen."
                : L"Apply/OK saves the preference. The ASIO host remains authoritative for createBuffers(bufferSize).",
            SS_LEFT,
            label_x,
            y + scale_px(dpi, 60),
            content_width - scale_px(dpi, 32),
            scale_px(dpi, 38),
            0,
            context->font);

        y += latency_group_height + scale_px(dpi, 10);
        const int diagnostics_height = scale_px(dpi, 112);
        make_control(dialog, L"BUTTON", L"Diagnostics", BS_GROUPBOX,
                     margin, y, content_width, diagnostics_height, 0, context->font);

        build_diagnostics(*context);
        context->diagnostics_edit = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            context->diagnostics,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            label_x,
            y + scale_px(dpi, 23),
            content_width - scale_px(dpi, 132),
            scale_px(dpi, 75),
            dialog,
            nullptr,
            nullptr,
            nullptr);
        set_font(context->diagnostics_edit, context->font);
        make_control(dialog, L"BUTTON", L"Copy", BS_PUSHBUTTON | WS_TABSTOP,
                     margin + content_width - scale_px(dpi, 98),
                     y + scale_px(dpi, 23),
                     scale_px(dpi, 82),
                     scale_px(dpi, 27),
                     kIdCopyDiagnostics,
                     context->font);

        y += diagnostics_height + scale_px(dpi, 14);
        const int button_width = scale_px(dpi, 78);
        const int button_height = scale_px(dpi, 28);
        const int gap = scale_px(dpi, 8);
        const int right = margin + content_width;

        make_control(dialog, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP,
                     right - button_width, y, button_width, button_height, IDCANCEL, context->font);
        make_control(dialog, L"BUTTON", L"OK", BS_DEFPUSHBUTTON | WS_TABSTOP,
                     right - (button_width * 2) - gap, y, button_width, button_height, IDOK, context->font);
        context->apply = make_control(
            dialog, L"BUTTON", L"Apply", BS_PUSHBUTTON | WS_TABSTOP,
            right - (button_width * 3) - (gap * 2), y, button_width, button_height,
            kIdApply, context->font);
        update_apply_enabled(*context);

        SetFocus(context->combo);
        return FALSE;
    }

    case WM_COMMAND:
        if (!context) return FALSE;
        switch (LOWORD(w_param)) {
        case kIdBufferCombo:
            if (HIWORD(w_param) == CBN_SELCHANGE) update_apply_enabled(*context);
            return TRUE;
        case kIdApply:
            apply_selected_preference(dialog, *context);
            return TRUE;
        case kIdCopyDiagnostics:
            refresh_diagnostics(*context);
            copy_to_clipboard(dialog, context->diagnostics);
            return TRUE;
        case IDOK:
            if (apply_selected_preference(dialog, *context)) EndDialog(dialog, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        default:
            break;
        }
        break;

    case WM_CLOSE:
        EndDialog(dialog, IDCANCEL);
        return TRUE;

    case WM_DESTROY:
        if (context) {
            if (context->title_font) {
                DeleteObject(context->title_font);
                context->title_font = nullptr;
            }
            if (context->font) {
                DeleteObject(context->font);
                context->font = nullptr;
            }
        }
        return TRUE;

    default:
        break;
    }

    return FALSE;
}

#pragma pack(push, 2)
struct RuntimeDialogTemplate {
    DLGTEMPLATE dialog;
    WORD menu;
    WORD window_class;
    wchar_t title[48];
};
#pragma pack(pop)

} // namespace

long b5_load_preferred_buffer_frames(double sample_rate) {
    DWORD value = 0;
    DWORD bytes = sizeof(value);
    DWORD type = 0;
    const LONG result = RegGetValueW(
        HKEY_CURRENT_USER,
        kPrefsKey,
        preference_value_name(sample_rate),
        RRF_RT_REG_DWORD,
        &type,
        &value,
        &bytes);

    const long frames = result == ERROR_SUCCESS ? static_cast<long>(value) : default_frames(sample_rate);
    return valid_preference(sample_rate, frames) ? frames : default_frames(sample_rate);
}

bool b5_show_control_panel(
    HINSTANCE module,
    HWND owner_window,
    const B5ControlPanelState& state) {

    if (!module) return false;

    PanelContext context{};
    context.state = state;
    context.applied_frames = b5_load_preferred_buffer_frames(state.sample_rate);

    alignas(DWORD) RuntimeDialogTemplate runtime_template{};
    runtime_template.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    runtime_template.dialog.dwExtendedStyle = WS_EX_CONTROLPARENT;
    runtime_template.dialog.cdit = 0;
    runtime_template.dialog.x = 0;
    runtime_template.dialog.y = 0;
    runtime_template.dialog.cx = 320;
    runtime_template.dialog.cy = 280;
    runtime_template.menu = 0;
    runtime_template.window_class = 0;
    wcscpy_s(runtime_template.title, L"Sound Blaster X4 ARM64 ASIO B5");

    HWND owner = owner_window && IsWindow(owner_window) ? owner_window : nullptr;
    const INT_PTR result = DialogBoxIndirectParamW(
        module,
        reinterpret_cast<const DLGTEMPLATE*>(&runtime_template),
        owner,
        panel_proc,
        reinterpret_cast<LPARAM>(&context));
    return result != -1;
}
