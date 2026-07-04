// Lyrics Display System — C++17 Unified Binary
//
// Usage:
//   lyrics                    Start backend + GTK4 transparent lyrics overlay
//   lyrics --console / -c     Start backend + terminal TUI mode
//   lyrics --no-gui           Start backend only (no GUI/TUI, just /dev/shm/lyrics
//                             and i3block signal, for Waybar users)

#include "app.h"
#include "config.h"
#include "util/log.h"

#ifndef _WIN32
#include "ui/gui.h"
#endif
#include "ui/console_tui.h"
#include "tray/tray.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// ── Global stop flag ──────────────────────────────────────────────
static std::atomic<bool> g_stop_flag{false};

extern "C" {
static void signal_handler(int) {
    g_stop_flag.store(true);
}
}

// ── Icon data (16x16 PNG, embedded) ───────────────────────────────
static std::vector<uint8_t> icon_data() {
    return {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x91, 0x68,
        0x36, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47,
        0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
        0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00,
        0xB1, 0x8F, 0x0B, 0xFC, 0x61, 0x05, 0x00, 0x00,
        0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
        0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3, 0x01, 0xC7,
        0x6F, 0xA8, 0x64, 0x00, 0x00, 0x00, 0x0C, 0x49,
        0x44, 0x41, 0x54, 0x38, 0x4F, 0x63, 0x60, 0x60,
        0x60, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0xBF,
        0x6F, 0x0D, 0xA2, 0x00, 0x00, 0x00, 0x00, 0x49,
        0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
    };
}

// ── Mode parsing ──────────────────────────────────────────────────

enum class Mode { GUI, TUI, NoGUI };

struct ParsedArgs {
    Mode mode = Mode::GUI;
    std::vector<std::string> remaining;
};

static ParsedArgs parse_args(int argc, char* argv[]) {
    ParsedArgs result;
    result.remaining.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--console" || arg == "-c") {
            result.mode = Mode::TUI;
        } else if (arg == "--no-gui") {
            result.mode = Mode::NoGUI;
        } else {
            result.remaining.push_back(arg);
        }
    }
    return result;
}

// ── Main ──────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    auto args = parse_args(argc, argv);

    // Load configuration
    auto cfg = lyrics::Config::load();

    // Initialize logger
    lyrics::init_logger(cfg.log_level);
    LOG_INFO("Lyrics v{} starting (mode: {})",
             1,  // Version placeholder
             args.mode == Mode::GUI ? "gui" :
             args.mode == Mode::TUI ? "tui" : "no-gui");

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create the app (shared across modes)
    auto app = std::make_unique<lyrics::App>(cfg);

    // ── GUI mode ──────────────────────────────────────────────────
#ifndef _WIN32
    if (args.mode == Mode::GUI) {
        // Start app in background thread
        std::thread app_thread([&app]() {
            app->run(g_stop_flag);
        });

        // Start system tray in background thread
        std::thread tray_thread([&app]() {
            std::vector<lyrics::tray::MenuItem> menu_items;
            // Note: font size actions need to interact with GTK;
            // we use quit as the primary menu action for now.
            menu_items.push_back({101, "退出", true, [&app]() {
                g_stop_flag.store(true);
            }});

            lyrics::tray::run(std::move(menu_items), icon_data());
        });
        tray_thread.detach();

        // Run GTK on main thread
        lyrics::run_gui("Lyrics Overlay", [&](auto callback) {
            app->set_lyric_listener(callback);
        });

        // GTK exited, signal app to stop
        g_stop_flag.store(true);
        if (app_thread.joinable()) app_thread.join();

        LOG_INFO("GUI mode exited");
    }
#else
    if (args.mode == Mode::GUI) {
        LOG_WARN("GUI mode not supported on Windows, falling back to console mode");
        // fall through to TUI or console mode
    }
#endif

    // ── TUI mode ──────────────────────────────────────────────────
    else if (args.mode == Mode::TUI) {
        // Start app in background
        std::thread app_thread([&app]() {
            app->run(g_stop_flag);
        });

        // Run TUI on main thread
        lyrics::run_console_tui([&](auto callback) {
            app->set_lyric_listener(callback);
        });

        // TUI exited
        g_stop_flag.store(true);
        if (app_thread.joinable()) app_thread.join();

        LOG_INFO("TUI mode exited");
    }

    // ── No-GUI mode ───────────────────────────────────────────────
    else {
        LOG_INFO("Running in headless mode (no GUI/TUI)");

        // Start system tray
        std::thread tray_thread([&app]() {
            std::vector<lyrics::tray::MenuItem> menu_items;
            menu_items.push_back({101, "退出", true, [&app]() {
                g_stop_flag.store(true);
            }});
            lyrics::tray::run(std::move(menu_items), icon_data());
        });
        tray_thread.detach();

        // Run app on main thread
        app->run(g_stop_flag);

        LOG_INFO("No-GUI mode exited");
    }

    return 0;
}
