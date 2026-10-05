// The generator reuses the independent solver.c graph from ida_host.cpp and
// changes only the candidate include to the exact instrumented ida.cpp copy.
#include "ida_profile_oracle.inc"
#include <inttypes.h>

struct DepthStatistics {
    uint64_t states = 0;
    uint64_t total_nodes = 0;
    uint64_t worst_nodes = 0;
    uint32_t worst_rank = 0;
};

static void state_code(uint32_t rank, char code[15])
{
    oracle::state_t state;
    oracle::unrank_state(rank, &state);
    for (unsigned i = 0; i < 7; ++i) {
        code[i] = char('1' + state.p[i]);
        code[i + 7] = char('1' + state.o[i]);
    }
    code[14] = '\0';
}

static uint64_t profile_state(const OracleGraph &graph, uint32_t rank)
{
    candidate::profile_nodes = 0;
    uint8_t length = 255;
    require(candidate::ida_star(
                {uint16_t(rank / oracle::ORIENTATIONS),
                 uint16_t(rank % oracle::ORIENTATIONS)}, &length),
            "profile ida_star returns solution", rank);
    require(length == graph.distance[rank], "profile optimal length", rank);
    uint32_t replay = rank;
    for (unsigned i = 0; i < length; ++i) {
        require(candidate::solution[i] < oracle::MOVES,
                "profile move range", rank);
        replay = graph.apply(replay, candidate::solution[i]);
    }
    require(replay == 0, "profile solver.c transition replay", rank);
    require(candidate::profile_nodes > 0, "profile counts root", rank);
    return candidate::profile_nodes;
}

int main(int argc, char **argv)
{
    require(argc == 2 || (argc == 3 && !strcmp(argv[2], "--sample")),
            "usage: ida-profile source-sha256 [--sample]");
    const bool sample = argc == 3;
    setbuf(stdout, nullptr);
    const auto start = Clock::now();
    OracleGraph graph;
    constexpr std::array<uint32_t, 12> expected_counts = {
        1, 9, 54, 321, 1847, 9992, 50136, 227536,
        870072, 1887748, 623800, 2644
    };
    require(graph.counts == expected_counts, "profile independent BFS histogram");
    printf("Oracle BFS PASS: states=%u diameter=11 wall=%.3fs\n", oracle::STATES, elapsed(start));
    printf("Node definition: one first entry into each IDA* stack frame, including immediate pruning, roots, table hits and repeated visits across thresholds; sparse-table suffix replay contributes no nodes.\n");

    std::array<bool, 12> depth_seen{};
    for (uint32_t rank = 0; rank < oracle::STATES; ++rank) {
        const unsigned depth = graph.distance[rank];
        if (depth_seen[depth]) continue;
        depth_seen[depth] = true;
        const uint64_t nodes = profile_state(graph, rank);
        if (depth <= 5) require(nodes == 1, "table hit is one node", rank);
        char code[15];
        state_code(rank, code);
        printf("Depth sample PASS: depth=%u rank=%u code=%s nodes=%" PRIu64 "\n", depth, rank, code, nodes);
    }
    if (sample) return 0;

    std::array<DepthStatistics, 12> statistics{};
    const auto search_start = Clock::now();
    uint64_t total_nodes = 0;
    uint64_t worst_nodes = 0;
    uint32_t worst_rank = 0;
    for (uint32_t rank = 0; rank < oracle::STATES; ++rank) {
        const uint64_t nodes = profile_state(graph, rank);
        auto &row = statistics[graph.distance[rank]];
        ++row.states;
        row.total_nodes += nodes;
        total_nodes += nodes;
        if (nodes > row.worst_nodes) {
            row.worst_nodes = nodes;
            row.worst_rank = rank;
        }
        if (nodes > worst_nodes) {
            worst_nodes = nodes;
            worst_rank = rank;
        }
        if ((rank + 1) % 100000 == 0)
            printf("Profile progress: checked=%u/%u total_nodes=%" PRIu64 " search_wall=%.3fs\n", rank + 1, oracle::STATES, total_nodes, elapsed(search_start));
    }
    FILE *json = fopen("tools/search-stats.json", "w");
    require(json != nullptr, "open profile JSON");
    fprintf(json, "{\n  \"implementation\": \"ida.cpp\",\n  \"source_sha256\": \"%s\",\n  \"node_definition\": \"First entry into each IDA* stack frame; includes root, immediate f-threshold pruning, sparse-table hits, and repeated visits across thresholds; excludes table-suffix replay.\",\n  \"states\": %u,\n  \"diameter\": 11,\n  \"per_depth\": [\n", argv[1], oracle::STATES);
    for (unsigned depth = 0; depth < statistics.size(); ++depth) {
        const auto &row = statistics[depth];
        require(row.states == graph.counts[depth], "profile all depth counts", depth);
        char code[15];
        state_code(row.worst_rank, code);
        const double mean = double(row.total_nodes) / double(row.states);
        printf("Depth %u: states=%" PRIu64 " total_nodes=%" PRIu64 " mean=%.6f worst=%" PRIu64 " worst_rank=%u worst_code=%s\n", depth, row.states, row.total_nodes, mean, row.worst_nodes, row.worst_rank, code);
        fprintf(json, "    {\"depth\": %u, \"states\": %" PRIu64 ", \"total_nodes\": %" PRIu64 ", \"mean_nodes\": %.9f, \"worst_nodes\": %" PRIu64 ", \"worst_rank\": %u, \"worst_code\": \"%s\"}%s\n", depth, row.states, row.total_nodes, mean, row.worst_nodes, row.worst_rank, code, depth + 1 == statistics.size() ? "" : ",");
    }
    char code[15];
    state_code(worst_rank, code);
    const double mean = double(total_nodes) / double(oracle::STATES);
    fprintf(json, "  ],\n  \"global\": {\"total_nodes\": %" PRIu64 ", \"mean_nodes\": %.9f, \"worst_nodes\": %" PRIu64 ", \"worst_rank\": %u, \"worst_code\": \"%s\"},\n  \"search_seconds\": %.6f,\n  \"total_seconds\": %.6f,\n  \"correctness\": \"PASS: all paths replay to solved using solver.c transitions and all lengths match independent BFS distances\"\n}\n", total_nodes, mean, worst_nodes, worst_rank, code, elapsed(search_start), elapsed(start));
    require(fclose(json) == 0, "write profile JSON");
    printf("PROFILE PASS ALL: states=%u total_nodes=%" PRIu64 " mean=%.6f worst=%" PRIu64 " worst_rank=%u worst_code=%s search_wall=%.3fs total_wall=%.3fs\n", oracle::STATES, total_nodes, mean, worst_nodes, worst_rank, code, elapsed(search_start), elapsed(start));
}
