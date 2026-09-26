#include "lamp/core/protocol.h"

#include "lamp/core/model.h"

#include <cctype>

namespace lamp::protocol {

const char* ChannelPrefix(int channel) {
    switch (channel) {
    case 1:
        return "SPA";
    case 2:
        return "SPB";
    case 3:
        return "SPC";
    case 4:
        return "SPD";
    default:
        return "SPA";
    }
}

std::string Build(const Command& command) {
    switch (command.type) {
    case CommandType::SetChannel:
        return std::string(ChannelPrefix(command.channel)) + std::to_string(command.value) + "#";
    case CommandType::SetMode:
        return "S_MOD:" + std::to_string(command.value) + "#";
    case CommandType::SetPwmFrequency:
        return "S_PWMP:" + std::to_string(command.value) + "#";
    case CommandType::EnableOutput:
        return "S_CTRL:" + std::to_string(command.value) + "#";
    case CommandType::Apply:
        return "S_ALL#";
    case CommandType::Query:
        return "S_" + command.key + ":#";
    }
    return "";
}

bool IsQuery(const Command& command) {
    return command.type == CommandType::Query;
}

static char ascii_lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

static bool case_insensitive_find(const std::string& text, const std::string& needle, size_t* position) {
    if (needle.empty()) {
        *position = 0;
        return true;
    }
    for (size_t index = 0; index + needle.size() <= text.size(); index++) {
        bool matched = true;
        for (size_t offset = 0; offset < needle.size(); offset++) {
            if (ascii_lower(text[index + offset]) != ascii_lower(needle[offset])) {
                matched = false;
                break;
            }
        }
        if (matched) {
            *position = index;
            return true;
        }
    }
    return false;
}

std::optional<int> ParseStatusValue(const std::string& response, const std::string& key) {
    size_t position = 0;
    if (!case_insensitive_find(response, key, &position)) {
        return std::nullopt;
    }

    size_t cursor = position + key.size();
    while (cursor < response.size() &&
           (response[cursor] == ' ' || response[cursor] == '\t' || response[cursor] == ':')) {
        cursor++;
    }

    int sign = 1;
    if (cursor < response.size() && response[cursor] == '-') {
        sign = -1;
        cursor++;
    } else if (cursor < response.size() && response[cursor] == '+') {
        cursor++;
    }

    int parsed = 0;
    bool found_digit = false;
    while (cursor < response.size() && response[cursor] >= '0' && response[cursor] <= '9') {
        found_digit = true;
        parsed = parsed * 10 + (response[cursor] - '0');
        cursor++;
    }

    if (!found_digit) {
        return std::nullopt;
    }
    return parsed * sign;
}

std::vector<Command> ConfigureStableSequence(int pwm_frequency) {
    return {
        {CommandType::SetMode, 0, 0, "", 220},
        {CommandType::SetPwmFrequency, 0, pwm_frequency, "", 220},
        {CommandType::EnableOutput, 0, 1, "", 220},
    };
}

std::vector<Command> ApplySequence(bool restore_stable_mode) {
    std::vector<Command> sequence = {
        {CommandType::Apply, 0, 0, "", 320},
    };
    if (restore_stable_mode) {
        sequence.push_back({CommandType::SetMode, 0, 0, "", 220});
        sequence.push_back({CommandType::SetPwmFrequency, 0, 1, "", 220});
    }
    return sequence;
}

std::vector<Command> SetChannelRawSequence(int channel, int raw, bool restore_stable_mode) {
    std::vector<Command> sequence;
    if (restore_stable_mode) {
        auto stable = ConfigureStableSequence(1);
        sequence.insert(sequence.end(), stable.begin(), stable.end());
    }
    sequence.push_back({CommandType::SetChannel, channel, raw, "", 220});
    auto apply = ApplySequence(restore_stable_mode);
    sequence.insert(sequence.end(), apply.begin(), apply.end());
    return sequence;
}

std::vector<Command> SetChannelsRawSequence(const std::vector<int>& raws, bool restore_stable_mode) {
    std::vector<Command> sequence;
    if (restore_stable_mode) {
        auto stable = ConfigureStableSequence(1);
        sequence.insert(sequence.end(), stable.begin(), stable.end());
    }
    for (size_t index = 0; index < raws.size(); index++) {
        sequence.push_back({CommandType::SetChannel, static_cast<int>(index + 1), raws[index], "", 220});
    }
    auto apply = ApplySequence(restore_stable_mode);
    sequence.insert(sequence.end(), apply.begin(), apply.end());
    return sequence;
}

std::vector<Command> QueryStatusSequence() {
    static const char* keys[] = {"VER", "MOD", "PWMP", "CTRL", "TEMP", "VOLT", "CRV", "ERRS"};
    std::vector<Command> sequence;
    for (const char* key : keys) {
        sequence.push_back({CommandType::Query, 0, 0, key, 260});
    }
    return sequence;
}

std::vector<Command> InitializeDeviceSequence() {
    std::vector<Command> sequence = ConfigureStableSequence(1);
    for (int channel = 1; channel <= kChannelCount; channel++) {
        sequence.push_back({CommandType::SetChannel, channel, 0, "", 220});
    }
    auto apply = ApplySequence(true);
    sequence.insert(sequence.end(), apply.begin(), apply.end());
    return sequence;
}

}  // namespace lamp::protocol
