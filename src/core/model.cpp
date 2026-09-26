#include "lamp/core/model.h"

namespace lamp {

std::string ToString(ConnectionState state) {
    switch (state) {
    case ConnectionState::Disconnected:
        return "Disconnected";
    case ConnectionState::Connecting:
        return "Connecting";
    case ConnectionState::Connected:
        return "Connected";
    case ConnectionState::Reconnecting:
        return "Reconnecting";
    case ConnectionState::Failed:
        return "Failed";
    }
    return "Unknown";
}

}  // namespace lamp
