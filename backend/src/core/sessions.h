#pragma once
// Login sessions for all three portals, stored in the database (as SHA-256
// hashes of the tokens) so users stay logged in across server restarts.
// Also a simple in-memory brute-force guard for logins.

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace hc
{
    class DataStore;

    enum class Role
    {
        Admin,
        Worker,
        Student
    };

    std::string roleName(Role role); // "admin" / "worker" / "student"
    std::optional<Role> roleFromName(const std::string &name);

    struct Principal
    {
        Role role = Role::Student;
        std::string id;
    };

    class SessionManager
    {
    public:
        SessionManager(DataStore &store, long long idleSeconds = 30 * 60, long long absoluteSeconds = 8 * 3600);

        std::string create(const Principal &principal);
        // Valid token -> principal (and the idle timer is extended).
        std::optional<Principal> validate(const std::string &token);
        void revoke(const std::string &token);
        // Log a user out everywhere (deactivation, password change).
        void revokeUser(Role role, const std::string &id);

    private:
        DataStore &store_;
        long long idle_, absolute_;
        long long lastPurge_ = 0;
        std::mutex mutex_;
    };

    class LoginThrottle
    {
    public:
        using Clock = std::chrono::steady_clock;

        // Throws ApiError(429) if this key is currently locked.
        void check(const std::string &key);
        void recordFailure(const std::string &key);
        void recordSuccess(const std::string &key);

        static const int kMaxFailures = 5;

    private:
        struct Entry
        {
            int failures = 0;
            Clock::time_point windowStart{};
            Clock::time_point lockedUntil{};
        };
        std::unordered_map<std::string, Entry> entries_;
        std::mutex mutex_;
    };
}
