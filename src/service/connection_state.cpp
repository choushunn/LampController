#include "lamp/service/connection_state.h"

namespace lamp {

ConnectionState ConnectionStateMachine::Current() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return state_;
}

bool ConnectionStateMachine::TryBeginConnect() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (state_ == ConnectionState::Disconnected || state_ == ConnectionState::Failed) {
        state_ = ConnectionState::Connecting;
        return true;
    }
    return false;
}

bool ConnectionStateMachine::TryConnected() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (state_ == ConnectionState::Connecting || state_ == ConnectionState::Reconnecting) {
        state_ = ConnectionState::Connected;
        return true;
    }
    return false;
}

bool ConnectionStateMachine::TryFailed() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (state_ == ConnectionState::Connecting) {
        state_ = ConnectionState::Failed;
        return true;
    }
    return false;
}

bool ConnectionStateMachine::TryReconnect() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (state_ == ConnectionState::Connected) {
        state_ = ConnectionState::Reconnecting;
        return true;
    }
    return false;
}

bool ConnectionStateMachine::TryDisconnect() {
    std::lock_guard<std::mutex> lock(mtx_);
    state_ = ConnectionState::Disconnected;
    return true;
}

void ConnectionStateMachine::Reset() {
    std::lock_guard<std::mutex> lock(mtx_);
    state_ = ConnectionState::Disconnected;
}

}  // namespace lamp
