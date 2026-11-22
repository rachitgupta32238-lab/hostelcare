#pragma once
// All HostelCare REST endpoints. Each handler:
//   1. authenticates the caller (role check on the SERVER, never the UI),
//   2. validates input,
//   3. takes the data lock, loads, modifies and saves records,
//   4. returns JSON.
// Handlers throw ApiError for expected failures (400/401/403/404/409/429).

#include "core/domain.h"
#include "core/models.h"
#include "core/routes_client.h"
#include "core/sessions.h"
#include "core/store.h"
#include "core/util.h"
#include "http_types.h"

#include "crow/json.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace hc
{
    using Json = crow::json::wvalue;

    struct Reply
    {
        int status = 200;
        Json body;
    };

    // Typed access to a JSON request body with readable 400 errors.
    class Body
    {
    public:
        explicit Body(const std::string &raw);
        bool has(const char *key) const;
        std::string text(const char *key, bool required = true) const;
        int integer(const char *key) const;
        bool boolean(const char *key, bool fallback) const;
        double number(const char *key) const;
        // Direct access for nested values (lists/objects).
        const crow::json::rvalue &raw() const { return value_; }
        bool isEmpty() const { return empty_; }

    private:
        crow::json::rvalue value_;
        bool empty_ = false;
    };

    class Api
    {
    public:
        explicit Api(const std::string &dataDirectory);

        // Opens the database (importing old text files once), creates the
        // first Admin from env vars, seeds campus locations and demo accounts.
        // Reads: HOSTELCARE_IMPORT_DIR, HOSTELCARE_DEMO_MODE,
        // HOSTELCARE_GOOGLE_MAPS_SERVER_KEY, HOSTELCARE_TRAVEL_MODE.
        void initialize();
        // Tests inject a fake; production uses Google when a key is set.
        void setRoutesClient(std::unique_ptr<RoutesClient> client) { routes_ = std::move(client); }

        using Handler = std::function<Reply(Api &, const HttpRequest &)>;
        struct Route
        {
            std::string method;
            std::string pattern; // may contain <int>
            Handler handler;
        };
        static const std::vector<Route> &routes();

        // Runs one route handler and converts errors to JSON responses.
        HttpResponse handle(const Route &route, const HttpRequest &request);
        // Finds the matching route by method + path (used by tests).
        HttpResponse dispatch(HttpRequest request);

        // ---------------- Auth ----------------
        Reply adminLogin(const HttpRequest &);
        Reply workerLogin(const HttpRequest &);
        Reply studentLogin(const HttpRequest &);
        Reply studentActivate(const HttpRequest &);
        Reply studentRegister(const HttpRequest &);
        Reply me(const HttpRequest &);
        Reply logout(const HttpRequest &);
        Reply changeOwnPassword(const HttpRequest &);

        // ---------------- Complaints ----------------
        Reply listComplaints(const HttpRequest &);
        Reply createComplaint(const HttpRequest &);
        Reply complaintHistory(const HttpRequest &);
        Reply updateStatus(const HttpRequest &);
        Reply updatePriority(const HttpRequest &);
        Reply assignComplaint(const HttpRequest &);
        Reply submitFeedback(const HttpRequest &);

        // ---------------- Admin: workers ----------------
        Reply listWorkers(const HttpRequest &);
        Reply addWorker(const HttpRequest &);
        Reply editWorker(const HttpRequest &);
        Reply setWorkerStatus(const HttpRequest &);
        Reply setWorkerPassword(const HttpRequest &);

        // ---------------- Admin: smart features ----------------
        Reply dashboard(const HttpRequest &);
        Reply analytics(const HttpRequest &);
        Reply dispatchOptions(const HttpRequest &);
        Reply autoAssign(const HttpRequest &);
        Reply dispatchQueue(const HttpRequest &);
        Reply runDispatch(const HttpRequest &);
        Reply listFeedback(const HttpRequest &);
        Reply listLocations(const HttpRequest &);
        Reply updateLocation(const HttpRequest &);

        // ---------------- Worker portal ----------------
        Reply workerTasks(const HttpRequest &);
        Reply workerNextTask(const HttpRequest &);
        Reply workerCompleted(const HttpRequest &);
        Reply workerAccept(const HttpRequest &);
        Reply workerStart(const HttpRequest &);
        Reply workerResolve(const HttpRequest &);
        Reply workerEscalate(const HttpRequest &);
        Reply getAvailability(const HttpRequest &);
        Reply saveAvailability(const HttpRequest &);
        Reply taskDirections(const HttpRequest &);

        // ---------------- Notifications, routes, meta ----------------
        Reply listNotifications(const HttpRequest &);
        Reply markNotificationsRead(const HttpRequest &);
        Reply routeLocations(const HttpRequest &);
        Reply shortestRoute(const HttpRequest &);
        Reply meta(const HttpRequest &);
        Reply health(const HttpRequest &);

    private:
        DataStore store_; // declared first: sessions_ uses it
        SessionManager sessions_;
        LoginThrottle throttle_;
        std::mutex dataMutex_;
        std::unique_ptr<RoutesClient> routes_;
        bool demoMode_ = false;

        // Everything a complaint JSON needs, loaded once per request.
        struct Snapshot
        {
            std::vector<Complaint> complaints;
            std::map<std::string, WorkerRecord> workers;
            std::map<int, std::vector<HistoryEvent>> history;
            std::map<int, Feedback> feedback;
            std::map<std::string, Availability> availability;
        };
        Snapshot loadSnapshot();

        Principal requireAuth(const HttpRequest &request, std::initializer_list<Role> allowed);
        static std::string bearerToken(const HttpRequest &request);
        static std::string actorOf(const Principal &p);

        Json complaintJson(const Complaint &c, const Snapshot &s, Role viewer) const;
        Json userJson(const Principal &p);
        Json availabilityJson(const Availability *a, const DutyStatus &duty) const;
        bool isDemoAccount(const Principal &p) const;
        Json routeJson(const RouteInfo &route) const;
        static std::time_t submittedAt(const Snapshot &s, int complaintId);
        std::vector<WorkerLoad> workerLoads(const Snapshot &s) const;
        static Complaint &findComplaint(std::vector<Complaint> &list, int id);
        static int idFromBody(const Body &body, const char *key);

        // Must be called with dataMutex_ held.
        void recordHistory(int complaintId, const std::string &status, const std::string &actor);
        void notify(const std::string &role, const std::string &recipientId, int complaintId,
                    const std::string &message);
        void notifyAdmins(int complaintId, const std::string &message);
        // Assignment is split so the file is saved BEFORE history and
        // notifications are written. Returns the previous worker ID ("" if none).
        static std::string assignInMemory(Complaint &c, const WorkerRecord &worker);
        void announceAssignment(const Complaint &c, const WorkerRecord &worker,
                                const std::string &previousWorker, const std::string &actor);

        Reply workerTransition(const HttpRequest &request, const std::string &from1,
                               const std::string &from2, const std::string &to, bool needsNote);
        Reply loginWithAccount(Role role, const std::string &id, const std::string &password);
    };
}
