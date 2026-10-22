#ifndef DISPATCH_MANAGER_H
#define DISPATCH_MANAGER_H

#include <stdexcept>
#include <string>
#include <vector>

#include "campusgraph.h"
#include "worker.h"

using namespace std;

struct DispatchResult {
    bool success;
    Worker assignedWorker;
    ShortestPathResult route;
    string message;
};

class DispatchManager {
private:
    vector<Worker>& workers;
    CampusGraph& campusGraph;

public:
    DispatchManager(
        vector<Worker>& workerList,
        CampusGraph& graph
    );

    Worker* findWorkerById(const string& workerId);

    DispatchResult findNearestWorker(
        const string& requiredDepartment,
        int complaintLocationId
    );

    void displayNearestWorkerResult(
        const DispatchResult& result,
        int complaintLocationId
    ) const;

    void assignWorkerToComplaint(
        const string& workerId
    );

    void releaseWorker(
        const string& workerId
    );
};

#endif