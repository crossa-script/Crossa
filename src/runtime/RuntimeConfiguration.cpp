#include "crossa/runtime/RuntimeConfiguration.h"

#include <charconv>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "crossa/compiler/ir/IrDeclaration.h"
#include "crossa/compiler/ir/IrExpression.h"
#include "crossa/compiler/ir/IrJsonExpression.h"
#include "crossa/network/HttpHeader.h"
#include "crossa/network/NetworkPolicy.h"

using namespace std;

namespace crossa::runtime {

// Reads compile-time config constants and applies their native policies.
class RuntimeConfigurationLoader final {
public:
    // Loads at most one config block from the supplied IR programs.
    static RuntimeConfiguration load(
        const compiler::ir::Program& program,
        const compiler::ir::Program* configurationProgram,
        const network::json::JsonValue* runtimeOverrides
    ) {
        network::NetworkConfiguration networkConfiguration;
        scheduler::SchedulerOptions defaults =
            scheduler::SchedulerOptions::createDefault();
        optional<size_t> workerCount;
        optional<size_t> maximumQueuedTasks;
        unordered_set<string> appliedKeys;
        size_t configBlocks = 0;

        if (configurationProgram != nullptr) {
            applyProgram(
                *configurationProgram,
                networkConfiguration,
                workerCount,
                maximumQueuedTasks,
                appliedKeys,
                configBlocks
            );
        }
        applyProgram(
            program,
            networkConfiguration,
            workerCount,
            maximumQueuedTasks,
            appliedKeys,
            configBlocks
        );
        if (runtimeOverrides != nullptr) {
            applyRuntimeOverrides(
                *runtimeOverrides,
                networkConfiguration,
                workerCount,
                maximumQueuedTasks
            );
        }
        if (configBlocks > 1) {
            throw runtime_error(
                "Runtime configuration failed: only one config block is allowed."
            );
        }

        (void)network::NetworkPolicy::parseRetry(
            networkConfiguration.getRetryPolicy()
        );
        const vector<network::NetworkPolicy::AuthProvider> authProviders =
            network::NetworkPolicy::parseAuthProviders(
            networkConfiguration.getAuthProviders()
        );
        if (!networkConfiguration.getDefaultAuthProvider().empty()) {
            bool found = false;
            for (const network::NetworkPolicy::AuthProvider& provider :
                 authProviders) {
                if (provider.name ==
                    networkConfiguration.getDefaultAuthProvider()) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw runtime_error(
                    "Runtime configuration defaultAuthProvider was not declared."
                );
            }
        }
        (void)network::NetworkPolicy::parseProxy(
            networkConfiguration.getProxy()
        );
        (void)network::NetworkPolicy::parseCertificate(
            networkConfiguration.getCertificatePolicy()
        );
        (void)network::NetworkPolicy::parseTelemetry(
            networkConfiguration.getTelemetry()
        );

        return RuntimeConfiguration(
            std::move(networkConfiguration),
            scheduler::SchedulerOptions(
                workerCount.value_or(defaults.getWorkerCount()),
                maximumQueuedTasks.value_or(
                    defaults.getMaximumQueuedTasks()
                )
            )
        );
    }

private:
    // Applies one validated platform override object after compiled defaults.
    static void applyRuntimeOverrides(
        const network::json::JsonValue& overrides,
        network::NetworkConfiguration& networkConfiguration,
        optional<size_t>& workerCount,
        optional<size_t>& maximumQueuedTasks
    ) {
        if (overrides.getKind() != network::json::JsonValueKind::Object) {
            throw runtime_error("Runtime overrides must be a JSON object.");
        }
        for (const auto& [name, value] : overrides.getObject()) {
            if (name == "baseUrl") {
                networkConfiguration.setBaseUrl(readOverrideString(value, name));
            } else if (name == "timeoutRequest") {
                networkConfiguration.setTimeoutMilliseconds(readOverrideInt(value, name));
            } else if (name == "commonHeaders") {
                if (value.getKind() != network::json::JsonValueKind::Array) {
                    throw runtime_error("Runtime override commonHeaders must be an array.");
                }
                vector<network::HttpHeader> headers;
                headers.reserve(value.getArray().size());
                for (const network::json::JsonValue& header : value.getArray()) {
                    if (header.getKind() != network::json::JsonValueKind::Object) {
                        throw runtime_error("Runtime override headers must be objects.");
                    }
                    const network::json::JsonValue* headerName = header.find("name");
                    const network::json::JsonValue* headerValue = header.find("value");
                    if (headerName == nullptr || headerValue == nullptr) {
                        throw runtime_error("Runtime override headers require name and value.");
                    }
                    headers.emplace_back(
                        readOverrideString(*headerName, "header.name"),
                        readOverrideString(*headerValue, "header.value")
                    );
                }
                networkConfiguration.setCommonHeaders(std::move(headers));
            } else if (name == "interceptor") {
                applyRuntimeInterceptor(value, networkConfiguration);
            } else if (name == "workerThreads") {
                workerCount = readOverrideSize(value, name);
            } else if (name == "maxQueuedTasks") {
                maximumQueuedTasks = readOverrideSize(value, name);
            } else if (name == "maxResponseBytes") {
                networkConfiguration.setMaximumResponseBytes(readOverrideSize(value, name));
            } else if (name == "maxJsonDepth") {
                networkConfiguration.setMaximumJsonDepth(readOverrideSize(value, name));
            } else if (name == "followRedirects") {
                networkConfiguration.setFollowRedirects(readOverrideBool(value, name));
            } else if (name == "uploadProgress") {
                networkConfiguration.setUploadProgress(readOverrideBool(value, name));
            } else if (name == "downloadStreaming") {
                networkConfiguration.setDownloadStreaming(readOverrideBool(value, name));
            } else if (name == "requestCoalescing") {
                networkConfiguration.setRequestCoalescing(readOverrideBool(value, name));
            } else {
                throw runtime_error("Unknown runtime override key '" + name + "'.");
            }
        }
    }

    // Applies the typed interceptor members supplied by the Android wrapper.
    static void applyRuntimeInterceptor(
        const network::json::JsonValue& value,
        network::NetworkConfiguration& configuration
    ) {
        if (value.getKind() != network::json::JsonValueKind::Object) {
            throw runtime_error("Runtime override interceptor must be an object.");
        }
        for (const auto& [name, member] : value.getObject()) {
            if (name == "excludedLogHeaders") {
                if (member.getKind() != network::json::JsonValueKind::Array) {
                    throw runtime_error("Runtime override excludedLogHeaders must be an array.");
                }
                vector<string> headers;
                headers.reserve(member.getArray().size());
                for (const network::json::JsonValue& header : member.getArray()) {
                    headers.push_back(readOverrideString(header, "excludedLogHeaders"));
                }
                configuration.setExcludedLogHeaders(std::move(headers));
            } else {
                const bool enabled = readOverrideBool(member, name);
                if (name == "enabled") configuration.setInterceptorEnabled(enabled);
                else if (name == "logRequests") configuration.setLogRequests(enabled);
                else if (name == "logResponses") configuration.setLogResponses(enabled);
                else if (name == "logHeaders") configuration.setLogHeaders(enabled);
                else if (name == "logBody") configuration.setLogBody(enabled);
                else throw runtime_error("Unknown runtime interceptor override '" + name + "'.");
            }
        }
    }

    // Reads one platform override string with a deterministic type diagnostic.
    static string readOverrideString(
        const network::json::JsonValue& value,
        const string& name
    ) {
        if (value.getKind() != network::json::JsonValueKind::String) {
            throw runtime_error("Runtime override '" + name + "' must be a String.");
        }
        return value.getString();
    }

    // Reads one platform override integer with checked native conversion.
    static int64_t readOverrideInt(
        const network::json::JsonValue& value,
        const string& name
    ) {
        if (value.getKind() != network::json::JsonValueKind::Number) {
            throw runtime_error("Runtime override '" + name + "' must be an Int.");
        }
        int64_t result = 0;
        const string& number = value.getNumber();
        const auto parsed = from_chars(number.data(), number.data() + number.size(), result);
        if (parsed.ec != errc{} || parsed.ptr != number.data() + number.size()) {
            throw runtime_error("Runtime override '" + name + "' is outside the native range.");
        }
        return result;
    }

    // Reads one positive platform override size through the native policy.
    static size_t readOverrideSize(
        const network::json::JsonValue& value,
        const string& name
    ) {
        const int64_t result = readOverrideInt(value, name);
        if (result <= 0) throw runtime_error("Runtime override '" + name + "' must be positive.");
        return static_cast<size_t>(result);
    }

    // Reads one platform override Boolean with a deterministic type diagnostic.
    static bool readOverrideBool(
        const network::json::JsonValue& value,
        const string& name
    ) {
        if (value.getKind() != network::json::JsonValueKind::Boolean) {
            throw runtime_error("Runtime override '" + name + "' must be a Bool.");
        }
        return value.getBoolean();
    }

    // Applies every config declaration found in one IR program.
    static void applyProgram(
        const compiler::ir::Program& program,
        network::NetworkConfiguration& networkConfiguration,
        optional<size_t>& workerCount,
        optional<size_t>& maximumQueuedTasks,
        unordered_set<string>& appliedKeys,
        size_t& configBlocks
    ) {
        for (const unique_ptr<compiler::ir::IrDeclaration>& declaration :
             program.getDeclarations()) {
            if (declaration->getKind() !=
                compiler::ir::IrDeclarationKind::Config) {
                continue;
            }
            ++configBlocks;
            const auto& config = static_cast<
                const compiler::ir::IrConfigDeclaration&
            >(*declaration);
            for (const compiler::ir::IrConfigEntry& entry :
                 config.getEntries()) {
                if (!appliedKeys.insert(entry.getName()).second) {
                    throw runtime_error(
                        "Runtime configuration contains duplicate key '" +
                        entry.getName() + "'."
                    );
                }
                applyEntry(
                    entry,
                    networkConfiguration,
                    workerCount,
                    maximumQueuedTasks
                );
            }
        }
    }

    // Applies one typed config entry to networking or scheduler options.
    static void applyEntry(
        const compiler::ir::IrConfigEntry& entry,
        network::NetworkConfiguration& networkConfiguration,
        optional<size_t>& workerCount,
        optional<size_t>& maximumQueuedTasks
    ) {
        const string& name = entry.getName();
        if (name == "baseUrl") {
            networkConfiguration.setBaseUrl(readString(entry.getValue()));
        } else if (name == "timeoutRequest") {
            networkConfiguration.setTimeoutMilliseconds(
                readInt(entry.getValue())
            );
        } else if (name == "interceptor") {
            applyInterceptor(entry.getValue(), networkConfiguration);
        } else if (name == "commonHeaders") {
            networkConfiguration.setCommonHeaders(
                readHeaders(entry.getValue())
            );
        } else if (name == "workerThreads") {
            workerCount = readSize(entry.getValue(), name);
        } else if (name == "maxQueuedTasks") {
            maximumQueuedTasks = readSize(entry.getValue(), name);
        } else if (name == "maxResponseBytes") {
            networkConfiguration.setMaximumResponseBytes(
                readSize(entry.getValue(), name)
            );
        } else if (name == "maxJsonDepth") {
            networkConfiguration.setMaximumJsonDepth(
                readSize(entry.getValue(), name)
            );
        } else if (name == "maxResponseHeaderBytes") {
            networkConfiguration.setMaximumResponseHeaderBytes(
                readSize(entry.getValue(), name)
            );
        } else if (name == "maxResponseHeaderCount") {
            networkConfiguration.setMaximumResponseHeaderCount(
                readSize(entry.getValue(), name)
            );
        } else if (name == "followRedirects") {
            networkConfiguration.setFollowRedirects(
                readBool(entry.getValue())
            );
        } else if (name == "retryPolicy") {
            networkConfiguration.setRetryPolicy(readJson(entry.getValue()));
        } else if (name == "authProviders") {
            networkConfiguration.setAuthProviders(readJson(entry.getValue()));
        } else if (name == "defaultAuthProvider") {
            networkConfiguration.setDefaultAuthProvider(
                readString(entry.getValue())
            );
        } else if (name == "uploadProgress") {
            networkConfiguration.setUploadProgress(readBool(entry.getValue()));
        } else if (name == "downloadStreaming") {
            networkConfiguration.setDownloadStreaming(
                readBool(entry.getValue())
            );
        } else if (name == "requestCoalescing") {
            networkConfiguration.setRequestCoalescing(
                readBool(entry.getValue())
            );
        } else if (name == "proxy") {
            networkConfiguration.setProxy(readJson(entry.getValue()));
        } else if (name == "certificatePolicy") {
            networkConfiguration.setCertificatePolicy(
                readJson(entry.getValue())
            );
        } else if (name == "telemetry") {
            networkConfiguration.setTelemetry(readJson(entry.getValue()));
        }
    }

    // Reads one compile-time string expression without runtime symbols.
    static string readString(const compiler::ir::IrExpression& expression) {
        if (expression.getKind() !=
            compiler::ir::IrExpressionKind::StringBuild) {
            throw runtime_error("Config value must be a String literal.");
        }
        const auto& stringBuild = static_cast<
            const compiler::ir::IrStringBuildExpression&
        >(expression);
        string value;
        for (const compiler::ir::IrStringSegment& segment :
             stringBuild.getSegments()) {
            if (segment.getKind() !=
                compiler::ir::IrStringSegmentKind::Literal) {
                throw runtime_error(
                    "Config String values cannot interpolate symbols."
                );
            }
            value += segment.getValue();
        }
        return value;
    }

    // Reads one positive bounded size from an integer config value.
    static size_t readSize(
        const compiler::ir::IrExpression& expression,
        const string& name
    ) {
        const int64_t value = readInt(expression);
        if (value <= 0) {
            throw runtime_error(
                "Config key '" + name + "' must be positive."
            );
        }
        return static_cast<size_t>(value);
    }

    // Reads one signed native integer from a config expression.
    static int64_t readInt(const compiler::ir::IrExpression& expression) {
        if (expression.getKind() !=
            compiler::ir::IrExpressionKind::IntegerConstant) {
            throw runtime_error("Config value must be an Int literal.");
        }
        const string& digits = static_cast<
            const compiler::ir::IrIntegerConstantExpression&
        >(expression).getValue();
        int64_t value = 0;
        const auto result = from_chars(
            digits.data(),
            digits.data() + digits.size(),
            value
        );
        if (result.ec != errc{} ||
            result.ptr != digits.data() + digits.size()) {
            throw runtime_error("Config Int value is outside the native range.");
        }
        return value;
    }

    // Reads one Boolean config expression.
    static bool readBool(const compiler::ir::IrExpression& expression) {
        if (expression.getKind() !=
            compiler::ir::IrExpressionKind::BooleanConstant) {
            throw runtime_error("Config value must be a Bool literal.");
        }
        return static_cast<
            const compiler::ir::IrBooleanConstantExpression&
        >(expression).getValue();
    }

    // Converts one compile-time JSON-compatible expression into native JSON.
    static network::json::JsonValue readJson(
        const compiler::ir::IrExpression& expression
    ) {
        switch (expression.getKind()) {
            case compiler::ir::IrExpressionKind::IntegerConstant:
                return network::json::JsonValue::createNumber(
                    static_cast<
                        const compiler::ir::IrIntegerConstantExpression&
                    >(expression).getValue()
                );
            case compiler::ir::IrExpressionKind::DoubleConstant:
                return network::json::JsonValue::createNumber(
                    static_cast<
                        const compiler::ir::IrDoubleConstantExpression&
                    >(expression).getValue()
                );
            case compiler::ir::IrExpressionKind::StringBuild:
                return network::json::JsonValue::createString(
                    readString(expression)
                );
            case compiler::ir::IrExpressionKind::BooleanConstant:
                return network::json::JsonValue::createBoolean(
                    readBool(expression)
                );
            case compiler::ir::IrExpressionKind::JsonNumber:
                return network::json::JsonValue::createNumber(
                    static_cast<
                        const compiler::ir::IrJsonNumberExpression&
                    >(expression).getValue()
                );
            case compiler::ir::IrExpressionKind::JsonNull:
                return network::json::JsonValue::createNull();
            case compiler::ir::IrExpressionKind::JsonObject: {
                const auto& object = static_cast<
                    const compiler::ir::IrJsonObjectExpression&
                >(expression);
                network::json::JsonValue::Object values;
                values.reserve(object.getEntries().size());
                for (const compiler::ir::IrJsonObjectEntry& entry :
                     object.getEntries()) {
                    values.emplace_back(
                        entry.getKey(),
                        readJson(entry.getValue())
                    );
                }
                return network::json::JsonValue::createObject(
                    std::move(values)
                );
            }
            case compiler::ir::IrExpressionKind::JsonArray: {
                const auto& array = static_cast<
                    const compiler::ir::IrJsonArrayExpression&
                >(expression);
                network::json::JsonValue::Array values;
                values.reserve(array.getValues().size());
                for (const unique_ptr<compiler::ir::IrExpression>& value :
                     array.getValues()) {
                    values.push_back(readJson(*value));
                }
                return network::json::JsonValue::createArray(std::move(values));
            }
            default:
                throw runtime_error(
                    "Config JSON values must be compile-time constants."
                );
        }
    }

    // Converts a JSON object into validated common HTTP headers.
    static vector<network::HttpHeader> readHeaders(
        const compiler::ir::IrExpression& expression
    ) {
        const network::json::JsonValue object = readJson(expression);
        if (object.getKind() != network::json::JsonValueKind::Object) {
            throw runtime_error("commonHeaders must be a JSON object.");
        }
        vector<network::HttpHeader> headers;
        headers.reserve(object.getObject().size());
        for (const auto& [name, value] : object.getObject()) {
            headers.emplace_back(name, scalarToString(value));
        }
        return headers;
    }

    // Applies Boolean or object interceptor configuration.
    static void applyInterceptor(
        const compiler::ir::IrExpression& expression,
        network::NetworkConfiguration& configuration
    ) {
        if (expression.getKind() ==
            compiler::ir::IrExpressionKind::BooleanConstant) {
            configuration.setInterceptorEnabled(readBool(expression));
            return;
        }
        const network::json::JsonValue object = readJson(expression);
        if (object.getKind() != network::json::JsonValueKind::Object) {
            throw runtime_error("interceptor must be Bool or a JSON object.");
        }
        for (const auto& [name, value] : object.getObject()) {
            if (name == "excludedHeaders") {
                if (value.getKind() != network::json::JsonValueKind::Array) {
                    throw runtime_error(
                        "Interceptor excludedHeaders must be an array."
                    );
                }
                vector<string> excludedHeaders;
                excludedHeaders.reserve(value.getArray().size());
                for (const network::json::JsonValue& header :
                     value.getArray()) {
                    if (header.getKind() !=
                        network::json::JsonValueKind::String) {
                        throw runtime_error(
                            "Interceptor excludedHeaders values must be String."
                        );
                    }
                    excludedHeaders.push_back(header.getString());
                }
                configuration.setExcludedLogHeaders(
                    std::move(excludedHeaders)
                );
                continue;
            }
            if (value.getKind() != network::json::JsonValueKind::Boolean) {
                throw runtime_error(
                    "Interceptor option '" + name + "' must be Bool."
                );
            }
            if (name == "enabled") {
                configuration.setInterceptorEnabled(value.getBoolean());
            } else if (name == "logRequests") {
                configuration.setLogRequests(value.getBoolean());
            } else if (name == "logResponses") {
                configuration.setLogResponses(value.getBoolean());
            } else if (name == "logHeaders") {
                configuration.setLogHeaders(value.getBoolean());
            } else if (name == "logBody") {
                configuration.setLogBody(value.getBoolean());
            } else {
                throw runtime_error(
                    "Unknown interceptor option '" + name + "'."
                );
            }
        }
    }

    // Converts one scalar JSON value into header text.
    static string scalarToString(const network::json::JsonValue& value) {
        switch (value.getKind()) {
            case network::json::JsonValueKind::Boolean:
                return value.getBoolean() ? "true" : "false";
            case network::json::JsonValueKind::Number:
                return value.getNumber();
            case network::json::JsonValueKind::String:
                return value.getString();
            case network::json::JsonValueKind::Null:
            case network::json::JsonValueKind::Array:
            case network::json::JsonValueKind::Object:
                throw runtime_error("HTTP header values must be scalar.");
        }
        throw runtime_error("Unknown JSON value in HTTP header.");
    }
};

// Loads defaults and applies the optional main or sibling config block.
RuntimeConfiguration RuntimeConfiguration::load(
    const compiler::ir::Program& program,
    const compiler::ir::Program* configurationProgram,
    const network::json::JsonValue* runtimeOverrides
) {
    return RuntimeConfigurationLoader::load(program, configurationProgram, runtimeOverrides);
}

// Returns immutable native networking configuration.
const network::NetworkConfiguration&
RuntimeConfiguration::getNetworkConfiguration() const noexcept {
    return networkConfiguration_;
}

// Returns immutable bounded scheduler configuration.
const scheduler::SchedulerOptions&
RuntimeConfiguration::getSchedulerOptions() const noexcept {
    return schedulerOptions_;
}

// Creates one complete runtime configuration.
RuntimeConfiguration::RuntimeConfiguration(
    network::NetworkConfiguration networkConfiguration,
    scheduler::SchedulerOptions schedulerOptions
)
    : networkConfiguration_(std::move(networkConfiguration)),
      schedulerOptions_(std::move(schedulerOptions)) {}

}
