#include "crossa/network/json/JsonParser.h"

#include <stdexcept>
#include <unordered_set>

using namespace std;

namespace crossa::network::json {

    // Parses one complete JSON document under the supplied limits.
    JsonValue JsonParser::parse(
        string_view input,
        size_t maximumBytes,
        size_t maximumDepth,
        const runtime::RequestHandle* requestHandle
    ) {
        if (input.size() > maximumBytes) {
            throw runtime_error("JSON input exceeds the configured byte limit.");
        }
        if (requestHandle != nullptr) {
            requestHandle->throwIfCancellationRequested();
        }
        JsonParser parser(input, maximumDepth, requestHandle);
        parser.skipWhitespace();
        JsonValue result = parser.parseValue(0);
        parser.skipWhitespace();
        if (!parser.isAtEnd()) {
            parser.fail("Unexpected trailing JSON content.");
        }
        return result;
    }

    // Creates one parser over a bounded input view.
    JsonParser::JsonParser(
        string_view input,
        size_t maximumDepth,
        const runtime::RequestHandle* requestHandle
    ) noexcept
        : input_(input),
          maximumDepth_(maximumDepth),
          current_(0),
          nextCancellationCheck_(0),
          requestHandle_(requestHandle) {}

    // Parses one JSON value at the requested nesting depth.
    JsonValue JsonParser::parseValue(size_t depth) {
        if (depth > maximumDepth_) {
            fail("JSON nesting exceeds the configured depth limit.");
        }
        skipWhitespace();
        switch (peek()) {
            case '{':
                return parseObject(depth + 1);
            case '[':
                return parseArray(depth + 1);
            case '"':
                return JsonValue::createString(parseString());
            case 't':
                consumeLiteral("true");
                return JsonValue::createBoolean(true);
            case 'f':
                consumeLiteral("false");
                return JsonValue::createBoolean(false);
            case 'n':
                consumeLiteral("null");
                return JsonValue::createNull();
            default:
                if (peek() == '-' || (peek() >= '0' && peek() <= '9')) {
                    return parseNumber();
                }
                fail("Expected a JSON value.");
        }
    }

    // Parses one JSON object.
    JsonValue JsonParser::parseObject(size_t depth) {
        consume('{');
        skipWhitespace();
        JsonValue::Object values;
        unordered_set<string> keys;
        if (peek() == '}') {
            advance();
            return JsonValue::createObject(std::move(values));
        }

        while (true) {
            skipWhitespace();
            if (peek() != '"') {
                fail("Expected a quoted JSON object key.");
            }
            string key = parseString();
            if (!keys.insert(key).second) {
                fail("Duplicate JSON object key '" + key + "'.");
            }
            skipWhitespace();
            consume(':');
            values.emplace_back(std::move(key), parseValue(depth));
            skipWhitespace();
            if (peek() == '}') {
                advance();
                break;
            }
            consume(',');
        }
        return JsonValue::createObject(std::move(values));
    }

    // Parses one JSON array.
    JsonValue JsonParser::parseArray(size_t depth) {
        consume('[');
        skipWhitespace();
        JsonValue::Array values;
        if (peek() == ']') {
            advance();
            return JsonValue::createArray(std::move(values));
        }

        while (true) {
            values.push_back(parseValue(depth));
            skipWhitespace();
            if (peek() == ']') {
                advance();
                break;
            }
            consume(',');
        }
        return JsonValue::createArray(std::move(values));
    }

    // Parses and decodes one JSON string.
    string JsonParser::parseString() {
        consume('"');
        string output;
        while (!isAtEnd()) {
            const unsigned char value = static_cast<unsigned char>(advance());
            if (value == '"') {
                return output;
            }
            if (value < 0x20) {
                fail("JSON string contains an unescaped control byte.");
            }
            if (value != '\\') {
                output.push_back(static_cast<char>(value));
                continue;
            }

            const char escaped = advance();
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    output.push_back(escaped);
                    break;
                case 'b':
                    output.push_back('\b');
                    break;
                case 'f':
                    output.push_back('\f');
                    break;
                case 'n':
                    output.push_back('\n');
                    break;
                case 'r':
                    output.push_back('\r');
                    break;
                case 't':
                    output.push_back('\t');
                    break;
                case 'u': {
                    unsigned int codePoint = parseHexCodeUnit();
                    if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                        consume('\\');
                        consume('u');
                        const unsigned int low = parseHexCodeUnit();
                        if (low < 0xDC00 || low > 0xDFFF) {
                            fail("Invalid JSON Unicode surrogate pair.");
                        }
                        codePoint = 0x10000 +
                            ((codePoint - 0xD800) << 10U) +
                            (low - 0xDC00);
                    } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
                        fail("Unexpected low JSON Unicode surrogate.");
                    }
                    appendCodePoint(codePoint, output);
                    break;
                }
                default:
                    fail("Invalid JSON string escape.");
            }
        }
        fail("Unterminated JSON string.");
    }

    // Parses one validated JSON number.
    JsonValue JsonParser::parseNumber() {
        const size_t start = current_;
        if (peek() == '-') {
            advance();
        }
        if (peek() == '0') {
            advance();
        } else {
            if (peek() < '1' || peek() > '9') {
                fail("Invalid JSON number integer part.");
            }
            while (peek() >= '0' && peek() <= '9') {
                advance();
            }
        }
        if (peek() == '.') {
            advance();
            if (peek() < '0' || peek() > '9') {
                fail("Invalid JSON number fraction.");
            }
            while (peek() >= '0' && peek() <= '9') {
                advance();
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') {
                advance();
            }
            if (peek() < '0' || peek() > '9') {
                fail("Invalid JSON number exponent.");
            }
            while (peek() >= '0' && peek() <= '9') {
                advance();
            }
        }
        return JsonValue::createNumber(string(input_.substr(start, current_ - start)));
    }

    // Parses one four-digit Unicode escape.
    unsigned int JsonParser::parseHexCodeUnit() {
        unsigned int value = 0;
        for (size_t index = 0; index < 4; ++index) {
            const char digit = advance();
            value <<= 4U;
            if (digit >= '0' && digit <= '9') {
                value += static_cast<unsigned int>(digit - '0');
            } else if (digit >= 'a' && digit <= 'f') {
                value += static_cast<unsigned int>(digit - 'a' + 10);
            } else if (digit >= 'A' && digit <= 'F') {
                value += static_cast<unsigned int>(digit - 'A' + 10);
            } else {
                fail("Invalid hexadecimal digit in JSON Unicode escape.");
            }
        }
        return value;
    }

    // Appends one Unicode code point as UTF-8.
    void JsonParser::appendCodePoint(unsigned int codePoint, string& output) {
        if (codePoint <= 0x7F) {
            output.push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | (codePoint >> 6U)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3FU)));
        } else if (codePoint <= 0xFFFF) {
            output.push_back(static_cast<char>(0xE0 | (codePoint >> 12U)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3FU)));
        } else if (codePoint <= 0x10FFFF) {
            output.push_back(static_cast<char>(0xF0 | (codePoint >> 18U)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 12U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3FU)));
        } else {
            throw runtime_error("JSON Unicode code point is out of range.");
        }
    }

    // Skips JSON whitespace.
    void JsonParser::skipWhitespace() noexcept {
        while (peek() == ' ' || peek() == '\t' ||
               peek() == '\n' || peek() == '\r') {
            ++current_;
        }
    }

    // Consumes one exact literal or fails.
    void JsonParser::consumeLiteral(string_view literal) {
        for (const char expected : literal) {
            consume(expected);
        }
    }

    // Consumes one expected byte or fails.
    void JsonParser::consume(char expected) {
        if (peek() != expected) {
            fail("Expected '" + string(1, expected) + "'.");
        }
        advance();
    }

    // Returns and consumes the current byte.
    char JsonParser::advance() {
        if (isAtEnd()) {
            fail("Unexpected end of JSON input.");
        }
        checkCancellation();
        return input_[current_++];
    }

    // Returns the current byte without consuming it.
    char JsonParser::peek() const noexcept {
        return isAtEnd() ? '\0' : input_[current_];
    }

    // Returns whether all input bytes were consumed.
    bool JsonParser::isAtEnd() const noexcept {
        return current_ >= input_.size();
    }

    // Checks cancellation at bounded byte intervals during parsing.
    void JsonParser::checkCancellation() {
        constexpr size_t CancellationCheckInterval = 4096;
        if (requestHandle_ == nullptr || current_ < nextCancellationCheck_) {
            return;
        }
        requestHandle_->throwIfCancellationRequested();
        nextCancellationCheck_ = current_ + CancellationCheckInterval;
    }

    // Throws a stable JSON parse failure at the current byte offset.
    [[noreturn]] void JsonParser::fail(const string& message) const {
        throw runtime_error(
            "JSON parse failed at byte " + to_string(current_) + ": " + message
        );
    }

}
