#include "workerportal.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {
const string COMPLAINT_FILE = "data/complaints.txt";

struct ComplaintRecord {
    string id, studentId, studentName, roomNo, hostelName;
    string category, description, priority, status, workerId, feedback;
};

string lowerText(string value) {
    transform(value.begin(), value.end(), value.begin(),
              [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return value;
}

bool loadComplaintRecords(vector<ComplaintRecord>& records) {
    records.clear();
    ifstream file(COMPLAINT_FILE);
    if (!file) return false;
    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        ComplaintRecord c;
        getline(ss, c.id, '|');
        getline(ss, c.studentId, '|');
        getline(ss, c.studentName, '|');
        getline(ss, c.roomNo, '|');
        getline(ss, c.hostelName, '|');
        getline(ss, c.category, '|');
        getline(ss, c.description, '|');
        getline(ss, c.priority, '|');
        getline(ss, c.status, '|');
        getline(ss, c.workerId, '|');
        getline(ss, c.feedback);
        if (c.workerId.empty()) c.workerId = "-1";
        records.push_back(c);
    }
    return true;
}

bool saveComplaintRecords(const vector<ComplaintRecord>& records) {
    ofstream file(COMPLAINT_FILE, ios::trunc);
    if (!file) return false;
    for (const auto& c : records) {
        file << c.id << '|' << c.studentId << '|' << c.studentName << '|'
             << c.roomNo << '|' << c.hostelName << '|' << c.category << '|'
             << c.description << '|' << c.priority << '|' << c.status << '|'
             << (c.workerId.empty() ? "-1" : c.workerId) << '|'
             << c.feedback << '\n';
    }
    return static_cast<bool>(file);
}

string departmentForCategory(const string& category) {
    string c = lowerText(category);
    if (c.find("electric") != string::npos || c.find("fan") != string::npos ||
        c.find("light") != string::npos || c.find("power") != string::npos ||
        c.find("wire") != string::npos) return "Electrical";
    if (c.find("plumb") != string::npos || c.find("water") != string::npos ||
        c.find("tap") != string::npos || c.find("pipe") != string::npos ||
        c.find("drain") != string::npos) return "Plumbing";
    if (c.find("it") != string::npos || c.find("internet") != string::npos ||
        c.find("wifi") != string::npos || c.find("computer") != string::npos ||
        c.find("network") != string::npos) return "IT Support";
    if (c.find("security") != string::npos || c.find("guard") != string::npos)
        return "Security";
    return "General Maintenance";
}

int priorityRank(const string& priority) {
    string p = lowerText(priority);
    if (p == "critical" || p == "urgent") return 0;
    if (p == "high") return 1;
    if (p == "medium") return 2;
    if (p == "low") return 3;
    return 4;
}

int complaintLocation(const CampusGraph& graph, const string& hostel) {
    string h = lowerText(hostel);
    if (h.find("atlantis") != string::npos)
        return graph.getLocationId("Atlantis Hostel");
    if (h.find("bose") != string::npos)
        return graph.getLocationId("SC Bose Hostel");
    if (h.find("bhagat") != string::npos)
        return graph.getLocationId("Bhagat Singh Hostel");
    if (h.find("patel") != string::npos)
        return graph.getLocationId("Sardar Patel Hostel");
    // Unknown hostel name: use the nearest central junction as a safe fallback.
    return graph.getLocationId("Junction 1");
}

bool isWorkerDepartmentMatch(const ComplaintRecord& c, const Worker& w) {
    return departmentForCategory(c.category) == w.getDepartment();
}

void printComplaint(const ComplaintRecord& c) {
    cout << "\nComplaint ID : " << c.id
         << "\nStudent      : " << c.studentName << " (" << c.studentId << ")"
         << "\nRoom/Hostel  : " << c.roomNo << " / " << c.hostelName
         << "\nCategory     : " << c.category
         << "\nDescription  : " << c.description
         << "\nPriority     : " << c.priority
         << "\nStatus       : " << c.status
         << "\nWorker       : "
         << ((c.workerId == "-1" || c.workerId.empty()) ? "Not assigned" : c.workerId)
         << "\nFeedback     : " << c.feedback << "\n";
}
}

WorkerPortal::WorkerPortal(CampusGraph& graph)
    : workers{
          Worker("E101", "worker123", "Aman Sharma", "Electrical", 0),
          Worker("P101", "worker123", "Rohit Kumar", "Plumbing", 1),
          Worker("IT101", "worker123", "Neha Singh", "IT Support", 2),
          Worker("S101", "worker123", "Vikas Rawat", "Security", 10),
          Worker("G101", "worker123", "Karan Joshi", "General Maintenance", 4)
      },
      campusGraph(graph),
      dispatchManager(workers, graph),
      loggedInWorker(nullptr) {}

void WorkerPortal::clearInputBuffer() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

int WorkerPortal::readInteger(const string& message) {
    int value;
    while (true) {
        cout << message;
        if (cin >> value) {
            clearInputBuffer();
            return value;
        }
        cin.clear();
        clearInputBuffer();
        cout << "Invalid input. Please enter a number.\n";
    }
}

string WorkerPortal::readLine(const string& message) {
    string value;
    cout << message;
    getline(cin >> ws, value);
    return value;
}

void WorkerPortal::workerLogin() {
    string workerId = readLine("\nEnter Worker ID: ");
    string password = readLine("Enter Password: ");
    Worker* worker = dispatchManager.findWorkerById(workerId);
    if (worker == nullptr || !worker->checkPassword(password)) {
        cout << "\nInvalid worker ID or password.\n";
        loggedInWorker = nullptr;
        return;
    }
    loggedInWorker = worker;
    cout << "\nLogin successful. Welcome, " << worker->getName() << "!\n";
}

void WorkerPortal::viewAssignedTasks() {
    if (!loggedInWorker) { cout << "Please login first.\n"; return; }
    vector<ComplaintRecord> records;
    if (!loadComplaintRecords(records)) {
        cout << "Could not open data/complaints.txt. Run the program from the project folder.\n";
        return;
    }
    bool found = false;
    cout << "\n========== MY ASSIGNED COMPLAINTS ==========" << endl;
    for (const auto& c : records) {
        if (c.workerId == loggedInWorker->getWorkerId() &&
            c.status != "Resolved" && c.status != "Rejected" && c.status != "Duplicate") {
            printComplaint(c); found = true;
        }
    }
    if (!found) cout << "No active complaints assigned to you.\n";
    cout << "\n========== MY PROFILE ==========" << endl;
    loggedInWorker->displayWorker();
}

void WorkerPortal::viewNextPriorityTask() {
    if (!loggedInWorker) { cout << "Please login first.\n"; return; }
    vector<ComplaintRecord> records;
    if (!loadComplaintRecords(records)) { cout << "Cannot open complaint file.\n"; return; }
    ComplaintRecord* best = nullptr;
    for (auto& c : records) {
        if (c.status == "Forwarded" && (c.workerId == "-1" || c.workerId.empty()) &&
            isWorkerDepartmentMatch(c, *loggedInWorker)) {
            if (!best || priorityRank(c.priority) < priorityRank(best->priority)) best = &c;
        }
    }
    cout << "\n========== NEXT PRIORITY TASK ==========" << endl;
    if (!best) { cout << "No unassigned forwarded complaints match your department.\n"; return; }
    printComplaint(*best);
    cout << "Use Accept Task and enter complaint ID " << best->id << " to accept it.\n";
}

void WorkerPortal::acceptTask() {
    if (!loggedInWorker) { cout << "Please login first.\n"; return; }
    int id = readInteger("Enter Complaint ID to accept: ");
    vector<ComplaintRecord> records;
    if (!loadComplaintRecords(records)) { cout << "Cannot open complaint file.\n"; return; }
    for (auto& c : records) {
        if (c.id != to_string(id)) continue;
        if (c.status != "Forwarded") { cout << "Only forwarded complaints can be accepted. Current status: " << c.status << "\n"; return; }
        if (c.workerId != "-1" && !c.workerId.empty()) { cout << "This complaint is already assigned to worker " << c.workerId << ".\n"; return; }
        if (!isWorkerDepartmentMatch(c, *loggedInWorker)) { cout << "This complaint belongs to the " << departmentForCategory(c.category) << " department, not yours.\n"; return; }
        c.workerId = loggedInWorker->getWorkerId();
        c.status = "Assigned";
        if (!saveComplaintRecords(records)) { cout << "Could not save complaint update.\n"; return; }
        loggedInWorker->setActiveTasks(loggedInWorker->getActiveTasks() + 1);
        loggedInWorker->setAvailability(false);
        cout << "Complaint " << id << " assigned to " << loggedInWorker->getWorkerId() << ".\n";
        return;
    }
    cout << "Complaint not found.\n";
}

void WorkerPortal::startWork() {
    if (!loggedInWorker) { cout << "Please login first.\n"; return; }
    int id = readInteger("Enter Complaint ID to start: ");
    vector<ComplaintRecord> records;
    if (!loadComplaintRecords(records)) { cout << "Cannot open complaint file.\n"; return; }
    for (auto& c : records) {
        if (c.id != to_string(id)) continue;
        if (c.workerId != loggedInWorker->getWorkerId()) { cout << "This complaint is not assigned to you.\n"; return; }
        if (c.status != "Assigned") { cout << "Only Assigned complaints can be started. Current status: " << c.status << "\n"; return; }
        c.status = "In Progress";
        if (!saveComplaintRecords(records)) { cout << "Could not save complaint update.\n"; return; }
        cout << "Complaint " << id << " status updated to In Progress.\n"; return;
    }
    cout << "Complaint not found.\n";
}

void WorkerPortal::resolveTask() {
    if (!loggedInWorker) { cout << "Please login first.\n"; return; }
    int id = readInteger("Enter Complaint ID to resolve: ");
    string note = readLine("Enter resolution note: ");
    vector<ComplaintRecord> records;
    if (!loadComplaintRecords(records)) { cout << "Cannot open complaint file.\n"; return; }
    for (auto& c : records) {
        if (c.id != to_string(id)) continue;
        if (c.workerId != loggedInWorker->getWorkerId()) { cout << "This complaint is not assigned to you.\n"; return; }
        if (c.status != "In Progress" && c.status != "Assigned") { cout << "Complaint must be Assigned or In Progress to resolve. Current status: " << c.status << "\n"; return; }
        c.status = "Resolved";
        c.feedback = "Worker " + loggedInWorker->getWorkerId() + ": " + note;
        if (!saveComplaintRecords(records)) { cout << "Could not save complaint update.\n"; return; }
        int active = loggedInWorker->getActiveTasks();
        if (active > 0) loggedInWorker->setActiveTasks(active - 1);
        loggedInWorker->setAvailability(loggedInWorker->getActiveTasks() == 0);
        cout << "Complaint " << id << " marked Resolved and saved to the shared complaint file.\n"; return;
    }
    cout << "Complaint not found.\n";
}

void WorkerPortal::escalateTask() {
    if (!loggedInWorker) { cout << "Please login first.\n"; return; }
    int id = readInteger("Enter Complaint ID to escalate: ");
    string reason = readLine("Enter escalation reason: ");
    vector<ComplaintRecord> records;
    if (!loadComplaintRecords(records)) { cout << "Cannot open complaint file.\n"; return; }
    for (auto& c : records) {
        if (c.id != to_string(id)) continue;
        if (c.workerId != loggedInWorker->getWorkerId()) { cout << "This complaint is not assigned to you.\n"; return; }
        c.status = "Escalated";
        c.feedback = "Escalated by " + loggedInWorker->getWorkerId() + ": " + reason;
        if (!saveComplaintRecords(records)) { cout << "Could not save complaint update.\n"; return; }
        cout << "Complaint " << id << " escalated and saved for the warden to review.\n"; return;
    }
    cout << "Complaint not found.\n";
}

void WorkerPortal::viewCompletedTasks() {
    if (!loggedInWorker) { cout << "Please login first.\n"; return; }
    vector<ComplaintRecord> records;
    if (!loadComplaintRecords(records)) { cout << "Cannot open complaint file.\n"; return; }
    bool found = false;
    cout << "\n========== COMPLETED TASKS ==========" << endl;
    for (const auto& c : records) {
        if (c.workerId == loggedInWorker->getWorkerId() && c.status == "Resolved") { printComplaint(c); found = true; }
    }
    if (!found) cout << "No completed complaints found.\n";
}

void WorkerPortal::viewShortestRoute() {
    if (!loggedInWorker) { cout << "Please login first.\n"; return; }
    vector<ComplaintRecord> records;
    if (!loadComplaintRecords(records)) { cout << "Cannot open complaint file.\n"; return; }
    bool found = false;
    for (const auto& c : records) {
        if (c.workerId == loggedInWorker->getWorkerId() && c.status != "Resolved") {
            int destination = complaintLocation(campusGraph, c.hostelName);
            ShortestPathResult route = campusGraph.findShortestPath(loggedInWorker->getCurrentLocationId(), destination);
            cout << "\nRoute for complaint " << c.id << " (" << c.hostelName << "):\n";
            if (!route.pathExists) { cout << "No route found.\n"; continue; }
            for (size_t i = 0; i < route.path.size(); ++i) {
                cout << campusGraph.getLocationName(route.path[i]);
                if (i + 1 < route.path.size()) cout << " -> ";
            }
            cout << "\nDistance: " << route.totalDistance << " metres\n";
            found = true;
        }
    }
    if (!found) {
        campusGraph.displayCampusMap();
        int destination = readInteger("Enter destination location ID: ");
        int source = loggedInWorker->getCurrentLocationId();
        ShortestPathResult route = campusGraph.findShortestPath(source, destination);
        if (!route.pathExists) { cout << "No route found.\n"; return; }
        cout << "Shortest route: ";
        for (size_t i = 0; i < route.path.size(); ++i) {
            cout << campusGraph.getLocationName(route.path[i]);
            if (i + 1 < route.path.size()) cout << " -> ";
        }
        cout << "\nDistance: " << route.totalDistance << " metres\n";
    }
}

void WorkerPortal::showWorkerMenu() {
    int choice;
    do {
        cout << "\n========== WORKER PORTAL =========="
             << "\nLogged in as: " << (loggedInWorker ? loggedInWorker->getName() : "Not logged in")
             << "\n1. Login"
             << "\n2. View Assigned Tasks"
             << "\n3. View Next Priority Task"
             << "\n4. Accept Task"
             << "\n5. Start Work"
             << "\n6. Mark Task Resolved"
             << "\n7. Escalate Task"
             << "\n8. View Completed Tasks"
             << "\n9. View Shortest Route"
             << "\n0. Back to Main Menu\n";
        choice = readInteger("Enter your choice: ");
        switch (choice) {
            case 1: workerLogin(); break;
            case 2: viewAssignedTasks(); break;
            case 3: viewNextPriorityTask(); break;
            case 4: acceptTask(); break;
            case 5: startWork(); break;
            case 6: resolveTask(); break;
            case 7: escalateTask(); break;
            case 8: viewCompletedTasks(); break;
            case 9: viewShortestRoute(); break;
            case 0: cout << "Returning to Main Menu...\n"; loggedInWorker = nullptr; break;
            default: cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

void WorkerPortal::run() {
    showWorkerMenu();
}
