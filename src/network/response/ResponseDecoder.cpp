#include "crossa/network/response/ResponseDecoder.h"

#include <charconv>
#include <exception>

#include "crossa/compiler/ir/IrDeclaration.h"
#include "crossa/network/json/JsonParser.h"
#include "crossa/runtime/errors/CrossaException.h"
#include "crossa/runtime/objects/NativeList.h"
#include "crossa/runtime/objects/NativeModel.h"

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
        const compiler::types::SemanticType& expectedType,
        const runtime::RequestHandle& requestHandle
    ) const {
        using compiler::types::SemanticTypeKind;

        requestHandle.throwIfCancellationRequested();
        if (expectedType.getKind() == SemanticTypeKind::Unit) {
            return runtime::RuntimeValue::createUnit();
        }
        if (expectedType.getKind() == SemanticTypeKind::String) {
            return runtime::RuntimeValue::createString(body);
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

}
