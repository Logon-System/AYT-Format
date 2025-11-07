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


ResultSequences buildBuffers(const array<ByteBlock, 16>& rawData, uint16_t activeRegs,
                                    int patSize, int optimizationLevel) {

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

                if (optimizationLevel == 0) {
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

    if (optimizationLevel != 0) {

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
