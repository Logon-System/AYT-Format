
#include "platforms.h"
#include <iostream>
#include <cmath>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <sstream>  

/**
 * @brief Search platform name associated with a frequency
 * @param targetClock masterClock found in YM file
 * @return string : name of platform or 'unknown' if not found
 */
const PlatformFreq& find_platform_by_frequency(double targetClock) {
    for (const auto& platform : platformFrequencies) {
        // Comparison with tolerance
        if (fabs(targetClock - platform.freq) < EPSILON_FREQ) {
            return platform;
        }
    }
    return platformFrequencies[0]; //{"Unknown", -1, 15};
}

const PlatformFreq& find_frequency_by_platform(std::string& platformName) {
    // uppercase
    transform(platformName.begin(), platformName.end(), platformName.begin(), ::toupper);

    auto it = find_if(platformFrequencies.begin(), platformFrequencies.end(),
                      [&platformName](const PlatformFreq& p) {
                          // strict comparison
                          return p.name == platformName;
                      });

    if (it != platformFrequencies.end()) {
        return *it;
    } else {
        return platformFrequencies[0];
    }
}

std::string generate_platform_list() {
    std::stringstream ss;

    for (size_t i = 0; i < platformFrequencies.size(); ++i) {
        ss << platformFrequencies[i].name;
        if (i < platformFrequencies.size() - 1) {
            ss << ", ";
        }
    }
    return ss.str();
}