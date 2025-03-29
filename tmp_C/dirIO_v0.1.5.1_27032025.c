// gcc dirIO.c -o dirIO -lncurses // -lcurses?
// find /dev/shm -type d | sed 's|[^/]||g' | sort | tail -n1 | wc -c
// du -m /dev/shm | sort -nr | head -n 20

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
#include <errno.h>
#include <limits.h>

#include <sys/statvfs.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))
#define ALL_DEPTH 31
#define ALL_EVENTS 1024
#define INOTIFY_LOG_SIZE 15

// Global variables
const unsigned int GB = (1024 * 1024) * 1024;
const unsigned int MB = (1024 * 1024);
long long insum=0, outsum=0;
volatile sig_atomic_t paused = 0;
volatile sig_atomic_t running = 1;
int mode = 1;
int n_level = 30;
int prev_n_level = 0;
int window_height = 1;
int rounding = 0;
int row_=0;
char path_[1000000][2048]={};
char evntnm[30]="";
long dir_sizes[ALL_DEPTH + 1] = {0}, dir_sizes_[ALL_DEPTH + 1] = {0};
long start_dir_size_[ALL_DEPTH + 1] = {0};
long start_dir_size = 0;
long current_dir_size = 0;
long data_rate_output = 0;
time_t start_time;
struct timespec last_time, last_io_time, current_time, init_time, delay_time;
double time_diff2=0;
char *directory = NULL;
char *inotify_log_path = "/dev/shm/inotify.lg";
int inotify_fd;
int inotify_wd;
char inotify_events[INOTIFY_LOG_SIZE][128];
struct stat st;
char pathfile[4096];
int tree_depth=0;
int initial_watches=0;
int event_counter = 0;
long evntcnt2=0;
int wtchcnt=1; //init base dir watch
unsigned int cnt02=0;
int counter1 = 1;
int counter2 = 1;
time_t start_date;

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
void posYX(int row, int col, int cursor_visible);
void process_inotify_events();
void process_inotify_events2();
void setup_inotify();
void update_inotify_log(const char *event_str);

void posYX(int row, int col, int cursor_visible) {
    move(row, col);
    if (cursor_visible) {
        curs_set(1);
    } else {
        curs_set(0);
    }
    refresh();
}

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
    posYX(47, 0, 0);
    printw("                                             \n");
    printw("       keys: search tree level == 'n'        \n");
    printw("             output mode       == 'm'        \n");
    printw("             pause             == 'p'        \n");
    printw("             resume            == ' ' or 'r' \n");
    printw("             clear screen      == 'c' or 'C' \n");
    printw("             help              == 'h' or 'H' or '?'\n");
    printw("             quit              == 'q' or 'Q' \n");
    printw("                                             \n");
    printw("       version 0.1.5 (C implementation)      \n");
    printw("       March 2025                            \n");
    printw("                                             \n");
    refresh();
}

long calculate_dir_size(const char *path, int max_depth) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    long size = 0, size_=0;
    static int current_depth = 0;

    if ((dir = opendir(path)) == NULL) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[PATH_MAX];
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
                //size += calculate_dir_size(full_path, max_depth);
                size_ = calculate_dir_size(full_path, max_depth);
                if (initial_watches==1) {
                  //start_dir_size_[current_depth]+=size_;
                  if (current_depth>tree_depth) tree_depth=current_depth;
                }
                size += size_;
                current_depth--;
            }

            if (initial_watches==1) {
            inotify_wd = inotify_add_watch(inotify_fd, full_path,
                                  IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
              if (inotify_wd == -1) {
                perror("inotify_add_watch");
                close(inotify_fd);
                exit(EXIT_FAILURE);

              }
              else
              {
              wtchcnt+=1;
              sprintf(path_[inotify_wd], "%s/%s", path, entry->d_name);
              }
          }

        } else {
            //!S_ISDIR
            size += statbuf.st_size;
            if (initial_watches==1) {
              start_dir_size_[current_depth]+=statbuf.st_size;
            }
            //if (current_depth==n_level+1) dir_sizes[current_depth]+=statbuf.st_size;
            dir_sizes[current_depth]+=statbuf.st_size;

        }
    }
    closedir(dir);
    return size;
}

// Function to calculate directory size recursively
long long calculate_directory_size_recrsv(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    long long total_size = 0;
    char full_path[PATH_MAX];

    // Open the directory
    if ((dir = opendir(path)) == NULL) {
        fprintf(stderr, "Error opening directory '%s': %s\n", path, strerror(errno));
        return -1;
    }

    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Get file/directory status
        if (lstat(full_path, &statbuf) == -1) {
            fprintf(stderr, "Error getting stats for '%s': %s\n", full_path, strerror(errno));
            continue;
        }

        if (S_ISLNK(statbuf.st_mode)) {
            // Skip symbolic links to prevent infinite loops
            continue;
        } else if (S_ISDIR(statbuf.st_mode)) {
            // Recursively calculate size for subdirectories
            long long subdir_size = calculate_directory_size_recrsv(full_path);
            if (subdir_size == -1) {
                closedir(dir);
                return -1;
            }
            total_size += subdir_size;
        } else {
            // Add file size (regular files, FIFOs, sockets, etc.)
            total_size += statbuf.st_size;
        }
    }

    // Add directory entry size (filesystem overhead)
    // Note: This is optional as it's filesystem dependent
    // total_size += statbuf.st_blksize * 1;  // Approx 1 block per directory

    if (closedir(dir) == -1) {
        fprintf(stderr, "Error closing directory '%s': %s\n", path, strerror(errno));
    }

    return total_size;
}



void setup_inotify() {
    inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

    inotify_wd = inotify_add_watch(inotify_fd, directory,
                                  IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (inotify_wd == -1) {
        perror("inotify_add_watch");
        close(inotify_fd);
        exit(EXIT_FAILURE);
    }
    initial_watches=1;
    calculate_dir_size(directory, 100);
    n_level=tree_depth;
    initial_watches=0;



}

void update_inotify_log(const char *event_str) {
    if (event_counter >= INOTIFY_LOG_SIZE) {
        // Shift events up
        for (int i = 0; i < INOTIFY_LOG_SIZE - 1; i++) {
            strncpy(inotify_events[i], inotify_events[i+1], sizeof(inotify_events[i]));
        }
        event_counter = INOTIFY_LOG_SIZE - 1;
    }
    strncpy(inotify_events[event_counter], event_str, sizeof(inotify_events[event_counter]) - 1);
    event_counter++;
    evntcnt2+=1;
}

long get_file_size(char *filename) {
    struct stat file_status;
    if (stat(filename, &file_status) < 0) {
        return -1;
    }

    return file_status.st_size;
}

void process_inotify_events() {
    char buffer[BUF_LEN];
    int length = read(inotify_fd, buffer, BUF_LEN);
    if (length < 0) {
        if (errno != EAGAIN) {
            perror("read");
        }
        return;
    }

    time_t now;
    struct tm *tm_info;
    char time_str[20];


    for (int i = 0; i < length;) {
        struct inotify_event *event = (struct inotify_event *) &buffer[i];

        time(&now);
        tm_info = localtime(&now);
        strftime(time_str, sizeof(time_str), "%m/%d/%Y %H:%M:%S", tm_info);
        float fsze;
        FILE *fp;

        //char event_str[128];
        char event_str[4096];
        if (event->len) {
            //sprintf(pathfile, "%s/%s", directory, event->name);
            snprintf(pathfile, sizeof(pathfile), "%s/%s", path_[event->wd], event->name);
            lstat(pathfile, &st);
            fsze=(float)(st.st_size/MB);

            if (event->mask & IN_CREATE) {
                sprintf(evntnm, "CREATE");
                snprintf(event_str, sizeof(event_str), "[%s] %s,%s,%i,CREATE,%.3f",
                         time_str, path_[event->wd], event->name, event->wd, fsze);
            } else if (event->mask & IN_DELETE) {
                sprintf(evntnm, "IN_DELETE");
                snprintf(event_str, sizeof(event_str), "[%s] %s/%s,%i,DELETE,%.3f",
                         time_str, path_[event->wd], event->name, event->wd, fsze);
            } else if (event->mask & IN_MODIFY) {
                sprintf(evntnm, "IN_MODIFY");
                snprintf(event_str, sizeof(event_str), "[%s] %s/%s,%i,MODIFY,%f",
                         time_str, path_[event->wd], event->name, event->wd, fsze);
            } else if (event->mask & IN_MOVED_FROM) {
                sprintf(evntnm, "IN_MOVED_FROM");
                snprintf(event_str, sizeof(event_str), "[%s] %s/%s,%i,MOVED_FROM,%f",
                         time_str, path_[event->wd], event->name, event->wd, fsze);
            } else if (event->mask & IN_MOVED_TO) {
                sprintf(evntnm, "IN_MOVED_TO");
                snprintf(event_str, sizeof(event_str), "[%s] %s/%s,%i,MOVED_TO,%.03f",
                         time_str, path_[event->wd], event->name, event->wd, fsze);
            } else if (event->mask & IN_ACCESS) {
                sprintf(evntnm, "IN_ACCESS");
                snprintf(event_str, sizeof(event_str), "[%s] %s/%s,%i,IN_ACCESS,%f",
                         //time_str, directory, event->name, fsze);
                         time_str, path_[event->wd], event->name, event->wd, fsze);
            }

            update_inotify_log(event_str);
        }

        i += EVENT_SIZE + event->len;
    }
}



void monitor_io() {

    process_inotify_events();

    // Update directory size based on depth level
    for (int i=0; i<=tree_depth; i++) dir_sizes[i]=0;
    if (n_level == 0 || n_level == tree_depth) {
        current_dir_size = calculate_dir_size(directory, tree_depth);
    } else {
        current_dir_size = calculate_dir_size(directory, n_level );
    }
    for (int i=1; i<=tree_depth; i++) dir_sizes[0]+=dir_sizes[i];


    prev_n_level = n_level;

    // Display inotify events
    //if (window_height > 50 && mode > 0) {
        posYX(48, 0, 0);
        for (int i = 0; i < event_counter && i < 15; i++) {
            int len = strlen(inotify_events[i]);
            //printw("\t%127s\n", inotify_events[i] + (len > 127 ? len - 127 : 0));
            printw("\t%.127s\n", inotify_events[i] + (len > 127 ? len - 127 : 0) );
            counter2++;
        }
        for (int i = 0; i < 2; i++) {
            printw("\n");
        }
    //}

    posYX(1, 20, 0);
    printw("start level_sizes [MB]: \n");
    posYX(1, 50, 0);
    for (int i = 0; i <= 10; i++) {
        printw("%12.06f_%02d ", (float)start_dir_size_[i]/MB, i);
    }
    printw("\n");
    printw("n %d treedp %d \n", n_level, tree_depth);
    posYX(2, 20, 0);
    printw("current level_sizes [MB]: \n");
    posYX(2, 50, 0);
    for (int i = 0; i <= tree_depth && i<=12; i++) {
        if (dir_sizes[i]!=dir_sizes_[i]) attron(A_REVERSE);
        //posYX(2, 50+(i*16), 0);
        printw("%12.06f_%02d ", (float)dir_sizes[i]/MB, i);
        clrtoeol();
        if (dir_sizes[i]!=dir_sizes_[i]) attroff(A_REVERSE);
    }
    for (int i=0; i<=tree_depth; i++) dir_sizes_[i]=dir_sizes[i];

    posYX(5, 30, 0);
    printw("base directory %s   current_dir_size %li ", directory, current_dir_size);
    //int mvprintw(int y, int x, const char *fmt, ...);
    //mvprintw(6, 30, " %s ", ctime(&start_date));

}

void calculate_data_rate() {
    time_t now = time(NULL);
    long uptime = now - start_time;

    data_rate_output = current_dir_size - start_dir_size;

    if (data_rate_output <= 0) {
      insum+=data_rate_output;
    } else {
      outsum+=data_rate_output;
    }


    posYX(7, 0, 0);
    attron(A_REVERSE);
    printw("Data rate io: %ld bytes/s  %.4f MB/s",
           data_rate_output, data_rate_output / (1024.0 * 1024.0));
    clrtoeol();

    posYX(7, 70, 0);
    printw("in sum: %.02f bytes out sum: %.02fMB  (%ld) ", (float) insum/MB, (float) outsum/MB, uptime );

    clrtoeol();
    attroff(A_REVERSE);

    if (mode > 2) {
        posYX(40, 0, 0);
        printw("Data Input Rate: %ld bytes/sec %.2f kB/s %.2f MB/s",
               data_rate_output,
               data_rate_output / 1024.0,
               data_rate_output / (1024.0 * 1024.0));

        posYX(41, 0, 0);
        printw("Data Output Rate: %ld bytes/sec %.2f kB/s %.2f MB/s",
               data_rate_output,
               data_rate_output / 1024.0,
               data_rate_output / (1024.0 * 1024.0));
    } else {
        for (int i = 40; i <= 43; i++) {
            posYX(i, 0, 0);
            clrtoeol();
        }
    }

    start_dir_size=current_dir_size;
}


void graphical_output(long data_rate_output, double time_diff) {
    int row, col;
    posYX(15, 0, 0);
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

    struct timespec ts;
    struct tm tmn;
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tmn);
    if (rounding) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d.%ld",
                tmn.tm_hour, tmn.tm_min, tmn.tm_sec, ts.tv_nsec / 1000);
    } else {
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d..%ld",
                tmn.tm_hour, tmn.tm_min, tmn.tm_sec, ts.tv_nsec / 1000);
    }


    double ioMBps = data_rate_output / (1024.0 * 1024.0);

    row_=(row_+1) % 20;

    posYX(row + row_, 0, 0);
    clrtoeol();  // Clear to end of line
    if (data_io != 0) {
        double time_diff3 = (current_time.tv_sec - last_io_time.tv_sec) +
                         (current_time.tv_nsec - last_io_time.tv_nsec) / 1e9;
        printw("%s %ld bytes/s %.3fs ", time_str, data_rate_output, time_diff3);
    } else {
        printw("   %s", time_str);
    }

    if (data_rate_output < 0) {
        posYX(row + row_, 53, 0);
        printw("%.2f MB/s", ioMBps);
    } else if (data_rate_output > 0) {
        posYX(row + row_, 112, 0);
        printw("%.2f MB/s", ioMBps);
    }

    posYX(row + row_, 70, 0);
    //clrtoeol();  // Clear to end of line
    printw("|");

    if (data_rate_output < 0) {
        posYX(row + row_, 90 - relh_pos, 0);
    } else {
        posYX(row + row_, 91, 0);
    }

    for (int i = 0; i < relh_pos; i++) {
        printw("~");
    }

    posYX(row + row_, 90, 0);
    printw("|");
    posYX(row + row_, 110, 0);
    printw("|");


    posYX(64, 40, 0);
    printw("pathfile %s %s", pathfile, evntnm);
    posYX(65, 40, 0);
    printw("row %d row_ %02d col %d data_rate_output %li relh_pos %d eventCounterSum %li watchCounter %d ", row, row_, col, data_rate_output, relh_pos, evntcnt2, wtchcnt);
    clrtoeol();

}

void cleanup() {
    if (inotify_fd >= 0) {
        inotify_rm_watch(inotify_fd, inotify_wd);
        close(inotify_fd);
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

char retstr[100]="";
// Human-readable format helper function
char* print_human_readable(long long bytes) {
    const char *units[] = {"bytes", "KB", "MB"};
    //const char *units[] = {"bytes", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;


    while (size >= 1024 && unit < (sizeof(units)/sizeof(units[0]) - 1)) {
        size /= 1024;
        unit++;
    }

    //printf("%.2f %s\n", size, units[unit]);
    sprintf(retstr, "%.2f %s\n", size, units[unit]);
    return retstr;
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

    //sleep(3);

    setup_signals();
    atexit(cleanup);
    init_ncurses();
    setup_inotify();

    posYX(5, 0, 1);
    printf("\nCalculating size for: %s\n\r", directory);

    //sleep(3);

    long long total_size = calculate_directory_size_recrsv(directory);
    if (total_size == -1) {
        return EXIT_FAILURE;
    }

    posYX(10, 0, 1);
    printf("Total size:\n\r");
    posYX(12, 0, 1);
    printf("%21lli bytes %6.06f MB  %6.06f GB\n\r", total_size, (float)total_size/(1024*1024), (float)total_size/GB );
    char dirsize[100]="";
    sprintf(dirsize, "%s", print_human_readable(total_size) );

    struct statvfs stats_;
    int ret = statvfs(directory, &stats_);
    double dirsize2 = (double)(stats_.f_blocks * stats_.f_frsize) / MB;
    double avail = (double)(stats_.f_bfree * stats_.f_frsize) / MB;

    sleep(3);


    // Set non-blocking mode for inotify
    int flags = fcntl(inotify_fd, F_GETFL, 0);
    fcntl(inotify_fd, F_SETFL, flags | O_NONBLOCK);

    // Initial directory size calculation
    start_dir_size = calculate_dir_size(directory, n_level + 1);
    start_dir_size_[0]=start_dir_size;
    current_dir_size = start_dir_size;
    start_time = time(NULL);

    clear();
    //time_t start_date = time(NULL);
    start_date = time(NULL);
    posYX(3, 0, 0);
    printw("monitoring start: %s", ctime(&start_date));
    posYX(3, 50, 0);
    printw("recursive size: %s   directory size: %.03fMB", dirsize, (float) start_dir_size/(1024*1024) );
    posYX(3, 100, 0);
    printw("statvfs disk size: %.03f    avail size: %.03fMB", dirsize2, (float) avail );
    refresh();

    //struct timespec last_time, current_time;
    clock_gettime(CLOCK_REALTIME, &last_time);
    last_io_time = last_time;

    while (running) {

        clock_gettime(CLOCK_REALTIME, &init_time);

        //posYX(4, 0, 0);
        posYX(4, 50, 0);
        printw("start_dir_size: %ldkB %ldMB  current_dir_size %ldkB %ldMB  io_diff %ldkB %.02fMB ", start_dir_size_[0]/1024, start_dir_size_[0]/(1024*1024), current_dir_size/1024, current_dir_size/(1024*1024), (start_dir_size_[0]-current_dir_size)/1024, (float)(start_dir_size_[0]-current_dir_size)/(1024*1024) );
        clrtoeol();
	posYX(4, 150, 0);
        printw("statvfs disk size: %.03fMB  avail: %.06fMB", dirsize2, (float) avail );
        clrtoeol();
        refresh();

        clock_gettime(CLOCK_REALTIME, &current_time);
        double time_diff = (current_time.tv_sec - last_time.tv_sec) +
                         (current_time.tv_nsec - last_time.tv_nsec) / 1e9;
        last_time = current_time;

        ret = statvfs(directory, &stats_);
        dirsize2 = (double)(stats_.f_blocks * stats_.f_frsize) / MB;
        avail = (double)(stats_.f_bfree * stats_.f_frsize) / MB;

        if (!paused) {
            monitor_io();
            calculate_data_rate();
            if (mode > 0) {
                graphical_output(data_rate_output, time_diff);
            }
        }
        else
        {
          usleep(10000);
        }
        if (data_rate_output != 0) last_io_time = current_time;


        if (kbhit()) {
            int ch = getch();
            posYX(46, 0, 0);
            printw("key pressed: '%c' (%d)", ch, ch);

            switch (ch) {
                case 'q':
                case 'Q':
                    running = 0;
                    break;
                case 'p':
                    paused = 1;
                    posYX(47, 0, 0);
                    printw("Output paused. Press space or key 'r' to resume.");
                    break;
                case ' ':
                case 'r':
                    paused = 0;
                    posYX(47, 0, 0);
                    printw("Output resumed.");
                    for (int i = 45; i <= 55; i++) {
                        posYX(i, 0, 0);
                        clrtoeol();
                    }
                    break;
                case 'm':
                    mode = (mode + 1) % 4;
                    posYX(46, 30, 0);
                    printw("m %d ", mode);
                    break;
                case 'n':
                    n_level = (n_level + 1) % (tree_depth+1);
                    if (n_level == 0 || n_level == tree_depth) {
                        posYX(65, 0, 0);
                        printw("n %d %d start_dir_size %li", n_level, prev_n_level, start_dir_size_[n_level] );
                        clrtoeol();
                        //current_dir_size = calculate_dir_size(directory, tree_depth);
		    } else {
                        posYX(65, 0, 0);
                        printw("n %d %d start_dir_size %li", n_level, prev_n_level, start_dir_size_[n_level] );
                        clrtoeol();
                        //current_dir_size = calculate_dir_size(directory, n_level + 1);
                    }
                    //start_dir_size = start_dir_size_[n_level];
                    //current_dir_size=dir_sizes[n_level];
                    break;
                case 't'|'T':
                //case 'T':
                    posYX(46, 50, 0);
                    printw(" '%c' start_dir_sizes tree", ch);
                    posYX(46, 100, 0);
                    printw("press key 'm' for continue");
                    mode = 0;
                    long int hd_dirs=0;
                    for (int i = 1; i <= 25; i++) {posYX(9+i, 0, 0); clrtoeol();}
                    posYX(15, 0, 0);
                    for (int i = 1; i <= tree_depth; i++) {
                      hd_dirs+=start_dir_size_[i];
                      printw("treedpth %i dir level usage: %.03fMB  head dirs: %.03fMB \n", i, (float)start_dir_size_[i]/MB, (float)hd_dirs/MB );
                    }
                    clrtoeol();
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

        counter1++;
        if (counter1 > 23) {
            counter1 = 1;
            rounding = 1 - rounding;
        }

        refresh();
        //usleep(100000); // 100ms delay
        //clock_gettime(CLOCK_REALTIME, &init_time);
        while ( time_diff2 < 0.1) {
        cnt02+=1;
        usleep(1);
        clock_gettime(CLOCK_REALTIME, &delay_time);
        time_diff2=(delay_time.tv_sec-init_time.tv_sec)+(delay_time.tv_nsec-init_time.tv_nsec)/1e9;
        }
        posYX(66, 40, 0);
        printw("time_diff %li %ld time_diff2 %f cnt02 %i", (delay_time.tv_sec-init_time.tv_sec), (delay_time.tv_nsec-init_time.tv_nsec)/1, time_diff2, cnt02 );  //1e9
        clrtoeol();
        time_diff2=0;
        cnt02=0;

    }

    time_t stop_date = time(NULL);
    posYX(47, 0, 0);
    printw("monitoring stop: %s", ctime(&stop_date));
    refresh();
    sleep(1);

    return 0;
}
