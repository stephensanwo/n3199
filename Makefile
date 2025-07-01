# Makefile for C Desktop App
CC = gcc
CFLAGS = -Wall -Wextra -std=c99
FRAMEWORKS = -framework Cocoa -framework Foundation
TARGET = desktop_app
SOURCE = main.c

# Default target
all: $(TARGET)

# Build the application
$(TARGET): $(SOURCE)
	@echo "Building $(TARGET)..."
	$(CC) $(CFLAGS) $(FRAMEWORKS) -o $(TARGET) $(SOURCE)
	@echo "Build complete! Run with: ./$(TARGET)"

# Run the application
run: $(TARGET)
	@echo "Running $(TARGET)..."
	./$(TARGET)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET)
	@echo "Clean complete!"

# Show help
help:
	@echo "Available targets:"
	@echo "  all    - Build the application (default)"
	@echo "  run    - Build and run the application"
	@echo "  clean  - Remove build artifacts"
	@echo "  help   - Show this help message"

.PHONY: all run clean help 