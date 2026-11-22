#include "crypto.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace hc
{
    namespace
    {
        const int kIterations = 200000;

        std::string toHex(const std::vector<unsigned char> &data)
        {
            std::ostringstream result;
            for (unsigned char byte : data)
                result << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
            return result.str();
        }

        std::vector<unsigned char> fromHex(const std::string &hex)
        {
            if (hex.empty() || hex.size() % 2 != 0)
                throw std::runtime_error("Invalid hexadecimal data");
            std::vector<unsigned char> result;
            for (std::size_t i = 0; i < hex.size(); i += 2)
                result.push_back(static_cast<unsigned char>(std::stoul(hex.substr(i, 2), nullptr, 16)));
            return result;
        }

        // PBKDF2-HMAC-SHA256 into `out`. Returns false on failure.
        bool pbkdf2(const std::string &password, const std::vector<unsigned char> &salt,
                    unsigned long iterations, std::vector<unsigned char> &out)
        {
#ifdef _WIN32
            BCRYPT_ALG_HANDLE algorithm = nullptr;
            NTSTATUS status = BCryptOpenAlgorithmProvider(
                &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
            if (status < 0)
                return false;

            status = BCryptDeriveKeyPBKDF2(
                algorithm,
                reinterpret_cast<PUCHAR>(const_cast<char *>(password.data())),
                static_cast<ULONG>(password.size()),
                const_cast<PUCHAR>(salt.data()),
                static_cast<ULONG>(salt.size()),
                static_cast<ULONGLONG>(iterations),
                out.data(),
                static_cast<ULONG>(out.size()),
                0);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return status >= 0;
#else
            return PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                                     salt.data(), static_cast<int>(salt.size()),
                                     static_cast<int>(iterations), EVP_sha256(),
                                     static_cast<int>(out.size()), out.data()) == 1;
#endif
        }
    }

    std::vector<unsigned char> secureRandomBytes(std::size_t size)
    {
        std::vector<unsigned char> data(size);
#ifdef _WIN32
        NTSTATUS status = BCryptGenRandom(nullptr, data.data(), static_cast<ULONG>(data.size()),
                                          BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status < 0)
            throw std::runtime_error("Secure random generation failed");
#else
        if (RAND_bytes(data.data(), static_cast<int>(size)) != 1)
            throw std::runtime_error("Secure random generation failed");
#endif
        return data;
    }

    std::string generateSessionToken()
    {
        return toHex(secureRandomBytes(32));
    }

    std::string sha256Hex(const std::string &data)
    {
        std::vector<unsigned char> digest(32);
#ifdef _WIN32
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            throw std::runtime_error("SHA-256 unavailable");
        NTSTATUS status = BCryptHash(algorithm, nullptr, 0,
                                     reinterpret_cast<PUCHAR>(const_cast<char *>(data.data())),
                                     static_cast<ULONG>(data.size()), digest.data(), 32);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (status < 0)
            throw std::runtime_error("SHA-256 failed");
#else
        unsigned int length = 0;
        if (EVP_Digest(data.data(), data.size(), digest.data(), &length, EVP_sha256(), nullptr) != 1 || length != 32)
            throw std::runtime_error("SHA-256 failed");
#endif
        return toHex(digest);
    }

    std::string hashPassword(const std::string &password)
    {
        auto salt = secureRandomBytes(16);
        std::vector<unsigned char> hash(32);
        if (!pbkdf2(password, salt, kIterations, hash))
            throw std::runtime_error("Password hashing failed");
        return "pbkdf2_sha256$" + std::to_string(kIterations) + "$" + toHex(salt) + "$" + toHex(hash);
    }

    bool verifyPassword(const std::string &password, const std::string &storedHash)
    {
        try
        {
            std::stringstream ss(storedHash);
            std::string algorithm, iterationsText, saltHex, hashHex;
            if (!std::getline(ss, algorithm, '$') || !std::getline(ss, iterationsText, '$') ||
                !std::getline(ss, saltHex, '$') || !std::getline(ss, hashHex, '$'))
                return false;
            if (algorithm != "pbkdf2_sha256")
                return false;

            std::size_t position = 0;
            unsigned long iterations = std::stoul(iterationsText, &position);
            if (position != iterationsText.size() || iterations < 100000 || iterations > 1000000)
                return false;

            auto salt = fromHex(saltHex);
            auto expected = fromHex(hashHex);
            if (salt.size() != 16 || expected.size() != 32)
                return false;

            std::vector<unsigned char> actual(32);
            if (!pbkdf2(password, salt, iterations, actual))
                return false;

            unsigned char difference = 0; // constant-time comparison
            for (std::size_t i = 0; i < actual.size(); ++i)
                difference |= static_cast<unsigned char>(actual[i] ^ expected[i]);
            return difference == 0;
        }
        catch (...)
        {
            return false;
        }
    }

    void dummyVerifyPassword(const std::string &password)
    {
        static const std::vector<unsigned char> salt(16, 0x5a);
        std::vector<unsigned char> out(32);
        pbkdf2(password, salt, kIterations, out);
    }
}
