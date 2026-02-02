#pragma once

#include "msquic.h"

#include <functional>
#include <optional>

namespace Network {
void setExitCallback(std::function<void()> func);

QUIC_STATUS streamCallback(HQUIC stream, void* context, QUIC_STREAM_EVENT* event);

QUIC_STATUS listenerCallback(HQUIC connection, void* context, QUIC_LISTENER_EVENT* event);

QUIC_STATUS connectionCallback(HQUIC connection, void* context, QUIC_CONNECTION_EVENT* event);
}
