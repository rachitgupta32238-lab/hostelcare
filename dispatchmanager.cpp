#include "dispatchmanager.h"

#include <climits>
#include <iostream>

using namespace std;

// Constructor
DispatchManager::DispatchManager(
    vector<Worker>& workerList,
    CampusGraph& graph
) : workers(workerList),
    campusGraph(graph) {
}

// Find the original worker object by ID.
Worker* DispatchManager::findWorkerById(
    const string& workerId
) {
    for (Worker& worker : workers) {
        if (worker.getWorkerId() == workerId) {
            return &worker;
        }
    }

    return nullptr;
}

// Find the nearest available worker in the required department.
// This function only selects a worker; it does not assign a task.
DispatchResult DispatchManager::findNearestWorker(
    const string& requiredDepartment,
    int complaintLocationId
) {
    DispatchResult result{};

    result.success = false;
    result.route.pathExists = false;
    result.route.totalDistance = -1;

    if (
        complaintLocationId < 0 ||
        complaintLocationId >= campusGraph.getLocationCount()
    ) {
        result.message = "Invalid complaint location ID.";
        return result;
    }

    int minimumDistance = INT_MAX;
    int minimumTasks = INT_MAX;

    for (const Worker& worker : workers) {
        if (worker.getDepartment() != requiredDepartment) {
            continue;
        }

        if (!worker.isAvailable()) {
            continue;
        }

        ShortestPathResult route =
            campusGraph.findShortestPath(
                worker.getCurrentLocationId(),
                complaintLocationId
            );

        if (!route.pathExists) {
            continue;
        }

        int distance = route.totalDistance;
        int activeTasks = worker.getActiveTasks();

        bool betterCandidate = false;

        if (!result.success) {
            betterCandidate = true;
        }
        else if (distance < minimumDistance) {
            betterCandidate = true;
        }
        else if (
            distance == minimumDistance &&
            activeTasks < minimumTasks
        ) {
            betterCandidate = true;
        }
        else if (
            distance == minimumDistance &&
            activeTasks == minimumTasks &&
            worker.getWorkerId() <
                result.assignedWorker.getWorkerId()
        ) {
            betterCandidate = true;
        }

        if (betterCandidate) {
            minimumDistance = distance;
            minimumTasks = activeTasks;

            result.assignedWorker = worker;
            result.route = route;
            result.success = true;
        }
    }

    if (!result.success) {
        result.message =
            "No available, reachable worker found for department: "
            + requiredDepartment;

        return result;
    }

    result.message =
        "Nearest available worker found successfully.";

    return result;
}

// Display the selected worker and route.
void DispatchManager::displayNearestWorkerResult(
    const DispatchResult& result,
    int complaintLocationId
) const {
    cout << "\n========== DISPATCH RESULT ==========\n";

    if (!result.success) {
        cout << result.message << "\n";
        return;
    }

    const Worker& worker = result.assignedWorker;

    cout << "Selected Worker ID: "
         << worker.getWorkerId() << "\n";

    cout << "Worker Name: "
         << worker.getName() << "\n";

    cout << "Department: "
         << worker.getDepartment() << "\n";

    cout << "Current Location: "
         << campusGraph.getLocationName(
                worker.getCurrentLocationId()
            )
         << "\n";

    cout << "Complaint Location: "
         << campusGraph.getLocationName(
                complaintLocationId
            )
         << "\n";

    cout << "Active Tasks: "
         << worker.getActiveTasks() << "\n";

    cout << "Total Distance: "
         << result.route.totalDistance
         << " metres\n";

    cout << "\nShortest Route:\n";

    for (size_t i = 0; i < result.route.path.size(); ++i) {
        cout << campusGraph.getLocationName(
                    result.route.path[i]
                );

        if (i + 1 < result.route.path.size()) {
            cout << " -> ";
        }
    }

    cout << "\n";
    cout << "Selection only: confirm assignment to reserve this worker.\n";
}

// Reserve the original worker after assignment is confirmed.
// The complaint record must be updated separately by your
// complaint/dispatch integration code.
void DispatchManager::assignWorkerToComplaint(
    const string& workerId
) {
    Worker* worker = findWorkerById(workerId);

    if (worker == nullptr) {
        cout << "Worker not found.\n";
        return;
    }

    if (!worker->isAvailable()) {
        cout << "Worker is unavailable. Please select another worker.\n";
        return;
    }

    worker->setActiveTasks(
        worker->getActiveTasks() + 1
    );

    worker->setAvailability(false);

    cout << "Worker "
         << worker->getWorkerId()
         << " reserved successfully.\n";
}

// Release the worker after a real assigned task is completed.
// Call this only once per completed assignment.
void DispatchManager::releaseWorker(
    const string& workerId
) {
    Worker* worker = findWorkerById(workerId);

    if (worker == nullptr) {
        cout << "Worker not found.\n";
        return;
    }

    if (worker->getActiveTasks() == 0) {
        cout << "Worker has no active task to release.\n";
        return;
    }

    worker->setActiveTasks(
        worker->getActiveTasks() - 1
    );

    worker->setAvailability(
        worker->getActiveTasks() == 0
    );

    cout << "Worker "
         << worker->getWorkerId()
         << " released from the completed task.\n";
}