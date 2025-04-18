#include "jpeg_utils.h"
#include <vector>
#include <algorithm>

namespace jpeg_utils
{
    // Probe the JPEG data to determine its kind (baseline, progressive, other, or invalid).
    JpegKind probeKind(const uint8_t *data, size_t len)
    {
        // Check for minimum length and JPEG Start Of Image (SOI) marker (0xFF 0xD8)
        if (len < 4 || data[0] != 0xFF || data[1] != 0xD8)
            return JpegKind::INVALID;

        // Start reading markers just after the SOI
        size_t pos = 2;
        // Loop through the data to find segment markers
        while (pos + 3 < len)
        {
            // Skip any bytes until we see a 0xFF marker prefix
            if (data[pos] != 0xFF)
            {
                ++pos;
                continue;
            }
            // Skip any padding 0xFF bytes
            while (data[pos] == 0xFF)
                ++pos;
            // Read the marker type
            uint8_t marker = data[pos++];

            // Restart markers (0xD0-0xD7), SOI (0xD8), and EOI (0xD9) have no data payload
            if (marker == 0xD8 || marker == 0xD9 ||
                (marker >= 0xD0 && marker <= 0xD7))
                continue;

            // Ensure there are at least two bytes for the segment length
            if (pos + 1 >= len)
                return JpegKind::INVALID;
            // Read segment length (big-endian), includes the two length bytes
            uint16_t segLen = (data[pos] << 8) | data[pos + 1];
            // Segment length must be at least 2 and fit in the buffer
            if (segLen < 2 || pos + segLen > len)
                return JpegKind::INVALID;
            // Move past the length bytes
            pos += 2;

            // Check for specific markers that indicate JPEG type
            if (marker == 0xC0)
                return JpegKind::BASELINE; // Baseline DCT
            if (marker == 0xC2)
                return JpegKind::PROGRESSIVE; // Progressive DCT
            if ((marker >= 0xC1 && marker <= 0xCF) &&
                marker != 0xC0 && marker != 0xC2)
                return JpegKind::OTHER; // Other JPEG format

            // Skip over this segment's payload
            pos += segLen - 2;
        }

        // If no valid type marker was found, return INVALID
        return JpegKind::INVALID;
    }
}
