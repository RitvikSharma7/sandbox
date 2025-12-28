#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libmemdrv.h"
#include "fs.h"

static char buf[MEMDRV_BLOCK_SIZE] = {0};
int main() {
    open_device();
    for (int i = 0; i < MEMDRV_NUM_BLOCKS; i++) {
        write_block(i, buf);
    }
    printf("memdrv cleared: all blocks zeroed.\n");
    close_device();
    return EXIT_SUCCESS;
}
