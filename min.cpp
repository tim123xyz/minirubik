#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>
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
static const uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

static rank_t rank_state(const state_t *state)
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

static void unrank_state(rank_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank.p, o = rank.o, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < CUBIES - i; ++j)
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

static rank_t _quarter_turn(rank_t rank, uint8_t face)
{
    return {.p = permutation[face][rank.p], .o = orientation[face][rank.o]};
}

static rank_t apply_move(rank_t rank, uint8_t move)
{
    uint8_t turns = (uint8_t) (move % 3U + 1U);
    for (uint8_t i = 0; i < turns; ++i)
        rank = _quarter_turn(rank, (uint8_t) (move / 3U));
    return rank;
}

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
    TARGET_SIDE = 0x08,
    MAX_SOLUTION = 11
};

static uint8_t *build_table(rank_t target, rank_t meeting[2], uint8_t *bridge)
{
    static uint8_t table[STATES];
    static rank_t queue[BFS_DEPTH5 + BFS_DEPTH6];
    memset(table, UINT8_MAX, STATES);
    queue[0] = {0, 0};
    queue[1] = target;
    set_table(table, 0, 0);
    set_table(table, target.p + (uint32_t) target.o * PERMUTATIONS, TARGET_SIDE);
    table[0] = 0;
    *bridge = UINT8_MAX;
    if (target.p == 0 && target.o == 0) {
        meeting[0] = meeting[1] = target;
        return table;
    }

    rank_t *head = queue, *tail = queue+2;
    rank_t *end = queue + BFS_DEPTH5 + BFS_DEPTH4;
    while(head < end) {
        rank_t now = *head++;
        uint8_t depth = get_table(table, now.p + (uint32_t) now.o * PERMUTATIONS);
        for(uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = now.p, next_o = now.o;
            for(uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];
                uint32_t dense = next_p + (uint32_t) next_o * PERMUTATIONS;
                uint8_t next_depth = get_table(table, dense);
                if(next_depth == 0xFF) {
                    set_table(table, dense, depth + 1);
                    *tail = {.p = next_p, .o = next_o};
                    ++tail;
                }
                else if (((next_depth ^ depth) & TARGET_SIDE) != 0) {
                    uint8_t side = (depth & TARGET_SIDE) != 0 ? 0 : 1;
                    uint8_t move = face * 3U + turn;
                    meeting[side] = now;
                    meeting[1U - side] = {.p = next_p, .o = next_o};
                    *bridge = side == 0 ? move : inverse_move[move];
                    return table;
                }
            }
        }
    }
    return nullptr;
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

static uint8_t find_move(const uint8_t *table, rank_t rank)
{
    uint16_t p = rank.p, o = rank.o;
    uint8_t depth = get_table(table, (uint32_t) p + o * PERMUTATIONS);
    if ((depth & ~TARGET_SIDE) == 0 || depth == UINT8_MAX)
        return UINT8_MAX;
    uint8_t target = depth - 1;
    for (uint8_t face = 0; face < 3; ++face) {
        uint16_t next_p = p, next_o = o;
        for(uint8_t turn = 0; turn < 3; ++turn) {
            next_p = permutation[face][next_p];
            next_o = orientation[face][next_o];
            if (get_table(table, (uint32_t) next_p + next_o * PERMUTATIONS) == target) {
                return face * 3U + turn;
            }
        }
    }
    return UINT8_MAX;
}

int main(int argc, char **argv)
{
    const char *input = argc == 2 ? argv[1] : "21345671111111";
    state_t state;
    if (argc > 2 || !parse_state(input, &state)) {
        fputs("invalid state\n", stderr);
        return 2;
    }
    rank_t target = rank_state(&state), meeting[2];
    uint8_t bridge;
    uint8_t *table = build_table(target, meeting, &bridge);
    if (!table) {
        fputs("could not build state table\n", stderr);
        return 1;
    }

    uint8_t solution[MAX_SOLUTION], length = 0;
    rank_t rank = meeting[0];
    while (rank.p != target.p || rank.o != target.o) {
        uint8_t move = find_move(table, rank);
        if (move == UINT8_MAX || length == MAX_SOLUTION)
            goto invalid_path;
        solution[length++] = inverse_move[move];
        rank = apply_move(rank, move);
    }
    for (uint8_t i = 0; i < length / 2U; ++i) {
        uint8_t move = solution[i];
        solution[i] = solution[length - 1U - i];
        solution[length - 1U - i] = move;
    }
    if (bridge != UINT8_MAX) {
        if (length == MAX_SOLUTION)
            goto invalid_path;
        solution[length++] = bridge;
    }
    rank = meeting[1];
    while (rank.p != 0 || rank.o != 0) {
        uint8_t move = find_move(table, rank);
        if (move == UINT8_MAX || length == MAX_SOLUTION)
            goto invalid_path;
        solution[length++] = move;
        rank = apply_move(rank, move);
    }
    for (uint8_t i = 0; i < length; ++i)
        printf("%s%s", i ? " " : "", move_names[solution[i]]);
    putchar('\n');
    return fflush(stdout) == 0 ? 0 : 1;

invalid_path:
    fputs("could not follow state table\n", stderr);
    return 1;
}
