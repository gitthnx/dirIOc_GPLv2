#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>
#include <sys/ioctl.h>
#include <wait.h>

// Defines
#define INOTIFY_PATH "/dev/shm/inotify.lg"
#define INOTIFY_MSG_PATH "/dev/shm/inotify_.msg"
#define INOTIFY_PART_PATH "/dev/shm/inotify_part.lg"
#define MAX_PATH_LENGTH 1024
#define MAX_COMMAND_LENGTH 256
#define BUFFER_SIZE 8192

// Global variables
static long long total_input = 0;
static long long total_output = 0;
static long long sum_in = 0;
static long long sum_out = 0;
static time_t start_time;
static struct timeval timePrev;
static bool paused = false;
static int mode = 1;
static int n_ = 10;
static int n2_ = 0;
static int cntr1 = 1;
static int cntr2 = 1;
static int winh_ = 1;
static int rnd_ = 0;
static long long dir_size_[11]; // Array for directory sizes
static long long start_dir_size = 0;
static long long current_dir_size = 0;
static char directory[MAX_PATH_LENGTH];
static pid_t inotify_pid = 0; // Initialize to 0
static struct termios original_term;
static int tty_fd;
static int screen_rows, screen_cols;
static bool running = true;
static char key = 0;
static double timeBtwIO;

// Function declarations
void sigterm_handler(int signum);
void sigterm_msg(const char *msg);
int posYX(int row, int col, int show_cursor);
long long get_directory_size(const char *path, int max_depth);
void monitor_io(void);
void calculate_data_rate(void);
void graphical_output(void);
void *run_inotify(void *arg);
int kbhit(void);
char getch(void);
void reset_terminal_mode(void);
void set_terminal_mode(void);
void get_window_size(void);
void cleanup(void);
void error_exit(const char *format, ...);
void print_usage_and_exit(const char *program_name);

// Helper function to print errors and exit
void error_exit(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    cleanup();
    exit(EXIT_FAILURE);
}

// Function to set terminal to non-canonical mode
void set_terminal_mode(void) {
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "Not a terminal.\n");
        exit(EXIT_FAILURE);
    }
    if (tcgetattr(STDIN_FILENO, &original_term) != 0)
        error_exit("tcgetattr failed: %s\n", strerror(errno));

    struct termios new_term = original_term;
    new_term.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echoing
    new_term.c_iflag &= ~(ICRNL | INLCR);
    new_term.c_cc[VMIN] = 0; // Return immediately, even if no input
    new_term.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) != 0)
        error_exit("tcsetattr failed: %s\n", strerror(errno));

    tty_fd = STDIN_FILENO;
}

// Function to reset terminal to original mode
void reset_terminal_mode(void) {
    if (isatty(STDIN_FILENO) &&
        tcsetattr(STDIN_FILENO, TCSANOW, &original_term) != 0) {
        fprintf(stderr, "tcsetattr failed: %s\n", strerror(errno));
    }
}

// Function to get window size
void get_window_size(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        screen_rows = ws.ws_row;
        screen_cols = ws.ws_col;
    } else {
        // Default values if ioctl fails
        screen_rows = 24;
        screen_cols = 80;
    }
}

// Function to check for keypress without blocking
int kbhit(void) {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

// Function to get a character from the keyboard
char getch(void) {
    char ch = 0;
    if (read(STDIN_FILENO, &ch, 1) < 0)
    {
       //perror("read()"); // Don't print error here, non-blocking
       return 0;
    }
    return ch;
}

// Function to set cursor position
int posYX(int row, int col, int show_cursor) {
    if (show_cursor) {
        printf("\e[?25h"); // Show cursor
    } else {
        printf("\e[?25l"); // Hide cursor
    }
    return printf("\e[%d;%dH", row, col);
}

// Signal handler function
void sigterm_handler(int signum) {
    (void)signum; // Suppress unused parameter warning
    running = false; // Set the running flag to false to exit the main loop
    // No need to call cleanup here, the main loop will handle it.
}

// Signal message function
void sigterm_msg(const char *msg) {
    posYX(50, 0, 0);
    printf("%s received, press 'q' or 'Q' to exit dirIO script \033[0K\n", msg);
}

// Function to get directory size
long long get_directory_size(const char *path, int max_depth) {
    long long size = 0;
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char full_path[MAX_PATH_LENGTH];
    int current_depth = 0;

    if (max_depth < 0)
        max_depth = 10;

    if (stat(path, &statbuf) == -1) {
        if (errno == ENOTDIR)
            return statbuf.st_size;
        else
            return 0; // Return 0 on error to keep going.
    }

    if (!S_ISDIR(statbuf.st_mode))
        return statbuf.st_size;

    if ((dir = opendir(path)) == NULL) {
        //perror("opendir");  //inside the main loop
        return 0; // Return 0.  Log and continue.
    }

    while ((entry = readdir(dir)) != NULL && running) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &statbuf) == -1) {
            //fprintf(stderr, "Error stating %s: %s\n", full_path, strerror(errno)); //in main loop
            continue;
        }

        size += statbuf.st_size;

        if (S_ISDIR(statbuf.st_mode) && current_depth < max_depth) {
            size += get_directory_size(full_path, max_depth);
        }
    }
    if (closedir(dir) != 0)
        perror("closedir");
    return size;
}

// Function to monitor I/O using inotify
void monitor_io(void) {
    long long rate_in = 0;
    long long rate_out = 0;
    FILE *inotify_file;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    inotify_file = fopen(INOTIFY_PATH, "r");
    if (inotify_file != NULL) {
        posYX(48, 0, 0);
        cntr2 = 1;
        while ((read = getline(&line, &len, inotify_file)) != -1) {
            printf("\033[1K\t%s", line);
            cntr2++;
        }
        free(line);
        fclose(inotify_file);
        // Clear the inotify log file.
        inotify_file = fopen(INOTIFY_PATH, "w");
        if (inotify_file != NULL)
            fclose(inotify_file);
    } else if (n_ != n2_) {
        posYX(48, 0, 0);
        printf("n_ = %d, n2_ = %d\n", n_, n2_);
    }

    if (n_ == 10) {
        current_dir_size = get_directory_size(directory, 10);
    } else {
        posYX(65, 0, 0);
        printf(" %d %d  ", n_, n2_);
        current_dir_size = get_directory_size(directory, n_ + 1);
        usleep(10000); // Sleep for 10ms
    }

    posYX(1, 0, 0);
    dir_size_[n_] = current_dir_size;
    dir_size_du = current_dir_size;
    for (int i = 0; i <= 10; i++) {
        printf("%d %lld \033[0K", i, dir_size_[i]);
    }
    printf("\n%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",
           dir_size_[0], dir_size_[1], dir_size_[2], dir_size_[3], dir_size_[4],
           dir_size_[5], dir_size_[6], dir_size_[7], dir_size_[8], dir_size_[9],
           dir_size_[10]);
    n2_ = n_;

    long long rate_io = current_dir_size - dir_size_du;
    if (rate_io > 0) {
        rate_in += rate_io;
        sum_in += rate_in;
    } else if (rate_io < 0) {
        rate_out += rate_io;
        sum_out += rate_out;
    }

    struct timeval timeNext;
    gettimeofday(&timeNext, NULL);
    timeBtwIO = (timeNext.tv_sec - timePrev.tv_sec) +
                         (timeNext.tv_usec - timePrev.tv_usec) / 1000000.0;
    if (rate_io != 0)
        timePrev = timeNext;

    dir_size_du = current_dir_size;

    char sum_in_str[32], sum_out_str[32];
    if (sum_in >= 1024 * 1024) {
        snprintf(sum_in_str, sizeof(sum_in_str), "%.2f MB", (double)sum_in / (1024 * 1024));
    } else if (sum_in >= 1024) {
        snprintf(sum_in_str, sizeof(sum_in_str), "%.2f kB", (double)sum_in / 1024);
    } else {
        snprintf(sum_in_str, sizeof(sum_in_str), "%lld B", sum_in);
    }

    if (sum_out >= 1024 * 1024) {
        snprintf(sum_out_str, sizeof(sum_out_str), "%.2f MB", (double)sum_out / (1024 * 1024));
    } else if (sum_out >= 1024) {
        snprintf(sum_out_str, sizeof(sum_out_str), "%.2f kB", (double)sum_out / 1024);
    } else {
        snprintf(sum_out_str, sizeof(sum_out_str), "%lld B", sum_out);
    }
    if (mode > 2) {
        posYX(40, 0, 0);
        printf("  Data Input Rate:  %lld  bytes/sec %.2f kB/s %.2f MB/s \033[0K\n",
               rate_in, (double)rate_in / 1024, (double)rate_in / (1024 * 1024));
        printf("  Data Output Rate: %lld bytes/sec  %.2f kB/s  %.2f MB/s \033[0K\n",
               rate_out, (double)rate_out / 1024, (double)rate_out / (1024 * 1024));
        printf("  Data Input Sum: %s  %.2f MB \033[0K\n", sum_in_str,
               (double)sum_in / (1024 * 1024));
        printf("  Data Output Sum: %s  bytes %.2f kB  %.2f MB \033[0K\n", sum_out_str,
               (double)sum_out / 1024, (double)sum_out / (1024 * 1024));
    } else {
        for (int i = 40; i <= 43; i++) {
            posYX(i, 0, 0);
            printf("\033[2K");
        }
    }
    char winsize_str[64];
    char winname_str[256];
    snprintf(winsize_str, sizeof(winsize_str), "%d %d", screen_cols, screen_rows);

    // Get window name.  This is dependent on xdotool
    FILE *cmd_pipe = popen("xwininfo -id $(xdotool getactivewindow) -all | awk -F ':' '/xwininfo/ {printf(\"%s %s\",$3,$4)}'", "r");
    if (cmd_pipe)
    {
        if (fgets(winname_str, sizeof(winname_str), cmd_pipe) == NULL)
            strcpy(winname_str, "Unknown");
        pclose(cmd_pipe);
    }
     else
        strcpy(winname_str, "Unknown");

    winh_ = (screen_rows - 400) * 100 / screen_rows;
    printf("  winsize %s  %d  %s \033[0K\n", winsize_str, winh_, winname_str);
}

// Function to calculate and display data rate
void calculate_data_rate(void) {
    posYX(7, 0, 0);
    printf("\e[132;7;3m"); // Highlighted text
    char start_date_str[64];
    struct tm *tm_info = localtime(&start_time);
    strftime(start_date_str, sizeof(start_date_str), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("%s    start_dir_size %lld kB  current_dir_size %lld kB  io diff %lld MB \033[0K\n",
           start_date_str, start_dir_size / 1024, current_dir_size / 1024,
           (current_dir_size - start_dir_size) / (1024 * 1024));

    const char *n_str = (n_ == 0)   ? "base dir"
                        : (n_ == 1) ? "1 dir level"
                        : (n_ == 10)  ? "all dir levels"
                                   : "%d dir levels";
    printf("pid_%d  m_%d n_%d for (%s of) %s \033[0K\n", getpid(), mode, n_, n_str,
           directory);
    printf("\e[0m"); // Reset

    time_t now;
    time(&now);
    long long uptime = now - start_time;

    long long data_rate_output = current_dir_size - dir_size_;
    printf("  data_rate_io %lld B/s \033[0K\n", data_rate_output);
    dir_size_ = current_dir_size;

    if (data_rate_output <= 0) {
        total_input += data_rate_output;
    } else {
        total_output += data_rate_output;
    }
    double in_sum_float = (double)total_input / (1024 * 1024);
    double out_sum_float = (double)total_output / (1024 * 1024);

    printf("  Data rate io: %lld bytes/s %.2f MB/s \033[0K\n", data_rate_output,
           (double)data_rate_output / (1024 * 1024));
    printf("  data io sum: %lld  %lld bytes \033[0K\n", total_input, total_output);
    printf("  data io sum: %.2f    %.2f MB (%llds) \033[0K\n", in_sum_float,
           out_sum_float, uptime);
    printf("\033[2K");
    get_window_size();
}

// Function for graphical output
void graphical_output(void) {
    posYX(12, 0, 0);
    int gpos = screen_rows + cntr1;

    long long data_io = abs(data_rate_output);
    int relh_pos;

    if (data_rate_output >= 10LL * 1024 * 1024 * 1024)
        relh_pos = 19;
    else if (data_io >= 1024 * 1024 * 1024)
        relh_pos = 16;
    else if (data_io >= 512 * 1024 * 1024)
        relh_pos = 9;
    else if (data_io >= 128 * 1024 * 1024)
        relh_pos = 7;
    else if (data_io >= 1024 * 1024)
        relh_pos = 5;
    else if (data_io >= 512 * 1024)
        relh_pos = 4;
    else if (data_io >= 64 * 1024)
        relh_pos = 3;
    else
        relh_pos = data_io / (22 * 1024);

    char date_str[64];
    struct tm *tm_info;
    time_t now = time(NULL);
    tm_info = localtime(&now);
    if (rnd_ == 1)
        strftime(date_str, sizeof(date_str), "%H:%M:%S.%03u", tm_info);
    else
        strftime(date_str, sizeof(date_str), "%H:%M:%S.%02u", tm_info);

    posYX(gpos, 5, 0);
    printf("\033[1K%s %lld bytes/s %.2fs \033[0K", date_str, data_rate_output, timeBtwIO);

    char ioMBps[32];
    snprintf(ioMBps, sizeof(ioMBps), "%.2f MB/s",
             (double)data_rate_output / (1024 * 1024));

    if (data_rate_output <= 0)
        posYX(gpos, 53, 0);
    else
        posYX(gpos, 112, 0);
    printf("%s", ioMBps);

    posYX(gpos, 70, 0);
    printf("|");
    if (data_rate_output <= 0)
        posYX(gpos, 90 - relh_pos, 0);
    else
        posYX(gpos, 90, 0);
    for (int i = 0; i < relh_pos; i++)
        printf("~");
    posYX(gpos, 90, 0);
    printf("|");
    posYX(gpos, 110, 0);
    printf("|\n");

    cntr1++;
    if (cntr1 > 23) {
        cntr1 = 1;
        rnd_ = 1 - rnd_;
    }
}

// Function to run inotifywait in a separate thread
void *run_inotify(void *arg) {
    (void)arg; // Suppress unused parameter warning
    char command[MAX_COMMAND_LENGTH];
    snprintf(command, sizeof(command),
             "/usr/bin/inotifywait -e create,modify,move,delete -r -m "
             "--timefmt '%%m/%%d/%%Y %%H:%%M:%%S' --format '[%%T] %%w,%%f,%%e,%%x' "
             "-o " INOTIFY_PATH " --exclude " INOTIFY_PATH " %s",
             directory); // Full path to inotifywait

    // Redirect standard error to /dev/null
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull == -1) {
        perror("open /dev/null");
        return NULL; // Return NULL to indicate failure
    }
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    // Execute the inotifywait command using execl
    if (execl("/bin/sh", "/bin/sh", "-c", command, (char *)NULL) == -1)
    {
       perror("execl");
       return NULL;
    }
    // If execl returns, an error occurred
    perror("execl");
    return NULL; // Return NULL to indicate failure
}

void cleanup(void) {
    reset_terminal_mode();
    if (inotify_pid > 0) {
        kill(inotify_pid, SIGTERM);
        waitpid(inotify_pid, NULL, 0);
    }
    remove(INOTIFY_MSG_PATH);
    remove(INOTIFY_PART_PATH);
}

void print_usage_and_exit(const char *program_name) {
    printf("Usage: %s /directory/to/monitor\n", program_name);
    printf(
        "      keys: search tree level == 'n'\n"
        "            output mode      == 'm'\n"
        "            pause           == 'p'\n"
        "            resume          == ' ' or 'r'\n"
        "            clear screen      == 'c' or 'C'\n"
        "            help            == 'h' or 'H' or '?'\n"
        "            quit            == 'q' or 'Q'\n"
        "\n"
        "      version 0.1.6\n"
        "      March 15, 2025\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 ||
                       strcmp(argv[1], "-?") == 0 || strcmp(argv[1], "/?") == 0)) {
        print_usage_and_exit(argv[0]);
    }

    if (argc != 2) {
        print_usage_and_exit(argv[0]);
    }

    if (strcmp(argv[1], "/") == 0) {
        fprintf(stderr, "*** no root fs io monitoring recommended ***\n");
        exit(EXIT_FAILURE);
    }

    if (strlen(argv[1]) >= MAX_PATH_LENGTH) {
        fprintf(stderr, "Error: Directory path too long (max %d characters)\n",
                MAX_PATH_LENGTH - 1);
        exit(EXIT_FAILURE);
    }
    // Initialize
    strncpy(directory, argv[1], MAX_PATH_LENGTH - 1);
    directory[MAX_PATH_LENGTH - 1] = '\0'; // Ensure null termination

    if (access(directory, F_OK) != 0) {
        fprintf(stderr, "Error: Directory '%s' does not exist or is not accessible.\n", directory);
        exit(EXIT_FAILURE);
    }

    set_terminal_mode();
    get_window_size();
    signal(SIGTERM, sigterm_handler);
    atexit(cleanup);

    start_dir_size = get_directory_size(directory, 10);

    time(&start_time);
    gettimeofday(&timePrev, NULL);

    pthread_t inotify_thread;
    if (pthread_create(&inotify_thread, NULL, run_inotify, NULL) != 0) {
        error_exit("pthread_create failed: %s\n", strerror(errno));
    }

    // Detach the thread, so we don't need to join it.  The thread will be terminated
    // when the program exits.
    if (pthread_detach(inotify_thread) != 0)
    {
        error_exit("pthread_detach failed: %s\n", strerror(errno));
    }

    // Store the inotify_pid.  This is a global variable
    FILE *pid_file = fopen(INOTIFY_MSG_PATH, "r");
    if (pid_file != NULL)
    {
        if (fscanf(pid_file, "pid of inotifywait&: %d", &inotify_pid) != 1)
            inotify_pid = 0; //error
        fclose(pid_file);
    }

    posYX(3, 0, 0);
    printf("monitoring start: %s\n", ctime(&start_time));
    printf("directory size (find -type cmd) %lld kB, directory size (du cmd) %lld kB\n", start_dir_size / 1024, start_dir_size / 1024);
    printf("\n");
    sleep(1);

    // Main loop
    while (running) {
        if (!paused) {
            calculate_data_rate();
            if (mode > 0)
                graphical_output();
            monitor_io();
        }

        if (kbhit())
        {
            key = getch();
            posYX(45, 0, 0);
            printf("  key(s) pressed: '%c' \033[0K\n", key);
            //printf("%d\n", key);
            switch (key) {
                case 'q':
                case 'Q':
                    posYX(47, 0, 0);
                    time_t now = time(NULL);
                    printf("monitoring stop: %s\n", ctime(&now));
                    printf("  key(s) pressed: '%c'\n", key);
                    running = false; // Set running to false to exit the loop.
                    break;
                case 'p':
                    paused = true;
                    printf("Output paused. Press space or key 'r' to resume.\n");
                    break;
                case ' ':
                case 'r':
                    mode = mode;
                    paused = false;
                    posYX(47, 0, 0);
                    printf("Output resumed. \033[0K\n");
                    for (int i = 45; i <= 55; i++) {
                        posYX(i, 0, 0);
                        printf("\033[2K");
                    }
                    break;
                case 'm':
                    mode = (mode + 1) % 4; // Cycle through modes 0, 1, 2, 3
                    break;
                case 'n':
                    n_ = (n_ + 1) % 11; // Cycle through 0-10
                    if (n_ == 10)
                        current_dir_size = get_directory_size(directory, 10);
                    else
                    {
                        posYX(65, 0, 0);
                        printf(" %d %d  ", n_, n2_);
                        current_dir_size = get_directory_size(directory, n_ + 1);
                        usleep(10000);
                    }
                    dir_size_ = current_dir_size;
                    break;
                case 'c':
                case 'C':
                    system("clear");
                    break;
                case 'h':
                case 'H':
                case '?':
                    mode = 0;
                    posYX(47, 0, 0);
                    print_usage_and_exit(argv[0]); //打印帮助信息
                    break;
            }
        }
        usleep(100000); // Sleep for 100ms
    }

    cleanup(); // Clean up resources before exiting.
    return 0;
}
