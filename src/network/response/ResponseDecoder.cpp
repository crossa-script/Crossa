#include "crossa/network/response/ResponseDecoder.h"

#include <charconv>
#include <stdexcept>

#include "crossa/compiler/ir/IrDeclaration.h"
#include "crossa/network/json/JsonParser.h"

using namespace std;

namespace crossa::network::response {

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
    const compiler::types::SemanticType& expectedType
) const {
    using compiler::types::SemanticTypeKind;

    if (expectedType.getKind() == SemanticTypeKind::Unit) {
        return runtime::RuntimeValue::createUnit();
    }
    if (expectedType.getKind() == SemanticTypeKind::String) {
        return runtime::RuntimeValue::createString(body);
    }

    json::JsonValue value = json::JsonParser::parse(
        body,
        maximumBytes_,
        maximumDepth_
    );
    validate(value, expectedType, "$response");
    if (expectedType.getKind() == SemanticTypeKind::Int) {
        return runtime::RuntimeValue::createInt(
            parseInteger(value, "$response")
        );
    }
    if (expectedType.getKind() == SemanticTypeKind::Bool) {
        return runtime::RuntimeValue::createBool(value.getBoolean());
    }
    return runtime::RuntimeValue::createJson(std::move(value));
}

// Validates one parsed JSON value against a resolved semantic type.
void ResponseDecoder::validate(
    const json::JsonValue& value,
    const compiler::types::SemanticType& expectedType,
    const string& path
) const {
    using compiler::types::SemanticTypeKind;
    using json::JsonValueKind;

    switch (expectedType.getKind()) {
        case SemanticTypeKind::Unit:
            return;
        case SemanticTypeKind::Int:
            (void)parseInteger(value, path);
            return;
        case SemanticTypeKind::String:
            if (value.getKind() != JsonValueKind::String) {
                throw runtime_error(path + " must be a JSON string.");
            }
            return;
        case SemanticTypeKind::Bool:
            if (value.getKind() != JsonValueKind::Boolean) {
                throw runtime_error(path + " must be a JSON boolean.");
            }
            return;
        case SemanticTypeKind::Json:
            return;
        case SemanticTypeKind::Model: {
            if (value.getKind() != JsonValueKind::Object) {
                throw runtime_error(
                    path + " must be model '" +
                    expectedType.getModelName() + "'."
                );
            }
            const compiler::ir::IrModelDeclaration& model =
                findModel(expectedType.getModelName());
            for (const compiler::ir::IrModelField& field :
                 model.getFields()) {
                const json::JsonValue* fieldValue = value.find(field.getName());
                if (fieldValue == nullptr) {
                    throw runtime_error(
                        path + " is missing model field '" +
                        field.getName() + "'."
                    );
                }
                validate(
                    *fieldValue,
                    field.getType(),
                    path + "." + field.getName()
                );
            }
            return;
        }
        case SemanticTypeKind::List: {
            if (value.getKind() != JsonValueKind::Array) {
                throw runtime_error(path + " must be a JSON array.");
            }
            const compiler::types::SemanticType* elementType =
                expectedType.getElementType();
            if (elementType == nullptr) {
                throw runtime_error("List response type has no element type.");
            }
            for (size_t index = 0; index < value.getArray().size(); ++index) {
                validate(
                    value.getArray()[index],
                    *elementType,
                    path + "[" + to_string(index) + "]"
                );
            }
            return;
        }
    }
    throw runtime_error("Unknown semantic response type.");
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
    throw runtime_error("Missing IR schema for model '" + name + "'.");
}

// Converts one JSON integer number into a native Int.
int64_t ResponseDecoder::parseInteger(
    const json::JsonValue& value,
    const string& path
) {
    if (value.getKind() != json::JsonValueKind::Number) {
        throw runtime_error(path + " must be a JSON integer.");
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
        throw runtime_error(path + " must fit the Crossa Int range.");
    }
    return result;
}

}
