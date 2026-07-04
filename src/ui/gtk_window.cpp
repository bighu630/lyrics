#include "ui/gui.h"
#include "util/log.h"
#include <gtk/gtk.h>
#include <string>
#include <atomic>

namespace lyrics {

// ── Constants ──
constexpr int kDefaultFontSize = 18;
constexpr int kMinFontSize = 12;
constexpr int kMaxFontSize = 48;
constexpr int kFontStep = 2;
constexpr int kWindowWidth = 580;
constexpr int kWindowHeight = 40;
constexpr int kMaxLyricLen = 50;

// ── Global state (GTK is C, needs globals for callbacks) ──
static struct {
    GtkWidget* window = nullptr;
    GtkWidget* label = nullptr;
    int font_size = kDefaultFontSize;
    std::atomic<bool> started{false};
} g_gui;

// ── CSS for transparent glowing text ──
static void apply_css() {
    if (!g_gui.window) return;
    
    int h = kWindowHeight;
    int delta = g_gui.font_size - kDefaultFontSize;
    if (delta > 0) h += delta * 3 / 2;
    if (h < 30) h = 30;
    gtk_window_set_default_size(GTK_WINDOW(g_gui.window), kWindowWidth, h);
    
    std::string css = 
        "window, window.background, .lyrics-window {"
        "  background-color: transparent;"
        "}"
        ".lyrics-text {"
        "  background-color: transparent;"
        "  color: rgb(80, 232, 204);"
        "  font-size: " + std::to_string(g_gui.font_size) + "px;"
        "  font-weight: 300;"
        "  padding: 2px 8px;"
        "  text-shadow:"
        "    0 0 12px rgba(80, 232, 204, 0.5),"
        "    0 0 24px rgba(80, 232, 204, 0.3),"
        "    0 0 36px rgba(80, 232, 204, 0.2),"
        "    2px 2px 4px rgba(0, 0, 0, 0.4);"
        "}";
    
    auto* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css.c_str());
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

// ── Font size actions ──
extern "C" {
static void on_increase_font(GSimpleAction* action, GVariant* param, gpointer data) {
    (void)action; (void)param; (void)data;
    if (g_gui.font_size < kMaxFontSize) {
        g_gui.font_size += kFontStep;
        apply_css();
        LOG_INFO("Font size -> {}px", g_gui.font_size);
    }
}
static void on_decrease_font(GSimpleAction* action, GVariant* param, gpointer data) {
    (void)action; (void)param; (void)data;
    if (g_gui.font_size > kMinFontSize) {
        g_gui.font_size -= kFontStep;
        apply_css();
        LOG_INFO("Font size -> {}px", g_gui.font_size);
    }
}
static void on_reset_font(GSimpleAction* action, GVariant* param, gpointer data) {
    (void)action; (void)param; (void)data;
    g_gui.font_size = kDefaultFontSize;
    apply_css();
    LOG_INFO("Font size reset -> {}px", kDefaultFontSize);
}
static void on_quit(GSimpleAction* action, GVariant* param, gpointer data) {
    (void)action; (void)param; (void)data;
    auto* app = static_cast<GtkApplication*>(data);
    g_application_quit(G_APPLICATION(app));
}
} // extern "C"

// ── Set lyrics text (thread-safe via g_idle_add) ──
static gboolean idle_set_text(gpointer text_ptr) {
    auto* text = static_cast<std::string*>(text_ptr);
    if (g_gui.label) {
        std::string display;
        if (text->size() > (size_t)kMaxLyricLen) {
            display = text->substr(0, kMaxLyricLen - 3) + "...";
        } else {
            display = *text;
        }
        // Escape for Pango markup
        char* escaped = g_markup_escape_text(display.c_str(), -1);
        gtk_label_set_markup(GTK_LABEL(g_gui.label), escaped);
        g_free(escaped);
    }
    delete text;
    return G_SOURCE_REMOVE;
}

static void set_lyrics(const std::string& text) {
    auto* copy = new std::string(text);
    g_idle_add(idle_set_text, copy);
}

// ── Create window callback ──
static void activate_cb(GtkApplication* app, gpointer user_data) {
    (void)user_data;
    
    // Create window
    g_gui.window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(g_gui.window), "Lyrics Overlay");
    gtk_window_set_decorated(GTK_WINDOW(g_gui.window), false);
    gtk_window_set_default_size(GTK_WINDOW(g_gui.window), kWindowWidth, kWindowHeight);
    gtk_widget_set_name(g_gui.window, "lyrics-window");
    gtk_widget_add_css_class(g_gui.window, "lyrics-window");
    
    // Label
    g_gui.label = gtk_label_new("Waiting for lyrics...");
    gtk_label_set_xalign(GTK_LABEL(g_gui.label), 0.5f);
    gtk_label_set_yalign(GTK_LABEL(g_gui.label), 0.5f);
    gtk_widget_add_css_class(g_gui.label, "lyrics-text");
    gtk_window_set_child(GTK_WINDOW(g_gui.window), g_gui.label);
    
    // Apply CSS
    apply_css();
    
    // Right-click menu (GAction-based popover)
    auto* menu_model = g_menu_new();
    g_menu_append(menu_model, "增大字体", "win.increase");
    g_menu_append(menu_model, "减小字体", "win.decrease");
    g_menu_append(menu_model, "重置字体", "win.reset");
    g_menu_append(menu_model, "退出", "win.quit");
    
    // Actions
    auto* win_actions = g_simple_action_group_new();
    
    auto* inc_action = g_simple_action_new("increase", nullptr);
    g_signal_connect(inc_action, "activate", G_CALLBACK(on_increase_font), nullptr);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(inc_action));
    
    auto* dec_action = g_simple_action_new("decrease", nullptr);
    g_signal_connect(dec_action, "activate", G_CALLBACK(on_decrease_font), nullptr);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(dec_action));
    
    auto* reset_action = g_simple_action_new("reset", nullptr);
    g_signal_connect(reset_action, "activate", G_CALLBACK(on_reset_font), nullptr);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(reset_action));
    
    auto* quit_action = g_simple_action_new("quit", nullptr);
    g_signal_connect(quit_action, "activate", G_CALLBACK(on_quit), app);
    g_action_map_add_action(G_ACTION_MAP(win_actions), G_ACTION(quit_action));
    
    gtk_widget_insert_action_group(g_gui.window, "win", G_ACTION_GROUP(win_actions));

    // Right-click gesture to show popup menu
    auto* gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 3);
    g_signal_connect(gesture, "pressed", G_CALLBACK(+[](GtkGestureClick*, int, double x, double y, gpointer data) {
        auto* menu_model = static_cast<GMenuModel*>(data);
        auto* popover = gtk_popover_menu_new_from_model(menu_model);
        gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
        gtk_widget_set_parent(GTK_WIDGET(popover), GTK_WIDGET(g_gui.window));
        gtk_popover_popup(GTK_POPOVER(popover));
    }), menu_model);
    gtk_widget_add_controller(g_gui.window, GTK_EVENT_CONTROLLER(gesture));

    gtk_widget_set_visible(g_gui.window, true);
    
    g_gui.started.store(true);
    LOG_INFO("GTK window created");
}

// ── Run GUI ──
void run_gui(const std::string& title,
             std::function<void(std::function<void(const std::string&)>)> start_lyrics_listener) {
    // Register the lyrics listener BEFORE starting the app
    start_lyrics_listener(set_lyrics);
    
    auto* app = gtk_application_new("com.hyprland.lyrics", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate_cb), nullptr);
    
    // Add the menu as app menu too for completeness
    auto* menu_model = g_menu_new();
    g_menu_append(menu_model, "增大字体", "app.increase");
    g_menu_append(menu_model, "减小字体", "app.decrease");
    g_menu_append(menu_model, "重置字体", "app.reset");
    g_menu_append(menu_model, "退出", "app.quit");
    
    auto* app_actions = g_simple_action_group_new();
    
    auto* inc_action = g_simple_action_new("increase", nullptr);
    g_signal_connect(inc_action, "activate", G_CALLBACK(on_increase_font), nullptr);
    g_action_map_add_action(G_ACTION_MAP(app_actions), G_ACTION(inc_action));
    
    auto* dec_action = g_simple_action_new("decrease", nullptr);
    g_signal_connect(dec_action, "activate", G_CALLBACK(on_decrease_font), nullptr);
    g_action_map_add_action(G_ACTION_MAP(app_actions), G_ACTION(dec_action));
    
    auto* reset_action = g_simple_action_new("reset", nullptr);
    g_signal_connect(reset_action, "activate", G_CALLBACK(on_reset_font), nullptr);
    g_action_map_add_action(G_ACTION_MAP(app_actions), G_ACTION(reset_action));
    
    auto* quit_action = g_simple_action_new("quit", nullptr);
    g_signal_connect(quit_action, "activate", G_CALLBACK(on_quit), app);
    g_action_map_add_action(G_ACTION_MAP(app_actions), G_ACTION(quit_action));
    
    static const char* quit_accels[] = {"<Control>Q", nullptr};
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit", quit_accels);
    
    int status = g_application_run(G_APPLICATION(app), 0, nullptr);
    g_object_unref(app);
    
    LOG_INFO("GTK application exited with status {}", status);
}

} // namespace lyrics
