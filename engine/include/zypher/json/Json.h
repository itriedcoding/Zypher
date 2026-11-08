// A small, dependency free JSON implementation used for scenes, project files,
// asset manifests and editor messages. `//` and `/* */` comments are accepted
// as a documented extension so hand written scene files stay readable.
#pragma once

#include "zypher/core/Base.h"
#include "zypher/core/Result.h"
#include "zypher/math/Vec.h"

#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace zy {

class JsonValue;

using JsonArray = std::vector<JsonValue>;
using JsonObject = std::vector<std::pair<std::string, JsonValue>>;

class JsonValue {
public:
    enum class Type : u8 { Null = 0, Bool, Number, String, Array, Object };

    JsonValue() = default;
    JsonValue(std::nullptr_t) {} // NOLINT(google-explicit-constructor)
    JsonValue(bool value) : m_data(value) {} // NOLINT(google-explicit-constructor)
    JsonValue(f64 value) : m_data(value) {} // NOLINT(google-explicit-constructor)
    JsonValue(f32 value) : m_data(static_cast<f64>(value)) {} // NOLINT(google-explicit-constructor)
    JsonValue(i32 value) : m_data(static_cast<f64>(value)) {} // NOLINT(google-explicit-constructor)
    JsonValue(u32 value) : m_data(static_cast<f64>(value)) {} // NOLINT(google-explicit-constructor)
    JsonValue(const char* value) : m_data(std::string(value)) {} // NOLINT(google-explicit-constructor)
    JsonValue(std::string value) : m_data(std::move(value)) {} // NOLINT(google-explicit-constructor)
    JsonValue(JsonArray value) : m_data(std::move(value)) {} // NOLINT(google-explicit-constructor)
    JsonValue(JsonObject value) : m_data(std::move(value)) {} // NOLINT(google-explicit-constructor)

    [[nodiscard]] static JsonValue makeArray() { return JsonValue(JsonArray{}); }
    [[nodiscard]] static JsonValue makeObject() { return JsonValue(JsonObject{}); }

    [[nodiscard]] Type type() const { return static_cast<Type>(m_data.index()); }
    [[nodiscard]] bool isNull() const { return type() == Type::Null; }
    [[nodiscard]] bool isBool() const { return type() == Type::Bool; }
    [[nodiscard]] bool isNumber() const { return type() == Type::Number; }
    [[nodiscard]] bool isString() const { return type() == Type::String; }
    [[nodiscard]] bool isArray() const { return type() == Type::Array; }
    [[nodiscard]] bool isObject() const { return type() == Type::Object; }

    [[nodiscard]] bool asBool(bool fallback = false) const;
    [[nodiscard]] f64 asDouble(f64 fallback = 0.0) const;
    [[nodiscard]] f32 asFloat(f32 fallback = 0.0f) const;
    [[nodiscard]] i64 asInt(i64 fallback = 0) const;
    [[nodiscard]] std::string asString(std::string_view fallback = "") const;

    [[nodiscard]] const JsonArray& asArray() const;
    [[nodiscard]] JsonArray& asArrayMutable();
    [[nodiscard]] const JsonObject& asObject() const;
    [[nodiscard]] JsonObject& asObjectMutable();

    [[nodiscard]] usize size() const;
    [[nodiscard]] bool empty() const { return size() == 0; }

    [[nodiscard]] const JsonValue& at(usize index) const;
    [[nodiscard]] const JsonValue& operator[](std::string_view key) const;
    [[nodiscard]] bool has(std::string_view key) const;
    [[nodiscard]] const JsonValue* find(std::string_view key) const;

    [[nodiscard]] f32 getFloat(std::string_view key, f32 fallback) const;
    [[nodiscard]] i64 getInt(std::string_view key, i64 fallback) const;
    [[nodiscard]] bool getBool(std::string_view key, bool fallback) const;
    [[nodiscard]] std::string getString(std::string_view key, std::string_view fallback) const;

    // Vec2/Vec3/Vec4 helpers accept both arrays and {x,y,z} objects.
    [[nodiscard]] Vec4 getVec4(std::string_view key, const Vec4& fallback) const;
    [[nodiscard]] Vec3 getVec3(std::string_view key, const Vec3& fallback) const;

    void push(JsonValue value);
    void set(std::string key, JsonValue value);

    [[nodiscard]] std::string dump(bool pretty = false) const;

private:
    friend Result<JsonValue> parseJson(std::string_view text);
    std::variant<std::monostate, bool, f64, std::string, JsonArray, JsonObject> m_data;
};

[[nodiscard]] Result<JsonValue> parseJson(std::string_view text);
[[nodiscard]] std::string toJsonString(const JsonValue& value, bool pretty = false);

} // namespace zy
