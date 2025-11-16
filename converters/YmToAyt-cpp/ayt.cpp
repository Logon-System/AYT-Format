#include "ayt.h"
#include "platforms.h"


using namespace std;

// Computes Hash FNV-1a for a byte sequence
static uint32_t fnv1a32(const ByteBlock& data) {
    uint32_t h = 2166136261u;
    for (uint8_t byte : data) {
        h ^= byte;
        h *= 16777619u;
    }
    return h;
}


// Récupère l'index du Registre de Volume associé au canal (R8, R9, R10).
// Renvoie -1 si le registre n'est pas associé à un canal ou une fréquence.
int get_volume_reg_index(size_t reg_idx) {
    if (reg_idx == 0 || reg_idx == 1) return 8; // Fréquence A -> Volume R8
    if (reg_idx == 2 || reg_idx == 3) return 9; // Fréquence B -> Volume R9
    if (reg_idx == 4 || reg_idx == 5) return 10; // Fréquence C -> Volume R10
    if (reg_idx == 6) return 8; // Bruit (partage les volumes, arbitrairement R8 pour l'exemple)
    return -1; 
}


/**
 * Vérifie si le volume du canal est à zéro en mode manuel.
 * @param volume_val La valeur du Registre de Volume (R8, R9 ou R10)
 * @return true si le volume est 0 (bits 3-0) ET en mode manuel (bit 4 = 0).
 */
bool is_volume_manual_zero(uint8_t volume_val) {
    // Bit 4 (M) à 0: Mode manuel (non enveloppé)
    bool is_manual = (volume_val & 0b00010000) == 0;
    
    // Bits 3-0 à 0: Volume = 0
    uint8_t volume_level = volume_val & 0b00001111;
    bool is_zero = (volume_level == 0);

    return is_manual && is_zero;
}



// Récupère l'index du Registre (0-14) correspondant à l'index de canal (0=A, 1=B, 2=C)
// Les indices de canal (0, 1, 2) correspondent aux bits 0, 1, 2 du Registre 7.
int get_channel_index(size_t reg_idx) {
    if (reg_idx == 0 || reg_idx == 1 || reg_idx == 8) return 0;  // Canal A: R0, R1 (Fréq), R8 (Vol)
    if (reg_idx == 2 || reg_idx == 3 || reg_idx == 9) return 1;  // Canal B: R2, R3 (Fréq), R9 (Vol)
    if (reg_idx == 4 || reg_idx == 5 || reg_idx == 10) return 2; // Canal C: R4, R5 (Fréq), R10 (Vol)
    // R6 (Bruit), R7 (Mixer), R11/12 (Env Période), R13 (Env Forme), R14/15 (E/S) n'ont pas de masque simple
    return -1; 
}



/**
 * Recherche ds buffers dupliqués
 */
ResultSequences buildBuffers(const array<ByteBlock, 16>& rawData, uint16_t activeRegs,
                                    int patSize, int optimizationLevel, bool useSilenceMasking) {

    // --- Phase 1: Découpage en blocs et déduplication initiale ---
    OptimizedResult no_opt_result;

    vector<ByteBlock> sequenceBuffers;
    map<int, ByteBlock>
        uniquePatternsMap; 
    unordered_map<uint32_t, int>
        uniqueHashes; 

    // patternSignificance est désormais locale et N'EST PAS passée au glouton
    map<int, int> patternSignificance; 
    map<int, int> patternFrequency; 

    int next_pattern_idx = 0; 

    // Les buffers de R7, R8, R9, R10 sont nécessaires pour le masquage
    const ByteBlock& R7_src = rawData[7]; 
    const ByteBlock& R8_src = rawData[8]; 
    const ByteBlock& R9_src = rawData[9]; 
    const ByteBlock& R10_src = rawData[10]; 

    
    for (size_t i = 0; i <= 14; ++i) {
        if (!(activeRegs & (1 << i))) {
            continue;
        }

        const auto& src = rawData[i]; 
        const size_t N = src.size();
        const size_t num_blocks = (N == 0) ? 0 : ((N + size_t(patSize) - 1) / size_t(patSize));

        ByteBlock seq;
        seq.reserve(num_blocks * 2);

        // Indices pour le masquage
        int channel_idx = useSilenceMasking ? get_channel_index(i) : -1;
        int volume_reg_idx = useSilenceMasking ? get_volume_reg_index(i) : -1;
        
        for (size_t blk = 0; blk < num_blocks; ++blk) {
            size_t start = blk * size_t(patSize);
            size_t remain = (start < N) ? (N - start) : 0;
            size_t take = min(remain, size_t(patSize));

            // Création du Pattern BRUT (avec padding)
            ByteBlock pat(patSize);
            if (take) {
                memcpy(pat.data(), &src[start], take);
                uint8_t pad = src[start + take - 1];
                if (take < (size_t)patSize)
                    memset(pat.data() + take, pad, size_t(patSize) - take);
            } else {
                memset(pat.data(), 0, size_t(patSize));
            }

            // --- Logique d'Application du Masque ---
            ByteBlock pattern_to_hash = pat; 
            int non_masked_bytes = 0; // Compteur pour la métrique de contrainte
            
            if (useSilenceMasking && (channel_idx != -1 || volume_reg_idx != -1)) {
                // On crée le pattern filtré
                pattern_to_hash.clear();
                pattern_to_hash.reserve(patSize);
                
                // Pointeur vers le bon buffer de volume
                const ByteBlock* R_VOL_src = nullptr;
                if (volume_reg_idx == 8) R_VOL_src = &R8_src;
                else if (volume_reg_idx == 9) R_VOL_src = &R9_src;
                else if (volume_reg_idx == 10) R_VOL_src = &R10_src;

                for (int frame = 0; frame < patSize; ++frame) {
                    bool should_mask = false;
                    
                    // Vérification des limites pour R7 et R_VOL
                    bool is_valid_frame = (start + frame < R7_src.size()); 
                    
                    if (is_valid_frame) {
                        // 1. Masquage via R7 (Canal coupé)
                        if (channel_idx != -1) {
                            // Bit 0=A, 1=B, 2=C. Bit à 1 => silencieux (mix)
                            uint8_t silence_mask_bit = (1 << channel_idx); 
                            uint8_t r7_val = R7_src[start + frame];
                            if ((r7_val & silence_mask_bit) != 0) {
                                should_mask = true;
                            }
                        }

                        // 2. Masquage via Volume Manuel Zéro
                        // S'applique aux registres de Fréquence (R0-R5) et Bruit (R6)
                        if (volume_reg_idx != -1 && R_VOL_src) {
                            // On vérifie toujours les limites sur R_VOL_src (volume)
                            if (start + frame < R_VOL_src->size()) {
                                uint8_t vol_val = R_VOL_src->at(start + frame);
                                if (is_volume_manual_zero(vol_val)) {
                                    should_mask = true;
                                }
                            }
                        }
                    } 
                    
                    if (should_mask) {
                        pattern_to_hash.push_back(0x00); 
                    } else { 
                        // Si aucune condition de masquage n'est remplie
                        pattern_to_hash.push_back(pat[frame]);
                        // Le compteur n'est incrémenté que pour les octets non masqués
                        non_masked_bytes++;
                    }
                }
            } else {
                // Si useSilenceMasking est false, tous les octets sont non-masqués
                non_masked_bytes = patSize;
            }
            
            // --- Déduplication révisée ---
            uint32_t h = fnv1a32(pattern_to_hash);
            int idx = -1;
            bool found = false;

            auto it_hash = uniqueHashes.find(h);
            
            if (it_hash != uniqueHashes.end()) {
                idx = it_hash->second;
                found = true;
            }

            if (found) {
                // IMPORTANT: Si le masquage est actif, nous devons vérifier si le pattern BRUT (pat) 
                // correspond aussi. Si deux patterns BRUTS sont différents mais ont le même 
                // pattern_to_hash (due au masquage partiel), nous DEVONS leur donner des index séparés
                // pour éviter les corruptions musicales.
                if (useSilenceMasking) {
                    const ByteBlock& existing_pat = uniquePatternsMap.at(idx);
                    if (existing_pat != pat) {
                        // Le hash (masqué) est le même, mais le pattern BRUT est différent. 
                        // C'est une collision logique due au masquage.
                        // On force la création d'un NOUVEL index pour éviter la corruption.
                        found = false; 
                    }
                }
            }

            if (!found) {
                // Nouveau pattern
                idx = next_pattern_idx++;
                // On utilise le HASH du pattern MASQUÉ pour le tracking (uniqueHashes)
                uniqueHashes[h] = idx; 
                // On stocke le pattern BRUT (pat) dans la map pour la phase 2 (overlap)
                uniquePatternsMap[idx] = move(pat); 
                // Stocker le score de contrainte pour le nouveau pattern (pour info, ou futures opts)
                patternSignificance[idx] = non_masked_bytes;
            }
            
            // MISE À JOUR DE LA FRÉQUENCE (pour info, on garde la fréquence)
            patternFrequency[idx]++;

            if (idx > 0xFFFF)
                throw runtime_error("Pattern index exceeds 16-bit");

            // Stockage de l'Index (16 bits, Little-Endian)
            seq.push_back(uint8_t(idx & 0xFF));
            seq.push_back(uint8_t((idx >> 8) & 0xFF));
        }
        sequenceBuffers.push_back(move(seq));
    }

    if (optimizationLevel != 0) {
        // --- Phase 2: Optimisation du chevauchement (Glouton) ---
        // Retour à la version simple, plus propice à une bonne solution de départ
        auto initial_result = merge_ByteBlocks_greedy(uniquePatternsMap, patSize);
        return {next_pattern_idx, move(sequenceBuffers), move(uniquePatternsMap), move(initial_result)};

    } else {
        // --- Phase 2: Niveau 1 - Déduplication SANS chevauchement ---
        int current_offset = 0;
        for (const auto& pair : uniquePatternsMap) {
            int pattern_idx = pair.first;
            const ByteBlock& pattern_data = pair.second;

            no_opt_result.optimized_heap.insert(no_opt_result.optimized_heap.end(), 
                                                pattern_data.begin(), 
                                                pattern_data.end());

            no_opt_result.optimized_pointers[pattern_idx] = current_offset;
            no_opt_result.optimized_block_order.push_back(pattern_idx);
            current_offset += patSize;
        }

        return {next_pattern_idx, move(sequenceBuffers), move(uniquePatternsMap), move(no_opt_result)};
    }
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


// Interleave sequence buffers
ByteBlock interleaveSequenceBuffers(const vector<ByteBlock>& sequenceBuffers) {

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
