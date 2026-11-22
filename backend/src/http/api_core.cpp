#include "api.h"
#include "core/crypto.h"
#include "core/routes_client.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace hc
{
    // ============================ Body ============================

    Body::Body(const std::string &raw)
    {
        if (trim(raw).empty())
        {
            empty_ = true;
            return;
        }
        value_ = crow::json::load(raw);
        if (!value_ || value_.t() != crow::json::type::Object)
            throw ApiError(400, "Request body must be a JSON object");
    }

    bool Body::has(const char *key) const
    {
        return !empty_ && value_.has(key) && value_[key].t() != crow::json::type::Null;
    }

    std::string Body::text(const char *key, bool required) const
    {
        if (!has(key))
        {
            if (required)
                throw ApiError(400, std::string(key) + " is required");
            return "";
        }
        const auto &v = value_[key];
        if (v.t() == crow::json::type::String)
            return std::string(v.s());
        if (v.t() == crow::json::type::Number)
            return Json(v).dump();
        throw ApiError(400, std::string(key) + " must be text");
    }

    int Body::integer(const char *key) const
    {
        if (!has(key))
            throw ApiError(400, std::string(key) + " is required");
        const auto &v = value_[key];
        if (v.t() == crow::json::type::Number)
        {
            double d = v.d();
            if (std::floor(d) != d || d < -2147483647.0 || d > 2147483647.0)
                throw ApiError(400, std::string(key) + " must be a whole number");
            return static_cast<int>(d);
        }
        if (v.t() == crow::json::type::String)
        {
            std::string s = trim(std::string(v.s()));
            try
            {
                std::size_t used = 0;
                int value = std::stoi(s, &used);
                if (used == s.size())
                    return value;
            }
            catch (...)
            {
            }
        }
        throw ApiError(400, std::string(key) + " must be a whole number");
    }

    bool Body::boolean(const char *key, bool fallback) const
    {
        if (!has(key))
            return fallback;
        const auto &v = value_[key];
        if (v.t() == crow::json::type::True)
            return true;
        if (v.t() == crow::json::type::False)
            return false;
        throw ApiError(400, std::string(key) + " must be true or false");
    }

    double Body::number(const char *key) const
    {
        if (!has(key))
            throw ApiError(400, std::string(key) + " is required");
        const auto &v = value_[key];
        if (v.t() == crow::json::type::Number)
            return v.d();
        if (v.t() == crow::json::type::String)
        {
            try
            {
                std::size_t used = 0;
                std::string text = trim(std::string(v.s()));
                double d = std::stod(text, &used);
                if (used == text.size())
                    return d;
            }
            catch (...)
            {
            }
        }
        throw ApiError(400, std::string(key) + " must be a number");
    }

    // ============================ Routes ============================

    const std::vector<Api::Route> &Api::routes()
    {
        static const std::vector<Route> table = {
            {"GET", "/api/health", &Api::health},
            {"GET", "/api/meta", &Api::meta},

            // Authentication (three separate portals, one session store)
            {"POST", "/api/admin/login", &Api::adminLogin},
            {"POST", "/api/worker/login", &Api::workerLogin},
            {"POST", "/api/student/login", &Api::studentLogin},
            {"POST", "/api/student/activate", &Api::studentActivate},
            {"POST", "/api/student/register", &Api::studentRegister},
            {"GET", "/api/auth/me", &Api::me},
            {"GET", "/api/worker/me", &Api::me},
            {"POST", "/api/auth/logout", &Api::logout},
            {"POST", "/api/admin/logout", &Api::logout},
            {"POST", "/api/worker/logout", &Api::logout},
            {"POST", "/api/student/logout", &Api::logout},
            {"PUT", "/api/auth/password", &Api::changeOwnPassword},

            // Complaints (role-scoped)
            {"GET", "/api/complaints", &Api::listComplaints},
            {"POST", "/api/complaints", &Api::createComplaint},
            {"GET", "/api/complaints/<int>/history", &Api::complaintHistory},
            {"PUT", "/api/complaints/<int>/status", &Api::updateStatus},
            {"PUT", "/api/complaints/<int>/priority", &Api::updatePriority},
            {"POST", "/api/complaints/assign", &Api::assignComplaint},
            {"POST", "/api/complaints/<int>/feedback", &Api::submitFeedback},

            // Admin: workers
            {"GET", "/api/workers", &Api::listWorkers},
            {"POST", "/api/workers/add", &Api::addWorker},
            {"PUT", "/api/workers/edit", &Api::editWorker},
            {"PUT", "/api/workers/status", &Api::setWorkerStatus},
            {"PUT", "/api/workers/password", &Api::setWorkerPassword},

            // Admin: smart features
            {"GET", "/api/admin/dashboard", &Api::dashboard},
            {"GET", "/api/admin/analytics", &Api::analytics},
            {"GET", "/api/complaints/<int>/dispatch", &Api::dispatchOptions},
            {"POST", "/api/complaints/<int>/auto-assign", &Api::autoAssign},
            {"GET", "/api/dispatch/queue", &Api::dispatchQueue},
            {"POST", "/api/dispatch/run", &Api::runDispatch},
            {"GET", "/api/feedback", &Api::listFeedback},
            {"GET", "/api/locations", &Api::listLocations},
            {"PUT", "/api/locations/<int>", &Api::updateLocation},

            // Worker portal
            {"GET", "/api/worker/tasks", &Api::workerTasks},
            {"GET", "/api/worker/tasks/next", &Api::workerNextTask},
            {"GET", "/api/worker/tasks/completed", &Api::workerCompleted},
            {"POST", "/api/worker/tasks/accept", &Api::workerAccept},
            {"PUT", "/api/worker/tasks/start", &Api::workerStart},
            {"PUT", "/api/worker/tasks/resolve", &Api::workerResolve},
            {"PUT", "/api/worker/tasks/escalate", &Api::workerEscalate},
            {"POST", "/api/worker/tasks/<int>/directions", &Api::taskDirections},
            {"GET", "/api/worker/availability", &Api::getAvailability},
            {"PUT", "/api/worker/availability", &Api::saveAvailability},

            // Notifications & campus routes
            {"GET", "/api/notifications", &Api::listNotifications},
            {"POST", "/api/notifications/read", &Api::markNotificationsRead},
            {"GET", "/api/routes/locations", &Api::routeLocations},
            {"GET", "/api/routes/shortest/<int>/<int>", &Api::shortestRoute},
        };
        return table;
    }

    HttpResponse Api::handle(const Route &route, const HttpRequest &request)
    {
        HttpResponse response;
        try
        {
            Reply reply = route.handler(*this, request);
            reply.body["ok"] = true;
            response.status = reply.status;
            response.body = reply.body.dump();
        }
        catch (const ApiError &e)
        {
            Json error;
            error["ok"] = false;
            error["message"] = e.what();
            if (!e.code.empty())
                error["code"] = e.code;
            response.status = e.status;
            response.body = error.dump();
        }
        catch (const std::exception &e)
        {
            std::cerr << "[error] " << request.method << " " << request.path << ": " << e.what() << "\n";
            Json error;
            error["ok"] = false;
            error["message"] = "Internal server error";
            response.status = 500;
            response.body = error.dump();
        }
        return response;
    }

    static std::vector<std::string> splitPath(const std::string &path)
    {
        std::vector<std::string> parts;
        std::string current;
        for (char ch : path)
        {
            if (ch == '/')
            {
                if (!current.empty())
                    parts.push_back(current);
                current.clear();
            }
            else
                current.push_back(ch);
        }
        if (!current.empty())
            parts.push_back(current);
        return parts;
    }

    HttpResponse Api::dispatch(HttpRequest request)
    {
        auto pathParts = splitPath(request.path.substr(0, request.path.find('?')));
        bool pathMatched = false;
        for (const auto &route : routes())
        {
            auto patternParts = splitPath(route.pattern);
            if (patternParts.size() != pathParts.size())
                continue;
            std::vector<int> params;
            bool match = true;
            for (std::size_t i = 0; i < patternParts.size() && match; ++i)
            {
                if (patternParts[i] == "<int>")
                {
                    const std::string &seg = pathParts[i];
                    match = !seg.empty() && seg.size() < 10 &&
                            std::all_of(seg.begin(), seg.end(), [](char c)
                                        { return c >= '0' && c <= '9'; });
                    if (match)
                        params.push_back(std::stoi(seg));
                }
                else
                    match = patternParts[i] == pathParts[i];
            }
            if (!match)
                continue;
            pathMatched = true;
            if (route.method != request.method)
                continue;
            request.params = params;
            return handle(route, request);
        }
        HttpResponse notFound;
        notFound.status = pathMatched ? 405 : 404;
        notFound.body = pathMatched ? R"({"ok":false,"message":"Method not allowed"})"
                                    : R"({"ok":false,"message":"Not found"})";
        return notFound;
    }

    // ============================ Setup ============================

    Api::Api(const std::string &dataDirectory) : store_(dataDirectory), sessions_(store_) {}

    static std::string envValue(const char *name)
    {
        const char *value = std::getenv(name);
        return value ? trim(value) : "";
    }

    // Demo accounts (HOSTELCARE_DEMO_MODE=1). Passwords are public on purpose:
    // the login page shows them so recruiters can try every portal.
    static const char *kDemoAdmin = "demo-warden";
    static const char *kDemoAdminPassword = "Demo-Warden-2026";
    static const char *kDemoWorker = "DEMO-E1";
    static const char *kDemoWorkerPassword = "Demo-Worker-2026";
    static const char *kDemoStudent = "DEMO-S1";
    static const char *kDemoStudentPassword = "Demo-Student-2026";

    void Api::initialize()
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        store_.initialize(envValue("HOSTELCARE_IMPORT_DIR"));
        demoMode_ = envValue("HOSTELCARE_DEMO_MODE") == "1";

        // Campus locations mirror the campus graph; coordinates are set later
        // by the warden on the Campus map page.
        for (const auto &loc : campusLocations())
            store_.ensureLocation(loc.id, loc.name);

        // First Admin account (same rule as before: env vars, 12+ characters).
        if (store_.loadAdmins().empty() && !demoMode_)
        {
            std::string user = envValue("HOSTELCARE_ADMIN_USERNAME");
            const char *pass = std::getenv("HOSTELCARE_ADMIN_PASSWORD");
            if (user.empty() || !pass || std::string(pass).size() < 12)
                throw std::runtime_error("No Admin account exists. Set HOSTELCARE_ADMIN_USERNAME and "
                                         "HOSTELCARE_ADMIN_PASSWORD (minimum 12 characters) and restart.");
            if (user.find('|') != std::string::npos || user.size() > 40)
                throw std::runtime_error("Invalid Admin username");
            store_.insertAdmin({"A001", user, hashPassword(pass)});
            std::cout << "Initial Admin account created for '" << user << "'." << std::endl;
        }

        // Optional: worker passwords from HOSTELCARE_WORKER_PASSWORD_<ID>.
        for (const auto &w : store_.loadWorkers())
        {
            if (!w.active || !store_.accountHash("worker", w.id).empty())
                continue;
            const char *pass = std::getenv(("HOSTELCARE_WORKER_PASSWORD_" + w.id).c_str());
            if (pass && std::string(pass).size() >= 12)
                store_.setAccountHash("worker", w.id, hashPassword(pass));
        }

        if (demoMode_)
        {
            bool hasDemoAdmin = false;
            for (const auto &a : store_.loadAdmins())
                hasDemoAdmin = hasDemoAdmin || a.username == kDemoAdmin;
            if (!hasDemoAdmin)
                store_.insertAdmin({"A900", kDemoAdmin, hashPassword(kDemoAdminPassword)});
            if (!store_.findWorker(kDemoWorker))
                store_.insertWorker({kDemoWorker, "Demo Electrician", "Electrical", true});
            if (store_.accountHash("worker", kDemoWorker).empty())
                store_.setAccountHash("worker", kDemoWorker, hashPassword(kDemoWorkerPassword));
            if (!store_.findStudent(kDemoStudent))
                store_.insertStudent({kDemoStudent, "Demo Student", "D-101", "9000000000", "Atlantis Hostel"});
            if (store_.accountHash("student", kDemoStudent).empty())
                store_.setAccountHash("student", kDemoStudent, hashPassword(kDemoStudentPassword));
            std::cout << "Demo mode: demo accounts are available on the login page." << std::endl;
        }

        // Real road distances via Google (optional).
        std::string key = envValue("HOSTELCARE_GOOGLE_MAPS_SERVER_KEY");
        if (!key.empty() && !routes_)
        {
            std::string endpoint = envValue("HOSTELCARE_ROUTES_ENDPOINT");
            std::string mode = envValue("HOSTELCARE_TRAVEL_MODE");
            if (mode != "WALK" && mode != "TWO_WHEELER" && mode != "DRIVE" && mode != "BICYCLE")
                mode = "WALK";
            routes_ = makeGoogleRoutesClient(
                key, endpoint.empty() ? "https://routes.googleapis.com/directions/v2:computeRoutes" : endpoint, mode);
            std::cout << "Google Routes API enabled (" << mode << ")." << std::endl;
        }
    }

    bool Api::isDemoAccount(const Principal &p) const
    {
        return demoMode_ && ((p.role == Role::Admin && p.id == "A900") ||
                             (p.role == Role::Worker && p.id == kDemoWorker) ||
                             (p.role == Role::Student && p.id == kDemoStudent));
    }

    // ============================ Helpers ============================

    std::string Api::bearerToken(const HttpRequest &request)
    {
        const std::string prefix = "Bearer ";
        if (request.authorization.rfind(prefix, 0) != 0)
            return "";
        return trim(request.authorization.substr(prefix.size()));
    }

    Principal Api::requireAuth(const HttpRequest &request, std::initializer_list<Role> allowed)
    {
        auto principal = sessions_.validate(bearerToken(request));
        if (!principal)
            throw ApiError(401, "Please log in again (session missing or expired)", "SESSION_EXPIRED");
        for (Role r : allowed)
            if (r == principal->role)
                return *principal;
        throw ApiError(403, "You do not have permission for this action");
    }

    std::string Api::actorOf(const Principal &p)
    {
        return roleName(p.role) + ":" + p.id;
    }

    int Api::idFromBody(const Body &body, const char *key)
    {
        int id = body.integer(key);
        if (id <= 0)
            throw ApiError(400, std::string(key) + " must be a positive number");
        return id;
    }

    Complaint &Api::findComplaint(std::vector<Complaint> &list, int id)
    {
        for (auto &c : list)
            if (c.id == id)
                return c;
        throw ApiError(404, "Complaint #" + std::to_string(id) + " not found");
    }

    void Api::recordHistory(int complaintId, const std::string &status, const std::string &actor)
    {
        store_.appendHistory({complaintId, status, nowTimestamp(), actor});
    }

    void Api::notify(const std::string &role, const std::string &recipientId, int complaintId,
                     const std::string &message)
    {
        Notification n;
        n.role = role;
        n.recipientId = recipientId;
        n.complaintId = complaintId;
        n.message = sanitizeField(message);
        n.timestamp = nowTimestamp();
        store_.appendNotification(n);
    }

    void Api::notifyAdmins(int complaintId, const std::string &message)
    {
        notify("admin", "*", complaintId, message);
    }

    Json Api::userJson(const Principal &p)
    {
        Json user;
        user["role"] = roleName(p.role);
        user["demo"] = isDemoAccount(p);
        if (p.role == Role::Admin)
        {
            for (const auto &a : store_.loadAdmins())
                if (a.id == p.id)
                {
                    user["adminId"] = a.id;
                    user["username"] = a.username;
                    user["name"] = a.username;
                }
        }
        else if (p.role == Role::Worker)
        {
            if (auto w = store_.findWorker(p.id))
            {
                user["workerId"] = w->id;
                user["name"] = w->name;
                user["department"] = canonicalDepartment(w->department).empty() ? w->department
                                                                                : canonicalDepartment(w->department);
                user["office"] = locationName(officeForDepartment(w->department));
            }
        }
        else if (auto s = store_.findStudent(p.id))
        {
            user["studentId"] = s->id;
            user["name"] = s->name;
            user["roomNo"] = s->room;
            user["phone"] = s->phone;
            user["hostelName"] = s->hostel;
        }
        return user;
    }

    // ============================ Auth handlers ============================

    static std::string throttleKey(Role role, const std::string &id)
    {
        return roleName(role) + ":" + toLower(trim(id));
    }

    Reply Api::adminLogin(const HttpRequest &request)
    {
        Body body(request.body);
        std::string username = trim(body.text("username"));
        std::string password = body.text("password");
        const std::string key = throttleKey(Role::Admin, username);
        throttle_.check(key);

        AdminRecord found;
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            for (const auto &a : store_.loadAdmins())
                if (a.username == username)
                    found = a;
        }

        // Verify outside the data lock: PBKDF2 is deliberately slow.
        bool ok;
        if (found.id.empty())
        {
            dummyVerifyPassword(password);
            ok = false;
        }
        else
            ok = verifyPassword(password, found.passwordHash);

        if (!ok)
        {
            throttle_.recordFailure(key);
            throw ApiError(401, "Invalid username or password");
        }
        throttle_.recordSuccess(key);

        Principal p{Role::Admin, found.id};
        Reply reply;
        reply.body["token"] = sessions_.create(p);
        std::lock_guard<std::mutex> lock(dataMutex_);
        reply.body["user"] = userJson(p);
        reply.body["message"] = "Admin login successful";
        return reply;
    }

    Reply Api::loginWithAccount(Role role, const std::string &id, const std::string &password)
    {
        const std::string key = throttleKey(role, id);
        throttle_.check(key);

        std::string hash;
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            if (role == Role::Worker)
            {
                auto worker = store_.findWorker(id);
                if (worker && worker->active)
                    hash = store_.accountHash("worker", id);
            }
            else if (store_.findStudent(id))
            {
                hash = store_.accountHash("student", id);
                if (hash.empty())
                    throw ApiError(403, "This account has no password yet. Use 'Set up password' "
                                        "with your registered phone number.",
                                   "PASSWORD_NOT_SET");
            }
        }

        bool ok;
        if (hash.empty())
        {
            dummyVerifyPassword(password);
            ok = false;
        }
        else
            ok = verifyPassword(password, hash);

        if (!ok)
        {
            throttle_.recordFailure(key);
            if (role == Role::Worker)
                throw ApiError(401, "Invalid Worker ID or password. New workers: ask the warden to "
                                    "set your password.");
            throw ApiError(401, "Invalid Student ID or password");
        }
        throttle_.recordSuccess(key);

        Principal p{role, id};
        Reply reply;
        reply.body["token"] = sessions_.create(p);
        std::lock_guard<std::mutex> lock(dataMutex_);
        reply.body["user"] = userJson(p);
        reply.body["message"] = "Login successful";
        return reply;
    }

    Reply Api::workerLogin(const HttpRequest &request)
    {
        Body body(request.body);
        return loginWithAccount(Role::Worker, trim(body.text("workerId")), body.text("password"));
    }

    Reply Api::studentLogin(const HttpRequest &request)
    {
        Body body(request.body);
        return loginWithAccount(Role::Student, trim(body.text("studentId")), body.text("password"));
    }

    // One-time password setup for students registered before passwords
    // existed. Proves identity with the registered phone number.
    Reply Api::studentActivate(const HttpRequest &request)
    {
        Body body(request.body);
        std::string studentId = trim(body.text("studentId"));
        std::string phone = trim(body.text("phone"));
        std::string password = body.text("password");
        requirePasswordStrength(password, 8);

        const std::string key = "student-activate:" + toLower(studentId);
        throttle_.check(key);
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            auto student = store_.findStudent(studentId);
            bool match = student && student->phone == phone;
            if (!match)
            {
                throttle_.recordFailure(key);
                throw ApiError(401, "Student ID and phone number do not match our records");
            }
            if (!store_.accountHash("student", studentId).empty())
                throw ApiError(409, "A password is already set. Please log in.");
        }

        std::string hash = hashPassword(password); // outside the lock
        Principal p{Role::Student, studentId};
        Reply reply;
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            if (!store_.accountHash("student", studentId).empty())
                throw ApiError(409, "A password is already set. Please log in.");
            store_.setAccountHash("student", studentId, hash);
            throttle_.recordSuccess(key);
            reply.body["user"] = userJson(p);
        }
        reply.body["token"] = sessions_.create(p);
        reply.body["message"] = "Password set. You are now logged in.";
        return reply;
    }

    Reply Api::studentRegister(const HttpRequest &request)
    {
        Body body(request.body);
        Student s;
        s.id = requireId(body.text("studentId"), "Student ID");
        s.name = requireText(body.text("name"), "Name", 60, 2);
        s.room = requireText(body.text("room"), "Room number", 20);
        s.phone = requirePhone(body.text("phone"));
        s.hostel = requireText(body.text("hostel"), "Hostel name", 60);
        std::string password = body.text("password");
        requirePasswordStrength(password, 8);

        std::string hash = hashPassword(password);

        std::lock_guard<std::mutex> lock(dataMutex_);
        for (const auto &existing : store_.loadStudents())
        {
            if (iequals(existing.id, s.id))
                throw ApiError(409, "This Student ID is already registered");
            if (existing.phone == s.phone)
                throw ApiError(409, "This phone number is already registered");
        }
        DataStore::Transaction tx(store_);
        store_.insertStudent(s);
        store_.setAccountHash("student", s.id, hash);
        tx.commit();

        Reply reply;
        reply.status = 201;
        reply.body["message"] = "Student registered successfully. You can now log in.";
        return reply;
    }

    Reply Api::me(const HttpRequest &request)
    {
        // /api/worker/me keeps its original meaning: worker sessions only.
        Principal p = request.path.rfind("/api/worker/", 0) == 0
                          ? requireAuth(request, {Role::Worker})
                          : requireAuth(request, {Role::Admin, Role::Worker, Role::Student});
        Reply reply;
        std::lock_guard<std::mutex> lock(dataMutex_);
        reply.body["user"] = userJson(p);
        reply.body["worker"] = userJson(p); // backward-compatible key
        reply.body["role"] = roleName(p.role);
        return reply;
    }

    Reply Api::logout(const HttpRequest &request)
    {
        sessions_.revoke(bearerToken(request));
        Reply reply;
        reply.body["message"] = "Logged out";
        return reply;
    }

    Reply Api::changeOwnPassword(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Admin, Role::Worker, Role::Student});
        if (isDemoAccount(p))
            throw ApiError(403, "Demo accounts cannot change their password");
        Body body(request.body);
        std::string current = body.text("currentPassword");
        std::string next = body.text("newPassword");
        requirePasswordStrength(next, p.role == Role::Admin ? 12 : 8);
        if (current == next)
            throw ApiError(400, "New password must be different from the current password");

        std::string storedHash;
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            if (p.role == Role::Admin)
            {
                for (const auto &a : store_.loadAdmins())
                    if (a.id == p.id)
                        storedHash = a.passwordHash;
            }
            else
                storedHash = store_.accountHash(roleName(p.role), p.id);
        }
        const std::string key = "password-change:" + actorOf(p);
        throttle_.check(key);
        if (storedHash.empty() || !verifyPassword(current, storedHash))
        {
            throttle_.recordFailure(key);
            throw ApiError(401, "Current password is incorrect");
        }
        throttle_.recordSuccess(key);

        std::string newHash = hashPassword(next);
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            if (p.role == Role::Admin)
                store_.setAdminPassword(p.id, newHash);
            else
                store_.setAccountHash(roleName(p.role), p.id, newHash);
        }

        // Log out every other device, then give this one a fresh session.
        sessions_.revokeUser(p.role, p.id);
        Reply reply;
        reply.body["token"] = sessions_.create(p);
        reply.body["message"] = "Password changed. Other sessions were logged out.";
        return reply;
    }

    // ============================ Misc ============================

    Reply Api::health(const HttpRequest &)
    {
        Reply reply;
        reply.body["message"] = "HostelCare C++ API is running";
        return reply;
    }

    static Json stringList(const std::vector<std::string> &values)
    {
        Json::list list;
        for (const auto &v : values)
            list.push_back(Json(v));
        return Json(std::move(list));
    }

    Reply Api::meta(const HttpRequest &)
    {
        Reply reply;
        reply.body["departments"] = stringList(departments());
        reply.body["categories"] = stringList(complaintCategories());
        reply.body["hostels"] = stringList(knownHostels());
        reply.body["statuses"] = stringList(allStatuses());
        reply.body["priorities"] = stringList({"Low", "Medium", "High", "Critical"});
        reply.body["maxActiveTasksPerWorker"] = kMaxActiveTasksPerWorker;
        reply.body["googleRoutes"] = static_cast<bool>(routes_);
        reply.body["travelMode"] = routes_ ? routes_->travelMode() : "WALK";
        reply.body["demoMode"] = demoMode_;
        if (demoMode_)
        {
            Json::list demo;
            auto entry = [&](const char *role, const char *login, const char *password)
            {
                Json item;
                item["role"] = role;
                item["login"] = login;
                item["password"] = password;
                demo.push_back(std::move(item));
            };
            entry("admin", kDemoAdmin, kDemoAdminPassword);
            entry("worker", kDemoWorker, kDemoWorkerPassword);
            entry("student", kDemoStudent, kDemoStudentPassword);
            reply.body["demoAccounts"] = std::move(demo);
        }
        return reply;
    }
}
