#pragma once

#include <optional>

#include "msquic.h"

namespace Network {

extern const QUIC_API_TABLE* s_QuicAPI;

extern HQUIC s_QuicRegistration;
extern HQUIC s_QuicConfiguration;

struct Node {
    HQUIC listener = nullptr;
    HQUIC connection = nullptr;
};

extern Node s_Node;

struct NetworkingInfo {
    bool enableServerSide = false;
    bool enableClientSide = false;

    bool networked;
};

}
