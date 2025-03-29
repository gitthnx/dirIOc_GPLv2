# Compiler settings
CC = gcc
#CFLAGS = -Wall -Wextra -Werror -std=c11
CFLAGS = -Wall -Wextra
LIBFLAGS = -lncurses
TARGET = dirIO_v0.1.5_deepSeek
TARGET2 = dirIO_v0.1.5.1_27032025

# Source files
SRC = dirIO_v0.1.5_deepSeek.c
SRC2 = dirIO_v0.1.5.1_27032025.c
OBJ = $(SRC:.c=.o)
OBJ2 = $(SRC2:.c=.o)

# Default target
all: $(TARGET)
	@echo
	@echo "build targets: all, 0.1.5.1 (29032025) " 
	@echo "options: clean, install, uninstall"
	@echo 

0.1.5.1: $(TARGET2)  # 'make -j12 0.1.5.1'
	@echo
	@echo "options: clean, install, uninstall"
	@echo


# Build target executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBFLAGS)
$(TARGET2): $(OBJ2)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBFLAGS)


# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJ) $(TARGET)
	rm -f $(OBJ) $(TARGET2)

# Install to /usr/local/bin (requires sudo)
install: $(TARGET)
	install -m 0755 $(TARGET) /usr/local/bin/$(TARGET)
	install -m 0755 $(TARGET2) /usr/local/bin/$(TARGET2)

# Uninstall from /usr/local/bin
uninstall:
	rm -f /usr/local/bin/$(TARGET)
	rm -f /usr/local/bin/$(TARGET2)

.PHONY: all clean install uninstall
