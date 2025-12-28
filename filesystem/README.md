Using a virtual RAM DISK we mimic how a single file is stored and retrieved. The memdrv is not implemented, but as the paramters as follows:
/* memdrv.h - defines parameters of the memdrv device */
#pragma once
#define MEMDRV_BLOCK_SIZE 64
#define MEMDRV_NUM_BLOCKS 80
#define MEMDRV_DEVICE_SIZE ((MEMDRV_NUM_BLOCKS) * (MEMDRV_BLOCK_SIZE)) #define MEMDRV_NDIRECT 14 Inode strucure is as standard but holds 15 addresses. 14 direct and 1 indirect for this implementation.
