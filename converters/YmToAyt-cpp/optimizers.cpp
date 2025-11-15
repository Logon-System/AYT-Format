#include "optimizers.h"

// Computes overlap betwen 2 Byte Blocks B1 B2
int find_max_overlap(const ByteBlock& B1, const ByteBlock& B2, int ByteBlockSize) {
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

OptimizedResult merge_ByteBlocks_greedy(const map<int, ByteBlock>& original_ByteBlocks,
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
