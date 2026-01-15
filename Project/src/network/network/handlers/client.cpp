#include "client.hpp"

#include "../header.hpp"
#include "network/serializers.hpp"

#include "fcntl.h"
#include "string.h"

#include <stack>

#include <thread>

namespace Network::Client {

const uint32_t BUF_SIZE = 1024;

struct Request {
    HeaderType type;
    std::vector<uint8_t> data;
    StandardCallback callback;
};

static uint32_t s_CurrentID = 0;
static std::unordered_map<uint32_t, StandardCallback> s_Callbacks;

static std::stack<Request> s_Requests;

void handleResponse(std::vector<uint8_t> data, size_t len)
{
    size_t index = 0;
    Header header = parseHeader(data, len, index);

    if (s_Callbacks.contains(header.id)) {
        std::vector<uint8_t> converted = std::vector(data.begin() + index, data.begin() + len);
        s_Callbacks.at(header.id)(converted);
    }
}

void run(const Node& node)
{
    using namespace std::chrono_literals;

    std::vector<uint8_t> buffer(BUF_SIZE);

    fcntl(node.socket, F_SETFL, fcntl(node.socket, F_GETFL) | O_NONBLOCK);

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

        ssize_t bits = recv(node.socket, buffer.data(), BUF_SIZE, MSG_WAITALL);

        if (bits == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
            } else {
                fprintf(stderr, "Error receiving: %s\n", strerror(errno));
            }
        } else {
            handleResponse(buffer, bits);
        }

        std::this_thread::sleep_for(10ms);
    }
}

void addDirEntryRequest(std::string path, StandardCallback callback)
{
    std::vector<uint8_t> data;

    Serializer::writeString(path, data);

    s_Requests.push({
        .type = HeaderType::REQUEST_DIR_ENTRIES,
        .data = data,
        .callback = callback,
    });
}

void addFileEntryRequest(std::string path, StandardCallback callback)
{
    std::vector<uint8_t> data;

    Serializer::writeString(path, data);

    s_Requests.push({
        .type = HeaderType::REQUEST_FILE_ENTRIES,
        .data = data,
        .callback = callback,
    });
}

}
