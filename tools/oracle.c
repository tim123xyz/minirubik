/* The user's oracle uses static buffers; suppress its legacy free calls only
 * in this host test translation unit, without modifying solver.c. */
#include <stdlib.h>
#define free(pointer) ((void)(pointer))
#include "../solver.c"
