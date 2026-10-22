#include "worker.h"
#include <iostream>

using namespace std;

Worker::Worker() {
    currentLocationId = -1;
    activeTasks = 0;
    available = true;
}

Worker::Worker(
    const string& workerId,
    const string& password,
    const string& name,
    const string& department,
    int currentLocationId
) {
    this->workerId = workerId;
    this->password = password;
    this->name = name;
    this->department = department;
    this->currentLocationId = currentLocationId;
    this->activeTasks = 0;
    this->available = true;
}

string Worker::getWorkerId() const {
    return workerId;
}

string Worker::getName() const {
    return name;
}

string Worker::getDepartment() const {
    return department;
}

int Worker::getCurrentLocationId() const {
    return currentLocationId;
}

int Worker::getActiveTasks() const {
    return activeTasks;
}

bool Worker::isAvailable() const {
    return available;
}

bool Worker::checkPassword(const string& enteredPassword) const {
    return password == enteredPassword;
}

void Worker::setCurrentLocationId(int locationId) {
    currentLocationId = locationId;
}

void Worker::setActiveTasks(int tasks) {
    if (tasks < 0) {
        tasks = 0;
    }

    activeTasks = tasks;
}

void Worker::setAvailability(bool status) {
    available = status;
}

void Worker::displayWorker() const {
    cout << "Worker ID: " << workerId << "\n";
    cout << "Name: " << name << "\n";
    cout << "Department: " << department << "\n";
    cout << "Current location ID: " << currentLocationId << "\n";
    cout << "Active tasks: " << activeTasks << "\n";
    cout << "Availability: "
         << (available ? "Available" : "Unavailable")
         << "\n";
}