package music

import (
	"context"
	"testing"
)

// mockProvider 模拟音乐提供商
type mockProvider struct {
	name       string
	shouldFail bool
	searchFail bool
	lyricsFail bool
}

func (m *mockProvider) SearchSong(ctx context.Context, title, artist string) (string, error) {
	if m.searchFail {
		return "", nil
	}
	return "mock-song-id", nil
}

func (m *mockProvider) GetLyrics(ctx context.Context, songID string) (string, error) {
	if m.lyricsFail {
		return "", nil
	}
	return "[00:10.00]Test lyrics", nil
}

func (m *mockProvider) GetProviderName() string {
	return m.name
}

// TestGetLyricsByInfo 测试新的封装方法
func TestGetLyricsByInfo(t *testing.T) {
	// 测试成功情况
	t.Run("Success", func(t *testing.T) {
		provider := &mockProvider{
			name:       "TestProvider",
			shouldFail: false,
		}

		manager := NewManager([]MusicAPI{provider})
		lyrics, err := manager.GetLyricsByInfo(context.Background(), "Test Song", "Test Artist", 0)

		if err != nil {
			t.Errorf("Expected success, got error: %v", err)
		}

		if lyrics != "[00:10.00]Test lyrics" {
			t.Errorf("Expected '[00:10.00]Test lyrics', got '%s'", lyrics)
		}
	})

	// 测试搜索失败但有回退提供商的情况
	t.Run("FailoverSuccess", func(t *testing.T) {
		failProvider := &mockProvider{
			name:       "FailProvider",
			searchFail: true,
		}

		successProvider := &mockProvider{
			name:       "SuccessProvider",
			shouldFail: false,
		}

		manager := NewManager([]MusicAPI{failProvider, successProvider})
		lyrics, err := manager.GetLyricsByInfo(context.Background(), "Test Song", "Test Artist", 0)

		if err != nil {
			t.Errorf("Expected success with failover, got error: %v", err)
		}

		if lyrics != "[00:10.00]Test lyrics" {
			t.Errorf("Expected '[00:10.00]Test lyrics', got '%s'", lyrics)
		}
	})

	// 测试所有提供商都失败的情况
	t.Run("AllFail", func(t *testing.T) {
		failProvider1 := &mockProvider{
			name:       "FailProvider1",
			searchFail: true,
		}

		failProvider2 := &mockProvider{
			name:       "FailProvider2",
			lyricsFail: true,
		}

		manager := NewManager([]MusicAPI{failProvider1, failProvider2})
		_, err := manager.GetLyricsByInfo(context.Background(), "Test Song", "Test Artist", 0)

		if err == nil {
			t.Error("Expected error when all providers fail, got success")
		}
	})
}

// TestManagerInterfaceCompliance 测试Manager是否正确实现了接口
func TestManagerInterfaceCompliance(t *testing.T) {
	provider := &mockProvider{name: "TestProvider"}
	manager := NewManager([]MusicAPI{provider})

	// 测试Manager是否实现了MusicAPI接口
	var _ MusicAPI = manager

	// 测试Manager是否实现了MusicManager接口
	var _ MusicManager = manager

	// 测试GetProviderName方法
	name := manager.GetProviderName()
	expected := "Manager[Primary: TestProvider]"
	if name != expected {
		t.Errorf("Expected provider name '%s', got '%s'", expected, name)
	}
}
