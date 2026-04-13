#include <stdint.h>

extern int32_t bootstrap_main(void);

int main(void) {
    return (int)bootstrap_main();
}