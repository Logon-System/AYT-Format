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

    if (original_ByteBlocks.empty())
        return result;

    // Suivi des blocs déjà inclus dans le Heap
    map<int, bool> included;
    for (const auto& pair : original_ByteBlocks) {
        included[pair.first] = false;
    }

    // Itérer sur tous les blocs pour s'assurer que même les séquences déconnectées sont incluses.
    for (const auto& pair : original_ByteBlocks) {
        int start_ByteBlock_index = pair.first;
        
        if (included[start_ByteBlock_index]) {
            continue; // Déjà inclus
        }

        // --- Démarrer une nouvelle séquence ---
        int current_block_index = start_ByteBlock_index;
        const ByteBlock& first_ByteBlock = original_ByteBlocks.at(current_block_index);
        
        // Le pointeur du premier bloc est la taille actuelle du Heap. 
        // C'est l'endroit où le nouveau bloc va commencer.
        int start_offset = result.optimized_heap.size(); 

        // Ajouter le bloc complet au Heap (il n'y a pas de chevauchement au début de la séquence)
        result.optimized_heap.insert(result.optimized_heap.end(), first_ByteBlock.begin(),
                                     first_ByteBlock.end());

        // Enregistrer le pointeur de départ
        result.optimized_pointers[current_block_index] = start_offset;
        result.optimized_block_order.push_back(current_block_index);
        included[current_block_index] = true;
        
        // --- Extension gloutonne de la séquence ---
        bool extended = true;
        while (extended) {
            extended = false;
            int best_overlap = 0;
            int best_next_ByteBlock_index = -1;

            const ByteBlock& current_ByteBlock = original_ByteBlocks.at(current_block_index);

            // Rechercher le meilleur chevauchement avec n'importe quel bloc restant
            for (const auto& inner_pair : original_ByteBlocks) {
                int next_index = inner_pair.first;
                const ByteBlock& next_ByteBlock = inner_pair.second;

                if (!included[next_index]) {
                    int overlap = find_max_overlap(current_ByteBlock, next_ByteBlock, ByteBlockSize);

                    // Si on trouve un chevauchement, on le choisit
                    if (overlap > best_overlap) {
                        best_overlap = overlap;
                        best_next_ByteBlock_index = next_index;
                        extended = true;
                    }
                }
            }

            if (extended) {
                // Un chevauchement optimal a été trouvé. Fusionner les blocs.
                const ByteBlock& best_next_ByteBlock =
                    original_ByteBlocks.at(best_next_ByteBlock_index);

                // Calculer l'index du pointeur du nouveau bloc : 
                // Pointeur = Pointeur_précédent + (Taille_bloc - Overlap)
                int new_pointer_index =
                    result.optimized_pointers.at(current_block_index) + (ByteBlockSize - best_overlap);

                // Ajouter la partie non chevauchante au Heap
                result.optimized_heap.insert(result.optimized_heap.end(),
                                             best_next_ByteBlock.begin() + best_overlap,
                                             best_next_ByteBlock.end());

                // Mettre à jour l'état
                result.optimized_pointers[best_next_ByteBlock_index] = new_pointer_index;
                result.optimized_block_order.push_back(best_next_ByteBlock_index);
                included[best_next_ByteBlock_index] = true;
                current_block_index = best_next_ByteBlock_index;
            }
        }
        // La boucle 'for' passe au prochain bloc non inclus.
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
