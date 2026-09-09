#include "crossa/network/response/ResponseDecoder.h"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <optional>
#include <unordered_set>

#include "crossa/compiler/ir/IrDeclaration.h"
#include "crossa/network/json/JsonParser.h"
#include "crossa/runtime/errors/CrossaException.h"
#include "crossa/runtime/objects/NativeList.h"
#include "crossa/runtime/objects/NativeModel.h"

using namespace std;

namespace crossa::network::response {

namespace {

class DirectResponseDecoder final {
public:
    DirectResponseDecoder(
        const compiler::ir::Program& program,
        size_t maximumBytes,
        size_t maximumDepth,
        const runtime::RequestHandle& requestHandle
    ) noexcept
        : program_(program),
          maximumBytes_(maximumBytes),
          maximumDepth_(maximumDepth),
          requestHandle_(requestHandle) {}

    runtime::RuntimeValue decode(
        const string& body,
        const compiler::types::SemanticType& expectedType
    ) {
        if (body.size() > maximumBytes_) {
            throw runtime_error("JSON input exceeds the configured byte limit.");
        }
        requestHandle_.throwIfCancellationRequested();
        input_ = body;
        current_ = 0;
        runtime::RuntimeValue result = parseTypedValue(
            expectedType,
            "$response",
            0
        );
        skipWhitespace();
        if (!isAtEnd()) {
            fail("Unexpected trailing JSON content.");
        }
        return result;
    }

private:
    runtime::RuntimeValue parseTypedValue(
        const compiler::types::SemanticType& expectedType,
        const string& path,
        size_t depth
    ) {
        checkDepth(depth);
        requestHandle_.throwIfCancellationRequested();
        using compiler::types::SemanticTypeKind;
        switch (expectedType.getKind()) {
            case SemanticTypeKind::Unit:
                return runtime::RuntimeValue::createUnit();
            case SemanticTypeKind::Int:
                return runtime::RuntimeValue::createInt(
                    parseInteger(parseTypedNumberText(path), path)
                );
            case SemanticTypeKind::Long:
                return runtime::RuntimeValue::createLong(
                    parseInteger(parseTypedNumberText(path), path)
                );
            case SemanticTypeKind::Double:
                return runtime::RuntimeValue::createDouble(
                    parseDouble(parseTypedNumberText(path), path)
                );
            case SemanticTypeKind::String:
                if (peek() != '"') {
                    failType(path + " must be a JSON string.");
                }
                return runtime::RuntimeValue::createString(parseString());
            case SemanticTypeKind::Bool:
                if (peek() == 't') {
                    consumeLiteral("true");
                    return runtime::RuntimeValue::createBool(true);
                }
                if (peek() == 'f') {
                    consumeLiteral("false");
                    return runtime::RuntimeValue::createBool(false);
                }
                failType(path + " must be a JSON boolean.");
            case SemanticTypeKind::Json:
                return runtime::RuntimeValue::createJson(parseGenericValue(depth));
            case SemanticTypeKind::Model:
                return parseModel(expectedType.getModelName(), path, depth);
            case SemanticTypeKind::List:
                return parseList(expectedType, path, depth);
        }
        throw runtime::CrossaException(
            runtime::CrossaError::runtime("Unknown semantic response type.")
        );
    }

    runtime::RuntimeValue parseModel(
        const string& modelName,
        const string& path,
        size_t depth
    ) {
        const compiler::ir::IrModelDeclaration& model = findModel(modelName);
        if (peek() != '{') {
            failType(path + " must be model '" + modelName + "'.");
        }
        consume('{');
        skipWhitespace();
        vector<optional<runtime::RuntimeValue>> values(model.getFields().size());
        unordered_set<string> keys;
        if (peek() != '}') {
            while (true) {
                skipWhitespace();
                const string key = parseStringKey();
                if (!keys.insert(key).second) {
                    fail("Duplicate JSON object key '" + key + "'.");
                }
                skipWhitespace();
                consume(':');
                skipWhitespace();
                const auto field = findField(model, key);
                if (field.has_value()) {
                    const size_t fieldIndex = field->first;
                    values[fieldIndex] = parseTypedValue(
                        field->second->getType(),
                        path + "." + key,
                        depth + 1
                    );
                } else {
                    skipValue(depth + 1);
                }
                skipWhitespace();
                if (peek() == '}') {
                    advance();
                    break;
                }
                consume(',');
                skipWhitespace();
            }
        } else {
            advance();
        }

        runtime::NativeModel::Fields fields;
        fields.reserve(model.getFields().size());
        for (size_t index = 0; index < model.getFields().size(); ++index) {
            if (!values[index].has_value()) {
                throw runtime::CrossaException(
                    runtime::CrossaError::responseTypeMismatch(
                        path + " is missing model field '" +
                        model.getFields()[index].getName() + "'."
                    )
                );
            }
            fields.emplace_back(
                model.getFields()[index].getName(),
                std::move(*values[index])
            );
        }
        return runtime::RuntimeValue::createModel(
            runtime::NativeModel(modelName, std::move(fields))
        );
    }

    runtime::RuntimeValue parseList(
        const compiler::types::SemanticType& expectedType,
        const string& path,
        size_t depth
    ) {
        const compiler::types::SemanticType* elementType =
            expectedType.getElementType();
        if (elementType == nullptr) {
            throw runtime::CrossaException(
                runtime::CrossaError::runtime(
                    "List response type has no element type."
                )
            );
        }
        if (peek() != '[') {
            failType(path + " must be a JSON array.");
        }
        consume('[');
        skipWhitespace();
        vector<runtime::RuntimeValue> values;
        if (peek() != ']') {
            size_t index = 0;
            while (true) {
                values.push_back(parseTypedValue(
                    *elementType,
                    path + "[" + to_string(index) + "]",
                    depth + 1
                ));
                ++index;
                skipWhitespace();
                if (peek() == ']') {
                    advance();
                    break;
                }
                consume(',');
                skipWhitespace();
            }
        } else {
            advance();
        }
        return runtime::RuntimeValue::createList(
            runtime::NativeList(std::move(values))
        );
    }

    json::JsonValue parseGenericValue(size_t depth) {
        checkDepth(depth);
        skipWhitespace();
        switch (peek()) {
            case '{':
                return parseGenericObject(depth);
            case '[':
                return parseGenericArray(depth);
            case '"':
                return json::JsonValue::createString(parseString());
            case 't':
                consumeLiteral("true");
                return json::JsonValue::createBoolean(true);
            case 'f':
                consumeLiteral("false");
                return json::JsonValue::createBoolean(false);
            case 'n':
                consumeLiteral("null");
                return json::JsonValue::createNull();
            default:
                if (peek() == '-' || (peek() >= '0' && peek() <= '9')) {
                    return json::JsonValue::createNumber(parseNumberText());
                }
                fail("Expected a JSON value.");
        }
    }

    json::JsonValue parseGenericObject(size_t depth) {
        consume('{');
        skipWhitespace();
        json::JsonValue::Object values;
        unordered_set<string> keys;
        if (peek() != '}') {
            while (true) {
                skipWhitespace();
                const string key = parseStringKey();
                if (!keys.insert(key).second) {
                    fail("Duplicate JSON object key '" + key + "'.");
                }
                skipWhitespace();
                consume(':');
                values.emplace_back(std::move(key), parseGenericValue(depth + 1));
                skipWhitespace();
                if (peek() == '}') {
                    advance();
                    break;
                }
                consume(',');
                skipWhitespace();
            }
        } else {
            advance();
        }
        return json::JsonValue::createObject(std::move(values));
    }

    json::JsonValue parseGenericArray(size_t depth) {
        consume('[');
        skipWhitespace();
        json::JsonValue::Array values;
        if (peek() != ']') {
            while (true) {
                values.push_back(parseGenericValue(depth + 1));
                skipWhitespace();
                if (peek() == ']') {
                    advance();
                    break;
                }
                consume(',');
                skipWhitespace();
            }
        } else {
            advance();
        }
        return json::JsonValue::createArray(std::move(values));
    }

    void skipValue(size_t depth) {
        checkDepth(depth);
        skipWhitespace();
        switch (peek()) {
            case '"':
                (void)parseString();
                return;
            case 't':
                consumeLiteral("true");
                return;
            case 'f':
                consumeLiteral("false");
                return;
            case 'n':
                consumeLiteral("null");
                return;
            case '{':
                consume('{');
                skipWhitespace();
                {
                    unordered_set<string> keys;
                    if (peek() == '}') {
                        advance();
                        return;
                    }
                    while (true) {
                        skipWhitespace();
                        const string key = parseStringKey();
                        if (!keys.insert(key).second) {
                            fail("Duplicate JSON object key '" + key + "'.");
                        }
                        skipWhitespace();
                        consume(':');
                        skipValue(depth + 1);
                        skipWhitespace();
                        if (peek() == '}') {
                            advance();
                            return;
                        }
                        consume(',');
                        skipWhitespace();
                }
            }
            return;
            case '[':
                consume('[');
                skipWhitespace();
                if (peek() == ']') {
                    advance();
                    return;
                }
                while (true) {
                    skipValue(depth + 1);
                    skipWhitespace();
                    if (peek() == ']') {
                        advance();
                        return;
                    }
                    consume(',');
                }
            default:
                if (peek() == '-' || (peek() >= '0' && peek() <= '9')) {
                    (void)parseNumberText();
                    return;
                }
                fail("Expected a JSON value.");
        }
    }

    const compiler::ir::IrModelDeclaration& findModel(
        const string& name
    ) const {
        for (const unique_ptr<compiler::ir::IrDeclaration>& declaration :
             program_.getDeclarations()) {
            if (declaration->getKind() != compiler::ir::IrDeclarationKind::Model) {
                continue;
            }
            const auto& model = static_cast<
                const compiler::ir::IrModelDeclaration&>(*declaration);
            if (model.getName() == name) return model;
        }
        throw runtime::CrossaException(
            runtime::CrossaError::runtime(
                "Missing IR schema for model '" + name + "'."
            )
        );
    }

    static optional<pair<size_t, const compiler::ir::IrModelField*>> findField(
        const compiler::ir::IrModelDeclaration& model,
        const string& name
    ) {
        for (size_t index = 0; index < model.getFields().size(); ++index) {
            if (model.getFields()[index].getName() == name) {
                return make_pair(index, &model.getFields()[index]);
            }
        }
        return nullopt;
    }

    string parseStringKey() {
        if (peek() != '"') fail("Expected a quoted JSON object key.");
        return parseString();
    }

    string parseString() {
        consume('"');
        string output;
        while (!isAtEnd()) {
            const unsigned char value = static_cast<unsigned char>(advance());
            if (value == '"') return output;
            if (value < 0x20) fail("JSON string contains an unescaped control byte.");
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
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
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
                            ((codePoint - 0xD800) << 10U) + low - 0xDC00;
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

    string parseNumberText() {
        const size_t start = current_;
        if (peek() == '-') advance();
        if (peek() == '0') {
            advance();
        } else {
            if (peek() < '1' || peek() > '9') fail("Invalid JSON number integer part.");
            while (peek() >= '0' && peek() <= '9') advance();
        }
        if (peek() == '.') {
            advance();
            if (peek() < '0' || peek() > '9') fail("Invalid JSON number fraction.");
            while (peek() >= '0' && peek() <= '9') advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            if (peek() < '0' || peek() > '9') fail("Invalid JSON number exponent.");
            while (peek() >= '0' && peek() <= '9') advance();
        }
        return string(input_.substr(start, current_ - start));
    }

    string parseTypedNumberText(const string& path) {
        if (peek() != '-' && (peek() < '0' || peek() > '9')) {
            failType(path + " must be a JSON number.");
        }
        return parseNumberText();
    }

    static int64_t parseInteger(const string& number, const string& path) {
        int64_t result = 0;
        const auto conversion = from_chars(
            number.data(), number.data() + number.size(), result
        );
        if (conversion.ec != errc{} ||
            conversion.ptr != number.data() + number.size()) {
            throw runtime::CrossaException(
                runtime::CrossaError::responseTypeMismatch(
                    path + " must be a JSON integer that fits the Crossa range."
                )
            );
        }
        return result;
    }

    static double parseDouble(const string& number, const string& path) {
        size_t processed = 0;
        try {
            const double result = stod(number, &processed);
            if (processed == number.size()) return result;
        } catch (const exception&) {
        }
        throw runtime::CrossaException(
            runtime::CrossaError::responseTypeMismatch(
                path + " must fit the Crossa Double range."
            )
        );
    }

    unsigned int parseHexCodeUnit() {
        unsigned int value = 0;
        for (size_t index = 0; index < 4; ++index) {
            const char digit = advance();
            value <<= 4U;
            if (digit >= '0' && digit <= '9') value += digit - '0';
            else if (digit >= 'a' && digit <= 'f') value += digit - 'a' + 10;
            else if (digit >= 'A' && digit <= 'F') value += digit - 'A' + 10;
            else fail("Invalid hexadecimal digit in JSON Unicode escape.");
        }
        return value;
    }

    void appendCodePoint(unsigned int codePoint, string& output) {
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
            fail("JSON Unicode code point is out of range.");
        }
    }

    void checkDepth(size_t depth) const {
        if (depth > maximumDepth_) {
            fail("JSON nesting exceeds the configured depth limit.");
        }
    }

    [[noreturn]] void failType(const string& message) const {
        throw runtime::CrossaException(
            runtime::CrossaError::responseTypeMismatch(message)
        );
    }

    [[noreturn]] void fail(const string& message) const {
        throw runtime_error(
            "JSON parse failed at byte " + to_string(current_) + ": " + message
        );
    }

    void skipWhitespace() noexcept {
        while (peek() == ' ' || peek() == '\t' ||
               peek() == '\n' || peek() == '\r') {
            ++current_;
        }
    }

    void consumeLiteral(string_view literal) {
        for (const char expected : literal) consume(expected);
    }

    void consume(char expected) {
        if (peek() != expected) fail("Expected '" + string(1, expected) + "'.");
        advance();
    }

    char advance() {
        if (isAtEnd()) fail("Unexpected end of JSON input.");
        return input_[current_++];
    }

    [[nodiscard]] char peek() const noexcept {
        return isAtEnd() ? '\0' : input_[current_];
    }

    [[nodiscard]] bool isAtEnd() const noexcept {
        return current_ >= input_.size();
    }

    const compiler::ir::Program& program_;
    size_t maximumBytes_;
    size_t maximumDepth_;
    const runtime::RequestHandle& requestHandle_;
    string_view input_;
    size_t current_ = 0;
};

}

    // Creates a schema-aware decoder with explicit input and depth limits.
    ResponseDecoder::ResponseDecoder(
        const compiler::ir::Program& program,
        size_t maximumBytes,
        size_t maximumDepth
    ) noexcept
        : program_(program),
          maximumBytes_(maximumBytes),
          maximumDepth_(maximumDepth) {}

    // Decodes one successful response body into a native runtime value.
    runtime::RuntimeValue ResponseDecoder::decode(
        const string& body,
        const compiler::types::SemanticType& expectedType,
        const runtime::RequestHandle& requestHandle
    ) const {
        using compiler::types::SemanticTypeKind;

        if (body.size() > maximumBytes_) {
            throw runtime::CrossaException(
                runtime::CrossaError::invalidJson(
                    "JSON input exceeds the configured byte limit."
                )
            );
        }
        requestHandle.throwIfCancellationRequested();
        if (expectedType.getKind() == SemanticTypeKind::Unit) {
            return runtime::RuntimeValue::createUnit();
        }
        if (expectedType.getKind() == SemanticTypeKind::String) {
            return runtime::RuntimeValue::createString(body);
        }

        if (expectedType.getKind() != SemanticTypeKind::Json) {
            try {
                return DirectResponseDecoder(
                    program_, maximumBytes_, maximumDepth_, requestHandle
                ).decode(body, expectedType);
            } catch (const runtime::CrossaException&) {
                throw;
            } catch (const exception& error) {
                throw runtime::CrossaException(
                    runtime::CrossaError::invalidJson(error.what())
                );
            }
        }

        json::JsonValue value = json::JsonValue::createNull();
        try {
            value = json::JsonParser::parse(
                body,
                maximumBytes_,
                maximumDepth_,
                &requestHandle
            );
        } catch (const runtime::CrossaException&) {
            throw;
        } catch (const exception& error) {
            throw runtime::CrossaException(
                runtime::CrossaError::invalidJson(error.what())
            );
        }
        requestHandle.throwIfCancellationRequested();
        return decodeValue(
            value,
            expectedType,
            "$response",
            requestHandle
        );
    }

    // Converts one parsed value directly into its typed native representation.
    runtime::RuntimeValue ResponseDecoder::decodeValue(
        const json::JsonValue& value,
        const compiler::types::SemanticType& expectedType,
        const string& path,
        const runtime::RequestHandle& requestHandle
    ) const {
        using compiler::types::SemanticTypeKind;
        using json::JsonValueKind;

        requestHandle.throwIfCancellationRequested();
        switch (expectedType.getKind()) {
            case SemanticTypeKind::Unit:
                return runtime::RuntimeValue::createUnit();
            case SemanticTypeKind::Int:
                return runtime::RuntimeValue::createInt(
                    parseInteger(value, path)
                );
            case SemanticTypeKind::Long:
                return runtime::RuntimeValue::createLong(
                    parseLong(value, path)
                );
            case SemanticTypeKind::Double:
                return runtime::RuntimeValue::createDouble(
                    parseDouble(value, path)
                );
            case SemanticTypeKind::String:
                if (value.getKind() != JsonValueKind::String) {
                    throw runtime::CrossaException(
                        runtime::CrossaError::responseTypeMismatch(
                            path + " must be a JSON string."
                        )
                    );
                }
                return runtime::RuntimeValue::createString(value.getString());
            case SemanticTypeKind::Bool:
                if (value.getKind() != JsonValueKind::Boolean) {
                    throw runtime::CrossaException(
                        runtime::CrossaError::responseTypeMismatch(
                            path + " must be a JSON boolean."
                        )
                    );
                }
                return runtime::RuntimeValue::createBool(value.getBoolean());
            case SemanticTypeKind::Json:
                return runtime::RuntimeValue::createJson(value);
            case SemanticTypeKind::Model: {
                if (value.getKind() != JsonValueKind::Object) {
                    throw runtime::CrossaException(
                        runtime::CrossaError::responseTypeMismatch(
                            path + " must be model '" +
                            expectedType.getModelName() + "'."
                        )
                    );
                }
                const compiler::ir::IrModelDeclaration& model =
                    findModel(expectedType.getModelName());
                runtime::NativeModel::Fields fields;
                fields.reserve(model.getFields().size());
                for (const compiler::ir::IrModelField& field :
                     model.getFields()) {
                    const json::JsonValue* fieldValue =
                        value.find(field.getName());
                    if (fieldValue == nullptr) {
                        throw runtime::CrossaException(
                            runtime::CrossaError::responseTypeMismatch(
                                path + " is missing model field '" +
                                field.getName() + "'."
                            )
                        );
                    }
                    fields.emplace_back(
                        field.getName(),
                        decodeValue(
                            *fieldValue,
                            field.getType(),
                            path + "." + field.getName(),
                            requestHandle
                        )
                    );
                }
                return runtime::RuntimeValue::createModel(
                    runtime::NativeModel(
                        expectedType.getModelName(),
                        std::move(fields)
                    )
                );
            }
            case SemanticTypeKind::List: {
                if (value.getKind() != JsonValueKind::Array) {
                    throw runtime::CrossaException(
                        runtime::CrossaError::responseTypeMismatch(
                            path + " must be a JSON array."
                        )
                    );
                }
                const compiler::types::SemanticType* elementType =
                    expectedType.getElementType();
                if (elementType == nullptr) {
                    throw runtime::CrossaException(
                        runtime::CrossaError::runtime(
                            "List response type has no element type."
                        )
                    );
                }
                vector<runtime::RuntimeValue> values;
                values.reserve(value.getArray().size());
                for (size_t index = 0; index < value.getArray().size(); ++index) {
                    values.push_back(decodeValue(
                        value.getArray()[index],
                        *elementType,
                        path + "[" + to_string(index) + "]",
                        requestHandle
                    ));
                }
                return runtime::RuntimeValue::createList(
                    runtime::NativeList(std::move(values))
                );
            }
        }
        throw runtime::CrossaException(
            runtime::CrossaError::runtime("Unknown semantic response type.")
        );
    }

    // Locates one lowered model schema by exact name.
    const compiler::ir::IrModelDeclaration& ResponseDecoder::findModel(
        const string& name
    ) const {
        for (const unique_ptr<compiler::ir::IrDeclaration>& declaration :
             program_.getDeclarations()) {
            if (declaration->getKind() !=
                compiler::ir::IrDeclarationKind::Model) {
                continue;
            }
            const auto& model = static_cast<
                const compiler::ir::IrModelDeclaration&
            >(*declaration);
            if (model.getName() == name) {
                return model;
            }
        }
        throw runtime::CrossaException(
            runtime::CrossaError::runtime(
                "Missing IR schema for model '" + name + "'."
            )
        );
    }

    // Converts one JSON integer number into a native Int.
    int64_t ResponseDecoder::parseInteger(
        const json::JsonValue& value,
        const string& path
    ) {
        if (value.getKind() != json::JsonValueKind::Number) {
            throw runtime::CrossaException(
                runtime::CrossaError::responseTypeMismatch(
                    path + " must be a JSON integer."
                )
            );
        }
        const string& number = value.getNumber();
        int64_t result = 0;
        const auto conversion = from_chars(
            number.data(),
            number.data() + number.size(),
            result
        );
        if (conversion.ec != errc{} ||
            conversion.ptr != number.data() + number.size()) {
            throw runtime::CrossaException(
                runtime::CrossaError::responseTypeMismatch(
                    path + " must fit the Crossa Int range."
                )
            );
        }
        return result;
    }

    // Converts one JSON integer number into a native Long.
    int64_t ResponseDecoder::parseLong(
        const json::JsonValue& value,
        const string& path
    ) {
        return parseInteger(value, path);
    }

    // Converts one JSON number into a native Double.
    double ResponseDecoder::parseDouble(
        const json::JsonValue& value,
        const string& path
    ) {
        if (value.getKind() != json::JsonValueKind::Number) {
            throw runtime::CrossaException(
                runtime::CrossaError::responseTypeMismatch(
                    path + " must be a JSON number."
                )
            );
        }
        const string& number = value.getNumber();
        size_t processed = 0;
        try {
            const double result = stod(number, &processed);
            if (processed == number.size()) {
                return result;
            }
        } catch (const exception&) {
        }
        throw runtime::CrossaException(
            runtime::CrossaError::responseTypeMismatch(
                path + " must fit the Crossa Double range."
            )
        );
    }

}
