#include "node.hpp"

namespace Network {
const QUIC_API_TABLE* s_QuicAPI = nullptr;

HQUIC s_QuicRegistration = nullptr;
HQUIC s_QuicConfiguration = nullptr;

Node s_Node;
}
