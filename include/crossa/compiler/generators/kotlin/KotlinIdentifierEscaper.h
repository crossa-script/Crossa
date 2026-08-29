#pragma once

#include <string>

namespace crossa::compiler::generators::kotlin {

// Escapes Crossa identifiers that would otherwise collide with Kotlin keywords.
class KotlinIdentifierEscaper final {
public:
    // Returns a Kotlin declaration or reference identifier for one Crossa name.
    [[nodiscard]] static std::string escape(const std::string& identifier);

private:
    // Returns whether one Crossa identifier is reserved by Kotlin.
    [[nodiscard]] static bool isKeyword(const std::string& identifier) noexcept;
};

}
