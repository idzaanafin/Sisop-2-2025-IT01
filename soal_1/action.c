#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#define ZIP_FILE "Clues.zip"
#define ZIP_URL "https://drive.google.com/uc?export=download&id=1xFn1OBJUuSdnApDseEczKhtNzyGekauK"
#define ZIP_PASSWORD "password123" 

int is_valid_file(const char *filename) {
    return strlen(filename) == 5 && isalnum(filename[0]) && strcmp(filename + 1, ".txt") == 0;
}

int run_command(const char *cmd, char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd, argv);
        perror("exec failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    } else {
        perror("fork failed");
        return 0;
    }
}

void download_and_unzip() {
    struct stat st = {0};
    if (stat("Clues", &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("Folder Clues sudah ada. Lewati download.\n");
        return;
    }

    printf("Mengunduh Clues.zip...\n");
    char *wget_args[] = {"wget", "-q", "-O", ZIP_FILE, ZIP_URL, NULL};
    if (!run_command("wget", wget_args)) {
        fprintf(stderr, "Gagal mengunduh Clues.zip\n");
        return;
    }

    // Cek Zip download
    if (stat(ZIP_FILE, &st) != 0) {
        fprintf(stderr, "File Clues.zip tidak ditemukan setelah download.\n");
        return;
    }

    printf("Ekstrak Clues.zip...\n");
    char *unzip_args[] = {"unzip", "-P", ZIP_PASSWORD, "-q", ZIP_FILE, NULL};
    if (!run_command("unzip", unzip_args)) {
        fprintf(stderr, "Gagal mengekstrak Clues.zip. Pastikan password benar.\n");
        return;
    }

    // Cek apakah folder Clues
    if (stat("Clues", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Folder Clues tidak ditemukan setelah ekstraksi.\n");
        return;
    }

    remove(ZIP_FILE);
    printf("Download dan ekstrak selesai. Folder Clues berhasil dibuat.\n");
}

void filter_files() {
    mkdir("Filtered", 0755);
    const char *subdirs[] = {"Clues/ClueA", "Clues/ClueB", "Clues/ClueC", "Clues/ClueD"};

    for (int i = 0; i < 4; ++i) {
        DIR *dir = opendir(subdirs[i]);
        if (!dir) {
            fprintf(stderr, "Gagal membuka folder %s\n", subdirs[i]);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (entry->d_type != DT_REG) continue;

            size_t src_length = strlen(subdirs[i]) + strlen(entry->d_name) + 2;
            size_t dst_length = strlen("Filtered") + strlen(entry->d_name) + 2;

            if (src_length >= 512 || dst_length >= 512) {
                fprintf(stderr, "Buffer terlalu kecil untuk file %s\n", entry->d_name);
                continue;
            }

            char src[512], dst[512];
            snprintf(src, sizeof(src), "%s/%s", subdirs[i], entry->d_name);

            if (is_valid_file(entry->d_name)) {
                snprintf(dst, sizeof(dst), "Filtered/%s", entry->d_name);
                if (rename(src, dst) == 0) {
                    printf("File %s dipindahkan ke Filtered.\n", entry->d_name);
                } else {
                    perror("Gagal memindahkan file");
                }
            } else {
                if (remove(src) == 0) {
                    printf("File %s dihapus.\n", entry->d_name);
                } else {
                    perror("Gagal menghapus file");
                }
            }
        }
        closedir(dir);
    }

    printf("Filtering selesai. File disimpan di folder Filtered.\n");
}

int cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

void combine_files() {
    DIR *dir = opendir("Filtered");
    if (!dir) {
        fprintf(stderr, "Folder Filtered tidak ditemukan.\n");
        return;
    }

    char numbers[100][6], letters[100][6];
    int n = 0, l = 0;
    struct dirent *entry;

    while ((entry = readdir(dir))) {
        if (is_valid_file(entry->d_name)) {
            if (isdigit(entry->d_name[0]))
                strcpy(numbers[n++], entry->d_name);
            else if (isalpha(entry->d_name[0]))
                strcpy(letters[l++], entry->d_name);
        }
    }
    closedir(dir);

    qsort(numbers, n, sizeof(numbers[0]), cmp);
    qsort(letters, l, sizeof(letters[0]), cmp);

    FILE *out = fopen("Combined.txt", "w");
    if (!out) {
        perror("Gagal membuat Combined.txt");
        return;
    }

    for (int i = 0; i < n || i < l; ++i) {
        if (i < n) {
            char path[512];
            snprintf(path, sizeof(path), "Filtered/%s", numbers[i]);
            FILE *f = fopen(path, "r");
            if (f) {
                int c; while ((c = fgetc(f)) != EOF) fputc(c, out);
                fclose(f);
                remove(path);
            }
        }
        if (i < l) {
            char path[512];
            snprintf(path, sizeof(path), "Filtered/%s", letters[i]);
            FILE *f = fopen(path, "r");
            if (f) {
                int c; while ((c = fgetc(f)) != EOF) fputc(c, out);
                fclose(f);
                remove(path);
            }
        }
    }

    fclose(out);
    printf("Isi file telah digabung ke Combined.txt\n");
}

void rot13_decode() {
    FILE *in = fopen("Combined.txt", "r");
    FILE *out = fopen("Decoded.txt", "w");
    if (!in || !out) {
        perror("Gagal membuka Combined.txt atau membuat Decoded.txt");
        if (in) fclose(in);
        if (out) fclose(out);
        return;
    }

    int c;
    while ((c = fgetc(in)) != EOF) {
        if (isalpha(c)) {
            c = ((c & 32) ? 'a' : 'A') + ((c - ((c & 32) ? 'a' : 'A') + 13) % 26);
        }
        fputc(c, out);
    }

    fclose(in);
    fclose(out);
    printf("Decode ROT13 selesai. Output disimpan di Decoded.txt\n");
}

void print_usage() {
    printf("Penggunaan:\n");
    printf("  ./action            # Download & extract Clues.zip\n");
    printf("  ./action -m Filter  # Filter file valid ke folder Filtered\n");
    printf("  ./action -m Combine # Gabungkan isi file ke Combined.txt\n");
    printf("  ./action -m Decode  # Decode Combined.txt ke Decoded.txt\n");
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        download_and_unzip();
    } else if (argc == 3 && strcmp(argv[1], "-m") == 0) {
        if (strcmp(argv[2], "Filter") == 0)
            filter_files();
        else if (strcmp(argv[2], "Combine") == 0)
            combine_files();
        else if (strcmp(argv[2], "Decode") == 0)
            rot13_decode();
        else
            print_usage();
    } else {
        print_usage();
    }

    return 0;
}

