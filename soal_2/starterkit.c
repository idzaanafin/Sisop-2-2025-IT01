
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zip.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>

//activity logger
void log_activity(const char *format, ...) {
    FILE *log_file = fopen("activity.log", "a");
    if (!log_file) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t) {
        fclose(log_file);
        return;
    }

    char time_str[32];
    strftime(time_str, sizeof(time_str), "[%d-%m-%Y][%H:%M:%S]", t);
    fprintf(log_file, "%s - ", time_str);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");
    fclose(log_file);
}


// Base64 decoder
int base64_char_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

void base64_decode(const char *input, unsigned char *output) {
    int len = strlen(input);
    int val = 0, valb = -8;
    int index = 0;

    for (int i = 0; i < len; i++) {
        if (input[i] == '=' || input[i] == '\n') break;
        int c = base64_char_value(input[i]);
        if (c == -1) continue;
        val = (val << 6) + c;
        valb += 6;
        if (valb >= 0) {
            output[index++] = (val >> valb) & 0xFF;
            valb -= 8;
        }
    }
    output[index] = '\0';
}

// Returns 1 if the string is likely Base64, 0 otherwise
int is_base64(const char *str) {
    int len = strlen(str);
    if (len == 0) return 0;

    // Check for padding at the end
    if (str[len - 1] == '=') {
        if (len >= 2 && str[len - 2] == '=') {
            len -= 2;  // Handle double padding
        } else {
            len -= 1;  // Handle single padding
        }
    }

    // Check remaining characters
    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (!(isalnum(c) || c == '+' || c == '/')) {
            return 0;
        }
    }
    return 1;
}

// Membuat direktori
void create_directory(const char *path) {
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        perror("Failed to create directory");
        exit(EXIT_FAILURE);
    }
}

// Mengekstrak ZIP
void extract_zip(const char *zip_path, const char *extract_path) {
    struct zip *z;
    struct zip_file *zf;
    struct zip_stat sb;
    char buffer[8192];
    int err;

    z = zip_open(zip_path, 0, &err);
    if (!z) {
        fprintf(stderr, "Failed to open ZIP file: %s\n", zip_path);
        return;
    }

    for (zip_uint64_t i = 0; i < zip_get_num_entries(z, 0); i++) {
        if (zip_stat_index(z, i, 0, &sb) == 0) {
            printf("Extracting: %s\n", sb.name);

            zf = zip_fopen_index(z, i, 0);
            if (!zf) {
                fprintf(stderr, "Failed to open file in ZIP: %s\n", sb.name);
                continue;
            }

            char out_path[1024];
            snprintf(out_path, sizeof(out_path), "%s/%s", extract_path, sb.name);

            // Buat direktori kalau perlu
            char dir_path[1024];
            strncpy(dir_path, out_path, sizeof(dir_path));
            char *last_slash = strrchr(dir_path, '/');
            if (last_slash) {
                *last_slash = '\0';
                mkdir(dir_path, 0755);
            }

            FILE *out = fopen(out_path, "wb");
            if (!out) {
                fprintf(stderr, "Failed to create output file: %s\n", out_path);
                zip_fclose(zf);
                continue;
            }

            zip_int64_t bytes_read;
            while ((bytes_read = zip_fread(zf, buffer, sizeof(buffer))) > 0) {
                fwrite(buffer, 1, bytes_read, out);
            }

            fclose(out);
            zip_fclose(zf);
        }
    }

    zip_close(z);
    printf("Extraction complete!\n");
}

// Menghapus file
void delete_file(const char *file_path) {
    if (remove(file_path) != 0) {
        perror("Failed to delete file");
    } else {
        printf("File deleted: %s\n", file_path);
    }
}

// Daemon untuk decrypt nama file
volatile sig_atomic_t keep_running = 1;

void sigterm_handler(int signum) {
    keep_running = 0;
}

void run_decryption_daemon() {
    const char *source_dir = "starter_kit";
    const char *quarantine_dir = "quarantine";

    // Setup signal handler
    signal(SIGTERM, sigterm_handler);

    // Daemon initialization
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        fprintf(stderr, "Daemon starting in: %s\n", cwd);
    }


    umask(0);
    setsid();

    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null == -1) {
        perror("open /dev/null failed");
        exit(EXIT_FAILURE);
    }
    dup2(dev_null, STDIN_FILENO);
    dup2(dev_null, STDOUT_FILENO);

    // Create quarantine directory if needed
    if (mkdir(quarantine_dir, 0755) == -1 && errno != EEXIST) {
        perror("Failed to create quarantine directory");
        exit(EXIT_FAILURE);
    }

    // Loop until SIGTERM
    while (keep_running) {
        DIR *dir = opendir(source_dir);
        if (!dir) {
            perror("Failed to open source directory");
            sleep(5);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type != DT_REG) continue;

            if (!is_base64(entry->d_name)) continue;

            unsigned char decoded_name[256];
            base64_decode(entry->d_name, decoded_name);

            char src_path[1024], dest_path[1024];
            snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, entry->d_name);
            snprintf(dest_path, sizeof(dest_path), "%s/%s", quarantine_dir, decoded_name);

            rename(src_path, dest_path); // Move file
        }

        closedir(dir);
        sleep(5); // Wait before next scan
    }

    fprintf(stderr, "Daemon shutting down...\n");
    close(dev_null);
    exit(EXIT_SUCCESS);
}


void move_files(const char *source_dir, const char *dest_dir, const char *mode) {
    DIR *dir = opendir(source_dir);
    if (!dir) {
        perror("Failed to open source directory");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;

        char src_path[1024], dest_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);

        if (rename(src_path, dest_path) == -1) {
            perror("Failed to move file");
        } else {
            printf("Moved: %s -> %s\n", src_path, dest_path);
            if (strcmp(mode, "quarantine") == 0)
                log_activity("%s - Successfully moved to quarantine directory.", entry->d_name);
            else if (strcmp(mode, "return") == 0)
                log_activity("%s - Successfully returned to starter kit directory.", entry->d_name);
        }
    }

    closedir(dir);
}

void eradicate_files(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("Failed to open directory");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;

        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

        if (remove(file_path) == -1) {
            perror("Failed to delete file");
        } else {
            printf("Deleted: %s\n", file_path);
            log_activity("%s - Successfully deleted.", entry->d_name);
        }
    }

    closedir(dir);
}

// Fungsi untuk membaca PID dari file
pid_t read_pid(const char *pid_file_path) {
    FILE *pid_file = fopen(pid_file_path, "r");
    if (!pid_file) {
        perror("Failed to open PID file");
        return -1;
    }

    pid_t pid;
    if (fscanf(pid_file, "%d", &pid) != 1) {
        perror("Failed to read PID from file");
        fclose(pid_file);
        return -1;
    }

    fclose(pid_file);
    return pid;
}

// Fungsi untuk menulis PID ke file
void write_pid(const char *pid_file_path, pid_t pid) {
    FILE *pid_file = fopen(pid_file_path, "w");
    if (!pid_file) {
        perror("Failed to write PID file");
        exit(EXIT_FAILURE);
    }

    fprintf(pid_file, "%d\n", pid);
    fclose(pid_file);
}

// Fungsi untuk menghentikan proses berdasarkan PID
void shutdown_daemon(const char *pid_file_path) {
    pid_t pid = read_pid(pid_file_path);
    if (pid == -1) {
        fprintf(stderr, "Could not retrieve PID.\n");
        return;
    }

    if (kill(pid, SIGTERM) == -1) {
        perror("Failed to terminate daemon process");
    } else {
        printf("Daemon process with PID %d terminated.\n", pid);
        log_activity("Successfully shut off decryption process with PID %d.", pid);
    }

    // Hapus file PID
    remove(pid_file_path);
}

// Fungsi untuk memeriksa apakah direktori ada dan bisa diakses
int directory_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

// Fungsi untuk memeriksa apakah file ada dan bisa diakses
int file_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        return 0;
    }
    return S_ISREG(st.st_mode);
}

// Main
// Main dengan error handling yang lebih baik
int main(int argc, char *argv[]) {
    const char *quarantine_dir = "quarantine";
    const char *starter_kit_dir = "starter_kit";
    const char *pid_file = "starterkit.pid";

    // Validasi argumen
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [--decrypt|--quarantine|--return|--eradicate|--shutdown]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Handle opsi command line
    if (argc == 2) {
        if (strcmp(argv[1], "--decrypt") == 0) {
            // Validasi sebelum menjalankan daemon
            if (!directory_exists(starter_kit_dir)) {
                fprintf(stderr, "Error: Directory '%s' does not exist or is not accessible\n", starter_kit_dir);
                return EXIT_FAILURE;
            }

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                return EXIT_FAILURE;
            }
            if (pid > 0) {
                write_pid(pid_file, pid);
                printf("Daemon PID: %d\n", pid);
                log_activity("Successfully started decryption process with PID %d.", pid);
                return EXIT_SUCCESS;
            }

            umask(0);
            setsid();
            run_decryption_daemon();
            return 0;
        }
        else if (strcmp(argv[1], "--quarantine") == 0) {
            if (!directory_exists(starter_kit_dir)) {
                fprintf(stderr, "Error: Source directory '%s' does not exist\n", starter_kit_dir);
                return EXIT_FAILURE;
            }
            create_directory(quarantine_dir);
            move_files(starter_kit_dir, quarantine_dir, "quarantine");
            return 0;
        }
        else if (strcmp(argv[1], "--return") == 0) {
            if (!directory_exists(quarantine_dir)) {
                fprintf(stderr, "Error: Quarantine directory '%s' does not exist\n", quarantine_dir);
                return EXIT_FAILURE;
            }
            create_directory(starter_kit_dir);
            move_files(quarantine_dir, starter_kit_dir, "return");
            return 0;
        }
        else if (strcmp(argv[1], "--eradicate") == 0) {
            if (!directory_exists(quarantine_dir)) {
                fprintf(stderr, "Error: Quarantine directory '%s' does not exist\n", quarantine_dir);
                return EXIT_FAILURE;
            }
            printf("Deleting all files in quarantine directory...\n");
            eradicate_files(quarantine_dir);
            printf("Eradication complete!\n");
            return 0;
        }
        else if (strcmp(argv[1], "--shutdown") == 0) {
            if (!file_exists(pid_file)) {
                fprintf(stderr, "Error: PID file '%s' does not exist - daemon may not be running\n", pid_file);
                return EXIT_FAILURE;
            }
            printf("Shutting down daemon process...\n");
            shutdown_daemon(pid_file);
            return 0;
        }
        else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[1]);
            fprintf(stderr, "Valid options: --decrypt, --quarantine, --return, --eradicate, --shutdown\n");
            return EXIT_FAILURE;
        }
    }

    // Download dan extract
    const char *url = "https://docs.google.com/uc?export=download&id=1_5GxIGfQr3mNKuavJbte_AoRkEQLXSKS";
    const char *download_path = "starterkit.zip";
    const char *extract_path = "starter_kit";

    // Periksa apakah file sudah ada
    if (file_exists(download_path)) {
        fprintf(stderr, "Error: File '%s' already exists. Please remove it before downloading again.\n", download_path);
        return EXIT_FAILURE;
    }

    printf("Downloading file from Google Drive...\n");

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return EXIT_FAILURE;
    } else if (pid == 0) {
        char *argv[] = {
            "wget",
            "--no-check-certificate",
            (char *)url,
            "-O",
            (char *)download_path,
            NULL
        };
        execvp("wget", argv);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Download failed with status: %d\n", WEXITSTATUS(status));
            return EXIT_FAILURE;
        }
    }

    // Validasi file yang didownload
    struct stat st;
    if (stat(download_path, &st) == -1 || st.st_size == 0) {
        fprintf(stderr, "Error: Downloaded file is empty or inaccessible\n");
        return EXIT_FAILURE;
    }

    printf("Creating extraction directory...\n");
    create_directory(extract_path);

    printf("Extracting ZIP file...\n");
    extract_zip(download_path, extract_path);

    // Validasi hasil ekstraksi
    DIR *dir = opendir(extract_path);
    if (!dir) {
        perror("Failed to open extraction directory");
        return EXIT_FAILURE;
    }
    closedir(dir);

    printf("Deleting original ZIP file...\n");
    delete_file(download_path);

    printf("All tasks completed successfully!\n");
    return 0;
}

