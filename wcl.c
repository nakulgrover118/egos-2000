// egos-2000/apps/user/wcl.c
#include "app.h"
#include <string.h>
#include <stdlib.h>

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 4096
#endif

extern int dir_lookup(int dir_ino, char *name);
extern int file_read(int file_ino, unsigned int offset, char *block);
extern void printf(const char *fmt, ...);
extern void write(int fd, const void *buf, int len);

static void usage() {
    printf("Usage: wcl [FILE1] [FILE2] ...\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    unsigned long long total_lines = 0;

    char *buf = malloc(BLOCK_SIZE + 1);
    if (!buf) return 1;

    for (int fi = 1; fi < argc; fi++) {
        char *filename = argv[fi];
        int file_ino = dir_lookup(0, filename);
        if (file_ino < 0) {
            // If file doesn't exist, skip it (you may choose to print an error)
            continue;
        }

        unsigned int offset = 0;
        while (1) {
            int n = file_read(file_ino, offset, buf);
            if (n < 0) break;
            if (n == 0) break;
            for (int i = 0; i < n; i++) {
                if (buf[i] == '\n') total_lines++;
            }
            offset += n;
        }
    }

    // print total lines followed by newline
    char out[64];
    int len = snprintf(out, sizeof(out), "%llu\n", total_lines);
    write(1, out, len);

    free(buf);
    return 0;
}
