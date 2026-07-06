#include "i3block/controller.h"
#include "util/exec.h"
#include "util/log.h"

#include <fstream>
#include <sstream>
#include <signal.h>
#include <string>
#include <chrono>
#include <thread>

namespace lyrics {

// ── Constructor / Destructor ──
I3BlockController::I3BlockController() = default;

I3BlockController::~I3BlockController() {
    stop();
}

// ── Start / Stop ──
void I3BlockController::start() {
    if (running_.exchange(true)) return;

    // Initial PID refresh
    refresh_pid();

    monitor_thread_ = std::thread(&I3BlockController::monitor_loop, this);
    LOG_INFO("i3block controller started");
}

void I3BlockController::stop() {
    if (!running_.exchange(false)) return;
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    LOG_INFO("i3block controller stopped");
}

// ── PID Refresh ──
void I3BlockController::refresh_pid() {
    std::string out = util::exec("pgrep -f i3blocks 2>/dev/null");
    if (out.empty()) {
        refresh_pid_alternative();
        return;
    }

    // Take first line
    auto nl = out.find('\n');
    if (nl != std::string::npos) out = out.substr(0, nl);

    try {
        int new_pid = std::stoi(out);
        if (new_pid > 0) {
            int old = pid_.exchange(new_pid);
            if (old != new_pid) {
                LOG_INFO("i3block PID updated: {} -> {}", old, new_pid);
            }
            return;
        }
    } catch (...) {}

    pid_.store(-1);
}

void I3BlockController::refresh_pid_alternative() {
    std::string out = util::exec("ps aux 2>/dev/null");
    std::istringstream stream(out);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.find("i3blocks") != std::string::npos &&
            line.find("grep") == std::string::npos) {
            std::istringstream ls(line);
            std::string field;
            for (int i = 0; i < 2 && ls >> field; ++i);
            try {
                int new_pid = std::stoi(field);
                if (new_pid > 0) {
                    int old = pid_.exchange(new_pid);
                    if (old != new_pid) {
                        LOG_INFO("i3block PID updated (alt): {} -> {}", old, new_pid);
                    }
                    return;
                }
            } catch (...) {}
        }
    }

    pid_.store(-1);
}

// ── Monitor Loop ──
void I3BlockController::monitor_loop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (!running_.load()) break;
        refresh_pid();
    }
}

// ── Send Signal 55 (SIGRTMIN+21) ──
void I3BlockController::send_signal55() {
    int pid = pid_.load();
    if (pid <= 0) {
        LOG_DEBUG("Cannot send signal 55: no i3block PID");
        return;
    }

    if (kill(pid, SIGRTMIN + 21) != 0) {
        LOG_WARN("Failed to send signal 55 to PID {}: {}", pid, strerror(errno));
        if (kill(pid, SIGUSR1) != 0) {
            LOG_DEBUG("SIGUSR1 also failed: {}", strerror(errno));
        }
    } else {
        LOG_DEBUG("Sent signal 55 to i3block (PID {})", pid);
    }
}

// ── Write to Shared Memory ──
void I3BlockController::write_to_shm(const std::string& text) {
    std::ofstream ofs("/dev/shm/lyrics");
    if (!ofs) {
        LOG_DEBUG("Failed to write /dev/shm/lyrics: {}", strerror(errno));
        return;
    }
    ofs << text;
    if (text.empty() || text.back() != '\n') {
        ofs << '\n';
    }
}

} // namespace lyrics
