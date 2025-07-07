package ipc

import (
	"net"
	"os"
	"sync"

	"github.com/rs/zerolog/log"
)

type Server struct {
	socketPath      string
	listener        net.Listener
	clientConns     map[net.Conn]struct{}
	clientConnsLock sync.Mutex
	lyrics          string
	lyricsLock      sync.Mutex
}

func NewServer(socketPath string) *Server {
	return &Server{
		socketPath:  socketPath,
		clientConns: make(map[net.Conn]struct{}),
	}
}

func (s *Server) Start() error {
	if err := os.RemoveAll(s.socketPath); err != nil {
		return err
	}

	listener, err := net.Listen("unix", s.socketPath)
	if err != nil {
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
	s.listener.Close()
}
