// Load min.cpp's standard headers before the narrowly scoped keyword macros.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>
#include <errno.h>
#include <stddef.h>

// min.cpp's consteval builders call non-constexpr functions. This host-only
// wrapper initializes their identical tables at startup, preserving main,
// state validation, move generation, and the complete search implementation.
#define consteval
#define constexpr const
#include "../min.cpp"
#undef constexpr
#undef consteval
