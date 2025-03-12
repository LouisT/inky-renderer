#ifndef BATTERY_H
#define BATTERY_H

// A table mapping voltage levels to battery percentages
// Each entry consists of a voltage (in volts) and the corresponding battery percentage
extern const float voltageTable[11][2];

// Calculate the battery percentage based on the given voltage
int getBatteryPercentage(float voltage);

#endif
