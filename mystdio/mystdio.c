#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#define BUFFER_SIZE 4096 // Size of read/write buffer
typedef MY_FILE* file_t;

typedef enum 
{
    UNBUFFERED,
    LINE_BUFFERED,
    FULLY_BUFFERED
} BUFFER_MODE;

typedef enum
{
    READ,
    WRITE
} IO_MODE;

typedef struct MY_FILE
{
    int fd;
    char *buffer;
    size_t bytes_in_buf;

    BUFFER_MODE bmode;
    IO_MODE imode;

    int err;
    int eof;
} MY_FILE;

int mode_to_flags(const char *mode, IO_MODE *m)
{
    if (strcmp(mode, "r") == 0) {
        *m = READ;
        return O_RDONLY;
    }

    if (strcmp(mode, "w") == 0) {
        *m = WRITE;
        return O_WRONLY | O_CREAT | O_TRUNC;
    }

    if (strcmp(mode, "a") == 0) {
        *m = WRITE;
        return O_WRONLY | O_CREAT | O_APPEND;
    }

    return -1;  // unsupported 
}


file_t my_fdopen(int fdes, IO_MODE mode)
{
    if (fdes < 0 || mode != READ || mode != WRITE)
    {
        return NULL;
    }

    int fflags = fcntl(fdes , F_GETFL);
    if (fflags == -1)
    {
        return NULL;
    }

    int acc = fflags & O_ACCMODE;

    if (mode == READ && acc == O_WRONLY)
    {
        return NULL;
    }
    if (mode == WRITE && acc == O_RDONLY)
    {
        return NULL;
    }

    file_t f = malloc(sizeof(MY_FILE));
    if (!f)
    {
        return NULL;
    }

    memset(f,0,sizeof(MY_FILE));
    f->fd = fdes;
    f->imode = mode;

    if (f->fd == STDERR_FILENO)
    {
        f->bmode = UNBUFFERED;
    } else if (isatty(f->fd))
    {
        f->bmode = LINE_BUFFERED;
        if (!f->buffer)
        {
            f->buffer = malloc(sizeof(char) * BUFFER_SIZE);
            if (!f->buffer)
            {
                free(f);
                return -1;
            }
        }
    } else
    {
        f->bmode = FULLY_BUFFERED;
        if (!f->buffer)
        {
            f->buffer = malloc(sizeof(char) * BUFFER_SIZE);
            if (!f->buffer)
            {
                free(f);
                return -1;
            }
        }
    }

    return f;
}

file_t my_fopen(const char *fname, const char *fmode)
{
    IO_MODE m;
    int flags = mode_to_flags(fmode, &m);
    if (flags == -1)
    {
        return NULL;
    }

    int fdes = open(fname,flags,0644);
    if (fdes < 0)
    {
        return NULL;
    }
    file_t f = my_fdopen(fdes,m);
    if (!f)
    {
        close(fdes);
        return NULL;
    }
    return f;
}

ssize_t my_fflush(file_t f)
{
    if (!f) 
    {
        errno = EINVAL;
        return -1;
        
    }

    if (f->bmode == UNBUFFERED || f->imode == READ || f->bytes_in_buff == 0)
    {
        return 0;
    }

    ssize_t written = 0;
    ssize_t bytes_to_write = f->bytes_in_buff;
    while (written < bytes_to_write)
    {
        ssize_t w = write(f->fd,f->buffer + written,bytes_to_write-written);
        if (w < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            f->err = errno;
            return -1;
        }

        if (w == 0) // Undefined behaviour, exit with error
        {
            f->err = EIO;
            return -1;
        }

        written += w;
    }

    f->bytes_in_buff = 0;
    return 0;
    
}

int my_putc(char c, file_t f)
{
    if (!f || f->imode == READ) {
        errno = EINVAL;
        return -1;
    }

    if (f->bmode == UNBUFFERED) {
        ssize_t w = write(f->fd, &c, 1);
        if (w < 0) {
            f->err = errno;
            return -1;
        }
        if (w == 0) {
            f->err = EIO;
            return -1;
        }
        return (unsigned char)c;
    }

    // Buffered modes 

    if (f->bytes_in_buf == BUFFER_SIZE) {
        if (my_fflush(f) < 0)
            return -1;
    }

    f->buffer[f->bytes_in_buf++] = c;

    if (f->bmode == LINE_BUFFERED && c == '\n') {
        if (my_fflush(f) < 0)
            return -1;
    }

    return (unsigned char)c;
}

int my_puts(const char *s, file_t f)
{

    if (!f || !s || f->imode == READ) {
        errno = EINVAL;
        return -1;
    }

    const char *i = s;
    int res;
    while (*i != '\0')
    {
        res = my_putc(*i,f);
        if (res == -1)
        {
            return -1;
        }
        i++;
    }
    res = my_putc('\n',f);
    if (res == -1)
    {
        return -1;
    }
    return 0;
}

int my_fclose(file_t f)
{
    if (!f) {
        errno = EINVAL;
        return -1;
    }

    if (f->imode == WRITE)
        my_fflush(f);

    int rc = close(f->fd);
    int saved_errno = errno;

    free(f->buffer);
    free(f);

    if (rc == -1) {
        errno = saved_errno;
        return -1;
    }

    return 0;
}

int my_printf(file_t f, const char *fmt, ...)
{
    if (!f || !fmt)
    {
        errno = EINVAL;
        return -1;
    }

    int count = 0;
    const char *s = fmt;

    va_list args;
    va_start(args,fmt);


    while (*s)
    {
        if (*s != '%')
        {
            if (my_putc(*s,f) < 0)
            {
                goto error;
            }
            count++;
            s++;
            continue;
        }

        s++;
        if (!*s)
        {
            break;
        }

        switch (*s)
        {
            case '%': 
                if (my_putc(*s,f) < 0)
                {
                    goto error;
                }
                count++;
                break;

            case 'c':
            {
                char c = (char)va_arg(args,int);
                if (my_putc(c,f) < 0)
                {
                    goto error;
                }
                count++;
                break;
            }

            case 's':
            {
                const char *str = (const char*)va_arg(args,const char*);
                if (!str)
                {
                    str = "NULL";
                }   
                while (*s)
                {
                    if (my_putc(*str,f) < 0)
                    {
                        goto error;
                    }
                    count++;
                }
                break;              
            }

            case 'd':
            {
                int i = va_arg(args,int);
                char temp[32];
                const char *str = int_to_string(i,temp);   
                while (*str)
                {
                    if (my_putc(*str,f) < 0)
                    {
                        goto error;
                    }
                    count++;
                }
                break;   

            }

            default:
                if (my_putc('%', f) < 0 || my_putc(*s, f) < 0)
                {
                    goto error;
                }
                count += 2;

        }

        s++;
    }

    va_end(args);
    return count;

error:
    va_end(args);
    return -1;
}

char* int_to_string(int d, char *s)
{
    char* start = s;
    int i = 0;
    int sign = d;

    if ( d < 0)
    {
        d = -d;
    }

    if (d == 0) 
    {
        s[i++] = '0';
        s[i] = '\0';
        return s;
    }

    while (d > 0)
    {
        s[i++] = (d % 10) + '0';
        d /= 10;
    }

     if (sign < 0)
    {
        s[i++] = '-';
    }

    s[i] = '\0';

    for (int j = 0, k = i - 1; j < k; j++, k--)
    {
        char temp = s[j];
        s[j] = s[k];
        s[k] = temp;
    }

    return start;
}