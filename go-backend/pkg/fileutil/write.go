package main

package fileutil

import (
	"fmt"
	"os"
)

// WriteFileOverwrite writes content to a file at the specified path,
// overwriting it if it already exists.
func WriteFileOverwrite(filePath string, content []byte, perm os.FileMode) error {
	f, err := os.OpenFile(filePath, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, perm)
	if err != nil {
		return fmt.Errorf("failed to open file %s: %w", filePath, err)
	}
	defer f.Close()

	_, err = f.Write(content)
	if err != nil {
		return fmt.Errorf("failed to write to file %s: %w", filePath, err)
	}

	return nil
}

import "fmt"

func main() {
	fmt.Println("vim-go")
}
