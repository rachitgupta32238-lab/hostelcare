#pragma once
// Business rules that do not touch files: department matching, priority
// ranking, the complaint status machine, the campus graph (Dijkstra) and
// worker dispatch. The graph and dispatch reuse the legacy C++ classes
// (CampusGraph, DispatchManager, Worker, assignComplaintPriority).

#include "models.h"

#include <ctime>
#include <string>
#include <vector>

namespace hc
{
    // ---- Departments & categories ----
    const std::vector<std::string> &departments();       // 5 canonical names
    const std::vector<std::string> &complaintCategories(); // student form options
    // Canonical department name, or "" if not one of the five.
    std::string canonicalDepartment(const std::string &value);
    // Which department handles a complaint (category first, then keywords).
    std::string departmentForComplaint(const std::string &category, const std::string &description);

    // ---- Priority ----
    // Critical=4, High=3, Medium=2, Low=1, unknown=0
    int priorityRank(const std::string &priority);
    std::string priorityName(int rank);
    std::string canonicalPriority(const std::string &value); // "" if invalid
    // Keyword-based priority from the legacy priority module.
    std::string suggestedPriority(const std::string &category, const std::string &description);
    // Starvation prevention: every 48 hours of waiting raises a Low/Medium
    // complaint one level, up to High. Critical is never exceeded.
    int effectivePriorityRank(const std::string &priority, std::time_t submittedAt, std::time_t now);

    // ---- Status machine ----
    // Statuses: Pending, Verified, Forwarded, Assigned, Accepted, In Progress,
    //           Resolved, Rejected, Escalated
    const std::vector<std::string> &allStatuses();
    bool isActiveWorkStatus(const std::string &status); // Assigned/Accepted/In Progress
    bool isOpenStatus(const std::string &status);        // not Resolved/Rejected
    bool canAssignFrom(const std::string &status);
    bool canAutoDispatchFrom(const std::string &status); // Verified / Forwarded
    // Admin-driven status changes (assignment and worker actions have their
    // own endpoints and rules).
    std::vector<std::string> adminTransitions(const std::string &fromStatus);

    // ---- Campus graph (legacy Dijkstra) ----
    struct RouteInfo
    {
        bool exists = false;
        int distance = -1;
        std::vector<int> pathIds;
        std::vector<std::string> pathNames;
    };
    struct Location
    {
        int id;
        std::string name;
    };
    std::vector<Location> campusLocations();
    std::string locationName(int id);
    RouteInfo shortestRoute(int from, int to);
    int locationForHostel(const std::string &hostelName);     // never -1
    int officeForDepartment(const std::string &department);   // never -1
    const std::vector<std::string> &knownHostels();

    // ---- Worker availability ----
    struct DutyStatus
    {
        std::string state = "no_schedule"; // on_duty | off_duty | on_leave | no_schedule
        std::string label = "Hours not set";
        bool assignable = true; // on duty, or no schedule set (backward compatible)
    };
    // `availability` may be null (worker never set hours).
    DutyStatus dutyStatus(const Availability *availability, std::time_t now);
    std::string formatMinutes(int minutes); // 570 -> "09:30"
    const char *dayName(int day);            // 0 = Monday

    // ---- Dispatch (legacy DispatchManager) ----
    struct WorkerLoad
    {
        WorkerRecord worker;
        int activeTasks = 0;
        DutyStatus duty;
    };
    const int kMaxActiveTasksPerWorker = 5;

    struct DispatchCandidate
    {
        std::string workerId, name, department;
        int activeTasks = 0;
        int distance = -1;
        bool available = false;
        bool onDuty = true;
        std::string duty;   // human-readable duty status
        std::string reason; // why unavailable
        RouteInfo route;
    };
    struct DispatchDecision
    {
        bool success = false;
        std::string message;
        DispatchCandidate chosen;
        std::vector<DispatchCandidate> candidates; // department matches, best first
    };
    // Finds the nearest available worker of the right department using the
    // legacy DispatchManager::findNearestWorker (Dijkstra distance, then
    // fewest active tasks, then lowest worker ID). `excludeWorkerId` skips one
    // worker (for example the one who escalated). Only on-duty workers are
    // chosen, except that a Critical complaint may call in an off-duty (but
    // not on-leave) worker when nobody suitable is on duty.
    DispatchDecision chooseWorker(const Complaint &complaint, const std::vector<WorkerLoad> &workers,
                                  const std::string &excludeWorkerId = "");

    // ---- Priority queue ordering ----
    struct QueueItem
    {
        Complaint complaint;
        int effectiveRank = 0;
        std::time_t submittedAt = -1;
    };
    // Returns items in max-priority-queue pop order (std::priority_queue):
    // higher effective priority first, then higher original priority, then
    // In Progress before others, then older first, then lower ID.
    std::vector<QueueItem> priorityOrder(const std::vector<QueueItem> &items);
}
