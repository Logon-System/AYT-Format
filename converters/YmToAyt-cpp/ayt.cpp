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

void print_pattern(const ByteBlock& pattern, bool nl = true) {
    for (size_t i = 0; i < pattern.size(); ++i) {
        std::cout << hex << std::setw(2) << std::setfill('0') << (int)pattern[i];
    }
    if (nl)
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
 * Vérifie si le volume du canal est à zéro (b0=b1=b2=b3=0) en mode manuel (M=b4=0).
 */
inline bool is_volume_manual_zero(uint8_t volume_val) {
    return ((volume_val & 0b00011111) == 0);
    /*
    bool is_manual = (volume_val & 0b00010000) == 0;
    uint8_t volume_level = volume_val & 0b00001111;
    bool is_zero = (volume_level == 0);
    return is_manual && is_zero;
    */
}

/**
* Calcule le masque binaire (0x00 = significatif, 0x01 = masqué) et la signification (nb d'octets
non masqués).
* en foncton de la valeur des autres registres
* Gère les règles étendues pour R6, R8, R9, R10, R11, R12 et R13.
// FIXME: pat_brut correspond a &rawData[i][start]
*/

tuple<ByteBlock, int>
calculate_masked_pattern_and_significance(size_t r, size_t start, int patSize,
                                          // const ByteBlock& pat_brut,
                                          const array<ByteBlock, 16>& rawData) {

    const ByteBlock& R7_src = rawData[7];
    size_t lastFrame = R7_src.size() - 1;

    int significance = 0;
    ByteBlock pattern_mask;
    pattern_mask.reserve(patSize);

    for (int frame = 0; frame < patSize; ++frame) {
        uint8_t mask = 0xFF;
        size_t offset = start + frame;

        bool is_valid_frame = (offset <= lastFrame);
        if (!is_valid_frame) {
            // Hors limites des données, on considère masquable.
            pattern_mask.push_back(0x00);
            continue;
        }

        uint8_t rvalue = rawData[r][offset];

        uint8_t r7_val = R7_src[offset];
        int channel_idx = get_channel_index(r);
        // Bit de coupure de tonalité du canal (R7 bits 0-2)
        uint8_t tone_silence_mask_bit = (channel_idx != -1) ? (1 << channel_idx) : 0;
        //uint8_t noise_silence_mask_bit = (channel_idx != -1) ? (0b1000 << channel_idx) : 0;

        // Condition de silence (canal coupé dans R7)
        bool tone_channel_is_cut =
            (tone_silence_mask_bit != 0 && (r7_val & tone_silence_mask_bit) != 0);
        //bool noise_channel_is_cut =
        //   (noise_silence_mask_bit != 0 && (r7_val & noise_silence_mask_bit) != 0);

        // Is envelope used?
        bool volume_envelope_used =
            ((rawData[8][offset] | rawData[9][offset] | rawData[10][offset]) & (1 << 4)) != 0;
        bool all_volumes_cut =
            ((rawData[8][offset] | rawData[9][offset] | rawData[10][offset]) & 0b11111) == 0;
        bool all_noise_cut = (rawData[7][offset] & 0b111000) == 0b111000;

        // --- Règle 1: R0-R5 (Tone Freq) ---
        if (r <= 5) {
            // Si R1,R3,R5=> on masque les 4MSB
            if (r & 1)
                mask &= 0x0F;

            // 1a. Masquage si le canal Tonalité est coupé par R7
            // Désactiver via R7 ne coupe pas un channel, il faut passer par les volumes, mais R0-R5
            // sont ignorés
            if (tone_channel_is_cut) {
                mask = 0x00;
            }

            // 1b. Masquage si le volume est à Zéro ET  Manuel (R8 ou R9 ou R10)
            int vol_reg_idx = get_volume_reg_index(r);
            if (is_volume_manual_zero(rawData[vol_reg_idx][offset])) {
                mask = 0x00;
            }
        }

        // --- Règle 2: R6 (Noise Period) ---
        if (r == 6) {
            mask &= 0b00011111;
            // Masquer si le bruit est coupé sur TOUS les canaux (R7 bits 3, 4, 5) ou si le volume
            // des 3 channels a 0
            if (all_volumes_cut)
                mask = 0x00;
            // L'opérateur de mixage des bruits est le 0x38 (binaire 00111000)
            // La encore,  en principe désactiver via R7 ne coupe pas un channel, il faut passer par
            // les volumes, ce registre peut il etre ignoré?

            if (all_noise_cut) { // Si tous les bits 3,4,5 de R7 sont à 1 -> bruit coupé partout
                mask = 0x00;
            }
        }

        if (r == 7) {

            mask &= 0b00111111;
            // Si les volumes sont a 0, les bits de R7 peuvent ils etre ignoré?
            // Mais ca ne devraut pas etre appliqué a la derniere frame, car on attend 3f pour
            // couper
            // if (offset<lastFrame) {
            if ((rawData[8][offset] & 0b11111) == 0)
                mask &= 0b110110;
            if ((rawData[9][offset] & 0b11111) == 0)
                mask &= 0b101101;
            if ((rawData[10][offset] & 0b11111) == 0)
                mask &= 0b011011;
            //}
        }

        // --- Règle 3: R8, R9, R10 (Volume) ---
        if (r >= 8 && r <= 10) {
            // 3 MSB non significatifs
            mask &= 0b00011111;

            // 3a. Masquage si le canal Tonalité est coupé par R7
            // La encore,  en principe désactiver via R7 ne coupe pas un channel, mais la valeur de
            // R8-R10 n'est pas significative
            // /!\ Mais la valeur 0 peut etre significative, donc peut etre rendre optionel ce
            // masque si b0=b1=b2=b3=0
            // if (tone_channel_is_cut && noise_channel_is_cut) {
            //    mask =0x00;
            //}

            // 3b. Si M=1, alors les 3 bits pour le volume peuvent etre ignorés
            if (rvalue & (1 << 4))
                mask &= 0b00010000;
        }

        // --- Règle 4: R11, R12 (Envelope Period) ---
        if (r == 11 || r == 12) {
            // Masquer si l'enveloppe de volume n'est utilisée par AUCUN canal
            if (!volume_envelope_used) {
                mask = 0x00;
            }
        }

        // --- Règle 5: R13 (Envelope Shape) ---
        if (r == 13) {
            // Les 4MSBs ne sont pas significatifs, sauf si la valeur est 0xFF
            // Attention aussi à la valeur de termnaison (BF)
            if (rvalue != 0xFF && rvalue != final_sequence_values[2])
                mask &= 0x0F;

            /*
            // Masquer si l'enveloppe de volume n'est utilisée par AUCUN canal
            // Mais il faut qu'au moment ou l'enveloppe est utilisé, une valeur correcte soit
            rétablie (on pourrait avoir FF, impliquant que la bonne valeur avait été placée)
            // Il faut donc propager la valeur, peut etre avec un pretraitement sur tout la track,
            pas que la pattern
            // Ca risque d'alterer le son

            if (!volume_envelope_used) {
                 mask =0x00;
             }

             // Mais on peut aussi considérer que si il est masqué, la valeur doit être forcée à
            0xFF if (mask) { rawData[13]frame = 0xFF; // Fixer la valeur 'Envelope Off' mask =
            false;    // Le masque devient 0x00 pour ce byte (significatif)
             }

             */
        }
        // Enregistrement du masque et de la signification
        pattern_mask.push_back(mask); // Masqué (valeur ignorée)

        for (int i = 1; i < 256; i <<= 1) {
            if (mask & i)
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
    for (size_t i = 0; i < new_pattern.size(); ++i) {
        if ((ref_pattern[i] & new_pattern_mask[i]) != (new_pattern[i] & new_pattern_mask[i])) {
            return false;
        }
    }
    return true;
}

/**
 */
void printMasks(const array<ByteBlock, 16>& rawData) {

    for (size_t f = 0; f < rawData[0].size(); f++) {
        for (int r = 0; r < 14; r++) {
            std::cout << hex << setw(2) << (int)rawData[r][f] << " ";
        }

        std::cout << " : ";
        int s;
        ByteBlock m;

        for (int r = 0; r < 14; r++) {
            tie(m, s) = calculate_masked_pattern_and_significance(r, f, 1, rawData);
            std::cout << hex << setw(2) << (int)m[0] << " " << s << " ";
        }
        std::cout << endl;
    }
}

/**
 * @brief Normalise tous les patterns dans la liste en appliquant le masque.
 * * Remplace dans le ByteBlock de données (ici 'pat_normical') les bits non significatifs
 * (où le bit correspondant dans 'mask' est à 1) par des zéros.
 * * @param all_patterns La liste de tous les patterns collectés.
 * @param reset_masks_to_00 Si vrai, tous les masques (mask) seront forcés à 0x00 après la normalisation.
 */
void replace_masked_bits_by_zeros(std::vector<PatternInstance>& all_patterns, bool reset_masks_to_00 = false) {
    for (auto& instance : all_patterns) {
        // Sanity check
        if (instance.pat_brut.size() != instance.mask.size()) {
            // Error
            continue; 
        }

        // --- Étape 1: normalization by applying the mask to data, replacing ignored bits by zeroes ---
        for (size_t i = 0; i < instance.pat_brut.size(); ++i) {
            instance.pat_brut[i] &= instance.mask[i];
        }

        // --- Step 2 (optional): disable masking for deduplication 
        if (reset_masks_to_00) {
            // Set all masks to 0, so they will be ignored
            for (size_t i = 0; i < instance.mask.size(); ++i) {
                instance.mask[i] = 0xFF;
            }
        }
    }
}


/**
 * Recherche des buffers dupliqués
 */
ResultSequences buildBuffers(const array<ByteBlock, 16>& rawData, uint16_t activeRegs, int patSize,
                             const Options &options) {

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

            if (options.enableMaskPatterns || options.enableNormPatterns) {
                tie(pat_masque, significance) =
                    calculate_masked_pattern_and_significance(i, start, patSize, rawData);
            } else {
            }


            all_patterns.push_back({
                (int)i, start, move(pat_brut), move(pat_masque), significance,
                (int)blk // Position dans la séquence (pour la reconstruction)
            });
        }
    }

    if (options.enableNormPatterns) 
        replace_masked_bits_by_zeros(all_patterns, !options.enableMaskPatterns);


    // --- PHASE 1b: Tri des Patterns par Signification (du plus significatif au moins) ---
    // Only necessary when using masks for dedupliation process
    if (options.sortPatterns || options.enableMaskPatterns)  {

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
    }

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
        if (!found && options.enableMaskPatterns) {
            if (p_inst.significance >= 0) {

                // Parcourir tous les patterns uniques déjà enregistrés pour vérifier la
                // compatibilité
                for (const auto& pair : uniquePatternsMap) {
                    int unique_idx = pair.first;                  // Index dans l'ordre trié
                    const ByteBlock& p_unique_brut = pair.second; // block de reference

                    // On ne eut pas calculer le masque a la volée, car il dépend des autres
                    // patterns qui avaient lieu en meme temps
                    if (are_patterns_compatible(p_unique_brut, p_inst.pat_brut, p_inst.mask)) {
                        // On a trouvé un pattern compatible.
                        // On pourrait faire l'inverse, a savoir chercher le dernier, autrement dit
                        // le plus "masqué", donc le plus proche, strictement
                        idx = unique_idx;
                        found = true;
                        // std::cout << "Found a partial match " <<idx << endl;
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

        // std::cout << "Storing R" <<p_inst.reg_idx << ":" << p_inst.sequence_pos<<  "=" << idx <<
        // endl;
        temp_sequence_indices.at(p_inst.reg_idx).at(p_inst.sequence_pos) = idx;
    }

    // Reconstruire les sequenceBuffers finaux à partir de temp_sequence_indices
    //
    vector<ByteBlock> sequenceBuffers(14);
    for (const auto& pair : temp_sequence_indices) {
        int reg_idx = pair.first;

        const auto& indices = pair.second;
        ByteBlock& seq = sequenceBuffers[reg_idx];

        // size_t expected_num_indices = indices.size();
        //  Vérification de la complétude
        // size_t non_affected_count = 0;

        for (int idx : indices) {
            // Stockage de l'Index (16 bits, Little-Endian)
            seq.push_back(uint8_t(idx & 0xFF));
            seq.push_back(uint8_t((idx >> 8) & 0xFF));
        }
    }

    if (options.optimizationLevel != 0) {
        // --- Phase 2: Overlap optimization (greedy)
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
