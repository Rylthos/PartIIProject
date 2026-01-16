#include "client.hpp"

#include "../header.hpp"
#include "network/serializers.hpp"

#include "fcntl.h"
#include "string.h"

#include <cerrno>
#include <stack>

#include <thread>

namespace Network::Client {

struct Request {
    HeaderType type;
    std::vector<uint8_t> data;
    StandardCallback callback;
};

static uint32_t s_CurrentID = 0;
static std::unordered_map<uint32_t, StandardCallback> s_Callbacks;

static std::stack<Request> s_Requests;

void handleResponse(Header header, std::vector<uint8_t> data)
{
    if (s_Callbacks.contains(header.id)) {
        s_Callbacks.at(header.id)(data);
    }
}

void run(const Node& node)
{
    using namespace std::chrono_literals;

    std::vector<uint8_t> headerBuffer(9);

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

        ssize_t bits = -1;
        bits = recv(node.socket, headerBuffer.data(), headerBuffer.size(), 0);
        if (bits == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                fprintf(stderr, "Error receiving: %s\n", strerror(errno));
            }
            continue;
        }

        size_t index = 0;
        Header header = parseHeader(headerBuffer, bits, index);

        bool valid = false;
        if (header.type == HeaderType::RETURN) {
            valid = true;
            printf("Received: %d : %d : %d\n", (uint8_t)header.type, header.id, header.size);
        }

        size_t totalBits = 0;

        std::vector<uint8_t> data(header.size);
        while (totalBits != header.size) {
            ssize_t read = recv(node.socket, data.data() + totalBits, header.size - totalBits, 0);
            if (read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                } else {
                    fprintf(stderr, "Error receiving: %s\n", strerror(errno));
                }
            } else {
                totalBits += read;
            }
        }

        if (totalBits != 0 && valid) {
            handleResponse(header, data);
        }

        std::this_thread::sleep_for(10ms);
    }
}

void addSceneRequest(std::string path, StandardCallback callback)
{
    std::vector<uint8_t> data;
    Serializer::writeString(path, data);

    s_Requests.push({
        .type = HeaderType::REQUEST_SCENE,
        .data = data,
        .callback = callback,
    });
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
