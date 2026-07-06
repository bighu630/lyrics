#include "util/json.h"
#include <cstring>

namespace lyrics {

// ── Constructor / Destructor ─────────────────────────────────────

JsonDoc::JsonDoc()
    : doc_mut_(yyjson_mut_doc_new(nullptr)), root_mut_(yyjson_mut_obj(doc_mut_)) {
    yyjson_mut_doc_set_root(doc_mut_, root_mut_);
}

JsonDoc::JsonDoc(const std::string& json)
    : doc_(yyjson_read(json.c_str(), json.size(), 0)) {
    if (doc_) {
        root_ = yyjson_doc_get_root(doc_);
    }
}

JsonDoc::JsonDoc(const char* data, size_t len)
    : doc_(yyjson_read(data, len, 0)) {
    if (doc_) {
        root_ = yyjson_doc_get_root(doc_);
    }
}

JsonDoc::~JsonDoc() {
    if (doc_mut_) yyjson_mut_doc_free(doc_mut_);
    if (doc_) yyjson_doc_free(doc_);
}

JsonDoc::JsonDoc(JsonDoc&& other) noexcept
    : doc_mut_(other.doc_mut_), doc_(other.doc_),
      root_(other.root_), root_mut_(other.root_mut_) {
    other.doc_mut_ = nullptr;
    other.doc_ = nullptr;
    other.root_ = nullptr;
    other.root_mut_ = nullptr;
}

JsonDoc& JsonDoc::operator=(JsonDoc&& other) noexcept {
    if (this != &other) {
        if (doc_mut_) yyjson_mut_doc_free(doc_mut_);
        if (doc_) yyjson_doc_free(doc_);
        doc_mut_ = other.doc_mut_;
        doc_ = other.doc_;
        root_ = other.root_;
        root_mut_ = other.root_mut_;
        other.doc_mut_ = nullptr;
        other.doc_ = nullptr;
        other.root_ = nullptr;
        other.root_mut_ = nullptr;
    }
    return *this;
}

std::string JsonDoc::to_string() const {
    if (doc_mut_) {
        auto* data = yyjson_mut_write(doc_mut_, 0, nullptr);
        if (!data) return {};
        std::string s(data);
        free(data);
        return s;
    }
    return {};
}

std::string JsonDoc::to_pretty() const {
    if (doc_mut_) {
        auto* data = yyjson_mut_write(doc_mut_, YYJSON_WRITE_PRETTY, nullptr);
        if (!data) return {};
        std::string s(data);
        free(data);
        return s;
    }
    return {};
}

// ── Static builders ──────────────────────────────────────────────

JsonDoc JsonDoc::make_object() {
    return JsonDoc(); // default constructor creates an object
}

JsonDoc JsonDoc::make_array() {
    JsonDoc doc;
    // Replace root with array
    yyjson_mut_doc_set_root(doc.doc_mut_, yyjson_mut_arr(doc.doc_mut_));
    doc.root_mut_ = yyjson_mut_doc_get_root(doc.doc_mut_);
    return doc;
}

// ── Mutation helpers ─────────────────────────────────────────────

void JsonDoc::set_str(yyjson_mut_val* obj, yyjson_mut_doc* doc,
                       const char* key, const std::string& val) {
    yyjson_mut_obj_add_str(doc, obj, key, val.c_str());
}

void JsonDoc::set_real(yyjson_mut_val* obj, yyjson_mut_doc* doc,
                        const char* key, double val) {
    yyjson_mut_obj_add_real(doc, obj, key, val);
}

void JsonDoc::set_int(yyjson_mut_val* obj, yyjson_mut_doc* doc,
                       const char* key, int64_t val) {
    yyjson_mut_obj_add_int(doc, obj, key, val);
}

void JsonDoc::set_bool(yyjson_mut_val* obj, yyjson_mut_doc* doc,
                        const char* key, bool val) {
    yyjson_mut_obj_add_bool(doc, obj, key, val);
}

void JsonDoc::arr_append(yyjson_mut_val* arr, yyjson_mut_doc* doc,
                          yyjson_mut_val* val) {
    (void)doc;
    yyjson_mut_arr_append(arr, val);
}

yyjson_mut_val* JsonDoc::new_str(yyjson_mut_doc* doc, const std::string& s) {
    return yyjson_mut_strcpy(doc, s.c_str());
}

yyjson_mut_val* JsonDoc::new_int(yyjson_mut_doc* doc, int64_t v) {
    return yyjson_mut_int(doc, v);
}

// ── Reading helpers ──────────────────────────────────────────────

std::string JsonDoc::get_str(yyjson_val* obj, const char* key) {
    auto* v = yyjson_obj_get(obj, key);
    if (v && yyjson_is_str(v)) {
        return yyjson_get_str(v);
    }
    return {};
}

std::optional<std::string> JsonDoc::get_str_opt(yyjson_val* obj, const char* key) {
    auto* v = yyjson_obj_get(obj, key);
    if (v && yyjson_is_str(v)) {
        return std::string(yyjson_get_str(v));
    }
    return std::nullopt;
}

double JsonDoc::get_real(yyjson_val* obj, const char* key, double def) {
    auto* v = yyjson_obj_get(obj, key);
    if (v && yyjson_is_real(v)) return yyjson_get_real(v);
    return def;
}

int64_t JsonDoc::get_int(yyjson_val* obj, const char* key, int64_t def) {
    auto* v = yyjson_obj_get(obj, key);
    if (v && yyjson_is_int(v)) return yyjson_get_int(v);
    return def;
}

bool JsonDoc::get_bool(yyjson_val* obj, const char* key, bool def) {
    auto* v = yyjson_obj_get(obj, key);
    if (v && yyjson_is_bool(v)) return yyjson_get_bool(v);
    return def;
}

yyjson_val* JsonDoc::get_path(yyjson_val* root, const std::string& path) {
    if (!root || path.empty()) return root;

    // Convert dot/bracket notation to JSON Pointer (RFC 6901).
    // Examples:
    //   "choices"                    -> "/choices"
    //   "choices[0]"                 -> "/choices/0"
    //   "choices[0].message.content" -> "/choices/0/message/content"
    std::string pointer;
    pointer.reserve(path.size() + 1);
    pointer += '/';

    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '.') {
            pointer += '/';
        } else if (c == '[') {
            pointer += '/';
        } else if (c == ']') {
            continue;
        } else {
            pointer += c;
        }
    }

    return yyjson_ptr_get(root, pointer.c_str());
}

} // namespace lyrics
