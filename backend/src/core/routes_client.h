#pragma once
// Real-world travel distance and time from the Google Maps Routes API
// (computeRoutes). Google marked the older Directions / Distance Matrix APIs
// as Legacy in March 2025, so they cannot be enabled on new projects.
//
// The server key stays on the server; the browser never sees it.

#include <memory>
#include <string>

namespace hc
{
    struct LatLng
    {
        double latitude = 0, longitude = 0;
    };

    struct TravelResult
    {
        bool ok = false;
        int distanceMeters = 0;
        int durationSeconds = 0;
        std::string error; // why it failed (shown to the user as a fallback reason)
    };

    class RoutesClient
    {
    public:
        virtual ~RoutesClient() = default;
        virtual TravelResult compute(const LatLng &from, const LatLng &to) = 0;
        virtual std::string travelMode() const = 0; // WALK, TWO_WHEELER, DRIVE ...
    };

    // Google implementation (libcurl). Results are cached for 10 minutes per
    // origin/destination pair to keep API usage and cost low.
    std::unique_ptr<RoutesClient> makeGoogleRoutesClient(const std::string &apiKey, const std::string &endpoint,
                                                         const std::string &travelMode, long timeoutSeconds = 6);

    bool validCoordinates(double latitude, double longitude);
}
