#!/bin/bash

# Simple IPC Reader for Lyrics
SOCKET_PATH="/tmp/lyrics_app.sock"

# Function to center text
center_text() {
    local text="$1"
    local terminal_width=$(tput cols 2>/dev/null || echo 80)
    local text_length=${#text}
    local padding=$(( (terminal_width - text_length) / 2 ))
    
    if [ $padding -gt 0 ]; then
        printf "%*s%s\n" $padding "" "$text"
    else
        echo "$text"
    fi
}

echo "Connecting to lyrics server..."

# Check if socket exists
if [ ! -S "$SOCKET_PATH" ]; then
    echo "Error: Socket not found at $SOCKET_PATH"
    echo "Make sure the lyrics backend is running."
    exit 1
fi

echo "Connected! Listening for lyrics (Press Ctrl+C to exit):"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Read from socket continuously
if command -v socat >/dev/null 2>&1; then
    socat - UNIX-CONNECT:"$SOCKET_PATH" | while IFS= read -r line; do
        center_text "$line"
    done
elif command -v nc >/dev/null 2>&1; then
    nc -U "$SOCKET_PATH" | while IFS= read -r line; do
        center_text "$line"
    done
else
    echo "Error: Need 'socat' or 'nc' to connect to Unix socket"
    echo "Install with: sudo apt install socat"
    exit 1
fi
