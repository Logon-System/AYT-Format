#pragma once

#include <algorithm> 
#include <array>
#include <bitset>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using ByteBlock = std::vector<uint8_t>;
using RegisterValues = std::array<ByteBlock, 16>;
using namespace std;