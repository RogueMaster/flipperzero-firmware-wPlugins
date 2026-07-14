# Makefile for Flipper Printer
# This Makefile provides convenient shortcuts for FBT commands
# Note: You must have the Flipper Zero firmware repository set up

# Configuration
APP_ID = flipper_printer
FBT_PATH = ../../../fbt  # Adjust this path to your FBT location
UFBT = ufbt  # If you have uFBT installed

# Default target
.PHONY: all
all: build

# Build the application
.PHONY: build
build:
	@echo "Building Flipper Printer..."
	@if command -v $(UFBT) >/dev/null 2>&1; then \
		echo "Using uFBT..."; \
		$(UFBT); \
	elif [ -f "$(FBT_PATH)" ]; then \
		echo "Using FBT..."; \
		$(FBT_PATH) fap_$(APP_ID); \
	else \
		echo "Error: Neither uFBT nor FBT found!"; \
		echo "Install uFBT with: pip3 install --upgrade ufbt"; \
		echo "Or adjust FBT_PATH in this Makefile"; \
		exit 1; \
	fi

# Build and upload to Flipper
.PHONY: upload
upload:
	@echo "Building and uploading to Flipper..."
	@if command -v $(UFBT) >/dev/null 2>&1; then \
		$(UFBT) launch; \
	elif [ -f "$(FBT_PATH)" ]; then \
		$(FBT_PATH) fap_$(APP_ID) upload; \
	else \
		echo "Error: Neither uFBT nor FBT found!"; \
		exit 1; \
	fi

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf dist/
	@rm -rf .ufbt/
	@if command -v $(UFBT) >/dev/null 2>&1; then \
		$(UFBT) -c; \
	elif [ -f "$(FBT_PATH)" ]; then \
		$(FBT_PATH) -c fap_$(APP_ID); \
	fi

# Generate VS Code configuration
.PHONY: vscode
vscode:
	@echo "Generating VS Code configuration..."
	@if command -v $(UFBT) >/dev/null 2>&1; then \
		$(UFBT) vscode_dist; \
	else \
		echo "This command requires uFBT"; \
		echo "Install with: pip3 install --upgrade ufbt"; \
		exit 1; \
	fi

# Format code
.PHONY: format
format:
	@echo "Formatting code..."
	@if [ -f "$(FBT_PATH)" ]; then \
		$(FBT_PATH) format; \
	else \
		echo "Warning: FBT not found, using clang-format directly"; \
		find . -name "*.c" -o -name "*.h" | xargs clang-format -i; \
	fi

# Lint code
.PHONY: lint
lint:
	@echo "Linting code..."
	@if [ -f "$(FBT_PATH)" ]; then \
		$(FBT_PATH) lint; \
	else \
		echo "Warning: FBT not found"; \
	fi

# Debug build
.PHONY: debug
debug:
	@echo "Building debug version..."
	@if command -v $(UFBT) >/dev/null 2>&1; then \
		$(UFBT) -DDEBUG=1; \
	elif [ -f "$(FBT_PATH)" ]; then \
		$(FBT_PATH) fap_$(APP_ID) DEBUG=1; \
	fi

# Install uFBT (standalone build tool)
.PHONY: install-ufbt
install-ufbt:
	@echo "Installing uFBT..."
	pip3 install --upgrade ufbt

# Help
.PHONY: help
help:
	@echo "Flipper Printer Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  make build       - Build the application"
	@echo "  make upload      - Build and upload to Flipper"
	@echo "  make clean       - Clean build artifacts"
	@echo "  make vscode      - Generate VS Code configuration"
	@echo "  make format      - Format source code"
	@echo "  make lint        - Lint source code"
	@echo "  make debug       - Build debug version"
	@echo "  make install-ufbt - Install uFBT build tool"
	@echo "  make help        - Show this help message"
	@echo ""
	@echo "Configuration:"
	@echo "  FBT_PATH = $(FBT_PATH)"
	@echo "  APP_ID = $(APP_ID)"