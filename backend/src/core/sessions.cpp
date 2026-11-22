#include "sessions.h"
#include "crypto.h"
#include "store.h"
#include "util.h"

namespace hc
{
    std::string roleName(Role role)
    {
        switch (role)
        {
        case Role::Admin:
            return "admin";
        case Role::Worker:
            return "worker";
        default:
            return "student";
        }
    }

    std::optional<Role> roleFromName(const std::string &name)
    {
        if (name == "admin")
            return Role::Admin;
        if (name == "worker")
            return Role::Worker;
        if (name == "student")
            return Role::Student;
        return std::nullopt;
    }

    SessionManager::SessionManager(DataStore &store, long long idleSeconds, long long absoluteSeconds)
        : store_(store), idle_(idleSeconds), absolute_(absoluteSeconds) {}

    std::string SessionManager::create(const Principal &principal)
    {
        std::string token = generateSessionToken();
        long long now = static_cast<long long>(nowEpoch());
        std::lock_guard<std::mutex> lock(mutex_);
        if (now - lastPurge_ > 600) // clean up old rows every 10 minutes
        {
            store_.deleteExpiredSessions(now - idle_, now - absolute_);
            lastPurge_ = now;
        }
        store_.insertSession({sha256Hex(token), roleName(principal.role), principal.id, now, now});
        return token;
    }

    std::optional<Principal> SessionManager::validate(const std::string &token)
    {
        if (token.size() != 64)
            return std::nullopt;
        const std::string hash = sha256Hex(token);
        long long now = static_cast<long long>(nowEpoch());
        std::lock_guard<std::mutex> lock(mutex_);
        auto row = store_.findSession(hash);
        if (!row)
            return std::nullopt;
        auto role = roleFromName(row->role);
        if (!role || now - row->lastSeen >= idle_ || now - row->createdAt >= absolute_)
        {
            store_.deleteSession(hash);
            return std::nullopt;
        }
        if (now - row->lastSeen >= 30) // sliding expiry, at most one write per 30 s
            store_.touchSession(hash, now);
        return Principal{*role, row->userId};
    }

    void SessionManager::revoke(const std::string &token)
    {
        if (token.size() != 64)
            return;
        std::lock_guard<std::mutex> lock(mutex_);
        store_.deleteSession(sha256Hex(token));
    }

    void SessionManager::revokeUser(Role role, const std::string &id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        store_.deleteUserSessions(roleName(role), id);
    }

    // ---------------- LoginThrottle ----------------
    // 5 failures within 15 minutes lock the account key for 5 minutes.

    void LoginThrottle::check(const std::string &key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it != entries_.end() && Clock::now() < it->second.lockedUntil)
            throw ApiError(429, "Too many failed attempts. Try again in a few minutes.");
    }

    void LoginThrottle::recordFailure(const std::string &key)
    {
        auto now = Clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        Entry &e = entries_[key];
        if (now - e.windowStart > std::chrono::minutes(15))
        {
            e.failures = 0;
            e.windowStart = now;
        }
        if (++e.failures >= kMaxFailures)
        {
            e.lockedUntil = now + std::chrono::minutes(5);
            e.failures = 0;
            e.windowStart = now;
        }
    }

    void LoginThrottle::recordSuccess(const std::string &key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.erase(key);
    }
}
