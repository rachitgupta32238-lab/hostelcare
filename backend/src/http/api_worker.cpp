#include "api.h"

#include <algorithm>
#include <cstdio>
#include <map>

namespace hc
{
    // ============================ Worker portal ============================

    Reply Api::workerTasks(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Worker});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();
        const std::time_t now = nowEpoch();

        std::string department;
        auto self = s.workers.find(p.id);
        if (self != s.workers.end())
            department = self->second.department;
        const int office = officeForDepartment(department);

        std::vector<QueueItem> items;
        for (const auto &c : s.complaints)
            if (c.workerId == p.id && isActiveWorkStatus(c.status))
            {
                std::time_t at = submittedAt(s, c.id);
                items.push_back({c, effectivePriorityRank(c.priority, at, now), at});
            }

        Json::list tasks;
        int position = 0;
        for (const auto &item : priorityOrder(items)) // priority-queue order
        {
            Json task = complaintJson(item.complaint, s, Role::Worker);
            task["queuePosition"] = ++position;
            task["route"] = routeJson(hc::shortestRoute(office, locationForHostel(item.complaint.hostelName)));
            tasks.push_back(std::move(task));
        }
        Reply reply;
        reply.body["workerId"] = p.id;
        reply.body["office"] = locationName(office);
        reply.body["count"] = tasks.size();
        reply.body["tasks"] = std::move(tasks);
        return reply;
    }

    Reply Api::workerNextTask(const HttpRequest &request)
    {
        // The next task is the top of this worker's priority queue.
        Reply reply;
        Principal p = requireAuth(request, {Role::Worker});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();
        const std::time_t now = nowEpoch();
        std::vector<QueueItem> items;
        for (const auto &c : s.complaints)
            if (c.workerId == p.id && isActiveWorkStatus(c.status))
            {
                std::time_t at = submittedAt(s, c.id);
                items.push_back({c, effectivePriorityRank(c.priority, at, now), at});
            }
        auto ordered = priorityOrder(items);
        if (ordered.empty())
        {
            reply.body["task"] = Json();
            reply.body["message"] = "No pending tasks";
            return reply;
        }
        std::string department;
        auto self = s.workers.find(p.id);
        if (self != s.workers.end())
            department = self->second.department;
        Json task = complaintJson(ordered.front().complaint, s, Role::Worker);
        task["route"] = routeJson(hc::shortestRoute(officeForDepartment(department),
                                                    locationForHostel(ordered.front().complaint.hostelName)));
        reply.body["task"] = std::move(task);
        reply.body["remaining"] = static_cast<int>(ordered.size()) - 1;
        return reply;
    }

    Reply Api::workerCompleted(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Worker});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Snapshot s = loadSnapshot();
        Json::list tasks;
        int ratings = 0, sum = 0;
        for (auto it = s.complaints.rbegin(); it != s.complaints.rend(); ++it)
            if (it->workerId == p.id && it->status == "Resolved")
            {
                tasks.push_back(complaintJson(*it, s, Role::Worker));
                auto f = s.feedback.find(it->id);
                if (f != s.feedback.end())
                {
                    ++ratings;
                    sum += f->second.rating;
                }
            }
        Reply reply;
        reply.body["workerId"] = p.id;
        reply.body["count"] = tasks.size();
        reply.body["tasks"] = std::move(tasks);
        reply.body["ratingsCount"] = ratings;
        reply.body["averageRating"] = ratings ? static_cast<double>(sum) / ratings : 0.0;
        return reply;
    }

    Reply Api::workerTransition(const HttpRequest &request, const std::string &from1,
                                const std::string &from2, const std::string &to, bool needsNote)
    {
        Principal p = requireAuth(request, {Role::Worker});
        Body body(request.body);
        int id = idFromBody(body, "complaintId");
        std::string note;
        if (needsNote)
            note = requireText(body.text("note"), to == "Resolved" ? "Resolution details" : "Escalation reason",
                               300, 3);

        std::lock_guard<std::mutex> lock(dataMutex_);
        DataStore::Transaction tx(store_);
        auto complaints = store_.loadComplaints();
        Complaint &c = findComplaint(complaints, id);
        if (c.workerId != p.id)
            throw ApiError(403, "This task is not assigned to you");
        if (c.status != from1 && c.status != from2)
            throw ApiError(400, "Task #" + std::to_string(id) + " is " + c.status + "; it must be " + from1 +
                                    (from2.empty() ? "" : " or " + from2) + " for this action");

        c.status = to;
        if (to == "Resolved")
            c.feedback = note;
        else if (to == "Escalated")
            c.feedback = "Escalated by " + p.id + ": " + note;
        store_.updateComplaint(c);
        recordHistory(id, to, actorOf(p));

        const std::string label = "Complaint #" + std::to_string(id);
        if (to == "Accepted")
            notify("student", c.studentId, id, label + " was accepted by the maintenance worker.");
        else if (to == "In Progress")
            notify("student", c.studentId, id, label + ": work has started.");
        else if (to == "Resolved")
        {
            notify("student", c.studentId, id, label + " is resolved: " + note + ". Please rate the service.");
            notifyAdmins(id, label + " resolved by " + p.id + ".");
        }
        else if (to == "Escalated")
        {
            notify("student", c.studentId, id, label + " needs further review by the warden.");
            notifyAdmins(id, label + " escalated by " + p.id + ": " + note);
        }

        tx.commit();

        Reply reply;
        reply.body["message"] = "Task #" + std::to_string(id) + " is now " + to;
        reply.body["status"] = to;
        return reply;
    }

    Reply Api::workerAccept(const HttpRequest &r) { return workerTransition(r, "Assigned", "", "Accepted", false); }
    Reply Api::workerStart(const HttpRequest &r) { return workerTransition(r, "Accepted", "", "In Progress", false); }
    Reply Api::workerResolve(const HttpRequest &r) { return workerTransition(r, "In Progress", "", "Resolved", true); }
    Reply Api::workerEscalate(const HttpRequest &r) { return workerTransition(r, "Accepted", "In Progress", "Escalated", true); }

    // ============================ Notifications ============================

    Reply Api::listNotifications(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Admin, Role::Worker, Role::Student});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Json::list list;
        for (const auto &n : store_.listNotifications(roleName(p.role), p.id, 30))
        {
            Json item;
            item["id"] = n.id;
            item["complaintId"] = n.complaintId;
            item["message"] = n.message;
            item["timestamp"] = n.timestamp;
            item["read"] = n.read;
            list.push_back(std::move(item));
        }
        Reply reply;
        reply.body["unread"] = store_.unreadNotifications(roleName(p.role), p.id);
        reply.body["notifications"] = std::move(list);
        return reply;
    }

    Reply Api::markNotificationsRead(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Admin, Role::Worker, Role::Student});
        Body body(request.body);
        int only = body.has("id") ? body.integer("id") : 0; // 0 = mark all
        std::lock_guard<std::mutex> lock(dataMutex_);
        Reply reply;
        reply.body["updated"] = store_.markNotificationsRead(roleName(p.role), p.id, only);
        return reply;
    }

    // ============================ Availability ============================

    Json Api::availabilityJson(const Availability *a, const DutyStatus &duty) const
    {
        Json json;
        json["configured"] = a != nullptr && a->configured;
        json["state"] = duty.state;
        json["label"] = duty.label;
        json["assignable"] = duty.assignable;
        Json::list days;
        for (int d = 0; d < 7; ++d)
        {
            Shift shift = a ? a->days[d] : Shift{};
            Json day;
            day["day"] = dayName(d);
            day["working"] = shift.working;
            day["start"] = formatMinutes(shift.start);
            day["end"] = formatMinutes(shift.end);
            days.push_back(std::move(day));
        }
        json["days"] = std::move(days);
        json["leaveFrom"] = a ? a->leaveFrom : "";
        json["leaveTo"] = a ? a->leaveTo : "";
        json["note"] = a ? a->note : "";
        json["updatedAt"] = a ? a->updatedAt : "";
        return json;
    }

    namespace
    {
        // "HH:MM" -> minutes after midnight, or -1 if invalid.
        int parseClock(const std::string &text)
        {
            if (text.size() != 5 || text[2] != ':')
                return -1;
            for (int i : {0, 1, 3, 4})
                if (text[i] < '0' || text[i] > '9')
                    return -1;
            int h = (text[0] - '0') * 10 + (text[1] - '0');
            int m = (text[3] - '0') * 10 + (text[4] - '0');
            return h < 24 && m < 60 ? h * 60 + m : -1;
        }

        bool validDate(const std::string &text)
        {
            if (text.size() != 10 || text[4] != '-' || text[7] != '-')
                return false;
            for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
                if (text[i] < '0' || text[i] > '9')
                    return false;
            int month = std::stoi(text.substr(5, 2)), day = std::stoi(text.substr(8, 2));
            return month >= 1 && month <= 12 && day >= 1 && day <= 31;
        }
    }

    Reply Api::getAvailability(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Worker});
        std::lock_guard<std::mutex> lock(dataMutex_);
        auto all = store_.loadAvailability();
        auto it = all.find(p.id);
        const Availability *a = it == all.end() ? nullptr : &it->second;
        Reply reply;
        reply.body["availability"] = availabilityJson(a, dutyStatus(a, nowEpoch()));
        return reply;
    }

    Reply Api::saveAvailability(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Worker});
        Body body(request.body);
        if (body.isEmpty() || !body.raw().has("days") || body.raw()["days"].t() != crow::json::type::List ||
            body.raw()["days"].size() != 7)
            throw ApiError(400, "days must list all 7 days (Monday first)");

        Availability a;
        a.workerId = p.id;
        a.configured = true;
        const auto &days = body.raw()["days"];
        for (int d = 0; d < 7; ++d)
        {
            const auto &day = days[d];
            if (day.t() != crow::json::type::Object || !day.has("working"))
                throw ApiError(400, std::string(dayName(d)) + ": working is required");
            a.days[d].working = day["working"].t() == crow::json::type::True;
            if (!a.days[d].working)
                continue;
            int start = day.has("start") && day["start"].t() == crow::json::type::String
                            ? parseClock(std::string(day["start"].s()))
                            : -1;
            int end = day.has("end") && day["end"].t() == crow::json::type::String
                          ? parseClock(std::string(day["end"].s()))
                          : -1;
            if (start < 0 || end < 0)
                throw ApiError(400, std::string(dayName(d)) + ": use HH:MM times, e.g. 09:00");
            a.days[d].start = start;
            a.days[d].end = end;
        }

        a.leaveFrom = trim(body.text("leaveFrom", false));
        a.leaveTo = trim(body.text("leaveTo", false));
        if (a.leaveFrom.empty() != a.leaveTo.empty())
            throw ApiError(400, "Give both a leave start and end date, or neither");
        if (!a.leaveFrom.empty() && (!validDate(a.leaveFrom) || !validDate(a.leaveTo)))
            throw ApiError(400, "Leave dates must be YYYY-MM-DD");
        if (!a.leaveFrom.empty() && a.leaveTo < a.leaveFrom)
            throw ApiError(400, "Leave end date is before the start date");
        a.note = requireText(body.text("note", false), "Note", 200, 0);
        a.updatedAt = nowTimestamp();

        std::lock_guard<std::mutex> lock(dataMutex_);
        DataStore::Transaction tx(store_);
        auto previous = store_.loadAvailability();
        auto old = previous.find(p.id);
        bool leaveChanged = old == previous.end() ? !a.leaveFrom.empty()
                                                  : (old->second.leaveFrom != a.leaveFrom || old->second.leaveTo != a.leaveTo);
        store_.saveAvailability(a);
        if (leaveChanged && !a.leaveFrom.empty())
        {
            auto worker = store_.findWorker(p.id);
            notifyAdmins(0, (worker ? worker->name : p.id) + " (" + p.id + ") will be on leave from " + a.leaveFrom +
                                " to " + a.leaveTo + (a.note.empty() ? "" : ": " + a.note));
        }
        tx.commit();

        DutyStatus duty = dutyStatus(&a, nowEpoch());
        Reply reply;
        reply.body["message"] = "Availability saved. You are now shown as: " + duty.label;
        reply.body["availability"] = availabilityJson(&a, duty);
        return reply;
    }

    // ============================ Maps & directions ============================

    namespace
    {
        std::string mapsTravelMode(const std::string &mode)
        {
            if (mode == "DRIVE")
                return "driving";
            if (mode == "BICYCLE")
                return "bicycling";
            if (mode == "TWO_WHEELER")
                return "two-wheeler";
            return "walking";
        }

        std::string coordText(double lat, double lng)
        {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.6f,%.6f", lat, lng);
            return buffer;
        }

        Json locationJson(const LocationRecord &l)
        {
            Json json;
            json["id"] = l.id;
            json["name"] = l.name;
            json["hasCoordinates"] = l.hasCoordinates;
            if (l.hasCoordinates)
            {
                json["latitude"] = l.latitude;
                json["longitude"] = l.longitude;
            }
            return json;
        }
    }

    Reply Api::listLocations(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin, Role::Worker});
        std::lock_guard<std::mutex> lock(dataMutex_);
        Json::list list;
        for (const auto &l : store_.loadLocations())
            list.push_back(locationJson(l));
        Reply reply;
        reply.body["locations"] = std::move(list);
        return reply;
    }

    Reply Api::updateLocation(const HttpRequest &request)
    {
        requireAuth(request, {Role::Admin});
        int id = request.params.at(0);
        Body body(request.body);
        bool clear = body.boolean("clear", false);
        double lat = 0, lng = 0;
        if (!clear)
        {
            lat = body.number("latitude");
            lng = body.number("longitude");
            if (!validCoordinates(lat, lng))
                throw ApiError(400, "Latitude must be -90..90 and longitude -180..180");
        }
        std::lock_guard<std::mutex> lock(dataMutex_);
        bool exists = false;
        for (const auto &l : store_.loadLocations())
            exists = exists || l.id == id;
        if (!exists)
            throw ApiError(404, "Location not found");
        store_.setLocationCoordinates(id, !clear, lat, lng);
        Reply reply;
        reply.body["message"] = clear ? "Coordinates removed" : "Coordinates saved";
        return reply;
    }

    Reply Api::taskDirections(const HttpRequest &request)
    {
        Principal p = requireAuth(request, {Role::Worker});
        int id = request.params.at(0);
        Body body(request.body);
        bool live = body.has("latitude") && body.has("longitude");
        LatLng current;
        if (live)
        {
            current = {body.number("latitude"), body.number("longitude")};
            if (!validCoordinates(current.latitude, current.longitude))
                throw ApiError(400, "Invalid current location");
        }

        // 1. Gather everything under the data lock...
        Complaint complaint;
        std::string department;
        std::map<int, LocationRecord> locations;
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            auto c = store_.findComplaint(id);
            if (!c || c->workerId != p.id)
                throw ApiError(404, "Task #" + std::to_string(id) + " not found in your tasks");
            complaint = *c;
            if (auto w = store_.findWorker(p.id))
                department = w->department;
            for (const auto &l : store_.loadLocations())
                locations[l.id] = l;
        }

        const int officeId = officeForDepartment(department);
        const int destinationId = locationForHostel(complaint.hostelName);
        const LocationRecord office = locations.count(officeId) ? locations[officeId] : LocationRecord{officeId, locationName(officeId)};
        const LocationRecord destination =
            locations.count(destinationId) ? locations[destinationId] : LocationRecord{destinationId, locationName(destinationId)};

        Reply reply;
        reply.body["complaintId"] = id;
        reply.body["destination"] = locationJson(destination);
        reply.body["campusRoute"] = routeJson(hc::shortestRoute(officeId, destinationId));

        Json origin;
        bool originHasCoords = live || office.hasCoordinates;
        LatLng from = live ? current : LatLng{office.latitude, office.longitude};
        origin["label"] = live ? "Your current location" : office.name;
        origin["hasCoordinates"] = originHasCoords;
        if (originHasCoords)
        {
            origin["latitude"] = from.latitude;
            origin["longitude"] = from.longitude;
        }
        reply.body["origin"] = std::move(origin);

        const std::string mode = routes_ ? routes_->travelMode() : "WALK";
        reply.body["travelMode"] = mode;
        if (destination.hasCoordinates)
        {
            // Opens Google Maps navigation (no API key needed). Without an
            // origin, Google Maps starts from the phone's own location.
            std::string url = "https://www.google.com/maps/dir/?api=1&destination=" +
                              coordText(destination.latitude, destination.longitude) +
                              "&travelmode=" + mapsTravelMode(mode);
            if (originHasCoords)
                url += "&origin=" + coordText(from.latitude, from.longitude);
            reply.body["navigationUrl"] = url;
        }

        // 2. ...then call Google WITHOUT holding the lock (network can be slow).
        std::string fallbackReason;
        if (!routes_)
            fallbackReason = "Google distance is not configured on the server";
        else if (!destination.hasCoordinates)
            fallbackReason = "The warden has not set map coordinates for " + destination.name;
        else if (!originHasCoords)
            fallbackReason = "No coordinates for " + office.name + "; share your location for an exact distance";
        else
        {
            TravelResult travel = routes_->compute(from, {destination.latitude, destination.longitude});
            if (travel.ok)
            {
                std::tm tm = localTimeOf(nowEpoch());
                reply.body["source"] = "google";
                reply.body["distanceMeters"] = travel.distanceMeters;
                reply.body["durationSeconds"] = travel.durationSeconds;
                reply.body["attribution"] = "Powered by Google, ©" + std::to_string(tm.tm_year + 1900) + " Google";
                return reply;
            }
            fallbackReason = travel.error;
        }

        RouteInfo campus = hc::shortestRoute(officeId, destinationId);
        reply.body["source"] = "campus-map";
        reply.body["distanceMeters"] = campus.distance;
        reply.body["message"] = fallbackReason + ". Showing the approximate campus-map distance from " + office.name + ".";
        return reply;
    }

    // ============================ Campus routes ============================

    Reply Api::routeLocations(const HttpRequest &)
    {
        Json::list list;
        for (const auto &loc : campusLocations())
        {
            Json item;
            item["id"] = loc.id;
            item["name"] = loc.name;
            list.push_back(std::move(item));
        }
        Reply reply;
        reply.body["locations"] = std::move(list);
        return reply;
    }

    Reply Api::shortestRoute(const HttpRequest &request)
    {
        RouteInfo route = hc::shortestRoute(request.params.at(0), request.params.at(1));
        Reply reply;
        reply.body = routeJson(route);
        return reply;
    }
}
