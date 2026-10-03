#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>

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

static std::array<std::array<uint16_t,PERMUTATIONS>,3> build_permutations()
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

static std::array<std::array<uint16_t,ORIENTATIONS>,3> build_orientation()
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

std::array<std::array<uint16_t,PERMUTATIONS>,3> permutation = build_permutations();
std::array<std::array<uint16_t,ORIENTATIONS>,3> orientation = build_orientation();

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

static void dfs(uint8_t *table, rank_t rank, uint32_t dense, uint8_t prev_face, uint8_t depth, uint8_t target) {
    if (get_table(table, dense) > depth) {
        set_table(table, dense, depth);
        if (depth == target) {
            return;
        }
        uint16_t p = rank.p;
        uint16_t o = rank.o;
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p, next_o = o;
            if (face == prev_face) {
                continue;
            }
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];
                uint32_t next_dense = (uint32_t) next_p + next_o * PERMUTATIONS;
                dfs(table, {.p = next_p, .o = next_o}, next_dense, face, depth + 1, target);
            }
        }
    }
    return;
}

static uint8_t *build_table(uint8_t *diameter)
{
    static uint8_t toward_solved[STATES];
    memset(toward_solved, UINT8_MAX, sizeof toward_solved);
    uint8_t depth = 7;
    dfs(toward_solved, {.p = 0, .o = 0}, 0, UINT8_MAX, 0, depth);
    for (; depth < 11; ++depth) {
        uint8_t next_depth = depth + 1;
        for (uint16_t p = 0; p < PERMUTATIONS; ++p) {
            for (uint16_t o = 0; o < ORIENTATIONS; ++o) {
                if (get_table(toward_solved, (uint32_t) p + o * PERMUTATIONS) == depth) {
                    for (uint8_t face = 0; face < 3; ++face) {
                        uint16_t next_p = p, next_o = o;
                        for (uint8_t turn = 0; turn < 3; ++turn) {
                            next_p = permutation[face][next_p];
                            next_o = orientation[face][next_o];
                            uint32_t next_rank = (uint32_t) next_p + next_o * PERMUTATIONS;
                            if (get_table(toward_solved, next_rank) == UINT8_MAX)
                                set_table(toward_solved, next_rank, next_depth);
                        }
                    }
                }
            }
        }
    }
    for (uint32_t rank = 0; rank < STATES; ++rank)
        if (get_table(toward_solved, rank) == 0xFF)
            return NULL;
    *diameter = 11;
    return toward_solved;
}

static int parse_state(const char *input, state_t *state)
{
    for (int i = 0; i < 14; ++i) {
        int limit = i < 7 ? 7 : 3;
        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;
        (i < 7 ? state->p : state->o)[i % 7] = (uint8_t) (input[i] - '1');
    }
    return input[14] == '\0' && valid(state);
}

static int output_failed(void)
{
    return fflush(stdout) != 0 || ferror(stdout);
}

static int self_test(void)
{
    const rank_t solved = {.p = 0, .o = 0};
    rank_t rank;
    for (uint8_t move = 0; move < MOVES; ++move) {
        rank = solved;
        rank = apply_move(rank, move);
        rank = apply_move(rank, inverse_move[move]);
        if (memcmp(&solved, &rank, sizeof solved))
            return 0;
    }
    state_t state;
    for (uint16_t p = 0; p < PERMUTATIONS; ++p) {
        for (uint16_t o = 0; o < ORIENTATIONS; ++o) {
            unrank_state({.p = p, .o = o}, &state);
            rank_t ranked = rank_state(&state);
            if (!valid(&state) || ranked.p != p || ranked.o != o)
                return 0;
        }
    }
    return 1;
}

static uint8_t find_move(const uint8_t *table, rank_t rank)
{
    uint16_t p = rank.p, o = rank.o;
    uint8_t depth = get_table(table, (uint32_t) p + o * PERMUTATIONS);
    if (depth == 0 || depth == 0xFF)
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
    char program[] = "mine";
    char input[] = "21345671111111";
    char *fake_argv[] = {program, input, nullptr};
    argc = 2;
    argv = fake_argv;

    state_t state;
    uint8_t diameter;
    if (argc == 2 && !strcmp(argv[1], "--self-test")) {
        if (!self_test()) {
            fputs("self-test failed\n", stderr);
            return 1;
        }
        uint8_t *table = build_table(&diameter);
        if (!table) {
            fputs("could not build complete state table\n", stderr);
            return 1;
        }
        if (diameter != 11) {
            fputs("BFS check failed\n", stderr);
            return 1;
        }
        puts("3674160 states; diameter 11");
        return output_failed();
    }
    if (argc != 2 || !parse_state(argv[1], &state)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "solver");
        return 2;
    }
    uint8_t *table = build_table(&diameter);
    if (!table) {
        fputs("could not build complete state table\n", stderr);
        return 1;
    }
    const char *separator = "";
    for (rank_t rank = rank_state(&state); rank.p != 0 || rank.o != 0;) {
        uint8_t move = find_move(table, rank);
        if (move == UINT8_MAX) {
            fputs("could not follow state table\n", stderr);
            return 1;
        }
        printf("%s%s", separator, move_names[move]);
        separator = " ";
        rank = apply_move(rank, move);
    }
    putchar('\n');
    return output_failed();
}
