#include "api.h"
#include "core/crypto.h"

#include <algorithm>
#include <cmath>

namespace hc
{
    namespace
    {
        double round1(double value) { return std::round(value * 10.0) / 10.0; }

        std::string dateOf(std::time_t t)
        {
            std::tm tm = localTimeOf(t);
            char buffer[11];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
            return buffer;
        }

        Json countList(const std::map<std::string, int> &counts)
        {
            std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
            std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b)
                      { return a.second != b.second ? a.second > b.second : a.first < b.first; });
            Json::list list;
            for (const auto &entry : sorted)
            {
                Json item;
                item["label"] = entry.first;
                item["count"] = entry.second;
                list.push_back(std::move(item));
            }
            return Json(std::move(list));
        }

        Json candidateJson(const DispatchCandidate &c, bool withRoute)
        {
            Json item;
            item["workerId"] = c.workerId;
            item["name"] = c.name;
            item["department"] = c.department;
            item["activeTasks"] = c.activeTasks;
            item["distance"] = c.distance;
            item["available"] = c.available;
            item["reason"] = c.reason;
            item["duty"] = c.duty;
            item["onDuty"] = c.onDuty;
            if (withRoute)
            {
                Json::list names;
                for (const auto &n : c.route.pathNames)
                    names.push_back(Json(n));
                item["route"] = std::move(names);
            }
            return item;
        }

        // Last time a complaint reached `status`, or -1.
        std::time_t lastEventTime(const std::vector<HistoryEvent> &events, const std::string &status)
        {
            std::time_t found = -1;
            for (const auto &e : events)
                if (e.status == status)
                    found = parseTimestamp(e.timestamp);
            return found;
        }
    }

    // ============================ Workers ============================

    Reply Api::listWorkers(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();
        auto withLogin = store_.accountIds("worker");

        Json::list list;
        for (const auto &entry : s.workers)
        {
            const WorkerRecord &w = entry.second;
            int assigned = 0, active = 0, resolved = 0, ratings = 0, ratingSum = 0;
            for (const auto &c : s.complaints)
            {
                if (c.workerId != w.id)
                    continue;
                ++assigned;
                if (isActiveWorkStatus(c.status))
                    ++active;
                if (c.status == "Resolved")
                    ++resolved;
            }
            for (const auto &f : s.feedback)
                if (f.second.workerId == w.id)
                {
                    ++ratings;
                    ratingSum += f.second.rating;
                }

            Json item;
            item["workerId"] = w.id;
            item["name"] = w.name;
            std::string dept = canonicalDepartment(w.department);
            item["department"] = dept.empty() ? w.department : dept;
            item["isActive"] = w.active;
            item["hasLogin"] = std::find(withLogin.begin(), withLogin.end(), w.id) != withLogin.end();
            auto av = s.availability.find(w.id);
            const Availability *availability = av == s.availability.end() ? nullptr : &av->second;
            DutyStatus duty = dutyStatus(availability, nowEpoch());
            item["schedule"] = availabilityJson(availability, duty);
            item["assignedComplaintCount"] = assigned;
            item["activeTaskCount"] = active;
            item["resolvedCount"] = resolved;
            item["ratingsCount"] = ratings;
            item["averageRating"] = ratings ? round1(static_cast<double>(ratingSum) / ratings) : 0.0;
            item["office"] = locationName(officeForDepartment(w.department));
            if (!w.active)
                item["availability"] = "Inactive";
            else if (duty.state == "on_leave")
                item["availability"] = "On leave";
            else if (duty.state == "off_duty")
                item["availability"] = "Off duty";
            else if (active >= kMaxActiveTasksPerWorker)
                item["availability"] = "At capacity";
            else if (active > 0)
                item["availability"] = "Busy";
            else
                item["availability"] = "Available";
            list.push_back(std::move(item));
        }
        Reply reply;
        reply.body["workers"] = std::move(list);
        return reply;
    }

    Reply Api::addWorker(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin});
        Body body(request.body);
        WorkerRecord w;
        w.id = requireId(body.text("workerId"), "Worker ID");
        w.name = requireText(body.text("name"), "Name", 60, 2);
        w.department = canonicalDepartment(body.text("department"));
        if (w.department.empty())
            throw ApiError(400, "Department must be Electrical, Plumbing, IT Support, Security or "
                                "General Maintenance");
        w.active = true;

        std::string password = body.text("password", false);
        std::string hash;
        if (!password.empty())
        {
            requirePasswordStrength(password, 8);
            hash = hashPassword(password);
        }

        std::lock_guard<std::mutex> lock(dataMutex_);
        DataStore::Transaction tx(store_);
        for (const auto &existing : store_.loadWorkers())
            if (iequals(existing.id, w.id))
                throw ApiError(409, "Worker ID " + w.id + " already exists");
        store_.insertWorker(w);
        if (!hash.empty())
            store_.setAccountHash("worker", w.id, hash);
        tx.commit();

        Reply reply;
        reply.status = 201;
        reply.body["message"] = hash.empty() ? "Worker added. Set a password so they can log in."
                                             : "Worker added with login access.";
        return reply;
    }

    Reply Api::editWorker(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin});
        Body body(request.body);
        std::string id = trim(body.text("workerId"));
        std::string name = requireText(body.text("name"), "Name", 60, 2);
        std::string department = canonicalDepartment(body.text("department"));
        if (department.empty())
            throw ApiError(400, "Department must be Electrical, Plumbing, IT Support, Security or "
                                "General Maintenance");

        std::lock_guard<std::mutex> lock(dataMutex_);
        auto worker = store_.findWorker(id);
        if (!worker)
            throw ApiError(404, "Worker not found");
        worker->name = name;
        worker->department = department; // ID and active flag are preserved
        store_.updateWorker(*worker);

        Reply reply;
        reply.body["message"] = "Worker updated successfully";
        return reply;
    }

    Reply Api::setWorkerStatus(const HttpRequest &request)
    {
        Principal admin = requireAuth(request, {Role::Admin});
        Body body(request.body);
        std::string id = trim(body.text("workerId"));
        if (!body.has("isActive"))
            throw ApiError(400, "isActive is required");
        bool active = body.boolean("isActive", true);

        std::lock_guard<std::mutex> lock(dataMutex_);
        auto target = store_.findWorker(id);
        if (!target)
            throw ApiError(404, "Worker not found");
        if (isDemoAccount({Role::Worker, id}) && !active)
            throw ApiError(403, "The demo worker cannot be deactivated");
        target->active = active;
        store_.updateWorker(*target);

        int activeTasks = 0;
        for (const auto &c : store_.loadComplaints())
            if (c.workerId == id && isActiveWorkStatus(c.status))
                ++activeTasks;

        std::string message;
        if (active)
            message = target->name + " activated";
        else
        {
            sessions_.revokeUser(Role::Worker, id); // lose access immediately
            message = target->name + " deactivated and logged out";
            if (activeTasks > 0)
            {
                message += ". " + std::to_string(activeTasks) +
                           " active task(s) are still assigned to them; reassign them from the Complaints page.";
                notifyAdmins(0, target->name + " (" + id + ") was deactivated with " +
                                    std::to_string(activeTasks) + " active task(s) by " + admin.id + ".");
            }
        }

        Reply reply;
        reply.body["message"] = message;
        reply.body["activeTasks"] = activeTasks;
        return reply;
    }

    Reply Api::setWorkerPassword(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin});
        Body body(request.body);
        std::string id = trim(body.text("workerId"));
        if (isDemoAccount({Role::Worker, id}))
            throw ApiError(403, "The demo worker's password cannot be changed");
        std::string password = body.text("password");
        requirePasswordStrength(password, 8);
        std::string hash = hashPassword(password); // slow: do it before locking

        bool replaced = false;
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            if (!store_.findWorker(id))
                throw ApiError(404, "Worker not found");
            replaced = !store_.accountHash("worker", id).empty();
            store_.setAccountHash("worker", id, hash);
        }
        sessions_.revokeUser(Role::Worker, id); // a reset logs them out everywhere

        Reply reply;
        reply.body["message"] = replaced ? "Worker password reset successfully"
                                         : "Worker login created successfully";
        reply.body["workerId"] = id;
        return reply;
    }

    // ============================ Dashboard & analytics ============================

    Reply Api::dashboard(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin});
        std::lock_guard<std::mutex> lock(dataMutex_);
        auto complaints = store_.loadComplaints();

        int pending = 0, inProgress = 0, resolved = 0, rejected = 0, forwarded = 0, escalated = 0, queue = 0;
        for (const auto &c : complaints)
        {
            if (c.status == "Pending" || c.status == "Verified" || c.status == "Forwarded" ||
                c.status == "Escalated")
                ++pending;
            if (isActiveWorkStatus(c.status))
                ++inProgress;
            if (c.status == "Resolved")
                ++resolved;
            if (c.status == "Rejected")
                ++rejected;
            if (c.status == "Forwarded")
                ++forwarded;
            if (c.status == "Escalated")
                ++escalated;
            if (canAutoDispatchFrom(c.status) || c.status == "Escalated")
                ++queue;
        }
        Reply reply;
        reply.body["total"] = complaints.size();
        reply.body["pending"] = pending;
        reply.body["inProgress"] = inProgress;
        reply.body["resolved"] = resolved;
        reply.body["rejected"] = rejected;
        reply.body["forwarded"] = forwarded;
        reply.body["escalated"] = escalated;
        reply.body["dispatchQueue"] = queue;
        return reply;
    }

    Reply Api::analytics(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();
        const std::time_t now = nowEpoch();

        std::map<std::string, int> byStatus, byCategory, byDepartment, byPriority, byLocation;
        std::vector<double> resolutionHours;
        int overdue = 0;
        for (const auto &c : s.complaints)
        {
            ++byStatus[c.status];
            ++byCategory[c.category.empty() ? "Unknown" : c.category];
            ++byDepartment[departmentForComplaint(c.category, c.description)];
            ++byPriority[canonicalPriority(c.priority).empty() ? "Unknown" : canonicalPriority(c.priority)];
            ++byLocation[locationName(locationForHostel(c.hostelName))];

            std::time_t start = submittedAt(s, c.id);
            if (c.status == "Resolved")
            {
                std::time_t end = lastEventTime(s.history[c.id], "Resolved");
                if (start >= 0 && end >= start)
                    resolutionHours.push_back((end - start) / 3600.0);
            }
            else if (isOpenStatus(c.status) && start >= 0 && now - start > 72 * 3600)
                ++overdue;
        }

        double average = 0, median = 0;
        if (!resolutionHours.empty())
        {
            for (double h : resolutionHours)
                average += h;
            average /= resolutionHours.size();
            std::sort(resolutionHours.begin(), resolutionHours.end());
            std::size_t n = resolutionHours.size();
            median = n % 2 ? resolutionHours[n / 2] : (resolutionHours[n / 2 - 1] + resolutionHours[n / 2]) / 2;
        }

        // Submissions per day for the last 14 days.
        std::map<std::string, int> perDay;
        for (const auto &entry : s.history)
            for (const auto &e : entry.second)
                if (e.status == "Submitted" && e.timestamp.size() >= 10)
                    ++perDay[e.timestamp.substr(0, 10)];
        Json::list trend;
        for (int daysAgo = 13; daysAgo >= 0; --daysAgo)
        {
            std::string day = dateOf(now - static_cast<std::time_t>(daysAgo) * 86400);
            Json point;
            point["date"] = day;
            point["count"] = perDay.count(day) ? perDay[day] : 0;
            trend.push_back(std::move(point));
        }

        // Ratings and worker performance.
        int ratingCount = 0, ratingSum = 0;
        int distribution[6] = {0, 0, 0, 0, 0, 0};
        for (const auto &f : s.feedback)
        {
            ++ratingCount;
            ratingSum += f.second.rating;
            ++distribution[f.second.rating];
        }
        Json::list dist;
        for (int star = 1; star <= 5; ++star)
        {
            Json point;
            point["label"] = std::to_string(star) + "★";
            point["count"] = distribution[star];
            dist.push_back(std::move(point));
        }

        Json::list workers;
        for (const auto &entry : s.workers)
        {
            const WorkerRecord &w = entry.second;
            int resolved = 0, active = 0, escalated = 0, ratings = 0, sum = 0;
            double hours = 0;
            int timed = 0;
            for (const auto &c : s.complaints)
            {
                if (c.workerId != w.id)
                    continue;
                if (isActiveWorkStatus(c.status))
                    ++active;
                if (c.status == "Escalated")
                    ++escalated;
                if (c.status == "Resolved")
                {
                    ++resolved;
                    std::time_t start = submittedAt(s, c.id);
                    std::time_t end = lastEventTime(s.history[c.id], "Resolved");
                    if (start >= 0 && end >= start)
                    {
                        hours += (end - start) / 3600.0;
                        ++timed;
                    }
                }
            }
            for (const auto &f : s.feedback)
                if (f.second.workerId == w.id)
                {
                    ++ratings;
                    sum += f.second.rating;
                }
            Json item;
            item["workerId"] = w.id;
            item["name"] = w.name;
            item["department"] = canonicalDepartment(w.department).empty() ? w.department
                                                                          : canonicalDepartment(w.department);
            item["isActive"] = w.active;
            item["resolved"] = resolved;
            item["active"] = active;
            item["escalated"] = escalated;
            item["averageRating"] = ratings ? round1(static_cast<double>(sum) / ratings) : 0.0;
            item["ratingsCount"] = ratings;
            item["averageResolutionHours"] = timed ? round1(hours / timed) : 0.0;
            workers.push_back(std::move(item));
        }

        Reply reply;
        reply.body["total"] = s.complaints.size();
        reply.body["byStatus"] = countList(byStatus);
        reply.body["byCategory"] = countList(byCategory);
        reply.body["byDepartment"] = countList(byDepartment);
        reply.body["byPriority"] = countList(byPriority);
        reply.body["byLocation"] = countList(byLocation);
        reply.body["averageResolutionHours"] = round1(average);
        reply.body["medianResolutionHours"] = round1(median);
        reply.body["resolvedWithTiming"] = resolutionHours.size();
        reply.body["overdueOpen"] = overdue;
        reply.body["averageRating"] = ratingCount ? round1(static_cast<double>(ratingSum) / ratingCount) : 0.0;
        reply.body["ratingsCount"] = ratingCount;
        reply.body["ratingDistribution"] = std::move(dist);
        reply.body["dailySubmissions"] = std::move(trend);
        reply.body["workers"] = std::move(workers);
        return reply;
    }

    // ============================ Dispatch ============================

    Reply Api::dispatchOptions(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin});
        int id = request.params.at(0);
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();
        Complaint &c = findComplaint(s.complaints, id);
        std::string exclude = c.status == "Escalated" ? c.workerId : "";
        DispatchDecision d = chooseWorker(c, workerLoads(s), exclude);

        Json::list candidates;
        for (const auto &cand : d.candidates)
            candidates.push_back(candidateJson(cand, false));
        Reply reply;
        reply.body["complaintId"] = id;
        reply.body["department"] = departmentForComplaint(c.category, c.description);
        reply.body["location"] = locationName(locationForHostel(c.hostelName));
        reply.body["success"] = d.success;
        reply.body["message"] = d.message;
        if (d.success)
            reply.body["recommended"] = candidateJson(d.chosen, true);
        reply.body["candidates"] = std::move(candidates);
        return reply;
    }

    Reply Api::autoAssign(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Admin});
        int id = request.params.at(0);
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();
        Complaint &c = findComplaint(s.complaints, id);
        if (!canAutoDispatchFrom(c.status) && c.status != "Escalated")
            throw ApiError(400, "Auto-assign works on Verified, Forwarded or Escalated complaints (this one is " +
                                    c.status + ")");

        std::string exclude = c.status == "Escalated" ? c.workerId : "";
        DispatchDecision d = chooseWorker(c, workerLoads(s), exclude);
        if (!d.success)
            throw ApiError(409, d.message, "NO_WORKER_AVAILABLE");

        const WorkerRecord &worker = s.workers.at(d.chosen.workerId);
        std::string previous = assignInMemory(c, worker);
        DataStore::Transaction tx(store_);
        store_.updateComplaint(c);
        announceAssignment(c, worker, previous, actorOf(p) + " (auto)");
        tx.commit();

        Reply reply;
        reply.body["message"] = d.message;
        reply.body["workerId"] = worker.id;
        reply.body["workerName"] = worker.name;
        reply.body["distance"] = d.chosen.distance;
        reply.body["route"] = routeJson(d.chosen.route);
        return reply;
    }

    Reply Api::dispatchQueue(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();
        const std::time_t now = nowEpoch();

        std::vector<QueueItem> items;
        for (const auto &c : s.complaints)
            if (canAutoDispatchFrom(c.status) || c.status == "Escalated")
            {
                std::time_t at = submittedAt(s, c.id);
                items.push_back({c, effectivePriorityRank(c.priority, at, now), at});
            }

        auto loads = workerLoads(s);
        Json::list list;
        int position = 0;
        for (const auto &item : priorityOrder(items))
        {
            Json entry = complaintJson(item.complaint, s, Role::Admin);
            entry["queuePosition"] = ++position;
            entry["needsManualReview"] = item.complaint.status == "Escalated";
            std::string exclude = item.complaint.status == "Escalated" ? item.complaint.workerId : "";
            DispatchDecision d = chooseWorker(item.complaint, loads, exclude);
            entry["dispatchMessage"] = d.message;
            if (d.success)
                entry["recommended"] = candidateJson(d.chosen, true);
            list.push_back(std::move(entry));
        }
        Reply reply;
        reply.body["count"] = list.size();
        reply.body["queue"] = std::move(list);
        return reply;
    }

    Reply Api::runDispatch(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Admin});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();
        const std::time_t now = nowEpoch();

        std::vector<QueueItem> items;
        for (const auto &c : s.complaints)
            if (canAutoDispatchFrom(c.status)) // escalations need a human decision
            {
                std::time_t at = submittedAt(s, c.id);
                items.push_back({c, effectivePriorityRank(c.priority, at, now), at});
            }

        auto loads = workerLoads(s);
        struct Done
        {
            int complaintId;
            std::string workerId, previous;
        };
        std::vector<Done> done;
        Json::list results;

        // Pop complaints in priority order; each assignment updates the
        // workload so the next decision sees it.
        for (const auto &item : priorityOrder(items))
        {
            Json result;
            result["complaintId"] = item.complaint.id;
            result["title"] = item.complaint.title.empty() ? item.complaint.category : item.complaint.title;
            result["priority"] = priorityName(item.effectiveRank);

            DispatchDecision d = chooseWorker(item.complaint, loads);
            result["assigned"] = d.success;
            result["message"] = d.message;
            if (d.success)
            {
                Complaint &c = findComplaint(s.complaints, item.complaint.id);
                std::string previous = assignInMemory(c, s.workers.at(d.chosen.workerId));
                done.push_back({c.id, d.chosen.workerId, previous});
                for (auto &load : loads)
                    if (load.worker.id == d.chosen.workerId)
                        ++load.activeTasks;
                result["workerId"] = d.chosen.workerId;
                result["workerName"] = d.chosen.name;
                result["distance"] = d.chosen.distance;
            }
            results.push_back(std::move(result));
        }

        DataStore::Transaction tx(store_);
        for (const auto &d : done)
        {
            const Complaint &c = findComplaint(s.complaints, d.complaintId);
            store_.updateComplaint(c);
            announceAssignment(c, s.workers.at(d.workerId), d.previous, actorOf(p) + " (auto)");
        }
        tx.commit();

        Reply reply;
        reply.body["assigned"] = done.size();
        reply.body["skipped"] = items.size() - done.size();
        reply.body["results"] = std::move(results);
        reply.body["message"] = std::to_string(done.size()) + " complaint(s) dispatched, " +
                                std::to_string(items.size() - done.size()) + " waiting for a free worker";
        return reply;
    }

    Reply Api::listFeedback(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();
        Json::list list;
        for (auto it = s.feedback.rbegin(); it != s.feedback.rend(); ++it)
        {
            const Feedback &f = it->second;
            Json item;
            item["complaintId"] = f.complaintId;
            item["studentId"] = f.studentId;
            item["workerId"] = f.workerId;
            auto w = s.workers.find(f.workerId);
            item["workerName"] = w != s.workers.end() ? w->second.name : "";
            item["rating"] = f.rating;
            item["comment"] = f.comment;
            item["timestamp"] = f.timestamp;
            std::string title;
            for (const auto &c : s.complaints)
                if (c.id == f.complaintId)
                    title = c.title.empty() ? c.category : c.title;
            item["title"] = title;
            list.push_back(std::move(item));
        }
        Reply reply;
        reply.body["feedback"] = std::move(list);
        return reply;
    }
}
