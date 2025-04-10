#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>


int xor_key;

void xor_encrypt_file(const char *path, time_t key) {
    FILE *f = fopen(path, "rb+");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    for (long i = 0; i < size; i++) {
        int c = fgetc(f);
        fseek(f, -1, SEEK_CUR);
        fputc(c ^ key, f);
    }

    fclose(f);
}


int main() {
    xor_key = 69;
    xor_encrypt_file("rahasia.txt", xor_key);
    return 0;
}
