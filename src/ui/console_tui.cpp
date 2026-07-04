#include "ui/gui.h"  // for run_tui declaration
#include "ui/console_tui.h"
#include "util/log.h"

#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <csignal>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace lyrics {

static std::atomic<bool> g_tui_running{true};

extern "C" {
static void tui_signal_handler(int) {
    g_tui_running.store(false);
}
}

static int get_terminal_width() {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return 80;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        return csbi.dwSize.X;
    }
    return 80;
#else
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return 80;
#endif
}

void run_console_tui(std::function<void(std::function<void(const std::string&)>)> start_lyrics_listener) {
    // Setup signal handler
    signal(SIGINT, tui_signal_handler);
    signal(SIGTERM, tui_signal_handler);
    
    std::string current_lyric;
    std::mutex lyric_mutex;
    
    auto callback = [&](const std::string& text) {
        std::lock_guard<std::mutex> lock(lyric_mutex);
        current_lyric = text;
        // Truncate to 50 chars
        if (current_lyric.size() > 50) {
            current_lyric = current_lyric.substr(0, 47) + "...";
        }
    };
    
    // Register the callback
    start_lyrics_listener(callback);
    
    std::cout << "Lyrics Client starting (Console Mode)..." << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    
    while (g_tui_running.load()) {
        std::string display;
        {
            std::lock_guard<std::mutex> lock(lyric_mutex);
            display = current_lyric;
        }
        
        // Clear screen and move to top
        std::cout << "\033[2J\033[H\n\n";
        
        // Center text
        int width = get_terminal_width();
        int padding = (width - (int)display.size()) / 2;
        if (padding > 0) {
            std::cout << std::string(padding, ' ');
        }
        std::cout << display << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    // Cleanup
    std::cout << "\033[2J\033[H";
    std::cout << "\nBye!" << std::endl;
}

} // namespace lyrics
