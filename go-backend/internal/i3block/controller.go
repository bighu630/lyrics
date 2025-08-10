package i3block

import (
	"fmt"
	"log"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
)

// Controller manages i3block process monitoring and signal sending
type Controller struct {
	pid       int
	pidMutex  sync.RWMutex
	ticker    *time.Ticker
	stopChan  chan struct{}
	isRunning bool
	runMutex  sync.Mutex
}

// NewController creates a new i3block controller
func NewController() *Controller {
	return &Controller{
		pid:       -1,
		stopChan:  make(chan struct{}),
		isRunning: false,
	}
}

// Start begins monitoring i3block PID every 10 seconds
func (c *Controller) Start() error {
	c.runMutex.Lock()
	defer c.runMutex.Unlock()

	if c.isRunning {
		return fmt.Errorf("controller is already running")
	}

	// Initial PID refresh
	if err := c.refreshPID(); err != nil {
	}

	c.ticker = time.NewTicker(10 * time.Second)
	c.isRunning = true

	go c.monitorLoop()

	log.Println("i3block controller started")
	return nil
}

// Stop stops the controller
func (c *Controller) Stop() {
	c.runMutex.Lock()
	defer c.runMutex.Unlock()

	if !c.isRunning {
		return
	}

	close(c.stopChan)
	c.ticker.Stop()
	c.isRunning = false

	log.Println("i3block controller stopped")
}

// monitorLoop runs the periodic PID refresh
func (c *Controller) monitorLoop() {
	for {
		select {
		case <-c.ticker.C:
			if err := c.refreshPID(); err != nil {
				log.Printf("Failed to refresh i3block PID: %v", err)
			}
		case <-c.stopChan:
			return
		}
	}
}

// refreshPID updates the stored PID of i3block process
func (c *Controller) refreshPID() error {
	// Try to find i3block process using pgrep
	cmd := exec.Command("pgrep", "-f", "i3blocks")
	output, err := cmd.Output()
	if err != nil {
		// If pgrep fails, try alternative method
		return c.refreshPIDAlternative()
	}

	pidStr := strings.TrimSpace(string(output))
	if pidStr == "" {
		c.pidMutex.Lock()
		c.pid = -1
		c.pidMutex.Unlock()
		return fmt.Errorf("i3block process not found")
	}

	// If multiple PIDs, take the first one
	lines := strings.Split(pidStr, "\n")
	if len(lines) > 0 && lines[0] != "" {
		pid, err := strconv.Atoi(lines[0])
		if err != nil {
			return fmt.Errorf("failed to parse PID: %v", err)
		}

		c.pidMutex.Lock()
		oldPID := c.pid
		c.pid = pid
		c.pidMutex.Unlock()

		if oldPID != pid {
			log.Printf("i3block PID updated: %d -> %d", oldPID, pid)
		}
	}

	return nil
}

// refreshPIDAlternative tries alternative method to find i3block PID
func (c *Controller) refreshPIDAlternative() error {
	// Try using ps command
	cmd := exec.Command("ps", "aux")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to run ps command: %v", err)
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "i3blocks") && !strings.Contains(line, "grep") {
			fields := strings.Fields(line)
			if len(fields) >= 2 {
				pid, err := strconv.Atoi(fields[1])
				if err != nil {
					continue
				}

				c.pidMutex.Lock()
				oldPID := c.pid
				c.pid = pid
				c.pidMutex.Unlock()

				if oldPID != pid {
					log.Printf("i3block PID updated (alternative method): %d -> %d", oldPID, pid)
				}
				return nil
			}
		}
	}

	c.pidMutex.Lock()
	c.pid = -1
	c.pidMutex.Unlock()
	return fmt.Errorf("i3block process not found with alternative method")
}

// GetPID returns the current stored PID
func (c *Controller) GetPID() int {
	c.pidMutex.RLock()
	defer c.pidMutex.RUnlock()
	return c.pid
}

// SendSignal55 sends signal 55 (34+21) to i3block process
func (c *Controller) SendSignal55() error {
	c.pidMutex.RLock()
	pid := c.pid
	c.pidMutex.RUnlock()

	if pid <= 0 {
		return fmt.Errorf("invalid PID: %d, i3block process not found", pid)
	}

	// Check if process exists
	process, err := os.FindProcess(pid)
	if err != nil {
		return fmt.Errorf("failed to find process %d: %v", pid, err)
	}

	// Send signal 55 (SIGUSR1 + SIGUSR2 in some contexts, or custom signal)
	// Note: Signal 55 might not be available on all systems
	// We'll use syscall.Signal(55) if available
	err = process.Signal(syscall.Signal(55))
	if err != nil {
		return fmt.Errorf("failed to send signal 55 to process %d: %v", pid, err)
	}

	return nil
}

// SendSignalUSR1 sends SIGUSR1 to i3block process (alternative method)
func (c *Controller) SendSignalUSR1() error {
	c.pidMutex.RLock()
	pid := c.pid
	c.pidMutex.RUnlock()

	if pid <= 0 {
		return fmt.Errorf("invalid PID: %d, i3block process not found", pid)
	}

	process, err := os.FindProcess(pid)
	if err != nil {
		return fmt.Errorf("failed to find process %d: %v", pid, err)
	}

	err = process.Signal(syscall.SIGUSR1)
	if err != nil {
		return fmt.Errorf("failed to send SIGUSR1 to process %d: %v", pid, err)
	}

	log.Printf("Successfully sent SIGUSR1 to i3block process (PID: %d)", pid)
	return nil
}

// SendSignalUSR2 sends SIGUSR2 to i3block process (alternative method)
func (c *Controller) SendSignalUSR2() error {
	c.pidMutex.RLock()
	pid := c.pid
	c.pidMutex.RUnlock()

	if pid <= 0 {
		return fmt.Errorf("invalid PID: %d, i3block process not found", pid)
	}

	process, err := os.FindProcess(pid)
	if err != nil {
		return fmt.Errorf("failed to find process %d: %v", pid, err)
	}

	err = process.Signal(syscall.SIGUSR2)
	if err != nil {
		return fmt.Errorf("failed to send SIGUSR2 to process %d: %v", pid, err)
	}

	log.Printf("Successfully sent SIGUSR2 to i3block process (PID: %d)", pid)
	return nil
}

// IsRunning returns whether the controller is currently running
func (c *Controller) IsRunning() bool {
	c.runMutex.Lock()
	defer c.runMutex.Unlock()
	return c.isRunning
}

// ForceRefreshPID manually triggers a PID refresh
func (c *Controller) ForceRefreshPID() error {
	return c.refreshPID()
}
