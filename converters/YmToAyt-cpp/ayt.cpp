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
 * Recherche des buffers dupliqués
 */
ResultSequences buildBuffers(const array<ByteBlock, 16>& rawData, uint16_t activeRegs,
                                    int patSize, int optimizationLevel, bool useSilenceMasking) {

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

        const auto& src = rawData[i]; // Buffer du registre courant (R_i)
        const size_t N = src.size();
        const size_t num_blocks = (N == 0) ? 0 : ((N + size_t(patSize) - 1) / size_t(patSize));

        ByteBlock seq;
        seq.reserve(num_blocks * 2);

        // Détermination du canal affecté par R7 (seulement si useSilenceMasking est vrai)
        int channel_idx = useSilenceMasking ? get_channel_index(i) : -1;
        
        // Référence au Registre 7 pour l'état de silence (si nécessaire)
        const ByteBlock& R7_src = rawData[7]; 
        
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

            // --- Logique d'Application du Masque de Silence ---
            ByteBlock pattern_to_hash = pat; // Par défaut, on utilise le pattern brut
            
            if (useSilenceMasking && channel_idx != -1) {
                // Le registre i est lié à un canal (A, B ou C).
                
                // Masque binaire pour le Registre 7: Bit 0=A, 1=B, 2=C. Bit à 1 => silencieux (mix)
                uint8_t silence_mask_bit = (1 << channel_idx); 
                
                // On prépare le pattern filtré qui servira au hachage et à la comparaison
                pattern_to_hash.clear();
                pattern_to_hash.reserve(patSize);

                for (int frame = 0; frame < patSize; ++frame) {
                    // Vérification de la limite du buffer R7 (pour le padding)
                    if (start + frame >= R7_src.size()) {
                        // Si au-delà des données réelles, on garde la valeur padée du pattern brut
                        pattern_to_hash.push_back(pat[frame]);
                        continue;
                    }
                    
                    uint8_t r7_val = R7_src[start + frame];
                    
                    // Si le bit de silence est ACTIF dans R7 pour ce canal (r7_val & silence_mask_bit) est VRAI
                    if ((r7_val & silence_mask_bit) != 0) { // Le canal est SILENCIEUX (bit à 1)
                        // La valeur est ignorée (masquée). On met une valeur neutre (0x00) pour la déduplication.
                        pattern_to_hash.push_back(0x00); 
                    } else { // Le canal est ACTIF (bit à 0)
                        // La valeur est importante, on la garde.
                        pattern_to_hash.push_back(pat[frame]);
                    }
                }
            }
            
            // Déduplication par Hachage et Comparaison (utilise pattern_to_hash)
            uint32_t h = fnv1a32(pattern_to_hash);
            int idx;

            // Recherche du hash
            auto it_hash = uniqueHashes.find(h);
            bool found = false;

            if (it_hash != uniqueHashes.end()) {
                int existing_idx = it_hash->second;
                
                // === Vérification stricte des collisions (re-masquage) ===
                // On masque le pattern brut déjà stocké pour le comparer au pattern_to_hash actuel
                ByteBlock existing_pattern_masked = uniquePatternsMap.at(existing_idx);
                
                if (useSilenceMasking && channel_idx != -1) {
                    existing_pattern_masked.clear();
                    existing_pattern_masked.reserve(patSize);

                    for (int frame = 0; frame < patSize; ++frame) {
                        if (start + frame >= R7_src.size()) {
                            existing_pattern_masked.push_back(uniquePatternsMap.at(existing_idx)[frame]);
                            continue;
                        }
                        
                        uint8_t r7_val = R7_src[start + frame];
                        uint8_t silence_mask_bit = (1 << channel_idx); 
                        
                        if ((r7_val & silence_mask_bit) != 0) {
                            existing_pattern_masked.push_back(0x00); // Masque
                        } else {
                            existing_pattern_masked.push_back(uniquePatternsMap.at(existing_idx)[frame]);
                        }
                    }
                }

                if (existing_pattern_masked == pattern_to_hash) {
                    idx = existing_idx;
                    found = true;
                }
            }

            if (!found) {
                // Nouveau pattern
                idx = next_pattern_idx++;
                // On utilise le HASH du pattern MASQUÉ pour le tracking (uniqueHashes)
                uniqueHashes[h] = idx; 
                // On stocke le pattern BRUT (pat) dans la map pour la phase 2 (overlap)
                uniquePatternsMap[idx] = move(pat); 
            }

            if (idx > 0xFFFF)
                throw runtime_error("Pattern index exceeds 16-bit");

            // Stockage de l'Index (16 bits, Little-Endian)
            seq.push_back(uint8_t(idx & 0xFF));
            seq.push_back(uint8_t((idx >> 8) & 0xFF));
        }
        sequenceBuffers.push_back(move(seq));
    }

    if (optimizationLevel != 0) {
        // --- Phase 2: Optimisation du chevauchement (Glouton ou GA/SA) ---
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
