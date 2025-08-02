package openai

import (
	"context"
	"errors"
	"maps"
	"time"

	"github.com/rs/zerolog/log"
	openai "github.com/sashabaranov/go-openai"
)

const (
	saveTime = 100 * time.Hour
)

type openAi struct {
	model  string
	client *openai.Client
	ctx    context.Context
	chats  map[string][]openai.ChatCompletionMessage
}

func NewOpenAi(apiKey, modelName, baseURL string) *openAi {
	ctx := context.Background()
	openai_config := openai.DefaultConfig(apiKey)
	openai_config.BaseURL = baseURL
	client := openai.NewClientWithConfig(openai_config)

	css := make(map[string][]openai.ChatCompletionMessage)

	g := &openAi{modelName, client, ctx, css}
	return g
}

func (o *openAi) Name() string {
	return "openai"
}

func (o *openAi) HandleTextWithImg(msg string, imgType string, imgData []byte) (string, error) {
	return o.HandleText(msg)
}

func (o *openAi) HandleText(msg string) (string, error) {
	resp, err := o.client.CreateChatCompletion(o.ctx, openai.ChatCompletionRequest{
		Model: o.model, // "gpt-3.5-turbo" 或 "gpt-4"
		Messages: []openai.ChatCompletionMessage{
			{
				Role:    openai.ChatMessageRoleUser,
				Content: msg,
			},
		},
		MaxTokens: 2000,
	})
	if err != nil {
		log.Error().Err(err).Msg("could not get response from openai")
		return "", err
	}
	result := resp.Choices[0].Message.Content
	return result, nil
}

func (o *openAi) LoadContext(chatCtx map[string][]openai.ChatCompletionMessage) {
	maps.Copy(o.chats, chatCtx)
}

// openAi不支持
func (o *openAi) ChatWithImg(chatId string, msg string, imgType string, imgData []byte) (string, error) {
	return o.Chat(chatId, msg)
}

func (o *openAi) Chat(chatId string, msg string) (string, error) {
	var chatMessages []openai.ChatCompletionMessage
	var ok bool
	if chatMessages, ok = o.chats[chatId]; !ok {
		chatMessages = []openai.ChatCompletionMessage{}
	}

	if len(chatMessages) > 29 {
		chatMessages = chatMessages[len(chatMessages)-30:]
	}

	chatMessages = append(chatMessages, openai.ChatCompletionMessage{
		Role:    openai.ChatMessageRoleUser,
		Content: msg,
	})

	for i := 0; i < 3; i++ {
		resp, err := o.client.CreateChatCompletion(o.ctx, openai.ChatCompletionRequest{
			Model:    o.model,
			Messages: chatMessages,
		})
		if err != nil {
			log.Error().Err(err).Msg("failed to send message to openai")
		} else {
			result := resp.Choices[0].Message.Content
			chatMessages = append(chatMessages, openai.ChatCompletionMessage{
				Role:    openai.ChatMessageRoleAssistant,
				Content: result,
			})
			o.chats[chatId] = chatMessages
			return result, nil
		}
	}
	chatMessages = append(chatMessages, openai.ChatCompletionMessage{
		Role:    openai.ChatMessageRoleAssistant,
		Content: "I got something wrong. I'll try again.",
	})
	return "", errors.New("failed to send message to openai")
}

func (o *openAi) AddChatMsg(chatId string, userSay string, botSay string) error {
	var chatMessages []openai.ChatCompletionMessage
	var ok bool
	if chatMessages, ok = o.chats[chatId]; !ok {
		return nil
	}
	chatMessages = append(chatMessages, openai.ChatCompletionMessage{
		Role:    openai.ChatMessageRoleUser,
		Content: userSay,
	}, openai.ChatCompletionMessage{
		Role:    openai.ChatMessageRoleAssistant,
		Content: botSay,
	})
	o.chats[chatId] = chatMessages
	return nil
}

func (o *openAi) Translate(text string) (string, error) {
	return "", errors.New("implement me")
}
