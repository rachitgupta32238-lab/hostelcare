#ifndef WORKER_H
#define WORKER_H

#include <string>

using namespace std;

class Worker {
private:
    string workerId;
    string password;
    string name;
    string department;
    int currentLocationId;
    int activeTasks;
    bool available;

public:
    Worker();

    Worker(
        const string& workerId,
        const string& password,
        const string& name,
        const string& department,
        int currentLocationId
    );

    string getWorkerId() const;
    string getName() const;
    string getDepartment() const;
    int getCurrentLocationId() const;
    int getActiveTasks() const;
    bool isAvailable() const;

    bool checkPassword(const string& enteredPassword) const;

    void setCurrentLocationId(int locationId);
    void setActiveTasks(int tasks);
    void setAvailability(bool status);

    void displayWorker() const;
};

#endif