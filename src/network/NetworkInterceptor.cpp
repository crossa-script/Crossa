#include "crossa/network/NetworkInterceptor.h"

#include "crossa/network/HttpMethod.h"
#include "crossa/network/NetworkPolicy.h"
#include "crossa/network/utils/UrlUtils.h"

using namespace std;

namespace crossa::network {

    // Creates an interceptor over immutable configuration and logging.
    NetworkInterceptor::NetworkInterceptor(
        const NetworkConfiguration& configuration,
        const crossa::utils::Log& log
    ) noexcept
        : configuration_(configuration), log_(log) {}

    // Appends common headers and reports request lifecycle metadata.
    void NetworkInterceptor::beforeRequest(
        request::PreparedRequest& request
    ) const {
        for (const HttpHeader& header : configuration_.getCommonHeaders()) {
            request.setHeader(header, false);
        }
        if (request.getBody().has_value() &&
            !request.hasHeader("Content-Type")) {
            request.setHeader(
                HttpHeader("Content-Type", "application/json"),
                false
            );
        }
        if (!request.hasHeader("Accept")) {
            request.setHeader(HttpHeader("Accept", "application/json"), false);
        }
        if (!configuration_.isInterceptorEnabled()) {
            return;
        }
        if (configuration_.shouldLogRequests()) {
            const string message =
                "Network request started: method=" +
                string(HttpMethodUtils::toString(request.getMethod())) +
                " url=" + utils::UrlUtils::stripQuery(request.getUrl()) +
                " headers=" + to_string(request.getHeaders().size()) +
                " requestBytes=" + to_string(
                    request.getBody().has_value()
                        ? request.getBody()->size()
                        : 0
                );
            log_.debug(message);
            if (configuration_.shouldLogHeaders()) {
                log_.debug(
                    "Network request headers: " +
                    formatHeaders(request.getHeaders())
                );
            }
            if (configuration_.shouldLogBody() && request.getBody().has_value()) {
                log_.debug("Network request body: " + *request.getBody());
            }
            log_.debug("Network request curl: " + buildCurlCommand(request));
        }
    }

    // Reports response status and emits optional response values.
    void NetworkInterceptor::afterResponse(
        const request::PreparedRequest& request,
        const response::HttpResponse& response
    ) const {
        if (!configuration_.isInterceptorEnabled() ||
            !configuration_.shouldLogResponses()) {
            return;
        }
        const string message =
            "Network request completed: url=" +
            utils::UrlUtils::stripQuery(request.getUrl()) +
            " status=" + to_string(response.getStatusCode()) +
            " responseBytes=" + to_string(response.getBody().size());
        log_.debug(message);
        if (configuration_.shouldLogHeaders()) {
            log_.debug(
                "Network response headers: " +
                formatHeaders(response.getHeaders())
            );
        }
        if (configuration_.shouldLogBody()) {
            log_.debug("Network response body: " + response.getBody());
        }
    }

    // Reports a sanitized request failure for the global interception path.
    void NetworkInterceptor::onError(
        const request::PreparedRequest& request,
        const string& message
    ) const {
        if (!configuration_.isInterceptorEnabled()) {
            return;
        }
        log_.debug(
            "Network request failed: url=" +
            utils::UrlUtils::stripQuery(request.getUrl()) +
            " error=" + message
        );
    }

    // Formats headers while excluding configured sensitive names.
    string NetworkInterceptor::formatHeaders(
        const vector<HttpHeader>& headers
    ) const {
        string result = "[";
        bool first = true;
        for (const HttpHeader& header : headers) {
            if (isHeaderExcluded(header.getName())) {
                continue;
            }
            if (!first) {
                result += ", ";
            }
            first = false;
            result += header.getName() + "=" + header.getValue();
        }
        return result + "]";
    }

    // Builds one copyable curl command from the prepared request.
    string NetworkInterceptor::buildCurlCommand(
        const request::PreparedRequest& request
    ) const {
        string command = "curl";
        command += " -X " + escapeShellArgument(
            string(HttpMethodUtils::toString(request.getMethod()))
        );
        if (configuration_.shouldLogHeaders()) {
            for (const HttpHeader& header : request.getHeaders()) {
                if (isHeaderExcluded(header.getName())) {
                    continue;
                }
                command += " -H " + escapeShellArgument(
                    header.getName() + ": " + header.getValue()
                );
            }
        }
        if (request.getMultipart().has_value() &&
            configuration_.shouldLogBody()) {
            for (const NetworkPolicy::MultipartPart& part :
                 NetworkPolicy::parseMultipart(*request.getMultipart())) {
                string value = part.name + "=";
                if (part.filePath.has_value()) {
                    value += "@" + *part.filePath;
                } else if (part.data.has_value()) {
                    value += *part.data;
                }
                if (part.filename.has_value()) {
                    value += ";filename=" + *part.filename;
                }
                if (part.contentType.has_value()) {
                    value += ";type=" + *part.contentType;
                }
                command += " -F " + escapeShellArgument(value);
            }
        } else if (request.getBody().has_value() &&
                   configuration_.shouldLogBody()) {
            command += " --data-raw " +
                escapeShellArgument(*request.getBody());
        }
        command += " " + escapeShellArgument(
            utils::UrlUtils::stripQuery(request.getUrl())
        );
        return command;
    }

    // Returns whether one header name must be removed from logs.
    bool NetworkInterceptor::isHeaderExcluded(
        const string& name
    ) const noexcept {
        static constexpr const char* SensitiveHeaders[] = {
            "authorization",
            "proxy-authorization",
            "cookie",
            "set-cookie",
            "x-api-key",
            "x-auth-token",
            "x-access-token",
            "x-refresh-token"
        };
        for (const char* sensitiveName : SensitiveHeaders) {
            if (headerNamesEqual(name, sensitiveName)) {
                return true;
            }
        }
        for (const string& excludedName : configuration_.getExcludedLogHeaders()) {
            if (headerNamesEqual(name, excludedName)) {
                return true;
            }
        }
        return false;
    }

    // Escapes one shell argument for safe single-line curl output.
    string NetworkInterceptor::escapeShellArgument(const string& value) {
        string escaped = "'";
        for (const char character : value) {
            if (character == '\'') {
                escaped += "'\"'\"'";
                continue;
            }
            escaped.push_back(character);
        }
        escaped.push_back('\'');
        return escaped;
    }

    // Compares two header names without ASCII case sensitivity.
    bool NetworkInterceptor::headerNamesEqual(
        const string& left,
        const string& right
    ) noexcept {
        if (left.size() != right.size()) {
            return false;
        }
        for (size_t index = 0; index < left.size(); ++index) {
            const char leftValue = left[index] >= 'A' && left[index] <= 'Z'
                ? static_cast<char>(left[index] - 'A' + 'a')
                : left[index];
            const char rightValue = right[index] >= 'A' && right[index] <= 'Z'
                ? static_cast<char>(right[index] - 'A' + 'a')
                : right[index];
            if (leftValue != rightValue) {
                return false;
            }
        }
        return true;
    }

}
