#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>

#define RIO_BUFSIZE 0x1000

#define RIO_SEEK_SET 0
#define RIO_SEEK_CUR 1
#define RIO_SEEK_END 2

typedef struct rio {
        int     fd;
        size_t  cursor_pos;
        size_t  unread;
        char    rbuf[RIO_BUFSIZE];
} rio_t;

static rio_t fdata = { .fd = -1 };

int rio_openf(const char *file_name, int flags, mode_t mode)
{
        memset(&fdata, 0, sizeof(fdata));
        fdata.fd = -1;

        int fd;
        if (flags & O_CREAT)
                fd = open(file_name, flags, mode);
        else
                fd = open(file_name, flags);

        if (fd < 0)
                return -1;

        fdata.fd = fd;
        return 0;
}

ssize_t rio_read(void *buf, size_t n)
{
        if (fdata.fd < 0)
        {
                errno = EBADF;
                return -1;
        }

        if (n >= RIO_BUFSIZE)
                return rio_readn(buf, n);

        if (fdata.unread == 0)
        {
                ssize_t res;
                do {
                        res = read(fdata.fd, fdata.rbuf, RIO_BUFSIZE);
                } while (res < 0 && errno == EINTR);

                if (res <= 0)
                        return res;

                fdata.unread = res;
                fdata.cursor_pos = 0;
        }

        size_t cnt = n < fdata.unread ? n : fdata.unread;
        memcpy(buf, fdata.rbuf + fdata.cursor_pos, cnt);

        fdata.cursor_pos += cnt;
        fdata.unread     -= cnt;

        return cnt;
}

ssize_t rio_readn(void *buf, size_t n)
{
        if (fdata.fd < 0)
        {
                errno = EBADF;
                return -1;
        }

        size_t total = 0;
        char *p = buf;

        while (total < n)
        {
                ssize_t res = read(fdata.fd, p + total, n - total);

                if (res < 0)
                {
                        if (errno == EINTR)
                                continue;
                        return -1;
                }

                if (res == 0)
                        break;

                total += res;
        }

        return total;
}

ssize_t rio_write(const void *buf, size_t n)
{
        if (fdata.fd < 0)
        {
                errno = EBADF;
                return -1;
        }

        size_t total = 0;
        const char *p = buf;

        while (total < n)
        {
                ssize_t res = write(fdata.fd, p + total, n - total);

                if (res < 0)
                {
                        if (errno == EINTR)
                                continue;
                        return -1;
                }

                if (res == 0)
                        return -1;

                total += res;
        }

        return total;
}

off_t rio_fseek(uint8_t flag, off_t offset)
{
        if (fdata.fd < 0)
        {
                errno = EBADF;
                return -1;
        }

        int whence;
        switch (flag)
        {
                case RIO_SEEK_SET: whence = SEEK_SET; break;
                case RIO_SEEK_CUR: whence = SEEK_CUR; break;
                case RIO_SEEK_END: whence = SEEK_END; break;
                default:
                        errno = EINVAL;
                        return -1;
        }

        off_t res = lseek(fdata.fd, offset, whence);
        if (res == -1)
                return -1;

        fdata.cursor_pos = 0;
        fdata.unread = 0;

        return res;
}

void fdata_dump()
{
        if (fdata.fd < 0)
        {
                fprintf(stderr, "No file opened!");
                return;
        }

        puts("--- File Information ---");
        printf("FD #: %d\n", fdata.fd);
        printf("Position of cursor in buffer: %zu\n", fdata.cursor_pos);
        printf("Unread bytes left in buffer: %zu\n", fdata.unread);
}
