#include "battery.h"

// Lookup table mapping voltage levels to battery percentages
const float voltageTable[11][2] = {
    {4.2, 100}, // 100% at 4.2V
    {4.1, 90},  // 90% at 4.1V
    {4.0, 80},  // 80% at 4.0V
    {3.9, 70},  // 70% at 3.9V
    {3.85, 60}, // 60% at 3.85V
    {3.8, 50},  // 50% at 3.8V
    {3.75, 40}, // 40% at 3.75V
    {3.7, 30},  // 30% at 3.7V
    {3.6, 20},  // 20% at 3.6V
    {3.4, 10},  // 10% at 3.4V
    {3.0, 0}    // 0% at 3.0V
};

// Number of entries in the voltage table
const int numVoltagePoints = sizeof(voltageTable) / sizeof(voltageTable[0]);

// Uses binary search to determine battery percentage based on voltage
int getBatteryPercentage(float voltage)
{
    int left = 0;
    int right = numVoltagePoints - 1;

    while (left <= right)
    {
        int mid = left + (right - left) / 2;

        // Check if voltage falls within the range of this table entry
        if (voltage >= voltageTable[mid][0])
        {
            // If at the highest entry or the next one is lower, return this percentage
            if (mid == 0 || voltage < voltageTable[mid - 1][0])
                return static_cast<int>(voltageTable[mid][1]);

            right = mid - 1; // Search lower values
        }
        else
        {
            left = mid + 1; // Search higher values
        }
    }

    return 0; // Default to 0% if voltage is below the lowest threshold
}
