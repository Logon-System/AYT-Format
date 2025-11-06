#pragma once
#include <string>
#include <vector>

constexpr double FREQ_NOT_FOUND = -1;

// Tolerance for comparing frequencies and find a platform associated with the masterclock 
constexpr double EPSILON_FREQ = 100;

// Freqencies and masterclock per platform
struct PlatformFreq {
    std::string name;
    double freq;
    uint8_t id;
};

const std::vector<PlatformFreq> platformFrequencies = {
    {"Unknown", -1, 15},       {"CPC", 1000000.0f, 0},      {"ORIC", 1000000.0f, 1},
    {"ZXUNO", 1750000.0f, 2},  {"PENTAGON", 1750000.0f, 3}, {"TIMEXTS2068", 1764000.0f, 4},
    {"ZX128", 1773400.0f, 5},  {"MSX", 1789772.5f, 6},      {"ATARI", 2000000.0f, 7},
    {"VG5000", 1000000.0f, 8},
};

const PlatformFreq& find_platform_by_frequency(double targetClock);

/*
 * @brief Recherche la fréquence associée à un nom de plateforme donné.
 * @param platformName Le nom de la plateforme à rechercher.
 * @return double La fréquence associée ou FREQ_NOT_FOUND (-1.0) si le nom n'est
 * pas trouvé.
 */
const PlatformFreq& find_frequency_by_platform(std::string& platformName);

// Generate a comma separated list of architectures
std::string generate_platform_list();
