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

#define LOG_FILE "debugmon.log"
#define PID_FILE "debugmon.pid"

// Fungsi untuk mencatat ke dalam file log
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

// Fungsi untuk menampilkan daftar proses user
void list_user_processes(const char *username) {
    DIR *proc = opendir("/proc");
    struct dirent *entry;
    struct passwd *pwd = getpwnam(username);
    if (!pwd) {
        printf("User not found.\n");
        return;
    }
    uid_t target_uid = pwd->pw_uid;

    // Header tabel
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

    // Garis bawah tabel
    printf("------------------------------------------------------------------------------------------------\n");
}

// Fungsi untuk menjalankan daemon
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

// Fungsi untuk menghentikan proses daemon
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

// Fungsi untuk menghentikan semua proses user
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

// Fungsi untuk revert user menggunakan root
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

