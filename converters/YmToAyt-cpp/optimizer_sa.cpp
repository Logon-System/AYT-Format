#include "optimizers.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

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

// Fonction utilitaire pour générer un état voisin par "reversal" (Inversion)
vector<int> get_neighbor_by_reversal(const vector<int>& current_order) {
    vector<int> neighbor = current_order;
    size_t N = neighbor.size();
    if (N < 2)
        return neighbor;

    // Utiliser la rng statique
    uniform_int_distribution<size_t> distrib_idx(0, N - 1);
    size_t start = distrib_idx(rng);
    size_t end = distrib_idx(rng);

    if (start > end) {
        swap(start, end);
    }

    // Inverser la sous-séquence [start, end]
    reverse(neighbor.begin() + start, neighbor.begin() + end + 1);
    return neighbor;
}

// ----------------------------------------------------------------------------

/**
 * @brief Refine initial pattern orders using simulated anhealing (SA).
 *
 * @param glouton_result : Initial state
 * @param original_patterns : pattern map (Index -> Byte Block).
 * @param patSize : Patterns size (1-128 bytes)
 * @param options : options (verbosity, optimizer options)
 * @return OptimizedResult : Optimized result
 */
OptimizedResult refine_order_with_simulated_annealing(const OptimizedResult& glouton_result,
                                                      const PatternBlocks& original_patterns,
                                                      int patSize, Options& options,
                                                      bool& optimization_running) {
    if (glouton_result.optimized_block_order.size() < 2)
        return glouton_result;

    // --- Paramètres de l'Algorithme de Recuit Simulé ---
    uniform_real_distribution<> prob_distrib(0.0, 1.0);

    // Precompute the directed overlap matrix once (roadmap #1) : les swaps
    // s'évaluent alors en O(1) par delta, les reversals en O(N), au lieu de
    // O(N.patSize^2) à chaque itération.
    const OverlapMatrix overlap = build_overlap_matrix(original_patterns, patSize);

    // --- Initialisation ---
    vector<int> current_order = glouton_result.optimized_block_order;
    double current_cost = calculate_fitness(current_order, original_patterns, overlap, patSize);

    double temp_initial = 5.0; // Température initiale (point de départ de l'exploration)
    double alpha = 0.99; // Facteur de refroidissement (diminution de la température par palier)
    // Nombre d'itérations par palier de température (pour atteindre un quasi-équilibre à chaque T)
    const double C_CALIBRATION = 0.05;
    // Nombre d'itérations par palier (adaptatif : max(100, C * N^2))
    // Ceci garantit un temps de "recuit" approprié pour la taille N du problème.
    const int iterations_per_stage = max(100.0, C_CALIBRATION * pow(current_order.size(), 2));

    if (options.verbosity > 0) {
        cout << "SA Optimizer: T0 = " << temp_initial << ", Alpha = " << alpha
             << ", Iteration per stage = " << iterations_per_stage << endl;
    }

    // Le meilleur état trouvé jusqu'à présent (Meilleur coût global)
    vector<int> best_order = current_order;
    double best_cost = current_cost;

    double temp = temp_initial;
    int stage_iteration = 0;

    // --- Boucle Principale ---
    for (int iter = 0; optimization_running; ++iter) {

        // Générer un état voisin. On préserve la séquence exacte de tirages RNG
        // de l'ancien code (get_neighbor_by_swap / get_neighbor_by_reversal) pour
        // garantir une trajectoire identique à seed fixe.
        bool move_is_swap;
        int swap_i = -1, swap_j = -1;
        vector<int> neighbor_order; // utilisé uniquement pour le reversal
        double neighbor_cost;

        // Exploraion locale la plupart du temps
        if (prob_distrib(rng) < 0.75) {
            // Swap : mêmes tirages que get_neighbor_by_swap, mais coût en O(1) (delta).
            uniform_int_distribution<> distrib(0, current_order.size() - 1);
            swap_i = distrib(rng);
            swap_j = distrib(rng);
            while (swap_i == swap_j) {
                swap_j = distrib(rng);
            }
            move_is_swap = true;
            neighbor_cost = current_cost + swap_delta_cost(current_order, original_patterns,
                                                           overlap, patSize, swap_i, swap_j);
        } else {
            neighbor_order = get_neighbor_by_reversal(current_order);
            move_is_swap = false;
            neighbor_cost = calculate_fitness(neighbor_order, original_patterns, overlap, patSize);
        }

        // Applique le mouvement retenu et met à jour current_cost (delta exact).
        auto apply_move = [&]() {
            if (move_is_swap)
                swap(current_order[swap_i], current_order[swap_j]);
            else
                current_order = neighbor_order;
            current_cost = neighbor_cost;
        };

        // delta_E < 0 signifie un GAIN (réduction du coût/taille = meilleur fitness)
        double delta_E = neighbor_cost - current_cost;

        // 1. Condition d'Acceptation
        if (delta_E < 0) {
            // Solution améliorée : accepter toujours
            apply_move();

            // Mise à jour du MEILLEUR état global
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best_order = current_order;
                if (options.verbosity > 0) {
                    cout << "SA Optimizer: Iter #" << iter << ", Temp = " << temp
                         << ", Best = " << best_cost << endl;
                }
                stage_iteration = 0;
            }
        } else {
            // Solution moins bonne (plus coûteuse) : accepter avec probabilité exp(-delta_E / T)
            double acceptance_prob = exp(-delta_E / temp);

            if (prob_distrib(rng) < acceptance_prob) {
                // Mouvement accepté pour échapper aux optima locaux
                apply_move();
            }
        }

        // 2. Refroidissement par Palier
        // La température diminue seulement après `iterations_per_stage` itérations.
        if (stage_iteration > iterations_per_stage) {
            temp *= alpha;
            stage_iteration = 0;
        }

        // Arrêt si la température devient trop faible
        if (temp < 1e-3) {
            break;
        }
        stage_iteration++;
    }

    // Le résultat final DOIT être basé sur le best_order trouvé, qui est la solution optimale.
    return reconstruct_result(best_order, original_patterns, patSize);
}
