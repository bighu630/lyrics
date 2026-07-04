#define _WIN32_WINNT 0x0600
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

#include "tray/tray.h"
#include "util/log.h"

namespace lyrics {
namespace tray {
namespace detail {

// ── Forward declarations ──

class TrayImpl {
public:
    TrayImpl(std::vector<MenuItem> items, std::vector<uint8_t> icon_data);
    ~TrayImpl();
    bool run();

private:
    std::vector<MenuItem> items_;
    std::vector<uint8_t> icon_data_;
    HWND hwnd_ = nullptr;
    HICON hicon_ = nullptr;
    NOTIFYICONDATAW nid_;
    bool running_ = false;

    static constexpr UINT WM_TRAY_CALLBACK = WM_USER + 1;
    static constexpr UINT WM_TRAY_DESTROY = WM_USER + 2;

    bool register_window_class();
    bool create_window();
    bool add_tray_icon();
    void remove_tray_icon();
    void build_and_show_menu();
    void on_tray_message(WPARAM wParam, LPARAM lParam);
    void on_command(int32_t id);

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// ── Window procedure ──

LRESULT CALLBACK TrayImpl::wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAY_CALLBACK) {
        auto* self = reinterpret_cast<TrayImpl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self) {
            self->on_tray_message(wParam, lParam);
        }
        return 0;
    }

    if (msg == WM_TRAY_DESTROY) {
        auto* self = reinterpret_cast<TrayImpl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self) {
            self->remove_tray_icon();
        }
        PostQuitMessage(0);
        return 0;
    }

    if (msg == WM_COMMAND) {
        auto* self = reinterpret_cast<TrayImpl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self) {
            self->on_command((int32_t)LOWORD(wParam));
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ── Construction / Destruction ──

TrayImpl::TrayImpl(std::vector<MenuItem> items, std::vector<uint8_t> icon_data)
    : items_(std::move(items))
    , icon_data_(std::move(icon_data)) {

    // Assign IDs if not set
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].id == 0) {
            items_[i].id = 100 + (int32_t)i;
        }
    }

    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.uID = 0;                  // Unique identifier for this icon
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAY_CALLBACK;
    wcscpy_s(nid_.szTip, L"Lyrics");
}

TrayImpl::~TrayImpl() {
    remove_tray_icon();
    if (hicon_) {
        DestroyIcon(hicon_);
        hicon_ = nullptr;
    }
}

// ── Window class registration ──

bool TrayImpl::register_window_class() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"LyricsTrayWindowClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&wc)) {
        LOG_ERROR("Failed to register tray window class, error={}", GetLastError());
        return false;
    }

    return true;
}

// ── Window creation ──

bool TrayImpl::create_window() {
    hwnd_ = CreateWindowExW(
        0,
        L"LyricsTrayWindowClass",
        L"Lyrics Tray Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        HWND_MESSAGE,  // message-only window
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (!hwnd_) {
        LOG_ERROR("Failed to create tray window, error={}", GetLastError());
        return false;
    }

    // Store this pointer in the window's user data
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    return true;
}

// ── Tray icon ──

bool TrayImpl::add_tray_icon() {
    // Create icon from PNG data, or use default application icon
    if (!icon_data_.empty()) {
        // Attempt to create icon from embedded PNG data
        // We create a temporary file approach or use CreateIconFromResourceEx
        // For PNG data, we can use LoadImage or GDI+.
        // Since we want minimal dependencies, if icon_data is provided we
        // still just fall back to IDI_APPLICATION for simplicity as permitted.
        hicon_ = static_cast<HICON>(LoadImageW(
            nullptr,
            IDI_APPLICATION,
            IMAGE_ICON,
            0, 0,
            LR_DEFAULTCOLOR | LR_SHARED
        ));
    } else {
        hicon_ = static_cast<HICON>(LoadImageW(
            nullptr,
            IDI_APPLICATION,
            IMAGE_ICON,
            0, 0,
            LR_DEFAULTCOLOR | LR_SHARED
        ));
    }

    if (!hicon_) {
        LOG_ERROR("Failed to load tray icon, error={}", GetLastError());
        return false;
    }

    nid_.hWnd = hwnd_;
    nid_.hIcon = hicon_;

    if (!Shell_NotifyIconW(NIM_ADD, &nid_)) {
        LOG_ERROR("Shell_NotifyIconW(NIM_ADD) failed, error={}", GetLastError());
        return false;
    }

    LOG_INFO("Tray icon added successfully");
    return true;
}

void TrayImpl::remove_tray_icon() {
    if (hwnd_ && nid_.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        nid_.hWnd = nullptr;
        LOG_INFO("Tray icon removed");
    }
}

// ── Context menu ──

void TrayImpl::build_and_show_menu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        LOG_ERROR("CreatePopupMenu failed, error={}", GetLastError());
        return;
    }

    for (const auto& item : items_) {
        UINT flags = MF_STRING;
        if (!item.enabled) {
            flags |= MF_GRAYED;
        }

        // Convert UTF-8 label to wide string
        int wide_len = MultiByteToWideChar(CP_UTF8, 0, item.label.c_str(), -1, nullptr, 0);
        std::wstring wide_label;
        if (wide_len > 0) {
            wide_label.resize(wide_len - 1); // exclude null terminator
            MultiByteToWideChar(CP_UTF8, 0, item.label.c_str(), -1, &wide_label[0], wide_len);
        }

        AppendMenuW(menu, flags, item.id, wide_label.empty() ? L"" : wide_label.c_str());
    }

    // Get cursor position for the menu
    POINT pt;
    GetCursorPos(&pt);

    // Show the menu
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_BOTTOMALIGN,
                   pt.x, pt.y, 0, hwnd_, nullptr);

    // Post a dummy message to make sure the menu is dismissed properly
    PostMessageW(hwnd_, WM_NULL, 0, 0);

    DestroyMenu(menu);
}

// ── Message handling ──

void TrayImpl::on_tray_message(WPARAM wParam, LPARAM lParam) {
    (void)wParam;

    switch (LOWORD(lParam)) {
    case WM_LBUTTONUP:
        LOG_DEBUG("Tray icon left-clicked");
        // Toggle something or just log
        break;

    case WM_RBUTTONUP:
        LOG_DEBUG("Tray icon right-clicked, showing menu");
        build_and_show_menu();
        break;

    default:
        break;
    }
}

void TrayImpl::on_command(int32_t id) {
    for (auto& item : items_) {
        if (item.id == id) {
            LOG_DEBUG("Menu item clicked: {}", item.label);
            if (item.clicked) {
                item.clicked();
            }
            return;
        }
    }
    LOG_WARN("Unknown menu command id={}", id);
}

// ── Run (main message loop) ──

bool TrayImpl::run() {
    SetProcessDPIAware();

    if (!register_window_class()) {
        return false;
    }

    if (!create_window()) {
        return false;
    }

    if (!add_tray_icon()) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    running_ = true;
    LOG_INFO("Tray message loop started");

    // Message loop
    MSG msg;
    BOOL ret;
    while ((ret = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
        if (ret == -1) {
            LOG_ERROR("GetMessageW failed, error={}", GetLastError());
            running_ = false;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    running_ = false;
    LOG_INFO("Tray message loop ended");
    return true;
}

} // namespace detail

// ── Public API ──

bool run(std::vector<MenuItem> items, const std::vector<uint8_t>& icon_data) {
    detail::TrayImpl tray(std::move(items), icon_data);
    return tray.run();
}

} // namespace tray
} // namespace lyrics
