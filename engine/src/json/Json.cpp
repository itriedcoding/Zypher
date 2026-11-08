#include "zypher/json/Json.h"

#include "zypher/math/Vec.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zy {
namespace {

const JsonValue& nullValue() {
    static const JsonValue value;
    return value;
}

const JsonArray& emptyArray() {
    static const JsonArray value;
    return value;
}

const JsonObject& emptyObject() {
    static const JsonObject value;
    return value;
}

[[nodiscard]] f64 readNumber(std::string_view text) {
    const std::string copy(text);
    char* end = nullptr;
    const double parsed = std::strtod(copy.c_str(), &end);
    return (end == copy.c_str()) ? 0.0 : parsed;
}

void appendUtf8(std::string& output, u32 codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

[[nodiscard]] int hexDigit(char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

class Parser {
public:
    Parser(std::string_view text, usize maxDepth) : m_text(text), m_maxDepth(maxDepth) {}

    [[nodiscard]] bool parse(JsonValue& output) {
        skipTrivia();
        if (!parseValue(output, 0)) {
            return false;
        }
        skipTrivia();
        if (m_position != m_text.size()) {
            return fail("unexpected trailing characters");
        }
        return true;
    }

    [[nodiscard]] const std::string& error() const { return m_error; }

private:
    [[nodiscard]] bool atEnd() const { return m_position >= m_text.size(); }
    [[nodiscard]] char peek() const { return atEnd() ? '\0' : m_text[m_position]; }

    bool fail(std::string message) {
        if (m_error.empty()) {
            m_error = std::move(message);
            m_error += " at byte ";
            m_error += std::to_string(m_position);
        }
        return false;
    }

    void skipTrivia() {
        for (;;) {
            while (!atEnd()) {
                const char character = peek();
                if (character == ' ' || character == '\t' || character == '\n' || character == '\r') {
                    ++m_position;
                } else {
                    break;
                }
            }
            if (m_position + 1 < m_text.size() && peek() == '/' && m_text[m_position + 1] == '/') {
                m_position += 2;
                while (!atEnd() && peek() != '\n') {
                    ++m_position;
                }
                continue;
            }
            if (m_position + 1 < m_text.size() && peek() == '/' && m_text[m_position + 1] == '*') {
                m_position += 2;
                while (m_position + 1 < m_text.size() &&
                       !(peek() == '*' && m_text[m_position + 1] == '/')) {
                    ++m_position;
                }
                m_position = (m_position + 1 < m_text.size()) ? m_position + 2 : m_text.size();
                continue;
            }
            break;
        }
    }

    bool parseValue(JsonValue& output, usize depth) {
        if (depth > m_maxDepth) {
            return fail("maximum nesting depth exceeded");
        }
        skipTrivia();
        if (atEnd()) {
            return fail("unexpected end of input");
        }
        switch (peek()) {
        case '{': return parseObject(output, depth);
        case '[': return parseArray(output, depth);
        case '"': {
            std::string text;
            if (!parseString(text)) {
                return false;
            }
            output = JsonValue(std::move(text));
            return true;
        }
        case 't':
            if (!expectLiteral("true")) return false;
            output = JsonValue(true);
            return true;
        case 'f':
            if (!expectLiteral("false")) return false;
            output = JsonValue(false);
            return true;
        case 'n':
            if (!expectLiteral("null")) return false;
            output = JsonValue();
            return true;
        default:
            return parseNumber(output);
        }
    }

    bool expectLiteral(std::string_view literal) {
        if (m_text.substr(m_position, literal.size()) != literal) {
            return fail("invalid literal");
        }
        m_position += literal.size();
        return true;
    }

    bool parseNumber(JsonValue& output) {
        const usize start = m_position;
        if (!atEnd() && (peek() == '-' || peek() == '+')) {
            ++m_position;
        }
        bool sawDigit = false;
        while (!atEnd() && peek() >= '0' && peek() <= '9') {
            ++m_position;
            sawDigit = true;
        }
        if (!atEnd() && peek() == '.') {
            ++m_position;
            while (!atEnd() && peek() >= '0' && peek() <= '9') {
                ++m_position;
                sawDigit = true;
            }
        }
        if (!atEnd() && (peek() == 'e' || peek() == 'E')) {
            ++m_position;
            if (!atEnd() && (peek() == '-' || peek() == '+')) {
                ++m_position;
            }
            while (!atEnd() && peek() >= '0' && peek() <= '9') {
                ++m_position;
            }
        }
        if (!sawDigit) {
            return fail("invalid number");
        }
        output = JsonValue(readNumber(m_text.substr(start, m_position - start)));
        return true;
    }

    bool parseString(std::string& output) {
        if (peek() != '"') {
            return fail("expected a string");
        }
        ++m_position;
        output.clear();
        while (!atEnd()) {
            const char character = m_text[m_position++];
            if (character == '"') {
                return true;
            }
            if (character != '\\') {
                output.push_back(character);
                continue;
            }
            if (atEnd()) {
                return fail("unterminated escape sequence");
            }
            const char escape = m_text[m_position++];
            switch (escape) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                if (m_position + 4 > m_text.size()) {
                    return fail("truncated unicode escape");
                }
                u32 codePoint = 0;
                for (int digit = 0; digit < 4; ++digit) {
                    const int value = hexDigit(m_text[m_position++]);
                    if (value < 0) {
                        return fail("invalid unicode escape");
                    }
                    codePoint = (codePoint << 4) | static_cast<u32>(value);
                }
                appendUtf8(output, codePoint);
                break;
            }
            default: return fail("unknown escape sequence");
            }
        }
        return fail("unterminated string");
    }

    bool parseArray(JsonValue& output, usize depth) {
        if (peek() != '[') {
            return fail("expected '['");
        }
        ++m_position;
        JsonArray array;
        skipTrivia();
        if (!atEnd() && peek() == ']') {
            ++m_position;
            output = JsonValue(std::move(array));
            return true;
        }
        for (;;) {
            JsonValue element;
            if (!parseValue(element, depth + 1)) {
                return false;
            }
            array.push_back(std::move(element));
            skipTrivia();
            if (atEnd()) {
                return fail("unterminated array");
            }
            if (peek() == ',') {
                ++m_position;
                skipTrivia();
                if (!atEnd() && peek() == ']') {
                    return fail("trailing comma in array");
                }
                continue;
            }
            if (peek() == ']') {
                ++m_position;
                output = JsonValue(std::move(array));
                return true;
            }
            return fail("expected ',' or ']'");
        }
    }

    bool parseObject(JsonValue& output, usize depth) {
        if (peek() != '{') {
            return fail("expected '{'");
        }
        ++m_position;
        JsonObject object;
        skipTrivia();
        if (!atEnd() && peek() == '}') {
            ++m_position;
            output = JsonValue(std::move(object));
            return true;
        }
        for (;;) {
            skipTrivia();
            std::string key;
            if (!parseString(key)) {
                return false;
            }
            skipTrivia();
            if (atEnd() || peek() != ':') {
                return fail("expected ':' after object key");
            }
            ++m_position;
            JsonValue value;
            if (!parseValue(value, depth + 1)) {
                return false;
            }
            object.emplace_back(std::move(key), std::move(value));
            skipTrivia();
            if (atEnd()) {
                return fail("unterminated object");
            }
            if (peek() == ',') {
                ++m_position;
                continue;
            }
            if (peek() == '}') {
                ++m_position;
                output = JsonValue(std::move(object));
                return true;
            }
            return fail("expected ',' or '}'");
        }
    }

    std::string_view m_text;
    usize m_position = 0;
    usize m_maxDepth = 64;
    std::string m_error;
};

void writeEscaped(std::string_view text, std::string& output) {
    output.push_back('"');
    for (const char character : text) {
        switch (character) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                char buffer[8]{};
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(character));
                output += buffer;
            } else {
                output.push_back(character);
            }
        }
    }
    output.push_back('"');
}

void writeNumber(f64 value, std::string& output) {
    if (!std::isfinite(value)) {
        output += "null";
        return;
    }
    char buffer[48]{};
    if (value == std::floor(value) && std::fabs(value) < 1.0e15) {
        std::snprintf(buffer, sizeof(buffer), "%.0f", value);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.9g", value);
    }
    output += buffer;
}

void writeValue(const JsonValue& value, std::string& output, bool pretty, u32 indentLevel) {
    const auto indent = [&](u32 level) {
        if (pretty) {
            output.push_back('\n');
            output.append(static_cast<usize>(level) * 2, ' ');
        }
    };
    switch (value.type()) {
    case JsonValue::Type::Null: output += "null"; break;
    case JsonValue::Type::Bool: output += value.asBool() ? "true" : "false"; break;
    case JsonValue::Type::Number: writeNumber(value.asDouble(), output); break;
    case JsonValue::Type::String: writeEscaped(value.asString(), output); break;
    case JsonValue::Type::Array: {
        const auto& array = value.asArray();
        if (array.empty()) {
            output += "[]";
            break;
        }
        output.push_back('[');
        for (usize index = 0; index < array.size(); ++index) {
            if (index > 0) {
                output.push_back(',');
            }
            indent(indentLevel + 1);
            writeValue(array[index], output, pretty, indentLevel + 1);
        }
        indent(indentLevel);
        output.push_back(']');
        break;
    }
    case JsonValue::Type::Object: {
        const auto& object = value.asObject();
        if (object.empty()) {
            output += "{}";
            break;
        }
        output.push_back('{');
        for (usize index = 0; index < object.size(); ++index) {
            if (index > 0) {
                output.push_back(',');
            }
            indent(indentLevel + 1);
            writeEscaped(object[index].first, output);
            output.push_back(':');
            if (pretty) {
                output.push_back(' ');
            }
            writeValue(object[index].second, output, pretty, indentLevel + 1);
        }
        indent(indentLevel);
        output.push_back('}');
        break;
    }
    }
}

} // namespace

bool JsonValue::asBool(bool fallback) const {
    if (const auto* value = std::get_if<bool>(&m_data); value != nullptr) {
        return *value;
    }
    if (const auto* value = std::get_if<f64>(&m_data); value != nullptr) {
        return *value != 0.0;
    }
    return fallback;
}

f64 JsonValue::asDouble(f64 fallback) const {
    if (const auto* value = std::get_if<f64>(&m_data); value != nullptr) {
        return *value;
    }
    if (const auto* value = std::get_if<bool>(&m_data); value != nullptr) {
        return *value ? 1.0 : 0.0;
    }
    if (const auto* value = std::get_if<std::string>(&m_data); value != nullptr) {
        return readNumber(*value);
    }
    return fallback;
}

f32 JsonValue::asFloat(f32 fallback) const { return static_cast<f32>(asDouble(fallback)); }

i64 JsonValue::asInt(i64 fallback) const {
    if (const auto* value = std::get_if<f64>(&m_data); value != nullptr) {
        return static_cast<i64>(*value);
    }
    return fallback;
}

std::string JsonValue::asString(std::string_view fallback) const {
    if (const auto* value = std::get_if<std::string>(&m_data); value != nullptr) {
        return *value;
    }
    return std::string(fallback);
}

const JsonArray& JsonValue::asArray() const {
    const auto* value = std::get_if<JsonArray>(&m_data);
    return value != nullptr ? *value : emptyArray();
}

JsonArray& JsonValue::asArrayMutable() {
    auto* value = std::get_if<JsonArray>(&m_data);
    if (value == nullptr) {
        m_data = JsonArray{};
        value = std::get_if<JsonArray>(&m_data);
    }
    return *value;
}

const JsonObject& JsonValue::asObject() const {
    const auto* value = std::get_if<JsonObject>(&m_data);
    return value != nullptr ? *value : emptyObject();
}

JsonObject& JsonValue::asObjectMutable() {
    auto* value = std::get_if<JsonObject>(&m_data);
    if (value == nullptr) {
        m_data = JsonObject{};
        value = std::get_if<JsonObject>(&m_data);
    }
    return *value;
}

usize JsonValue::size() const {
    if (const auto* value = std::get_if<JsonArray>(&m_data); value != nullptr) {
        return value->size();
    }
    if (const auto* value = std::get_if<JsonObject>(&m_data); value != nullptr) {
        return value->size();
    }
    if (const auto* value = std::get_if<std::string>(&m_data); value != nullptr) {
        return value->size();
    }
    return 0;
}

const JsonValue& JsonValue::at(usize index) const {
    const auto& array = asArray();
    return index < array.size() ? array[index] : nullValue();
}

bool JsonValue::has(std::string_view key) const { return find(key) != nullptr; }

const JsonValue* JsonValue::find(std::string_view key) const {
    const auto* object = std::get_if<JsonObject>(&m_data);
    if (object == nullptr) {
        return nullptr;
    }
    for (const auto& entry : *object) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

const JsonValue& JsonValue::operator[](std::string_view key) const {
    const JsonValue* value = find(key);
    return value != nullptr ? *value : nullValue();
}

f32 JsonValue::getFloat(std::string_view key, f32 fallback) const {
    const JsonValue* value = find(key);
    return value != nullptr ? value->asFloat(fallback) : fallback;
}

i64 JsonValue::getInt(std::string_view key, i64 fallback) const {
    const JsonValue* value = find(key);
    return value != nullptr ? value->asInt(fallback) : fallback;
}

bool JsonValue::getBool(std::string_view key, bool fallback) const {
    const JsonValue* value = find(key);
    return value != nullptr ? value->asBool(fallback) : fallback;
}

std::string JsonValue::getString(std::string_view key, std::string_view fallback) const {
    const JsonValue* value = find(key);
    return value != nullptr ? value->asString(fallback) : std::string(fallback);
}

Vec3 JsonValue::getVec3(std::string_view key, const Vec3& fallback) const {
    const JsonValue* value = find(key);
    if (value == nullptr) {
        return fallback;
    }
    if (value->isArray() && value->size() >= 3) {
        return Vec3{value->at(0).asFloat(), value->at(1).asFloat(), value->at(2).asFloat()};
    }
    if (value->isObject()) {
        return Vec3{value->getFloat("x", fallback.x), value->getFloat("y", fallback.y),
                    value->getFloat("z", fallback.z)};
    }
    if (value->isNumber()) {
        const f32 scalar = value->asFloat();
        return Vec3{scalar, scalar, scalar};
    }
    return fallback;
}

Vec4 JsonValue::getVec4(std::string_view key, const Vec4& fallback) const {
    const JsonValue* value = find(key);
    if (value == nullptr) {
        return fallback;
    }
    if (value->isArray() && value->size() >= 4) {
        return Vec4{value->at(0).asFloat(), value->at(1).asFloat(), value->at(2).asFloat(),
                    value->at(3).asFloat()};
    }
    if (value->isArray() && value->size() == 3) {
        return Vec4{value->at(0).asFloat(), value->at(1).asFloat(), value->at(2).asFloat(), 1.0f};
    }
    if (value->isObject()) {
        return Vec4{value->getFloat("r", fallback.x), value->getFloat("g", fallback.y),
                    value->getFloat("b", fallback.z), value->getFloat("a", fallback.w)};
    }
    return fallback;
}

void JsonValue::push(JsonValue value) { asArrayMutable().push_back(std::move(value)); }

void JsonValue::set(std::string key, JsonValue value) {
    auto& object = asObjectMutable();
    for (auto& entry : object) {
        if (entry.first == key) {
            entry.second = std::move(value);
            return;
        }
    }
    object.emplace_back(std::move(key), std::move(value));
}

std::string JsonValue::dump(bool pretty) const { return toJsonString(*this, pretty); }

Result<JsonValue> parseJson(std::string_view text) {
    Parser parser(text, 128);
    JsonValue value;
    if (!parser.parse(value)) {
        return Error{ErrorCode::Parse, parser.error()};
    }
    return value;
}

std::string toJsonString(const JsonValue& value, bool pretty) {
    std::string output;
    output.reserve(1024);
    writeValue(value, output, pretty, 0);
    if (pretty) {
        output.push_back('\n');
    }
    return output;
}

} // namespace zy
