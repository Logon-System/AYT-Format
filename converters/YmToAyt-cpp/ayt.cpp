#include "ayt.h"
#include "optimizers.h"
#include "platforms.h"
#include <algorithm>
#include <map>
#include <numeric>

using namespace std;


// Definition of the structure to store temporary patterns and their metadata
struct PatternInstance {
    // Register index (0-13)
    int reg_idx;
    // Starting position in the source register data
    size_t block_start;
    // Data pattern (initially contains the raw pattern)
    ByteBlock pat_brut;
    // Patter, Mask
    // Mask bit = 0: Significant Bit (to be kept)
    // Mask bit = 1: Non-Significant Bit (to be forced to zero)
    ByteBlock mask;
    // Significance score (number of unmasked bits) used for ordering the deduplication process
    int significance;
    // Position of this block in the final register sequence
    int sequence_pos;
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

// Returns the volume register index associated with a tone channel (R0-R6)
int get_volume_reg_index(size_t reg_idx) {
    if (reg_idx == 0 || reg_idx == 1)
        return 8;
    if (reg_idx == 2 || reg_idx == 3)
        return 9;
    if (reg_idx == 4 || reg_idx == 5)
        return 10;
    return -1;
}

// Returns the channel index (0=A, 1=B, 2=C) for R0-R5, R8-R10
int get_channel_index(size_t reg_idx) {
    if (reg_idx == 0 || reg_idx == 1 || reg_idx == 8)
        return 0; // A
    if (reg_idx == 2 || reg_idx == 3 || reg_idx == 9)
        return 1; // B
    if (reg_idx == 4 || reg_idx == 5 || reg_idx == 10)
        return 2; // C
    return -1;
}

// Checks if volume is manually set to zero (bit 4 is 0, bits 0-3 are 0)
inline bool is_volume_manual_zero(uint8_t volume_val) {
    return ((volume_val & 0b00011111) == 0);
}

/**
 * Calculates the bitwise mask (0x00-0xFF) and the significance score (number of unmasked bits).
 * Applies CANONICALIZATION to the raw pattern: non-significant bits are forced to zero.
 *
 * @param r Register Index (0-13)
 * @param start Start offset in the raw data
 * @param patSize Pattern size
 * @param rawData All raw register data
 * @return Tuple containing the Canonicalized Pattern, the Mask, and the Significance Score.
 */
tuple<ByteBlock, int>
calculate_masked_pattern_and_significance(size_t r, size_t start, int patSize,
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
            // If out of bounds, mask is 0x00 and canonicalized pattern is 0x00
            pattern_mask.push_back(0x00);
            continue;
        }

        uint8_t rvalue = rawData[r][offset];

        uint8_t r7_val = R7_src[offset];
        int channel_idx = get_channel_index(r);
        // Tone channel cut bit (R7 bits 0-2)
        uint8_t tone_silence_mask_bit = (channel_idx != -1) ? (1 << channel_idx) : 0;

        // Silence condition (Tone channel cut in R7: bit=1)
        bool tone_channel_is_cut =
            (tone_silence_mask_bit != 0 && (r7_val & tone_silence_mask_bit) != 0);
 
        // Is the Volume Envelope used on at least one channel?
        bool volume_envelope_used =
            ((rawData[8][offset] | rawData[9][offset] | rawData[10][offset]) & (1 << 4)) != 0;
        bool all_volumes_cut =
            ((rawData[8][offset] | rawData[9][offset] | rawData[10][offset]) & 0b11111) == 0;
        bool all_noise_cut = (rawData[7][offset] & 0b111000) == 0b111000;

        // --- Rule 1: R0-R5 (Tone Freq) ---
        if (r <= 5) {
            // Si R1,R3,R5=> on masque les 4MSB
            if (r & 1)
                mask &= 0x0F;

            // 1.a If tone channel is cut by R7 OR volume is manually set to Zero
            if (tone_channel_is_cut) {
                mask = 0x00;
            }

            // 1b. Masquage si le volume est à Zéro ET  Manuel (R8 ou R9 ou R10)
            int vol_reg_idx = get_volume_reg_index(r);
            if (is_volume_manual_zero(rawData[vol_reg_idx][offset])) {
                mask = 0x00;
            }
        }

        // --- Rule 2: R6 (Noise Period) ---
        if (r == 6) {
            // Mask the 3 unused MSBs
            mask &= 0b00011111;
            // Mask if Voume  is cut on ALL channels (R7 bits 0,1,2 are 1)
            // 0b00111000 = 0x38 (bits 3, 4, 5)
            if (all_volumes_cut)
                mask = 0x00;
        
            // Mask if Noise is cut on ALL channels (R7 bits 3, 4, 5 are 1)
            if (all_noise_cut) { // Si tous les bits 3,4,5 de R7 sont à 1 -> bruit coupé partout
                mask = 0x00;
            }
        }

        // --- Rule 3: R7 (Mixer) ---
        if (r == 7) {
            // Mask the 2 unused MSBs (bits 6, 7)
            mask &= 0b00111111;

            // if volume is 0, we can cut the corresponding channel
            if ((rawData[8][offset] & 0b11111) == 0)
                mask &= 0b110110;
            if ((rawData[9][offset] & 0b11111) == 0)
                mask &= 0b101101;
            if ((rawData[10][offset] & 0b11111) == 0)
                mask &= 0b011011;
        }

        // --- Rule 4: R8, R9, R10 (Volume) ---
        if (r >= 8 && r <= 10) {
            // Mask the 3 non-significant MSBs (bits 5, 6, 7)
            mask &= 0b00011111;

            // 4a. Masking if the Tone Channel is cut by R7
            // This rule is not valid
            //if (tone_channel_is_cut) {
            //    mask = 0x00;
            //}
            
            // 4b. If M=1 (Envelope Mode), the 4 volume bits (0-3) can be ignored
            // Only bit 4 (M) must remain significant

            if (rvalue & (1 << 4))
                mask &= 0b00010000;
        }

        // --- Rule 5: R11, R12 (Envelope Period) ---
        if (r == 11 || r == 12) {
            // Mask if the Volume Envelope is not used by ANY channel
            if (!volume_envelope_used) {
                mask = 0x00;
            }
        }

        // --- Rule 6: R13 (Envelope Shape) ---
        if (r == 13) {
            // The 4 MSBs are non-significant, unless the value is 0xFF or 0xBF (Envelope End/Hold Pattern)
            // BF is a special value for AYT
            if (rvalue != 0xFF && rvalue != final_sequence_values[2])
                mask &= 0x0F;

            // If the envelope is inactive, the R13 value is non-significant
            //if (!volume_envelope_used) {
            //    mask = 0x00;
            //}
            // But we must handle #FF values - we need to restore a correct value once envelope is used again
            // So we disable it for the moment
            // And We could also replace it with 0xFFs
    
        }

        // Store the mask
        pattern_mask.push_back(mask); 
        
        // Calculate significance (number of unmasked bits)
        // Significance is calculated by the number of set bits (1s) in the mask
        for (int i = 1; i < 256; i <<= 1) {
            if (mask & i)
                significance++;
        }
    }

    return {pattern_mask, significance};
}

/**
 * @brief Checks if two patterns are compatible via bitwise masking.
 * @param ref_pattern_canonical : data pattern already in the Heap (the reference).
 * @param new_pattern_canonical : candidate data pattern (the new pattern).
 * @param new_pattern_mask : binary Mask (0x00-0xFF) of the candidate 
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
 * @brief Canonicalizes all patterns in the list by applying the mask.
 * Replaces non-significant bits (where the corresponding mask bit is 0) with zeros
 * in the data ByteBlock ('pat_canonical').
 * @param all_patterns The list of all collected patterns.
 * @param reset_masks_to_FF If true, all masks (mask) will be forced to 0xFF after normalization.
 */
void replace_masked_bits_by_zeros(std::vector<PatternInstance>& all_patterns, bool reset_masks_to_00 = false) {
    for (auto& instance : all_patterns) {
        // Pattern size must equal mask size
        if (instance.pat_brut.size() != instance.mask.size()) {
            // Error
            continue;
        }

        // --- Step 1: normalization by masking, replacing ignored bits by zeroes ---
        for (size_t i = 0; i < instance.pat_brut.size(); ++i) {
            instance.pat_brut[i] &= instance.mask[i];
        }

        // --- Step 2: Optional Mask Reset to 0x00 ---
        if (reset_masks_to_00) {
            // Set all masks to 0, so they will be ignored
            for (size_t i = 0; i < instance.mask.size(); ++i) {
                instance.mask[i] = 0xFF;
            }
        }
    }
}


/**
 * Deduplication algorithm
 */
ResultSequences buildBuffers(const array<ByteBlock, 16>& rawData, uint16_t activeRegs, int patSize,
                             const Options &options) {

    // --- PHASE 1a: Create all patterns, by splittng raw data per register into chunks  ---
    vector<PatternInstance> all_patterns;

    for (size_t i = 0; i < 14; ++i) {
        if (!(activeRegs & (1 << i))) {
            continue;
        }

        const auto& src = rawData[i];
        const size_t N = src.size();
        // number of patterns
        const size_t num_blocks = (N == 0) ? 0 : ((N + size_t(patSize) - 1) / size_t(patSize));

        for (size_t blk = 0; blk < num_blocks; ++blk) {
            size_t start = blk * size_t(patSize);
            // For the last bloc, if nframes/patSize is odd, there are remaining bytes
            size_t remain = (start < N) ? (N - start) : 0;

            // Creat raw pattern (with padding)
            ByteBlock pat_brut(patSize);
            size_t take = 0;
            // Data remaining? 
            if (remain > 0) {
                take = min(remain, size_t(patSize));
                memcpy(pat_brut.data(), &src[start], take);
                uint8_t pad = src[start + take - 1];
                if (take < (size_t)patSize)
                    memset(pat_brut.data() + take, pad, size_t(patSize) - take);
            } else {
                memset(pat_brut.data(), 0, size_t(patSize));
            }

            // Computes Pattern mask (if required)
            ByteBlock pat_masque;
            int significance = -1;
            if (options.enableMaskPatterns || options.enableNormPatterns) {
                tie(pat_masque, significance) =
                    calculate_masked_pattern_and_significance(i, start, patSize, rawData);
            } 
            
            all_patterns.push_back({
                (int)i, start, move(pat_brut), move(pat_masque), significance,
                (int)blk // Position dans la séquence (pour la reconstruction)
            });
        }
    }

    if (options.enableNormPatterns)
        replace_masked_bits_by_zeros(all_patterns, !options.enableMaskPatterns);


    // --- PHASE 1b: Sort patterns by signifiance 
    //  Only necessary when using masks for dedupliation process
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

    // --- PHASE 1c: dedupllicate ---

    // Pour stocker l'index optimisé
    map<int, vector<int>> temp_sequence_indices;
    for (size_t i = 0; i < 14; ++i) {
        if (activeRegs & (1 << i)) {
            temp_sequence_indices[(int)i].resize((rawData[i].size() + patSize - 1) / patSize, -1);
        }
    }

    map<int, ByteBlock> uniquePatternsMap;

    // Map: Hash Brut -> Unique Pattern Index 
    // Used for strict deduplication
    unordered_map<uint32_t, int> uniqueHashesBrut;
    
    int next_pattern_idx = 0;

    // Evaluate all patterns 
    for (const auto& p_inst : all_patterns) {

        int idx = -1;
        bool found = false;

        uint32_t h_brut = fnv1a32(p_inst.pat_brut);

        // 1. Strict comparison (using Hash)
        auto it_brut = uniqueHashesBrut.find(h_brut);
        if (it_brut != uniqueHashesBrut.end()) {
            // Vérification de l'identité des patterns bruts
            if (uniquePatternsMap.at(it_brut->second) == p_inst.pat_brut) {
                idx = it_brut->second; // Pattern index
                found = true;
            }
        }

        // 2. Deduplication using masking 
        if (!found && options.enableMaskPatterns) {
            if (p_inst.significance >= 0) {
                // Check compatibiity with all pattern already stored in heap
                for (const auto& pair : uniquePatternsMap) {
                    int unique_idx = pair.first;                  // Index dans l'ordre trié
                    const ByteBlock& p_unique_brut = pair.second; // block de reference

                    if (are_patterns_compatible(p_unique_brut, p_inst.pat_brut, p_inst.mask)) {
                        // Found a (partially) matching pattern
                        idx = unique_idx;
                        found = true;
                        
                        break; // Le premier compatible trouvé est le bon.
                    }
                }
            }
        }

        // No pattern matching, it's a new pattern
        if (!found) {
            idx = next_pattern_idx++;
            // Stores hash and pattern
            uniqueHashesBrut[h_brut] = idx;
            uniquePatternsMap[idx] = p_inst.pat_brut;
        }

        // Enregistrer l'index dédupliqué dans le tableau temporaire de séquences
        if (idx > 0xFFFF)
            throw runtime_error("Pattern index exceeds 16-bit");

        temp_sequence_indices.at(p_inst.reg_idx).at(p_inst.sequence_pos) = idx;
    }

    // Rebuilds sequence pointers from optimized indices
    vector<ByteBlock> sequenceBuffers(14);
    for (const auto& pair : temp_sequence_indices) {
        int reg_idx = pair.first;

        const auto& indices = pair.second;
        ByteBlock& seq = sequenceBuffers[reg_idx];

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

// Rebuids heap and final sequence pointers
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
