package tencent

import (
	"testing"
)

func getClient(secID, secKey string, t *testing.T) TencentClient {
	client, err := NewClient(secID, secKey)
	if err != nil {
		t.Fatalf("failed to create tencent client: %v", err)
	}
	return client
}

func TestTransText(t *testing.T) {
	client := getClient("secID", "secKey", t)
	testMsg := "hello world"

	resp := client.TranslateText(testMsg)

	if len(resp) == 0 {
		t.Fatalf("failed to translate text")
	}
	t.Logf("translated text: %s", resp)
}
