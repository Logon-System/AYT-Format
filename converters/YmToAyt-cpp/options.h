#pragma once
#include <string>

struct Options {
    bool saveFiles = false;
    double periodCoef = -1.0;   // Disabled by default
    double envCoef = -1.0;      // Disabled by default
    double noiseCoef = -1.0;    // Disabled by default
    double targetClock = -1.0f; // Disabled by default
    uint8_t targetId = 0;
    uint8_t targetClockId = 0;

    int patternSizeMin = 4;
    int patternSizeMax = 64;
    int patternSizeStep = 4;

    // Add an extra sequence in case frames are divided evenly
    bool extraFinalSequence= false;
    // Dedupliation using masks
    bool enableMaskPatterns = false; 
    // Replace masked bits by zeros
    bool enableNormPatterns = false; 
    // Fix R13 repeated values
    bool filterReg13=false;
    
    int optimizationLevel = 1;
    // sort patterns by signifiance (force to true whent pattern deduplication is enabled)
    bool sortPatterns = false; 

    std::string optimizationMethod = "greedy";

    std::string outputPath = "";
    std::string outputFile = "";

    std::string sizeParam = "4:64/4";
    std::string targetParam = "cpc";

    bool onlyDivisible = false;
    bool exportCsv = false;
    bool exportAllRegs = false; // Do not skip any register
    bool save_size= false; // Use this option if you want to store the size of the file in the name

    // Genetic Algorithm Options
    size_t ga_MU = 10;                  // Taille de la population de parents (μ)
    size_t ga_LAMBDA = 40;              // Nombre d'enfants générés (λ)
    double ga_MUTATION_RATE_MIN = 0.02; // Min Mutation rate per childend
    double ga_MUTATION_RATE_MAX = 0.5; // Min Mutation rate per childend
    double ga_CROSSOVER_RATE = 0.8;     // chance d'utiliser le croisement
    int ga_NUM_GENERATION_MIN = 20000;  
    int ga_NUM_GENERATION_MAX = 100000;
    int ga_ADDITIONAL_GENERATIONS = 10000;

    // Verbosity level
    int verbosity = 1;
};