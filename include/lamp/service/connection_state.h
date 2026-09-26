#pragma once

#include "lamp/core/model.h"

#include <mutex>

namespace lamp {

// Connection lifecycle state machine. Guarded internally, safe to use from
// multiple threads.
class ConnectionStateMachine {
public:
    ConnectionState Current() const;

    // Attempted transitions; each returns false when the move is not allowed
    // from the current state.
    bool TryBeginConnect();
    bool TryConnected();
    bool TryFailed();
    bool TryReconnect();
    bool TryDisconnect();
    void Reset();

private:
    mutable std::mutex mtx_;
    ConnectionState state_ = ConnectionState::Disconnected;
};

}  // namespace lamp
