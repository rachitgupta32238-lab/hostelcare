#pragma once
// Plain data records. Each maps 1:1 to a line in a file under data/.

#include <string>
#include <vector>

namespace hc
{
    // data/complaints.txt — the 12-field format used by the API since Phase 3:
    // id|studentId|studentName|roomNo|hostelName|category|description|
    // priority|status|workerId|feedback|title
    // Legacy 11-field lines (no title) are read with title = "".
    struct Complaint
    {
        int id = 0;
        std::string studentId, studentName, roomNo, hostelName;
        std::string category, description, priority, status;
        std::string workerId = "-1"; // "-1" = unassigned (legacy convention)
        std::string feedback;        // worker resolution / escalation note
        std::string title;

        bool isAssigned() const { return !workerId.empty() && workerId != "-1"; }
    };

    // data/students.txt: id|name|room|phone|hostel
    struct Student
    {
        std::string id, name, room, phone, hostel;
    };

    // data/workers.txt: id|name|department|active(1/0)
    struct WorkerRecord
    {
        std::string id, name, department;
        bool active = true;
    };

    // data/admins.txt: adminId|username|passwordHash
    struct AdminRecord
    {
        std::string id, username, passwordHash;
    };

    // data/worker_accounts.txt and data/student_accounts.txt: id|passwordHash
    struct Account
    {
        std::string id, passwordHash;
    };

    // data/status_history.txt: complaintId|status|timestamp|actor
    // (actor is optional; older lines have only three fields)
    struct HistoryEvent
    {
        int complaintId = 0;
        std::string status, timestamp, actor;
    };

    // data/feedback.txt: complaintId|studentId|workerId|rating|comment|timestamp
    struct Feedback
    {
        int complaintId = 0;
        std::string studentId, workerId;
        int rating = 0;
        std::string comment, timestamp;
    };

    // Weekly working hours set by a worker. Day 0 = Monday ... 6 = Sunday.
    // Times are minutes after midnight; end <= start means the shift ends
    // after midnight (e.g. a 22:00-06:00 security shift).
    struct Shift
    {
        bool working = false;
        int start = 9 * 60;
        int end = 17 * 60;
    };
    struct Availability
    {
        std::string workerId;
        Shift days[7];
        std::string leaveFrom, leaveTo; // YYYY-MM-DD, inclusive; empty = no leave
        std::string note;
        std::string updatedAt;
        bool configured = false; // false = worker never set a schedule
    };

    // A campus place. Coordinates are optional until the warden sets them.
    struct LocationRecord
    {
        int id = 0;
        std::string name;
        bool hasCoordinates = false;
        double latitude = 0, longitude = 0;
    };

    // A persisted login session. Only a SHA-256 hash of the token is stored.
    struct SessionRow
    {
        std::string tokenHash, role, userId;
        long long createdAt = 0, lastSeen = 0; // Unix seconds
    };

    // data/notifications.txt: id|role|recipientId|complaintId|message|timestamp|read
    // role is admin/worker/student; recipientId "*" means every admin.
    struct Notification
    {
        int id = 0;
        std::string role, recipientId;
        int complaintId = 0;
        std::string message, timestamp;
        bool read = false;
    };
}
