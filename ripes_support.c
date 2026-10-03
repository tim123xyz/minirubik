#include <stddef.h>
#include <stdint.h>
#include <errno.h>

static unsigned char heap[20u * 1024u * 1024u] __attribute__((aligned(16)));
static size_t used;

void *_sbrk(ptrdiff_t increment)
{
    size_t old = used;
    if (increment >= 0) {
        if ((size_t)increment > sizeof heap - used) {
            errno = ENOMEM;
            return (void *)-1;
        }
        used += (size_t)increment;
    } else {
        size_t decrease = (size_t)(-(increment + 1)) + 1;
        if (decrease > used) {
            errno = ENOMEM;
            return (void *)-1;
        }
        used -= decrease;
    }
    return heap + old;
}

int solver_main(int argc, char **argv);
int main(void)
{
    char program[] = "solver";
    char input[] = "21345671111111";
    char *argv[] = {program, input, 0};
    return solver_main(2, argv);
}