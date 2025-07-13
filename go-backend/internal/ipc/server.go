package ipc

import (
	"fmt"
	"net"
	"os"
	"strconv"
	"strings"
	"sync"
	"syscall"

	"github.com/rs/zerolog/log"
)

type Server struct {
	socketPath      string
	listener        net.Listener
	clientConns     map[net.Conn]struct{}
	clientConnsLock sync.Mutex
	lyrics          string
	lyricsLock      sync.Mutex
	lockFile        *os.File
	lockFilePath    string
}

func NewServer(socketPath string) *Server {
	return &Server{
		socketPath:   socketPath,
		clientConns:  make(map[net.Conn]struct{}),
		lockFilePath: socketPath + ".lock",
	}
}

func (s *Server) checkAndCleanOldLock() error {
	// 检查锁文件是否存在
	if _, err := os.Stat(s.lockFilePath); os.IsNotExist(err) {
		return nil // 锁文件不存在，无需清理
	}

	// 尝试读取锁文件中的PID
	content, err := os.ReadFile(s.lockFilePath)
	if err != nil {
		// 读取失败，直接删除锁文件
		log.Warn().Err(err).Msg("Failed to read lock file, removing it")
		os.Remove(s.lockFilePath)
		return nil
	}

	pidStr := strings.TrimSpace(string(content))
	if pidStr == "" {
		// 锁文件为空，删除它
		log.Warn().Msg("Lock file is empty, removing it")
		os.Remove(s.lockFilePath)
		return nil
	}

	pid, err := strconv.Atoi(pidStr)
	if err != nil {
		// PID格式不正确，删除锁文件
		log.Warn().Err(err).Str("pid_str", pidStr).Msg("Invalid PID in lock file, removing it")
		os.Remove(s.lockFilePath)
		return nil
	}

	// 检查进程是否存在
	if !s.isProcessRunning(pid) {
		log.Info().Int("old_pid", pid).Msg("Process in lock file is not running, removing lock file")
		os.Remove(s.lockFilePath)
		return nil
	}

	log.Info().Int("existing_pid", pid).Msg("Another process is still running")
	return nil
}

func (s *Server) isProcessRunning(pid int) bool {
	// 在Linux系统上，使用kill(pid, 0)来检查进程是否存在
	// 这不会发送任何信号，只是检查进程是否存在
	err := syscall.Kill(pid, 0)
	return err == nil
}

func (s *Server) acquireLock() error {
	// 检查是否存在旧的锁文件
	if err := s.checkAndCleanOldLock(); err != nil {
		log.Warn().Err(err).Msg("Failed to clean old lock file")
	}

	// 尝试创建并锁定文件
	file, err := os.OpenFile(s.lockFilePath, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0644)
	if err != nil {
		return fmt.Errorf("failed to create lock file: %w", err)
	}

	// 尝试获取独占锁
	err = syscall.Flock(int(file.Fd()), syscall.LOCK_EX|syscall.LOCK_NB)
	if err != nil {
		file.Close()
		if err == syscall.EWOULDBLOCK {
			return fmt.Errorf("another lyrics server instance is already running")
		}
		return fmt.Errorf("failed to acquire lock: %w", err)
	}

	// 写入当前进程ID到锁文件
	_, err = file.WriteString(fmt.Sprintf("%d\n", os.Getpid()))
	if err != nil {
		syscall.Flock(int(file.Fd()), syscall.LOCK_UN)
		file.Close()
		return fmt.Errorf("failed to write PID to lock file: %w", err)
	}

	s.lockFile = file
	log.Info().Str("lock_file", s.lockFilePath).Int("pid", os.Getpid()).Msg("Acquired process lock")
	return nil
}

func (s *Server) releaseLock() {
	if s.lockFile != nil {
		syscall.Flock(int(s.lockFile.Fd()), syscall.LOCK_UN)
		s.lockFile.Close()
		os.Remove(s.lockFilePath)
		log.Info().Str("lock_file", s.lockFilePath).Msg("Released process lock")
		s.lockFile = nil
	}
}

func (s *Server) Start() error {
	// 首先尝试获取进程锁
	if err := s.acquireLock(); err != nil {
		return err
	}

	if err := os.RemoveAll(s.socketPath); err != nil {
		s.releaseLock()
		return err
	}

	listener, err := net.Listen("unix", s.socketPath)
	if err != nil {
		s.releaseLock()
		return err
	}
	s.listener = listener

	log.Info().Str("socket_path", s.socketPath).Msg("IPC server listening")

	go s.acceptConnections()

	return nil
}

func (s *Server) acceptConnections() {
	for {
		conn, err := s.listener.Accept()
		if err != nil {
			log.Error().Err(err).Msg("Failed to accept IPC connection")
			continue
		}
		go s.handleConnection(conn)
	}
}

func (s *Server) handleConnection(conn net.Conn) {
	s.clientConnsLock.Lock()
	s.clientConns[conn] = struct{}{}
	s.clientConnsLock.Unlock()

	log.Info().Msg("GUI client connected")

	s.lyricsLock.Lock()
	_, err := conn.Write([]byte(s.lyrics))
	s.lyricsLock.Unlock()
	if err != nil {
		log.Error().Err(err).Msg("Failed to send initial lyrics")
	}

	buf := make([]byte, 1)
	for {
		_, err := conn.Read(buf)
		if err != nil {
			break
		}
	}

	s.clientConnsLock.Lock()
	delete(s.clientConns, conn)
	s.clientConnsLock.Unlock()
	conn.Close()
	log.Info().Msg("GUI client disconnected")
}

func (s *Server) Broadcast(lyrics string) {
	if lyrics != "" {
		os.WriteFile("/tmp/lyrics", []byte(lyrics+"\n"), 0644)
	}
	s.lyricsLock.Lock()
	s.lyrics = lyrics
	s.lyricsLock.Unlock()

	s.clientConnsLock.Lock()
	defer s.clientConnsLock.Unlock()

	lyricsBytes := []byte(lyrics)
	for conn := range s.clientConns {
		_, err := conn.Write(lyricsBytes)
		if err != nil {
			log.Error().Err(err).Msg("Failed to write to client, removing")
			conn.Close()
			delete(s.clientConns, conn)
		}
	}
}

func (s *Server) Close() {
	if s.listener != nil {
		s.listener.Close()
	}
	s.releaseLock()
}
