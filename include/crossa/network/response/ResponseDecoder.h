#pragma once

#include <cstddef>
#include <string>

#include "crossa/compiler/ir/Program.h"
#include "crossa/compiler/types/SemanticType.h"
#include "crossa/network/json/JsonValue.h"
#include "crossa/runtime/RequestHandle.h"
#include "crossa/runtime/RuntimeValue.h"

namespace crossa::network::response {

// Decodes bounded HTTP response bytes into the request's semantic result type.
// decode() validates model and list shapes using the immutable Crossa IR schema.
class ResponseDecoder final {
public:
    // Creates a schema-aware decoder with explicit input and depth limits.
    ResponseDecoder(
        const compiler::ir::Program& program,
        std::size_t maximumBytes,
        std::size_t maximumDepth
    ) noexcept;

    // Decodes one successful response body into a native runtime value.
    [[nodiscard]] runtime::RuntimeValue decode(
        const std::string& body,
        const compiler::types::SemanticType& expectedType,
        const runtime::RequestHandle& requestHandle
    ) const;

private:
    // Converts one parsed value directly into its typed native representation.
    [[nodiscard]] runtime::RuntimeValue decodeValue(
        const json::JsonValue& value,
        const compiler::types::SemanticType& expectedType,
        const std::string& path,
        const runtime::RequestHandle& requestHandle
    ) const;

    // Locates one lowered model schema by exact name.
    [[nodiscard]] const compiler::ir::IrModelDeclaration& findModel(
        const std::string& name
    ) const;

    // Converts one JSON integer number into a native Int.
    [[nodiscard]] static std::int64_t parseInteger(
        const json::JsonValue& value,
        const std::string& path
    );

    const compiler::ir::Program& program_;
    std::size_t maximumBytes_;
    std::size_t maximumDepth_;
};

}
