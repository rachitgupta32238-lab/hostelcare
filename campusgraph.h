#ifndef CAMPUS_GRAPH_H
#define CAMPUS_GRAPH_H

#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct Edge {
    int destination;
    int distance;
};

struct ShortestPathResult {
    int totalDistance;
    vector<int> path;
    bool pathExists;
};

class CampusGraph {
private:
    vector<vector<Edge>> adjacencyList;
    unordered_map<string, int> locationIdMap;
    vector<string> locationNames;

    bool isValidLocationId(int locationId) const;

public:
    void addLocation(const string& locationName);

    void addPath(
        const string& sourceName,
        const string& destinationName,
        int distance
    );

    int getLocationId(const string& locationName) const;
    string getLocationName(int locationId) const;

    int getLocationCount() const;

    ShortestPathResult findShortestPath(
        int sourceLocationId,
        int destinationLocationId
    ) const;

    void displayCampusMap() const;
    void loadDefaultCampusMap();
};

#endif
