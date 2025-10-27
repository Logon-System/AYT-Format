/*
 *
 *  ┓┏┳┳┓       ┏┓┓┏┏┳┓
 *  ┗┫┃┃┃  ╋┏┓  ┣┫┗┫ ┃
 *  ┗┛┛ ┗  ┗┗┛  ┛┗┗┛ ┻
 *  by
 *  ┏┓┏┓┏┓   ╻   ┓ ┏┓┏┓┏┓┳┓  ┏┓┓┏┏┓┏┳┓┏┓┳┳┓
 *  ┃┓┃┃┣┫  ━╋━  ┃ ┃┃┃┓┃┃┃┃  ┗┓┗┫┗┓ ┃ ┣ ┃┃┃
 *  ┗┛┣┛┛┗   ╹   ┗┛┗┛┗┛┗┛┛┗  ┗┛┗┛┗┛ ┻ ┗┛┛ ┗
 *
 *  // Proof of Concept by Tronic/GPA
 *  // Format definition and improvements by Longshot & Siko
 *  // C++ Conversion Tool: Siko
 *  // Ultra optimized Z80 Players by Longshot
 *
 *  Build with :
 *    g++ -std=c++17 -O2 -Wall YmToSeq.cpp -o YmToSeq
 *  Under windows (mingw or cygwin) you might add -static option to avoid a missing DLL error:
 *    g++ -std=c++17 -O2 -Wall YmToSeq.cpp -o YmToSeq -static
 *  And then:
 *    strip YmToSeq
 *
 */

#include "ym_to_ayt.h"
#include <csignal>

namespace fs = filesystem;

// Globals
struct Options options;
int verbosity = 1;
bool optimization_running = false;

// Générateur de nombres aléatoires
static random_device rd;
static mt19937 rng(rd());
static uniform_real_distribution<> dist_prob(0.0, 1.0);

uint32_t real_changeMask = 0;

/**
 * @brief Search platform name associated with a frequency
 * @param targetClock masterClock found in YM file
 * @return string : name of platform or 'unknown' if not found
 */
const PlatformFreq& find_platform_by_frequency(double targetClock) {
    for (const auto& platform : platformFrequencies) {
        // Comparison with tolerance
        if (fabs(targetClock - platform.freq) < EPSILON) {
            return platform;
        }
    }
    return platformFrequencies[0]; //{"Unknown", -1, 15};
}

/*
 * @brief Recherche la fréquence associée à un nom de plateforme donné.
 * @param platformName Le nom de la plateforme à rechercher.
 * @return double La fréquence associée ou FREQ_NOT_FOUND (-1.0) si le nom n'est
 * pas trouvé.
 */
static const PlatformFreq& find_frequency_by_platform(string& platformName) {
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

// ===== Correction fréquence TONES (R0..R5) : période12b *= coef =====
static void YMScalePeriods(array<ByteBlock, 16>& regs, double coef) {
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
}

static void YMScaleNoise(array<ByteBlock, 16>& regs, double coef) {
    // Noise
    auto& N = regs[YM_REG_FREQ_NOISE];
    const size_t frames = N.size();

    for (size_t f = 0; f < frames; ++f) {
        double scaled = round(double(N[f]) * coef);
        if (scaled > 15.0f)
            scaled = 15.0f;
        N[f] = (static_cast<uint8_t>(scaled)) & 0xFF;
    }
}

static void YMScaleEnvelope(array<ByteBlock, 16>& regs, double coef) {
    // Noise
    auto& NLSB = regs[YM_REG_FREQ_WF_LSB];
    auto& NMSB = regs[YM_REG_FREQ_WF_MSB];

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

        // An empty buffer is considered as 'constant', nd has no init value
        if (buffer.empty()) {
            continue;
        }

        // Check if a buffer is contant
        bool isConstant = true;
        // We skip the very 1st value, as sometimes a register only changes once, at the beginning
        // (0,255, 255, ...)
        uint8_t firstValue = buffer[1];
        if (buffer.size() > 2) {
            isConstant = all_of(buffer.begin() + 2, buffer.end() - 1,
                                [&firstValue](uint8_t val) { return val == firstValue; });
        }

        if (!isConstant) {
            real_changeMask |= (1u << i);
            changeMask |= (1u << i);
        } else {
            if (i == 13) {
                if (verbosity > 0) {
                    cout << "Reg13 is constant, but in AYT format V1, it cannot considered as fixed"
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

// Fonction Coût : Calcule la taille des patterns pour un ordre donné
double calculate_fitness(const vector<int>& ByteBlock_order, const map<int, ByteBlock>& ByteBlocks,
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
    result.optimized_block_order.push_back(current_ByteBlock_index);
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
            result.optimized_block_order.push_back(best_next_ByteBlock_index);
            included[best_next_ByteBlock_index] = true;
            current_ByteBlock_index = best_next_ByteBlock_index;
        } else {
            // Aucun chevauchement trouvé. Démarrer une nouvelle séquence.
            // (Dans cette approche gloutonne, on ne devrait pas arriver ici si les données sont
            // très liées)
            break;
        }
    }

    return result;
}

// Fonction pour reconstruire le Heap et les pointeurs finaux
OptimizedResult reconstruct_result(const vector<int>& best_order,
                                   const map<int, ByteBlock>& original_blocks, int patSize) {
    // Cette fonction reproduit l'étape finale de merge_blocks_greedy
    // en utilisant l'ordre optimal trouvé (best_order)
    OptimizedResult final_result;
    if (best_order.empty())
        return final_result;

    int current_block_index = best_order[0];
    const ByteBlock& first_block = original_blocks.at(current_block_index);

    final_result.optimized_heap.insert(final_result.optimized_heap.end(), first_block.begin(),
                                       first_block.end());
    final_result.optimized_pointers[current_block_index] = 0;
    final_result.optimized_block_order = best_order;

    int current_heap_pointer = 0; // Pointer to the start of the current block in the heap

    for (size_t i = 1; i < best_order.size(); ++i) {
        int prev_index = best_order[i - 1];
        int curr_index = best_order[i];

        const ByteBlock& B_prev = original_blocks.at(prev_index);
        const ByteBlock& B_curr = original_blocks.at(curr_index);

        int overlap = find_max_overlap(B_prev, B_curr, patSize);
        int non_overlapping_length = patSize - overlap;

        current_heap_pointer =
            final_result.optimized_pointers.at(prev_index) + non_overlapping_length;

        // Greffer le nouveau fragment au Heap
        final_result.optimized_heap.insert(final_result.optimized_heap.end(),
                                           B_curr.begin() + overlap, B_curr.end());

        final_result.optimized_pointers[curr_index] = current_heap_pointer;
    }

    return final_result;
}

// ------------------ Algorithme Genetique -------------------

// --- Opérateur de Croisement ---
vector<int> crossover_pmx(const vector<int>& parent1, const vector<int>& parent2) {
    size_t N = parent1.size();
    vector<int> child(N, -1);
    map<int, int> mapping; // Pour gérer le mapping des éléments

    // Choisir deux points de coupe aléatoirement
    uniform_int_distribution<> dist_cut(0, N - 1);
    size_t cut1 = dist_cut(rng);
    size_t cut2 = dist_cut(rng);
    if (cut1 > cut2)
        swap(cut1, cut2);
    if (cut1 == cut2) {
        cut2 = min(N - 1, cut1 + 1); // Assurer une zone de croisement
    }

    // 1. Copier la section centrale du Parent 1
    for (size_t i = cut1; i <= cut2; ++i) {
        child[i] = parent1[i];
        mapping[parent1[i]] = parent2[i]; // Mettre en place le mapping de la zone
    }

    // 2. Transférer les éléments restants du Parent 2
    for (size_t i = 0; i < N; ++i) {
        if (i >= cut1 && i <= cut2)
            continue; // Sauter la section centrale

        int element = parent2[i];

        // 3. Résoudre les conflits via le mapping
        while (mapping.count(element)) {
            element = mapping.at(element);
        }

        // Si l'élément (après mapping) n'est pas déjà dans la section centrale
        bool already_in_center = false;
        for (size_t k = cut1; k <= cut2; ++k) {
            if (child[k] == element) {
                already_in_center = true;
                break;
            }
        }

        if (!already_in_center) {
            child[i] = element;
        } else {
            if (find(child.begin(), child.end(), element) == child.end()) {
                child[i] = element;
            }
        }
    }

    // Correction finale pour garantir la permutation:
    vector<int> missing_elements;
    vector<bool> present(N, false);
    for (size_t i = 0; i < N; ++i) {
        if (child[i] != -1) {
            present[child[i]] = true;
        }
    }
    for (int i = 0; i < (int)N; ++i) {
        if (!present[i]) {
            missing_elements.push_back(i);
        }
    }

    size_t missing_idx = 0;
    for (size_t i = 0; i < N; ++i) {
        if (child[i] == -1) {
            child[i] = missing_elements[missing_idx++];
        }
    }

    return child;
}

// Opérateur de Mutation (Swap aléatoire)
void mutate(vector<int>& order, double tmut) {
    if (order.size() < 2)
        return;

    if (tmut > 0.99f || dist_prob(rng) < tmut) {
        uniform_int_distribution<> dist_idx(0, order.size() - 1);
        int idx1 = dist_idx(rng);
        int idx2 = dist_idx(rng);
        while (idx1 == idx2) {
            idx2 = dist_idx(rng);
        }
        swap(order[idx1], order[idx2]);
    }
}

/**
 * @brief Applique la mutation (swap aléatoire) avec une probabilité MUTATION_RATE
 * pour chaque position de la séquence.
 *
 * @param order L'ordre des blocs (permutation) à muter.
 */
void mutate_by_index_swap(vector<int>& order, double tmut) {
    if (order.size() < 2)
        return;
    size_t N = order.size();

    // Distribution uniforme pour le choix de la cible du swap
    uniform_int_distribution<> dist_idx(0, N - 1);

    for (size_t i = 0; i < N; ++i) {
        // Déterminer si l'élément à la position i doit muter
        if (dist_prob(rng) < tmut / (double)N) {

            // Si mutation, choisir une position cible j aléatoirement
            int target_j = dist_idx(rng);

            // Assurer que l'échange ne se fait pas avec lui-même
            while ((size_t)target_j == i) {
                target_j = dist_idx(rng);
            }

            // Effectuer le swap
            swap(order[i], order[target_j]);
            return;
        }
    }
}

/**
 * @brief Algorithme Évolutionnaire (μ + λ) pour affiner l'ordre des patterns.
 */
OptimizedResult
refine_order_with_evolutionary_algorithm(const OptimizedResult& glouton_result,
                                         const map<int, ByteBlock>& original_patterns,
                                         int patSize) {
    size_t N = original_patterns.size();
    if (N < 2)
        return glouton_result;

    // 1. Initialisation de la Population
    // La population contient des paires (Ordre, Coût)
    vector<pair<vector<int>, double>> population;

    // Utiliser l'ordre glouton comme premier individu
    double glouton_fitness =
        calculate_fitness(glouton_result.optimized_block_order, original_patterns, patSize);
    population.push_back({glouton_result.optimized_block_order, glouton_fitness});

    // Remplir le reste de la population avec des ordres aléatoires
    vector<int> base_order(N);
    iota(base_order.begin(), base_order.end(), 0); // Ordre 0, 1, 2, ...
    int BestCost0 = 0;
    int BestCost1 = 0;
    int bestCostLog = 0;

    int max_generations = options.GA_NUM_GENERATION_MIN;

    cout << "Num Gen" << max_generations << endl;

    for (size_t i = 1; i < options.GA_MU; ++i) {
        shuffle(base_order.begin(), base_order.end(), rng);
        double fitness = calculate_fitness(base_order, original_patterns, patSize);
        population.push_back({base_order, fitness});
    }

    // 2. Boucle Évolutionnaire
    for (int gen = 0; gen < max_generations && optimization_running; ++gen) {

        vector<pair<vector<int>, double>> children;

        // Création des λ enfants
        for (size_t i = 0; i < options.GA_LAMBDA; ++i) {

            // Sélection des Parents (ici, sélection aléatoire simple parmi les meilleurs existants)
            uniform_int_distribution<> dist_parent(0, population.size() - 1);
            const auto& p1 = population[dist_parent(rng)];
            const auto& p2 = population[dist_parent(rng)];

            vector<int> child_order;
            //            uniform_real_distribution<> dist_prob(0.0, 1.0);

            if (dist_prob(rng) < options.GA_CROSSOVER_RATE) {
                // Croisement
                child_order = crossover_pmx(p1.first, p2.first);
            } else {
                // Simple copie (reproduction) du meilleur parent
                child_order = (p1.second < p2.second) ? p1.first : p2.first;
            }

            // Mutation
            // int nmut = 1;
            double tmut = 0.05;
            if (BestCost1 == BestCost0) {
                // nmut = (int) (dist_prob(rng)*5);
                tmut = 1;
                // for (int i=0; i<=nmut; i++)
                //    mutate2(child_order);
            }

            if (BestCost1 > BestCost0 && p1.second >= BestCost0) {
                // nmut = 1+p1.second-BestCost0;
                float dc = p1.second - BestCost0;
                if (dc > 0)
                    tmut = 0.01 + 0.5 * dc / (double)(BestCost1 - BestCost0);

                // cout << p1.second << " " << BestCost0 << " " << BestCost1 << " " << nmut << " "
                // << tmut << endl;
            }

            switch (i & 15) {
            case 0:
                mutate(child_order, 1.0f);
                break;
            case 1:
                mutate_by_index_swap(child_order, tmut);
                break;
            default:
                mutate(child_order, tmut);
                break;
            }

            //  Évaluation de l'enfant
            double child_fitness = calculate_fitness(child_order, original_patterns, patSize);
            children.push_back({move(child_order), child_fitness});
        }

        // 3. Sélection (μ + λ) : Les μ meilleurs survivent

        // Combiner parents et enfants
        population.insert(population.end(), children.begin(), children.end());

        // Tri par coût (les plus petits coûts sont les meilleurs)
        sort(population.begin(), population.end(),
             [](const auto& a, const auto& b) { return a.second < b.second; });

        // Réduire la population aux μ meilleurs (Survival Selection)
        // On pourrait garder quelques individus moins bons.
        if (population.size() > options.GA_MU) {
            population.resize(options.GA_MU);
        }

        // Affichage des stats optionnel
        BestCost0 = population[0].second;
        BestCost1 = population[options.GA_MU - 1].second;

        if (bestCostLog == 0 || bestCostLog > BestCost0) {
            max_generations = max(max_generations, options.GA_ADDITIONAL_GENERATIONS + gen);

            if (options.GA_NUM_GENERATION_MAX > 0)
                max_generations = min(max_generations, options.GA_NUM_GENERATION_MAX);

            if (verbosity > 0)
                cout << "GA Optimizer: Gen #" << gen << ": Fitness Range = " << population[0].second
                     << "-" << population[options.GA_MU - 1].second
                     << " Max Gen=" << max_generations << "\n";
            bestCostLog = BestCost0;
        }
    }

    // 4. Finalisation
    const auto& best_solution = population[0];
    // cout << "Coût final (AE): " << best_solution.second << " octets.\n";

    // Reconstruire le Heap et les pointeurs avec l'ordre le plus optimal trouvé
    return reconstruct_result(best_solution.first, original_patterns, patSize);
}

static ResultSequences buildBuffers(const array<ByteBlock, 16>& rawData, uint16_t activeRegs,
                                    int patSize) {

    // --- Phase 1: Découpage en blocs et déduplication initiale ---
    OptimizedResult no_opt_result;

    vector<ByteBlock> sequenceBuffers;
    map<int, ByteBlock>
        uniquePatternsMap; // Clé: Index de Pattern (0, 1, 2...), Valeur: Le Pattern lui-même
    unordered_map<uint32_t, int>
        uniqueHashes; // Hash -> Index de Pattern (pour la recherche rapide)

    int next_pattern_idx = 0; // Compteur pour les indices de pattern (0, 1, 2, ...)

    for (size_t i = 0; i <= 14; ++i) {
        if (!(activeRegs & (1 << i))) {
            continue;
        }

        const auto& src = rawData[i];
        const size_t N = src.size();
        const size_t num_blocks = (N == 0) ? 0 : ((N + size_t(patSize) - 1) / size_t(patSize));

        ByteBlock seq;
        seq.reserve(num_blocks * 2);

        // PointerSequence seq;
        // seq.reserve(num_blocks); // Moins de mémoire nécessaire!

        for (size_t blk = 0; blk < num_blocks; ++blk) {
            size_t start = blk * size_t(patSize);
            size_t remain = (start < N) ? (N - start) : 0;
            size_t take = min(remain, size_t(patSize));

            // Création du Pattern (avec padding)
            ByteBlock pat(patSize);
            if (take) {
                memcpy(pat.data(), &src[start], take);
                uint8_t pad = src[start + take - 1];
                if (take < (size_t)patSize)
                    memset(pat.data() + take, pad, size_t(patSize) - take);
            } else {
                memset(pat.data(), 0, size_t(patSize));
            }

            // Déduplication par Hachage et Comparaison
            uint32_t h = fnv1a32(pat);
            int idx;

            // Recherche du hash
            auto it_hash = uniqueHashes.find(h);
            bool found = false;

            if (it_hash != uniqueHashes.end()) {
                // Le hash est déjà là. On doit vérifier le contenu pour les collisions
                int existing_idx = it_hash->second;
                if (uniquePatternsMap.at(existing_idx) == pat) {
                    idx = existing_idx;
                    found = true;
                }
                // Si collision (le pattern n'est pas le même), on le traite comme un nouveau
                // pattern ci-dessous.
            }

            if (!found) {
                // Nouveau pattern
                idx = next_pattern_idx++;
                uniqueHashes[h] = idx;
                uniquePatternsMap[idx] = move(pat); // Stockage du pattern

                if (options.optimizationLevel == 0) {
                    no_opt_result.optimized_heap.insert(no_opt_result.optimized_heap.end(),
                                                         pat.begin(), pat.end());
                }
            }

            if (idx > 0xFFFF)
                throw runtime_error("Pattern index exceeds 16-bit");

            // Stockage de l'Index (16 bits, Little-Endian)
            seq.push_back(uint8_t(idx & 0xFF));
            seq.push_back(uint8_t((idx >> 8) & 0xFF));
        }
        sequenceBuffers.push_back(move(seq));
    }

    if (options.optimizationLevel != 0) {

        // --- Phase 2: Fast overlap optimization (gluttony) ---
        auto initial_result = merge_ByteBlocks_greedy(uniquePatternsMap, patSize);
        return {move(sequenceBuffers), move(uniquePatternsMap), move(initial_result)};
    }

    // return no opt
    return {move(sequenceBuffers), move(uniquePatternsMap), move(no_opt_result)};
}

void replaceByOptimizedIndex(vector<ByteBlock>& sequenceBuffers,
                             const OptimizedResult& optimized_result) {

    // --- Phase 3: Remplacement des Index ---
    for (size_t i = 0; i < sequenceBuffers.size(); ++i) {
        ByteBlock& seq = sequenceBuffers[i];
        for (size_t j = 0; j < seq.size(); j += 2) {

            // 1. Lecture de l'Ancien Index (16 bits, Little-Endian)
            // L'ancien index est la clé pour trouver le nouvel offset.
            int old_idx = seq[j] + (seq[j + 1] << 8);

            // 2. Récupération du Nouvel Offset (Pointeur)
            // L'utilisation de .at() garantit que l'index existe.
            int new_offset;
            try {
                new_offset = optimized_result.optimized_pointers.at(old_idx);
            } catch (const out_of_range& e) {
                throw runtime_error("Index de pattern manquant dans les pointeurs optimisés.");
            }

            // 3. Vérification de la Contrainte 16 bits (l'offset devient le nouveau 'pointeur'
            // 16-bit)
            if (new_offset > 0xFFFF)
                throw runtime_error("Nouvel offset (pointeur) dépasse 16-bit");

            // 4. Écriture du Nouvel Offset (16 bits, Little-Endian)
            seq[j] = uint8_t(new_offset & 0xFF);
            seq[j + 1] = uint8_t((new_offset >> 8) & 0xFF);
        }
    }
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

    writeAllBytes(baseName.string() + "_patterns.bin", buffers.optimizedOverlap.optimized_heap);
    dumpAsDb(baseName.string() + "_patterns.txt", buffers.optimizedOverlap.optimized_heap,
             16); // converter.patternSize);

    for (size_t regIndex = 0; regIndex < buffers.sequenced.size(); ++regIndex) {

        string prefix = baseName.string() + "_R" + (regIndex < 10 ? "0" : "") + to_string(regIndex);
        writeAllBytes(prefix + "_sequences.bin", buffers.sequenced[regIndex]);
        dumpAsDb(prefix + "_sequences.txt", buffers.sequenced[regIndex], 16);
    }
}

// ----------- RECUIT SIMULE ----------------

// Fonction utilitaire pour générer un état voisin par "swap" (Mutation)
vector<int> get_neighbor_by_swap(const vector<int>& current_order, mt19937& rng) {
    vector<int> neighbor = current_order;
    if (neighbor.size() < 2)
        return neighbor;

    uniform_int_distribution<> distrib(0, neighbor.size() - 1);
    int idx1 = distrib(rng);
    int idx2 = distrib(rng);

    // S'assurer que les indices sont différents
    while (idx1 == idx2) {
        idx2 = distrib(rng);
    }
    swap(neighbor[idx1], neighbor[idx2]);
    return neighbor;
}

// ----------------------------------------------------------------------------

/**
 * @brief Améliore l'ordre glouton en utilisant l'algorithme de Recuit Simulé (SA).
 *
 * @param glouton_result Le résultat initial de l'approche gloutonne.
 * @param original_patterns La map des patterns originaux (Index -> Block).
 * @param max_iterations Nombre maximal d'itérations (ou de "refroidissements").
 * @param patSize Taille des blocs (patterns).
 * @return OptimizedResult Le résultat final optimisé.
 */
OptimizedResult refine_order_with_simulated_annealing(const OptimizedResult& glouton_result,
                                                      const map<int, ByteBlock>& original_patterns,
                                                      int max_iterations, int patSize) {
    if (glouton_result.optimized_block_order.size() < 2)
        return glouton_result;

    // Initialisation
    vector<int> current_order = glouton_result.optimized_block_order;
    vector<int> best_order = current_order;
    double current_cost = calculate_fitness(current_order, original_patterns, patSize);
    double best_cost = current_cost;

    // --- Paramètres du Recuit Simulé (SA) ---
    // 1. Température Initiale (T0) : Assez élevée pour accepter les mauvais mouvements
    double initial_temp = 1000.0; // Une valeur arbitraire, doit être ajustée
    // 2. Taux de Refroidissement (Alpha) : Proche de 1 pour un refroidissement lent (meilleur
    // résultat)
    const double alpha = 0.999;
    // 3. Critère d'Arrêt (simplifié ici par max_iterations)

    double temp = initial_temp;
    uniform_real_distribution<> prob_distrib(0.0, 1.0);

    // Le nombre d'itérations à chaque température doit être ajusté pour les grands N.
    // Pour simplifier, nous utilisons le max_iterations total comme un compteur de cycle de
    // refroidissement.
    const int ITER_PER_TEMP_STEP = 100;

    cout << "--- Simulated anhealing ---\n";
    cout << "Initial fitness: " << best_cost << " bytes.\n";

    for (int iter = 0; iter < max_iterations && optimization_running; ++iter) {

        // Boucle interne (exploration à température constante)
        for (int step = 0; step < ITER_PER_TEMP_STEP; ++step) {

            // Mutation (Création d'un voisin)
            vector<int> neighbor_order = get_neighbor_by_swap(current_order, rng);
            double neighbor_cost = calculate_fitness(neighbor_order, original_patterns, patSize);

            double delta_E = neighbor_cost - current_cost; // Gain = E_nouveau - E_ancien

            //            cout << "Current cost: " << current_cost <<endl;

            // 1. Acceptance condition
            if (delta_E < 0) {
                // Solution améliorée : accepter toujours
                current_order = neighbor_order;
                current_cost = neighbor_cost;

                if (current_cost < best_cost) {
                    best_cost = current_cost;
                    best_order = current_order;
                    cout << "Found a better solution: " << best_cost << endl;
                }
            } else {
                // Solution moins bonne : accepter avec probabilité exp(-delta_E / T)
                double acceptance_prob = exp(-delta_E / temp);

                if (prob_distrib(rng) < acceptance_prob) {
                    // Mouvement accepté pour échapper aux optima locaux
                    current_order = neighbor_order;
                    current_cost = neighbor_cost;
                }
            }
        }

        // 2. Refroidissement
        temp *= alpha;

        // Optionnel : Arrêt si la température devient trop faible
        if (temp < 1e-4) {
            break;
        }
    }

    cout << "Best Fitness (SA): " << best_cost << " bytes.\n";

    // Reconstruire le Heap et les pointeurs avec l'ordre le plus optimal trouvé
    return reconstruct_result(best_order, original_patterns, patSize);
}

// ---------------------------------------------------

// Le type de la Liste Tabou : stocke les paires d'indices échangés (les mouvements)
using TabuList = list<pair<int, int>>;
const size_t TABU_TENURE = 20; // Durée de l'interdiction (ex: 20 itérations)

// Une fonction helper pour vérifier si un mouvement est tabou
bool is_tabu(const TabuList& tabu_list, int idx1, int idx2) {
    // Le mouvement est symétrique, donc vérifier (idx1, idx2) et (idx2, idx1)
    return find(tabu_list.begin(), tabu_list.end(), make_pair(idx1, idx2)) != tabu_list.end() ||
           find(tabu_list.begin(), tabu_list.end(), make_pair(idx2, idx1)) != tabu_list.end();
}

/**
 * @brief Améliore l'ordre glouton en utilisant la Recherche Tabou.
 *
 * @param glouton_result Le résultat initial de l'approche gloutonne.
 * @param original_patterns La map des patterns originaux (Index -> Block).
 * @param max_iterations Nombre maximal d'itérations.
 * @param patSize Taille des blocs.
 * @return OptimizedResult Le résultat final optimisé.
 */
OptimizedResult refine_order_with_tabu_search(const OptimizedResult& glouton_result,
                                              const map<int, ByteBlock>& original_patterns,
                                              int max_iterations, int patSize) {
    if (glouton_result.optimized_block_order.size() < 2)
        return glouton_result;

    vector<int> current_order = glouton_result.optimized_block_order;
    vector<int> best_order = current_order;
    double current_cost = calculate_fitness(current_order, original_patterns, patSize);
    double best_cost = current_cost;

    TabuList tabu_list;

    size_t N = current_order.size();

    cout << "--- Recherche Tabou ---\n";
    cout << "Coût initial (Glouton): " << best_cost << " octets.\n";

    for (int iter = 0; iter < max_iterations && optimization_running; ++iter) {

        double best_neighbor_cost = numeric_limits<double>::max();
        vector<int> best_neighbor_order;
        pair<int, int> best_move = {-1, -1}; // Indices de la permutation (pas des patterns)

        // 1. Exploration du Voisinage (Recherche du Meilleur Voisin non Tabou)
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = i + 1; j < N; ++j) {

                // Mouvement : échange des éléments aux positions i et j
                vector<int> neighbor_order = current_order;
                swap(neighbor_order[i], neighbor_order[j]);

                double neighbor_cost =
                    calculate_fitness(neighbor_order, original_patterns, patSize);

                // Critère d'Aspiration : Accepter un mouvement tabou si c'est le meilleur jamais
                // trouvé
                bool aspiration_criterion = neighbor_cost < best_cost;
                if (neighbor_cost < best_neighbor_cost &&
                    (!is_tabu(tabu_list, i, j) || aspiration_criterion)) {
                    best_neighbor_cost = neighbor_cost;
                    best_neighbor_order = neighbor_order;
                    best_move = {i, j};
                }
            }
        }

        // 2. Mise à jour de la solution (si un mouvement valide a été trouvé)
        if (best_move.first != -1) {

            // Effectuer le mouvement
            current_order = best_neighbor_order;
            current_cost = best_neighbor_cost;

            // Mettre à jour la meilleure solution globale
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best_order = current_order;
                cout << "TABU: found a better solution:" << best_neighbor_cost << endl;
            }

            // Mettre à jour la Liste Tabou
            tabu_list.push_back(best_move);
            if (tabu_list.size() > TABU_TENURE) {
                tabu_list.pop_front();
            }

        } else {
            // Tous les mouvements sont tabous ou le voisinage n'améliore pas
            // Dans une implémentation robuste, on pourrait déclencher une forte perturbation ici.
            break;
        }
    }

    cout << "Coût final (TS): " << best_cost << " octets.\n";

    // Reconstruire le Heap et les pointeurs avec l'ordre le plus optimal trouvé
    return reconstruct_result(best_order, original_patterns, patSize);
}

// ----------------------- ILS

/**
 * @brief Perturbe l'ordre actuel en appliquant N_PERTURBATIONS swaps aléatoires.
 *
 * @param order L'ordre des blocs à perturber.
 * @param rng Le générateur de nombres aléatoires.
 * @return L'ordre perturbé.
 */
vector<int> perturb(vector<int> order, mt19937& rng) {
    size_t N = order.size();
    // Choisir le nombre de perturbations. 3 à 5% de la taille de N est un bon point de départ.
    const int N_PERTURBATIONS = max(1, (int)(N * 0.05));

    uniform_int_distribution<> distrib(0, N - 1);

    for (int k = 0; k < N_PERTURBATIONS; ++k) {
        int idx1 = distrib(rng);
        int idx2 = distrib(rng);
        while (idx1 == idx2) {
            idx2 = distrib(rng);
        }
        swap(order[idx1], order[idx2]);
    }
    return order;
}
/**
 * @brief Effectue une recherche locale agressive en testant tous les swaps possibles
 * et en acceptant le meilleur jusqu'à atteindre un optimum local.
 *
 * @param current_order L'ordre des blocs à améliorer.
 * @param original_patterns Les patterns originaux (pour le calcul du coût).
 * @param current_cost Le coût de l'ordre actuel.
 * @return La paire (ordre_optimal_local, coût_optimal_local).
 */
pair<vector<int>, double> local_search(const vector<int>& current_order,
                                       const map<int, ByteBlock>& original_patterns,
                                       double current_cost, int patSize) {
    vector<int> current = current_order;
    double cost = current_cost;
    size_t N = current.size();
    bool improved = true;

    // Répéter tant qu'une amélioration est possible
    while (improved) {
        improved = false;
        double best_local_gain = 0.0;
        int best_i = -1, best_j = -1;

        // Tester tous les swaps (voisinage)
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = i + 1; j < N; ++j) {

                // 1. Simuler le swap
                swap(current[i], current[j]);

                // 2. Calculer le nouveau coût
                double neighbor_cost = calculate_fitness(current, original_patterns, patSize);

                // 3. Évaluer l'amélioration
                double gain = cost - neighbor_cost; // gain > 0 si amélioration

                if (gain > best_local_gain) {
                    best_local_gain = gain;
                    best_i = i;
                    best_j = j;
                    improved = true;
                }

                // Annuler le swap pour la prochaine itération de la boucle
                swap(current[i], current[j]);
            }
        }

        // Appliquer le meilleur swap trouvé
        if (improved) {
            swap(current[best_i], current[best_j]);
            cost -= best_local_gain; // Mise à jour rapide du coût
        }
    }

    return {current, cost};
}

/**
 * @brief Améliore l'ordre glouton en utilisant la Recherche Locale Itérée (ILS).
 *
 * @param glouton_result Le résultat initial de l'approche gloutonne.
 * @param original_patterns La map des patterns originaux (Index -> Block).
 * @param max_iterations Nombre maximal d'itérations de la boucle ILS.
 * @param patSize Taille des blocs.
 * @return OptimizedResult Le résultat final optimisé.
 */
OptimizedResult refine_order_with_ils(const OptimizedResult& glouton_result,
                                      const map<int, ByteBlock>& original_patterns,
                                      int max_iterations, int patSize) {
    if (glouton_result.optimized_block_order.size() < 2)
        return glouton_result;

    //    random_device rd;
    //    mt19937 rng(rd());

    // Initialisation avec le résultat glouton (ou un ordre aléatoire)
    vector<int> current_order = glouton_result.optimized_block_order;
    double current_cost = calculate_fitness(current_order, original_patterns, patSize);

    // La meilleure solution trouvée
    vector<int> best_order = current_order;
    double best_cost = current_cost;

    cout << "--- Recherche Locale Itérée (ILS) ---\n";
    cout << "Coût initial (Glouton): " << best_cost << " octets.\n";

    for (int iter = 0; iter < max_iterations && optimization_running; ++iter) {
        // cout << iter << endl;
        //  1. Recherche Locale (Passer à l'optimum local S*)
        //  Cette étape est essentielle et consomme le plus de temps.
        auto local_optimum_pair =
            local_search(current_order, original_patterns, current_cost, patSize);
        vector<int> S_star = local_optimum_pair.first;
        double cost_S_star = local_optimum_pair.second;

        // Mise à jour de la meilleure solution globale
        if (cost_S_star < best_cost) {
            best_cost = cost_S_star;
            best_order = S_star;
            cout << "ILS: best solution fitness: " << best_cost << " octets.\n";
        }

        // 2. Perturbation (Créer S' très différent de S*)
        vector<int> S_prime = perturb(S_star, rng);
        double cost_S_prime = calculate_fitness(S_prime, original_patterns, patSize);

        // 3. Critère d'Acceptation (Acceptation purement déterministe ici : accepter si S' est
        // meilleur) Pour une version plus robuste (similaire au SA), on pourrait ajouter une
        // probabilité d'acceptation.
        if (cost_S_prime < cost_S_star) {
            current_order = S_prime;
            current_cost = cost_S_prime;
        } else {
            // Si la perturbation est mauvaise, on revient à l'optimum local pour la prochaine
            // itération
            current_order = S_star;
            current_cost = cost_S_star;
        }
    }

    cout << "Coût final (ILS): " << best_cost << " octets.\n";

    // Reconstruire le Heap et les pointeurs avec l'ordre le plus optimal trouvé
    return reconstruct_result(best_order, original_patterns, patSize);
}

//======================================================================================
// CLI
//======================================================================================
static void printUsage(const char* prog) {
    cout << "Usage: " << prog << " [options] YM-FILE" << endl
         << "  -h, --help                   Show help" << endl
         << "  -v, --verbose                Increase verbosity" << endl
         << "  -q, --quiet                  Low verbosity" << endl
         << "  -s, --save                   Save intermediary files" << endl
         << "  -p, --pattern-size N[:N][/N] Pattern search range (presets: 'full', 'auto')" << endl
         << "  -l, --only-evenly-looping    Skip sizes that don't loop at thebeginning of a pattern"
         << endl
         << "  -sp, --scale-period F        Multiply tone periods (R0..R5) by F (e.g., 0.5,2)"
         << endl
         << "  -sn, --scale-noise F         Multiply tone periods (R0..R5) by F (e.g., 0.5,2)"
         << endl
         << "  -sv, --scale-envelope F      Multiply tone periods (R0..R5) by F (e.g., 0.5,2)"
         << endl
         << "  -t, --target ARCH            Name of the target architecture ("
         << generate_platform_list() << ")" << endl
         << "  -o, --output FILE            Name of the output file"
         << "  -O, --overlap-optim N        Overlap optimization method (none, ga, tabu, "
            "sa, ifs)"
         << "  -P, --output-path PATH       Folder where to store results" << endl
         << "  -c, --csv                    Export stats in CSV format" << endl
         << "  --GA-pop-size M L            Size of population Mu and Lambda" << endl
         << "  --GA-pop-min-generation N    Minimum number of generations" << endl
         << "  --GA-pop-max-generation N    Maximum number of generations" << endl
         << "  --GA-pop-ext-generation N    When finding a new solution, extends number of "
            "generation, for further search"
         << endl;
}

/**
 * @brief Parse le paramètre de taille de motif (Min[:Max][/Step]) et met à jour les options.
 * @param size_str La chaîne de l'argument (e.g., "4", "4:64", "4:64/4", "full", "auto").
 * @param options La structure Options à mettre à jour.
 * @return true si l'analyse est réussie, false sinon.
 */
bool parsePatternSize(const string& size_str, Options& options) {
    string s = size_str;

    if (s == "full") {
        options.patternSizeMin = 1;
        options.patternSizeMax = 128;
        options.patternSizeStep = 1;
        return true;
    }
    if (s == "auto") {
        options.patternSizeMin = 4;
        options.patternSizeMax = 64;
        options.patternSizeStep = 4;
        return true;
    }

    options.sizeParam = s; // Sauvegarder la valeur brute

    try {
        size_t colon_pos = s.find(':');
        size_t slash_pos = s.find('/');

        // 1. Gérer le Step (s'il existe)
        string step_str = "1";
        if (slash_pos != string::npos) {
            step_str = s.substr(slash_pos + 1);
            s = s.substr(0, slash_pos);
        }
        int step = stoi(step_str);
        if (step < 1) {
            cerr << "Pattern Size Step must be >= 1" << endl;
            return false;
        }
        options.patternSizeStep = step;

        // 2. Gérer Min et Max
        if (colon_pos == string::npos) {
            // Un seul nombre N -> Min=N, Max=N
            int n = stoi(s);
            if (n < 1 || n > 256) {
                cerr << "Pattern Size must be 1..256" << endl;
                return false;
            }
            options.patternSizeMin = n;
            options.patternSizeMax = n;
        } else {
            // Deux nombres Min:Max
            string min_str = s.substr(0, colon_pos);
            string max_str = s.substr(colon_pos + 1);

            int min_val = stoi(min_str);
            int max_val = stoi(max_str);

            if (min_val < 1 || max_val > 256 || min_val > max_val) {
                cerr << "Pattern Size Min/Max must be 1..256 and Min <= Max" << endl;
                return false;
            }
            options.patternSizeMin = min_val;
            options.patternSizeMax = max_val;
        }

        return true;

    } catch (...) {
        cerr << "Invalid pattern size format: " << size_str << ". Expected: N, N:M, or N:M/S"
             << endl;
        return false;
    }
}

double parseDouble(const string& s) { return stod(s); }
int parseInt(const string& s) { return stoi(s); }
string parseString(const string& s) { return s; }

template <typename T>
bool parseOptionArgument(const vector<string>& optionNames, T Options::*member, Options& options,
                         char** argv, int& i, int argc, T (*converter)(const string&)) {
    const string& arg = argv[i];
    // Vérifie si l'argument actuel correspond à une des options attendues
    bool isOption = false;
    for (const auto& name : optionNames) {
        if (arg == name) {
            isOption = true;
            break;
        }
    }
    if (!isOption) {
        return false; // Ce n'est pas cette option, on passe
    }

    // Parsing de l'argument
    if (i + 1 >= argc) {
        cerr << "Option " << arg << " requires an argument" << endl;
        return false;
    }
    try {
        options.*member = converter(argv[++i]);
        if (verbosity > 1)
            cout << "Option " << arg << "=" << options.*member << endl;
        return true;
    } catch (...) {
        cerr << "Invalid argument for " << arg << endl;
        return false;
    }
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

#ifndef TEST_MODE

static void signal_handler(int sig) {
    // cout << "Signal " << sig << " opt running:" << optimization_running;
    if (optimization_running == true)
        optimization_running = false;
    else
        exit(1);
}

int main(int argc, char** argv) {

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // string outputFileName = "";
    vector<string> inputPaths;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            ++verbosity;
            continue;
        } else if (arg == "-q" || arg == "--quiet") {
            verbosity = 0;
            continue;
        } else if (arg == "-s" || arg == "--save") {
            options.saveFiles = true;
            continue;
        } else if (arg == "-l" || arg == "--only-evenly-looping") {
            options.onlyDivisible = true;
            continue;
        } else if (arg == "-c" || arg == "--csv") {
            options.exportCsv = true;
            continue;
        }

        /* ------------------------------------------------------------------------------------------------
         */
        /* Options nécessitant un argument (ou plus) */

        else if (arg == "-t" || arg == "--target") {
            if (i + 1 >= argc) {
                cerr << "Option " << arg << " requires an argument" << endl;
                return 1;
            }

            options.targetParam = argv[++i];

            const PlatformFreq& pf = find_frequency_by_platform(options.targetParam);
            options.targetClock = pf.freq;
            options.targetId = pf.id;
            if (options.targetClock < 0) {
                cerr << "Option -t argument must be a known platform (e.g., ATARI, CPC, ...)"
                     << endl;
                return 1;
            }
            continue;
        } else if (arg == "-p" || arg == "--pattern-size") {
            if (i + 1 >= argc) {
                cerr << "Option " << arg << " requires an argument" << endl;
                return 1;
            }
            if (!parsePatternSize(argv[++i], options)) {
                return 1; // Erreur gérée dans parsePatternSize
            }
            continue;
        }

        if (parseOptionArgument({"-sp", "--scale-period"}, &Options::periodCoef, options, argv, i,
                                argc, &parseDouble)) {
            if (options.periodCoef <= 0.0) {
                cerr << "Period coefficient must be > 0" << endl; // exception?
                return 1;
            }
            continue;
        }

        if (parseOptionArgument({"-sn", "--scale-noise"}, &Options::noiseCoef, options, argv, i,
                                argc, &parseDouble)) {
            if (options.noiseCoef <= 0.0) {
                cerr << "Envelope coefficient must be > 0" << endl; // exception?
                return 1;
            }
            continue;
        }

        if (parseOptionArgument({"-se", "--scale-envelope"}, &Options::envCoef, options, argv, i,
                                argc, &parseDouble)) {
            if (options.envCoef <= 0.0) {
                cerr << "Envelope coefficient must be > 0" << endl; // exception?
                return 1;
            }
            continue;
        }

        if (parseOptionArgument({"-O", "--overlap-optim"}, &Options::optimizationMethod, options,
                                argv, i, argc, &parseString)) {

            if (options.optimizationMethod == "none") {
                options.optimizationLevel = 0;
                continue;
            }
            if (options.optimizationMethod == "ga") {
                options.optimizationLevel = 2;
                continue;
            }
            if (options.optimizationMethod == "sa") {
                options.optimizationLevel = 2;
                continue;
            }
            if (options.optimizationMethod == "ils") {
                options.optimizationLevel = 2;
                continue;
            }
            if (options.optimizationMethod == "tabu") {
                options.optimizationLevel = 2;
                continue;
            }
            if (options.optimizationMethod == "glutonny") {
                options.optimizationLevel = 1;
                continue;
            }
            cerr << "Unknown optimization method:" << options.optimizationMethod;
            return 1;
        }

        if (parseOptionArgument({"-P", "--output-path"}, &Options::outputPath, options, argv, i,
                                argc, parseString)) {
            continue;
        }

        /* Options GA */
        if (arg == "--GA-pop-size") {
            if (i + 2 >= argc) {
                cerr << "Option " << arg << " requires two arguments: Mu and Lambda" << endl;
                return 1;
            }
            try {
                options.GA_MU = stoul(argv[++i]);
                options.GA_LAMBDA = stoul(argv[++i]);
            } catch (...) {
                cerr << "Invalid population sizes for " << arg << endl;
                return 1;
            }
            if (options.GA_MU < 1 || options.GA_LAMBDA < 1) {
                cerr << "Population sizes must be >= 1" << endl;
                return 1;
            }
            continue;
        }

        if (parseOptionArgument({"--GA-min-generation"}, &Options::GA_NUM_GENERATION_MIN, options,
                                argv, i, argc, &parseInt)) {
            cout << "**** min gen" << options.GA_NUM_GENERATION_MIN << endl;
            continue;
        }

        if (parseOptionArgument({"--GA-max-generation"}, &Options::GA_NUM_GENERATION_MAX, options,
                                argv, i, argc, &parseInt)) {
            continue;
        }

        if (parseOptionArgument({"--GA-ext-generation"}, &Options::GA_ADDITIONAL_GENERATIONS,
                                options, argv, i, argc, &parseInt)) {
            continue;
        }

        if (!arg.empty() && arg[0] != '-') {
            inputPaths.push_back(arg);
        } else {
            cerr << "Unknown option: " << arg << endl;
            return 1;
        }
    }

    //--  Options parsing finished

    if (inputPaths.empty()) {
        cerr << "Error: No input FILE specified." << endl;
        printUsage(argv[0]);
        return 1;
    }

    for (auto path : inputPaths) {
        try {
            fs::path inputPath = path;

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

            // Computes automatically coefficients to appy to registers, depending on YM masterClock
            // and target platform
            if (options.targetClock > 0) {
                if (options.periodCoef < 0)
                    options.periodCoef = options.targetClock / ymdata.header.masterClock;
                if (options.noiseCoef < 0)
                    options.noiseCoef = options.targetClock / ymdata.header.masterClock;
                if (options.envCoef < 0)
                    options.envCoef = options.targetClock / ymdata.header.masterClock;
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
                    cout << "Applying tone period multiplier (R0..R5): " << options.periodCoef
                         << endl;
                YMScalePeriods(ymdata.rawRegisters, options.periodCoef);
            }

            if (options.periodCoef > 0) {
                if (verbosity)
                    cout << "Applying tone period multiplier (R0..R5): " << options.periodCoef
                         << endl;
                YMScaleNoise(ymdata.rawRegisters, options.periodCoef);
            }

            if (options.envCoef > 0) {
                if (verbosity)
                    cout << "Applying tone period multiplier (R0..R5): " << options.envCoef << endl;
                YMScaleEnvelope(ymdata.rawRegisters, options.envCoef);
            }


            // Determine number of registers and fixed values
            converter.activeRegs =
                analyze_data_buffers(ymdata.rawRegisters, converter.initRegValues);

            // devrait etre calculé avant
            int numRegs = 0;
            for (int r = 0; r < 14; ++r) {
                // Check if register is exported
                if (converter.activeRegs & (1 << r)) {
                    numRegs += 1;
                }
            }



            // If not dividing evenly, we add final sequence to rawRegisters
            auto rawRegistersWithEndSequence = ymdata.rawRegisters;
            for (int i=0; i<16; i++) {
                rawRegistersWithEndSequence[i].push_back(final_sequence_values[final_sequence[i]]);

            }



            // If dividing evenly, we can create an additional sequence, or replace last values with special sequence
            // Depending on options.addFinalSequence
            if (!options.addFinalSequence) {
                for (int i=0; i<16; i++) {
                    uint8_t v = final_sequence_values[final_sequence[i]];
                    size_t s = ymdata.rawRegisters[i].size();
                    //if (v!=0) {
                        ymdata.rawRegisters[i][s-1] = v;
                    //}
                }
            }

            cout << "rawRegistersWithEndSequence size = "<<rawRegistersWithEndSequence[0].size()<<endl;
            cout << "rawRegisters                size = "<<ymdata.rawRegisters[0].size();


            // Save Raw register patterns
            if (options.saveFiles) {
                for (size_t r = 0; r < ymdata.rawRegisters.size(); ++r) {
                    saveRawData(baseName, ymdata.rawRegisters[r],
                                string("_Raw_R") + (r < 10 ? "0" : "") + to_string(r));
                }
            }


            size_t bestTotal = static_cast<size_t>(-1);
            int bestSize = -1;

            for (int s = options.patternSizeMax; s >= options.patternSizeMin;
                 s -= options.patternSizeStep)
            // Utilisez une nouvelle portée pour garantir la destruction immédiate
            {
                size_t currentTotal = 0;
                //          vector<AYTConverter> currentBuffers(numActiveRegs);
                ResultSequences currentBuffers;
                bool valid = true;

                bool isDividingEvenly = (ymdata.header.frameCount % s) == 0;

                if ((options.onlyDivisible == true) && !isDividingEvenly) {
                    valid = false;
                } else {
                    if (verbosity > 2)
                        cout << "Testing Pattern Size " << s << " :";

                    try {

                        if (isDividingEvenly)
                            currentBuffers =
                                buildBuffers(ymdata.rawRegisters, converter.activeRegs, s);
                        else
                            currentBuffers =
                                buildBuffers(rawRegistersWithEndSequence, converter.activeRegs, s);

                        currentTotal = currentBuffers.optimizedOverlap.optimized_heap.size();

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
                                cout << "New Best Pattern size found: " << s
                                     << " (Total Size: " << currentTotal << " bytes)"
                                     << (isDividingEvenly ? "" : " (info: not evenly dividing)")
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

            if (options.optimizationLevel > 1) {

                // Amélioration de l'ordre par stratégie d'évolution
                optimization_running = true;
                OptimizedResult final_result;

                if (options.optimizationMethod == "ga") {
                    final_result = refine_order_with_evolutionary_algorithm(
                        finalBuffers.optimizedOverlap, finalBuffers.patternMap, patternSize);
                }

                if (options.optimizationMethod == "sa") {
                    final_result = refine_order_with_simulated_annealing(
                        finalBuffers.optimizedOverlap, finalBuffers.patternMap, 1000, patternSize);
                }
                if (options.optimizationMethod == "ils") {
                    final_result = refine_order_with_ils(
                        finalBuffers.optimizedOverlap, finalBuffers.patternMap,
                        100, // Ex: nb patterns **2 // 10 000 itérations
                        patternSize);
                }
                if (options.optimizationMethod == "tabu") {
                    final_result = refine_order_with_tabu_search(
                        finalBuffers.optimizedOverlap, finalBuffers.patternMap, 50, patternSize);
                }

                finalBuffers.optimizedOverlap = final_result;
            }

            replaceByOptimizedIndex(finalBuffers.sequenced, finalBuffers.optimizedOverlap);

            // Interlacing sequences
            if (verbosity > 1)
                cout << "\nInterlacing " << finalBuffers.sequenced.size() << " sequence buffers";

            ByteBlock interleavedData = interleaveSequenceBuffers(finalBuffers.sequenced);

            // Sauvegarde des fichiers intermediaires si demandé => dans le _ayt.asm
            if (options.saveFiles) {
                saveOutputs(baseName, finalBuffers);
                cout << "Intermediary files saved for patternSize " << patternSize << endl;
            }

            bool isDividingEvenly = (ymdata.header.frameCount % patternSize) == 0;
            bool isLoopingEvenly = (ymdata.header.loopFrame % patternSize) == 0;

            if (verbosity > 0) {
                cout << " Active Regs: " << numRegs << "0x" << hex << converter.activeRegs << dec
                     << endl;
                cout << " Pattern Length: " << patternSize << endl;

                cout << " Init Sequence: ";
                for (const auto& pair : converter.initRegValues) {
                    cout << pair.first << "=" << static_cast<int>(pair.second) << " ";
                }
                cout << endl;

                totalSize =
                    finalBuffers.optimizedOverlap.optimized_heap.size() + interleavedData.size();
                cout << " Loop to sequence : " << (ymdata.header.loopFrame / patternSize);
                if (!isLoopingEvenly) {
                    cout << " (Warn: not looping to the start of a pattern) "
                         << ymdata.header.loopFrame << "/" << patternSize;
                }
                cout << endl;

                cout << " Patterns size : " << finalBuffers.optimizedOverlap.optimized_heap.size()
                     << " bytes" << endl;
                cout << " Sequences size : " << interleavedData.size() << " bytes" << endl;
                cout << " Total size (sequences + patterns) : " << totalSize << " bytes" << endl;
            }

            // Export AYTs
            ByteBlock ayt_header;
            ayt_header.resize(14);

            ByteBlock ayt_file;
            uint16_t offset_sequences =
                ayt_header.size() + finalBuffers.optimizedOverlap.optimized_heap.size();
            uint16_t offset_loop_sequence =
                offset_sequences + (ymdata.header.loopFrame / patternSize) * 2 * numRegs;

            // Total number of pointers, including final sequence
            uint16_t seqTotalSize =
                finalBuffers.sequenced.size() * (1 + (finalBuffers.sequenced[0].size() >> 1));

            ayt_header[0] = (2 << 4); // Version 2.0
            //  uint16_t Ayt_ActiveRegs     ; active reg (bit 2:reg 13...bit 15:reg 0), encoded in
            //  Big endian!
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

            // Nb of pattern pointer (NbSeq x NbReg)
            ayt_header[10] = seqTotalSize & 0xff;
            ayt_header[11] = (seqTotalSize >> 8) & 0xff;

            // Platform/Freq (default 0 = cpc+50hz)
            ayt_header[12] = options.targetId | (options.targetClockId << 5);
            ayt_header[13] = 0; // Reserved

            // Reserve space
            ayt_file.resize(ayt_header.size() + interleavedData.size() +
                            finalBuffers.optimizedOverlap.optimized_heap.size());

            copy(ayt_header.begin(), ayt_header.end(), ayt_file.begin());

            // Append patterns
            copy(finalBuffers.optimizedOverlap.optimized_heap.begin(),
                 finalBuffers.optimizedOverlap.optimized_heap.end(),
                 ayt_file.begin() + ayt_header.size());

            // Append interleaved sequences
            copy(interleavedData.begin(), interleavedData.end(),
                 ayt_file.begin() + offset_sequences);

            if (!isDividingEvenly) {
                if (options.addFinalSequence) {

                    // TODO: Adding Final sequence to add to interleaved Data
                    // If dividing evenly, and if final sequence was not inserted, then we must add a final sequence
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
            }
        }

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

            if (verbosity > 0) {
                cout << " Total File size: " << ayt_file.size() << endl;
                cout << "AYT file \"" << baseName.string() << ".ayt\" saved." << endl;
            }

            // CSV stats
          
            if (options.exportCsv == true) {
                cout << "Export csv" << endl;

                cout << baseName << ";" << ymdata.header.masterClock << ";"
                     << ymdata.header.frequency << ";" << ymdata.header.frameCount << ";"
                     << ((double)ymdata.header.frameCount) / ymdata.header.frequency << ";"
                     << ymdata.header.loopFrame << ";" << finalBuffers.sequenced.size() << ";"
                     << hex << real_changeMask /*converter.activeRegs*/ << dec << ";" << patternSize
                     << ";" << isDividingEvenly << ";" << isLoopingEvenly
                     << ";" //<< orig_patterns_size << ";"
                     << finalBuffers.optimizedOverlap.optimized_heap.size() << ";"
                     << interleavedData.size() << ";" << ayt_file.size() << ";" << endl;
            }
        } catch (const exception& e) {
            cerr << "Exception: " << e.what() << endl;
            return 1;
        }
    }

    return 0;
}
#endif
