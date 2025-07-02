# Makefile for C Desktop App
CC = gcc
CFLAGS = -Wall -Wextra -std=c99
PLATFORM_FLAGS = -DPLATFORM_MACOS -framework Cocoa -framework Foundation -framework WebKit

# Source files
SRCS = main.c config.c webview_framework.c platform_macos.c bridge.c bridge_builtin.c bridge_custom.c
OBJS = $(SRCS:.c=.o)
TARGET = desktop_app

# Default target
.PHONY: all
all: $(TARGET)

# Build the application
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CC) $(CFLAGS) $(PLATFORM_FLAGS) -o $(TARGET) $(OBJS)
	@echo "Build complete!"

# Compile source files to object files
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(PLATFORM_FLAGS) -c $< -o $@

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET) $(OBJS)
	@echo "Clean complete!"

# Debug build with extra information
.PHONY: debug
debug: CFLAGS += -g -DDEBUG
debug: all

# Show help
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all        - Build the application (default)"
	@echo "  clean      - Remove build artifacts"
	@echo "  debug      - Build with debug symbols"
	@echo "  run        - Build and run the application"
	@echo "  help       - Show this help message"

# Show project info
.PHONY: info
info:
	@echo "Platform: $(shell uname)"

# Add after the existing targets
.PHONY: sync-types scripts-setup run
sync-types:
	@chmod +x scripts/sync-types.sh
	@./scripts/sync-types.sh

scripts-setup:
	@mkdir -p scripts
	@chmod +x scripts/run.sh
	@chmod +x scripts/sync-types.sh

# Update the build target to include types
build: scripts-setup sync-types $(TARGET)

# Add a run target
run: scripts-setup
	@./scripts/run.sh

.PHONY: all run clean debug help info 