#include "optimizers.h"


// ------------------ Algorithme Genetique -------------------

// --- Opérateur de Croisement ---
std::vector<int> crossover_pmx(const std::vector<int>& parent1, const std::vector<int>& parent2) {
    size_t N = parent1.size();
    std::vector<int> child(N, -1);
    std::map<int, int> mapping; // Pour gérer le mapping des éléments

    // Choisir deux points de coupe aléatoirement
    uniform_int_distribution<> dist_cut(0, N - 1);
    size_t cut1 = dist_cut(rng);
    size_t cut2 = dist_cut(rng);
    if (cut1 > cut2)
        swap(cut1, cut2);
    if (cut1 == cut2) {
        cut2 = min(N - 1, cut1 + 1); // Assurer une zone de croisement
    }

    // 1. Copier la section centrale du Parent 1
    for (size_t i = cut1; i <= cut2; ++i) {
        child[i] = parent1[i];
        mapping[parent1[i]] = parent2[i]; // Mettre en place le mapping de la zone
    }

    // 2. Transférer les éléments restants du Parent 2
    for (size_t i = 0; i < N; ++i) {
        if (i >= cut1 && i <= cut2)
            continue; // Sauter la section centrale

        int element = parent2[i];

        // 3. Résoudre les conflits via le mapping
        while (mapping.count(element)) {
            element = mapping.at(element);
        }

        // Si l'élément (après mapping) n'est pas déjà dans la section centrale
        bool already_in_center = false;
        for (size_t k = cut1; k <= cut2; ++k) {
            if (child[k] == element) {
                already_in_center = true;
                break;
            }
        }

        if (!already_in_center) {
            child[i] = element;
        } else {
            if (find(child.begin(), child.end(), element) == child.end()) {
                child[i] = element;
            }
        }
    }

    // Correction finale pour garantir la permutation:
    std::vector<int> missing_elements;
    std::vector<bool> present(N, false);
    for (size_t i = 0; i < N; ++i) {
        if (child[i] != -1) {
            present[child[i]] = true;
        }
    }
    for (int i = 0; i < (int)N; ++i) {
        if (!present[i]) {
            missing_elements.push_back(i);
        }
    }

    size_t missing_idx = 0;
    for (size_t i = 0; i < N; ++i) {
        if (child[i] == -1) {
            child[i] = missing_elements[missing_idx++];
        }
    }

    return child;
}

// Opérateur de Mutation (Swap aléatoire)
void mutate(std::vector<int>& order, double tmut) {
    if (order.size() < 2)
        return;

    if (tmut > 0.99f || dist_prob(rng) < tmut) {
        uniform_int_distribution<> dist_idx(0, order.size() - 1);
        int idx1 = dist_idx(rng);
        int idx2 = dist_idx(rng);
        while (idx1 == idx2) {
            idx2 = dist_idx(rng);
        }
        swap(order[idx1], order[idx2]);
    }
}

/**
 * @brief Applique la mutation (swap aléatoire) avec une probabilité MUTATION_RATE
 * pour chaque position de la séquence.
 *
 * @param order L'ordre des blocs (permutation) à muter.
 */
void mutate_by_index_swap(vector<int>& order, double tmut) {
    if (order.size() < 2)
        return;
    size_t N = order.size();

    // Distribution uniforme pour le choix de la cible du swap
    uniform_int_distribution<> dist_idx(0, N - 1);

    for (size_t i = 0; i < N; ++i) {
        // Déterminer si l'élément à la position i doit muter
        if (dist_prob(rng) < tmut / (double)N) {

            // Si mutation, choisir une position cible j aléatoirement
            int target_j = dist_idx(rng);

            // Assurer que l'échange ne se fait pas avec lui-même
            while ((size_t)target_j == i) {
                target_j = dist_idx(rng);
            }

            // Effectuer le swap
            swap(order[i], order[target_j]);
            return;
        }
    }
}


/**
 * @brief Algorithme Évolutionnaire (μ + λ) pour affiner l'ordre des patterns.
 */
OptimizedResult
refine_order_with_evolutionary_algorithm(const OptimizedResult& glouton_result,
                                         const map<int, ByteBlock>& original_patterns,
                                         int patSize, Options &options, bool &optimization_running) {
    size_t N = original_patterns.size();
    if (N < 2)
        return glouton_result;

    // 1. Initialisation de la Population
    // La population contient des paires (Ordre, Coût)
    vector<pair<vector<int>, double>> population;

    // Utiliser l'ordre glouton comme premier individu
    double glouton_fitness =
        calculate_fitness(glouton_result.optimized_block_order, original_patterns, patSize);
    population.push_back({glouton_result.optimized_block_order, glouton_fitness});

    // Remplir le reste de la population avec des ordres aléatoires
    vector<int> base_order(N);
    iota(base_order.begin(), base_order.end(), 0); // Ordre 0, 1, 2, ...
    int BestCost0 = 0;
    int BestCost1 = 0;
    int bestCostLog = 0;

    int max_generations = options.ga_NUM_GENERATION_MIN;

    for (size_t i = 1; i < options.ga_MU; ++i) {
        shuffle(base_order.begin(), base_order.end(), rng);
        double fitness = calculate_fitness(base_order, original_patterns, patSize);
        population.push_back({base_order, fitness});
    }

    // 2. Boucle Évolutionnaire
    for (int gen = 0; gen < max_generations && optimization_running; ++gen) {

        vector<pair<vector<int>, double>> children;

        // Création des λ enfants
        for (size_t i = 0; i < options.ga_LAMBDA; ++i) {

            // Sélection des Parents (ici, sélection aléatoire simple parmi les meilleurs existants)
            uniform_int_distribution<> dist_parent(0, population.size() - 1);
            const auto& p1 = population[dist_parent(rng)];
            const auto& p2 = population[dist_parent(rng)];

            vector<int> child_order;

            if (dist_prob(rng) < options.ga_CROSSOVER_RATE) {
                // Croisement
                child_order = crossover_pmx(p1.first, p2.first);
            } else {
                // Simple copie (reproduction) du meilleur parent
                child_order = (p1.second < p2.second) ? p1.first : p2.first;
            }

            double tmut = options.ga_MUTATION_RATE_MIN;

            // Adaptive Mutation Rate
            if (BestCost1 == BestCost0) {
                tmut = options.ga_MUTATION_RATE_MAX;
            }
            if (BestCost1 < BestCost0 + 10) {
                if ((BestCost1 > BestCost0) && (p1.second >= BestCost0)) {
                    double dc = p1.second - BestCost0;
                    if (dc > 0)
                        tmut = options.ga_MUTATION_RATE_MIN +
                               (options.ga_MUTATION_RATE_MAX - options.ga_MUTATION_RATE_MIN) * dc /
                                   (double)(BestCost1 - BestCost0);
                }
            }

            switch (i & 15) {
            case 0:
                mutate(child_order, 1.0f);
                break;
            case 1:
                mutate_by_index_swap(child_order, tmut);
                break;
            default:
                mutate(child_order, tmut);
                break;
            }

            //  Évaluation de l'enfant
            double child_fitness = calculate_fitness(child_order, original_patterns, patSize);
            children.push_back({move(child_order), child_fitness});
        }

        // 3. Sélection (μ + λ) : Les μ meilleurs survivent

        // Combiner parents et enfants
        population.insert(population.end(), children.begin(), children.end());

        // Tri par coût (les plus petits coûts sont les meilleurs)
        sort(population.begin(), population.end(),
             [](const auto& a, const auto& b) { return a.second < b.second; });

        // Réduire la population aux μ meilleurs (Survival Selection)
        // On pourrait garder quelques individus moins bons.
        if (population.size() > options.ga_MU) {
            population.resize(options.ga_MU);
        }

        // Affichage des stats optionnel
        BestCost0 = population[0].second;
        BestCost1 = population[options.ga_MU - 1].second;

        if (bestCostLog == 0 || bestCostLog > BestCost0) {
            max_generations = max(max_generations, options.ga_ADDITIONAL_GENERATIONS + gen);

            if (options.ga_NUM_GENERATION_MAX > 0)
                max_generations = min(max_generations, options.ga_NUM_GENERATION_MAX);

            if (options.verbosity > 0)
                cout << "GA Optimizer: Gen #" << gen << "/" << max_generations
                     << ": Fitness Range = " << population[0].second << "-"
                     << population[options.ga_MU - 1].second << endl;
            bestCostLog = BestCost0;
        }
    }

    // 4. Finalisation
    const auto& best_solution = population[0];
    // cout << "Coût final (AE): " << best_solution.second << " octets.\n";

    // Reconstruire le Heap et les pointeurs avec l'ordre le plus optimal trouvé
    return reconstruct_result(best_solution.first, original_patterns, patSize);
}