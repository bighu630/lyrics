#pragma once
#include <atomic>
#include <thread>
#include <string>

namespace lyrics {

/// i3block integration controller.
/// Monitors i3block PID and sends Signal 55 to refresh the block.
/// Also writes lyrics to /dev/shm/lyrics for Waybar integration.
class I3BlockController {
public:
    I3BlockController();
    ~I3BlockController();
    
    /// Start monitoring (non-blocking, launches background thread).
    void start();
    
    /// Stop monitoring.
    void stop();
    
    /// Send signal 55 to i3block process to trigger a refresh.
    void send_signal55();
    
    /// Write lyrics text to shared memory (/dev/shm/lyrics).
    void write_to_shm(const std::string& text);
    
    /// Check if controller is running.
    bool running() const { return running_.load(); }
    
private:
    std::thread monitor_thread_;
    std::atomic<bool> running_{false};
    std::atomic<int> pid_{-1};
    
    /// Monitor loop: refresh PID every 10s.
    void monitor_loop();
    
    /// Refresh stored PID by pgrep.
    void refresh_pid();
    
    /// Alternative PID detection using ps aux.
    void refresh_pid_alternative();
};

} // namespace lyrics
