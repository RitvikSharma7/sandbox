/* retrieve_file - retrieves file given by command line from our memdrv */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include "libmemdrv.h"
#include "fs.h"

static char buf[MEMDRV_BLOCK_SIZE];
static char nbuf[MEMDRV_BLOCK_SIZE];

int main(int argc, char* argv[]){
    int fd;
    char ans;
    if(argc == 1){
        fd = 1;
    } else if (argc == 2) {
        if (access(argv[1], F_OK) != -1) {
            printf("File exists, do you want to overwrite %s (Y/N)? ", argv[1]);
            do {
                scanf(" %c", &ans); 
            } while (ans != 'Y' && ans != 'N' && ans != 'y' && ans != 'n');

            if (ans != 'Y' && ans != 'y') {
                printf("Skipped %s\n", argv[1]);
                exit(EXIT_SUCCESS);
            }

        }
        fd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0){
            perror("open");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Error with operation. Please specify 1 file or no file.");
        exit(EXIT_FAILURE);
    }


    Inode *inode = malloc(MEMDRV_BLOCK_SIZE);
    if(!inode){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
	open_device();
	read_block(0, (char *)inode);
	ssize_t bytes_written;
	for (int i = 0; i < MEMDRV_NDIRECT; i++) {
        if (inode->addrs[i] != 0){
		    read_block(inode->addrs[i], buf);
            bytes_written = write(fd,buf,MEMDRV_BLOCK_SIZE);
            if (bytes_written == -1) {
                perror("write");
                if (fd != 1){close(fd);}
                return EXIT_FAILURE;
            }       
        }
	}
    
    if (inode->addrs[MEMDRV_NDIRECT] != 0) {
        read_block(inode->addrs[MEMDRV_NDIRECT], nbuf);
        for (int j = 0; j < MEMDRV_BLOCK_SIZE; j++) {
            if (nbuf[j] != 0){
                read_block(nbuf[j], buf);
                bytes_written = write(fd,buf,MEMDRV_BLOCK_SIZE);
                if (bytes_written == -1) {
                    perror("write");
                    if (fd != 1){close(fd);}
                    return EXIT_FAILURE;
                } 
            }
        }
    } 

    close_device();
    free(inode);
    if (fd != 1){
        close(fd);
    }
    return EXIT_SUCCESS;
}
