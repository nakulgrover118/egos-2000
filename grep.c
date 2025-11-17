// egos-2000/apps/user/grep.c
// Minimal grep implementation for EGOS-2000
#include "app.h"
#include <string.h>
#include <stdlib.h>

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 4096
#endif

// External syscall wrappers (provided in EGOS)
extern int dir_lookup(int dir_ino, char *name);
extern int file_read(int file_ino, unsigned int offset, char *block);
extern void printf(const char *fmt, ...);
extern void write(int fd, const void *buf, int len);

static void usage() {
    printf("Usage: grep [PATTERN] [FILE]\n");
}

int main(int argc, char **argv) {
    if (argc != 3) {
        usage();
        return 1;
    }

    char *pattern = argv[1];
    char *filename = argv[2];

    int file_ino = dir_lookup(0, filename);
    if (file_ino < 0) {
        // file not found -> per spec, print nothing but exit successfully or
        // optionally return error code. We'll return 0 to match "print nothing".
        return 0;
    }

    // Read entire file block by block
    char *buf = malloc(BLOCK_SIZE + 1);
    if (!buf) return 1;
    unsigned int offset = 0;
    // We'll accumulate lines across blocks in a dynamic buffer for correct line detection.
    // Use a small growable string to hold partial line across block boundaries.
    char *linebuf = NULL;
    size_t linecap = 0;
    size_t linelen = 0;

    while (1) {
        int n = file_read(file_ino, offset, buf);
        if (n < 0) break; // error reading
        if (n == 0) break; // EOF
        buf[n] = '\0';

        unsigned int i = 0;
        while (i < (unsigned int)n) {
            // Find newline or consume rest
            unsigned int start = i;
            while (i < (unsigned int)n && buf[i] != '\n') i++;
            // append buf[start..i-1] to linebuf
            size_t chunk = i - start;
            if (linecap < linelen + chunk + 1) {
                size_t newcap = (linecap == 0) ? 1024 : linecap * 2;
                while (newcap < linelen + chunk + 1) newcap *= 2;
                char *tmp = realloc(linebuf, newcap);
                if (!tmp) {
                    free(linebuf);
                    free(buf);
                    return 1;
                }
                linebuf = tmp;
                linecap = newcap;
            }
            memcpy(linebuf + linelen, buf + start, chunk);
            linelen += chunk;
            linebuf[linelen] = '\0';

            if (i < (unsigned int)n && buf[i] == '\n') {
                // full line available
                if (strstr(linebuf, pattern) != NULL) {
                    // print line followed by newline
                    write(1, linebuf, (int)linelen);
                    write(1, "\n", 1);
                }
                // reset line buffer
                linelen = 0;
                linebuf[0] = '\0';
                i++; // skip newline
            }
        }

        offset += n;
    }

    // If file ended without newline, check last partial line
    if (linelen > 0) {
        if (strstr(linebuf, pattern) != NULL) {
            write(1, linebuf, (int)linelen);
            write(1, "\n", 1);
        }
    }

    free(linebuf);
    free(buf);
    return 0;
}
