#include "optimizers.h"

using namespace std;


// ----------- Simulated Anhealing ----------------

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
 * @brief Refine initial pattern orders using simulated anhealing (SA).
 *
 * @param glouton_result : Initial state
 * @param original_patterns : pattern map (Index -> Byte Block).
 * @param max_iterations : Maximum iterations steps
 * @param patSize : Patterns size (1-128 bytes)
 * @return OptimizedResult : Optimized result
 */
OptimizedResult refine_order_with_simulated_annealing(const OptimizedResult& glouton_result,
                                                      const map<int, ByteBlock>& original_patterns,
                                                      int max_iterations, int patSize ,bool &optimization_running) {
    if (glouton_result.optimized_block_order.size() < 2)
        return glouton_result;

    // Initialization
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

