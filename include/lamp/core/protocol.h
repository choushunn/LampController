#pragma once

#include <optional>
#include <string>
#include <vector>

namespace lamp::protocol {

enum class CommandType {
    SetChannel,
    SetMode,
    SetPwmFrequency,
    EnableOutput,
    Apply,
    Query
};

struct Command {
    CommandType type = CommandType::SetChannel;
    int channel = 0;
    int value = 0;
    std::string key;
    int read_ms = 220;
};

std::string Build(const Command& command);
bool IsQuery(const Command& command);

// Case-insensitive key lookup in a device response, followed by an optional
// sign and decimal digits. Returns nullopt when the key or a number is absent.
std::optional<int> ParseStatusValue(const std::string& response, const std::string& key);

const char* ChannelPrefix(int channel);

std::vector<Command> ConfigureStableSequence(int pwm_frequency);
std::vector<Command> ApplySequence(bool restore_stable_mode);
std::vector<Command> SetChannelRawSequence(int channel, int raw, bool restore_stable_mode);
std::vector<Command> SetChannelsRawSequence(const std::vector<int>& raws, bool restore_stable_mode);
std::vector<Command> QueryStatusSequence();
std::vector<Command> InitializeDeviceSequence();

}  // namespace lamp::protocol
