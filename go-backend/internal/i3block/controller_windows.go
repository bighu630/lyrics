//go:build windows

package i3block

import "fmt"

// Controller manages i3block process monitoring (stub on Windows)
type Controller struct{}

func NewController() *Controller { return &Controller{} }
func (c *Controller) Start() error { return nil }
func (c *Controller) Stop() {}
func (c *Controller) SendSignal55() error { return nil }
func (c *Controller) GetPID() int { return -1 }
func (c *Controller) IsRunning() bool { return false }
