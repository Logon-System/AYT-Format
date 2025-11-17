#pragma once
#include "typedefs.h"
#include "options.h"

// Structure storing optimization results
struct OptimizedResult {
    ByteBlock optimized_heap;               // Le tableau contigu minimal
    std::map<int, int> optimized_pointers;  // Map: Index Bloc Original -> Index dans le Heap
    std::vector<int> optimized_block_order; // Ordre des blocs dans le Heap
};

OptimizedResult
refine_order_with_evolutionary_algorithm(const OptimizedResult& glouton_result,
                                         const map<int, ByteBlock>& original_patterns, int patSize,
                                         Options& options, bool& optimization_running);

OptimizedResult refine_order_with_simulated_annealing(const OptimizedResult& glouton_result,
                                                      const map<int, ByteBlock>& original_patterns,
                                                      int patSize, bool& optimization_running);

OptimizedResult refine_order_with_tabu_search(const OptimizedResult& glouton_result,
                                              const map<int, ByteBlock>& original_patterns,
                                              int max_iterations, int patSize,
                                              bool& optimization_running);

OptimizedResult refine_order_with_ils(const OptimizedResult& glouton_result,
                                      const map<int, ByteBlock>& original_patterns,
                                      int max_iterations, int patSize, bool& optimization_running);

// Random generators
static std::random_device rd;
static std::mt19937 rng(rd());
static std::uniform_real_distribution<> dist_prob(0.0, 1.0);

int find_max_overlap(const ByteBlock& B1, const ByteBlock& B2, int ByteBlockSize);

void replaceByOptimizedIndex(vector<ByteBlock>& sequenceBuffers,
                             const OptimizedResult& optimized_result);

ByteBlock interleaveSequenceBuffers(const vector<ByteBlock>& sequenceBuffers);

double calculate_fitness(const vector<int>& ByteBlock_order, const map<int, ByteBlock>& ByteBlocks,
                         int ByteBlock_size);

OptimizedResult reconstruct_result(const vector<int>& best_order,
                                   const map<int, ByteBlock>& original_blocks, int patSize);

OptimizedResult merge_ByteBlocks_greedy(const map<int, ByteBlock>& original_ByteBlocks,
                                               int ByteBlockSize);