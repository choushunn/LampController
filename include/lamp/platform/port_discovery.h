#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lamp {

class PortDiscovery {
public:
    virtual ~PortDiscovery() = default;

    virtual std::vector<std::string> GetAllPorts() = 0;
    virtual std::vector<std::string> GetPreferredPorts() = 0;
    virtual std::optional<std::string> FindPreferredPort() = 0;
};

std::unique_ptr<PortDiscovery> CreateWindowsPortDiscovery();

// Pure selection logic: returns the single usable port, or nullopt when the
// situation is ambiguous. Prefers the unique preferred (USB) port, otherwise
// the unique port in general.
std::optional<std::string> PickPreferredPort(
    const std::vector<std::string>& all_ports,
    const std::vector<std::string>& preferred_ports);

}  // namespace lamp
