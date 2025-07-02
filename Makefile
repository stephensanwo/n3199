# Clean Makefile for C Desktop Application
# Build logic has been moved to scripts for better maintainability

.DEFAULT_GOAL := help

# Project configuration
PROJECT_NAME = C Desktop Application
SCRIPTS_DIR = scripts

# Ensure scripts are executable
.PHONY: scripts-setup
scripts-setup:
	@chmod +x $(SCRIPTS_DIR)/*.sh

# Setup project dependencies and environment
.PHONY: setup
setup: scripts-setup
	@./$(SCRIPTS_DIR)/setup.sh

# Build the application
.PHONY: build
build: scripts-setup
	@./$(SCRIPTS_DIR)/build.sh

# Build with debug symbols
.PHONY: debug
debug: scripts-setup
	@./$(SCRIPTS_DIR)/build.sh --debug

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -f desktop_app *.o
	@echo "✓ Clean completed"

# Clean and rebuild
.PHONY: rebuild
rebuild: scripts-setup
	@./$(SCRIPTS_DIR)/build.sh --clean

# Build and run the application
.PHONY: run
run: scripts-setup
	@./$(SCRIPTS_DIR)/run.sh

# Run in debug mode
.PHONY: run-debug
run-debug: scripts-setup
	@./$(SCRIPTS_DIR)/run.sh --debug

# Sync TypeScript bridge types
.PHONY: sync-types
sync-types: scripts-setup
	@./$(SCRIPTS_DIR)/sync-types.sh

# Show project information
.PHONY: info
info:
	@echo "$(PROJECT_NAME)"
	@echo "Platform: $(shell uname -s)"
	@echo "Architecture: $(shell uname -m)"
	@echo "Scripts directory: $(SCRIPTS_DIR)"

# Show available targets
.PHONY: help
help:
	@echo "$(PROJECT_NAME) - Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  setup      - Setup project dependencies and environment"
	@echo "  build      - Build the application"
	@echo "  debug      - Build with debug symbols"
	@echo "  clean      - Remove build artifacts"
	@echo "  rebuild    - Clean and rebuild"
	@echo "  run        - Build and run the application"
	@echo "  run-debug  - Build and run in debug mode"
	@echo "  sync-types - Sync TypeScript bridge types"
	@echo "  info       - Show project information"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "For first-time setup, run: make setup"
	@echo "For normal development, run: make run"

# Legacy compatibility (redirect to scripts)
.PHONY: all
all: build 