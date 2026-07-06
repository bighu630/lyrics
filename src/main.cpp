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

#ifdef HAS_GUI
#include "ui/gui.h"
#endif
#include "ui/console_tui.h"

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

// ── Mode parsing ──────────────────────────────────────────────────

enum class Mode { Unset, GUI, TUI, NoGUI };

struct ParsedArgs {
    Mode mode = Mode::Unset;
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

    // Resolve mode: CLI args take priority, otherwise use config.gui
    Mode mode = args.mode;
    if (mode == Mode::Unset) {
        mode = cfg.gui ? Mode::GUI : Mode::NoGUI;
    }

    // Initialize logger
    lyrics::init_logger(cfg.log_level);
    LOG_INFO("Lyrics v{} starting (mode: {})",
             1,  // Version placeholder
             mode == Mode::GUI ? "gui" :
             mode == Mode::TUI ? "tui" : "no-gui");

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create the app (shared across modes)
    auto app = std::make_unique<lyrics::App>(cfg);

    // ── GUI mode ──────────────────────────────────────────────────
#ifdef HAS_GUI
    if (mode == Mode::GUI) {
        // Start app in background thread
        std::thread app_thread([&app]() {
            app->run(g_stop_flag);
        });

        // Run GTK on main thread
        lyrics::run_gui("Lyrics Overlay", [&](auto callback) {
            app->set_lyric_listener(callback);
        }, g_stop_flag);

        // GTK exited, signal app to stop
        g_stop_flag.store(true);
        if (app_thread.joinable()) app_thread.join();

        LOG_INFO("GUI mode exited");
    }
#else
    if (mode == Mode::GUI) {
        LOG_WARN("GUI mode not available (no GTK4), falling back to TUI mode");
        mode = Mode::TUI; // fall through to TUI
    }
#endif

    // ── TUI mode ──────────────────────────────────────────────────
    else if (mode == Mode::TUI) {
        // Start app in background
        std::thread app_thread([&app]() {
            app->run(g_stop_flag);
        });

        // Run TUI on main thread
        lyrics::run_console_tui([&](auto callback) {
            app->set_lyric_listener(callback);
        }, g_stop_flag);

        // TUI exited
        g_stop_flag.store(true);
        if (app_thread.joinable()) app_thread.join();

        LOG_INFO("TUI mode exited");
    }

    // ── No-GUI mode ───────────────────────────────────────────────
    else {
        LOG_INFO("Running in headless mode (no GUI/TUI)");

        // Run app on main thread
        app->run(g_stop_flag);

        LOG_INFO("No-GUI mode exited");
    }

    return 0;
}
