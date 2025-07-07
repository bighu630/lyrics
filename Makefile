# Makefile for the Lyrics Project

# --- Variables ---

# Installation paths
INSTALL_PREFIX := /usr/local
INSTALL_BIN_DIR := $(INSTALL_PREFIX)/bin
INSTALL_BACKEND := $(INSTALL_BIN_DIR)/$(GO_BINARY_NAME)
INSTALL_GUI := $(INSTALL_BIN_DIR)/$(CPP_BINARY_NAME)

# User installation paths (for install-user target)
USER_BIN_DIR := $(HOME)/.local/bin
USER_INSTALL_BACKEND := $(USER_BIN_DIR)/$(GO_BINARY_NAME)
USER_INSTALL_GUI := $(USER_BIN_DIR)/$(CPP_BINARY_NAME)

# Go Backend
GO_BINARY_NAME := lyrics-backend
GO_BUILD_DIR := build
GO_MAIN_PATH := ./go-backend/cmd/lyrics-backend
GO_TARGET := $(GO_BUILD_DIR)/$(GO_BINARY_NAME)

# C++ GUI
CPP_BINARY_NAME := lyrics-gui
CPP_BUILD_DIR := build
CPP_SRC_DIR := cpp-gui
CPP_TARGET := $(CPP_BUILD_DIR)/$(CPP_BINARY_NAME)

# C++ Source and Object files
CPP_MAIN_SRC := $(CPP_SRC_DIR)/main.cpp
CPP_OBJS := $(CPP_BUILD_DIR)/main.o

# Compiler and Flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall `pkg-config --cflags gtk4` -I$(CPP_BUILD_DIR)
LDFLAGS := `pkg-config --libs gtk4`

# --- Build Targets ---

.PHONY: all backend gui clean run-backend run-gui install install-user uninstall uninstall-user help

all: backend gui
	@echo "Build complete for all targets."

help:
	@echo "Available targets:"
	@echo "  all             - Build both the backend and the GUI (default)"
	@echo "  backend         - Build the Go backend"
	@echo "  gui             - Build the C++ GUI"
	@echo "  run-backend     - Build and run the Go backend"
	@echo "  run-gui         - Build and run the C++ GUI (window mode - default)"
	@echo "  run-console     - Build and run the C++ GUI (console mode)"
	@echo "  install         - Install binaries to system directories (requires sudo)"
	@echo "  install-user    - Install binaries to user directory (~/.local/bin)"
	@echo "  uninstall       - Remove installed binaries from system"
	@echo "  uninstall-user  - Remove installed binaries from user directory"
	@echo "  clean           - Remove all build artifacts"
	@echo "  help            - Show this help message"

# --- Go Backend Targets ---

backend: $(GO_TARGET)

$(GO_TARGET):
	@echo "Building Go backend..."
	@mkdir -p $(GO_BUILD_DIR)
	cd go-backend && CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -o ../$(GO_TARGET) ./cmd/lyrics-backend

run-backend: backend
	@echo "Running Go backend..."
	./$(GO_TARGET)

# --- C++ GUI Targets ---

gui: $(CPP_TARGET)

$(CPP_TARGET): $(CPP_OBJS)
	@echo "Linking C++ GUI..."
	$(CXX) -o $@ $^ $(LDFLAGS)

$(CPP_BUILD_DIR)/main.o: $(CPP_MAIN_SRC)
	@echo "Compiling main.cpp..."
	@mkdir -p $(CPP_BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(CPP_PROTOCOL_C_SRC): $(XDG_SHELL_PROTOCOL)
	@echo "Generating Wayland private code..."
	@mkdir -p $(CPP_BUILD_DIR)
	wayland-scanner private-code < $< > $@

run-gui: gui
	@echo "Running C++ GUI in window mode..."
	./$(CPP_TARGET)

run-console: gui
	@echo "Running C++ GUI in console mode..."
	./$(CPP_TARGET) --console

# --- Installation ---

install: all
	@echo "Installing binaries to system directories..."
	@echo "Installing to: $(INSTALL_BIN_DIR)"
	@if [ ! -w "$(INSTALL_BIN_DIR)" ] && [ ! -w "$$(dirname $(INSTALL_BIN_DIR))" ]; then \
		echo "✗ Permission denied. Please run 'sudo make install'"; \
		exit 1; \
	fi
	@mkdir -p $(INSTALL_BIN_DIR)
	@if [ -f "$(GO_TARGET)" ]; then \
		if cp $(GO_TARGET) $(INSTALL_BACKEND) && chmod +x $(INSTALL_BACKEND); then \
			echo "✓ Installed $(GO_BINARY_NAME) to $(INSTALL_BACKEND)"; \
		else \
			echo "✗ Failed to install $(GO_BINARY_NAME)"; \
			exit 1; \
		fi \
	else \
		echo "✗ Backend binary not found. Run 'make backend' first."; \
		exit 1; \
	fi
	@if [ -f "$(CPP_TARGET)" ]; then \
		if cp $(CPP_TARGET) $(INSTALL_GUI) && chmod +x $(INSTALL_GUI); then \
			echo "✓ Installed $(CPP_BINARY_NAME) to $(INSTALL_GUI)"; \
		else \
			echo "✗ Failed to install $(CPP_BINARY_NAME)"; \
			exit 1; \
		fi \
	else \
		echo "✗ GUI binary not found. Run 'make gui' first."; \
		exit 1; \
	fi
	@echo "Installation complete!"
	@echo ""
	@echo "You can now run:"
	@echo "  $(GO_BINARY_NAME)     - Start the backend service"
	@echo "  $(CPP_BINARY_NAME)    - Start the GUI client"
	@echo "  $(CPP_BINARY_NAME) -c - Start in console mode"

install-user: all
	@echo "Installing binaries to user directory..."
	@echo "Installing to: $(USER_BIN_DIR)"
	@mkdir -p $(USER_BIN_DIR)
	@if [ -f "$(GO_TARGET)" ]; then \
		if cp $(GO_TARGET) $(USER_INSTALL_BACKEND) && chmod +x $(USER_INSTALL_BACKEND); then \
			echo "✓ Installed $(GO_BINARY_NAME) to $(USER_INSTALL_BACKEND)"; \
		else \
			echo "✗ Failed to install $(GO_BINARY_NAME)"; \
			exit 1; \
		fi \
	else \
		echo "✗ Backend binary not found. Run 'make backend' first."; \
		exit 1; \
	fi
	@if [ -f "$(CPP_TARGET)" ]; then \
		if cp $(CPP_TARGET) $(USER_INSTALL_GUI) && chmod +x $(USER_INSTALL_GUI); then \
			echo "✓ Installed $(CPP_BINARY_NAME) to $(USER_INSTALL_GUI)"; \
		else \
			echo "✗ Failed to install $(CPP_BINARY_NAME)"; \
			exit 1; \
		fi \
	else \
		echo "✗ GUI binary not found. Run 'make gui' first."; \
		exit 1; \
	fi
	@echo "User installation complete!"
	@echo ""
	@echo "Note: Make sure $(USER_BIN_DIR) is in your PATH:"
	@echo "  echo 'export PATH=\"\$$HOME/.local/bin:\$$PATH\"' >> ~/.bashrc"
	@echo "  echo 'export PATH=\"\$$HOME/.local/bin:\$$PATH\"' >> ~/.zshrc"
	@echo ""
	@echo "You can now run:"
	@echo "  $(GO_BINARY_NAME)     - Start the backend service"
	@echo "  $(CPP_BINARY_NAME)    - Start the GUI client"
	@echo "  $(CPP_BINARY_NAME) -c - Start in console mode"

uninstall:
	@echo "Removing installed binaries..."
	@if [ -f "$(INSTALL_BACKEND)" ]; then \
		rm -f $(INSTALL_BACKEND); \
		echo "✓ Removed $(INSTALL_BACKEND)"; \
	else \
		echo "- Backend not installed"; \
	fi
	@if [ -f "$(INSTALL_GUI)" ]; then \
		rm -f $(INSTALL_GUI); \
		echo "✓ Removed $(INSTALL_GUI)"; \
	else \
		echo "- GUI not installed"; \
	fi
	@echo "Uninstall complete!"

uninstall-user:
	@echo "Removing user-installed binaries..."
	@if [ -f "$(USER_INSTALL_BACKEND)" ]; then \
		rm -f $(USER_INSTALL_BACKEND); \
		echo "✓ Removed $(USER_INSTALL_BACKEND)"; \
	else \
		echo "- Backend not installed in user directory"; \
	fi
	@if [ -f "$(USER_INSTALL_GUI)" ]; then \
		rm -f $(USER_INSTALL_GUI); \
		echo "✓ Removed $(USER_INSTALL_GUI)"; \
	else \
		echo "- GUI not installed in user directory"; \
	fi
	@echo "User uninstall complete!"

# --- Cleanup ---

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(GO_BUILD_DIR)
	rm -f $(CPP_SRC_DIR)/lyrics-gui $(CPP_SRC_DIR)/*.o $(CPP_SRC_DIR)/xdg-shell-client-protocol.h $(CPP_SRC_DIR)/xdg-shell-protocol.c
	@echo "Cleanup complete."
