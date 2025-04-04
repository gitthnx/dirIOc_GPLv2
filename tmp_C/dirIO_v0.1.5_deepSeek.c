#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <time.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <ncurses.h>
#include <math.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))
#define MAX_DEPTH 10

// Global variables
volatile sig_atomic_t paused = 0;
volatile sig_atomic_t running = 1;
int mode = 1;
int n_level = 10;
int prev_n_level = 0;
int window_height = 1;
int rounding = 0;
long dir_sizes[MAX_DEPTH + 1] = {0};
long start_dir_size = 0;
long current_dir_size = 0;
time_t start_time;
char *directory = NULL;
char *inotify_log_path = "/dev/shm/inotify.lg";
pid_t inotify_pid = 0;

// Function prototypes
void sig_handler(int signo);
void setup_signals();
void display_help();
long calculate_dir_size(const char *path, int max_depth);
void monitor_io();
void calculate_data_rate();
void graphical_output(long data_rate_output, double time_diff);
void cleanup();
int kbhit();
void init_ncurses();
void exit_ncurses();

void sig_handler(int signo) {
    if (signo == SIGTERM || signo == SIGINT) {
        running = 0;
    }
}

void setup_signals() {
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

void display_help() {
    printw("\n");
    printw("       keys: search tree level == 'n'\n");
    printw("             output mode       == 'm'\n");
    printw("             pause             == 'p'\n");
    printw("             resume            == ' ' or 'r'\n");
    printw("             clear screen      == 'c' or 'C'\n");
    printw("             help              == 'h' or 'H' or '?'\n");
    printw("             quit              == 'q' or 'Q'\n");
    printw("\n");
    printw("       version 0.1.5 (C implementation)\n");
    printw("       March 2025\n");
    printw("\n");
    refresh();
}

long calculate_dir_size(const char *path, int max_depth) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    long size = 0;
    static int current_depth = 0;
    
    if ((dir = opendir(path)) == NULL) {
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        if (lstat(full_path, &statbuf) == -1) {
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            if (max_depth == 10 || current_depth < max_depth) {
                current_depth++;
                size += calculate_dir_size(full_path, max_depth);
                current_depth--;
            }
        } else {
            size += statbuf.st_size;
        }
    }
    closedir(dir);
    return size;
}

void monitor_io() {
    FILE *log_file = fopen(inotify_log_path, "r");
    if (log_file) {
        // Simplified inotify log processing
        fclose(log_file);
        
        // Update directory size based on depth level
        if (n_level == 10) {
            current_dir_size = calculate_dir_size(directory, 10);
        } else {
            current_dir_size = calculate_dir_size(directory, n_level + 1);
        }
        
        dir_sizes[n_level] = current_dir_size;
        prev_n_level = n_level;
    } else {
        mvprintw(65, 0, "no io");
    }
}

void calculate_data_rate() {
    time_t now = time(NULL);
    long uptime = now - start_time;
    
    long data_rate_output = current_dir_size - start_dir_size;
    
    mvprintw(7, 0, "Data rate io: %ld bytes/s  %.4f MB/s", 
             data_rate_output, data_rate_output / (1024.0 * 1024.0));
    
    if (mode > 2) {
        mvprintw(40, 0, "Data Input Rate: %ld bytes/sec %.2f kB/s %.2f MB/s",
                 data_rate_output, 
                 data_rate_output / 1024.0, 
                 data_rate_output / (1024.0 * 1024.0));
    }
}

void graphical_output(long data_rate_output, double time_diff) {
    int row, col;
    getyx(stdscr, row, col);
    
    int relh_pos;
    long data_io = labs(data_rate_output);
    
    if (data_io >= 10L * 1024 * 1024 * 1024) relh_pos = 19;
    else if (data_io >= 1024L * 1024 * 1024) relh_pos = 16;
    else if (data_io >= 512L * 1024 * 1024) relh_pos = 9;
    else if (data_io >= 128L * 1024 * 1024) relh_pos = 7;
    else if (data_io >= 1024L * 1024) relh_pos = 5;
    else if (data_io >= 512L * 1024) relh_pos = 4;
    else if (data_io >= 64L * 1024) relh_pos = 3;
    else relh_pos = data_io / (22 * 1024);
    
    char time_str[32];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (rounding) {
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm);
    } else {
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm);
    }
    
    double ioMBps = data_rate_output / (1024.0 * 1024.0);
    
    if (data_io != 0) {
        mvprintw(row, 0, "%s %ld bytes/s %.2fs", time_str, data_rate_output, time_diff);
    } else {
        mvprintw(row, 0, "   %s", time_str);
    }
    
    if (data_rate_output < 0) {
        mvprintw(row, 53, "%.2f MB/s", ioMBps);
    } else if (data_rate_output > 0) {
        mvprintw(row, 112, "%.2f MB/s", ioMBps);
    }
    
    mvprintw(row, 70, "|");
    if (data_rate_output <= 0) {
        move(row, 90 - relh_pos);
    } else {
        move(row, 90);
    }
    
    for (int i = 0; i < relh_pos; i++) {
        printw("~");
    }
    
    mvprintw(row, 90, "|");
    mvprintw(row, 110, "|");
}

void cleanup() {
    if (inotify_pid > 0) {
        kill(inotify_pid, SIGTERM);
    }
    exit_ncurses();
}

int kbhit() {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

void init_ncurses() {
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);
}

void exit_ncurses() {
    curs_set(1);
    endwin();
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s /directory/to/monitor\n", argv[0]);
        return 1;
    }
    
    directory = argv[1];
    
    // Check if directory exists
    DIR *dir = opendir(directory);
    if (!dir) {
        printf("Error: Directory '%s' does not exist or is not accessible\n", directory);
        return 1;
    }
    closedir(dir);
    
    // Prevent monitoring root filesystem
    if (strcmp(directory, "/") == 0) {
        printf("*** no root fs io monitoring recommended ***\n");
        return 1;
    }
    
    //setup_signals();
    atexit(cleanup);
    init_ncurses();
    
    // Initial directory size calculation
    start_dir_size = calculate_dir_size(directory, n_level + 1);
    current_dir_size = start_dir_size;
    start_time = time(NULL);
    
    // Start inotifywait in background (simplified)
    inotify_pid = fork();
    if (inotify_pid == 0) {
        // Child process - run inotifywait
        execl("/usr/bin/inotifywait", "inotifywait", "-e", "create,modify,move,delete",
              "-r", "-m", "--timefmt", "%m/%d/%Y %H:%M:%S", 
              "--format", "[%T] %w,%f,%e,%x", "-o", inotify_log_path,
              "--exclude", inotify_log_path, directory, NULL);
        exit(EXIT_FAILURE); // Only reached if execl fails
    }
    
    clear();
    time_t start_date = time(NULL);
    mvprintw(3, 0, "monitoring start: %s", ctime(&start_date));
    mvprintw(4, 0, "directory size: %ld kB", start_dir_size / 1024);
    refresh();
    
    struct timespec last_time, current_time;
    clock_gettime(CLOCK_REALTIME, &last_time);
    
    while (running) {
        clock_gettime(CLOCK_REALTIME, &current_time);
        double time_diff = (current_time.tv_sec - last_time.tv_sec) + 
                         (current_time.tv_nsec - last_time.tv_nsec) / 1e9;
        last_time = current_time;
        
        if (!paused) {
            monitor_io();
            calculate_data_rate();
            if (mode > 0) {
                graphical_output(current_dir_size - start_dir_size, time_diff);
            }
        }
        
        if (kbhit()) {
            int ch = getch();
            mvprintw(46, 0, "key pressed: '%c' (%d)", ch, ch);
            
            switch (ch) {
                case 'q':
                case 'Q':
                    running = 0;
                    break;
                case 'p':
                    paused = 1;
                    mvprintw(47, 0, "Output paused. Press space or key 'r' to resume.");
                    break;
                case ' ':
                case 'r':
                    paused = 0;
                    mvprintw(47, 0, "Output resumed.");
                    break;
                case 'm':
                    mode = (mode + 1) % 4;
                    break;
                case 'n':
                    n_level = (n_level + 1) % 11;
                    break;
                case 'c':
                case 'C':
                    clear();
                    break;
                case 'h':
                case 'H':
                case '?':
                    display_help();
                    break;
            }
        }
        
        refresh();
        usleep(100000); // 100ms delay
    }
    
    time_t stop_date = time(NULL);
    mvprintw(47, 0, "monitoring stop: %s", ctime(&stop_date));
    refresh();
    sleep(1);
    
    return 0;
}
