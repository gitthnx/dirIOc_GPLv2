#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MAX_PATH_LENGTH 1024
#define INOTIFY_PATH "/dev/shm/inotify.lg"

// Global variables
long long total_input = 0, total_output = 0, sum_in = 0, sum_out = 0;
long long dir_size_du = 0, current_dir_size = 0, start_dir_size = 0;
int mode = 1;  // Display mode (0-3)
int n_ = 10;   // Directory depth level
int paused = 0; // Paused state
time_t start_time;

// Function to handle SIGTERM
void sigterm_handler(int sig) {
    printf("Signal %d received, press 'q' or 'Q' to exit the program.\n", sig);
}

// Function to get directory size
long long get_directory_size(const char *dir) {
    DIR *d = opendir(dir);
    struct dirent *entry;
    struct stat statbuf;
    long long size = 0;

    if (!d) return 0;

    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char path[MAX_PATH_LENGTH];
            snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
            if (stat(path, &statbuf) == 0) {
                if (S_ISDIR(statbuf.st_mode)) {
                    size += get_directory_size(path);  // Recursively add directory size
                } else {
                    size += statbuf.st_size;  // Add file size
                }
            }
        }
    }

    closedir(d);
    return size;
}

// Function to monitor I/O using inotify (simplified, adjust as needed)
void monitor_io(const char *dir) {
    // For demonstration, we'll just simulate directory size changes
    long long new_dir_size = get_directory_size(dir);
    long long rate_io = new_dir_size - current_dir_size;

    // Update cumulative stats
    if (rate_io > 0) {
        total_input += rate_io;
        sum_in += rate_io;
    } else if (rate_io < 0) {
        total_output += rate_io;
        sum_out += rate_io;
    }

    current_dir_size = new_dir_size;

    // Print I/O rates and stats (simplified)
    if (mode > 2) {
        printf("Data Input Rate: %lld bytes/sec\n", rate_io);
        printf("Data Output Rate: %lld bytes/sec\n", -rate_io);
        printf("Total Input: %lld bytes\n", total_input);
        printf("Total Output: %lld bytes\n");
    }
}

// Function to update graphical output (simplified for console)
void graphical_output() {
    time_t now = time(NULL);
    printf("Time: %s", ctime(&now));
    printf("Data rate: %lld bytes/sec\n", total_input - total_output);
}

// Function to handle key press and user input
void handle_key_press(char key) {
    switch (key) {
        case 'q':
        case 'Q':
            printf("Exiting program.\n");
            exit(0);
            break;
        case 'p':
            paused = !paused;
            printf("Paused: %d\n", paused);
            break;
        case 'm':
            mode = (mode + 1) % 4;
            printf("Mode: %d\n", mode);
            break;
        case 'n':
            n_ = (n_ + 1) % 11;
            printf("Directory depth level: %d\n", n_);
            break;
        case 'c':
            system("clear");
            break;
        default:
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <directory_to_monitor>\n", argv[0]);
        return 1;
    }

    char *dir = argv[1];
    if (access(dir, F_OK) != 0) {
        perror("Directory does not exist");
        return 1;
    }

    // Set signal handlers
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    // Initial directory size calculations
    start_time = time(NULL);
    start_dir_size = get_directory_size(dir);
    current_dir_size = start_dir_size;

    printf("Monitoring directory: %s\n", dir);

    // Main monitoring loop
    while (1) {
        if (!paused) {
            monitor_io(dir);  // Monitor directory I/O

            // Graphical output (simplified)
            graphical_output();
        }

        // Handle key press (non-blocking input simulation)
        char key = getchar();
        if (key != EOF) {
            handle_key_press(key);
        }

        usleep(100000);  // Sleep for 100ms (adjust as needed)
    }

    return 0;
}
