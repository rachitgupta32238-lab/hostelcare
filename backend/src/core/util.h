#pragma once
// Small, dependency-free helpers shared by the whole backend.

#include <cstddef>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

namespace hc
{
    // Thrown by the service layer; the HTTP layer turns it into a JSON error
    // response with the given status code.
    struct ApiError : std::runtime_error
    {
        int status;
        std::string code; // optional machine-readable code for the frontend
        ApiError(int httpStatus, const std::string &message, std::string errorCode = "")
            : std::runtime_error(message), status(httpStatus), code(std::move(errorCode)) {}
    };

    std::string trim(const std::string &value);
    std::string toLower(std::string value);
    bool iequals(const std::string &a, const std::string &b);
    bool containsWord(const std::string &lowerText, const std::string &word);

    // Pipe-delimited record helpers. splitPipe always returns at least
    // minFields entries so that missing trailing fields read as "".
    std::vector<std::string> splitPipe(const std::string &line, std::size_t minFields = 0);
    std::string joinPipe(const std::vector<std::string> &fields);
    void stripCarriageReturn(std::string &line);

    // Replace characters that would corrupt a pipe-delimited record.
    std::string sanitizeField(const std::string &value);

    // Validation helpers. Each throws ApiError(400, ...) with a readable message.
    std::string requireText(const std::string &value, const std::string &label,
                            std::size_t maxLength, std::size_t minLength = 1);
    std::string requireId(const std::string &value, const std::string &label);
    std::string requirePhone(const std::string &value);
    void requirePasswordStrength(const std::string &password, std::size_t minLength);

    // Timestamps use the local time format already present in status_history.txt.
    std::string nowTimestamp();
    std::time_t parseTimestamp(const std::string &text); // returns -1 if invalid
    std::time_t nowEpoch();
    std::tm localTimeOf(std::time_t value); // thread-safe local time
}
