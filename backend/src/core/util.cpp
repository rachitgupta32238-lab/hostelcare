#include "util.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <mutex>
#include <sstream>

namespace hc
{
    std::string trim(const std::string &value)
    {
        std::size_t start = 0;
        std::size_t end = value.size();
        while (start < end && std::isspace(static_cast<unsigned char>(value[start])))
            ++start;
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
            --end;
        return value.substr(start, end - start);
    }

    std::string toLower(std::string value)
    {
        for (char &ch : value)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return value;
    }

    bool iequals(const std::string &a, const std::string &b)
    {
        return toLower(trim(a)) == toLower(trim(b));
    }

    // True if `word` appears in `lowerText` as a whole word (not inside another
    // word). Avoids bugs like "it" matching "kitchen".
    bool containsWord(const std::string &lowerText, const std::string &word)
    {
        std::size_t pos = lowerText.find(word);
        while (pos != std::string::npos)
        {
            bool startOk = pos == 0 ||
                           !std::isalnum(static_cast<unsigned char>(lowerText[pos - 1]));
            std::size_t after = pos + word.size();
            bool endOk = after >= lowerText.size() ||
                         !std::isalnum(static_cast<unsigned char>(lowerText[after]));
            if (startOk && endOk)
                return true;
            pos = lowerText.find(word, pos + 1);
        }
        return false;
    }

    std::vector<std::string> splitPipe(const std::string &line, std::size_t minFields)
    {
        std::vector<std::string> fields;
        std::string current;
        for (char ch : line)
        {
            if (ch == '|')
            {
                fields.push_back(current);
                current.clear();
            }
            else
            {
                current.push_back(ch);
            }
        }
        fields.push_back(current); // keeps a trailing empty field, unlike getline
        while (fields.size() < minFields)
            fields.emplace_back();
        return fields;
    }

    std::string joinPipe(const std::vector<std::string> &fields)
    {
        std::string line;
        for (std::size_t i = 0; i < fields.size(); ++i)
        {
            if (i > 0)
                line += '|';
            line += fields[i];
        }
        return line;
    }

    void stripCarriageReturn(std::string &line)
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
    }

    std::string sanitizeField(const std::string &value)
    {
        std::string clean = value;
        for (char &ch : clean)
        {
            if (ch == '|' || ch == '\n' || ch == '\r' || ch == '\t')
                ch = ' ';
        }
        return trim(clean);
    }

    std::string requireText(const std::string &value, const std::string &label,
                            std::size_t maxLength, std::size_t minLength)
    {
        std::string clean = sanitizeField(value);
        if (clean.size() < minLength)
            throw ApiError(400, label + " is required");
        if (clean.size() > maxLength)
            throw ApiError(400, label + " must be at most " +
                                    std::to_string(maxLength) + " characters");
        return clean;
    }

    std::string requireId(const std::string &value, const std::string &label)
    {
        std::string id = trim(value);
        if (id.empty())
            throw ApiError(400, label + " is required");
        if (id.size() > 20)
            throw ApiError(400, label + " must be at most 20 characters");
        for (char ch : id)
        {
            bool ok = std::isalnum(static_cast<unsigned char>(ch)) ||
                      ch == '-' || ch == '_';
            if (!ok)
                throw ApiError(400, label + " may contain only letters, digits, - and _");
        }
        return id;
    }

    std::string requirePhone(const std::string &value)
    {
        std::string phone = trim(value);
        if (phone.size() != 10 ||
            !std::all_of(phone.begin(), phone.end(),
                         [](char ch)
                         { return std::isdigit(static_cast<unsigned char>(ch)) != 0; }))
            throw ApiError(400, "Phone number must be exactly 10 digits");
        return phone;
    }

    void requirePasswordStrength(const std::string &password, std::size_t minLength)
    {
        if (password.size() < minLength)
            throw ApiError(400, "Password must be at least " +
                                    std::to_string(minLength) + " characters");
        if (password.size() > 128)
            throw ApiError(400, "Password must be at most 128 characters");

        bool hasLetter = false, hasDigit = false;
        for (char ch : password)
        {
            if (std::isalpha(static_cast<unsigned char>(ch)))
                hasLetter = true;
            if (std::isdigit(static_cast<unsigned char>(ch)))
                hasDigit = true;
        }
        if (!hasLetter || !hasDigit)
            throw ApiError(400, "Password must contain at least one letter and one digit");
    }

    std::tm localTimeOf(std::time_t value)
    {
        // std::localtime returns a shared buffer; copy it under a lock.
        // (Portable: avoids localtime_r / localtime_s differences.)
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        std::tm result{};
        if (std::tm *tm = std::localtime(&value))
            result = *tm;
        return result;
    }

    std::string nowTimestamp()
    {
        std::tm tm = localTimeOf(std::time(nullptr));
        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        return buffer;
    }

    std::time_t parseTimestamp(const std::string &text)
    {
        std::tm tm{};
        std::istringstream input(trim(text));
        int year, month, day, hour, minute, second;
        char d1, d2, sp, c1, c2;
        if (!(input >> year >> d1 >> month >> d2 >> day >> std::noskipws >> sp >>
              std::skipws >> hour >> c1 >> minute >> c2 >> second))
            return -1;
        if (d1 != '-' || d2 != '-' || c1 != ':' || c2 != ':')
            return -1;
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = -1;
        return std::mktime(&tm);
    }

    std::time_t nowEpoch()
    {
        return std::time(nullptr);
    }
}
