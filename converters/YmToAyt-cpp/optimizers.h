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

// ---------------------------------------------------------------------------
// Incremental / matrix-based fitness (roadmap #1)
// ---------------------------------------------------------------------------
// Directed overlap matrix: overlap(from, to) = max k such that the k-byte
// suffix of pattern `from` equals the k-byte prefix of pattern `to`.
// Since patSize <= 128, overlap <= 127 and fits in a uint8_t. Stored dense
// row-major (data[from * N + to]). An empty matrix (N == 0) means "not built"
// (memory guard tripped) and callers transparently fall back to on-the-fly
// find_max_overlap via overlapOf() below.
struct OverlapMatrix {
    int N = 0;
    std::vector<uint8_t> data;
    int get(int from, int to) const { return data[(size_t)from * N + to]; }
};

// Build the full N*N overlap matrix, or return an empty matrix if it would
// exceed the memory budget (callers then fall back to on-the-fly computation).
OverlapMatrix build_overlap_matrix(const PatternBlocks& patterns, int patSize);

// Overlap of two pattern ids, using the matrix when available, else recomputing
// on the fly. The branch is loop-invariant (matrix present for the whole run),
// so it predicts perfectly and inlines cleanly under LTO.
inline int overlapOf(const OverlapMatrix& M, const PatternBlocks& P, int patSize,
                     int from, int to) {
    return M.N ? M.get(from, to) : find_max_overlap(P[from], P[to], patSize);
}

// O(N) fitness using the overlap matrix. Equivalent to
// calculate_fitness(order, P, patSize) but without the per-pair patSize^2 scan.
double calculate_fitness(const vector<int>& order, const PatternBlocks& P,
                         const OverlapMatrix& M, int patSize);

// O(1) delta cost of swapping order[i] and order[j] WITHOUT mutating `order`.
// Returns new_cost - old_cost (negative == improvement). Only the <=4 edges
// incident to positions i and j change, so a swap costs a handful of lookups.
double swap_delta_cost(const vector<int>& order, const PatternBlocks& P,
                       const OverlapMatrix& M, int patSize, size_t i, size_t j);

void replaceByOptimizedIndex(vector<ByteBlock>& sequenceBuffers,
                             const OptimizedResult& optimized_result);

ByteBlock interleaveSequenceBuffers(const vector<ByteBlock>& sequenceBuffers);

double calculate_fitness(const vector<int>& ByteBlock_order, const PatternBlocks& ByteBlocks,
                         int ByteBlock_size);

OptimizedResult reconstruct_result(const vector<int>& best_order,
                                   const PatternBlocks& original_blocks, int patSize);

OptimizedResult merge_ByteBlocks_greedy(const PatternBlocks& original_ByteBlocks,
                                               int ByteBlockSize);