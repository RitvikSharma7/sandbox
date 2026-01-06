#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct kernel_timespec
{
        long tv_secs;
        long tv_nsecs;
}kernel_timespec;

int str_to_int (char *s)
{
        int result = 0;
        int i = 0;
        if (!*s)
        {
                printf("Empty string.\n");
                return -1;
        }

        while (s[i] == ' ' || s[i] == '\n' || s[i] == '\t')
        {
                i++;
        }

        int sign = 1;
        if (s[i] == '-' || s[i] == '+')
        {
                sign = s[i++] == '+' ? 1 : -1;

        }

        while(s[i] >= '0' && s[i] <= '9')
        {
                result = result * 10 + (s[i] - '0');
                i++;
        }

        return (result * sign);
}

int SYS_sleep(unsigned long seconds)
{
    kernel_timespec rqtp = {0};
    rqtp.tv_secs = seconds;
    int ret;
     __asm__ __volatile__
    (
         "syscall"
         : "=a"(ret)
         : "a"(35), "D"((uintptr_t)&rqtp), "S"((uintptr_t)0)
         : "rcx", "r11", "memory"
    );
    return ret;
}

int main(int argc, char *argv[])
{
        if (argc != 2)
        {
                fprintf(stderr,"Usage: cmd arg\n");
                return EXIT_FAILURE;
        }

        char *raw_seconds = argv[1];
        int seconds = str_to_int(raw_seconds);

        printf("Sleeping for %d seconds\n", seconds);
        SYS_sleep(seconds);

        return EXIT_SUCCESS;

}

__attribute__((noreturn)) void exit(int status)
{
    __asm__ __volatile__
    (
         "syscall"
         :
         : "a"(60), "D"(status)
         : "rcx", "r11", "memory"
    );
    __builtin_unreachable();
}


__attribute__((naked,noreturn)) void _start()
{
        __asm__ __volatile__
        (
        "xor %ebp, %ebp\n"
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "and $-16, %rsp\n"
        "call main\n"
        "mov %rax, %rdi\n"
        "call exit\n"
        );
        __builtin_unreachable();

}
