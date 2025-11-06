#pragma once
#include "typedefs.h"

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

const std::string ID_YM6 = "YM6!";
const std::string ID_YM5 = "YM5!";
const std::string CTRLSTR = "LeOnArD!";

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
    std::string title;
    std::string author;
    std::string notes;

    void print();
};

class YMData {
  public:
    YmHeader header;
    std::string fileName;

    uint32_t readHeader(std::ifstream& in);
    void readFrames(std::ifstream& in);

    void CheckEndMarker(std::ifstream& in);
    void verifyRegisterSizes();

    void load(std::string fname);

    void scalePeriods(double coeff, bool verbose);
    void scaleEnvelope(double coeff);
    void scaleNoise(double coeff);

    RegisterValues rawRegisters{};

    uint16_t readBEWord(std::ifstream& stream);
    uint32_t readBEDWord(std::ifstream& stream);
    std::string readNullTerminatedString(std::ifstream& stream);
    int filterReg13(bool apply);

    


};

/**
 * Some stats about a register values in a YM song
 */
struct TRegisterInfo {
    // Indicates the value doesnt's change for a given register
    bool IsConstant = false;
    // Value in case a register is constant
    uint8_t ConstantValue = 0;
    uint8_t differentValues = 0;
    uint32_t numberOfChanges = 0;
};


uint32_t analyze_data_buffers(const std::array<ByteBlock, 16>& rawValues,
                              std::vector<std::pair<size_t, uint8_t>>& fixedValues, bool exportAllRegs);