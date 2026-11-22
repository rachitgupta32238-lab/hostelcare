// HostelCare API server: connects the route table in http/api_core.cpp to
// Crow. All business logic lives in src/core and src/http; this file only
// adapts Crow requests/responses.
//
// Environment variables (all optional except on the very first run):
//   HOSTELCARE_ADMIN_USERNAME / HOSTELCARE_ADMIN_PASSWORD  first Admin account
//   HOSTELCARE_DATA_DIR        data folder (default: data)
//   HOSTELCARE_PORT            port (default: 18080)
//   HOSTELCARE_ALLOWED_ORIGIN  CORS origin, e.g. http://localhost:5173 (default: *)
//   HOSTELCARE_IMPORT_DIR      folder with old .txt files to import into a new database
//   HOSTELCARE_GOOGLE_MAPS_SERVER_KEY / HOSTELCARE_TRAVEL_MODE   Google Routes API
//   HOSTELCARE_DEMO_MODE=1     demo accounts + login buttons (separate demo deployment only)

#include "crow.h"
#include "crow/middlewares/cors.h"

#include "http/api.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    crow::HTTPMethod toCrowMethod(const std::string &method)
    {
        if (method == "POST")
            return crow::HTTPMethod::Post;
        if (method == "PUT")
            return crow::HTTPMethod::Put;
        if (method == "DELETE")
            return crow::HTTPMethod::Delete;
        return crow::HTTPMethod::Get;
    }

    hc::HttpRequest fromCrow(const crow::request &req, const std::string &method,
                             std::vector<int> params)
    {
        hc::HttpRequest r;
        r.method = method;
        r.path = req.url;
        r.body = req.body;
        r.authorization = req.get_header_value("Authorization");
        r.params = std::move(params);
        return r;
    }

    crow::response toCrow(const hc::HttpResponse &r)
    {
        crow::response res(r.status);
        res.set_header("Content-Type", "application/json");
        res.set_header("Cache-Control", "no-store");
        res.body = r.body;
        return res;
    }

    int countIntParams(const std::string &pattern)
    {
        int count = 0;
        for (std::size_t pos = pattern.find("<int>"); pos != std::string::npos;
             pos = pattern.find("<int>", pos + 1))
            ++count;
        return count;
    }

    std::string envOr(const char *name, const std::string &fallback)
    {
        const char *value = std::getenv(name);
        return value && *value ? std::string(value) : fallback;
    }
}

int main()
{
    hc::Api api(envOr("HOSTELCARE_DATA_DIR", "data"));
    try
    {
        api.initialize();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Startup failed: " << e.what() << std::endl;
        return 1;
    }

    crow::App<crow::CORSHandler> app;
    app.loglevel(crow::LogLevel::Warning);

    auto &cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin(envOr("HOSTELCARE_ALLOWED_ORIGIN", "*"))
        .headers("Content-Type", "Authorization")
        .methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method, "OPTIONS"_method);

    // Register every route from the shared table. Crow needs the handler's
    // argument list to match the number of <int> segments in the URL.
    for (const auto &route : hc::Api::routes())
    {
        const hc::Api::Route *r = &route; // the table is static, so this stays valid
        auto &rule = app.route_dynamic(route.pattern);
        rule.methods(toCrowMethod(route.method));

        switch (countIntParams(route.pattern))
        {
        case 0:
            rule([&api, r](const crow::request &req)
                 { return toCrow(api.handle(*r, fromCrow(req, r->method, {}))); });
            break;
        case 1:
            rule([&api, r](const crow::request &req, int a)
                 { return toCrow(api.handle(*r, fromCrow(req, r->method, {a}))); });
            break;
        default:
            rule([&api, r](const crow::request &req, int a, int b)
                 { return toCrow(api.handle(*r, fromCrow(req, r->method, {a, b}))); });
            break;
        }
    }

    CROW_ROUTE(app, "/")
    ([]
     { return "HostelCare API is running. Try /api/health"; });

    int port = 18080;
    try
    {
        port = std::stoi(envOr("HOSTELCARE_PORT", "18080"));
    }
    catch (...)
    {
    }
    std::cout << "HostelCare API listening on http://localhost:" << port << std::endl;
    app.port(static_cast<std::uint16_t>(port)).multithreaded().run();
    return 0;
}
