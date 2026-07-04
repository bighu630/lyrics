#include "tray/tray.h"
#include "util/log.h"

#include <dbus/dbus.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <vector>
#include <map>

namespace lyrics {
namespace tray {

// ── Forward declarations ──
namespace detail {

class TrayImpl {
public:
    TrayImpl(std::vector<MenuItem> items, std::vector<uint8_t> icon_data);
    ~TrayImpl();
    bool run();

private:
    std::vector<MenuItem> items_;
    std::vector<uint8_t> icon_data_;
    DBusConnection* conn_ = nullptr;
    std::string service_name_;
    bool initialized_ = false;

    bool connect_bus();
    void register_sni();
    void register_menu();
    void handle_messages();
    void reply_to_method_call(DBusMessage* msg, DBusMessage* reply);

    // D-Bus method handlers
    DBusHandlerResult handle_sni_method(DBusConnection* c, DBusMessage* msg);
    DBusHandlerResult handle_menu_method(DBusConnection* c, DBusMessage* msg);

    static DBusHandlerResult sni_handler(DBusConnection* c, DBusMessage* msg, void* user_data);
    static DBusHandlerResult menu_handler(DBusConnection* c, DBusMessage* msg, void* user_data);
};

static TrayImpl* g_tray = nullptr;

// ── Implementation ──

TrayImpl::TrayImpl(std::vector<MenuItem> items, std::vector<uint8_t> icon_data)
    : items_(std::move(items))
    , icon_data_(std::move(icon_data)) {

    // Assign IDs if not set
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].id == 0) items_[i].id = 100 + (int32_t)i;
    }
}

TrayImpl::~TrayImpl() {
    if (conn_) {
        dbus_connection_unref(conn_);
    }
    g_tray = nullptr;
}

bool TrayImpl::connect_bus() {
    DBusError err;
    dbus_error_init(&err);

    conn_ = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn_) {
        LOG_ERROR("D-Bus connection failed: {}", err.message ? err.message : "unknown");
        dbus_error_free(&err);
        return false;
    }

    // Request name
    service_name_ = "org.kde.StatusNotifierItem-" + std::to_string(getpid()) + "-1";
    int ret = dbus_bus_request_name(conn_, service_name_.c_str(),
                                     DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        LOG_WARN("D-Bus name request returned {}", ret);
    }

    initialized_ = true;
    LOG_INFO("D-Bus connected, service: {}", service_name_);
    return true;
}

void TrayImpl::register_sni() {
    // Register object path handler
    DBusObjectPathVTable vtable;
    memset(&vtable, 0, sizeof(vtable));
    vtable.message_function = sni_handler;
    dbus_connection_register_object_path(conn_, "/StatusNotifierItem", &vtable, this);

    LOG_INFO("StatusNotifierItem registered");
}

void TrayImpl::register_menu() {
    DBusObjectPathVTable vtable;
    memset(&vtable, 0, sizeof(vtable));
    vtable.message_function = menu_handler;
    dbus_connection_register_object_path(conn_, "/com/canonical/dbusmenu", &vtable, this);
    LOG_INFO("DBusMenu registered");
}

DBusHandlerResult TrayImpl::handle_sni_method(DBusConnection* c, DBusMessage* msg) {
    (void)c;
    const char* method = dbus_message_get_member(msg);

    if (strcmp(method, "Identify") == 0) {
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            const char* id = "lyrics";
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &id, DBUS_TYPE_INVALID);
            dbus_connection_send(conn_, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (strcmp(method, "Get") == 0) {
        // Handle org.freedesktop.DBus.Properties.Get for StatusNotifierItem
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            DBusMessageIter iter;
            dbus_message_iter_init_append(reply, &iter);
            // Return a valid variant containing an empty string
            DBusMessageIter variant;
            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant);
            const char* empty = "";
            dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &empty);
            dbus_message_iter_close_container(&iter, &variant);
            dbus_connection_send(conn_, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (strcmp(method, "GetAll") == 0) {
        // Handle org.freedesktop.DBus.Properties.GetAll
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            DBusMessageIter iter;
            dbus_message_iter_init_append(reply, &iter);
            // Return empty dict<string,variant>
            DBusMessageIter dict;
            dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
            dbus_message_iter_close_container(&iter, &dict);
            dbus_connection_send(conn_, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult TrayImpl::handle_menu_method(DBusConnection* c, DBusMessage* msg) {
    (void)c;
    const char* iface = dbus_message_get_interface(msg);
    const char* method = dbus_message_get_member(msg);

    if (!iface || strcmp(iface, "com.canonical.dbusmenu") != 0) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (strcmp(method, "GetLayout") == 0) {
        // Build menu layout
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (!reply) return DBUS_HANDLER_RESULT_HANDLED;

        DBusMessageIter iter;
        dbus_message_iter_init_append(reply, &iter);

        // Revision (uint32)
        uint32_t revision = 0;
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &revision);

        // Layout structure
        // Build as: [parentID, props, children]
        // Where props is dict and children is array of same structure
        DBusMessageIter layout_arr, layout_struct;
        dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "a(ia{sv}av)", &layout_arr);
        dbus_message_iter_open_container(&layout_arr, DBUS_TYPE_ARRAY, "(ia{sv}av)", &layout_struct);

        // Root item (id=0)
        for (const auto& item : items_) {
            DBusMessageIter item_struct;
            dbus_message_iter_open_container(&layout_struct, DBUS_TYPE_STRUCT, nullptr, &item_struct);

            // Item ID
            int32_t id = item.id;
            dbus_message_iter_append_basic(&item_struct, DBUS_TYPE_INT32, &id);

            // Properties dict
            DBusMessageIter props_dict, props_entry;
            dbus_message_iter_open_container(&item_struct, DBUS_TYPE_ARRAY, "{sv}", &props_dict);

            // Label property
            {
                dbus_message_iter_open_container(&props_dict, DBUS_TYPE_DICT_ENTRY, nullptr, &props_entry);
                const char* key = "label";
                dbus_message_iter_append_basic(&props_entry, DBUS_TYPE_STRING, &key);

                DBusMessageIter variant;
                dbus_message_iter_open_container(&props_entry, DBUS_TYPE_VARIANT, "s", &variant);
                const char* label = item.label.c_str();
                dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &label);
                dbus_message_iter_close_container(&props_entry, &variant);
                dbus_message_iter_close_container(&props_dict, &props_entry);
            }

            // Enabled property
            {
                dbus_message_iter_open_container(&props_dict, DBUS_TYPE_DICT_ENTRY, nullptr, &props_entry);
                const char* key = "enabled";
                dbus_message_iter_append_basic(&props_entry, DBUS_TYPE_STRING, &key);

                DBusMessageIter variant;
                dbus_message_iter_open_container(&props_entry, DBUS_TYPE_VARIANT, "b", &variant);
                int enabled = item.enabled ? 1 : 0;
                dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &enabled);
                dbus_message_iter_close_container(&props_entry, &variant);
                dbus_message_iter_close_container(&props_dict, &props_entry);
            }

            // Type property
            {
                dbus_message_iter_open_container(&props_dict, DBUS_TYPE_DICT_ENTRY, nullptr, &props_entry);
                const char* key = "type";
                dbus_message_iter_append_basic(&props_entry, DBUS_TYPE_STRING, &key);

                DBusMessageIter variant;
                dbus_message_iter_open_container(&props_entry, DBUS_TYPE_VARIANT, "s", &variant);
                const char* type = "standard";
                dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &type);
                dbus_message_iter_close_container(&props_entry, &variant);
                dbus_message_iter_close_container(&props_dict, &props_entry);
            }

            dbus_message_iter_close_container(&item_struct, &props_dict);

            // Empty children array
            DBusMessageIter children_arr;
            dbus_message_iter_open_container(&item_struct, DBUS_TYPE_ARRAY, "v", &children_arr);
            dbus_message_iter_close_container(&item_struct, &children_arr);

            dbus_message_iter_close_container(&layout_struct, &item_struct);
        }

        dbus_message_iter_close_container(&layout_arr, &layout_struct);
        dbus_message_iter_close_container(&iter, &layout_arr);

        dbus_connection_send(conn_, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (strcmp(method, "Event") == 0) {
        DBusMessageIter iter;
        dbus_message_iter_init(msg, &iter);

        int32_t id = 0;
        const char* event_id = nullptr;

        dbus_message_iter_get_basic(&iter, &id);
        dbus_message_iter_next(&iter);

        if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
            dbus_message_iter_get_basic(&iter, &event_id);
        }

        if (event_id && strcmp(event_id, "clicked") == 0) {
            for (auto& item : items_) {
                if (item.id == id && item.clicked) {
                    LOG_DEBUG("Menu clicked: {}", item.label);
                    item.clicked();
                    break;
                }
            }
        }

        // Send empty reply
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_connection_send(conn_, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (strcmp(method, "AboutToShow") == 0 || strcmp(method, "AboutToShowGroup") == 0) {
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            int false_val = 0;
            dbus_message_append_args(reply,
                DBUS_TYPE_BOOLEAN, &false_val,
                DBUS_TYPE_INVALID);
            dbus_connection_send(conn_, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (strcmp(method, "GetGroupProperties") == 0) {
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            DBusMessageIter iter;
            dbus_message_iter_init_append(reply, &iter);

            DBusMessageIter arr;
            dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(ia{sv})", &arr);
            dbus_message_iter_close_container(&iter, &arr);

            dbus_connection_send(conn_, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (strcmp(method, "Introspect") == 0) {
        const char* xml =
            "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
            "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
            "<node name=\"/com/canonical/dbusmenu\">\n"
            "  <interface name=\"com.canonical.dbusmenu\">\n"
            "    <method name=\"GetLayout\">\n"
            "      <arg direction=\"in\" name=\"parentId\" type=\"i\"/>\n"
            "      <arg direction=\"in\" name=\"recursionDepth\" type=\"i\"/>\n"
            "      <arg direction=\"in\" name=\"propertyNames\" type=\"as\"/>\n"
            "      <arg direction=\"out\" name=\"revision\" type=\"u\"/>\n"
            "      <arg direction=\"out\" name=\"layout\" type=\"a(ia{sv}av)\"/>\n"
            "    </method>\n"
            "    <method name=\"GetGroupProperties\">\n"
            "      <arg direction=\"in\" name=\"ids\" type=\"ai\"/>\n"
            "      <arg direction=\"in\" name=\"propertyNames\" type=\"as\"/>\n"
            "      <arg direction=\"out\" type=\"a(ia{sv})\"/>\n"
            "    </method>\n"
            "    <method name=\"Event\">\n"
            "      <arg direction=\"in\" name=\"id\" type=\"i\"/>\n"
            "      <arg direction=\"in\" name=\"eventId\" type=\"s\"/>\n"
            "      <arg direction=\"in\" name=\"data\" type=\"v\"/>\n"
            "      <arg direction=\"in\" name=\"timestamp\" type=\"u\"/>\n"
            "    </method>\n"
            "    <method name=\"AboutToShow\">\n"
            "      <arg direction=\"in\" name=\"id\" type=\"i\"/>\n"
            "      <arg direction=\"out\" type=\"b\"/>\n"
            "    </method>\n"
            "    <method name=\"AboutToShowGroup\">\n"
            "      <arg direction=\"in\" name=\"ids\" type=\"ai\"/>\n"
            "      <arg direction=\"in\" name=\"exceptionIds\" type=\"ai\"/>\n"
            "      <arg direction=\"out\" type=\"ab\"/>\n"
            "    </method>\n"
            "  </interface>\n"
            "</node>";

        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
            dbus_connection_send(conn_, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult TrayImpl::sni_handler(DBusConnection* c, DBusMessage* msg, void* user_data) {
    auto* self = static_cast<TrayImpl*>(user_data);
    return self->handle_sni_method(c, msg);
}

DBusHandlerResult TrayImpl::menu_handler(DBusConnection* c, DBusMessage* msg, void* user_data) {
    auto* self = static_cast<TrayImpl*>(user_data);
    return self->handle_menu_method(c, msg);
}

void TrayImpl::handle_messages() {
    while (initialized_ && dbus_connection_read_write_dispatch(conn_, 100)) {
        // D-Bus dispatches callbacks automatically
    }
}

bool TrayImpl::run() {
    if (!connect_bus()) return false;

    register_sni();
    register_menu();

    // Main loop
    LOG_INFO("Tray running, handling D-Bus messages...");

    while (initialized_) {
        handle_messages();
    }

    return true;
}

} // namespace detail

// ── Public API ──

bool run(std::vector<MenuItem> items, const std::vector<uint8_t>& icon_data) {
    detail::TrayImpl tray(std::move(items), icon_data);
    detail::g_tray = &tray;
    return tray.run();
}

} // namespace tray
} // namespace lyrics
