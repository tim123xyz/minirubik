// Independent host checks for the unmodified IDA implementation and its data.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>
#include <algorithm>
#include <errno.h>
#include <stddef.h>
#include <chrono>
#include <vector>

namespace oracle {
#define main oracle_main
#include "../solver.c"
#undef main
}
namespace candidate {
#define IDA_NO_MAIN
#include "../ida.cpp"
#undef IDA_NO_MAIN
}

using Clock = std::chrono::steady_clock;
static double elapsed(Clock::time_point start)
{
    return std::chrono::duration<double>(Clock::now() - start).count();
}
static void require(bool condition, const char *description, uint32_t index = 0)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s index=%u\n", description, index);
        exit(1);
    }
}

struct OracleGraph {
    std::array<std::array<uint16_t, oracle::PERMUTATIONS>, 3> p{};
    std::array<std::array<uint16_t, oracle::ORIENTATIONS>, 3> o{};
    std::vector<uint8_t> distance{std::vector<uint8_t>(oracle::STATES, 255)};
    std::vector<uint8_t> parent{std::vector<uint8_t>(oracle::STATES, 255)};
    std::vector<uint32_t> queue;
    std::array<uint32_t, 12> counts{};

    uint32_t apply(uint32_t rank, unsigned move) const
    {
        unsigned next_p = rank / oracle::ORIENTATIONS;
        unsigned next_o = rank % oracle::ORIENTATIONS;
        for (unsigned turn = 0; turn <= move % 3; ++turn) {
            next_p = p[move / 3][next_p];
            next_o = o[move / 3][next_o];
        }
        return next_p * oracle::ORIENTATIONS + next_o;
    }

    OracleGraph()
    {
        oracle::state_t state;
        for (unsigned rank = 0; rank < oracle::PERMUTATIONS; ++rank) {
            oracle::unrank_state(rank * oracle::ORIENTATIONS, &state);
            for (unsigned face = 0; face < 3; ++face) {
                auto next = oracle::quarter_turn(state, face);
                p[face][rank] = oracle::rank_state(&next) / oracle::ORIENTATIONS;
            }
        }
        for (unsigned rank = 0; rank < oracle::ORIENTATIONS; ++rank) {
            oracle::unrank_state(rank, &state);
            for (unsigned face = 0; face < 3; ++face) {
                auto next = oracle::quarter_turn(state, face);
                o[face][rank] = oracle::rank_state(&next) % oracle::ORIENTATIONS;
            }
        }
        // Distances and parents are built only from solver.c's model, never
        // candidate transition tables or candidate search output.
        queue.reserve(oracle::STATES);
        queue.push_back(0);
        distance[0] = 0;
        parent[0] = 0;
        for (size_t head = 0; head < queue.size(); ++head) {
            const uint32_t here = queue[head];
            require(distance[here] < counts.size(), "oracle diameter", here);
            ++counts[distance[here]];
            for (unsigned move = 0; move < oracle::MOVES; ++move) {
                const uint32_t next = apply(here, move);
                if (distance[next] == 255) {
                    distance[next] = distance[here] + 1;
                    parent[next] = oracle::inverse_move[move];
                    queue.push_back(next);
                }
            }
        }
        require(queue.size() == oracle::STATES, "oracle reachability");
    }
};

static uint32_t dense_key(uint32_t oracle_rank)
{
    return oracle_rank / oracle::ORIENTATIONS +
           (oracle_rank % oracle::ORIENTATIONS) * oracle::PERMUTATIONS;
}

// Verify the assembly's separate, checked-in data too. This is source data
// agreement, not an execution test of the assembly search.
static void check_assembly_data(const char *path)
{
    FILE *file = fopen(path, "r");
    require(file != nullptr, "open assembly data");
    enum Section { none, perm, orient, sparse, prefix } section = none;
    std::array<std::vector<uint32_t>, 5> values;
    char line[4096];
    while (fgets(line, sizeof line, file)) {
        if (!strcmp(line, "permutation:\n")) section = perm;
        else if (!strcmp(line, "orientation:\n")) section = orient;
        else if (!strcmp(line, "table:\n")) section = sparse;
        else if (!strcmp(line, "table_prefix:\n")) section = prefix;
        else if (!strncmp(line, ".size ", 6)) section = none;
        else if (section != none) {
            char *numbers = strstr(line, section == sparse ? ".word " : ".half ");
            if (!numbers) continue;
            numbers += 6;
            while (*numbers) {
                char *end;
                errno = 0;
                unsigned long number = strtoul(numbers, &end, 0);
                require(end != numbers && errno == 0 && number <= UINT32_MAX,
                        "assembly numeric data");
                values[section].push_back(number);
                while (*end == ' ' || *end == '\t') ++end;
                if (*end != ',') break;
                numbers = end + 1;
            }
        }
    }
    require(!ferror(file), "read assembly data");
    fclose(file);
    require(values[perm].size() == 3 * oracle::PERMUTATIONS,
            "assembly permutation count");
    require(values[orient].size() == 3 * oracle::ORIENTATIONS,
            "assembly orientation count");
    require(values[sparse].size() == candidate::table.size(),
            "assembly sparse count");
    require(values[prefix].size() == 257, "assembly prefix count");
    for (unsigned face = 0; face < 3; ++face) {
        for (unsigned rank = 0; rank < oracle::PERMUTATIONS; ++rank)
            require(values[perm][face * oracle::PERMUTATIONS + rank] ==
                        candidate::permutation[face][rank],
                    "assembly permutation data", rank);
        for (unsigned rank = 0; rank < oracle::ORIENTATIONS; ++rank)
            require(values[orient][face * oracle::ORIENTATIONS + rank] ==
                        candidate::orientation[face][rank],
                    "assembly orientation data", rank);
    }
    require(std::equal(values[sparse].begin(), values[sparse].end(),
                       candidate::table.begin()), "assembly sparse data");
    size_t offset = 0;
    for (unsigned bucket = 0; bucket <= 256; ++bucket) {
        const uint64_t lower = uint64_t(bucket) << 22;
        while (offset < candidate::table.size() && candidate::table[offset] < lower)
            ++offset;
        require(values[prefix][bucket] == offset, "assembly prefix data", bucket);
    }
    printf("Assembly data PASS: %s; 15120 permutation + 2187 orientation + "
           "12224 sparse + 257 prefix entries; 84024 bytes\n", path);
}

int main(int argc, char **argv)
{
    bool full = false;
    unsigned sample = 300;
    const char *assembly = "table.S";
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--full")) full = true;
        else if (!strcmp(argv[i], "--sample") && i + 1 < argc) {
            char *end;
            errno = 0;
            unsigned long n = strtoul(argv[++i], &end, 10);
            require(*argv[i] && !*end && !errno && n >= 12 && n <= oracle::STATES,
                    "sample must be between 12 and 3674160");
            sample = n;
        } else if (!strcmp(argv[i], "--assembly") && i + 1 < argc)
            assembly = argv[++i];
        else {
            fprintf(stderr, "usage: %s [--sample N | --full] [--assembly table.S]\n", argv[0]);
            return 2;
        }
    }
    setbuf(stdout, nullptr);
    const auto start = Clock::now();
    OracleGraph graph;
    constexpr std::array<uint32_t, 12> expected_counts = {
        1, 9, 54, 321, 1847, 9992, 50136, 227536,
        870072, 1887748, 623800, 2644
    };
    require(graph.counts == expected_counts, "oracle distance histogram");
    printf("Oracle BFS PASS: reachable=3674160 diameter=11 wall=%.3fs; "
           "model=solver.c only\n", elapsed(start));
    printf("Oracle counts depth 0..11:");
    for (auto count : graph.counts) printf(" %u", count);
    putchar('\n');

    for (unsigned face = 0; face < 3; ++face) {
        for (unsigned p = 0; p < oracle::PERMUTATIONS; ++p)
            require(candidate::permutation[face][p] == graph.p[face][p],
                    "candidate permutation transition", p);
        for (unsigned o = 0; o < oracle::ORIENTATIONS; ++o)
            require(candidate::orientation[face][o] == graph.o[face][o],
                    "candidate orientation transition", o);
    }
    printf("H2 transitions PASS: all 15120 permutation + 2187 orientation "
           "entries agree with solver.c; maxima p=5039 o=728\n");

    std::vector<uint32_t> expected;
    for (uint32_t rank : graph.queue) {
        if (graph.distance[rank] > 5) break;
        expected.push_back((dense_key(rank) << 8) |
                           (uint32_t(graph.distance[rank]) << 4) | graph.parent[rank]);
    }
    std::sort(expected.begin(), expected.end());
    require(expected.size() == candidate::table.size(), "sparse exact count");
    require(std::equal(expected.begin(), expected.end(), candidate::table.begin()),
            "sparse exact encoded BFS records");
    for (size_t index = 0; index < expected.size(); ++index) {
        const uint32_t word = candidate::table[index];
        const uint32_t key = word >> 8;
        const unsigned depth = (word >> 4) & 15;
        const unsigned move = word & 15;
        const uint32_t rank = (key % oracle::PERMUTATIONS) * oracle::ORIENTATIONS +
                             key / oracle::PERMUTATIONS;
        require(key < oracle::STATES && depth <= 5 && move < oracle::MOVES,
                "record field ranges", index);
        require(((key << 8) | (depth << 4) | move) == word,
                "record field round trip", index);
        require(depth == graph.distance[rank], "record exact depth", index);
        require(index == 0 || (candidate::table[index - 1] >> 8) < key,
                "record strict key ordering", index);
        if (depth)
            require(graph.distance[graph.apply(rank, move)] + 1U == depth,
                    "record inverse parent", index);
        else require(key == 0 && move == 0, "solved record", index);
        uint8_t length = 255;
        const uint8_t prefix = candidate::MAX_SOLUTION - depth;
        std::fill(std::begin(candidate::solution), std::end(candidate::solution), 255);
        require(candidate::append_table_solution(
                    {uint16_t(key % oracle::PERMUTATIONS),
                     uint16_t(key / oracle::PERMUTATIONS)}, prefix, &length),
                "append sparse suffix", index);
        require(length == candidate::MAX_SOLUTION, "sparse suffix length", index);
        for (unsigned j = 0; j < prefix; ++j)
            require(candidate::solution[j] == 255, "suffix prefix preserved", index);
        oracle::state_t state;
        oracle::unrank_state(rank, &state);
        for (unsigned j = prefix; j < length; ++j) {
            require(candidate::solution[j] < oracle::MOVES, "suffix move range", index);
            state = oracle::apply_move(state, candidate::solution[j]);
        }
        require(oracle::rank_state(&state) == 0, "oracle suffix replay", index);
    }
    printf("H2 sparse PASS: all 12224 sorted depth<=5 records exactly match "
           "independent BFS; every inverse move and table suffix replays with solver.c\n");
    printf("H4 records PASS: all 12224 uint32 key/depth/move decodings and "
           "re-encodings; no packed-nibble distance accessor is used\n");

    for (uint32_t rank = 0; rank < oracle::STATES; ++rank) {
        const uint32_t key = dense_key(rank);
        const uint32_t *entry = candidate::search_table(key);
        require((entry != nullptr) == (graph.distance[rank] <= 5),
                "sparse membership", rank);
        const unsigned h = entry ? ((*entry >> 4) & 15) : 6;
        require(h <= graph.distance[rank], "heuristic admissibility", rank);
        oracle::state_t oracle_state;
        oracle::unrank_state(rank, &oracle_state);
        require(oracle::valid(&oracle_state) && oracle::rank_state(&oracle_state) == rank,
                "oracle rank round trip", rank);
        candidate::state_t state;
        candidate::unrank_state(
            {uint16_t(rank / oracle::ORIENTATIONS), uint16_t(rank % oracle::ORIENTATIONS)},
            &state);
        require(!memcmp(&state, &oracle_state, sizeof state),
                "candidate unrank agrees with oracle", rank);
        const auto back = candidate::rank_state(&state);
        require(back.p == rank / oracle::ORIENTATIONS &&
                    back.o == rank % oracle::ORIENTATIONS,
                "candidate rank round trip", rank);
    }
    printf("H1 PASS: actual sparse h=exact depth at <=5, otherwise 6, "
           "admissible on all 3674160 states; no perm/orient PDB used\n");
    printf("H4 rank PASS: all 3674160 oracle/candidate rank round trips "
           "and state reconstructions agree\n");
    check_assembly_data(assembly);
    printf("Exhaustive table/heuristic/encoding wall=%.3fs\n", elapsed(start));

    std::vector<uint32_t> inputs;
    if (!full) {
        // Cover every optimal depth, then add reproducible, distinct LCG samples.
        std::array<bool, 12> depth_seen{};
        std::vector<uint8_t> selected(oracle::STATES, 0);
        for (uint32_t rank = 0; rank < oracle::STATES; ++rank)
            if (!depth_seen[graph.distance[rank]]) {
                depth_seen[graph.distance[rank]] = true;
                selected[rank] = 1;
                inputs.push_back(rank);
            }
        uint32_t rng = 1127;
        while (inputs.size() < sample) {
            rng = rng * 1664525U + 1013904223U;
            const uint32_t rank = rng % oracle::STATES;
            if (!selected[rank]) {
                selected[rank] = 1;
                inputs.push_back(rank);
            }
        }
    }
    const unsigned limit = full ? oracle::STATES : sample;
    const auto search_start = Clock::now();
    std::array<uint32_t, 12> tested{};
    for (unsigned i = 0; i < limit; ++i) {
        const uint32_t rank = full ? i : inputs[i];
        uint8_t length = 255;
        require(candidate::ida_star(
                    {uint16_t(rank / oracle::ORIENTATIONS),
                     uint16_t(rank % oracle::ORIENTATIONS)}, &length),
                "ida_star returns solution", rank);
        require(length == graph.distance[rank], "ida_star optimal length", rank);
        oracle::state_t state;
        oracle::unrank_state(rank, &state);
        for (unsigned j = 0; j < length; ++j) {
            require(candidate::solution[j] < oracle::MOVES, "ida_star move range", rank);
            state = oracle::apply_move(state, candidate::solution[j]);
        }
        require(oracle::rank_state(&state) == 0, "ida_star oracle path replay", rank);
        ++tested[length];
        if (full && (i + 1) % 10000 == 0)
            printf("H3 progress checked=%u/%u search wall=%.3fs\n", i + 1, limit,
                   elapsed(search_start));
    }
    printf("H3 %s: checked=%u/%u actual ida_star paths, optimal lengths and "
           "solver.c replay; search wall=%.3fs total wall=%.3fs\n",
           full || sample == oracle::STATES ? "PASS ALL" : "PARTIAL",
           limit, oracle::STATES, elapsed(search_start), elapsed(start));
    printf("H3 sampled counts depth 0..11:");
    for (auto count : tested) printf(" %u", count);
    putchar('\n');
}
