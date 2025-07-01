# Makefile for C Desktop App
CC = gcc
CFLAGS = -Wall -Wextra -std=c99
PLATFORM_FLAGS =

ifeq ($(shell uname),Darwin)
    PLATFORM_FLAGS += -DPLATFORM_MACOS
    PLATFORM_FLAGS += -framework Cocoa -framework Foundation -framework WebKit
    PLATFORM_NAME = macos
    PLATFORM_SRC = platform_macos.c
    PLATFORM_OBJ = platform_macos.o
else
    $(error Unsupported platform)
endif

SRC = main.c config.c $(PLATFORM_SRC)
OBJ = $(SRC:.c=.o)
TARGET = desktop_app

# Default target
all: $(TARGET)

# Build the application
$(TARGET): $(OBJ)
	@echo "Linking $(TARGET)..."
	$(CC) $(CFLAGS) $(PLATFORM_FLAGS) -o $(TARGET) $(OBJ)
	@echo "Build complete! Run with: ./$(TARGET)"

# Compile source files to object files
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(PLATFORM_FLAGS) -c $< -o $@

# Run the application
run: $(TARGET)
	@echo "Running $(TARGET)..."
	./$(TARGET)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET) $(OBJ)
	@echo "Clean complete!"

# Debug build with extra information
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Show help
help:
	@echo "Available targets:"
	@echo "  all    - Build the application (default)"
	@echo "  debug  - Build with debug information"
	@echo "  run    - Build and run the application"
	@echo "  clean  - Remove build artifacts"
	@echo "  help   - Show this help message"
	@echo ""
	@echo "Source files: $(SRC)"
	@echo "Platform: $(shell uname)"

# Show project info
info:
	@echo "=== Project Information ==="
	@echo "Target: $(TARGET)"
	@echo "Sources: $(SRC)"
	@echo "Platform: $(shell uname)"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS) $(PLATFORM_FLAGS)"
	@echo "Platform: $(PLATFORM_NAME)"

.PHONY: all run clean debug help info 