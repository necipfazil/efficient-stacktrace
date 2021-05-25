// int backtrace(void **buffer, int size);
// series of currently active functions
// each is the return address from the corresponding stack frame
// returns number of addresses returned in buffer



// char **backtrace_symbols(void *const *buffer, int size);
// after backtrace(), translates the addresses into strings that describe
// addresses symbolically.
// symbolic repr: function name, hexadecimal offset to the function, and 
// actual hex return address

// symbol names may require -rdynamic

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define BT_BUF_SIZE 100

void
myfunc3(void)
{
    int nptrs;
    void *buffer[BT_BUF_SIZE];
    char **strings;

    nptrs = backtrace(buffer, BT_BUF_SIZE);
    printf("backtrace() returned %d addresses\n", nptrs);

    /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
      would produce similar output to the following: */

    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }

    for (int j = 0; j < nptrs; j++) {
        //printf("\naddress: %x\n", buffer[j]);
        void* buf_j = buffer[j];
        printf("\n");
        printf("retaddr= %p\n", buf_j);
        
        printf("%s\n", strings[j]);
    }

    free(strings);
}

static void   /* "static" means don't export the symbol... */
myfunc2(void)
{
    myfunc3();
}

void
myfunc(int ncalls)
{
    if (ncalls > 1)
        myfunc(ncalls - 1);
    else
        myfunc2();
}

int
main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "%s num-calls\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    myfunc(atoi(argv[1]));
    exit(EXIT_SUCCESS);
}