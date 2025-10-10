#include <algorithm> // Pour all_of
#include <cmath>
#include <cmath> // Pour fabs
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric> // Pour accumulate (dans certains cas, bien que non utilisé directement ici)
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility> // Pour pair
#include <vector>
#include <array>

using namespace std;
using ByteBlock = vector<uint8_t>;
using RegisterValues = array<ByteBlock,16>;



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

// Définition de la marge de tolérance (epsilon) pour la comparaison des doubles
constexpr double EPSILON = 0.5;

// Structure pour le résultat de l'optimisation
struct OptimizedResult {
    ByteBlock optimized_heap;              // Le tableau contigu minimal
    map<int, int> optimized_pointers;      // Map: Index Bloc Original -> Index dans le Heap
    vector<int> optimized_ByteBlock_order; // Ordre des blocs dans le Heap
};

// Définition de la structure pour associer le nom de la plateforme à sa fréquence
struct PlatformFreq {
    string name;
    double freq;
};

// Tableau des fréquences associées aux plateformes
const vector<PlatformFreq> platformFrequencies = {
    {"CPC", 1000000.0f},      {"ORIC", 1000000.0f},        {"ZXUNO", 1750000.0f},
    {"PENTAGON", 1750000.0f}, {"TIMEXTS2068", 1764000.0f}, {"ZX128", 1773450.0f},
    {"MSX", 1789772.5f},      {"ATARI", 2000000.0f},       {"VG5000", 1000000.0f}};

// result buffers
class ResultSequences {
  public:
    // Sequences (pointers to patterns)
    vector<ByteBlock> sequenced;
    // patterns are data blocs to be sent to a register
    ByteBlock patterns;
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
