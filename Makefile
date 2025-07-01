# Makefile for C Desktop App
CC = gcc
CFLAGS = -Wall -Wextra -std=c99
FRAMEWORKS = -framework Cocoa -framework Foundation
TARGET = desktop_app

# Source files
SOURCES = main.c config.c
HEADERS = config.h platform.h

# Platform-specific sources
ifeq ($(shell uname),Darwin)
    SOURCES += platform_macos.c
    PLATFORM_CFLAGS = -DPLATFORM_MACOS
endif

# Objects
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET)

# Build the application
$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CC) $(CFLAGS) $(PLATFORM_CFLAGS) $(FRAMEWORKS) -o $(TARGET) $(OBJECTS)
	@echo "Build complete! Run with: ./$(TARGET)"

# Compile source files to object files
%.o: %.c $(HEADERS)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(PLATFORM_CFLAGS) -c $< -o $@

# Run the application
run: $(TARGET)
	@echo "Running $(TARGET)..."
	./$(TARGET)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET) $(OBJECTS)
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
	@echo "Source files: $(SOURCES)"
	@echo "Platform: $(shell uname)"

# Show project info
info:
	@echo "=== Project Information ==="
	@echo "Target: $(TARGET)"
	@echo "Sources: $(SOURCES)"
	@echo "Headers: $(HEADERS)"
	@echo "Platform: $(shell uname)"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS) $(PLATFORM_CFLAGS)"
	@echo "Frameworks: $(FRAMEWORKS)"

.PHONY: all run clean debug help info 