#include "store.h"
#include "util.h"

#include <sqlite3.h>

#include <fstream>
#include <stdexcept>

namespace hc
{
    // ======================= Prepared-statement helper =======================

    class Statement
    {
    public:
        Statement(sqlite3 *db, const char *sql) : db_(db)
        {
            if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK)
                throw std::runtime_error(std::string("SQL prepare failed: ") + sqlite3_errmsg(db) + " in: " + sql);
        }
        ~Statement() { sqlite3_finalize(stmt_); }
        Statement(const Statement &) = delete;
        Statement &operator=(const Statement &) = delete;

        Statement &text(int index, const std::string &value)
        {
            check(sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT));
            return *this;
        }
        Statement &integer(int index, long long value)
        {
            check(sqlite3_bind_int64(stmt_, index, value));
            return *this;
        }
        Statement &real(int index, double value)
        {
            check(sqlite3_bind_double(stmt_, index, value));
            return *this;
        }
        Statement &null(int index)
        {
            check(sqlite3_bind_null(stmt_, index));
            return *this;
        }

        // true = a row is available, false = finished.
        bool step()
        {
            int rc = sqlite3_step(stmt_);
            if (rc == SQLITE_ROW)
                return true;
            if (rc == SQLITE_DONE)
                return false;
            throw std::runtime_error(std::string("SQL step failed: ") + sqlite3_errmsg(db_));
        }
        void run()
        {
            while (step())
            {
            }
        }

        std::string col(int index)
        {
            const unsigned char *value = sqlite3_column_text(stmt_, index);
            return value ? reinterpret_cast<const char *>(value) : "";
        }
        long long colInt(int index) { return sqlite3_column_int64(stmt_, index); }
        double colReal(int index) { return sqlite3_column_double(stmt_, index); }
        bool colNull(int index) { return sqlite3_column_type(stmt_, index) == SQLITE_NULL; }

    private:
        sqlite3 *db_;
        sqlite3_stmt *stmt_ = nullptr;
        void check(int rc)
        {
            if (rc != SQLITE_OK)
                throw std::runtime_error(std::string("SQL bind failed: ") + sqlite3_errmsg(db_));
        }
    };

    namespace
    {
        Complaint readComplaint(Statement &s)
        {
            Complaint c;
            c.id = static_cast<int>(s.colInt(0));
            c.studentId = s.col(1);
            c.studentName = s.col(2);
            c.roomNo = s.col(3);
            c.hostelName = s.col(4);
            c.category = s.col(5);
            c.description = s.col(6);
            c.priority = s.col(7);
            c.status = s.col(8);
            c.workerId = s.col(9).empty() ? "-1" : s.col(9);
            c.feedback = s.col(10);
            c.title = s.col(11);
            return c;
        }

        const char *kComplaintColumns =
            "id, student_id, student_name, room_no, hostel_name, category, description, "
            "priority, status, worker_id, feedback, title";

        std::string encodeSchedule(const Availability &a)
        {
            std::string out;
            for (int d = 0; d < 7; ++d)
            {
                if (d)
                    out += ';';
                out += (a.days[d].working ? "1," : "0,") + std::to_string(a.days[d].start) + "," +
                       std::to_string(a.days[d].end);
            }
            return out;
        }

        void decodeSchedule(const std::string &text, Availability &a)
        {
            std::size_t pos = 0;
            for (int d = 0; d < 7 && pos <= text.size(); ++d)
            {
                std::size_t next = text.find(';', pos);
                std::string part = text.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
                std::size_t c1 = part.find(','), c2 = part.find(',', c1 == std::string::npos ? 0 : c1 + 1);
                if (c1 != std::string::npos && c2 != std::string::npos)
                {
                    try
                    {
                        a.days[d].working = part.substr(0, c1) == "1";
                        a.days[d].start = std::stoi(part.substr(c1 + 1, c2 - c1 - 1));
                        a.days[d].end = std::stoi(part.substr(c2 + 1));
                    }
                    catch (...)
                    {
                        a.days[d] = Shift{};
                    }
                }
                if (next == std::string::npos)
                    break;
                pos = next + 1;
            }
        }

        int toInt(const std::string &text, int fallback)
        {
            try
            {
                std::size_t used = 0;
                int value = std::stoi(trim(text), &used);
                return used == trim(text).size() ? value : fallback;
            }
            catch (...)
            {
                return fallback;
            }
        }

        std::vector<std::string> readLines(const std::string &path)
        {
            std::vector<std::string> lines;
            std::ifstream input(path, std::ios::binary);
            std::string line;
            while (std::getline(input, line))
            {
                stripCarriageReturn(line);
                if (!trim(line).empty())
                    lines.push_back(line);
            }
            return lines;
        }
    }

    // ======================= Setup =======================

    DataStore::DataStore(std::string dataDirectory) : dir_(std::move(dataDirectory)) {}

    DataStore::~DataStore()
    {
        if (db_)
            sqlite3_close(db_);
    }

    void DataStore::exec(const std::string &sql)
    {
        char *error = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK)
        {
            std::string message = error ? error : "unknown error";
            sqlite3_free(error);
            throw std::runtime_error("SQL failed: " + message);
        }
    }

    void DataStore::initialize(const std::string &importDirectory)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        const std::string path = dir_ + "/hostelcare.db";
        if (sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                            nullptr) != SQLITE_OK)
        {
            std::string message = db_ ? sqlite3_errmsg(db_) : "out of memory";
            throw std::runtime_error("Cannot open database " + path + ": " + message +
                                     " (does the data directory exist?)");
        }
        sqlite3_busy_timeout(db_, 5000);
        // Rollback-journal mode works on every filesystem (including WSL's
        // /mnt/c and Docker volumes); this app uses a single connection.
        exec("PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL; PRAGMA foreign_keys=ON;");
        createSchema();
        if (isEmpty())
            importTextFiles(importDirectory.empty() ? dir_ : importDirectory);
    }

    void DataStore::createSchema()
    {
        exec(R"SQL(
CREATE TABLE IF NOT EXISTS complaints (
  id INTEGER PRIMARY KEY,
  student_id TEXT NOT NULL, student_name TEXT NOT NULL, room_no TEXT NOT NULL,
  hostel_name TEXT NOT NULL, category TEXT NOT NULL, description TEXT NOT NULL,
  priority TEXT NOT NULL, status TEXT NOT NULL,
  worker_id TEXT NOT NULL DEFAULT '-1', feedback TEXT NOT NULL DEFAULT '', title TEXT NOT NULL DEFAULT '');
CREATE INDEX IF NOT EXISTS idx_complaints_student ON complaints(student_id);
CREATE INDEX IF NOT EXISTS idx_complaints_worker ON complaints(worker_id);
CREATE INDEX IF NOT EXISTS idx_complaints_status ON complaints(status);

CREATE TABLE IF NOT EXISTS students (
  id TEXT PRIMARY KEY, name TEXT NOT NULL, room TEXT NOT NULL, phone TEXT NOT NULL, hostel TEXT NOT NULL);
CREATE INDEX IF NOT EXISTS idx_students_phone ON students(phone);

CREATE TABLE IF NOT EXISTS workers (
  id TEXT PRIMARY KEY, name TEXT NOT NULL, department TEXT NOT NULL, active INTEGER NOT NULL DEFAULT 1);

CREATE TABLE IF NOT EXISTS admins (
  id TEXT PRIMARY KEY, username TEXT NOT NULL UNIQUE, password_hash TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS accounts (
  role TEXT NOT NULL CHECK (role IN ('worker','student')), user_id TEXT NOT NULL,
  password_hash TEXT NOT NULL, PRIMARY KEY (role, user_id));

CREATE TABLE IF NOT EXISTS status_history (
  seq INTEGER PRIMARY KEY AUTOINCREMENT, complaint_id INTEGER NOT NULL,
  status TEXT NOT NULL, timestamp TEXT NOT NULL, actor TEXT NOT NULL DEFAULT '');
CREATE INDEX IF NOT EXISTS idx_history_complaint ON status_history(complaint_id);

CREATE TABLE IF NOT EXISTS feedback (
  complaint_id INTEGER PRIMARY KEY, student_id TEXT NOT NULL, worker_id TEXT NOT NULL,
  rating INTEGER NOT NULL CHECK (rating BETWEEN 1 AND 5), comment TEXT NOT NULL DEFAULT '',
  timestamp TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS notifications (
  id INTEGER PRIMARY KEY AUTOINCREMENT, role TEXT NOT NULL, recipient_id TEXT NOT NULL,
  complaint_id INTEGER NOT NULL DEFAULT 0, message TEXT NOT NULL, timestamp TEXT NOT NULL,
  is_read INTEGER NOT NULL DEFAULT 0);
CREATE INDEX IF NOT EXISTS idx_notifications_recipient ON notifications(role, recipient_id);

CREATE TABLE IF NOT EXISTS sessions (
  token_hash TEXT PRIMARY KEY, role TEXT NOT NULL, user_id TEXT NOT NULL,
  created_at INTEGER NOT NULL, last_seen INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(role, user_id);

CREATE TABLE IF NOT EXISTS worker_availability (
  worker_id TEXT PRIMARY KEY, schedule TEXT NOT NULL, leave_from TEXT NOT NULL DEFAULT '',
  leave_to TEXT NOT NULL DEFAULT '', note TEXT NOT NULL DEFAULT '', updated_at TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS locations (
  id INTEGER PRIMARY KEY, name TEXT NOT NULL, latitude REAL, longitude REAL);
)SQL");
    }

    bool DataStore::isEmpty()
    {
        Statement s(db_, "SELECT (SELECT COUNT(*) FROM complaints) + (SELECT COUNT(*) FROM students) + "
                         "(SELECT COUNT(*) FROM workers) + (SELECT COUNT(*) FROM admins)");
        return s.step() && s.colInt(0) == 0;
    }

    // One-time migration from the Phase 3 text files. Runs in one
    // transaction: either everything is imported or nothing is.
    void DataStore::importTextFiles(const std::string &directory)
    {
        Transaction tx(*this);
        int imported = 0;

        for (const auto &line : readLines(directory + "/complaints.txt"))
        {
            auto f = splitPipe(line, 12);
            int id = toInt(f[0], -1);
            if (id <= 0)
                continue;
            Statement s(db_, "INSERT OR REPLACE INTO complaints VALUES (?,?,?,?,?,?,?,?,?,?,?,?)");
            s.integer(1, id);
            for (int i = 1; i < 12; ++i)
                s.text(i + 1, i == 9 ? (trim(f[i]).empty() ? "-1" : trim(f[i])) : f[i]);
            s.run();
            ++imported;
        }
        for (const auto &line : readLines(directory + "/students.txt"))
        {
            auto f = splitPipe(line, 5);
            if (trim(f[0]).empty())
                continue;
            Statement s(db_, "INSERT OR REPLACE INTO students VALUES (?,?,?,?,?)");
            s.text(1, trim(f[0])).text(2, f[1]).text(3, f[2]).text(4, trim(f[3])).text(5, f[4]).run();
        }
        for (const auto &line : readLines(directory + "/workers.txt"))
        {
            auto f = splitPipe(line, 4);
            if (trim(f[0]).empty())
                continue;
            Statement s(db_, "INSERT OR REPLACE INTO workers VALUES (?,?,?,?)");
            s.text(1, trim(f[0])).text(2, trim(f[1])).text(3, trim(f[2])).integer(4, trim(f[3]) == "1").run();
        }
        for (const auto &line : readLines(directory + "/admins.txt"))
        {
            auto f = splitPipe(line, 3);
            if (trim(f[0]).empty())
                continue;
            Statement s(db_, "INSERT OR REPLACE INTO admins VALUES (?,?,?)");
            s.text(1, trim(f[0])).text(2, trim(f[1])).text(3, trim(f[2])).run();
        }
        for (const char *file : {"worker_accounts.txt", "student_accounts.txt"})
        {
            std::string role = std::string(file).rfind("worker", 0) == 0 ? "worker" : "student";
            for (const auto &line : readLines(directory + "/" + file))
            {
                auto f = splitPipe(line, 2);
                if (trim(f[0]).empty() || trim(f[1]).empty())
                    continue;
                Statement s(db_, "INSERT OR REPLACE INTO accounts VALUES (?,?,?)");
                s.text(1, role).text(2, trim(f[0])).text(3, trim(f[1])).run();
            }
        }
        for (const auto &line : readLines(directory + "/status_history.txt"))
        {
            auto f = splitPipe(line, 4);
            int id = toInt(f[0], -1);
            if (id <= 0)
                continue;
            Statement s(db_, "INSERT INTO status_history (complaint_id, status, timestamp, actor) VALUES (?,?,?,?)");
            s.integer(1, id).text(2, trim(f[1])).text(3, trim(f[2])).text(4, trim(f[3])).run();
        }
        for (const auto &line : readLines(directory + "/feedback.txt"))
        {
            auto f = splitPipe(line, 6);
            int id = toInt(f[0], -1), rating = toInt(f[3], 0);
            if (id <= 0 || rating < 1 || rating > 5)
                continue;
            Statement s(db_, "INSERT OR REPLACE INTO feedback VALUES (?,?,?,?,?,?)");
            s.integer(1, id).text(2, trim(f[1])).text(3, trim(f[2])).integer(4, rating).text(5, f[4]).text(6, trim(f[5])).run();
        }
        for (const auto &line : readLines(directory + "/notifications.txt"))
        {
            auto f = splitPipe(line, 7);
            int id = toInt(f[0], -1);
            if (id <= 0)
                continue;
            Statement s(db_, "INSERT OR REPLACE INTO notifications VALUES (?,?,?,?,?,?,?)");
            s.integer(1, id).text(2, trim(f[1])).text(3, trim(f[2])).integer(4, toInt(f[3], 0));
            s.text(5, f[4]).text(6, trim(f[5])).integer(7, trim(f[6]) == "1").run();
        }
        tx.commit();
        (void)imported;
    }

    // ======================= Transaction =======================

    DataStore::Transaction::Transaction(DataStore &store) : store_(store), lock_(store.mutex_)
    {
        store_.exec("BEGIN IMMEDIATE");
    }

    DataStore::Transaction::~Transaction()
    {
        if (!done_)
        {
            try
            {
                store_.exec("ROLLBACK");
            }
            catch (...)
            {
            }
        }
    }

    void DataStore::Transaction::commit()
    {
        store_.exec("COMMIT");
        done_ = true;
    }

    // ======================= Complaints =======================

    std::vector<Complaint> DataStore::loadComplaints()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, (std::string("SELECT ") + kComplaintColumns + " FROM complaints ORDER BY id").c_str());
        std::vector<Complaint> list;
        while (s.step())
            list.push_back(readComplaint(s));
        return list;
    }

    std::optional<Complaint> DataStore::findComplaint(int id)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, (std::string("SELECT ") + kComplaintColumns + " FROM complaints WHERE id = ?").c_str());
        s.integer(1, id);
        if (!s.step())
            return std::nullopt;
        return readComplaint(s);
    }

    int DataStore::insertComplaint(Complaint c)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement next(db_, "SELECT COALESCE(MAX(id), 0) + 1 FROM complaints");
        next.step();
        c.id = static_cast<int>(next.colInt(0));
        Statement s(db_, "INSERT INTO complaints VALUES (?,?,?,?,?,?,?,?,?,?,?,?)");
        s.integer(1, c.id).text(2, c.studentId).text(3, c.studentName).text(4, c.roomNo).text(5, c.hostelName);
        s.text(6, c.category).text(7, c.description).text(8, c.priority).text(9, c.status);
        s.text(10, c.workerId.empty() ? "-1" : c.workerId).text(11, c.feedback).text(12, c.title).run();
        return c.id;
    }

    void DataStore::updateComplaint(const Complaint &c)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "UPDATE complaints SET student_id=?, student_name=?, room_no=?, hostel_name=?, category=?, "
                         "description=?, priority=?, status=?, worker_id=?, feedback=?, title=? WHERE id=?");
        s.text(1, c.studentId).text(2, c.studentName).text(3, c.roomNo).text(4, c.hostelName).text(5, c.category);
        s.text(6, c.description).text(7, c.priority).text(8, c.status).text(9, c.workerId.empty() ? "-1" : c.workerId);
        s.text(10, c.feedback).text(11, c.title).integer(12, c.id).run();
        if (sqlite3_changes(db_) != 1)
            throw std::runtime_error("Complaint " + std::to_string(c.id) + " was not updated");
    }

    // ======================= Students =======================

    std::vector<Student> DataStore::loadStudents()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT id, name, room, phone, hostel FROM students ORDER BY id");
        std::vector<Student> list;
        while (s.step())
            list.push_back({s.col(0), s.col(1), s.col(2), s.col(3), s.col(4)});
        return list;
    }

    std::optional<Student> DataStore::findStudent(const std::string &id)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT id, name, room, phone, hostel FROM students WHERE id = ?");
        s.text(1, id);
        if (!s.step())
            return std::nullopt;
        return Student{s.col(0), s.col(1), s.col(2), s.col(3), s.col(4)};
    }

    void DataStore::insertStudent(const Student &st)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "INSERT INTO students VALUES (?,?,?,?,?)");
        s.text(1, st.id).text(2, st.name).text(3, st.room).text(4, st.phone).text(5, st.hostel).run();
    }

    // ======================= Workers =======================

    std::vector<WorkerRecord> DataStore::loadWorkers()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT id, name, department, active FROM workers ORDER BY id");
        std::vector<WorkerRecord> list;
        while (s.step())
            list.push_back({s.col(0), s.col(1), s.col(2), s.colInt(3) != 0});
        return list;
    }

    std::optional<WorkerRecord> DataStore::findWorker(const std::string &id)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT id, name, department, active FROM workers WHERE id = ?");
        s.text(1, id);
        if (!s.step())
            return std::nullopt;
        return WorkerRecord{s.col(0), s.col(1), s.col(2), s.colInt(3) != 0};
    }

    void DataStore::insertWorker(const WorkerRecord &w)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "INSERT INTO workers VALUES (?,?,?,?)");
        s.text(1, w.id).text(2, w.name).text(3, w.department).integer(4, w.active).run();
    }

    void DataStore::updateWorker(const WorkerRecord &w)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "UPDATE workers SET name=?, department=?, active=? WHERE id=?");
        s.text(1, w.name).text(2, w.department).integer(3, w.active).text(4, w.id).run();
    }

    // ======================= Admins & accounts =======================

    std::vector<AdminRecord> DataStore::loadAdmins()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT id, username, password_hash FROM admins ORDER BY id");
        std::vector<AdminRecord> list;
        while (s.step())
            list.push_back({s.col(0), s.col(1), s.col(2)});
        return list;
    }

    void DataStore::insertAdmin(const AdminRecord &a)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "INSERT INTO admins VALUES (?,?,?)");
        s.text(1, a.id).text(2, a.username).text(3, a.passwordHash).run();
    }

    void DataStore::setAdminPassword(const std::string &adminId, const std::string &hash)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "UPDATE admins SET password_hash = ? WHERE id = ?");
        s.text(1, hash).text(2, adminId).run();
    }

    std::string DataStore::accountHash(const std::string &role, const std::string &id)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT password_hash FROM accounts WHERE role = ? AND user_id = ?");
        s.text(1, role).text(2, id);
        return s.step() ? s.col(0) : "";
    }

    void DataStore::setAccountHash(const std::string &role, const std::string &id, const std::string &hash)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "INSERT INTO accounts (role, user_id, password_hash) VALUES (?,?,?) "
                         "ON CONFLICT(role, user_id) DO UPDATE SET password_hash = excluded.password_hash");
        s.text(1, role).text(2, id).text(3, hash).run();
    }

    std::vector<std::string> DataStore::accountIds(const std::string &role)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT user_id FROM accounts WHERE role = ?");
        s.text(1, role);
        std::vector<std::string> ids;
        while (s.step())
            ids.push_back(s.col(0));
        return ids;
    }

    // ======================= History & feedback =======================

    std::vector<HistoryEvent> DataStore::loadHistory()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT complaint_id, status, timestamp, actor FROM status_history ORDER BY seq");
        std::vector<HistoryEvent> list;
        while (s.step())
            list.push_back({static_cast<int>(s.colInt(0)), s.col(1), s.col(2), s.col(3)});
        return list;
    }

    std::vector<HistoryEvent> DataStore::loadHistoryFor(int complaintId)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT complaint_id, status, timestamp, actor FROM status_history "
                         "WHERE complaint_id = ? ORDER BY seq");
        s.integer(1, complaintId);
        std::vector<HistoryEvent> list;
        while (s.step())
            list.push_back({static_cast<int>(s.colInt(0)), s.col(1), s.col(2), s.col(3)});
        return list;
    }

    void DataStore::appendHistory(const HistoryEvent &e)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "INSERT INTO status_history (complaint_id, status, timestamp, actor) VALUES (?,?,?,?)");
        s.integer(1, e.complaintId).text(2, e.status).text(3, e.timestamp).text(4, e.actor).run();
    }

    std::vector<Feedback> DataStore::loadFeedback()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT complaint_id, student_id, worker_id, rating, comment, timestamp FROM feedback "
                         "ORDER BY timestamp");
        std::vector<Feedback> list;
        while (s.step())
            list.push_back({static_cast<int>(s.colInt(0)), s.col(1), s.col(2), static_cast<int>(s.colInt(3)),
                            s.col(4), s.col(5)});
        return list;
    }

    void DataStore::appendFeedback(const Feedback &f)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "INSERT INTO feedback VALUES (?,?,?,?,?,?)");
        s.integer(1, f.complaintId).text(2, f.studentId).text(3, f.workerId).integer(4, f.rating);
        s.text(5, f.comment).text(6, f.timestamp).run();
    }

    // ======================= Notifications =======================

    static const char *kRecipientFilter = "role = ?1 AND (recipient_id = ?2 OR (?1 = 'admin' AND recipient_id = '*'))";

    std::vector<Notification> DataStore::listNotifications(const std::string &role, const std::string &id, int limit)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, (std::string("SELECT id, role, recipient_id, complaint_id, message, timestamp, is_read "
                                      "FROM notifications WHERE ") +
                          kRecipientFilter + " ORDER BY id DESC LIMIT ?3")
                             .c_str());
        s.text(1, role).text(2, id).integer(3, limit);
        std::vector<Notification> list;
        while (s.step())
            list.push_back({static_cast<int>(s.colInt(0)), s.col(1), s.col(2), static_cast<int>(s.colInt(3)),
                            s.col(4), s.col(5), s.colInt(6) != 0});
        return list;
    }

    int DataStore::unreadNotifications(const std::string &role, const std::string &id)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, (std::string("SELECT COUNT(*) FROM notifications WHERE ") + kRecipientFilter +
                          " AND is_read = 0")
                             .c_str());
        s.text(1, role).text(2, id);
        return s.step() ? static_cast<int>(s.colInt(0)) : 0;
    }

    void DataStore::appendNotification(const Notification &n)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "INSERT INTO notifications (role, recipient_id, complaint_id, message, timestamp, is_read) "
                         "VALUES (?,?,?,?,?,0)");
        s.text(1, n.role).text(2, n.recipientId).integer(3, n.complaintId).text(4, n.message).text(5, n.timestamp).run();
    }

    int DataStore::markNotificationsRead(const std::string &role, const std::string &id, int onlyId)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, (std::string("UPDATE notifications SET is_read = 1 WHERE ") + kRecipientFilter +
                          " AND is_read = 0 AND (?3 = 0 OR id = ?3)")
                             .c_str());
        s.text(1, role).text(2, id).integer(3, onlyId).run();
        return sqlite3_changes(db_);
    }

    // ======================= Sessions =======================

    void DataStore::insertSession(const SessionRow &r)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "INSERT INTO sessions VALUES (?,?,?,?,?)");
        s.text(1, r.tokenHash).text(2, r.role).text(3, r.userId).integer(4, r.createdAt).integer(5, r.lastSeen).run();
    }

    std::optional<SessionRow> DataStore::findSession(const std::string &tokenHash)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT token_hash, role, user_id, created_at, last_seen FROM sessions WHERE token_hash = ?");
        s.text(1, tokenHash);
        if (!s.step())
            return std::nullopt;
        return SessionRow{s.col(0), s.col(1), s.col(2), s.colInt(3), s.colInt(4)};
    }

    void DataStore::touchSession(const std::string &tokenHash, long long lastSeen)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "UPDATE sessions SET last_seen = ? WHERE token_hash = ?");
        s.integer(1, lastSeen).text(2, tokenHash).run();
    }

    void DataStore::deleteSession(const std::string &tokenHash)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "DELETE FROM sessions WHERE token_hash = ?");
        s.text(1, tokenHash).run();
    }

    void DataStore::deleteUserSessions(const std::string &role, const std::string &userId)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "DELETE FROM sessions WHERE role = ? AND user_id = ?");
        s.text(1, role).text(2, userId).run();
    }

    void DataStore::deleteExpiredSessions(long long idleCutoff, long long absoluteCutoff)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "DELETE FROM sessions WHERE last_seen < ? OR created_at < ?");
        s.integer(1, idleCutoff).integer(2, absoluteCutoff).run();
    }

    // ======================= Availability =======================

    std::map<std::string, Availability> DataStore::loadAvailability()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT worker_id, schedule, leave_from, leave_to, note, updated_at FROM worker_availability");
        std::map<std::string, Availability> result;
        while (s.step())
        {
            Availability a;
            a.workerId = s.col(0);
            decodeSchedule(s.col(1), a);
            a.leaveFrom = s.col(2);
            a.leaveTo = s.col(3);
            a.note = s.col(4);
            a.updatedAt = s.col(5);
            a.configured = true;
            result[a.workerId] = a;
        }
        return result;
    }

    void DataStore::saveAvailability(const Availability &a)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "INSERT INTO worker_availability VALUES (?,?,?,?,?,?) ON CONFLICT(worker_id) DO UPDATE SET "
                         "schedule=excluded.schedule, leave_from=excluded.leave_from, leave_to=excluded.leave_to, "
                         "note=excluded.note, updated_at=excluded.updated_at");
        s.text(1, a.workerId).text(2, encodeSchedule(a)).text(3, a.leaveFrom).text(4, a.leaveTo);
        s.text(5, a.note).text(6, a.updatedAt).run();
    }

    // ======================= Locations =======================

    std::vector<LocationRecord> DataStore::loadLocations()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "SELECT id, name, latitude, longitude FROM locations ORDER BY id");
        std::vector<LocationRecord> list;
        while (s.step())
        {
            LocationRecord l;
            l.id = static_cast<int>(s.colInt(0));
            l.name = s.col(1);
            l.hasCoordinates = !s.colNull(2) && !s.colNull(3);
            l.latitude = l.hasCoordinates ? s.colReal(2) : 0;
            l.longitude = l.hasCoordinates ? s.colReal(3) : 0;
            list.push_back(l);
        }
        return list;
    }

    void DataStore::ensureLocation(int id, const std::string &name)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "INSERT INTO locations (id, name) VALUES (?, ?) ON CONFLICT(id) DO UPDATE SET name = excluded.name");
        s.integer(1, id).text(2, name).run();
    }

    void DataStore::setLocationCoordinates(int id, bool hasCoordinates, double latitude, double longitude)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Statement s(db_, "UPDATE locations SET latitude = ?, longitude = ? WHERE id = ?");
        if (hasCoordinates)
            s.real(1, latitude).real(2, longitude);
        else
            s.null(1).null(2);
        s.integer(3, id).run();
        if (sqlite3_changes(db_) != 1)
            throw std::runtime_error("Location " + std::to_string(id) + " not found");
    }
}
