# Makefile for the Lyrics Display System (C++17 Unified Binary)
#
# Prerequisites:
#   - CMake 3.20+
#   - C++17 compiler (GCC 9+ or Clang 10+)
#   - GTK4 development libraries (libgtk-4-dev)
#   - libcurl development (libcurl4-openssl-dev)
#   - libdbus development (libdbus-1-dev)
#   - libspdlog-dev (optional, fetched automatically if missing)
#
# Quick start:
#   make              - Build the lyrics binary -> build/lyrics
#   make run          - Build and run (GTK GUI mode)
#   make run-console  - Build and run (console/TUI mode, --console)
#   make run-headless - Build and run (backend only, --no-gui)
#
# Install:
#   make install        - Install to /usr/local/bin (requires sudo)
#   make install-user   - Install to ~/.local/bin
#
# Clean:
#   make clean          - Remove build artifacts

BINARY_NAME := lyrics
BUILD_DIR   := build
TARGET      := $(BUILD_DIR)/$(BINARY_NAME)

CMAKE       := cmake
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release

# Installation
INSTALL_PREFIX  := /usr/local
INSTALL_BIN_DIR := $(INSTALL_PREFIX)/bin
USER_BIN_DIR    := $(HOME)/.local/bin

.PHONY: all run run-console run-headless install install-user clean help

all: $(TARGET)
	@echo "Build complete: $(TARGET)"

help:
	@echo "Usage: make [target]"
	@echo ""
	@echo "Build targets:"
	@echo "  all              - Build the lyrics binary -> $(TARGET) (default)"
	@echo ""
	@echo "Run targets:"
	@echo "  run              - Build and run (GTK GUI mode)"
	@echo "  run-console      - Build and run (console TUI mode, --console)"
	@echo "  run-headless     - Build and run (backend only, --no-gui)"
	@echo ""
	@echo "Install targets:"
	@echo "  install          - Install to $(INSTALL_BIN_DIR) (requires sudo)"
	@echo "  install-user     - Install to $(USER_BIN_DIR)"
	@echo ""
	@echo "Other targets:"
	@echo "  clean            - Remove build artifacts"
	@echo "  help             - Show this help message"

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(CMAKE) .. $(CMAKE_FLAGS)

$(TARGET): $(BUILD_DIR)
	cd $(BUILD_DIR) && $(CMAKE) --build . -j$$(nproc)

run: $(TARGET)
	@echo "Running lyrics (GUI mode)..."
	./$(TARGET)

run-console: $(TARGET)
	@echo "Running lyrics (console mode)..."
	./$(TARGET) --console

run-headless: $(TARGET)
	@echo "Running lyrics (headless mode)..."
	./$(TARGET) --no-gui

install: $(TARGET)
	@echo "Installing to $(INSTALL_BIN_DIR)..."
	@if [ ! -w "$(INSTALL_BIN_DIR)" ] && [ ! -w "$$(dirname $(INSTALL_BIN_DIR))" ]; then \
		echo "✗ Permission denied. Run 'sudo make install'"; \
		exit 1; \
	fi
	@mkdir -p $(INSTALL_BIN_DIR)
	cp $(TARGET) $(INSTALL_BIN_DIR)/$(BINARY_NAME) && chmod +x $(INSTALL_BIN_DIR)/$(BINARY_NAME)
	@echo "✓ Installed $(BINARY_NAME) to $(INSTALL_BIN_DIR)/$(BINARY_NAME)"

install-user: $(TARGET)
	@echo "Installing to $(USER_BIN_DIR)..."
	@mkdir -p $(USER_BIN_DIR)
	cp $(TARGET) $(USER_BIN_DIR)/$(BINARY_NAME) && chmod +x $(USER_BIN_DIR)/$(BINARY_NAME)
	@echo "✓ Installed $(BINARY_NAME) to $(USER_BIN_DIR)/$(BINARY_NAME)"
	@echo ""
	@echo "Make sure $(USER_BIN_DIR) is in your PATH:"
	@echo "  echo 'export PATH=\"\$$HOME/.local/bin:\$$PATH\"' >> ~/.bashrc"

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	@echo "Cleanup complete."
