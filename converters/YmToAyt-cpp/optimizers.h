#pragma once
#include "typedefs.h"
#include "options.h"

// Pattern blocks are stored in a dense vector indexed by pattern id (0..N-1).
// Deduplication in buildBuffers assigns ids sequentially, so a plain vector
// replaces the previous map<int, ByteBlock> without changing iteration order,
// while being far more cache-friendly in the hot optimization loops.
using PatternBlocks = std::vector<ByteBlock>;

// Structure storing optimization results
struct OptimizedResult {
    ByteBlock optimized_heap;               // Le tableau contigu minimal
    std::vector<int> optimized_pointers;    // Index pattern id -> Index dans le Heap (-1 si absent)
    std::vector<int> optimized_block_order; // Ordre des blocs dans le Heap
};

OptimizedResult
refine_order_with_evolutionary_algorithm(const OptimizedResult& glouton_result,
                                         const PatternBlocks& original_patterns, int patSize,
                                         Options& options, bool& optimization_running);

OptimizedResult refine_order_with_simulated_annealing(const OptimizedResult& glouton_result,
                                                      const PatternBlocks& original_patterns,
                                                      int patSize, Options& options, bool& optimization_running);

OptimizedResult refine_order_with_tabu_search(const OptimizedResult& glouton_result,
                                              const PatternBlocks& original_patterns,
                                              int max_iterations, int patSize,
                                              bool& optimization_running);

OptimizedResult refine_order_with_ils(const OptimizedResult& glouton_result,
                                      const PatternBlocks& original_patterns,
                                      int max_iterations, int patSize, bool& optimization_running);

// Shared random generator (a single instance across all translation units).
// By default it is seeded non-deterministically (std::random_device).
// Call seed_rng() to make the metaheuristics (SA/GA/ILS/Tabu) reproducible.
extern std::mt19937 rng;
extern std::uniform_real_distribution<> dist_prob;
void seed_rng(uint32_t seed);

int find_max_overlap(const ByteBlock& B1, const ByteBlock& B2, int ByteBlockSize);

void replaceByOptimizedIndex(vector<ByteBlock>& sequenceBuffers,
                             const OptimizedResult& optimized_result);

ByteBlock interleaveSequenceBuffers(const vector<ByteBlock>& sequenceBuffers);

double calculate_fitness(const vector<int>& ByteBlock_order, const PatternBlocks& ByteBlocks,
                         int ByteBlock_size);

OptimizedResult reconstruct_result(const vector<int>& best_order,
                                   const PatternBlocks& original_blocks, int patSize);

OptimizedResult merge_ByteBlocks_greedy(const PatternBlocks& original_ByteBlocks,
                                               int ByteBlockSize);