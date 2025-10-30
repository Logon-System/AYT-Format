#include <algorithm> // Pour all_of
#include <array>
#include <cmath>
#include <cmath> // Pour fabs
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <numeric> // Pour accumulate (dans certains cas, bien que non utilisé directement ici)
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility> // Pour pair
#include <vector>

using namespace std;
using ByteBlock = vector<uint8_t>;
using RegisterValues = array<ByteBlock, 16>;

// Header
class YmHeader {
  public:
    char ID[4];
    char controlStr[8];
    uint32_t frameCount;
    uint32_t songAttr;
    uint16_t digidrumCnt;
    uint32_t masterClock;
    uint16_t frequency;
    uint32_t loopFrame;
    uint16_t future;
    string title;
    string author;
    string notes;

    void print();
};

struct TRegisterInfo {
    bool IsConstant = false;
    uint8_t ConstantValue = 0;
    uint8_t differentValues = 0;
    uint32_t numberOfChanges = 0;
};

// Votre structure Options
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

    int optimizationLevel = 1;
    string optimizationMethod = "gluttony";
    string outputPath = "";

    string sizeParam = "4:64/4";
    string targetParam = "cpc";
    bool onlyDivisible = false;
    bool exportCsv = false;
    bool exportAllRegs = false; // Do not skip any register
    bool filterReg13=false;
    bool save_size= false; // Use this option if you want to store the size of the file in the name

    // Options for Genetic Algorithm Optimization
    size_t GA_MU = 10;                  // Taille de la population de parents (μ)
    size_t GA_LAMBDA = 40;              // Nombre d'enfants générés (λ)
    double GA_MUTATION_RATE_MIN = 0.02; // Min Mutation rate per childend
    double GA_MUTATION_RATE_MAX = 0.5; // Min Mutation rate per childend
    double GA_CROSSOVER_RATE = 0.8;     // chance d'utiliser le croisement
    int GA_NUM_GENERATION_MIN = 20000;  
    int GA_NUM_GENERATION_MAX = 100000;
    int GA_ADDITIONAL_GENERATIONS = 10000;
};

class YMData {
  public:
    YmHeader header;
    string fileName;

    uint32_t readHeader(ifstream& in);
    void readFrames(ifstream& in);

    void CheckEndMarker(ifstream& in);
    void verifyRegisterSizes();

    void load(string fname);

    void scalePeriods(int regLo, int regHi, double coeff);
    void scaleEnvelope(double coeff);
    void scaleNoise(double coeff);

    RegisterValues rawRegisters{};
};

constexpr uint8_t YM_REG_FREQ_A_LSB = 0;
constexpr uint8_t YM_REG_FREQ_A_MSB = 1;
constexpr uint8_t YM_REG_FREQ_B_LSB = 2;
constexpr uint8_t YM_REG_FREQ_B_MSB = 3;
constexpr uint8_t YM_REG_FREQ_C_LSB = 4;
constexpr uint8_t YM_REG_FREQ_C_MSB = 5;
constexpr uint8_t YM_REG_FREQ_NOISE = 6;
constexpr uint8_t YM_REG_MIXER_CTRL = 7;
constexpr uint8_t YM_REG_VOLUME_A = 8;
constexpr uint8_t YM_REG_VOLUME_B = 9;
constexpr uint8_t YM_REG_VOLUME_C = 10;
constexpr uint8_t YM_REG_FREQ_WF_LSB = 11;
constexpr uint8_t YM_REG_FREQ_WF_MSB = 12;
constexpr uint8_t YM_REG_WFSHAPE = 13;

const string ID_YM6 = "YM6!";
const string ID_YM5 = "YM5!";
const string CTRLSTR = "LeOnArD!";

constexpr double FREQ_NOT_FOUND = -1;

// Définition de la marge de tolérance (epsilon) pour la comparaison des frequences
constexpr double EPSILON = 100;

constexpr uint8_t final_sequence_values[3] = {0x00, 0x3F, 0xBF};
constexpr uint8_t final_sequence[16] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0};

// Définition de la structure pour associer le nom de la plateforme à sa fréquence
struct PlatformFreq {
    string name;
    double freq;
    uint8_t id;
};

// Tableau des fréquences associées aux plateformes
const vector<PlatformFreq> platformFrequencies = {
    {"Unknown", -1, 15},       {"CPC", 1000000.0f, 0},      {"ORIC", 1000000.0f, 1},
    {"ZXUNO", 1750000.0f, 2},  {"PENTAGON", 1750000.0f, 3}, {"TIMEXTS2068", 1764000.0f, 4},
    {"ZX128", 1773400.0f, 5},  {"MSX", 1789772.5f, 6},      {"ATARI", 2000000.0f, 7},
    {"VG5000", 1000000.0f, 8},
};

const vector<int> songFrequencies = {50, 25, 60, 30, 100, 200, 300, 0};

// Structure pour le résultat de l'optimisation
struct OptimizedResult {
    ByteBlock optimized_heap;          // Le tableau contigu minimal
    map<int, int> optimized_pointers;  // Map: Index Bloc Original -> Index dans le Heap
    vector<int> optimized_block_order; // Ordre des blocs dans le Heap
};

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

    // various size

    uint32_t findActiveRegisters(const vector<ByteBlock>& rawValues);
};
