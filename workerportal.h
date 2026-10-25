#ifndef WORKER_PORTAL_H
#define WORKER_PORTAL_H

#include <string>
#include <vector>

#include "campusgraph.h"
#include "dispatchmanager.h"
#include "worker.h"

using namespace std;

class WorkerPortal {
private:
    vector<Worker> workers;
    CampusGraph& campusGraph;
    DispatchManager dispatchManager;

    Worker* loggedInWorker;

    void clearInputBuffer();
    int readInteger(const string& message);
    string readLine(const string& message);

    void workerLogin();
    void showWorkerMenu();
    void viewAssignedTasks();
    void viewNextPriorityTask();
    void acceptTask();
    void startWork();
    void resolveTask();
    void escalateTask();
    void viewCompletedTasks();
    void viewShortestRoute();

public:
    explicit WorkerPortal(CampusGraph& graph);

    void run();
};

#endif
