#include "ayt.h"
#include "optimizers.h"
#include "platforms.h"
#include <algorithm>
#include <map>
#include <numeric> // For std::iota

using namespace std;

// Définition de la structure pour stocker les patterns temporaires et leurs métadonnées
struct PatternInstance {
    int reg_idx;        // Index du registre (0-13)
    size_t block_start; // Position de début dans le registre source
    ByteBlock pat_brut; // Le pattern de données brut
    ByteBlock mask; // Le masque du pattern (pourrait etre optimisé avec un bitfield). 1 si masqué ,
                    // 0 si non masqué
    int significance; // Score de signification (octets non masqués) sert pour ordonnancer la
                      // deduplication
    int sequence_pos; // Position de ce bloc dans la séquence finale du registre
};

// Computes Hash FNV-1a for a byte sequence
static uint32_t fnv1a32(const ByteBlock& data) {
    uint32_t h = 2166136261u;
    for (uint8_t byte : data) {
        h ^= byte;
        h *= 16777619u;
    }
    return h;
}

void print_pattern(const ByteBlock& pattern) {
        for (size_t i = 0; i < pattern.size(); ++i) {
            std::cout << hex << std::setw(2) << std::setfill('0') << (int) pattern[i] ;    
    }
    std::cout << endl;
}


// Récupère l'index du Registre de Volume associé au canal (R8, R9, R10).
int get_volume_reg_index(size_t reg_idx) {
    if (reg_idx == 0 || reg_idx == 1 || reg_idx == 6)
        return 8;
    if (reg_idx == 2 || reg_idx == 3)
        return 9;
    if (reg_idx == 4 || reg_idx == 5)
        return 10;
    return -1;
}

// Récupère l'index du Registre (0-14) correspondant à l'index de canal (0=A, 1=B, 2=C)
int get_channel_index(size_t reg_idx) {
    if (reg_idx == 0 || reg_idx == 1 || reg_idx == 8)
        return 0;
    if (reg_idx == 2 || reg_idx == 3 || reg_idx == 9)
        return 1;
    if (reg_idx == 4 || reg_idx == 5 || reg_idx == 10)
        return 2;
    return -1;
}

/**
 * Vérifie si le volume du canal est à zéro en mode manuel.
 */
bool is_volume_manual_zero(uint8_t volume_val) {
    bool is_manual = (volume_val & 0b00010000) == 0;
    uint8_t volume_level = volume_val & 0b00001111;
    bool is_zero = (volume_level == 0);
    return is_manual && is_zero;
}

/**
 * Calcule le mask pour un pattern et pour un registre donné,
 * en foncton de la valeur des autres registres 
 */
tuple<ByteBlock, int>
calculate_masked_pattern_and_significance(size_t i, size_t start, int patSize,
                                          const ByteBlock& pat_brut,
                                          const array<ByteBlock, 16>& rawData) {
    // Si nous traitons des registres critiques pour le masquage (Mix, Volumes), on suppose 100% de
    // signification s'ils sont actifs.
    if (i >=6)
        return {pat_brut, -1}; //Le 1er param devrait etre vide ou 

    // On peut vouloir masquer 8,9,10 sur le critere de R7
    // On peut vouloir masquer R13 si aucune enveloppe utilisée, mais R13 masqué: on met FF et pas n'importe quoi
    

    const ByteBlock& R7_src = rawData[7];
    const ByteBlock* R_VOL_src = nullptr;
    int volume_reg_idx = get_volume_reg_index(i);
    if (volume_reg_idx != -1)
        R_VOL_src = &rawData[volume_reg_idx];

    int significance = 0;
    int channel_idx = get_channel_index(i);
    ByteBlock pattern_mask;
    pattern_mask.reserve(patSize);

    for (int frame = 0; frame < patSize; ++frame) {
        bool should_mask = false;
        bool is_valid_frame = (start + frame < R7_src.size());

        if (is_valid_frame) {
            // 1. Masquage via R7 (Canal coupé)
            if (channel_idx != -1) {
                uint8_t silence_mask_bit = (1 << channel_idx);
                uint8_t r7_val = R7_src[start + frame];
                if ((r7_val & silence_mask_bit) != 0) {
                    should_mask = true;
                }
            }

            // 2. Masquage via Volume Manuel Zéro
            if (R_VOL_src) {
                if (start + frame < R_VOL_src->size()) {
                    uint8_t vol_val = R_VOL_src->at(start + frame);
                    if (is_volume_manual_zero(vol_val)) {
                        should_mask = true;
                    }
                }
            }
        }

        // Mais comment différencier un pattern avec 0 comme valeur attendue et 0 comme masque?
        if (should_mask) {
            pattern_mask.push_back(0x01);
        } else {
            pattern_mask.push_back(0x00);
            significance++;
        }
    }

    

    return {pattern_mask, significance};
}

/**
 * Vérifie si deux patterns sont compatibles via masquage.
 * La 1er est la pattern de reference
 */
bool are_patterns_compatible(const ByteBlock& ref_pattern, const ByteBlock& new_pattern,
                             const ByteBlock& new_pattern_mask) {
    //  if (ref_pattern.size() != new_pattern.size())
    //  if (ref_pattern.size() != new_pattern_mask.size())

    for (size_t i = 0; i < new_pattern.size(); ++i) {
        if (new_pattern_mask[i] == 0) {
            if (ref_pattern[i] != new_pattern[i]) {
                return false;
            }
        }
    }
    return true;
}


/**
 * Recherche ds buffers dupliqués
 */
ResultSequences buildBuffers(const array<ByteBlock, 16>& rawData, uint16_t activeRegs, int patSize,
                             int optimizationLevel, bool useSilenceMasking) {

    // --- PHASE 1a: Collecte de TOUTES les instances de pattern ---
    vector<PatternInstance> all_patterns;

    for (size_t i = 0; i < 14; ++i) {
        if (!(activeRegs & (1 << i))) {
            continue;
        }

        const auto& src = rawData[i];
        const size_t N = src.size();
        // On découpe rawData en blocs de patSize données: ca nous fait num_blocks blocs
        const size_t num_blocks = (N == 0) ? 0 : ((N + size_t(patSize) - 1) / size_t(patSize));

        for (size_t blk = 0; blk < num_blocks; ++blk) {
            size_t start = blk * size_t(patSize);
            // Pour e dernier bloc, il peut y avoir un reste, si la division nframes/patSize ne
            // tombe pas juste
            size_t remain = (start < N) ? (N - start) : 0;

            // Création du Pattern BRUT (avec padding)
            ByteBlock pat_brut(patSize);
            size_t take = 0;
            // Si il y a un reste
            if (remain > 0) {
                take = min(remain, size_t(patSize));
                memcpy(pat_brut.data(), &src[start], take);
                uint8_t pad = src[start + take - 1];
                if (take < (size_t)patSize)
                    memset(pat_brut.data() + take, pad, size_t(patSize) - take);
            } else {
                memset(pat_brut.data(), 0, size_t(patSize));
            }

            // Le masque du pattern
            ByteBlock pat_masque;
            int significance = -1;

            if (useSilenceMasking) {
                tie(pat_masque, significance) =
                    calculate_masked_pattern_and_significance(i, start, patSize, pat_brut, rawData);
            } else {
                // pat_masque = pat_brut;
                // significance = -1;
            }

            // On ajoute chaque pattern avec son masque (qu'on pourrait calculer a la volée, on ne
            // s'en sert qu'une fois)
            all_patterns.push_back({
                (int)i, start, move(pat_brut), move(pat_masque), significance,
                (int)blk // Position dans la séquence (pour la reconstruction)
            });
        }
    }

    // --- PHASE 1b: Tri des Patterns par Signification (du plus significatif au moins) ---
    // Le tri n'est utile que si on utliser l'optimisation par masquage
    
    sort(all_patterns.begin(), all_patterns.end(),
         [](const PatternInstance& a, const PatternInstance& b) {
             // Priorité 1: Signification (décroissante: plus grande signification en premier)
             if (a.significance != b.significance) {
                 return a.significance > b.significance;
             }
             // Priorité 2: Stabilité (par index de registre) : quel interet? on pourrait commencer
             // par R13, R7, ...
             return a.reg_idx < b.reg_idx;
         });
    
    
    // --- PHASE 1c: Déduplication basée sur l'ordre trié et la compatibilité ---

    // Pour stocker l'index optimisé
    map<int, vector<int>> temp_sequence_indices;
    for (size_t i = 0; i < 14; ++i) {
        if (activeRegs & (1 << i)) {
            temp_sequence_indices[(int)i].resize((rawData[i].size() + patSize - 1) / patSize, -1);
        }
    }

    map<int, ByteBlock> uniquePatternsMap;

    // Map: Hash Brut -> Index de Pattern UNIQUE déjà enregistré (pour déduplication stricte)
    unordered_map<uint32_t, int> uniqueHashesBrut;
    // Map pour trouver l'index unique à partir d'un hash masqué unique,
    // pour éviter de boucler sur TOUS les patterns uniques pour CHAQUE nouveau pattern.
    // unordered_map<uint32_t, int> uniqueHashesMasqueToUniqueIndex;

    int next_pattern_idx = 0;

    // Pour chaque instance de pattern, dans l'ordre de signification décroissante
    for (const auto& p_inst : all_patterns) {
        // On teste la duplication stricte:




        int idx = -1;
        bool found = false;

        uint32_t h_brut = fnv1a32(p_inst.pat_brut);

        // 1. Vérification stricte (Hash Brut)
        auto it_brut = uniqueHashesBrut.find(h_brut);
        if (it_brut != uniqueHashesBrut.end()) {
            // Vérification de l'identité des patterns bruts
            if (uniquePatternsMap.at(it_brut->second) == p_inst.pat_brut) {
                idx = it_brut->second; // Index des patterns triés
                found = true;
            }
        }

        // 2. Vérification permissive (Compatibilité par masquage)
        if (!found && useSilenceMasking) {
            if (p_inst.significance>=0) {


            //std::cout << "Sign = " << p_inst.significance<<endl;

            // Si égal a 0, c'est qu'on peut accepter directement

            //print_pattern(p_inst.pat_brut);
            //print_pattern(p_inst.mask);

            // Parcourir tous les patterns uniques déjà enregistrés pour vérifier la compatibilité 
            for (const auto& pair : uniquePatternsMap) {
                int unique_idx = pair.first;                  // Index dans l'ordre trié
                const ByteBlock& p_unique_brut = pair.second; // block de reference

                // On ne eut pas calculer le masque a la volée, car il dépend des autres patterns qui avaient lieu en meme temps 
                if (are_patterns_compatible(p_unique_brut, p_inst.pat_brut, p_inst.mask)) {
                    // On a trouvé un pattern compatible.
                    // On pourrait faire l'inverse, a savoir chercher le dernier, autrement dit le
                    // plus "masqué", donc le plus proche, strictement
                    idx = unique_idx;
                    found = true;
                    //std::cout << "Found a partial match " <<idx << endl;
                  //  print_pattern(p_inst.pat_brut);
                  //  print_pattern(p_inst.mask);
                  //  print_pattern(p_unique_brut);
                    

                    break; // Le premier compatible trouvé est le bon.
                }
            }

            }
        }

        // On n'a toujours pas trouvé, c'est un Nouveau Pattern
        if (!found) {
            idx = next_pattern_idx++;

            // Enregistrement des Hashes et Pattern
            uniqueHashesBrut[h_brut] = idx;
            uniquePatternsMap[idx] = p_inst.pat_brut;
        }

        // Enregistrer l'index dédupliqué dans le tableau temporaire de séquences
        if (idx > 0xFFFF)
            throw runtime_error("Pattern index exceeds 16-bit");

        //std::cout << "Storing R" <<p_inst.reg_idx << ":" << p_inst.sequence_pos<<  "=" << idx << endl;
        temp_sequence_indices.at(p_inst.reg_idx).at(p_inst.sequence_pos) = idx;
    }

    // Reconstruire les sequenceBuffers finaux à partir de temp_sequence_indices
    // 
    vector<ByteBlock> sequenceBuffers(14);
    for (const auto& pair : temp_sequence_indices) {
        int reg_idx = pair.first;

        const auto& indices = pair.second;
        ByteBlock& seq = sequenceBuffers[reg_idx];

        size_t expected_num_indices = indices.size();

        // Vérification de la complétude
        size_t non_affected_count = 0;

        for (int idx : indices) {
                // Stockage de l'Index (16 bits, Little-Endian)
                seq.push_back(uint8_t(idx & 0xFF));
                seq.push_back(uint8_t((idx >> 8) & 0xFF));
        }

    }




    if (optimizationLevel != 0) {
        // --- Phase 2: Optimisation du chevauchement (Glouton) ---
        auto initial_result = merge_ByteBlocks_greedy(uniquePatternsMap, patSize);

        return {next_pattern_idx, move(sequenceBuffers), move(uniquePatternsMap),
                move(initial_result)};

    } else {
        // --- Phase 2: Niveau 1 - Déduplication SANS chevauchement ---
        OptimizedResult no_opt_result;
        int current_offset = 0;
        for (const auto& pair : uniquePatternsMap) {
            int pattern_idx = pair.first;
            const ByteBlock& pattern_data = pair.second;

            no_opt_result.optimized_heap.insert(no_opt_result.optimized_heap.end(),
                                                pattern_data.begin(), pattern_data.end());

            no_opt_result.optimized_pointers[pattern_idx] = current_offset;
            no_opt_result.optimized_block_order.push_back(pattern_idx);
            current_offset += patSize;
        }

        return {next_pattern_idx, move(sequenceBuffers), move(uniquePatternsMap),
                move(no_opt_result)};
    }
}

void replaceByOptimizedIndex(vector<ByteBlock>& sequenceBuffers,
                             const OptimizedResult& optimized_result) {

    // --- Phase 3: Remplacement des Index ---
    for (size_t i = 0; i < sequenceBuffers.size(); ++i) {
        ByteBlock& seq = sequenceBuffers[i];
        for (size_t j = 0; j < seq.size(); j += 2) {

            // 1. Lecture de l'Ancien Index (16 bits, Little-Endian)
            int old_idx = seq[j] + (seq[j + 1] << 8);

            // 2. Récupération du Nouvel Offset (Pointeur)
            int new_offset;
            try {
                new_offset = optimized_result.optimized_pointers.at(old_idx);
            } catch (const out_of_range& e) {
                throw runtime_error("Index de pattern manquant dans les pointeurs optimisés.");
            }

            // 3. Vérification de la Contrainte 16 bits
            if (new_offset > 0xFFFF)
                throw runtime_error("Nouvel offset (pointeur) dépasse 16-bit");

            // 4. Écriture du Nouvel Offset (16 bits, Little-Endian)
            seq[j] = uint8_t(new_offset & 0xFF);
            seq[j + 1] = uint8_t((new_offset >> 8) & 0xFF);
        }
    }
}

// Interleave sequence buffers
ByteBlock interleaveSequenceBuffers(const vector<ByteBlock>& sequenceBuffers) {

    size_t numBuffers = sequenceBuffers.size();
    vector<size_t> sequenceBufferLengths;
    size_t totalsequenceBufferSize = 0;

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

// Fonction pour reconstruire le Heap et les pointeurs finaux
OptimizedResult reconstruct_result(const vector<int>& best_order,
                                   const map<int, ByteBlock>& original_blocks, int patSize) {
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
