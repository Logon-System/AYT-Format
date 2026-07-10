#include "optimizers.h"

// Definition of the shared random generator (declared extern in optimizers.h).
// Default seed is non-deterministic; seed_rng() overrides it for reproducible runs.
std::mt19937 rng{std::random_device{}()};
std::uniform_real_distribution<> dist_prob(0.0, 1.0);
void seed_rng(uint32_t seed) { rng.seed(seed); }

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


OptimizedResult merge_ByteBlocks_greedy(const PatternBlocks& original_ByteBlocks,
                                               int ByteBlockSize) {
    OptimizedResult result;

    const size_t N = original_ByteBlocks.size();
    if (N == 0)
        return result;

    result.optimized_pointers.assign(N, -1);

    // Suivi des blocs déjà inclus dans le Heap
    vector<char> included(N, 0);

    // Itérer sur tous les blocs pour s'assurer que même les séquences déconnectées sont incluses.
    for (size_t start_ByteBlock_index = 0; start_ByteBlock_index < N; ++start_ByteBlock_index) {

        if (included[start_ByteBlock_index]) {
            continue; // Déjà inclus
        }

        // --- Démarrer une nouvelle séquence ---
        int current_block_index = (int)start_ByteBlock_index;
        const ByteBlock& first_ByteBlock = original_ByteBlocks[current_block_index];

        // Le pointeur du premier bloc est la taille actuelle du Heap.
        // C'est l'endroit où le nouveau bloc va commencer.
        int start_offset = result.optimized_heap.size();

        // Ajouter le bloc complet au Heap (il n'y a pas de chevauchement au début de la séquence)
        result.optimized_heap.insert(result.optimized_heap.end(), first_ByteBlock.begin(),
                                     first_ByteBlock.end());

        // Enregistrer le pointeur de départ
        result.optimized_pointers[current_block_index] = start_offset;
        result.optimized_block_order.push_back(current_block_index);
        included[current_block_index] = 1;

        // --- Extension gloutonne de la séquence ---
        bool extended = true;
        while (extended) {
            extended = false;
            int best_overlap = 0;
            int best_next_ByteBlock_index = -1;

            const ByteBlock& current_ByteBlock = original_ByteBlocks[current_block_index];

            // Rechercher le meilleur chevauchement avec n'importe quel bloc restant
            for (size_t next_index = 0; next_index < N; ++next_index) {
                if (included[next_index])
                    continue;

                int overlap =
                    find_max_overlap(current_ByteBlock, original_ByteBlocks[next_index], ByteBlockSize);

                // Si on trouve un chevauchement, on le choisit
                if (overlap > best_overlap) {
                    best_overlap = overlap;
                    best_next_ByteBlock_index = (int)next_index;
                    extended = true;
                }
            }

            if (extended) {
                // Un chevauchement optimal a été trouvé. Fusionner les blocs.
                const ByteBlock& best_next_ByteBlock =
                    original_ByteBlocks[best_next_ByteBlock_index];

                // Calculer l'index du pointeur du nouveau bloc :
                // Pointeur = Pointeur_précédent + (Taille_bloc - Overlap)
                int new_pointer_index =
                    result.optimized_pointers[current_block_index] + (ByteBlockSize - best_overlap);

                // Ajouter la partie non chevauchante au Heap
                result.optimized_heap.insert(result.optimized_heap.end(),
                                             best_next_ByteBlock.begin() + best_overlap,
                                             best_next_ByteBlock.end());

                // Mettre à jour l'état
                result.optimized_pointers[best_next_ByteBlock_index] = new_pointer_index;
                result.optimized_block_order.push_back(best_next_ByteBlock_index);
                included[best_next_ByteBlock_index] = 1;
                current_block_index = best_next_ByteBlock_index;
            }
        }
        // La boucle 'for' passe au prochain bloc non inclus.
    }

    return result;
}



// Fonction Coût : Calcule la taille des patterns pour un ordre donné
double calculate_fitness(const vector<int>& ByteBlock_order, const PatternBlocks& ByteBlocks,
                         int ByteBlock_size) {
    if (ByteBlock_order.empty())
        return 0;

    double total_size = ByteBlock_size; // Commencez avec la taille du premier bloc

    for (size_t i = 1; i < ByteBlock_order.size(); ++i) {
        const ByteBlock& B_prev = ByteBlocks[ByteBlock_order[i - 1]];
        const ByteBlock& B_curr = ByteBlocks[ByteBlock_order[i]];

        int overlap = find_max_overlap(B_prev, B_curr, ByteBlock_size);
        total_size += (ByteBlock_size - overlap);
    }
    return total_size;
}
