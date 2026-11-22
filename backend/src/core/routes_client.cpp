#include "routes_client.h"
#include "util.h"

#include "crow/json.h"
#include <curl/curl.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>

namespace hc
{
    bool validCoordinates(double latitude, double longitude)
    {
        return std::isfinite(latitude) && std::isfinite(longitude) && latitude >= -90 && latitude <= 90 &&
               longitude >= -180 && longitude <= 180 && !(latitude == 0 && longitude == 0);
    }

    namespace
    {
        std::once_flag curlInit;

        std::size_t appendBody(char *data, std::size_t size, std::size_t count, void *target)
        {
            static_cast<std::string *>(target)->append(data, size * count);
            return size * count;
        }

        std::string point(const LatLng &p)
        {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "{\"location\":{\"latLng\":{\"latitude\":%.7f,\"longitude\":%.7f}}}",
                          p.latitude, p.longitude);
            return buffer;
        }

        // "123s" or "123.5s" -> 123
        int parseDuration(const std::string &text)
        {
            try
            {
                return static_cast<int>(std::lround(std::stod(text)));
            }
            catch (...)
            {
                return 0;
            }
        }

        class GoogleRoutesClient : public RoutesClient
        {
        public:
            GoogleRoutesClient(std::string key, std::string endpoint, std::string mode, long timeout)
                : key_(std::move(key)), endpoint_(std::move(endpoint)), mode_(std::move(mode)), timeout_(timeout)
            {
                std::call_once(curlInit, []
                               { curl_global_init(CURL_GLOBAL_DEFAULT); });
            }

            std::string travelMode() const override { return mode_; }

            TravelResult compute(const LatLng &from, const LatLng &to) override
            {
                char keyBuffer[128];
                std::snprintf(keyBuffer, sizeof(keyBuffer), "%.5f,%.5f>%.5f,%.5f", from.latitude, from.longitude,
                              to.latitude, to.longitude);
                const std::string cacheKey = keyBuffer;
                const auto now = std::chrono::steady_clock::now();
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = cache_.find(cacheKey);
                    if (it != cache_.end() && now - it->second.first < std::chrono::minutes(10))
                        return it->second.second;
                }

                TravelResult result = request(from, to);
                if (result.ok)
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (cache_.size() > 2000)
                        cache_.clear();
                    cache_[cacheKey] = {now, result};
                }
                return result;
            }

        private:
            std::string key_, endpoint_, mode_;
            long timeout_;
            std::mutex mutex_;
            std::map<std::string, std::pair<std::chrono::steady_clock::time_point, TravelResult>> cache_;

            TravelResult request(const LatLng &from, const LatLng &to)
            {
                TravelResult result;
                CURL *curl = curl_easy_init();
                if (!curl)
                {
                    result.error = "Could not start HTTP client";
                    return result;
                }

                const std::string body = "{\"origin\":" + point(from) + ",\"destination\":" + point(to) +
                                         ",\"travelMode\":\"" + mode_ + "\",\"languageCode\":\"en-US\",\"units\":\"METRIC\"}";
                std::string response;
                curl_slist *headers = nullptr;
                headers = curl_slist_append(headers, "Content-Type: application/json");
                headers = curl_slist_append(headers, ("X-Goog-Api-Key: " + key_).c_str());
                headers = curl_slist_append(headers, "X-Goog-FieldMask: routes.distanceMeters,routes.duration");

                curl_easy_setopt(curl, CURLOPT_URL, endpoint_.c_str());
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendBody);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
                curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // required in multi-threaded servers

                CURLcode code = curl_easy_perform(curl);
                long status = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);

                if (code != CURLE_OK)
                {
                    result.error = std::string("Google Maps unreachable: ") + curl_easy_strerror(code);
                    return result;
                }

                auto json = crow::json::load(response);
                if (status != 200)
                {
                    result.error = "Google Maps error (HTTP " + std::to_string(status) + ")";
                    if (json && json.has("error") && json["error"].has("message"))
                        result.error += ": " + std::string(json["error"]["message"].s());
                    return result;
                }
                if (!json || !json.has("routes") || json["routes"].t() != crow::json::type::List ||
                    json["routes"].size() == 0)
                {
                    result.error = "Google Maps found no route between these points";
                    return result;
                }
                const auto &route = json["routes"][0];
                result.distanceMeters = route.has("distanceMeters") ? static_cast<int>(route["distanceMeters"].i()) : 0;
                result.durationSeconds = route.has("duration") ? parseDuration(std::string(route["duration"].s())) : 0;
                result.ok = true;
                return result;
            }
        };
    }

    std::unique_ptr<RoutesClient> makeGoogleRoutesClient(const std::string &apiKey, const std::string &endpoint,
                                                         const std::string &travelMode, long timeoutSeconds)
    {
        return std::make_unique<GoogleRoutesClient>(apiKey, endpoint, travelMode, timeoutSeconds);
    }
}
