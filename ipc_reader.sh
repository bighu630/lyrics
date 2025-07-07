#!/bin/bash

# IPC Socket Reader Script for Lyrics App
# This script connects to the lyrics IPC server and displays lyrics in real-time

SOCKET_PATH="/tmp/lyrics_app.sock"
BUFFER_SIZE=1024

# Color codes for better display
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Function to display help
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -s, --socket PATH    Socket path (default: $SOCKET_PATH)"
    echo "  -h, --help          Show this help message"
    echo "  -c, --colored       Use colored output"
    echo ""
    echo "Examples:"
    echo "  $0                  # Connect with default settings"
    echo "  $0 -c               # Connect with colored output"
    echo "  $0 -s /custom/path  # Connect to custom socket path"
}

# Parse command line arguments
COLORED=false
while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--socket)
            SOCKET_PATH="$2"
            shift 2
            ;;
        -c|--colored)
            COLORED=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Function to clean up on exit
cleanup() {
    echo -e "\n${YELLOW}Disconnecting from lyrics server...${NC}"
    exit 0
}

# Set up signal handlers
trap cleanup SIGINT SIGTERM

# Function to display colored lyrics
display_lyric() {
    local lyric="$1"
    local timestamp=$(date '+%H:%M:%S')
    
    if [ "$COLORED" = true ]; then
        echo -e "${CYAN}[$timestamp]${NC} ${GREEN}♪${NC} ${PURPLE}$lyric${NC}"
    else
        echo "[$timestamp] ♪ $lyric"
    fi
}

# Function to connect and read from socket
connect_to_server() {
    echo -e "${GREEN}Connecting to lyrics server at: $SOCKET_PATH${NC}"
    
    # Check if socket exists
    if [ ! -S "$SOCKET_PATH" ]; then
        echo -e "${RED}Error: Socket file does not exist at $SOCKET_PATH${NC}"
        echo -e "${YELLOW}Make sure the lyrics backend server is running.${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Connected! Listening for lyrics...${NC}"
    echo -e "${BLUE}Press Ctrl+C to disconnect${NC}"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    # Connect to socket and read continuously
    # Use netcat (nc) or socat to connect to Unix socket
    if command -v socat >/dev/null 2>&1; then
        socat - UNIX-CONNECT:"$SOCKET_PATH" | while IFS= read -r line; do
            display_lyric "$line"
        done
    elif command -v nc >/dev/null 2>&1; then
        nc -U "$SOCKET_PATH" | while IFS= read -r line; do
            display_lyric "$line"
        done
    else
        echo -e "${RED}Error: Neither 'socat' nor 'nc' (netcat) is available.${NC}"
        echo -e "${YELLOW}Please install one of them:${NC}"
        echo "  Ubuntu/Debian: sudo apt install socat netcat-openbsd"
        echo "  CentOS/RHEL:   sudo yum install socat nmap-ncat"
        echo "  Arch Linux:    sudo pacman -S socat gnu-netcat"
        exit 1
    fi
}

# Function to test connection
test_connection() {
    echo -e "${YELLOW}Testing connection to $SOCKET_PATH...${NC}"
    
    if [ ! -S "$SOCKET_PATH" ]; then
        echo -e "${RED}✗ Socket file does not exist${NC}"
        return 1
    fi
    
    # Try to connect briefly
    if timeout 2s bash -c "echo test | socat - UNIX-CONNECT:$SOCKET_PATH" >/dev/null 2>&1; then
        echo -e "${GREEN}✓ Connection test successful${NC}"
        return 0
    elif timeout 2s bash -c "echo test | nc -U $SOCKET_PATH" >/dev/null 2>&1; then
        echo -e "${GREEN}✓ Connection test successful${NC}"
        return 0
    else
        echo -e "${RED}✗ Connection test failed${NC}"
        return 1
    fi
}

# Main execution
main() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}        Lyrics IPC Reader${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    # Test connection first
    if ! test_connection; then
        echo -e "${YELLOW}Starting connection anyway...${NC}"
    fi
    
    # Start main connection loop
    while true; do
        connect_to_server
        echo -e "${YELLOW}Connection lost. Retrying in 3 seconds...${NC}"
        sleep 3
    done
}

# Run main function
main
