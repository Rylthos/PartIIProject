#include "client.hpp"

#include "../header.hpp"
#include "network/serializers.hpp"

#include <stack>

#include <thread>

namespace Network::Client {

struct Request {
    HeaderType type;
    std::vector<uint8_t> data;
    const StandardCallback& callback;
};

static uint32_t s_CurrentID = 0;
static std::unordered_map<uint32_t, const StandardCallback&> s_Callbacks;

static std::stack<Request> s_Requests;

void run(const Node& node)
{
    using namespace std::chrono_literals;

    while (true) {
        if (s_Requests.size() != 0) {
            Request request = s_Requests.top();
            s_Requests.pop();

            Header header;
            header.type = request.type;
            header.id = s_CurrentID++;

            std::vector<uint8_t> data;
            addHeader(header, data);
            data.insert(data.end(), request.data.begin(), request.data.end());

            send(node.socket, data.data(), data.size(), 0);

            s_Callbacks.insert({ header.id, request.callback });
        }

        std::this_thread::sleep_for(10ms);
    }
}

void addFileRequest(std::string path, const StandardCallback& callback)
{
    std::vector<uint8_t> data;

    Serializer::writeString(path, data);

    s_Requests.push({
        .type = HeaderType::REQUEST_FILES,
        .data = data,
        .callback = callback,
    });
}

}
