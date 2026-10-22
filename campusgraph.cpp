#include "campusgraph.h"

#include <algorithm>
#include <climits>
#include <iostream>
#include <queue>

using namespace std;

bool CampusGraph::isValidLocationId(int locationId) const {
    return locationId >= 0 &&
           locationId < static_cast<int>(locationNames.size());
}

void CampusGraph::addLocation(const string& locationName) {
    if (locationIdMap.find(locationName) != locationIdMap.end()) {
        cout << "Location already exists: " << locationName << "\n";
        return;
    }

    int newId = static_cast<int>(locationNames.size());

    locationIdMap[locationName] = newId;
    locationNames.push_back(locationName);
    adjacencyList.push_back({});
}

int CampusGraph::getLocationId(const string& locationName) const {
    auto it = locationIdMap.find(locationName);

    if (it == locationIdMap.end()) {
        return -1;
    }

    return it->second;
}

string CampusGraph::getLocationName(int locationId) const {
    if (!isValidLocationId(locationId)) {
        return "Unknown Location";
    }

    return locationNames[locationId];
}

int CampusGraph::getLocationCount() const {
    return static_cast<int>(locationNames.size());
}

void CampusGraph::addPath(
    const string& sourceName,
    const string& destinationName,
    int distance
) {
    int source = getLocationId(sourceName);
    int destination = getLocationId(destinationName);

    if (source == -1 || destination == -1) {
        cout << "Invalid location name.\n";
        return;
    }

    if (distance < 0) {
        cout << "Distance cannot be negative.\n";
        return;
    }

    adjacencyList[source].push_back({destination, distance});
    adjacencyList[destination].push_back({source, distance});
}

ShortestPathResult CampusGraph::findShortestPath(
    int sourceLocationId,
    int destinationLocationId
) const {
    ShortestPathResult result;
    result.totalDistance = -1;
    result.pathExists = false;

    int totalLocations = getLocationCount();

    if (
        !isValidLocationId(sourceLocationId) ||
        !isValidLocationId(destinationLocationId)
    ) {
        cout << "Invalid source or destination location.\n";
        return result;
    }

    vector<int> distance(totalLocations, INT_MAX);
    vector<int> parent(totalLocations, -1);

    priority_queue<
        pair<int, int>,
        vector<pair<int, int>>,
        greater<pair<int, int>>
    > minHeap;

    distance[sourceLocationId] = 0;
    minHeap.push({0, sourceLocationId});

    while (!minHeap.empty()) {
        int currentDistance = minHeap.top().first;
        int currentNode = minHeap.top().second;
        minHeap.pop();

        if (currentDistance > distance[currentNode]) {
            continue;
        }

        if (currentNode == destinationLocationId) {
            break;
        }

        for (const Edge& edge : adjacencyList[currentNode]) {
            int neighbour = edge.destination;
            int newDistance = currentDistance + edge.distance;

            if (newDistance < distance[neighbour]) {
                distance[neighbour] = newDistance;
                parent[neighbour] = currentNode;
                minHeap.push({newDistance, neighbour});
            }
        }
    }

    if (distance[destinationLocationId] == INT_MAX) {
        return result;
    }

    result.pathExists = true;
    result.totalDistance = distance[destinationLocationId];

    for (
        int node = destinationLocationId;
        node != -1;
        node = parent[node]
    ) {
        result.path.push_back(node);
    }

    reverse(result.path.begin(), result.path.end());

    return result;
}

void CampusGraph::displayCampusMap() const {
    cout << "\n========== CAMPUS MAP ==========\n";

    for (int i = 0; i < getLocationCount(); i++) {
        cout << i << ". " << locationNames[i] << "\n";

        for (const Edge& edge : adjacencyList[i]) {
            cout << "   -> "
                 << locationNames[edge.destination]
                 << " (" << edge.distance << " m)\n";
        }
    }
}

void CampusGraph::loadDefaultCampusMap() {
    addLocation("Electrical Office");
    addLocation("Plumbing Office");
    addLocation("IT Support Office");
    addLocation("Main Gate");
    addLocation("Junction 1");
    addLocation("Junction 2");

    addLocation("Atlantis Hostel");
    addLocation("SC Bose Hostel");
    addLocation("Bhagat Singh Hostel");
    addLocation("Sardar Patel Hostel");

    addLocation("Security Office");

    addPath("Electrical Office", "Junction 1", 120);
    addPath("Plumbing Office", "Junction 1", 90);
    addPath("IT Support Office", "Junction 2", 110);
    addPath("Main Gate", "Junction 1", 200);

    addPath("Junction 1", "Atlantis Hostel", 80);
    addPath("Junction 1", "SC Bose Hostel", 150);
    addPath("Atlantis Hostel", "Bhagat Singh Hostel", 100);

    addPath("Junction 2", "Sardar Patel Hostel", 130);
    addPath("Junction 2", "Security Office", 70);
}