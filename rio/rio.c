#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 0x1000 // 4096 bytes
typedef struct fdata *rio_t; // Opaque type

// File metadata + for internal buffer
struct fdata
{
    int fd;
    char rbuf[BUFFER_SIZE];
    size_t cursor_pos;
    size_t unread_bytes;
};

/**
 * Open requested file using system open call 
 * To open process std file streams use:
 * "/dev/stdin" -> standard input
 * "/dev/stdout" -> standard output
 * "/dev/stderr" -> standard error
 * Returns rio_t struct which has file data information
 */
rio_t rio_open(const char *pathname, int flags, mode_t mode)
{
    rio_t finfo = malloc(sizeof(struct fdata));
    int df;
    if (!finfo)
    {
        perror("malloc");
        return NULL;
    }
    memset(finfo,0,sizeof(struct fdata)); // zero out for safety
    if (flags & O_CREAT)
    {
        df = open(pathname,flags,mode);
        if (df < 0)
        {
            perror("open");
            free(finfo);
            return NULL;
        }
        finfo->fd = df;
    } else
    {
        df = open(pathname,flags,0);
        if (df < 0)
        {
            perror("open");
            return NULL;
        }
        finfo->fd = df;
    }
    return finfo;
}

/**
 * Close file by passing rio_t struct that has file metadata
 * If file is not closed, struct is intact and can be used to retry operation
 */
void rio_close(rio_t finfo)
{
    if (!finfo)
    {
        return;

    }
    int df = finfo->fd;
    int c = close(df);
    if (c < 0)
    {
        perror("close");
        return;
    }

    free(finfo);

}

/**
 * Move read/write file pointer by specfic offset by using relative postion using lseek
 * Returns offset from start.
 */
off_t rio_seek(rio_t finfo, off_t offset, int whence)
{
    if (!finfo)
    {
        return (off_t)-1;
    }
    off_t res = lseek(finfo->fd,offset,whence);
    if (res < 0)
    {
        perror("seek");
        return (off_t)-1;
    }

    finfo->cursor_pos = 0;
    finfo->unread_bytes = 0;
    return res;

}

/**
 * Print state of buffer for debugging
 */
void rio_dump_state(rio_t finfo)
{
    if (!finfo)
    {
        puts("No file metadata found.");
        return;
    }

    puts("--- File Information ---");
    printf("File descriptor: %d\n", finfo->fd);
    printf("Current buffer position: %zu\n", finfo->cursor_pos);
    printf("Unread bytes left in buffer: %zu\n", finfo->unread_bytes);

}

/**
 * Read bytes requested into user buffer (user must make sure buffer is big enough to handle request)
 * This function guarantees all of bytes requested will be read unless an devastating error occurs, fewer on (EOF)
 */
ssize_t rio_readn(rio_t finfo, void *usr_buf, size_t bytes_to_read)
{
    if (!finfo || !usr_buf)
    {
        return -1;
    }

    char *ub = (char *)(usr_buf);
    size_t total_read = 0;
    while(total_read < bytes_to_read)
    {
        ssize_t itr_read = read(finfo->fd, ub + total_read, bytes_to_read - total_read);
        if (itr_read < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("read");
            return -1;
        }

        if (itr_read == 0) // EOF
        {
            break;
        } 

        total_read += itr_read;
    }

    return total_read;
}

/**
 * Write bytes from user buffer (user must make sure buffer is big enough to handle request)
 * This function guarantees all of bytes requested will be wrote unless an devasting error occurs
 */
ssize_t rio_writen(rio_t finfo, const void *usr_buf, size_t bytes_to_write)
{
    if (!finfo || !usr_buf)
    {
        return -1;
    }

    const char *ub = (char *)(usr_buf);
    size_t total_wrote = 0;
    while(total_wrote < bytes_to_write)
    {
        ssize_t itr_wrote = write(finfo->fd, ub + total_wrote, bytes_to_write - total_wrote);
        if (itr_wrote < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("write");
            return -1;
        }

        if (itr_wrote == 0) // Abnormal behaviour, exit with error
        {
            return -1;
        }
        total_wrote += itr_wrote;
    }

    return total_wrote;

}

/**
 * Attempt to read bytes from file and put fill user buffer (use internal buffer when buffer size is respectable w.r.t to constraint)
 * If request is to big, handle it with readn()
 *  Return up to N bytes with minimal syscalls, handling partial reads correctly
 */
ssize_t rio_read(rio_t finfo, void *usr_buf, size_t bytes_to_read)
{
    if (!finfo || !usr_buf)
    {
        return -1;
    }

    char *ub = (char *)(usr_buf);
    ssize_t res;
    if (bytes_to_read > BUFFER_SIZE)
    {
        size_t buffered_bytes = (finfo->unread_bytes > bytes_to_read) ? bytes_to_read : finfo->unread_bytes;
        if (buffered_bytes)
        {
            memcpy(ub,finfo->rbuf + finfo->cursor_pos,buffered_bytes);
            ub += buffered_bytes;
            bytes_to_read -= buffered_bytes;
            finfo->unread_bytes = 0;
            finfo->cursor_pos = 0;
        }

        res = rio_readn(finfo,ub,bytes_to_read);
        if (res < 0)
        {
            return -1;
        }
        return (res + buffered_bytes);
    }

    // Read into internal buffer
    if (finfo->unread_bytes == 0)
    {
    read:
        res = read(finfo->fd, finfo->rbuf, BUFFER_SIZE);
        if (res < 0)
        {
            if (errno == EINTR)
            {
                goto read;
            }
            perror("read");
            return -1;
        }

        if (res == 0)
        {
            return 0;
        }

        finfo->unread_bytes = res;
        finfo->cursor_pos = 0;
    }

    ssize_t bytes_read = (finfo->unread_bytes > bytes_to_read) ? bytes_to_read : finfo->unread_bytes;
    memcpy(usr_buf,finfo->rbuf + finfo->cursor_pos,bytes_read);
    finfo->cursor_pos += bytes_read;
    finfo->unread_bytes -= bytes_read;

    return bytes_read;
}