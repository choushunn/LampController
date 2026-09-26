#pragma once

namespace lamp {

int PercentToRaw(int percent);
int RawToPercent(int raw_value);
bool IsValidChannel(int channel);
bool IsValidPercent(int percent);
bool IsValidRaw(int raw_value);
bool IsValidBaudRate(int baud_rate);

}  // namespace lamp
