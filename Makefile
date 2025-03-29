# Compiler settings
CC = gcc
#CFLAGS = -Wall -Wextra -Werror -std=c11
CFLAGS = -Wall -Wextra
LIBFLAGS = -lncurses	
TARGET = tmp_C/dirIO_v0.1.5_deepSeek

# Source files
SRC = tmp_C/dirIO_v0.1.5_deepSeek.c
OBJ = $(SRC:.c=.o)

# Default target
all: $(TARGET)

# Build target executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBFLAGS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJ) $(TARGET)

# Install to /usr/local/bin (requires sudo)
install: $(TARGET)
	install -m 0755 $(TARGET) /usr/local/bin/$(TARGET)

# Uninstall from /usr/local/bin
uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall
