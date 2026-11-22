// HostelCare API tests. Runs every endpoint in-process against a COPY of the
// data folder, so your real data/ files are never modified.
//
// Build:  cmake --build build-ucrt --config Release --target hostelcare_tests
// Run (from backend/):  ./build-linux/hostelcare_tests   (uses tests/fixtures)
//
// Exit code 0 = all tests passed.

#include "http/api.h"
#include "core/store.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using hc::HttpRequest;
using hc::HttpResponse;

// Fake Google client: lets the directions endpoint be tested without
// network access or an API key.
struct FakeRoutes : hc::RoutesClient
{
    bool fail = false;
    int calls = 0;
    hc::TravelResult compute(const hc::LatLng &, const hc::LatLng &) override
    {
        ++calls;
        hc::TravelResult r;
        if (fail)
            r.error = "Google Maps error (HTTP 403): API key not valid";
        else
        {
            r.ok = true;
            r.distanceMeters = 1234;
            r.durationSeconds = 900;
        }
        return r;
    }
    std::string travelMode() const override { return "WALK"; }
};

static int passed = 0, failed = 0;

#define CHECK(cond, what)                                                       \
    do                                                                          \
    {                                                                           \
        if (cond)                                                               \
            ++passed;                                                           \
        else                                                                    \
        {                                                                       \
            ++failed;                                                           \
            std::cerr << "FAIL line " << __LINE__ << ": " << what << "\n";      \
        }                                                                       \
    } while (0)

static void setEnv(const char *name, const char *value)
{
#ifdef _WIN32
    _putenv((std::string(name) + "=" + value).c_str());
#else
    setenv(name, value, 1);
#endif
}

struct Client
{
    hc::Api &api;
    HttpResponse last;
    crow::json::rvalue json;

    crow::json::rvalue call(const std::string &method, const std::string &path,
                            const std::string &body = "", const std::string &token = "")
    {
        HttpRequest r;
        r.method = method;
        r.path = path;
        r.body = body;
        if (!token.empty())
            r.authorization = "Bearer " + token;
        last = api.dispatch(r);
        json = crow::json::load(last.body);
        return json;
    }
    int status() const { return last.status; }
    std::string str(const char *key) const
    {
        return json && json.has(key) && json[key].t() == crow::json::type::String ? std::string(json[key].s()) : "";
    }
};

static std::vector<std::string> fileLines(const fs::path &p)
{
    std::vector<std::string> lines;
    std::ifstream in(p);
    std::string line;
    while (std::getline(in, line))
        if (!line.empty())
            lines.push_back(line);
    return lines;
}

int main(int argc, char **argv)
{
    const fs::path source = argc > 1 ? argv[1] : "tests/fixtures";
    const fs::path dir = fs::temp_directory_path() / "hostelcare_test_data";
    fs::remove_all(dir);
    fs::create_directories(dir);
    for (const char *f : {"complaints.txt", "students.txt", "workers.txt", "status_history.txt",
                          "worker_accounts.txt", "feedback.txt"})
        if (fs::exists(source / f))
            fs::copy_file(source / f, dir / f);
    // admins.txt is NOT copied: the test creates its own Admin from env vars.
    setEnv("HOSTELCARE_ADMIN_USERNAME", "testwarden");
    setEnv("HOSTELCARE_ADMIN_PASSWORD", "Warden-Test-2026");

    const auto originalComplaints = fileLines(dir / "complaints.txt");

    setEnv("HOSTELCARE_DEMO_MODE", "0");
    auto *apiPtr = new hc::Api(dir.string());
    hc::Api &api = *apiPtr;
    api.initialize();
    auto fakeOwned = std::make_unique<FakeRoutes>();
    FakeRoutes *fake = fakeOwned.get();
    api.setRoutesClient(std::move(fakeOwned));
    Client c{api, {}, {}};
    int created = 0;

    // ---------- Public endpoints ----------
    c.call("GET", "/api/health");
    CHECK(c.status() == 200, "health returns 200");
    c.call("GET", "/api/routes/shortest/2/6");
    CHECK(c.json["pathExists"].b(), "IT Support Office -> Atlantis Hostel route exists (graph fixed)");
    c.call("GET", "/api/routes/locations");
    CHECK(c.json["locations"].size() == 12, "12 campus locations");

    // ---------- Everything else needs a session ----------
    c.call("GET", "/api/complaints");
    CHECK(c.status() == 401, "complaints without token -> 401");
    c.call("GET", "/api/workers");
    CHECK(c.status() == 401, "workers without token -> 401");
    c.call("POST", "/api/complaints", R"({"title":"x"})");
    CHECK(c.status() == 401, "create complaint without token -> 401");

    // ---------- Admin login ----------
    c.call("POST", "/api/admin/login", R"({"username":"testwarden","password":"wrong-password-1"})");
    CHECK(c.status() == 401, "admin wrong password -> 401");
    c.call("POST", "/api/admin/login", R"({"username":"testwarden","password":"Warden-Test-2026"})");
    CHECK(c.status() == 200, "admin login works");
    const std::string admin = c.str("token");
    CHECK(admin.size() == 64, "admin token is 64 hex chars");

    c.call("GET", "/api/complaints", "", admin);
    const std::size_t originalCount = originalComplaints.size();
    CHECK(c.json["complaints"].size() == originalCount, "text files imported: admin sees all existing complaints");

    // ---------- Workers ----------
    c.call("GET", "/api/workers", "", admin);
    CHECK(c.status() == 200, "admin lists workers");
    bool t101AtCapacity = false;
    for (const auto &w : c.json["workers"])
        if (std::string(w["workerId"].s()) == "T101")
            t101AtCapacity = std::string(w["availability"].s()) == "At capacity" && !w["hasLogin"].b();
    CHECK(t101AtCapacity, "T101 shows 6 active tasks = At capacity, no login yet");

    c.call("PUT", "/api/workers/password", R"({"workerId":"T101","password":"short"})", admin);
    CHECK(c.status() == 400, "weak worker password rejected");
    c.call("PUT", "/api/workers/password", R"({"workerId":"X999","password":"Laxman-2026"})", admin);
    CHECK(c.status() == 404, "unknown worker -> 404");
    c.call("PUT", "/api/workers/password", R"({"workerId":"T101","password":"Laxman-2026"})", admin);
    CHECK(c.status() == 200, "admin sets T101 password");

    c.call("POST", "/api/worker/login", R"({"workerId":"T101","password":"Laxman-2026"})");
    CHECK(c.status() == 200, "T101 can log in");
    std::string t101 = c.str("token");
    c.call("GET", "/api/worker/tasks", "", t101);
    CHECK(c.json["tasks"].size() == 6, "T101 sees exactly 6 active tasks");
    CHECK(c.json["tasks"][0]["id"].i() == 14, "highest priority task (#14 High) is first");
    CHECK(c.json["tasks"][0]["route"]["pathExists"].b(), "task includes Dijkstra route");
    c.call("GET", "/api/worker/tasks/next", "", t101);
    CHECK(c.json["task"]["id"].i() == 14, "next task = #14");

    c.call("GET", "/api/workers", "", t101);
    CHECK(c.status() == 403, "worker cannot list workers -> 403");
    c.call("POST", "/api/worker/tasks/accept", R"({"complaintId":2})", t101);
    CHECK(c.status() == 403, "worker cannot act on another worker's task");

    c.call("POST", "/api/workers/add",
           R"({"workerId":"E102","name":"Test Electrician","department":"electrical","password":"Spark-2026"})", admin);
    CHECK(c.status() == 201, "add worker with password");
    c.call("POST", "/api/workers/add", R"({"workerId":"e102","name":"Dup","department":"Electrical"})", admin);
    CHECK(c.status() == 409, "duplicate worker ID (case-insensitive) -> 409");
    c.call("POST", "/api/workers/add", R"({"workerId":"Z1","name":"Zed","department":"Cooking"})", admin);
    CHECK(c.status() == 400, "unknown department rejected");
    c.call("PUT", "/api/workers/edit", R"({"workerId":"T102","name":"SS Pun","department":"General Maintenance"})", admin);
    CHECK(c.status() == 200, "edit worker");

    // ---------- Students ----------
    c.call("POST", "/api/student/login", R"({"studentId":"2000","password":"anything1"})");
    CHECK(c.status() == 403 && c.str("code") == "PASSWORD_NOT_SET", "old student must set a password first");
    c.call("POST", "/api/student/activate", R"({"studentId":"2000","phone":"1111111111","password":"Yashu-2026"})");
    CHECK(c.status() == 401, "activation with wrong phone fails");
    c.call("POST", "/api/student/activate", R"({"studentId":"2000","phone":"9000000011","password":"Yashu-2026"})");
    CHECK(c.status() == 200, "activation with registered phone works");
    std::string yashu = c.str("token");
    c.call("POST", "/api/student/activate", R"({"studentId":"2000","phone":"9000000011","password":"Other-2026"})");
    CHECK(c.status() == 409, "cannot activate twice");

    c.call("GET", "/api/complaints", "", yashu);
    bool onlyOwn = c.json["complaints"].size() > 0;
    for (const auto &item : c.json["complaints"])
        onlyOwn = onlyOwn && std::string(item["studentId"].s()) == "2000";
    CHECK(onlyOwn, "student sees only their own complaints (server-side filter)");
    c.call("GET", "/api/complaints/1/history", "", yashu);
    CHECK(c.status() == 404, "student cannot read another student's history");
    c.call("GET", "/api/workers", "", yashu);
    CHECK(c.status() == 403, "student cannot list workers");

    c.call("POST", "/api/student/register",
           R"({"studentId":"3001","name":"New Student","room":"B12","phone":"9123400001","hostel":"SC Bose Hostel","password":"Newbie-2026"})");
    CHECK(c.status() == 201, "register new student");
    c.call("POST", "/api/student/register",
           R"({"studentId":"3002","name":"Copy","room":"B13","phone":"9123400001","hostel":"SC Bose Hostel","password":"Newbie-2026"})");
    CHECK(c.status() == 409, "duplicate phone rejected");
    c.call("POST", "/api/student/login", R"({"studentId":"3001","password":"Newbie-2026"})");
    CHECK(c.status() == 200, "new student logs in with password");
    std::string newbie = c.str("token");

    // ---------- Complaint lifecycle ----------
    c.call("POST", "/api/complaints",
           R"({"title":"Sparks from socket","category":"Electrical","description":"socket shows sparks near bed","priority":"Low"})",
           newbie);
    CHECK(c.status() == 201, "student submits complaint");
    ++created;
    int fireId = static_cast<int>(c.json["id"].i());
    CHECK(c.str("priority") == "Critical", "keyword priority raises Low -> Critical");
    c.call("POST", "/api/complaints", R"({"title":"Bad","category":"Cooking","description":"nothing here"})", newbie);
    CHECK(c.status() == 400, "unknown category rejected");
    c.call("POST", "/api/complaints", R"({"title":"x","category":"Plumbing","description":"leak"})", newbie);
    CHECK(c.status() == 400, "too-short title/description rejected");

    const std::string fire = std::to_string(fireId);
    c.call("POST", "/api/complaints/assign", R"({"complaintId":)" + fire + R"(,"workerId":"E101"})", admin);
    CHECK(c.status() == 400, "cannot assign a Pending complaint");
    c.call("PUT", "/api/complaints/" + fire + "/status", R"({"status":"Resolved"})", admin);
    CHECK(c.status() == 400, "invalid transition Pending -> Resolved rejected");
    c.call("PUT", "/api/complaints/" + fire + "/status", R"({"status":"Verified"})", newbie);
    CHECK(c.status() == 403, "student cannot change status");
    c.call("PUT", "/api/complaints/" + fire + "/status", R"({"status":"Verified"})", admin);
    CHECK(c.status() == 200, "admin verifies complaint");

    c.call("GET", "/api/complaints/" + fire + "/dispatch", "", admin);
    CHECK(c.json["success"].b() && std::string(c.json["recommended"]["department"].s()) == "Electrical",
          "dispatch recommends an Electrical worker");
    std::string recommended = std::string(c.json["recommended"]["workerId"].s());
    CHECK(recommended == "E102", "fewest active tasks wins the tie (E102 has 0, E101 has 1)");

    c.call("POST", "/api/complaints/assign", R"({"complaintId":)" + fire + R"(,"workerId":"P101"})", admin);
    CHECK(c.status() == 409 && c.str("code") == "DEPARTMENT_MISMATCH", "department mismatch needs override");

    c.call("POST", "/api/complaints/" + fire + "/auto-assign", "", admin);
    CHECK(c.status() == 200 && c.str("workerId") == "E102", "auto-assign picks E102");

    c.call("POST", "/api/worker/login", R"({"workerId":"E102","password":"Spark-2026"})");
    std::string e102 = c.str("token");
    c.call("PUT", "/api/worker/tasks/start", R"({"complaintId":)" + fire + "}", e102);
    CHECK(c.status() == 400, "cannot start before accepting");
    c.call("POST", "/api/worker/tasks/accept", R"({"complaintId":)" + fire + "}", e102);
    CHECK(c.status() == 200, "worker accepts");
    c.call("PUT", "/api/worker/tasks/start", R"({"complaintId":)" + fire + "}", e102);
    CHECK(c.status() == 200, "worker starts");
    c.call("PUT", "/api/worker/tasks/resolve", R"({"complaintId":)" + fire + "}", e102);
    CHECK(c.status() == 400, "resolve requires a note");
    c.call("PUT", "/api/worker/tasks/resolve", R"({"complaintId":)" + fire + R"(,"note":"Replaced socket"})", e102);
    CHECK(c.status() == 200, "worker resolves");

    c.call("POST", "/api/complaints/" + fire + "/feedback", R"({"rating":6})", newbie);
    CHECK(c.status() == 400, "rating out of range rejected");
    c.call("POST", "/api/complaints/" + fire + "/feedback", R"({"rating":5,"comment":"Quick fix"})", yashu);
    CHECK(c.status() == 404, "student cannot rate someone else's complaint");
    c.call("POST", "/api/complaints/" + fire + "/feedback", R"({"rating":5,"comment":"Quick fix"})", newbie);
    CHECK(c.status() == 201, "owner rates resolved complaint");
    c.call("POST", "/api/complaints/" + fire + "/feedback", R"({"rating":4})", newbie);
    CHECK(c.status() == 409, "cannot rate twice");
    c.call("GET", "/api/worker/tasks/completed", "", e102);
    CHECK(c.json["averageRating"].d() == 5.0, "worker sees average rating 5");

    c.call("GET", "/api/complaints/" + fire + "/history", "", newbie);
    CHECK(c.json["history"].size() == 6, "history: Submitted, Verified, Assigned, Accepted, In Progress, Resolved");

    // ---------- Notifications ----------
    c.call("GET", "/api/notifications", "", newbie);
    CHECK(c.json["unread"].i() >= 5, "student received status notifications");
    c.call("POST", "/api/notifications/read", "", newbie);
    c.call("GET", "/api/notifications", "", newbie);
    CHECK(c.json["unread"].i() == 0, "mark all read");
    c.call("GET", "/api/notifications", "", admin);
    CHECK(c.json["unread"].i() >= 2, "admin notified of new complaint and resolution");

    // ---------- Priority-queue dispatch ----------
    c.call("POST", "/api/complaints",
           R"({"title":"Wifi down","category":"Internet","description":"no network in room","priority":"Medium"})", newbie);
    int wifi = static_cast<int>(c.json["id"].i());
    ++created;
    c.call("POST", "/api/complaints",
           R"({"title":"Room dusty","category":"Cleaning","description":"room needs cleaning","priority":"Low"})", newbie);
    int dust = static_cast<int>(c.json["id"].i());
    ++created;
    for (int id : {wifi, dust})
        c.call("PUT", "/api/complaints/" + std::to_string(id) + "/status", R"({"status":"Verified"})", admin);
    c.call("GET", "/api/dispatch/queue", "", admin);
    CHECK(c.status() == 200 && c.json["queue"].size() >= 2, "dispatch queue lists waiting complaints");
    c.call("POST", "/api/dispatch/run", "", admin);
    CHECK(c.status() == 200 && c.json["assigned"].i() >= 2, "auto-dispatch assigns waiting complaints");
    CHECK(c.json["results"][0]["complaintId"].i() == wifi, "Medium complaint dispatched before Low");

    // ---------- Security: deactivation, throttling, password change ----------
    c.call("PUT", "/api/workers/status", R"({"workerId":"T101","isActive":false})", admin);
    CHECK(c.status() == 200 && c.json["activeTasks"].i() == 6, "deactivation reports active tasks");
    c.call("GET", "/api/worker/tasks", "", t101);
    CHECK(c.status() == 401, "deactivated worker's session is revoked immediately");
    c.call("POST", "/api/worker/login", R"({"workerId":"T101","password":"Laxman-2026"})");
    CHECK(c.status() == 401, "inactive worker cannot log in");
    c.call("PUT", "/api/workers/status", R"({"workerId":"T101","isActive":true})", admin);

    for (int i = 0; i < 5; ++i)
        c.call("POST", "/api/worker/login", R"({"workerId":"P101","password":"wrong-guess-1"})");
    c.call("POST", "/api/worker/login", R"({"workerId":"P101","password":"wrong-guess-1"})");
    CHECK(c.status() == 429, "6th failed login is throttled");

    c.call("PUT", "/api/auth/password", R"({"currentPassword":"bad","newPassword":"Newer-2026x"})", newbie);
    CHECK(c.status() == 401, "change password needs the current password");
    c.call("PUT", "/api/auth/password", R"({"currentPassword":"Newbie-2026","newPassword":"Newer-2026x"})", newbie);
    CHECK(c.status() == 200, "student changes password");
    std::string fresh = c.str("token");
    c.call("GET", "/api/auth/me", "", newbie);
    CHECK(c.status() == 401, "old session revoked after password change");
    c.call("GET", "/api/auth/me", "", fresh);
    CHECK(c.status() == 200, "new session works");

    c.call("POST", "/api/auth/logout", "", admin);
    c.call("GET", "/api/admin/dashboard", "", admin);
    CHECK(c.status() == 401, "logout revokes admin token");

    // ---------- Analytics ----------
    c.call("POST", "/api/admin/login", R"({"username":"testwarden","password":"Warden-Test-2026"})");
    std::string admin2 = c.str("token");
    c.call("GET", "/api/admin/analytics", "", admin2);
    CHECK(c.status() == 200 && c.json["ratingsCount"].i() == 1, "analytics include ratings");
    CHECK(c.json["dailySubmissions"].size() == 14, "14-day trend");

    // ---------- Worker availability ----------
    const std::string allOff = R"({"days":[{"working":false},{"working":false},{"working":false},{"working":false},{"working":false},{"working":false},{"working":false}]})";
    const std::string allOn = R"({"days":[{"working":true,"start":"00:00","end":"00:00"},{"working":true,"start":"00:00","end":"00:00"},{"working":true,"start":"00:00","end":"00:00"},{"working":true,"start":"00:00","end":"00:00"},{"working":true,"start":"00:00","end":"00:00"},{"working":true,"start":"00:00","end":"00:00"},{"working":true,"start":"00:00","end":"00:00"}]})";
    c.call("POST", "/api/worker/login", R"({"workerId":"E102","password":"Spark-2026"})");
    e102 = c.str("token");
    c.call("GET", "/api/worker/availability", "", e102);
    CHECK(c.status() == 200 && !c.json["availability"]["configured"].b(), "no schedule set yet");
    c.call("PUT", "/api/worker/availability", R"({"days":[{"working":true,"start":"9am","end":"17:00"}]})", e102);
    CHECK(c.status() == 400, "schedule must have 7 days");
    c.call("PUT", "/api/worker/availability", allOff, e102);
    CHECK(c.status() == 200 && std::string(c.json["availability"]["state"].s()) == "off_duty", "all days off = off duty");
    c.call("PUT", "/api/worker/availability", allOn, e102);
    CHECK(std::string(c.json["availability"]["state"].s()) == "on_duty", "00:00-00:00 every day = always on duty");
    c.call("PUT", "/api/worker/availability", R"({"days":[{"working":false},{"working":false},{"working":false},{"working":false},{"working":false},{"working":false},{"working":false}],"leaveFrom":"2030-01-05","leaveTo":"2030-01-01"})", e102);
    CHECK(c.status() == 400, "leave end before start rejected");
    c.call("PUT", "/api/worker/availability", allOff, e102);
    c.call("GET", "/api/workers", "", admin2);
    bool shownOff = false;
    for (const auto &w : c.json["workers"])
        if (std::string(w["workerId"].s()) == "E102")
            shownOff = std::string(w["availability"].s()) == "Off duty" && w["schedule"]["days"].size() == 7;
    CHECK(shownOff, "warden sees E102 as Off duty with the weekly schedule");

    // Off-duty worker: manual assignment needs confirmation; dispatch skips them.
    c.call("POST", "/api/complaints", R"({"title":"Tube light flickers","category":"Electrical","description":"tube light keeps flickering","priority":"Low"})", yashu);
    int flicker = static_cast<int>(c.json["id"].i());
    ++created;
    c.call("PUT", "/api/complaints/" + std::to_string(flicker) + "/status", R"({"status":"Verified"})", admin2);
    c.call("POST", "/api/complaints/assign", R"({"complaintId":)" + std::to_string(flicker) + R"(,"workerId":"E102"})", admin2);
    CHECK(c.status() == 409 && c.str("code") == "OFF_DUTY", "assigning an off-duty worker needs confirmation");
    c.call("GET", "/api/complaints/" + std::to_string(flicker) + "/dispatch", "", admin2);
    CHECK(std::string(c.json["recommended"]["workerId"].s()) == "E101", "dispatch skips off-duty E102 and picks E101");

    // Critical complaint, every electrician unavailable -> off-duty worker is called in.
    c.call("PUT", "/api/workers/status", R"({"workerId":"E101","isActive":false})", admin2);
    c.call("POST", "/api/complaints", R"({"title":"Smoke from switchboard","category":"Electrical","description":"smoke and sparks from switchboard","priority":"High"})", yashu);
    int smoke = static_cast<int>(c.json["id"].i());
    ++created;
    c.call("PUT", "/api/complaints/" + std::to_string(smoke) + "/status", R"({"status":"Verified"})", admin2);
    c.call("GET", "/api/complaints/" + std::to_string(smoke) + "/dispatch", "", admin2);
    CHECK(c.json["success"].b() && std::string(c.json["recommended"]["workerId"].s()) == "E102",
          "critical complaint calls in the off-duty E102");
    c.call("PUT", "/api/workers/status", R"({"workerId":"E101","isActive":true})", admin2);

    // ---------- Campus coordinates & Google directions ----------
    c.call("GET", "/api/locations", "", fresh);
    CHECK(c.status() == 403, "students cannot read campus coordinates");
    c.call("GET", "/api/locations", "", admin2);
    CHECK(c.json["locations"].size() == 12 && !c.json["locations"][6]["hasCoordinates"].b(), "12 locations, no coordinates yet");
    c.call("PUT", "/api/locations/6", R"({"latitude":200,"longitude":78})", admin2);
    CHECK(c.status() == 400, "invalid latitude rejected");

    c.call("POST", "/api/complaints/assign", R"({"complaintId":)" + std::to_string(flicker) + R"(,"workerId":"E102","override":true})", admin2);
    CHECK(c.status() == 200, "override assigns off-duty worker");
    const std::string dirPath = "/api/worker/tasks/" + std::to_string(flicker) + "/directions";
    c.call("POST", dirPath, "", e102);
    CHECK(c.status() == 200 && c.str("source") == "campus-map", "no coordinates -> campus-map fallback");
    CHECK(c.json["distanceMeters"].i() == 200, "fallback uses Dijkstra campus distance (200 m)");

    c.call("PUT", "/api/locations/6", R"({"latitude":30.2680,"longitude":77.9950})", admin2); // Atlantis Hostel
    CHECK(c.status() == 200, "warden sets hostel coordinates");
    c.call("POST", dirPath, R"({"latitude":30.2700,"longitude":77.9900})", e102);
    CHECK(c.str("source") == "google" && c.json["distanceMeters"].i() == 1234, "live location + coordinates -> Google distance");
    CHECK(std::string(c.json["origin"]["label"].s()) == "Your current location", "origin is the worker's live location");
    CHECK(c.str("navigationUrl").find("destination=30.268000,77.995000") != std::string::npos, "Google Maps navigation link");
    CHECK(c.str("attribution").find("Powered by Google") == 0, "Google attribution included");
    fake->fail = true;
    c.call("POST", dirPath, R"({"latitude":30.2700,"longitude":77.9900})", e102);
    CHECK(c.str("source") == "campus-map" && c.str("message").find("403") != std::string::npos,
          "Google error -> fallback with the reason");
    fake->fail = false;
    c.call("POST", "/api/worker/tasks/14/directions", "", e102);
    CHECK(c.status() == 404, "cannot get directions for another worker's task");

    // ---------- Sessions survive a server restart ----------
    {
        hc::Api restarted(dir.string());
        restarted.initialize();
        Client c2{restarted, {}, {}};
        c2.call("GET", "/api/auth/me", "", admin2);
        CHECK(c2.status() == 200, "session still valid after restart (stored hashed in the database)");
    }

    // ---------- Data integrity ----------
    delete apiPtr;
    hc::DataStore check(dir.string());
    check.initialize();
    auto rows = check.loadComplaints();
    CHECK(rows.size() == originalCount + created, "no complaint lost; new ones added");
    bool preserved = true;
    for (const auto &line : originalComplaints)
    {
        auto before = hc::splitPipe(line, 12);
        int id = std::stoi(before[0]);
        auto after = check.findComplaint(id);
        if (!after)
        {
            preserved = false;
            continue;
        }
        const std::string fields[] = {std::to_string(after->id), after->studentId, after->studentName, after->roomNo,
                                      after->hostelName, after->category, after->description, after->priority};
        for (int f = 0; f < 8; ++f)
            if (hc::trim(before[f]) != hc::trim(fields[f]))
                preserved = false;
        if (hc::trim(before[11]) != hc::trim(after->title))
            preserved = false;
    }
    CHECK(preserved, "every imported complaint keeps all original fields (12-field format)");
    CHECK(fs::exists(dir / "complaints.txt"), "original text files are left in place as a backup");

    // ---------- Demo mode ----------
    {
        const fs::path demoDir = fs::temp_directory_path() / "hostelcare_demo_test";
        fs::remove_all(demoDir);
        fs::create_directories(demoDir);
        setEnv("HOSTELCARE_DEMO_MODE", "1");
        hc::Api demo(demoDir.string());
        demo.initialize();
        Client d{demo, {}, {}};
        d.call("GET", "/api/meta");
        CHECK(d.json["demoMode"].b() && d.json["demoAccounts"].size() == 3, "demo accounts listed in meta");
        d.call("POST", "/api/student/login", R"({"studentId":"DEMO-S1","password":"Demo-Student-2026"})");
        std::string demoStudent = d.str("token");
        CHECK(d.status() == 200, "demo student can log in");
        d.call("PUT", "/api/auth/password", R"({"currentPassword":"Demo-Student-2026","newPassword":"Hacked-2026x"})", demoStudent);
        CHECK(d.status() == 403, "demo accounts cannot change password");
        setEnv("HOSTELCARE_DEMO_MODE", "0");
        fs::remove_all(demoDir);
    }

    std::cout << passed << " passed, " << failed << " failed\n";
    fs::remove_all(dir);
    return failed == 0 ? 0 : 1;
}
