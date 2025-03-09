#ifndef BATTERY_H
#define BATTERY_H

extern const float voltageTable[11][2];

int getBatteryPercentage(float voltage);

#endif
