#pragma once
// SQLite persistence (data/hostelcare.db). This is the ONLY file that knows
// table and column names. On first start, the old pipe-delimited text files
// in the data folder are imported once; they are left untouched as a backup.
//
// Thread safety: one connection guarded by a recursive mutex. A Transaction
// holds the mutex for its whole lifetime, so no other thread's statement can
// slip into it.

#include "models.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace hc
{
    class DataStore
    {
    public:
        explicit DataStore(std::string dataDirectory);
        ~DataStore();
        DataStore(const DataStore &) = delete;
        DataStore &operator=(const DataStore &) = delete;

        // Opens the database, creates tables, imports text files if the
        // database is new. `importDirectory` defaults to the data directory.
        void initialize(const std::string &importDirectory = "");
        const std::string &directory() const { return dir_; }

        // All-or-nothing group of writes: BEGIN IMMEDIATE ... COMMIT,
        // rolled back automatically if an exception escapes.
        class Transaction
        {
        public:
            explicit Transaction(DataStore &store);
            ~Transaction();
            void commit();

        private:
            DataStore &store_;
            std::unique_lock<std::recursive_mutex> lock_;
            bool done_ = false;
        };

        // Complaints
        std::vector<Complaint> loadComplaints();
        std::optional<Complaint> findComplaint(int id);
        int insertComplaint(Complaint complaint); // assigns and returns the new ID
        void updateComplaint(const Complaint &complaint);

        // Students
        std::vector<Student> loadStudents();
        std::optional<Student> findStudent(const std::string &id);
        void insertStudent(const Student &student);

        // Workers
        std::vector<WorkerRecord> loadWorkers();
        std::optional<WorkerRecord> findWorker(const std::string &id);
        void insertWorker(const WorkerRecord &worker);
        void updateWorker(const WorkerRecord &worker);

        // Admins
        std::vector<AdminRecord> loadAdmins();
        void insertAdmin(const AdminRecord &admin);
        void setAdminPassword(const std::string &adminId, const std::string &hash);

        // Worker / student password hashes ("" if none)
        std::string accountHash(const std::string &role, const std::string &id);
        void setAccountHash(const std::string &role, const std::string &id, const std::string &hash);
        std::vector<std::string> accountIds(const std::string &role);

        // Status history
        std::vector<HistoryEvent> loadHistory();
        std::vector<HistoryEvent> loadHistoryFor(int complaintId);
        void appendHistory(const HistoryEvent &event);

        // Ratings
        std::vector<Feedback> loadFeedback();
        void appendFeedback(const Feedback &feedback);

        // Notifications ("*" recipient = every admin)
        std::vector<Notification> listNotifications(const std::string &role, const std::string &id, int limit);
        int unreadNotifications(const std::string &role, const std::string &id);
        void appendNotification(const Notification &notification);
        int markNotificationsRead(const std::string &role, const std::string &id, int onlyId);

        // Sessions
        void insertSession(const SessionRow &row);
        std::optional<SessionRow> findSession(const std::string &tokenHash);
        void touchSession(const std::string &tokenHash, long long lastSeen);
        void deleteSession(const std::string &tokenHash);
        void deleteUserSessions(const std::string &role, const std::string &userId);
        void deleteExpiredSessions(long long idleCutoff, long long absoluteCutoff);

        // Worker availability
        std::map<std::string, Availability> loadAvailability();
        void saveAvailability(const Availability &availability);

        // Campus locations (IDs match the campus graph)
        std::vector<LocationRecord> loadLocations();
        void ensureLocation(int id, const std::string &name);
        void setLocationCoordinates(int id, bool hasCoordinates, double latitude, double longitude);

    private:
        std::string dir_;
        sqlite3 *db_ = nullptr;
        std::recursive_mutex mutex_;

        void exec(const std::string &sql);
        void createSchema();
        bool isEmpty();
        void importTextFiles(const std::string &directory);
        friend class Statement;
    };
}
