
#include <map>
#include "optimizers.h"
#include <list>
#include <algorithm>

using namespace std;

// ---------------------------------------------------

// Le type de la Liste Tabou : stocke les paires d'indices échangés (les mouvements)
using TabuList = list<pair<int, int>>;
const size_t TABU_TENURE = 20; // Durée de l'interdiction (ex: 20 itérations)

// Une fonction helper pour vérifier si un mouvement est tabou
bool is_tabu(const TabuList& tabu_list, int idx1, int idx2) {
    // Le mouvement est symétrique, donc vérifier (idx1, idx2) et (idx2, idx1)
    return find(tabu_list.begin(), tabu_list.end(), make_pair(idx1, idx2)) != tabu_list.end() ||
           find(tabu_list.begin(), tabu_list.end(), make_pair(idx2, idx1)) != tabu_list.end();
}

/**
 * @brief Améliore l'ordre glouton en utilisant la Recherche Tabou.
 *
 * @param glouton_result Le résultat initial de l'approche gloutonne.
 * @param original_patterns La map des patterns originaux (Index -> Block).
 * @param max_iterations Nombre maximal d'itérations.
 * @param patSize Taille des blocs.
 * @return OptimizedResult Le résultat final optimisé.
 */
OptimizedResult refine_order_with_tabu_search(const OptimizedResult& glouton_result,
                                              const map<int, ByteBlock>& original_patterns,
                                              int max_iterations, int patSize, bool &optimization_running) {
    if (glouton_result.optimized_block_order.size() < 2)
        return glouton_result;

    vector<int> current_order = glouton_result.optimized_block_order;
    vector<int> best_order = current_order;
    double current_cost = calculate_fitness(current_order, original_patterns, patSize);
    double best_cost = current_cost;

    TabuList tabu_list;

    size_t N = current_order.size();

    cout << "--- Recherche Tabou ---\n";
    cout << "Coût initial (Glouton): " << best_cost << " octets.\n";

    for (int iter = 0; iter < max_iterations && optimization_running; ++iter) {

        double best_neighbor_cost = numeric_limits<double>::max();
        vector<int> best_neighbor_order;
        pair<int, int> best_move = {-1, -1}; // Indices de la permutation (pas des patterns)

        // 1. Exploration du Voisinage (Recherche du Meilleur Voisin non Tabou)
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = i + 1; j < N; ++j) {

                // Mouvement : échange des éléments aux positions i et j
                vector<int> neighbor_order = current_order;
                swap(neighbor_order[i], neighbor_order[j]);

                double neighbor_cost =
                    calculate_fitness(neighbor_order, original_patterns, patSize);

                // Critère d'Aspiration : Accepter un mouvement tabou si c'est le meilleur jamais
                // trouvé
                bool aspiration_criterion = neighbor_cost < best_cost;
                if (neighbor_cost < best_neighbor_cost &&
                    (!is_tabu(tabu_list, i, j) || aspiration_criterion)) {
                    best_neighbor_cost = neighbor_cost;
                    best_neighbor_order = neighbor_order;
                    best_move = {i, j};
                }
            }
        }

        // 2. Mise à jour de la solution (si un mouvement valide a été trouvé)
        if (best_move.first != -1) {

            // Effectuer le mouvement
            current_order = best_neighbor_order;
            current_cost = best_neighbor_cost;

            // Mettre à jour la meilleure solution globale
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best_order = current_order;
                cout << "TABU: found a better solution:" << best_neighbor_cost << endl;
            }

            // Mettre à jour la Liste Tabou
            tabu_list.push_back(best_move);
            if (tabu_list.size() > TABU_TENURE) {
                tabu_list.pop_front();
            }

        } else {
            // Tous les mouvements sont tabous ou le voisinage n'améliore pas
            // Dans une implémentation robuste, on pourrait déclencher une forte perturbation ici.
            break;
        }
    }

    cout << "Coût final (TS): " << best_cost << " octets.\n";

    // Reconstruire le Heap et les pointeurs avec l'ordre le plus optimal trouvé
    return reconstruct_result(best_order, original_patterns, patSize);
}