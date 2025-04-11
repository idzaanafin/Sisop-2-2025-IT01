#!/bin/bash

# Nama file log
log_file="debugmon.log"

# Fungsi untuk mencatat pesan ke file log dengan format yang ditentukan
log_activity() {
  local timestamp=$(date "+%d:%m:%Y-%H:%M:%S")
  local process_name="$1"
  local status="$2"
  echo "[$timestamp]_$process_name_$status" >> "$log_file"
}

# Fungsi untuk mencatat pesan log umum
log_message() {
  local message="$1"
  date "+%Y-%m-%d %H:%M:%S" | tee -a "$log_file" <<< " [INFO] $message"
}

# Fungsi untuk menampilkan daftar proses untuk user tertentu
list_processes() {
  local username="$1"
  echo " -------------------------------------------------------------------------- "
  echo " |                       DAFTAR PROSES PENGGUNA: $username                     |"
  echo " -------------------------------------------------------------------------- "
  printf "| %-7s | %-30s | %-9s | %-10s |\n" "PID" "COMMAND" "CPU (%)" "MEM (%)"
  echo " -------------------------------------------------------------------------- "
  ps -u "$username" -o pid,command,%cpu,%mem --no-headers | \
  awk '{printf "| %-7s | %-30s | %-9.1f | %-10.1f |\n", $1, $2, $3, $4}'
  echo " -------------------------------------------------------------------------- "
}

# Fungsi untuk menjalankan Debugmon sebagai daemon
run_daemon() {
  local username="$1"

  if [ -z "$username" ]; then
    echo "Penggunaan: ./debugmon daemon <username>"
    exit 1
  fi

  log_message "Memulai Debugmon dalam mode daemon untuk pengguna: $username"
  log_activity "debugmon_daemon" "RUNNING" # Catat awal daemon

  while true; do
    timestamp=$(date "+%Y-%m-%d %H:%M:%S")
    log_message "Mencatat aktivitas pengguna $username pada: $timestamp"

    ps -u "$username" -o pid,command --no-headers |
    while IFS= read -r pid command; do
      log_activity "$command" "RUNNING"
    done

    sleep 5
  done > /dev/null 2>&1 &
}

# Fungsi untuk menghentikan pengawasan daemon
stop_daemon() {
  local username="$1"
  local pid=$(pgrep -u "$username" -f "debugmon daemon $username")

  if [ -n "$pid" ]; then
    log_message "Menghentikan Debugmon daemon (PID: $pid) untuk pengguna: $username"
    kill "$pid"
    log_activity "debugmon_daemon" "STOPPED" # Anda bisa menambahkan status STOPPED
  else
    log_message "Tidak ada instance Debugmon daemon yang berjalan untuk pengguna: $username"
  fi
}

# Fungsi untuk menggagalkan semua proses user
fail_processes() {
  local username="$1"
  log_message "Menggagalkan semua proses untuk pengguna: $username"
  pkill -u "$username"
  ps -u "$username" -o command --no-headers | \
  while IFS= read -r command; do
    log_activity "$command" "FAILED"
  done
  log_message "Semua proses untuk pengguna $username telah digagalkan. Mode gagal aktif."
}

# Fungsi untuk mengizinkan user kembali menjalankan proses
revert_processes() {
  local username="$1"
  log_message "Mengembalikan mode normal untuk pengguna: $username"
  log_activity "debugmon_revert" "RUNNING" # Catat revert sebagai aktivitas
  log_message "Mode normal dipulihkan untuk pengguna: $username."
}

# Nama pengguna yang ingin dipantau (default jika tidak ada argumen untuk daemon)
username_target="ocaastudy"

# Cek argumen yang diberikan
if [ "$#" -eq 2 ]; then
  command="$1"
  username="$2"

  if [ "$command" == "list" ]; then
    list_processes "$username"
  elif [ "$command" == "daemon" ]; then
    run_daemon "$username" &
  elif [ "$command" == "stop" ]; then
    stop_daemon "$username"
  elif [ "$command" == "fail" ]; then
    fail_processes "$username"
  elif [ "$command" == "revert" ]; then
    revert_processes "$username"
  else
    echo "Perintah tidak dikenal: $command"
    echo "Penggunaan: ./debugmon [list <username>] | [daemon <username>] | [stop <username>] | [fail <username>] | [revert <username>]"
    exit 1
  fi
else
  echo "Penggunaan: ./debugmon [list <username>] | [daemon <username>] | [stop <username>] | [fail <username>] | [revert <username>]"
  exit 1
fi



