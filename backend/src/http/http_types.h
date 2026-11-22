#pragma once
// Minimal request/response types so the API can be exercised without a
// network server (unit tests) and adapted to Crow in api_main.cpp.

#include <string>
#include <vector>

namespace hc
{
    struct HttpRequest
    {
        std::string method; // GET, POST, PUT, DELETE
        std::string path;   // e.g. /api/complaints/12/status
        std::string body;
        std::string authorization; // raw "Authorization" header
        std::vector<int> params;   // values of <int> segments, in order
    };

    struct HttpResponse
    {
        int status = 200;
        std::string body; // always JSON
    };
}
