package netease

import (
	"context"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"
)

// TestClientRetry 测试重试机制
func TestClientRetry(t *testing.T) {
	// 创建一个计数器，记录请求次数
	requestCount := 0

	// 创建测试服务器，模拟间歇性失败
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		requestCount++
		if requestCount <= 2 {
			// 前两次请求失败
			w.WriteHeader(http.StatusInternalServerError)
			return
		}
		// 第三次请求成功
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"result":{"songs":[{"id":123,"name":"Test Song","artists":[{"name":"Test Artist"}]}]}}`))
	}))
	defer server.Close()

	// 创建客户端，直接使用测试服务器URL
	client := &Client{
		httpClient:     &http.Client{Timeout: 1 * time.Second},
		maxRetries:     3,
		requestTimeout: 2 * time.Second,
	}

	// 使用客户端的doRequestWithRetry方法发起请求
	req, err := http.NewRequest("GET", server.URL, nil)
	if err != nil {
		t.Fatalf("创建请求失败: %v", err)
	}

	resp, err := client.doRequestWithRetry(req)
	if err != nil {
		t.Fatalf("请求失败: %v", err)
	}
	defer resp.Body.Close()

	// 检查是否进行了预期的重试次数
	if requestCount != 3 {
		t.Errorf("预期重试次数为3，实际为%d", requestCount)
	}

	// 检查最终请求是否成功
	if resp.StatusCode != http.StatusOK {
		t.Errorf("预期状态码200，实际为%d", resp.StatusCode)
	}
}

// TestTimeout 测试超时机制
func TestTimeout(t *testing.T) {
	// 创建一个模拟超时的服务器
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// 休眠2秒，超过客户端的超时时间
		time.Sleep(2 * time.Second)
		w.WriteHeader(http.StatusOK)
	}))
	defer server.Close()

	// 创建客户端，设置超时时间为1秒
	client := &Client{
		httpClient:     &http.Client{Timeout: 1 * time.Second},
		maxRetries:     1,
		requestTimeout: 1 * time.Second,
	}

	// 使用上下文超时
	ctx, cancel := context.WithTimeout(context.Background(), 1*time.Second)
	defer cancel()

	// 发起请求
	req, err := http.NewRequestWithContext(ctx, "GET", server.URL, nil)
	if err != nil {
		t.Fatalf("创建请求失败: %v", err)
	}

	_, err = client.doRequestWithRetry(req)

	// 检查是否确实超时
	if err == nil {
		t.Error("预期请求超时失败，但请求成功了")
	}
}
