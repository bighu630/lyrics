#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>

#include "yyjson/yyjson.h"

namespace lyrics {

/// RAII wrapper around yyjson documents and values.
/// Provides convenient helpers for constructing and parsing JSON.

class JsonDoc {
public:
    /// Create an empty document (mut).
    JsonDoc();

    /// Create a read-only document from a JSON string.
    explicit JsonDoc(const std::string& json);

    /// Create a read-only document from raw data.
    JsonDoc(const char* data, size_t len);

    ~JsonDoc();

    JsonDoc(JsonDoc&&) noexcept;
    JsonDoc& operator=(JsonDoc&&) noexcept;

    // Non-copyable
    JsonDoc(const JsonDoc&) = delete;
    JsonDoc& operator=(const JsonDoc&) = delete;

    /// Get the mutable root value (for building).
    yyjson_mut_val* root_mut() { return root_mut_; }

    /// Get the immutable root value (for reading).
    yyjson_val* root() const { return root_; }

    /// Get the mutable doc pointer.
    yyjson_mut_doc* doc_mut() { return doc_mut_; }

    /// Serialize to string (minified).
    std::string to_string() const;

    /// Pretty-print.
    std::string to_pretty() const;

    /// Check if the document is valid.
    bool valid() const { return doc_mut_ || doc_; }

    // ── Static builders ──────────────────────────────────────────

    /// Create a mutable JSON object.
    static JsonDoc make_object();

    /// Create a mutable JSON array.
    static JsonDoc make_array();

    // ── Mutation helpers (on mutable doc) ────────────────────────

    /// Set a string field on an object.
    static void set_str(yyjson_mut_val* obj, yyjson_mut_doc* doc,
                        const char* key, const std::string& val);

    /// Set a double field on an object.
    static void set_real(yyjson_mut_val* obj, yyjson_mut_doc* doc,
                         const char* key, double val);

    /// Set an int64 field on an object.
    static void set_int(yyjson_mut_val* obj, yyjson_mut_doc* doc,
                        const char* key, int64_t val);

    /// Set a bool field on an object.
    static void set_bool(yyjson_mut_val* obj, yyjson_mut_doc* doc,
                         const char* key, bool val);

    /// Append a value to an array.
    static void arr_append(yyjson_mut_val* arr, yyjson_mut_doc* doc,
                           yyjson_mut_val* val);

    /// Create a string value.
    static yyjson_mut_val* new_str(yyjson_mut_doc* doc, const std::string& s);

    /// Create an int value.
    static yyjson_mut_val* new_int(yyjson_mut_doc* doc, int64_t v);

    // ── Reading helpers (on immutable val) ───────────────────────

    /// Get string field (returns empty string if missing).
    static std::string get_str(yyjson_val* obj, const char* key);

    /// Get optional string field.
    static std::optional<std::string> get_str_opt(yyjson_val* obj, const char* key);

    /// Get double field.
    static double get_real(yyjson_val* obj, const char* key, double def = 0.0);

    /// Get int64 field.
    static int64_t get_int(yyjson_val* obj, const char* key, int64_t def = 0);

    /// Get bool field.
    static bool get_bool(yyjson_val* obj, const char* key, bool def = false);

    /// Get a nested object by path (e.g., "candidates[0].content.parts").
    static yyjson_val* get_path(yyjson_val* root, const std::string& path);

private:
    yyjson_mut_doc* doc_mut_{nullptr};
    yyjson_doc* doc_{nullptr};
    yyjson_val* root_{nullptr};
    yyjson_mut_val* root_mut_{nullptr};
};

} // namespace lyrics
