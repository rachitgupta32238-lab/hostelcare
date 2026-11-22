#include "api.h"

#include <algorithm>

namespace hc
{
    // ============================ Shared helpers ============================

    Api::Snapshot Api::loadSnapshot()
    {
        Snapshot s;
        s.availability = store_.loadAvailability();
        s.complaints = store_.loadComplaints();
        for (const auto &w : store_.loadWorkers())
            s.workers[w.id] = w;
        for (const auto &e : store_.loadHistory())
            s.history[e.complaintId].push_back(e);
        for (const auto &f : store_.loadFeedback())
            s.feedback[f.complaintId] = f; // latest wins (only one is allowed)
        return s;
    }

    std::time_t Api::submittedAt(const Snapshot &s, int complaintId)
    {
        auto it = s.history.find(complaintId);
        if (it == s.history.end() || it->second.empty())
            return -1; // complaints created before history tracking existed
        for (const auto &e : it->second)
            if (e.status == "Submitted")
                return parseTimestamp(e.timestamp);
        return parseTimestamp(it->second.front().timestamp);
    }

    std::vector<WorkerLoad> Api::workerLoads(const Snapshot &s) const
    {
        std::vector<WorkerLoad> loads;
        for (const auto &entry : s.workers)
        {
            WorkerLoad load{entry.second, 0, {}};
            auto a = s.availability.find(entry.first);
            load.duty = dutyStatus(a == s.availability.end() ? nullptr : &a->second, nowEpoch());
            for (const auto &c : s.complaints)
                if (c.workerId == entry.first && isActiveWorkStatus(c.status))
                    ++load.activeTasks;
            loads.push_back(load);
        }
        return loads;
    }

    Json Api::routeJson(const RouteInfo &route) const
    {
        Json json;
        json["pathExists"] = route.exists;
        json["totalDistance"] = route.distance;
        Json::list names;
        for (const auto &n : route.pathNames)
            names.push_back(Json(n));
        json["locations"] = std::move(names);
        return json;
    }

    Json Api::complaintJson(const Complaint &c, const Snapshot &s, Role viewer) const
    {
        Json item;
        item["id"] = c.id;
        item["title"] = c.title.empty() ? c.category : c.title;
        item["studentId"] = c.studentId;
        item["studentName"] = c.studentName;
        item["roomNo"] = c.roomNo;
        item["hostelName"] = c.hostelName;
        item["category"] = c.category;
        item["description"] = c.description;
        item["priority"] = canonicalPriority(c.priority).empty() ? c.priority : canonicalPriority(c.priority);
        item["status"] = c.status;
        item["workerId"] = c.isAssigned() ? c.workerId : "";
        item["feedback"] = c.feedback; // worker resolution / escalation note

        const std::string department = departmentForComplaint(c.category, c.description);
        item["department"] = department;
        int location = locationForHostel(c.hostelName);
        item["locationId"] = location;
        item["locationName"] = locationName(location);

        auto w = s.workers.find(c.workerId);
        item["workerName"] = (c.isAssigned() && w != s.workers.end()) ? w->second.name : "";

        std::time_t submitted = submittedAt(s, c.id);
        auto h = s.history.find(c.id);
        item["submittedAt"] = (h != s.history.end() && !h->second.empty()) ? h->second.front().timestamp : "";
        item["lastUpdated"] = (h != s.history.end() && !h->second.empty()) ? h->second.back().timestamp : "";
        int effective = isOpenStatus(c.status) ? effectivePriorityRank(c.priority, submitted, nowEpoch())
                                               : std::max(1, priorityRank(c.priority));
        item["effectivePriority"] = priorityName(effective);
        item["ageHours"] = submitted < 0 ? -1 : static_cast<int>((nowEpoch() - submitted) / 3600);

        auto f = s.feedback.find(c.id);
        bool rated = f != s.feedback.end();
        item["rating"] = rated ? f->second.rating : 0;
        item["ratingComment"] = rated ? f->second.comment : "";
        item["canRate"] = viewer == Role::Student && c.status == "Resolved" && !rated;
        if (viewer == Role::Admin)
        {
            // The UI shows only actions the server will accept.
            Json::list next;
            for (const auto &status : adminTransitions(c.status))
                next.push_back(Json(status));
            item["allowedStatuses"] = std::move(next);
            item["canAssign"] = canAssignFrom(c.status);
            item["canAutoAssign"] = canAutoDispatchFrom(c.status) || c.status == "Escalated";
        }
        return item;
    }

    std::string Api::assignInMemory(Complaint &c, const WorkerRecord &worker)
    {
        std::string previous;
        if (c.isAssigned() && c.workerId != worker.id && isActiveWorkStatus(c.status))
            previous = c.workerId;
        c.workerId = worker.id;
        c.status = "Assigned";
        return previous;
    }

    void Api::announceAssignment(const Complaint &c, const WorkerRecord &worker,
                                 const std::string &previousWorker, const std::string &actor)
    {
        recordHistory(c.id, "Assigned", actor);
        if (!previousWorker.empty())
            notify("worker", previousWorker, c.id,
                   "Task #" + std::to_string(c.id) + " was reassigned to another worker.");
        notify("worker", worker.id, c.id,
               "New task #" + std::to_string(c.id) + ": " + (c.title.empty() ? c.category : c.title) +
                   " (" + c.priority + ") at " + c.hostelName + ", room " + c.roomNo + ".");
        notify("student", c.studentId, c.id,
               "Complaint #" + std::to_string(c.id) + " was assigned to " + worker.name + " (" +
                   worker.department + ").");
    }

    // ============================ Handlers ============================

    Reply Api::listComplaints(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Admin, Role::Worker, Role::Student});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();

        Json::list items;
        for (auto it = s.complaints.rbegin(); it != s.complaints.rend(); ++it) // newest first
        {
            const Complaint &c = *it;
            bool visible = p.role == Role::Admin ||
                           (p.role == Role::Student && c.studentId == p.id) ||
                           (p.role == Role::Worker && c.workerId == p.id);
            if (visible)
                items.push_back(complaintJson(c, s, p.role));
        }
        Reply reply;
        reply.body["count"] = items.size();
        reply.body["complaints"] = std::move(items);
        return reply;
    }

    Reply Api::createComplaint(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Student});
        Body body(request.body);

        Complaint c;
        c.title = requireText(body.text("title"), "Title", 100, 3);
        c.description = requireText(body.text("description"), "Description", 1000, 5);
        std::string category = requireText(body.text("category"), "Category", 30);
        c.category.clear();
        for (const auto &known : complaintCategories())
            if (iequals(known, category))
                c.category = known;
        if (c.category.empty())
            throw ApiError(400, "Unknown category. Choose one of the listed categories.");

        std::string chosen = canonicalPriority(body.text("priority", false));
        if (chosen.empty())
            chosen = "Low";
        // Students cannot under-report an emergency: the stored priority is the
        // higher of their choice and the keyword-based suggestion. The warden
        // can still change it afterwards.
        std::string suggested = canonicalPriority(suggestedPriority(c.category, c.description));
        c.priority = priorityRank(suggested) > priorityRank(chosen) ? suggested : chosen;
        c.status = "Pending";
        c.workerId = "-1";

        std::string room = body.text("roomNo", false);
        std::string hostel = body.text("hostelName", false);

        std::lock_guard<std::mutex> lock(dataMutex_);
        DataStore::Transaction tx(store_);
        auto found = store_.findStudent(p.id);
        if (!found)
            throw ApiError(404, "Student profile not found");
        const Student student = *found;

        c.studentId = student.id;
        c.studentName = sanitizeField(student.name);
        c.roomNo = trim(room).empty() ? sanitizeField(student.room) : requireText(room, "Room number", 20);
        c.hostelName = trim(hostel).empty() ? sanitizeField(student.hostel)
                                            : requireText(hostel, "Hostel name", 60);

        auto complaints = store_.loadComplaints();
        int open = 0;
        for (const auto &existing : complaints)
            if (existing.studentId == student.id && isOpenStatus(existing.status))
                ++open;
        if (open >= 10)
            throw ApiError(429, "You already have 10 open complaints. Please wait for some to be resolved.");

        c.id = store_.insertComplaint(c);
        recordHistory(c.id, "Submitted", actorOf(p));
        notifyAdmins(c.id, "New " + c.priority + " complaint #" + std::to_string(c.id) + ": " + c.title +
                               " (" + c.hostelName + ", room " + c.roomNo + ")");
        tx.commit();

        Reply reply;
        reply.status = 201;
        reply.body["message"] = "Complaint submitted successfully";
        reply.body["id"] = c.id;
        reply.body["priority"] = c.priority;
        reply.body["suggestedPriority"] = suggested;
        reply.body["department"] = departmentForComplaint(c.category, c.description);
        return reply;
    }

    Reply Api::complaintHistory(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Admin, Role::Worker, Role::Student});
        int id = request.params.at(0);
        std::lock_guard<std::mutex> lock(dataMutex_);
        auto complaints = store_.loadComplaints();
        Complaint &c = findComplaint(complaints, id);
        bool allowed = p.role == Role::Admin || (p.role == Role::Student && c.studentId == p.id) ||
                       (p.role == Role::Worker && c.workerId == p.id);
        if (!allowed)
            throw ApiError(404, "Complaint #" + std::to_string(id) + " not found");

        Json::list events;
        for (const auto &e : store_.loadHistoryFor(id))
        {
            Json event;
            event["status"] = e.status;
            event["timestamp"] = e.timestamp;
            // Students see who acted by role only.
            std::string actor = e.actor;
            if (p.role != Role::Admin && actor.find(':') != std::string::npos)
                actor = actor.substr(0, actor.find(':'));
            event["actor"] = actor;
            events.push_back(std::move(event));
        }
        Reply reply;
        reply.body["complaintId"] = id;
        reply.body["history"] = std::move(events);
        return reply;
    }

    Reply Api::updateStatus(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Admin});
        int id = request.params.at(0);
        Body body(request.body);
        std::string requested = body.text("status");
        std::string note = requireText(body.text("note", false), "Note", 300, 0);

        std::string target;
        for (const auto &s : allStatuses())
            if (iequals(s, requested))
                target = s;
        if (target.empty())
            throw ApiError(400, "Invalid status");

        std::lock_guard<std::mutex> lock(dataMutex_);
        DataStore::Transaction tx(store_);
        auto complaints = store_.loadComplaints();
        Complaint &c = findComplaint(complaints, id);
        if (c.status == target)
            throw ApiError(400, "Complaint is already " + target);

        auto allowed = adminTransitions(c.status);
        if (std::find(allowed.begin(), allowed.end(), target) == allowed.end())
        {
            std::string list;
            for (const auto &a : allowed)
                list += (list.empty() ? "" : ", ") + a;
            std::string hint = isActiveWorkStatus(target) ? " Use worker assignment instead." : "";
            throw ApiError(400, "Cannot change status from " + c.status + " to " + target +
                                    ". Allowed: " + list + "." + hint);
        }

        std::string removedWorker;
        if (target == "Forwarded" && c.isAssigned() &&
            (isActiveWorkStatus(c.status) || c.status == "Escalated"))
        {
            if (isActiveWorkStatus(c.status))
                removedWorker = c.workerId;
            c.workerId = "-1"; // back in the dispatch queue
        }
        c.status = target;
        store_.updateComplaint(c);

        recordHistory(c.id, target, actorOf(p));
        std::string message = "Complaint #" + std::to_string(c.id) + " status changed to " + target + ".";
        if (!note.empty())
            message += " Note: " + note;
        notify("student", c.studentId, c.id, message);
        if (!removedWorker.empty())
            notify("worker", removedWorker, c.id,
                   "Task #" + std::to_string(c.id) + " was taken back by the warden.");

        tx.commit();

        Reply reply;
        reply.body["message"] = "Complaint status updated";
        reply.body["status"] = target;
        return reply;
    }

    Reply Api::updatePriority(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Admin});
        int id = request.params.at(0);
        Body body(request.body);
        std::string priority = canonicalPriority(body.text("priority"));
        if (priority.empty())
            throw ApiError(400, "Priority must be Low, Medium, High or Critical");

        std::lock_guard<std::mutex> lock(dataMutex_);
        DataStore::Transaction tx(store_);
        auto complaints = store_.loadComplaints();
        Complaint &c = findComplaint(complaints, id);
        if (!isOpenStatus(c.status))
            throw ApiError(400, "Priority can only be changed on open complaints");
        if (canonicalPriority(c.priority) == priority)
            throw ApiError(400, "Priority is already " + priority);
        c.priority = priority;
        store_.updateComplaint(c);

        recordHistory(c.id, "Priority: " + priority, actorOf(p));
        if (c.isAssigned() && isActiveWorkStatus(c.status))
            notify("worker", c.workerId, c.id,
                   "Task #" + std::to_string(c.id) + " priority changed to " + priority + ".");

        tx.commit();

        Reply reply;
        reply.body["message"] = "Priority updated";
        reply.body["priority"] = priority;
        return reply;
    }

    Reply Api::assignComplaint(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Admin});
        Body body(request.body);
        int id = idFromBody(body, "complaintId");
        std::string workerId = trim(body.text("workerId"));
        bool force = body.boolean("override", false);

        std::lock_guard<std::mutex> lock(dataMutex_);
        DataStore::Transaction tx(store_);
        Snapshot s = loadSnapshot();
        Complaint &c = findComplaint(s.complaints, id);

        if (!canAssignFrom(c.status))
            throw ApiError(400, "Complaint #" + std::to_string(id) + " is " + c.status +
                                    ". Only Verified, Forwarded, Escalated or already-assigned "
                                    "complaints can be assigned" +
                                    (c.status == "Pending" ? " (verify it first)." : "."));

        auto w = s.workers.find(workerId);
        if (w == s.workers.end())
            throw ApiError(404, "Worker " + workerId + " not found");
        if (!w->second.active)
            throw ApiError(400, "Worker " + workerId + " is inactive");
        if (c.workerId == workerId && c.status == "Assigned")
            throw ApiError(400, "Complaint is already assigned to " + w->second.name);

        const std::string required = departmentForComplaint(c.category, c.description);
        if (canonicalDepartment(w->second.department) != required && !force)
            throw ApiError(409, w->second.name + " works in " + w->second.department +
                                    ", but this complaint needs " + required + ". Assign anyway?",
                           "DEPARTMENT_MISMATCH");

        int load = 0;
        for (const auto &other : s.complaints)
            if (other.workerId == workerId && isActiveWorkStatus(other.status) && other.id != id)
                ++load;
        if (load >= kMaxActiveTasksPerWorker && !force)
            throw ApiError(409, w->second.name + " already has " + std::to_string(load) +
                                    " active tasks. Assign anyway?",
                           "AT_CAPACITY");

        auto av = s.availability.find(workerId);
        DutyStatus duty = dutyStatus(av == s.availability.end() ? nullptr : &av->second, nowEpoch());
        if (!duty.assignable && !force)
            throw ApiError(409, w->second.name + ": " + duty.label + ". Assign anyway?", "OFF_DUTY");

        std::string previous = assignInMemory(c, w->second);
        store_.updateComplaint(c);
        announceAssignment(c, w->second, previous, actorOf(p));

        tx.commit();

        Reply reply;
        reply.body["message"] = "Assigned to " + w->second.name;
        reply.body["workerId"] = workerId;
        reply.body["status"] = "Assigned";
        return reply;
    }

    Reply Api::submitFeedback(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Student});
        int id = request.params.at(0);
        Body body(request.body);
        int rating = body.integer("rating");
        if (rating < 1 || rating > 5)
            throw ApiError(400, "Rating must be between 1 and 5");
        std::string comment = requireText(body.text("comment", false), "Comment", 300, 0);

        std::lock_guard<std::mutex> lock(dataMutex_);
        DataStore::Transaction tx(store_);
        auto complaints = store_.loadComplaints();
        Complaint &c = findComplaint(complaints, id);
        if (c.studentId != p.id)
            throw ApiError(404, "Complaint #" + std::to_string(id) + " not found");
        if (c.status != "Resolved")
            throw ApiError(400, "You can rate a complaint only after it is resolved");
        for (const auto &f : store_.loadFeedback())
            if (f.complaintId == id)
                throw ApiError(409, "You have already rated this complaint");

        Feedback fb{id, p.id, c.isAssigned() ? c.workerId : "", rating, comment, nowTimestamp()};
        store_.appendFeedback(fb);

        if (!fb.workerId.empty())
            notify("worker", fb.workerId, id,
                   "Complaint #" + std::to_string(id) + " was rated " + std::to_string(rating) + "/5.");
        if (rating <= 2)
            notifyAdmins(id, "Low rating (" + std::to_string(rating) + "/5) on complaint #" +
                                 std::to_string(id) + (comment.empty() ? "" : ": " + comment));

        tx.commit();

        Reply reply;
        reply.status = 201;
        reply.body["message"] = "Thank you for your feedback";
        return reply;
    }
}
