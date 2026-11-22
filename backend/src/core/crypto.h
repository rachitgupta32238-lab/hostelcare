#pragma once
// Password hashing and secure tokens.
// Format: pbkdf2_sha256$<iterations>$<salt hex>$<hash hex>
// Windows uses BCrypt (bcrypt.lib); other platforms use OpenSSL libcrypto.

#include <string>
#include <vector>

namespace hc
{
    std::vector<unsigned char> secureRandomBytes(std::size_t size);
    std::string generateSessionToken();
    // SHA-256 as 64 hex characters. Session tokens are stored only as hashes,
    // so a copied database cannot be used to log in.
    std::string sha256Hex(const std::string &data);
    std::string hashPassword(const std::string &password);
    bool verifyPassword(const std::string &password, const std::string &storedHash);

    // Burns the same CPU time as a real verification. Used when an account
    // does not exist, so response timing does not reveal valid usernames.
    void dummyVerifyPassword(const std::string &password);
}
