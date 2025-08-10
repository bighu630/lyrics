package musiccache

import (
	"bufio"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
)

const (
	kvFormat = "%s => %s"
)

var cache = sync.Map{}
var loadCacheFile = ""

func init() {
	homeDir, err := os.UserHomeDir()
	if err != nil {
		panic(err)
	}
	loadCacheFile = filepath.Join(homeDir, ".cache", "lyrics", "music_cache.list")
	_, err = os.Stat(loadCacheFile)
	switch err {
	case os.ErrNotExist:
		dir := filepath.Dir(loadCacheFile)
		err := os.MkdirAll(dir, 0755)
		if err != nil {
			panic(err)
		}
		_, err = os.Create(loadCacheFile)
		if err != nil {
			panic(err)
		}
	case nil:
		f, err := os.Open(loadCacheFile)
		if err != nil {
			panic(err)
		}
		defer f.Close()
		scanner := bufio.NewScanner(f)
		for scanner.Scan() {
			line := scanner.Text()
			kv := strings.Split(line, " => ")
			if len(kv) != 2 {
				continue
			}
			cache.Store(kv[0], kv[1])
		}
	default:
		panic(err)
	}
}

func appendCache(key string) {
	f, err := os.OpenFile(loadCacheFile, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
	if err != nil {
		panic(err)
	}
	defer f.Close()

	_, err = f.WriteString(key + "\n")
	if err != nil {
		panic(err)
	}
}

func AddCache(key string, value string) {
	if _, ok := cache.Load(key); ok {
		return
	}
	cache.Store(key, value)
	appendCache(fmt.Sprintf(kvFormat, key, value))
}

func GetCache(key string) (string, error) {
	v, ok := cache.Load(key)
	if !ok {
		return "", errors.New("not found")
	}
	return v.(string), nil
}
