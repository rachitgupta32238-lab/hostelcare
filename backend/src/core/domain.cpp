#include "domain.h"
#include "util.h"

#include <algorithm>
#include <cstdio>
#include <queue>

// Legacy modules (backend/legacy). Their headers use `using namespace std;`,
// so they are included only in this .cpp file to keep that contained.
#include "campusgraph.h"
#include "dispatchmanager.h"
#include "priority.h"
#include "worker.h"

namespace hc
{
    // ======================= Departments =======================

    const std::vector<std::string> &departments()
    {
        static const std::vector<std::string> list = {
            "Electrical", "Plumbing", "IT Support", "Security", "General Maintenance"};
        return list;
    }

    const std::vector<std::string> &complaintCategories()
    {
        static const std::vector<std::string> list = {
            "Electrical", "Plumbing", "Cleaning", "Furniture", "Internet", "Security", "Other"};
        return list;
    }

    std::string canonicalDepartment(const std::string &value)
    {
        for (const auto &d : departments())
            if (iequals(d, value))
                return d;
        return "";
    }

    std::string departmentForComplaint(const std::string &category, const std::string &description)
    {
        const std::string cat = toLower(trim(category));

        // 1. Explicit categories from the student form.
        if (cat == "electrical")
            return "Electrical";
        if (cat == "plumbing")
            return "Plumbing";
        if (cat == "internet" || cat == "it support" || cat == "it")
            return "IT Support";
        if (cat == "security")
            return "Security";
        if (cat == "cleaning" || cat == "furniture")
            return "General Maintenance";
        if (!canonicalDepartment(category).empty())
            return canonicalDepartment(category);

        // 2. "Other" / free text: keyword scan (whole words only).
        const std::string text = cat + " " + toLower(description);
        auto any = [&](std::initializer_list<const char *> words)
        {
            for (const char *w : words)
                if (containsWord(text, w))
                    return true;
            return false;
        };
        if (any({"electric", "electrical", "fan", "light", "bulb", "switch", "socket",
                 "power", "wire", "wiring", "electricity"}))
            return "Electrical";
        if (any({"plumbing", "water", "tap", "pipe", "drain", "toilet", "leak", "leakage", "flush"}))
            return "Plumbing";
        if (any({"internet", "wifi", "wi-fi", "network", "lan", "computer", "router"}))
            return "IT Support";
        if (any({"security", "guard", "theft", "stolen", "cctv"}))
            return "Security";
        return "General Maintenance";
    }

    // ======================= Priority =======================

    int priorityRank(const std::string &priority)
    {
        std::string p = toLower(trim(priority));
        if (p == "critical" || p == "urgent")
            return 4;
        if (p == "high")
            return 3;
        if (p == "medium")
            return 2;
        if (p == "low")
            return 1;
        return 0;
    }

    std::string priorityName(int rank)
    {
        switch (rank)
        {
        case 4:
            return "Critical";
        case 3:
            return "High";
        case 2:
            return "Medium";
        default:
            return "Low";
        }
    }

    std::string canonicalPriority(const std::string &value)
    {
        int rank = priorityRank(value);
        return rank == 0 ? "" : priorityName(rank);
    }

    std::string suggestedPriority(const std::string &category, const std::string &description)
    {
        return ::assignComplaintPriority(category, description); // legacy priorityc.cpp
    }

    int effectivePriorityRank(const std::string &priority, std::time_t submittedAt, std::time_t now)
    {
        int rank = std::max(1, priorityRank(priority));
        if (rank >= 3 || submittedAt < 0 || now <= submittedAt)
            return rank;
        long long waitedHours = static_cast<long long>(now - submittedAt) / 3600;
        int boost = static_cast<int>(waitedHours / 48);
        return std::min(3, rank + boost);
    }

    // ======================= Status machine =======================

    const std::vector<std::string> &allStatuses()
    {
        static const std::vector<std::string> list = {
            "Pending", "Verified", "Forwarded", "Assigned", "Accepted",
            "In Progress", "Resolved", "Rejected", "Escalated"};
        return list;
    }

    bool isActiveWorkStatus(const std::string &s)
    {
        return s == "Assigned" || s == "Accepted" || s == "In Progress";
    }

    bool isOpenStatus(const std::string &s)
    {
        return s != "Resolved" && s != "Rejected";
    }

    bool canAssignFrom(const std::string &s)
    {
        return s == "Verified" || s == "Forwarded" || s == "Escalated" || isActiveWorkStatus(s);
    }

    bool canAutoDispatchFrom(const std::string &s)
    {
        return s == "Verified" || s == "Forwarded";
    }

    std::vector<std::string> adminTransitions(const std::string &from)
    {
        if (from == "Pending")
            return {"Verified", "Rejected"};
        if (from == "Verified")
            return {"Forwarded", "Pending", "Rejected"};
        if (from == "Forwarded")
            return {"Verified", "Rejected"};
        if (from == "Escalated")
            return {"Forwarded", "Rejected"};
        if (from == "Assigned")
            return {"Forwarded", "Rejected"}; // Forwarded = unassign
        if (from == "Accepted" || from == "In Progress")
            return {"Forwarded"};
        if (from == "Resolved" || from == "Rejected")
            return {"Pending"}; // reopen
        return {"Pending", "Verified", "Rejected"}; // unknown legacy status
    }

    // ======================= Campus graph =======================

    static CampusGraph &graph()
    {
        static CampusGraph g = []
        {
            CampusGraph built;
            built.loadDefaultCampusMap();
            return built;
        }();
        return g;
    }

    std::vector<Location> campusLocations()
    {
        std::vector<Location> list;
        for (int i = 0; i < graph().getLocationCount(); ++i)
            list.push_back({i, graph().getLocationName(i)});
        return list;
    }

    std::string locationName(int id)
    {
        return graph().getLocationName(id);
    }

    RouteInfo shortestRoute(int from, int to)
    {
        RouteInfo info;
        int count = graph().getLocationCount();
        if (from < 0 || to < 0 || from >= count || to >= count)
            return info; // invalid IDs: no route (avoid legacy console output)
        ShortestPathResult r = graph().findShortestPath(from, to);
        info.exists = r.pathExists;
        info.distance = r.pathExists ? r.totalDistance : -1;
        info.pathIds = r.path;
        for (int id : r.path)
            info.pathNames.push_back(graph().getLocationName(id));
        return info;
    }

    const std::vector<std::string> &knownHostels()
    {
        static const std::vector<std::string> list = {
            "Atlantis Hostel", "SC Bose Hostel", "Bhagat Singh Hostel", "Sardar Patel Hostel"};
        return list;
    }

    int locationForHostel(const std::string &hostelName)
    {
        // Same matching idea as legacy workerportal.cpp complaintLocation().
        std::string h = toLower(hostelName);
        if (h.find("atlantis") != std::string::npos)
            return graph().getLocationId("Atlantis Hostel");
        if (h.find("bose") != std::string::npos)
            return graph().getLocationId("SC Bose Hostel");
        if (h.find("bhagat") != std::string::npos)
            return graph().getLocationId("Bhagat Singh Hostel");
        if (h.find("patel") != std::string::npos)
            return graph().getLocationId("Sardar Patel Hostel");
        return graph().getLocationId("Junction 1"); // unknown: central junction
    }

    int officeForDepartment(const std::string &department)
    {
        std::string d = canonicalDepartment(department);
        if (d == "Electrical")
            return graph().getLocationId("Electrical Office");
        if (d == "Plumbing")
            return graph().getLocationId("Plumbing Office");
        if (d == "IT Support")
            return graph().getLocationId("IT Support Office");
        if (d == "Security")
            return graph().getLocationId("Security Office");
        int maintenance = graph().getLocationId("Maintenance Office");
        return maintenance >= 0 ? maintenance : graph().getLocationId("Main Gate");
    }

    // ======================= Availability =======================

    std::string formatMinutes(int minutes)
    {
        minutes = ((minutes % 1440) + 1440) % 1440;
        char buffer[6];
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes / 60, minutes % 60);
        return buffer;
    }

    const char *dayName(int day)
    {
        static const char *names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        return names[((day % 7) + 7) % 7];
    }

    DutyStatus dutyStatus(const Availability *a, std::time_t now)
    {
        if (!a || !a->configured)
            return {};

        std::tm tm = localTimeOf(now);
        char dateText[11];
        std::strftime(dateText, sizeof(dateText), "%Y-%m-%d", &tm);
        const std::string today = dateText;
        if (!a->leaveFrom.empty() && !a->leaveTo.empty() && today >= a->leaveFrom && today <= a->leaveTo)
            return {"on_leave", "On leave until " + a->leaveTo, false};

        const int dow = (tm.tm_wday + 6) % 7; // Monday = 0
        const int minute = tm.tm_hour * 60 + tm.tm_min;

        // Today's shift (end <= start means it continues past midnight).
        const Shift &t = a->days[dow];
        if (t.working)
        {
            bool overnight = t.end <= t.start;
            if ((!overnight && minute >= t.start && minute < t.end) || (overnight && minute >= t.start))
                return {"on_duty", "On duty until " + formatMinutes(t.end), true};
        }
        // Yesterday's overnight shift still running?
        const Shift &y = a->days[(dow + 6) % 7];
        if (y.working && y.end <= y.start && minute < y.end)
            return {"on_duty", "On duty until " + formatMinutes(y.end), true};

        for (int k = 0; k < 8; ++k)
        {
            int day = (dow + k) % 7;
            const Shift &shift = a->days[day];
            if (!shift.working || (k == 0 && shift.start <= minute))
                continue;
            std::string when = k == 0 ? "today" : k == 1 ? "tomorrow" : dayName(day);
            return {"off_duty", "Off duty, back " + when + " " + formatMinutes(shift.start), false};
        }
        return {"off_duty", "Off duty (no working days set)", false};
    }

    // ======================= Dispatch =======================

    namespace
    {
        DispatchDecision chooseFrom(const Complaint &complaint, const std::vector<WorkerLoad> &workers,
                                    const std::string &excludeWorkerId, bool allowOffDuty)
        {
            DispatchDecision decision;
            const std::string department = departmentForComplaint(complaint.category, complaint.description);
            const int destination = locationForHostel(complaint.hostelName);

            std::vector<Worker> legacyWorkers; // legacy class from worker.h
            for (const auto &load : workers)
            {
                const WorkerRecord &w = load.worker;
                if (canonicalDepartment(w.department) != department)
                    continue;

                DispatchCandidate c;
                c.workerId = w.id;
                c.name = w.name;
                c.department = department;
                c.activeTasks = load.activeTasks;
                c.duty = load.duty.label;
                c.onDuty = load.duty.assignable;
                c.route = shortestRoute(officeForDepartment(department), destination);
                c.distance = c.route.distance;

                if (!w.active)
                    c.reason = "Inactive";
                else if (w.id == excludeWorkerId)
                    c.reason = "Escalated this complaint";
                else if (load.duty.state == "on_leave")
                    c.reason = load.duty.label;
                else if (load.activeTasks >= kMaxActiveTasksPerWorker)
                    c.reason = "At capacity (" + std::to_string(kMaxActiveTasksPerWorker) + " active tasks)";
                else if (!c.route.exists)
                    c.reason = "No route to complaint location";
                else if (!load.duty.assignable && !allowOffDuty)
                    c.reason = load.duty.label;
                c.available = c.reason.empty();
                decision.candidates.push_back(c);

                if (w.active)
                {
                    Worker legacy(w.id, "", w.name, department, officeForDepartment(department));
                    legacy.setActiveTasks(load.activeTasks);
                    legacy.setAvailability(c.available);
                    legacyWorkers.push_back(legacy);
                }
            }

            std::sort(decision.candidates.begin(), decision.candidates.end(),
                      [](const DispatchCandidate &a, const DispatchCandidate &b)
                      {
                          if (a.available != b.available)
                              return a.available;
                          if (a.distance != b.distance)
                              return a.distance < b.distance;
                          if (a.activeTasks != b.activeTasks)
                              return a.activeTasks < b.activeTasks;
                          return a.workerId < b.workerId;
                      });

            DispatchManager manager(legacyWorkers, graph());
            DispatchResult result = manager.findNearestWorker(department, destination);
            if (!result.success)
            {
                decision.message = "No available " + department +
                                   " worker (off duty, on leave, inactive, at capacity or unreachable)";
                return decision;
            }
            for (const auto &c : decision.candidates)
            {
                if (c.workerId != result.assignedWorker.getWorkerId())
                    continue;
                decision.chosen = c;
                decision.success = true;
                decision.message = "Nearest available " + department + " worker: " + c.name + " (" +
                                   std::to_string(c.distance) + " m, " + std::to_string(c.activeTasks) +
                                   " active tasks)";
                if (!c.onDuty)
                    decision.message += ". Off duty, called in because the complaint is Critical";
                break;
            }
            return decision;
        }
    }

    DispatchDecision chooseWorker(const Complaint &complaint, const std::vector<WorkerLoad> &workers,
                                  const std::string &excludeWorkerId)
    {
        DispatchDecision decision = chooseFrom(complaint, workers, excludeWorkerId, false);
        if (!decision.success && canonicalPriority(complaint.priority) == "Critical")
        {
            DispatchDecision emergency = chooseFrom(complaint, workers, excludeWorkerId, true);
            if (emergency.success)
                return emergency;
        }
        return decision;
    }

    // ======================= Priority queue =======================

    std::vector<QueueItem> priorityOrder(const std::vector<QueueItem> &items)
    {
        // Comparator for a max-heap: returns true when `a` should come AFTER `b`.
        auto lowerPriority = [](const QueueItem &a, const QueueItem &b)
        {
            if (a.effectiveRank != b.effectiveRank)
                return a.effectiveRank < b.effectiveRank;
            // On a tie, a genuinely urgent complaint beats one that only
            // reached this level through ageing.
            int aBase = priorityRank(a.complaint.priority);
            int bBase = priorityRank(b.complaint.priority);
            if (aBase != bBase)
                return aBase < bBase;
            bool aWorking = a.complaint.status == "In Progress";
            bool bWorking = b.complaint.status == "In Progress";
            if (aWorking != bWorking)
                return bWorking;
            if (a.submittedAt != b.submittedAt)
                return a.submittedAt > b.submittedAt; // older first
            return a.complaint.id > b.complaint.id;
        };

        std::priority_queue<QueueItem, std::vector<QueueItem>, decltype(lowerPriority)> heap(lowerPriority);
        for (const auto &item : items)
            heap.push(item);

        std::vector<QueueItem> ordered;
        while (!heap.empty())
        {
            ordered.push_back(heap.top());
            heap.pop();
        }
        return ordered;
    }
}
