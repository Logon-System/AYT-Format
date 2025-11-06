#include "optimizers.h"

/**
 * @brief Perturbe l'ordre actuel en appliquant N_PERTURBATIONS swaps aléatoires.
 *
 * @param order L'ordre des blocs à perturber.
 * @param rng Le générateur de nombres aléatoires.
 * @return L'ordre perturbé.
 */
vector<int> perturb(vector<int> order, mt19937& rng) {
    size_t N = order.size();
    // Choisir le nombre de perturbations. 3 à 5% de la taille de N est un bon point de départ.
    const int N_PERTURBATIONS = max(1, (int)(N * 0.05));

    uniform_int_distribution<> distrib(0, N - 1);

    for (int k = 0; k < N_PERTURBATIONS; ++k) {
        int idx1 = distrib(rng);
        int idx2 = distrib(rng);
        while (idx1 == idx2) {
            idx2 = distrib(rng);
        }
        swap(order[idx1], order[idx2]);
    }
    return order;
}
/**
 * @brief Effectue une recherche locale agressive en testant tous les swaps possibles
 * et en acceptant le meilleur jusqu'à atteindre un optimum local.
 *
 * @param current_order L'ordre des blocs à améliorer.
 * @param original_patterns Les patterns originaux (pour le calcul du coût).
 * @param current_cost Le coût de l'ordre actuel.
 * @return La paire (ordre_optimal_local, coût_optimal_local).
 */
pair<vector<int>, double> local_search(const vector<int>& current_order,
                                       const map<int, ByteBlock>& original_patterns,
                                       double current_cost, int patSize) {
    vector<int> current = current_order;
    double cost = current_cost;
    size_t N = current.size();
    bool improved = true;

    // Répéter tant qu'une amélioration est possible
    while (improved) {
        improved = false;
        double best_local_gain = 0.0;
        int best_i = -1, best_j = -1;

        // Tester tous les swaps (voisinage)
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = i + 1; j < N; ++j) {

                // 1. Simuler le swap
                swap(current[i], current[j]);

                // 2. Calculer le nouveau coût
                double neighbor_cost = calculate_fitness(current, original_patterns, patSize);

                // 3. Évaluer l'amélioration
                double gain = cost - neighbor_cost; // gain > 0 si amélioration

                if (gain > best_local_gain) {
                    best_local_gain = gain;
                    best_i = i;
                    best_j = j;
                    improved = true;
                }

                // Annuler le swap pour la prochaine itération de la boucle
                swap(current[i], current[j]);
            }
        }

        // Appliquer le meilleur swap trouvé
        if (improved) {
            swap(current[best_i], current[best_j]);
            cost -= best_local_gain; // Mise à jour rapide du coût
        }
    }

    return {current, cost};
}

/**
 * @brief Améliore l'ordre glouton en utilisant la Recherche Locale Itérée (ILS).
 *
 * @param glouton_result Le résultat initial de l'approche gloutonne.
 * @param original_patterns La map des patterns originaux (Index -> Block).
 * @param max_iterations Nombre maximal d'itérations de la boucle ILS.
 * @param patSize Taille des blocs.
 * @return OptimizedResult Le résultat final optimisé.
 */
OptimizedResult refine_order_with_ils(const OptimizedResult& glouton_result,
                                      const map<int, ByteBlock>& original_patterns,
                                      int max_iterations, int patSize, bool &optimization_running) {
    if (glouton_result.optimized_block_order.size() < 2)
        return glouton_result;

    //    random_device rd;
    //    mt19937 rng(rd());

    // Initialisation avec le résultat glouton (ou un ordre aléatoire)
    vector<int> current_order = glouton_result.optimized_block_order;
    double current_cost = calculate_fitness(current_order, original_patterns, patSize);

    // La meilleure solution trouvée
    vector<int> best_order = current_order;
    double best_cost = current_cost;

    cout << "--- Iterative Local Search ---\n";
    cout << "Coût initial (Glouton): " << best_cost << " octets.\n";

    for (int iter = 0; iter < max_iterations && optimization_running; ++iter) {
        // cout << iter << endl;
        //  1. Recherche Locale (Passer à l'optimum local S*)
        //  Cette étape est essentielle et consomme le plus de temps.
        auto local_optimum_pair =
            local_search(current_order, original_patterns, current_cost, patSize);
        vector<int> S_star = local_optimum_pair.first;
        double cost_S_star = local_optimum_pair.second;

        // Mise à jour de la meilleure solution globale
        if (cost_S_star < best_cost) {
            best_cost = cost_S_star;
            best_order = S_star;
            cout << "ILS: best solution fitness: " << best_cost << " octets.\n";
        }

        // 2. Perturbation (Créer S' très différent de S*)
        vector<int> S_prime = perturb(S_star, rng);
        double cost_S_prime = calculate_fitness(S_prime, original_patterns, patSize);

        // 3. Critère d'Acceptation (Acceptation purement déterministe ici : accepter si S' est
        // meilleur) Pour une version plus robuste (similaire au SA), on pourrait ajouter une
        // probabilité d'acceptation.
        if (cost_S_prime < cost_S_star) {
            current_order = S_prime;
            current_cost = cost_S_prime;
        } else {
            // Si la perturbation est mauvaise, on revient à l'optimum local pour la prochaine
            // itération
            current_order = S_star;
            current_cost = cost_S_star;
        }
    }

    cout << "Coût final (ILS): " << best_cost << " octets.\n";

    // Reconstruire le Heap et les pointeurs avec l'ordre le plus optimal trouvé
    return reconstruct_result(best_order, original_patterns, patSize);
}
