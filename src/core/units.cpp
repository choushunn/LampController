#include "lamp/core/units.h"

#include "lamp/core/model.h"

namespace lamp {

int PercentToRaw(int percent) {
    if (percent < 0 || percent > 100) {
        return -1;
    }
    return (percent * kMaxRawValue + 50) / 100;
}

int RawToPercent(int raw_value) {
    if (raw_value < 0 || raw_value > kMaxRawValue) {
        return -1;
    }
    return (raw_value * 100 + kMaxRawValue / 2) / kMaxRawValue;
}

bool IsValidChannel(int channel) {
    return channel >= kMinChannel && channel <= kMaxChannel;
}

bool IsValidPercent(int percent) {
    return percent >= 0 && percent <= 100;
}

bool IsValidRaw(int raw_value) {
    return raw_value >= 0 && raw_value <= kMaxRawValue;
}

bool IsValidBaudRate(int baud_rate) {
    return baud_rate > 0;
}

}  // namespace lamp
