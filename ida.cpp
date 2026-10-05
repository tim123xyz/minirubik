#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>
#include <algorithm>
#include <errno.h>
#include <stddef.h>

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9
};

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

typedef struct {
    uint16_t p, o;
} rank_t;

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};
static constexpr uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};
static constexpr uint8_t move_face[MOVES] = {0, 0, 0, 1, 1, 1, 2, 2, 2};
static constexpr uint8_t move_turns[MOVES] = {1, 2, 3, 1, 2, 3, 1, 2, 3};
static constexpr uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static constexpr uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

static constexpr state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result{};
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

static constexpr rank_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    return {.p = (uint16_t) p, .o = (uint16_t) o};
}

static constexpr void unrank_state(rank_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank.p, o = rank.o, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < unsigned(CUBIES - i); ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
}

static int valid(const state_t *state)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;
        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;
        sum = (uint8_t) (sum + state->o[i]);
    }
    return sum % 3U == 0;
}

static consteval std::array<std::array<uint16_t,PERMUTATIONS>,3> build_permutations()
{
    std::array<std::array<uint16_t,PERMUTATIONS>,3> permutation;
    for (uint16_t p = 0; p < PERMUTATIONS; ++p) {
        state_t state;
        unrank_state({.p = p, .o = 0}, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            permutation[face][p] = rank_state(&next).p;
        }
    }
    return permutation;
}

static consteval std::array<std::array<uint16_t,ORIENTATIONS>,3> build_orientation()
{
    std::array<std::array<uint16_t,ORIENTATIONS>,3> orientation;
    for (uint16_t o = 0; o < ORIENTATIONS; ++o) {
        state_t state;
        unrank_state({.p = 0, .o = o}, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            orientation[face][o] = rank_state(&next).o;
        }
    }
    return orientation;
}

constexpr std::array<std::array<uint16_t,PERMUTATIONS>,3> permutation = build_permutations();
constexpr std::array<std::array<uint16_t,ORIENTATIONS>,3> orientation = build_orientation();

static inline uint8_t get_table(const uint8_t *table, uint32_t rank)
{
    return table[rank];
}

static inline void set_table(uint8_t *table, uint32_t rank, uint8_t depth)
{
    table[rank] = depth;
}

enum {
    BFS_DEPTH4 = 2232,
    BFS_DEPTH5 = 12224,
    BFS_DEPTH6 = 62360,
    MAX_SOLUTION = 11
};

static consteval std::array<uint32_t,BFS_DEPTH5> build_table()
{
    std::array<uint8_t,(STATES + 7) / 8> seen{};
    std::array<uint32_t,BFS_DEPTH5> queue{};
    queue[0] = 0;
    seen[0] = 1;

    uint32_t *head = queue.data(), *tail = head + 1;
    uint32_t *end = head + BFS_DEPTH4;
    while(head < end) {
        uint32_t now = *head++;
        uint32_t dense = now >> 8, depth = now & 0xF0u;
        uint16_t p = dense % PERMUTATIONS, o = dense / PERMUTATIONS;
        for(uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p, next_o = o;
            for(uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];
                uint32_t dense = next_p + (uint32_t) next_o * PERMUTATIONS;
                uint8_t mask = 1U << (dense & 7);
                if (!(seen[dense >> 3] & mask)) {
                    seen[dense >> 3] |= mask;
                    if (tail == queue.data() + queue.size());
                    *tail = dense << 8 | (depth+16) | inverse_move[face*3 + turn];
                    ++tail;
                }
            }
        }
    }
    if (tail != queue.data() + queue.size());
    std::sort(queue.begin(), queue.end());
    return queue;
}

static constexpr auto table = build_table();

static const uint32_t *search_table(uint32_t dense)
{
    dense <<= 8;
    const uint32_t *lo = table.data(), *hi = lo + table.size();
    while (lo < hi) {
        const uint32_t *mid = lo + (hi - lo) / 2;
        if (*mid < dense)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo != table.data() + table.size() && (*lo & 0xFFFFFF00u) == dense ? lo : nullptr;
}

static uint8_t solution[MAX_SOLUTION];

static bool append_table_solution(rank_t rank, uint8_t prefix_length,
                                  uint8_t *length)
{
    uint8_t position = prefix_length;
    while (true) {
        const uint32_t dense = rank.p + (uint32_t) rank.o * PERMUTATIONS;
        const uint32_t *entry = search_table(dense);
        if (!entry)
            return false;

        if ((*entry & 0xF0u) == 0) {
            if (rank.p != 0 || rank.o != 0)
                return false;
            *length = position;
            return true;
        }

        const uint8_t move = *entry & 15;
        if (move >= MOVES || position >= MAX_SOLUTION)
            return false;
        solution[position++] = move;

        const uint8_t face = move_face[move];
        const uint8_t turns = move_turns[move];
        for (uint8_t turn = 0; turn < turns; ++turn) {
            rank.p = permutation[face][rank.p];
            rank.o = orientation[face][rank.o];
        }
    }
}

static bool ida_star(rank_t target, uint8_t *length)
{
    struct frame_t {
        rank_t rank;
        uint8_t next_move;
        uint8_t face;
        uint8_t turns;
        uint8_t previous_face;
    };
    frame_t stack[MAX_SOLUTION + 1];
    const uint32_t dense = target.p + (uint32_t) target.o * PERMUTATIONS;
    const uint32_t *entry = search_table(dense);
    unsigned threshold = entry ? ((*entry >> 4) & 15) : 6;
    *length = 0;
    while (threshold <= MAX_SOLUTION) {
        unsigned next_threshold = MAX_SOLUTION + 1;
        stack[0] = {target, 0, 0, 1, 3};
        uint8_t depth = 0;

        while (true) {
            frame_t &frame = stack[depth];

            if (frame.next_move == 0) {
                const rank_t rank = frame.rank;
                const uint32_t dense = rank.p + (uint32_t) rank.o * PERMUTATIONS;
                const uint32_t *entry = search_table(dense);
                const unsigned heuristic = entry ? ((*entry >> 4) & 15) : 6;
                const unsigned cost = depth + heuristic;
                if (cost > threshold) {
                    next_threshold = std::min(next_threshold, cost);
                    if (depth == 0)
                        break;
                    --depth;
                    continue;
                }
                if (entry) {
                    return append_table_solution(frame.rank, depth, length);
                }
            }

            if (depth == MAX_SOLUTION || frame.next_move == MOVES) {
                if (depth == 0)
                    break;
                --depth;
                continue;
            }

            const uint8_t face = frame.face;
            if (face == frame.previous_face) {
                frame.next_move += 3;
                ++frame.face;
                continue;
            }

            const uint8_t move = frame.next_move++;
            const uint8_t turns = frame.turns;
            if (++frame.turns == 4) {
                frame.turns = 1;
                ++frame.face;
            }
            solution[depth] = move;
            rank_t child = frame.rank;
            for (uint8_t turn = 0; turn < turns; ++turn) {
                child.p = permutation[face][child.p];
                child.o = orientation[face][child.o];
            }
            ++depth;
            stack[depth] = {child, 0, 0, 1, face};
        }
        threshold = next_threshold;
    }
    return false;
}

static int parse_state(const char *input, state_t *state)
{
    if (strlen(input) != 14)
        return 0;
    for (int i = 0; i < 14; ++i) {
        int limit = i < 7 ? 7 : 3;
        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;
        (i < 7 ? state->p : state->o)[i % 7] = (uint8_t) (input[i] - '1');
    }
    return input[14] == '\0' && valid(state);
}

#ifndef IDA_NO_MAIN
int main(int argc, char **argv)
{
    const char *input = argc == 2 ? argv[1] : "21345671111111";
    state_t state{};
    if (argc > 2 || !parse_state(input, &state)) {
        fputs("invalid state\n", stderr);
        return 2;
    }
    uint8_t length = 0;
    if (!ida_star(rank_state(&state), &length)) {
        fputs("could not find solution\n", stderr);
        return 1;
    }
    for (uint8_t i = 0; i < length; ++i)
        printf("%s%s", i ? " " : "", move_names[solution[i]]);
    putchar('\n');
    return fflush(stdout) == 0 ? 0 : 1;
}
#endif
