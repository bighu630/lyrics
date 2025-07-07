#include <iostream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <memory>

// IPC headers
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>

// Terminal control
#include <sys/ioctl.h>
#include <termios.h>

// GTK4 headers
#include <gtk/gtk.h>
#include <gdk/gdk.h>

// --- Configuration ---
const char* SOCKET_PATH = "/tmp/lyrics_app.sock";
const int RECONNECT_DELAY_MS = 3000; // 3 seconds
const int MAX_RECONNECT_ATTEMPTS = 0; // 0 = infinite retries

// --- Output Interface ---
class LyricsOutput {
public:
    virtual ~LyricsOutput() = default;
    virtual void initialize() = 0;
    virtual void display_lyrics(const std::string &lyrics) = 0;
    virtual void display_status(const std::string &status) = 0;
    virtual void cleanup() = 0;
    virtual bool should_continue() = 0;
};

// --- Console Output Implementation ---
class ConsoleOutput : public LyricsOutput {
public:
    void initialize() override {
        std::cout << "Lyrics Client starting (Console Mode)..." << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    }

    void display_lyrics(const std::string &lyrics) override {
        // 限制歌词显示长度为50个字符
        std::string display_text = lyrics;
        if (display_text.length() > 50) {
            display_text = display_text.substr(0, 47) + "...";
        }
        
        std::cout << "\033[2J\033[H"; // Clear screen and move cursor to top
        std::cout << std::endl << std::endl;
        center_print("♪ " + display_text + " ♪");
        std::cout << std::endl;
    }

    void display_status(const std::string &status) override {
        std::cout << status << std::endl;
    }

    void cleanup() override {
        std::cout << std::endl << "Console shutting down..." << std::endl;
    }

    bool should_continue() override {
        return true; // Console mode runs until Ctrl+C
    }

private:
    int get_terminal_width() {
        winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            return w.ws_col;
        }
        return 80;
    }

    void center_print(const std::string &text) {
        int terminal_width = get_terminal_width();
        int text_length = text.length();
        int padding = (terminal_width - text_length) / 2;

        if (padding > 0) {
            std::cout << std::string(padding, ' ');
        }
        std::cout << text << std::endl;
    }
};

// --- GUI Output Implementation ---
class GuiOutput : public LyricsOutput {
private:
    GtkApplication *app = nullptr;
    GtkWidget *window = nullptr;
    GtkWidget *label = nullptr;
    bool running = true;
    std::string current_lyrics = "Waiting for lyrics...";

    static void on_activate(GtkApplication *app, gpointer user_data) {
        GuiOutput *gui = static_cast<GuiOutput*>(user_data);
        std::cout << "[GUI LOG] GTK application activated, creating window..." << std::endl;
        gui->create_window(app);
    }

    static gboolean on_delete_event(GtkWidget *widget, gpointer user_data) {
        GuiOutput *gui = static_cast<GuiOutput*>(user_data);
        gui->running = false;
        g_application_quit(G_APPLICATION(gui->app));
        return TRUE;
    }

    void create_window(GtkApplication *app) {
        std::cout << "[GUI LOG] Creating transparent lyrics window..." << std::endl;

        // Create window
        window = gtk_application_window_new(app);
        gtk_window_set_title(GTK_WINDOW(window), "Lyrics Overlay");
        gtk_window_set_default_size(GTK_WINDOW(window), 800, 120); // 调整窗口宽度适应50字符文本
        gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

        // Set window class name for window manager identification
        gtk_widget_set_name(window, "lyrics-gui");
        
        // Add CSS class for styling and window rules (GTK4 modern way)
        gtk_widget_add_css_class(window, "lyrics-window");

        // Create label for lyrics
        label = gtk_label_new(current_lyrics.c_str());
        gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
        
        // Add CSS class to label (GTK4 modern way)
        gtk_widget_add_css_class(label, "lyrics-text");

        // Set window and label styling using CSS
        GtkCssProvider *css_provider = gtk_css_provider_new();
        const char *css_data = 
            ".lyrics-window { "
            "  background: rgba(17, 17, 27, 0.1); "  /* Hyprland 深色背景 */
            "  border-radius: 12px; "
            "  padding: 16px 24px; "
            "} "
            ".lyrics-text { "
            "  color:rgb(80, 232, 204); "  /* 青绿色主色调 */
            "  font-size: 22px; "
            "  font-weight: 300; "
            "  padding: 8px 16px; "
            "  text-shadow: "
            "    0 0 12px rgba(80, 232, 204, 0.8), "     /* 主色发光 */
            "    0 0 24px rgba(80, 232, 204, 0.5), "     /* 中层发光 */
            "    0 0 36px rgba(80, 232, 204, 0.3), "     /* 外层发光 */
            "    2px 2px 4px rgba(0, 0, 0, 0.7); "       /* 黑色阴影增强对比 */
            "}";

        gtk_css_provider_load_from_string(css_provider, css_data);
        gtk_style_context_add_provider_for_display(
            gtk_widget_get_display(window),
            GTK_STYLE_PROVIDER(css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );

        // Set window child (GTK4 uses gtk_window_set_child instead of gtk_container_add)
        gtk_window_set_child(GTK_WINDOW(window), label);

        // Connect signals
        g_signal_connect(window, "close-request", G_CALLBACK(on_delete_event), this);

        // Show window
        gtk_window_present(GTK_WINDOW(window));

        std::cout << "[GUI LOG] Lyrics window created and displayed" << std::endl;
    }

public:
    void initialize() override {
        std::cout << "[GUI LOG] Initializing GUI mode..." << std::endl;
        
        // Set program class name for window manager
        g_set_prgname("lyrics-gui");
        g_set_application_name("Lyrics GUI");
        
        app = gtk_application_new("com.hyprland.lyrics-gui", G_APPLICATION_DEFAULT_FLAGS);
        g_signal_connect(app, "activate", G_CALLBACK(on_activate), this);
    }

    void display_lyrics(const std::string &lyrics) override {
        // 限制歌词显示长度为50个字符
        std::string display_text = lyrics;
        if (display_text.length() > 50) {
            display_text = display_text.substr(0, 47) + "...";
        }
        
        current_lyrics = display_text;
        std::cout << "[GUI LOG] Displaying lyrics: " << display_text << std::endl;
        if (label) {
            // Update label text
            gchar *markup = g_markup_printf_escaped("♪ %s ♪", display_text.c_str());
            gtk_label_set_markup(GTK_LABEL(label), markup);
            g_free(markup);
        }
    }

    void display_status(const std::string &status) override {
        // For GUI mode, show status in terminal as log
        std::cout << "[GUI LOG] " << status << std::endl;
    }

    void cleanup() override {
        if (app) {
            g_object_unref(app);
        }
    }

    bool should_continue() override {
        return running;
    }

    void process_events() {
        // Process GTK events using GMainContext
        GMainContext *context = g_main_context_default();
        while (g_main_context_pending(context)) {
            g_main_context_iteration(context, FALSE);
        }
    }

    GtkApplication* get_app() { return app; }
};

// --- Application State ---
struct ApplicationState {
    int ipc_socket_fd = -1;
    std::string lyrics_text = "Waiting for lyrics...";
    bool running = true;
    bool connected = false;
    int reconnect_attempts = 0;
    std::unique_ptr<LyricsOutput> output;
};

// --- Function Prototypes ---
bool try_connect_ipc(ApplicationState &state);
void cleanup_connection(ApplicationState &state);
void handle_ipc_message(ApplicationState &state);
void center_print(const std::string &text);
int get_terminal_width();

// --- Terminal Utilities ---
int get_terminal_width() {
    winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80; // Default fallback
}

void center_print(const std::string &text) {
    int terminal_width = get_terminal_width();
    int text_length = text.length();
    int padding = (terminal_width - text_length) / 2;

    if (padding > 0) {
        std::cout << std::string(padding, ' ');
    }
    std::cout << text << std::endl;
}

// --- IPC Logic ---
void cleanup_connection(ApplicationState &state) {
    if (state.ipc_socket_fd >= 0) {
        close(state.ipc_socket_fd);
        state.ipc_socket_fd = -1;
    }
    state.connected = false;
}

bool try_connect_ipc(ApplicationState &state) {
    // Clean up any existing connection
    cleanup_connection(state);

    state.ipc_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (state.ipc_socket_fd < 0) {
        state.output->display_status("Failed to create socket");
        return false;
    }

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(state.ipc_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(state.ipc_socket_fd);
        state.ipc_socket_fd = -1;
        return false;
    }

    state.connected = true;
    state.reconnect_attempts = 0;
    state.output->display_status("Connected to backend.");
    return true;
}

void handle_ipc_message(ApplicationState &state) {
    char buffer[4096];
    ssize_t bytes_read = read(state.ipc_socket_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';

        // Remove trailing newlines
        std::string new_lyrics(buffer);
        while (!new_lyrics.empty() && (new_lyrics.back() == '\n' || new_lyrics.back() == '\r')) {
            new_lyrics.pop_back();
        }

        if (state.lyrics_text != new_lyrics) {
            state.lyrics_text = new_lyrics;
            state.output->display_lyrics(state.lyrics_text);
        }
    } else if (bytes_read == 0) {
        // Connection closed
        state.output->display_status("Backend connection closed. Will attempt to reconnect...");
        cleanup_connection(state);
    } else {
        // Error
        state.output->display_status("Error reading from backend. Will attempt to reconnect...");
        cleanup_connection(state);
    }
}

// --- Main Application Logic ---
int main(int argc, char *argv[]) {
    ApplicationState state;

    // Set program name for window manager identification
    g_set_prgname("lyrics-gui");
    
    // Parse command line arguments
    bool use_gui = true; // 默认使用 GUI 模式
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--console" || std::string(argv[i]) == "-c") {
            use_gui = false;
            break;
        }
    }

    // Initialize GTK if using GUI mode
    if (use_gui) {
        gtk_init();
        state.output = std::make_unique<GuiOutput>();
    } else {
        state.output = std::make_unique<ConsoleOutput>();
    }

    state.output->initialize();

    // For GUI mode, we need to start the GTK application first
    if (use_gui) {
        std::cout << "[MAIN LOG] Starting GUI application..." << std::endl;
        GuiOutput* gui = static_cast<GuiOutput*>(state.output.get());
        GtkApplication *app = gui->get_app();

        // Register and activate the application
        GError *error = nullptr;
        if (!g_application_register(G_APPLICATION(app), nullptr, &error)) {
            std::cerr << "[GUI ERROR] Failed to register application: " << error->message << std::endl;
            g_error_free(error);
            return 1;
        }

        g_application_activate(G_APPLICATION(app));
        std::cout << "[MAIN LOG] GUI application started" << std::endl;
    }

    // Main connection loop with auto-reconnect
    while (state.running && state.output->should_continue()) {
        // Process GUI events if in GUI mode
        if (use_gui) {
            GuiOutput* gui = static_cast<GuiOutput*>(state.output.get());
            gui->process_events();
        }

        // Try to connect if not connected
        if (!state.connected) {
            if (MAX_RECONNECT_ATTEMPTS > 0 && state.reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) {
                state.output->display_status("Maximum reconnection attempts reached. Exiting.");
                break;
            }

            state.reconnect_attempts++;
            state.output->display_status("Attempting to connect to backend (attempt " +
                                       std::to_string(state.reconnect_attempts) + ")...");

            if (!try_connect_ipc(state)) {
                state.output->display_status("Failed to connect. Waiting " +
                                           std::to_string(RECONNECT_DELAY_MS/1000) +
                                           " seconds before retry...");
                std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
                continue;
            }

            state.output->display_status("Listening for lyrics... (Press Ctrl+C to exit)");
        }

        // Setup polling
        pollfd fds[1];
        fds[0].fd = state.ipc_socket_fd;
        fds[0].events = POLLIN;

        // Poll with timeout to check for reconnection needs and process GUI events
        int ret = poll(fds, 1, 100); // 100ms timeout for responsive GUI

        if (ret < 0) {
            state.output->display_status("Poll error");
            cleanup_connection(state);
            continue;
        } else if (ret == 0) {
            // Timeout - just continue to check connection status and process GUI events
            continue;
        }

        // Handle events
        if (fds[0].revents & POLLIN) {
            handle_ipc_message(state);
        }

        if (fds[0].revents & (POLLHUP | POLLERR)) {
            state.output->display_status("Connection lost. Will attempt to reconnect...");
            cleanup_connection(state);
        }
    }

    // --- Cleanup ---
    state.output->cleanup();
    cleanup_connection(state);

    return 0;
}
