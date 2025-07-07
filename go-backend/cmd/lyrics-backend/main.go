package main

import (
	"go-backend/internal/app"
	"go-backend/internal/config"
)

func main() {
	cfg := config.Load()
	app := app.New(cfg)
	app.Run()
}