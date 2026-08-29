#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "crossa/network/json/JsonValue.h"

namespace crossa::network::json {

// Parses untrusted JSON text with explicit depth and input-size limits.
// parse() returns an owned value and rejects duplicate object keys.
class JsonParser final {
public:
    // Parses one complete JSON document under the supplied limits.
    [[nodiscard]] static JsonValue parse(
        std::string_view input,
        std::size_t maximumBytes,
        std::size_t maximumDepth
    );

private:
    // Creates one parser over a bounded input view.
    JsonParser(std::string_view input, std::size_t maximumDepth) noexcept;

    // Parses one JSON value at the requested nesting depth.
    [[nodiscard]] JsonValue parseValue(std::size_t depth);

    // Parses one JSON object.
    [[nodiscard]] JsonValue parseObject(std::size_t depth);

    // Parses one JSON array.
    [[nodiscard]] JsonValue parseArray(std::size_t depth);

    // Parses and decodes one JSON string.
    [[nodiscard]] std::string parseString();

    // Parses one validated JSON number.
    [[nodiscard]] JsonValue parseNumber();

    // Parses one four-digit Unicode escape.
    [[nodiscard]] unsigned int parseHexCodeUnit();

    // Appends one Unicode code point as UTF-8.
    static void appendCodePoint(unsigned int codePoint, std::string& output);

    // Skips JSON whitespace.
    void skipWhitespace() noexcept;

    // Consumes one exact literal or fails.
    void consumeLiteral(std::string_view literal);

    // Consumes one expected byte or fails.
    void consume(char expected);

    // Returns and consumes the current byte.
    char advance();

    // Returns the current byte without consuming it.
    [[nodiscard]] char peek() const noexcept;

    // Returns whether all input bytes were consumed.
    [[nodiscard]] bool isAtEnd() const noexcept;

    // Throws a stable JSON parse failure at the current byte offset.
    [[noreturn]] void fail(const std::string& message) const;

    std::string_view input_;
    std::size_t maximumDepth_;
    std::size_t current_;
};

}
