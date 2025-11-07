#pragma once

#include "typedefs.h"
#include "optimizers.h"

constexpr uint8_t final_sequence_values[3] = {0x00, 0x3F, 0xBF};
constexpr uint8_t final_sequence[16] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0};

const std::vector<int> songFrequencies = {50, 25, 60, 30, 100, 200, 300, 0};

// result buffers
class ResultSequences {
  public:
    // Sequences (pointers to patterns)
    vector<ByteBlock> sequenced;
    // patterns are data blocs to be sent to a register
    map<int, ByteBlock> patternMap;   // Initial map of patterns
    OptimizedResult optimizedOverlap; // Optimized by gluttony algorithm
};

// Structure containing result of converter
// Used to generate AYT file
class AYTConverter {

  public:
    // Active registers: 1 bit ber rgister
    uint16_t activeRegs;
    // number of active registers
    uint8_t numActiveRegs;
    // Table for initializing constant registers
    vector<pair<size_t, uint8_t>> initRegValues;
    
    uint32_t findActiveRegisters(const vector<ByteBlock>& rawValues);
};

ResultSequences buildBuffers(const array<ByteBlock, 16>& rawData, uint16_t activeRegs, int patSize,
                             int optimizationLevel);
