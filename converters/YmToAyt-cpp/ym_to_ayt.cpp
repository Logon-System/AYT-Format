
// YmToAyt.cpp
//
// For the joint venture Logon System + GPA in 2025!
//
// Proof of Concept by Tronic/GPA
// Format definition and improvements by Longshot & Siko
// C++ Conversion Tool: Siko
// Ultra optimized Z80 Player Code by Longshot

// build with ; g++ -std=c++17 -O2 -Wall YmToSeq.cpp -o YmToSeq)

// TODO:
// - Final sequence optimisée
// - Export Asm prebuildé
// - Amélioration Export ayt en .asm avec un header comprehensible
// - la correction des periodes
// - le bon placement de la sequence de fin selon que ca tombe juste ou pas (en
// - selectionnant l'optimale parmi les 2 3 possibilités)

#include "ym_to_ayt.h"

namespace fs = filesystem;

int verbosity = 1;
uint32_t real_changeMask = 0;

/**
 * @brief Recherche le nom de la plateforme correspondant à une fréquence
 * donnée.
 * @param targetFreq La fréquence lue dans le fichier.
 * @return string Le nom de la plateforme ou "Inconnue" si aucune
 * correspondance n'est trouvée.
 */
static string find_platform_by_frequency(double targetFreq) {
    // Parcours du tableau
    for (const auto& platform : platformFrequencies) {

        // Comparaison avec une tolérance
        if (fabs(targetFreq - platform.freq) < EPSILON) {
            return platform.name;
        }
    }
    // Si aucune correspondance n'est trouvée après la boucle
    return "Unknown";
}

/*
 * @brief Recherche la fréquence associée à un nom de plateforme donné.
 * * @param platformName Le nom de la plateforme à rechercher.
 * @return double La fréquence associée ou FREQ_NOT_FOUND (-1.0) si le nom n'est
 * pas trouvé.
 */
double find_frequency_by_platform(string& platformName) {
    // uppercase
    transform(platformName.begin(), platformName.end(), platformName.begin(), ::toupper);

    auto it = find_if(platformFrequencies.begin(), platformFrequencies.end(),
                      [&platformName](const PlatformFreq& p) {
                          // strict comparison
                          return p.name == platformName;
                      });

    if (it != platformFrequencies.end()) {
        return it->freq;
    } else {
        return FREQ_NOT_FOUND;
    }
}

// Generate a comma separated list of architectures
static string generate_platform_list() {
    stringstream ss;

    for (size_t i = 0; i < platformFrequencies.size(); ++i) {
        ss << platformFrequencies[i].name;
        if (i < platformFrequencies.size() - 1) {
            ss << ", ";
        }
    }
    return ss.str();
}

// Read a 16 bits value encoded in big endian
static uint16_t readBEWord(ifstream& stream) {
    uint8_t b[2];
    if (!stream.read(reinterpret_cast<char*>(b), 2)) {
        throw runtime_error("Unexpected EOF while reading 16-bit BE value");
    }
    return (static_cast<uint16_t>(b[0]) << 8) | b[1];
}

// Read a 32 bits value encoded in big endian
static uint32_t readBEDWord(ifstream& stream) {
    uint8_t b[4];
    if (!stream.read(reinterpret_cast<char*>(b), 4)) {
        throw runtime_error("Unexpected EOF while reading 32-bit BE value");
    }
    return (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
           (static_cast<uint32_t>(b[2]) << 8) | b[3];
}

// Read a null terminated string
static string readNullTerminatedString(ifstream& stream) {
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
         << find_platform_by_frequency(masterClock) << ")" << endl;
    cout << "Frequency       : " << frequency << " Hz" << endl;
    cout << "Frame Counter   : " << frameCount << endl;
    cout << "Loop Frame      : " << loopFrame << endl;
    cout << "Attributs       : $" << hex << setw(8) << setfill('0') << songAttr << endl;
    cout << "Digidrums       : " << dec << digidrumCnt << endl;
    // cout << "Futur (ext.)    : " << header.future << endl;
    cout << "Interlaced      : " << ((songAttr & 1) == 1 ? "Yes" : "No") << endl;
    cout << "--------------------------" << endl;
}

// I/O helpers
static void writeAllBytes(const fs::path& filePath, const ByteBlock& data) {
    ofstream file(filePath, ios::binary);
    if (!file) {
        throw runtime_error("Failed writing file : " + filePath.string());
    }
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<streamsize>(data.size()));
}

static void dumpDb(ofstream& file, const ByteBlock& data, uint8_t groupSize, size_t start = 0,
                   size_t length = 0) {

    if (!file) {
        throw runtime_error("Failed writing file");
    }

    if (length == 0)
        length = data.size() - start;

    for (size_t i = 0; i < length; ++i) {
        if (i % groupSize == 0) {
            file << "  DB ";
        }
        file << "#" << uppercase << hex << setw(2) << setfill('0')
             << static_cast<int>(data[start + i]);
        if ((i + 1) % groupSize == 0) {
            file << endl;
        } else {
            if (i < length - 1) {
                file << ",";
            } else {
                file << endl;
            }
        }
    }
}

static void dumpDw(ofstream& file, const ByteBlock& data, uint8_t groupSize, size_t start = 0,
                   size_t length = 0) {
    if (!file) {
        throw runtime_error("Failed writing file");
    }

    if (length == 0)
        length = data.size() - start;

    for (size_t i = 0; i < length; i += 2) {
        if (i % groupSize == 0) {
            file << "  DW ";
        }
        file << "#" << uppercase << hex << setw(2) << setfill('0')
             << static_cast<int>(data[start + i + 1]) << setw(2) << setfill('0')
             << static_cast<int>(data[start + i]);
        if ((i + 2) % groupSize == 0) {
            file << endl;
        } else {
            if (i < length - 2) {
                file << ",";
            } else {
                file << endl;
            }
        }
    }
}

// Dump all data stored in a ByteBlock to a text file, using db
// directives
static void dumpAsDb(const fs::path& filePath, const ByteBlock& data, uint8_t groupSize) {
    ofstream file(filePath);
    if (!file) {
        throw runtime_error("Failed writing file : " + filePath.string());
    }
    dumpDb(file, data, groupSize);
}

static void saveRawData(const fs::path& baseName, const ByteBlock& data, const string& suffix) {
    string prefix = baseName.string() + suffix;
    writeAllBytes(prefix + ".bin", data);
    dumpAsDb(prefix + ".txt", data, 16);
}

// Computes Hash FNV-1a for a byte sequence
static uint32_t fnv1a32(const ByteBlock& data) {
    uint32_t h = 2166136261u;
    for (uint8_t byte : data) {
        h ^= byte;
        h *= 16777619u;
    }
    return h;
}

//   static uint32_t findActiveRegisters(const vector<ByteBlock>& rawValues);

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
static uint32_t analyze_data_buffers(const array<ByteBlock, 16>& rawValues,
                                     vector<pair<size_t, uint8_t>>& fixedValues) {
    // Réinitialise le vecteur de sortie pour les valeurs fixes
    fixedValues.clear();

    // Initialise le masque binaire des changements à 0
    uint32_t changeMask = 0;
    // Check the 14 registers data series
    for (size_t i = 0; i < rawValues.size() && i < 14; ++i) {
        const auto& buffer = rawValues[i];

        // Un buffer vide est considéré comme "constant" (trivialement) et n'a pas
        // de valeur fixe significative
        if (buffer.empty()) {
            // Le bit reste à 0 (constant). On ne stocke pas de valeur fixe dans ce
            // cas.
            continue;
        }

        // Check if a buffer is contant
        bool isConstant = true;
        uint8_t firstValue = buffer[0];
        if (buffer.size() > 1) {
            isConstant = all_of(buffer.begin() + 1, buffer.end(),
                                [&firstValue](uint8_t val) { return val == firstValue; });
        }

        if (!isConstant) {
            real_changeMask |= (1u << i);
            changeMask |= (1u << i);
        } else {
            if (i == 13) {
                if (verbosity > 0) {
                    cout << "Reg13 is constant, but in AYT format, it cannot considered as fixed"
                         << endl;
                }
                changeMask |= (1u << i);
            } else {
                fixedValues.emplace_back(i, firstValue);
            }
        }
    }

    return changeMask;
}

// Fonction pour trouver le chevauchement maximal entre Suffixe (B1) et Préfixe (B2)
static int find_max_overlap(const ByteBlock& B1, const ByteBlock& B2, int ByteBlockSize) {
    int max_overlap = 0;
    // Check overlap of different sizes (1 to ByteBlockSize)
    for (int k = ByteBlockSize - 1; k >= 1; --k) {
        // Suffixe de B1 de taille k
        auto suffix_start = B1.end() - k;
        // Préfixe de B2 de taille k
        auto prefix_start = B2.begin();

        if (equal(suffix_start, B1.end(), prefix_start)) {
            max_overlap = k;
            break;
        }
    }
    return max_overlap;
}

static OptimizedResult merge_ByteBlocks_greedy(const map<int, ByteBlock>& original_ByteBlocks,
                                               int ByteBlockSize) {
    OptimizedResult result;

    // Suivi des blocs déjà inclus dans le Heap
    map<int, bool> included;
    for (const auto& pair : original_ByteBlocks) {
        included[pair.first] = false;
    }

    // 1. Démarrer avec le premier bloc (nous choisissons B0 arbitrairement)
    int current_ByteBlock_index = original_ByteBlocks.begin()->first;
    const ByteBlock& first_ByteBlock = original_ByteBlocks.at(current_ByteBlock_index);

    result.optimized_heap.insert(result.optimized_heap.end(), first_ByteBlock.begin(),
                                 first_ByteBlock.end());
    result.optimized_pointers[current_ByteBlock_index] = 0;
    result.optimized_ByteBlock_order.push_back(current_ByteBlock_index);
    included[current_ByteBlock_index] = true;

    // 2. Itération gloutonne pour greffer les autres blocs
    int ByteBlocks_to_include = original_ByteBlocks.size() - 1;

    for (int i = 0; i < ByteBlocks_to_include; ++i) {
        int best_overlap = -1;
        int best_next_ByteBlock_index = -1;

        // Le bloc de tête est le dernier ajouté à la séquence (End-of-Heap)
        const ByteBlock& current_head = original_ByteBlocks.at(current_ByteBlock_index);

        // Trouver le meilleur bloc non inclus à greffer sur le bloc de tête
        for (const auto& pair : original_ByteBlocks) {
            int next_index = pair.first;
            const ByteBlock& next_ByteBlock = pair.second;

            if (!included[next_index]) {
                int overlap = find_max_overlap(current_head, next_ByteBlock, ByteBlockSize);

                if (overlap > best_overlap) {
                    best_overlap = overlap;
                    best_next_ByteBlock_index = next_index;
                }
            }
        }

        if (best_next_ByteBlock_index != -1) {
            const ByteBlock& best_next_ByteBlock =
                original_ByteBlocks.at(best_next_ByteBlock_index);

            // Calcul du nouveau pointeur
            int new_pointer_index = result.optimized_pointers.at(current_ByteBlock_index) +
                                    ByteBlockSize - best_overlap;

            // Greffer le nouveau fragment au Heap
            // int non_overlapping_length = ByteBlockSize - best_overlap;

            result.optimized_heap.insert(result.optimized_heap.end(),
                                         best_next_ByteBlock.begin() + best_overlap,
                                         best_next_ByteBlock.end());

            // Mettre à jour l'état
            result.optimized_pointers[best_next_ByteBlock_index] = new_pointer_index;
            result.optimized_ByteBlock_order.push_back(best_next_ByteBlock_index);
            included[best_next_ByteBlock_index] = true;
            current_ByteBlock_index = best_next_ByteBlock_index;

            // Affichage de la progression (pour vérification)
            //            cout << "Grefé B" << best_next_ByteBlock_index << " (Overlap: " <<
            //            best_overlap
            //                      << ", New Pointer: " << new_pointer_index << ")\n";
        } else {
            // Aucun chevauchement trouvé. Démarrer une nouvelle séquence.
            // (Dans cette approche gloutonne, on ne devrait pas arriver ici si les données sont
            // très liées)
            break;
        }
    }

    return result;
}

// ===== Correction fréquence TONES (R0..R5) : période12b *= coef =====
static void YMToneMultiplyPeriodsR0toR5(array<ByteBlock, 16>& regs, double coef) {
    if (coef == 1.0)
        return;
    if (coef <= 0.0)
        return;

    for (int k = 0; k < 3; ++k) {
        int lo = k * 2;  // R0, R2, R4
        int hi = lo + 1; // R1, R3, R5
        auto& L = regs[lo];
        auto& H = regs[hi];
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
                cerr << "Period overflow! R" << lo << "-R" << hi << endl;
                p = p >> 1; // Divide by 2 in case of overflow (higher octave)
            }
            L[f] = uint8_t(p & 0xFF);
            H[f] = uint8_t((hib & 0xF0) | ((p >> 8) & 0x0F)); // préserve nibble haut
        }
    }

    // Noise
    auto& N = regs[YM_REG_FREQ_NOISE];
    const size_t frames = N.size();

    for (size_t f = 0; f < frames; ++f) {
        double scaled = round(double(N[f]) * coef);
        if (scaled > 15.0f)
            scaled = 15.0f;
        N[f] = (static_cast<uint8_t>(scaled)) & 0xFF;
    }

    //
}

static ResultSequences buildBuffers(const array<ByteBlock, 16>& rawData, uint16_t activeRegs,
                                    int seqSize) {
    if (seqSize <= 0 || seqSize > 256) {
        throw invalid_argument("Invalid pattern size");
    }

    vector<ByteBlock> sequenceBuffers;
    ByteBlock uniquePatterns;
    unordered_map<uint32_t, uint32_t> uniqueHashes;
    unordered_map<uint32_t, ByteBlock> hashToPattern;

    for (size_t i = 0; i <= 14; ++i) {

        if (!(activeRegs & (1 << i))) {
            continue;
        }

        const auto& src = rawData[i];
        const size_t N = src.size();
        const size_t ByteBlocks = (N == 0) ? 0 : ((N + size_t(seqSize) - 1) / size_t(seqSize));

        ByteBlock seq;
        seq.reserve(ByteBlocks * 2);
        for (size_t blk = 0; blk < ByteBlocks; ++blk) {
            size_t start = blk * size_t(seqSize);
            size_t remain = (start < N) ? (N - start) : 0;
            size_t take = min(remain, size_t(seqSize));

            ByteBlock pat(seqSize);
            if (take) {
                memcpy(pat.data(), &src[start], take);
                uint8_t pad = src[start + take - 1];
                if (take < (size_t)seqSize)
                    memset(pat.data() + take, pad, size_t(seqSize) - take);
            } else {
                memset(pat.data(), 0, size_t(seqSize));
            }

            uint32_t h = fnv1a32(pat);
            uint32_t idx;
            auto it = uniqueHashes.find(h);
            if (it != uniqueHashes.end() && hashToPattern.at(h) == pat) {
                idx = it->second;
            } else {
                idx = (uint32_t)uniqueHashes.size();
                uniqueHashes[h] = idx;
                hashToPattern[h] = pat;
            }

            uint32_t off = idx * (uint32_t)seqSize;
            if (off > 0xFFFFu)
                throw runtime_error("Pattern offset exceeds 16-bit");

            // avec glouton
            seq.push_back(uint8_t(idx & 0xFF));
            seq.push_back(uint8_t((idx >> 8) & 0xFF));
            // Sans glouton: on stocke l'offset directement
            //            seq.push_back(uint8_t(off & 0xFF));
            //            seq.push_back(uint8_t((off >> 8) & 0xFF));
        }
        sequenceBuffers.push_back(move(seq));
    }

    uniquePatterns.resize(uniqueHashes.size() * (size_t)seqSize);
    for (const auto& kv : uniqueHashes) {
        const auto& pat = hashToPattern.at(kv.first);
        uint32_t idx = kv.second;
        copy(pat.begin(), pat.end(), uniquePatterns.begin() + idx * (size_t)seqSize);
    }

    return {sequenceBuffers, uniquePatterns};
}

//======================================================================================
// Fonctions utilitaires pour l'export
//======================================================================================

// Entrelace les buffers de sequences en un seul buffer de sortie
static ByteBlock interleaveSequenceBuffers(const vector<ByteBlock>& sequenceBuffers) {

    size_t numBuffers = sequenceBuffers.size();
    vector<size_t> sequenceBufferLengths;
    size_t totalsequenceBufferSize = 0;

    // Tous les buffers de sequence font la meme taille...
    for (const auto& buffer : sequenceBuffers) {
        sequenceBufferLengths.push_back(buffer.size());
        totalsequenceBufferSize += buffer.size();
    }

    ByteBlock interleavedOutput(totalsequenceBufferSize);

    vector<size_t> currentPositions(numBuffers, 0);
    bool allDone = false;
    size_t outputPos = 0;

    while (!allDone) {
        allDone = true;
        for (size_t i = 0; i < numBuffers; ++i) {
            if (currentPositions[i] < sequenceBufferLengths[i]) {
                size_t bytesToCopy =
                    min(static_cast<size_t>(2), sequenceBufferLengths[i] - currentPositions[i]);
                memcpy(&interleavedOutput[outputPos],
                       sequenceBuffers[i].data() + currentPositions[i], bytesToCopy);
                currentPositions[i] += bytesToCopy;
                outputPos += bytesToCopy;
                if (currentPositions[i] < sequenceBufferLengths[i]) {
                    allDone = false;
                }
            }
        }
    }

    return interleavedOutput;
}

// Sauvegarde les buffers dans des fichiers binaires et texte
void saveOutputs(const fs::path& baseName, const ResultSequences& buffers) {

    writeAllBytes(baseName.string() + "_patterns.bin", buffers.patterns);
    dumpAsDb(baseName.string() + "_patterns.txt", buffers.patterns,
             16); // converter.patternSize);

    for (size_t regIndex = 0; regIndex < buffers.sequenced.size(); ++regIndex) {

        string prefix = baseName.string() + "_R" + (regIndex < 10 ? "0" : "") + to_string(regIndex);
        writeAllBytes(prefix + "_sequences.bin", buffers.sequenced[regIndex]);
        dumpAsDb(prefix + "_sequences.txt", buffers.sequenced[regIndex], 16);
    }
}

// Fonction Coût : Calcule la taille totale du Heap pour un ordre donné
double calculate_cost(const vector<int>& ByteBlock_order, const map<int, ByteBlock>& ByteBlocks,
                      int ByteBlock_size) {
    if (ByteBlock_order.empty())
        return 0;

    double total_size = ByteBlock_size; // Commencez avec la taille du premier bloc

    for (size_t i = 1; i < ByteBlock_order.size(); ++i) {
        const ByteBlock& B_prev = ByteBlocks.at(ByteBlock_order[i - 1]);
        const ByteBlock& B_curr = ByteBlocks.at(ByteBlock_order[i]);

        int overlap = find_max_overlap(B_prev, B_curr, ByteBlock_size);
        total_size += (ByteBlock_size - overlap);
    }
    return total_size;
}

// Fonction de Mouvement : Génère un état voisin par "swap"
vector<int> get_neighbor(const vector<int>& current_order, mt19937& rng) {
    vector<int> neighbor = current_order;
    if (neighbor.size() < 2)
        return neighbor;

    // Échange de deux blocs aléatoirement
    uniform_int_distribution<> distrib(0, neighbor.size() - 1);
    int idx1 = distrib(rng);
    int idx2 = distrib(rng);

    while (idx1 == idx2) {
        idx2 = distrib(rng); // Assurez-vous qu'ils sont différents
    }
    swap(neighbor[idx1], neighbor[idx2]);
    return neighbor;
}

// ALGORITHME DE RECUIT SIMULÉ
vector<int> simulated_annealing(const map<int, ByteBlock>& ByteBlocks, int ByteBlockSize,
                                double initial_temp, double cooling_rate) {
    // Initialisation
    vector<int> current_order;
    for (const auto& pair : ByteBlocks) {
        current_order.push_back(pair.first);
    }
    // La solution initiale est un ordre aléatoire (ou glouton)
    random_device rd;
    mt19937 rng(rd());
    shuffle(current_order.begin(), current_order.end(), rng);

    vector<int> best_order = current_order;
    double current_cost = calculate_cost(current_order, ByteBlocks, ByteBlockSize);
    double best_cost = current_cost;
    double T = initial_temp;

    // Boucle de Recuit
    while (T > 1.0) { // Condition d'arrêt : T suffisamment proche de zéro
        // Le nombre d'itérations à chaque température peut être fixe ou proportionnel à N
        for (int i = 0; i < 100; ++i) {
            vector<int> neighbor_order = get_neighbor(current_order, rng);
            double neighbor_cost = calculate_cost(neighbor_order, ByteBlocks, ByteBlockSize);

            double delta_E = neighbor_cost - current_cost;

            if (delta_E < 0) {
                // Mouvement vers une meilleure solution (accepter toujours)
                current_order = neighbor_order;
                current_cost = neighbor_cost;

                if (current_cost < best_cost) {
                    best_cost = current_cost;
                    best_order = current_order;
                }
            } else {
                // Mouvement vers une solution moins bonne (accepter avec probabilité)
                double acceptance_prob = exp(-delta_E / T);
                uniform_real_distribution<> prob_distrib(0.0, 1.0);

                if (prob_distrib(rng) < acceptance_prob) {
                    current_order = neighbor_order;
                    current_cost = neighbor_cost;
                }
            }
        }

        // Refroidissement
        T *= cooling_rate; // Ex: 0.99
    }

    // Une fois le meilleur ordre trouvé, reconstruire le Heap et les pointeurs
    // ... (Appeler l'étape finale de l'algorithme glouton sur best_order)

    cout << "Optimisation terminée. Taille minimale trouvée: " << best_cost << " octets.\n";
    return best_order;
}

//======================================================================================
// CLI
//======================================================================================
static void printUsage(const char* prog) {
    cout << "Usage: " << prog << " [options] YM-FILE" << endl
         << "  -h, --help              Show help" << endl
         << "  -v, --verbose           Increase verbosity" << endl
         << "  -q, --quiet             Low verbosity" << endl
         << "  -s, --save              Save intermediary files" << endl
         << "  -p, --pattern-size N    Pattern size (1..255, default: auto)" << endl
         << "  -d, --only-divisible    Skip sizes that don't divide frame count" << endl
         << "  -c, --frequency-coef F  Multiply tone periods (R0..R5) by F (e.g., "
            "0.5,2)"
         << endl
         << "  -t, --target ARCH       Name of the target architecture ("
         << generate_platform_list() << ")"
         << endl
         //              << "  -o, --output FILE       Name of the output file"
         //              << "  -O, --output-path PATH  Folder where to store results"
         << "  -O, --output-path PATH  Folder where to store results"
         << "  -C, --csv               Export stats in CSV format";
}

static uint8_t reverse_bits(uint8_t byte) {
    uint8_t reversed = 0;
    for (int i = 0; i < 8; ++i) {
        uint8_t bit = (byte >> i) & 1;
        reversed |= (bit << (7 - i));
    }
    return reversed;
}

//======================================================================================
// Main program
//======================================================================================

struct Options {
    bool saveFiles = false;
    double periodCoef = -1.0;  // Disabled by default
    double envCoef = -1.0;   // Disabled by default
    double noiseCoef = -1.0; // Disabled by default
    double freqTarget = -1.0f; // Disabled by default
    string sizeParam = "auto";
    string targetParam = "cpc";
    bool onlyDivisible = false;

} options;

int main(int argc, char** argv) {

    fs::path inputPath;
   
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    /* The options we understand. */
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            ++verbosity;
        } else if (arg == "-q" || arg == "--quiet") {
            verbosity = 0;
        } else if (arg == "-s" || arg == "--save") {
            options.saveFiles = true;
        } else if (arg == "-d" || arg == "--only-divisible") {
            options.onlyDivisible = true;
        } else if (arg == "-t" || arg == "--target") {
            if (i + 1 >= argc) {
                cerr << "Option -t requires an argument" << endl;
                return 1;
            }

            options.targetParam = argv[++i];
            options.freqTarget = find_frequency_by_platform(options.targetParam);
            if (options.freqTarget < 0) {
                cerr << "Option -t argument must be a value among ATARI, CPC, ..." << endl;
                return 1;
            }

        } else if (arg == "-p" || arg == "--pattern-size") {
            if (i + 1 >= argc) {
                cerr << "Option -p requires an argument" << endl;
                return 1;
            }
            options.sizeParam = argv[++i];
            if (options.sizeParam != "auto") {
                int n = 0;
                try {
                    n = stoi(options.sizeParam);
                } catch (...) {
                    cerr << "Invalid pattern size" << endl;
                    return 1;
                }
                if (n < 1 || n > 256) {
                    cerr << "Pattern Size must be 1..256" << endl;
                    return 1;
                }
            }
        } else if (arg == "-sp" || arg == "--scale-period") {
            if (i + 1 >= argc) {
                cerr << "Option -c requires an argument" << endl;
                return 1;
            }
            try {
                options.periodCoef = stod(argv[++i]);
            } catch (...) {
                cerr << "Invalid period coefficient" << endl;
                return 1;
            }
            if (options.periodCoef <= 0.0) {
                cerr << "Frequency coefficient must be > 0" << endl;
                return 1;
            }
        } else if (!arg.empty() && arg[0] != '-') {
            inputPath = arg;
        } else {
            cerr << "Unknown option: " << arg << endl;
            return 1;
        }
    }

    if (inputPath.empty()) {
        cerr << "No input file specified" << endl;
        return 1;
    }

    try {
        if (verbosity > 0)
            cout << "Opening " << inputPath << endl;

        ifstream file(inputPath, ios::binary);
        if (!file) {
            throw runtime_error("Unable to open YM6 file: " + inputPath.string());
        }

        YMData ymdata;

        uint32_t frameCount = ymdata.readHeader(file);

        if (frameCount == 0) {
            throw runtime_error("YM File contains no frame");
        }
        if (verbosity > 0)
            ymdata.header.print();

        ymdata.readFrames(file);

        // Computes automatically coefficients to appy to registers, depending on YM masterClock and target platform
        if (options.freqTarget > 0) {
            if (options.periodCoef < 0)
                options.periodCoef = options.freqTarget / ymdata.header.masterClock;
            if (options.noiseCoef < 0)
                options.noiseCoef = options.freqTarget / ymdata.header.masterClock;
            if (options.envCoef < 0)
                options.envCoef = options.freqTarget / ymdata.header.masterClock;

        }

        if (verbosity > 0) {
            cout << "Period Coef = " << options.periodCoef << endl;
            cout << "Noise Coef = " << options.noiseCoef << endl;
            cout << "Envelop Coef = " << options.envCoef << endl;
        }


        fs::path baseName = inputPath.stem();
        size_t totalSize = 0;
        int patternSize = 0;

        ResultSequences finalBuffers;

        AYTConverter converter;

        // Apply tone period multiplier (R0..R5)

        if (options.periodCoef > 0) {
            if (verbosity)
                cout << "Applying tone period multiplier (R0..R5): " << options.periodCoef << endl;
            YMToneMultiplyPeriodsR0toR5(ymdata.rawRegisters, options.periodCoef);
        }

        // Save Raw register patterns
        if (options.saveFiles) {
            // cout << "Save per register data (after frequency correction) ";
            for (size_t r = 0; r < ymdata.rawRegisters.size(); ++r) {
                saveRawData(baseName, ymdata.rawRegisters[r],
                            string("_Raw_R") + (r < 10 ? "0" : "") + to_string(r));
            }
            // cout << "OK." << endl;
        }

        // Determine number of registers and fixed values
        converter.activeRegs = analyze_data_buffers(ymdata.rawRegisters, converter.initRegValues);

        // devrait etre calculé avant
        int numRegs = 0;
        for (int r = 0; r < 14; ++r) {
            // Check if register is exported
            if (converter.activeRegs & (1 << r)) {
                numRegs += 1;
            }
        }

        // ---- Automatic detection of best pattern size (brute force) ----
        if (options.sizeParam == "auto") {
            size_t bestTotal = static_cast<size_t>(-1);
            int bestSize = -1;

            for (int s = 1; s <= 128; ++s)
            // Utilisez une nouvelle portée pour garantir la destruction immédiate
            {
                size_t currentTotal = 0;
                //          vector<AYTConverter> currentBuffers(numActiveRegs);
                ResultSequences currentBuffers;
                bool valid = true;

                bool isDividingEvenly = (ymdata.header.frameCount % s) == 0;
                // TODO: skip if length is not a multipe of patternSize
                // si rawData.size() % seqSize != 0
                if ((options.onlyDivisible == true) && !isDividingEvenly) {
                    valid = false;
                    // cout << "Skipping Pattern Size " << s << endl;
                } else {
                    if (verbosity > 2)
                        cout << "Testing Pattern Size " << s << " :";

                    try {
                        currentBuffers = buildBuffers(ymdata.rawRegisters, converter.activeRegs, s);
                        currentTotal = currentBuffers.patterns.size();
                        for (size_t i = 0; i < currentBuffers.sequenced.size(); ++i) {
                            currentTotal += currentBuffers.sequenced[i].size();
                        }

                    } catch (const exception& e) {
                        if (verbosity > 2)
                            cerr << "Exception " << e.what() << endl;
                        valid = false;
                    }

                    if (valid) {
                        if (verbosity > 2)
                            cout << " " << currentTotal << endl;

                        if (currentTotal < bestTotal) {
                            bestTotal = currentTotal;
                            bestSize = s;
                            finalBuffers = currentBuffers;
                            if (verbosity > 0)
                                cout << "   --> New Best Pattern size found: " << s
                                     << " (Total Size: " << currentTotal << " bytes)"
                                     << (isDividingEvenly ? "" : " (warn: not evenly dividing)")
                                     << endl;
                        }
                    }
                } // currentBuffers est detruit ici
            }

            if (bestSize == -1) {
                cerr << "Exception: Unable to find a valid pattern size" << endl;
                return 1;
            }

            patternSize = bestSize;
            totalSize = bestTotal;

        } else {
            // ---- MODE DIRECT ----
            int seqSize = stoi(options.sizeParam);
            if (seqSize < 1 || seqSize > 256) {
                throw invalid_argument("Pattern Size must be set between 1 and 256.");
            }

            patternSize = seqSize;

            finalBuffers = buildBuffers(ymdata.rawRegisters, converter.activeRegs, seqSize);
            totalSize = finalBuffers.patterns.size();
            for (size_t i = 0; i < finalBuffers.sequenced.size(); ++i) {
                totalSize += finalBuffers.sequenced[i].size();
            }
        }

        // Shortest Common Supersequence optimization
        // Creation de la map des patterns
        map<int, ByteBlock> pattern_ByteBlocks;
        // a optim...
        for (int i = 0; i < finalBuffers.patterns.size() / patternSize; i++) {
            ByteBlock p;
            for (int j = 0; j < patternSize; j++)
                p.push_back(finalBuffers.patterns[i * patternSize + j]);

            pattern_ByteBlocks[i] = p;
        }

        /*
        for (int i = 0; i < finalBuffers.patterns.size() / patternSize; i++) {
          for (int j = 0; j < patternSize; j++)
          cout << (int)pattern_ByteBlocks[i][j] << " ";
          cout << endl;
        }

        */
        //        simulated_annealing(pattern_ByteBlocks, patternSize, 1000.0f, 0.999);

        // Pour les stats
        int orig_patterns_size = finalBuffers.patterns.size();

        auto result = merge_ByteBlocks_greedy(pattern_ByteBlocks, patternSize);
        //        cout << finalBuffers.patterns.size() << " => Heap Optimisé (Taille "
        //                  << result.optimized_heap.size() << ")\n";

        finalBuffers.patterns = result.optimized_heap;
        // new pointers
        for (int i = 0; i < finalBuffers.sequenced.size(); i++) {
            for (int j = 0; j < finalBuffers.sequenced[i].size(); j += 2) {
                int idx = finalBuffers.sequenced[i][j] + (finalBuffers.sequenced[i][j + 1] << 8);
                int newidx = result.optimized_pointers[idx];
                finalBuffers.sequenced[i][j] = newidx & 255;
                finalBuffers.sequenced[i][j + 1] = (newidx >> 8) & 255;
            }
        }

        // Entrelacement et écriture du fichier final
        if (verbosity > 1)
            cout << "\nInterlacing " << finalBuffers.sequenced.size() << " sequence buffers";

        ByteBlock interleavedData = interleaveSequenceBuffers(finalBuffers.sequenced);
        if (verbosity > 1)
            cout << "OK." << endl;

        /*
        string outputFileName = baseName.string() + "_sequences.bin";
        writeAllBytes(outputFileName, interleavedData);
        cout << "Interleaved Data \"" << outputFileName << "\" saved. "
        << endl;
        */

        // Sauvegarde des fichiers intermediaires si demandé => dans le _ayt.asm
        if (options.saveFiles) {
            saveOutputs(baseName, finalBuffers);
            cout << "Intermediary files saved for patternSize " << patternSize << endl;
        }

        if (verbosity > 0) {
            cout << "   Active Regs: " << numRegs << "0x" << hex << converter.activeRegs << dec
                 << endl;
            cout << "   Pattern Size: " << patternSize << endl;
            cout << "   Sequences size : " << interleavedData.size() << " bytes" << endl;
            cout << "   Init Sequence: ";
            for (const auto& pair : converter.initRegValues) {
                cout << pair.first << "=" << static_cast<int>(pair.second) << " ";
            }

            cout << endl;

            cout << "   Total size (sequences + patterns) : " << totalSize << " bytes" << endl;
            cout << "   Loop to sequence : " << (ymdata.header.loopFrame / patternSize);

            bool isLoopDividingEvenly = (ymdata.header.loopFrame % patternSize) == 0;
            if (!isLoopDividingEvenly) {
                cout << " (Warn: not looping to the start of a pattern) " << ymdata.header.loopFrame
                     << "/" << patternSize;
            }

            cout << endl;
        }

        // Export AYTs

        //        cout << "Export AYT file" << endl;

        /*
        ofstream aytbin(baseName.string() + ".ayt", ios::binary);
        if (!aytbin) {
          throw runtime_error("Failed writing file : " + (baseName.string() +
        ".ayt").string());
        }
        */
        //  file.write(reinterpret_cast<const char
        //  *>(data.data()),static_cast<streamsize>(data.size()));)

        // Version 1.0
        ByteBlock ayt_header;
        ayt_header.resize(14);

        ByteBlock ayt_file;
        uint16_t offset_sequences = ayt_header.size() + finalBuffers.patterns.size();
        uint16_t offset_loop_sequence =
            offset_sequences + (ymdata.header.loopFrame / patternSize) * 2 * numRegs;

        // Total number of pointers, including final sequence
        uint16_t seqTotalSize =
            finalBuffers.sequenced.size() * (1 + (finalBuffers.sequenced[0].size() >> 1));

        ayt_header[0] = (1 << 4);
        //  uint16_t Ayt_ActiveRegs     ; active reg (bit 2:reg 13...bit 15:reg 0), encoded in Big
        //  endian!
        ayt_header[1] = reverse_bits((converter.activeRegs >> 8) & 255); // Active Regs R13-R8
        ayt_header[2] = reverse_bits(converter.activeRegs & 255);        // Active Regs R7-R0

        // uint8_t Ayt_PatternSize
        ayt_header[3] = patternSize; // Num regs (vs reg patters)
        // uint16_t Ayt_FirstSeqMarker ; Offset from
        ayt_header[4] = uint8_t(offset_sequences & 0xFF);
        ayt_header[5] = uint8_t((offset_sequences >> 8) & 0xFF);
        // Ayt_LoopSeqMarker ;
        // Loop = beginning, can be changed (using data from YM?)
        ayt_header[6] = uint8_t(offset_loop_sequence & 0xFF);
        ayt_header[7] = uint8_t((offset_loop_sequence >> 8) & 0xFF);
        // ayt_file[6] = ayt_file[4];
        // ayt_file[7] = ayt_file[5];

        // Nb of pattern pointer (NbSeq x NbReg)
        ayt_header[10] = seqTotalSize & 0xff;
        ayt_header[11] = (seqTotalSize >> 8) & 0xff;

        ayt_header[12] = 0; // Platform/Freq (cpc+50hz)
        ayt_header[13] = 0; // Reserved

        // Reserve space
        ayt_file.resize(ayt_header.size() + interleavedData.size() + finalBuffers.patterns.size());

        copy(ayt_header.begin(), ayt_header.end(), ayt_file.begin());

        // Append patterns
        copy(finalBuffers.patterns.begin(), finalBuffers.patterns.end(),
             ayt_file.begin() + ayt_header.size());

        //        cout << "interleaved data size = " << interleavedData.size() << endl;
        //        cout << "offset_sequences = " << offset_sequences << endl;

        // Append interleaved sequences
        copy(interleavedData.begin(), interleavedData.end(), ayt_file.begin() + offset_sequences);

        // TODO: Adding Final sequence to add to interleaved Data
        uint8_t final_sequence_values[3] = {0x00, 0x3F, 0xC0};
        constexpr uint8_t final_sequence[14] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2};

        // TODO: search inside ayt file for specific data
        // Right now we simply add these 3 spacial bytes at the end of the file
        uint16_t ptr_final_sequence[3];
        for (int k = 0; k < 3; k++)
            ptr_final_sequence[k] = ayt_file.size() - ayt_header.size() + numRegs * 2 + k;

        for (int r = 0; r < 14; ++r) {
            // Check if register is exported
            if (converter.activeRegs & (1 << r)) {
                uint16_t ptr = ptr_final_sequence[final_sequence[r]];
                ayt_file.push_back(ptr & 255);
                ayt_file.push_back(ptr >> 8);
            }
        }

        // push final sequence special values
        ayt_file.push_back(0x00);
        ayt_file.push_back(0x3F);
        ayt_file.push_back(0xBF);

        // init sequence
        // 2 cases: if empty, we can take any byte with bit 7 set
        // otherwise we create the table using inactive registers values
        uint16_t pointerToInitTable = ayt_file.size();
        ayt_file[8] = pointerToInitTable & 0xff;
        ayt_file[9] = (pointerToInitTable >> 8) & 0xff;
        ayt_header[8] = pointerToInitTable & 0xff;
        ayt_header[9] = (pointerToInitTable >> 8) & 0xff;
        //        converter.

        if ((converter.activeRegs & 0x3fffu) != 0x3fffu) {
            for (const auto& pair : converter.initRegValues) {
                ayt_file.push_back(pair.first);
                ayt_file.push_back(pair.second);
            }
        }

        ayt_file.push_back(0xFF);

        writeAllBytes(baseName.string() + ".ayt", ayt_file);
        if (verbosity > 0)
            cout << "AYT file \"" << baseName.string() << "_ayt.bin\" saved." << endl;

        // Export asm version
        /*
        string filePath = baseName.string() + "_ayt.asm";
        ofstream asmfile(filePath);
        if (!asmfile) {
            throw runtime_error("Failed writing file : " + filePath);
        }

        dumpDb(asmfile, ayt_header, ayt_header.size(), 0, ayt_header.size());
        asmfile << "patterns:" << endl;
        dumpDb(asmfile, finalBuffers.patterns, patternSize, 0, finalBuffers.patterns.size());
        asmfile << "sequences:" << endl;
        dumpDw(asmfile, interleavedData, numRegs * 2, 0, interleavedData.size());

        asmfile << "final_bytes:" << endl;
        if ((converter.activeRegs & 0x3fffu) != 0x3fffu) {
            asmfile << "init_table:" << endl;
        }
        */

        // CSV stats
        bool isDividingEvenly = (ymdata.header.frameCount % patternSize) == 0;
        bool isLoopingEvenly = (ymdata.header.loopFrame % patternSize) == 0;

        cout << baseName << ";" << ymdata.header.masterClock << ";" << ymdata.header.frequency
             << ";" << ymdata.header.frameCount << ";"
             << ((double)ymdata.header.frameCount) / ymdata.header.frequency << ";"
             << ymdata.header.loopFrame << ";" << finalBuffers.sequenced.size() << ";" << hex
             << real_changeMask /*converter.activeRegs*/ << dec << ";" << patternSize << ";"
             << isDividingEvenly << ";" << isLoopingEvenly << ";" << orig_patterns_size << ";"
             << finalBuffers.patterns.size() << ";" << interleavedData.size() << ";"
             << ayt_file.size() << ";" << endl;

    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        return 1;
    }

    return 0;
}
