/* store_file - stores file given by command line (argv[1] or argv[2]) into our memdrv */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include "libmemdrv.h"
#include "fs.h"

static char buf[MEMDRV_BLOCK_SIZE] = {0};
static uint8_t free_list[MEMDRV_NUM_BLOCKS];

void shuffle(uint8_t *array, int n) {
    for (int i = 0; i < n - 1; i++) {
        int j = i + rand() / (RAND_MAX / (n - i) + 1);
        uint8_t t = array[j];
        array[j] = array[i];
        array[i] = t;
    }
}



int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Please give the filename of 1 file with the optional command line argument (-r)!\n");
        exit(EXIT_FAILURE);
    }

    open_device();

    if (argc == 2) {      
        int fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            perror("file");
            close_device();
            exit(EXIT_FAILURE);
        }
        off_t filesize = lseek(fd, 0, SEEK_END);  
        lseek(fd, 0, SEEK_SET);

        if (filesize > (MEMDRV_NUM_BLOCKS -2) * MEMDRV_BLOCK_SIZE) {
            fprintf(stderr, "File truncated\n");
        }

        Inode inode = {0};
        inode.size = (filesize > (MEMDRV_NUM_BLOCKS -2) * MEMDRV_BLOCK_SIZE) ? (MEMDRV_NUM_BLOCKS -2) * MEMDRV_BLOCK_SIZE: filesize;

        int addr_index = 0;
        int block_page = 1;
        unsigned int total_bytes = 0;

        uint8_t indirect_block[MEMDRV_BLOCK_SIZE] = {0};
        int indirect_block_counter = 0;
        int indirect_block_num = 0;

        while (total_bytes < inode.size) {
            ssize_t bytes_read = read(fd, buf, MEMDRV_BLOCK_SIZE);
            if (bytes_read < 0) {
                perror("read");
                close(fd);
                close_device();
                exit(EXIT_FAILURE);
            }
            if (bytes_read == 0) break;

            if (bytes_read < MEMDRV_BLOCK_SIZE) {
                memset(buf + bytes_read, 0, MEMDRV_BLOCK_SIZE - bytes_read);
            }

            
            if (addr_index < MEMDRV_NDIRECT) {
                inode.addrs[addr_index] = block_page;
            } else {
                if (indirect_block_num == 0) {
                    indirect_block_num = block_page;   
                    inode.addrs[MEMDRV_NDIRECT] = indirect_block_num;
                }
                indirect_block[indirect_block_counter] = block_page;
                indirect_block_counter++;
            }

                write_block(block_page, buf);
                addr_index++;
                block_page++;
                total_bytes += bytes_read;
            }

            if (indirect_block_num != 0) {
                write_block(indirect_block_num, (char*)indirect_block);
            }

            write_block(0, (char*)&inode);
            close(fd);
        }

    if (argc == 3) {
        if (strcmp(argv[1], "-r") != 0) {
            fprintf(stderr, "Invalid command line argument! Did you mean -r?\n");
            exit(EXIT_FAILURE);
        }

        // Initialize free list 0..79
        for (int i = 0; i < MEMDRV_NUM_BLOCKS; i++) {
            free_list[i] = i;
        }

        // Shuffle all blocks except 0 (inode is always block 0)
        srand(time(NULL));
        shuffle(free_list + 1, MEMDRV_NUM_BLOCKS - 1);

        // Open input file
        int fd = open(argv[2], O_RDONLY);
        if (fd < 0) {
            perror("file");
            close_device();
            exit(EXIT_FAILURE);
        }

        // Get file size
        off_t filesize = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

        if (filesize > (MEMDRV_NUM_BLOCKS -2) * MEMDRV_BLOCK_SIZE) {
            fprintf(stderr, "File truncated\n");
        }

        Inode inode = {0};
        inode.size = (filesize > (MEMDRV_NUM_BLOCKS -2) * MEMDRV_BLOCK_SIZE) ? (MEMDRV_NUM_BLOCKS -2) * MEMDRV_BLOCK_SIZE : filesize;

        int addr_index = 0;
        unsigned int total_bytes = 0;

        uint8_t indirect_block[MEMDRV_BLOCK_SIZE] = {0};
        int indirect_block_counter = 0;
        int indirect_block_num = 0;

        int next_free = 1; // free_list[0] reserved for inode

        // Read and store file data
        while (total_bytes < inode.size) {
            ssize_t bytes_read = read(fd, buf, MEMDRV_BLOCK_SIZE);
            if (bytes_read < 0) {
                perror("read");
                close(fd);
                close_device();
                exit(EXIT_FAILURE);
            }
            if (bytes_read == 0) break;

            // Pad last block
            if (bytes_read < MEMDRV_BLOCK_SIZE) {
                memset(buf + bytes_read, 0, MEMDRV_BLOCK_SIZE - bytes_read);
            }

            if (addr_index < MEMDRV_NDIRECT) {
                // Direct block
                uint8_t block = free_list[next_free];
                next_free++;
                inode.addrs[addr_index] = block;
                write_block(block, buf);
            } else {
                // Indirect blocks
                if (indirect_block_num == 0) {
                    indirect_block_num = free_list[next_free++];
                    inode.addrs[MEMDRV_NDIRECT] = indirect_block_num;
                }
                uint8_t block = free_list[next_free];
                next_free++;
                indirect_block[indirect_block_counter] = block;
                indirect_block_counter++;
                write_block(block, buf);
            }

            addr_index++;
            total_bytes += bytes_read;
        }

        // Write indirect block if used
        if (indirect_block_num != 0) {
            write_block(indirect_block_num, (char*)indirect_block);
        }

        // Write inode at block 0
        write_block(0, (char*)&inode);

        close(fd);
    }

        return EXIT_SUCCESS;
}
