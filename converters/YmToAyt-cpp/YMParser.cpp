#include "YMParser.h"
#include "platforms.h"
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace std;

// Display YM6 Header Data
void YmHeader::print() {
    cout << "----- YM6 HEADER -----" << endl;
    cout << "Title           : " << title << endl;
    cout << "Author          : " << author << endl;
    cout << "Notes           : " << notes << endl;
    cout << "ID              : " << string(ID, 4) << endl;
    //  cout << "Control String  : " << string(header.controlStr, 8)
    //<< endl;
    cout << "Master Clock    : " << masterClock << " Hz ("
         << find_platform_by_frequency(masterClock).name << ")" << endl;
    cout << "Frequency       : " << frequency << " Hz" << endl;
    cout << "Frame Counter   : " << frameCount << endl;
    cout << "Loop Frame      : " << loopFrame << endl;
    cout << "Attributs       : $" << hex << setw(8) << setfill('0') << songAttr << endl;
    cout << "Digidrums       : " << dec << digidrumCnt << endl;
    // cout << "Futur (ext.)    : " << header.future << endl;
    cout << "Interlaced      : " << ((songAttr & 1) == 1 ? "Yes" : "No") << endl;
    cout << "--------------------------" << endl;
}

// Read a 16 bits value encoded in big endian
uint16_t YMData::readBEWord(ifstream& stream) {
    uint8_t b[2];
    if (!stream.read(reinterpret_cast<char*>(b), 2)) {
        throw runtime_error("Unexpected EOF while reading 16-bit BE value");
    }
    return (static_cast<uint16_t>(b[0]) << 8) | b[1];
}

// Read a 32 bits value encoded in big endian
uint32_t YMData::readBEDWord(ifstream& stream) {
    uint8_t b[4];
    if (!stream.read(reinterpret_cast<char*>(b), 4)) {
        throw runtime_error("Unexpected EOF while reading 32-bit BE value");
    }
    return (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
           (static_cast<uint32_t>(b[2]) << 8) | b[3];
}

// Read a null terminated string
string YMData::readNullTerminatedString(ifstream& stream) {
    string result;
    char ch;
    while (stream.read(&ch, 1) && ch != '\0') {
        result += ch;
    }
    return result;
}

/**
 * Read a Ym header
 */
uint32_t YMData::readHeader(ifstream& file) {

    // static uint32_t readYmHeader(ifstream& file, TYmHeader& header) {
    file.read(header.ID, 4);
    file.read(header.controlStr, 8);

    if ((string(header.ID, 4) != ID_YM6 && string(header.ID, 4) != ID_YM5) ||
        string(header.controlStr, 8) != CTRLSTR) {
        cerr << "Error reading YM file: ID=" << header.ID << " controlStr: " << header.controlStr
             << endl;
        throw runtime_error("Invalid YM6 File : header missing or incorrect.");
    }

    header.frameCount = readBEDWord(file);
    header.songAttr = readBEDWord(file);
    header.digidrumCnt = readBEWord(file);
    header.masterClock = readBEDWord(file);
    header.frequency = readBEWord(file);
    header.loopFrame = readBEDWord(file);
    header.future = readBEWord(file);

    // Digidrums
    for (int i = 0; i < header.digidrumCnt; ++i) {
        uint32_t DrumSz = readBEDWord(file);
        ByteBlock DrumData(DrumSz);
        if (DrumSz > 0) {
            file.read(reinterpret_cast<char*>(DrumData.data()), DrumSz);
        }
    }

    // Additional strings (tite, author, comments)
    header.title = readNullTerminatedString(file);
    header.author = readNullTerminatedString(file);
    header.notes = readNullTerminatedString(file);

    return header.frameCount;
}

void YMData::readFrames(ifstream& file) {

    // Read Frames
    ByteBlock buf(header.frameCount * 16);
    file.read(reinterpret_cast<char*>(buf.data()), buf.size());
    file.close();

    for (int i = 0; i < 16; i++)
        rawRegisters[i].assign(header.frameCount, 0);

    // Separate data per register
    bool isInterleaved = (header.songAttr & 1) == 1;
    if (isInterleaved) {
        for (uint32_t i = 0; i < 16; ++i) {
            for (uint32_t j = 0; j < header.frameCount; ++j) {
                rawRegisters[i][j] = buf[(i * header.frameCount) + j];
            }
        }
    } else {
        for (uint32_t j = 0; j < header.frameCount; ++j) {
            for (uint32_t i = 0; i < 16; ++i) {
                rawRegisters[i][j] = buf[j * 16 + i];
            }
        }
    }
}

int YMData::filterReg13(bool apply) {
    std::vector<uint8_t>& vec = rawRegisters[YM_REG_WFSHAPE];

    if (vec.empty()) {
        return 0;
    }

    bool inSequence = false;
    uint8_t lastNon255Value = 255;
    int suspiciousCount = 0;

    for (uint8_t& value : vec) {
        if (value != 255) {
            if (value == lastNon255Value) {
                if (!inSequence) {
                    inSequence = true;
                }
                suspiciousCount++;
                if (apply) {
                    value = 255;
                }
            } else {
                inSequence = false;
                lastNon255Value = value;
            }
        } else {
            inSequence = false;
            lastNon255Value = value;
        }
    }

    return suspiciousCount;
}

// ===== Correction fréquence TONES (R0..R5) : période12b *= coef =====
void YMData::scalePeriods(double coef, bool verbose) {
    if (coef == 1.0)
        return;
    if (coef <= 0.0)
        return;
    int overflow_count[3] = {0, 0, 0};

    for (int k = 0; k < 3; ++k) {
        int lo = k * 2;  // R0, R2, R4
        int hi = lo + 1; // R1, R3, R5
        auto& L = rawRegisters[lo];
        auto& H = rawRegisters[hi];
        const size_t frames = L.size();
        for (size_t f = 0; f < frames; ++f) {
            uint8_t hib = H[f];
            uint16_t oldP = (uint16_t(hib & 0x0F) << 8) | uint16_t(L[f]);
            if (oldP == 0)
                continue;
            double scaled = round(double(oldP) * coef);
            int32_t p = static_cast<int32_t>(scaled);
            if (p < 1)
                p = 1;
            while (p > 0x0FFF) {
                if (verbose)
                    cerr << "Frame " << f << " : Period overflow! R" << lo << "-R" << hi << endl;
                overflow_count[k]++;
                p = p >> 1; // Divide by 2 in case of overflow (higher octave)
            }
            L[f] = uint8_t(p & 0xFF);
            H[f] = uint8_t((hib & 0xF0) | ((p >> 8) & 0x0F)); // préserve nibble haut
        }
    }

    for (int k = 0; k < 3; ++k) {
        if (overflow_count[k] > 0) {
            cerr << "/!\\ Warning !" << overflow_count[k]
                 << " period overflows detected for channel " << k
                 << ". Increase verbosity for more info" << endl;
        }
    }
}

void YMData::scaleNoise(double coef) {
    // Noise
    auto& N = rawRegisters[YM_REG_FREQ_NOISE];
    const size_t frames = N.size();

    for (size_t f = 0; f < frames; ++f) {
        double scaled = round(double(N[f]) * coef);
        if (scaled > 15.0f)
            scaled = 15.0f;
        N[f] = (static_cast<uint8_t>(scaled)) & 0xFF;
    }
}

void YMData::scaleEnvelope(double coef) {
    // Noise
    auto& NLSB = rawRegisters[YM_REG_FREQ_WF_LSB];
    auto& NMSB = rawRegisters[YM_REG_FREQ_WF_MSB];

    const size_t frames = NLSB.size();

    for (size_t f = 0; f < frames; ++f) {
        double scaled = round(double(NLSB[f] | (NMSB[f] << 8)) * coef);
        // Overfow?
        while (scaled > 65535) {
            scaled /= 2;
        }
        NLSB[f] = (static_cast<uint8_t>(scaled)) & 0xFF;
        NMSB[f] = (static_cast<uint8_t>(scaled) >> 8) & 0xFF;
    }
}

/**
 * @brief Analyse une liste de buffers de données pour vérifier la constance de
 * leurs valeurs.
 * * Génère un masque binaire (où le bit N est à 1 si le buffer N n'est pas
 * constant) et collecte les valeurs fixes des buffers constants.
 * * @param rawValues Le vecteur de buffers de données
 * (vector<ByteBlock>).
 * @param fixedValues Un vecteur de paires (numéro de séquence, valeur fixe)
 * rempli pour les buffers dont toutes les valeurs sont les mêmes.
 * @return uint32_t Un masque binaire où le bit i est à 1 si le i-ème buffer
 * contient des valeurs qui changent, et à 0 s'il est constant. Le type uint32_t
 * est utilisé pour supporter jusqu'à 32 buffers.
 */
uint32_t analyze_data_buffers(const array<ByteBlock, 16>& rawValues,
                              vector<pair<size_t, uint8_t>>& fixedValues, bool exportAllRegs) {
    // Reset fixed values vector
    fixedValues.clear();

    uint16_t real_changeMask; // Temporaire, a mettre dans la struct/classe YMData
    // Initialise le masque binaire des changements à 0
    uint32_t changeMask = 0;
    // Check the 14 registers data series
    for (size_t i = 0; i < rawValues.size() && i < 14; ++i) {
        const auto& buffer = rawValues[i];

        // An empty buffer is considered as 'constant', nd has no init value
        if (buffer.empty()) {
            continue;
        }

        bool isConstant = true;
        uint8_t firstValue = buffer[1];
        // Check if a buffer is contant (except if explicitly requested)
        if (!exportAllRegs) {
            // We skip the very 1st value, as sometimes a register only changes once, at the
            // beginning (0,255, 255, ...). This is very common. Also skip last value, for the same
            // reason
            if (buffer.size() > 2) {
                isConstant = all_of(buffer.begin() + 2, buffer.end() - 1,
                                    [&firstValue](uint8_t val) { return val == firstValue; });
            }
        }

        if (!isConstant) {
            real_changeMask |= (1u << i);
            changeMask |= (1u << i);
        } else {
            if (i == 13) {
                changeMask |= (1u << i);
            } else {
                fixedValues.emplace_back(i, firstValue);
            }
        }
    }

    return changeMask;
}
