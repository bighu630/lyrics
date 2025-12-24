package gemini

import (
	"context"
	"errors"
	"fmt"
	"go-backend/pkg/ai"
	"time"

	"github.com/rs/zerolog/log"
	"google.golang.org/genai"
)

const (
	saveTime = 100 * time.Hour
)

var _ ai.AiInterface = (*gemini)(nil)

type gemini struct {
	client    *genai.Client
	chats     map[string]*genai.Chat
	modelName string
	ctx       context.Context
}

func NewGemini(apiKey, model string) *gemini {
	ctx := context.Background()
	client, err := genai.NewClient(ctx, &genai.ClientConfig{APIKey: apiKey})
	if err != nil {
		log.Panic().Err(err)
	}
	modelName := model
	if modelName == "" {
		modelName = "gemini-2.5-flash"
	}

	css := make(map[string]*genai.Chat)
	g := &gemini{client, css, modelName, ctx}
	return g
}

func (g gemini) Name() string {
	return "gemini"
}

func (g *gemini) HandleTextWithImg(msg string, imgType string, imgData []byte) (string, error) {
	resp, err := g.client.Models.GenerateContent(g.ctx, g.modelName,
		[]*genai.Content{genai.NewContentFromBytes(imgData, imgType, genai.RoleUser)}, nil)
	if err != nil {
		log.Error().Err(err).Msg("could not get response from gemini")
		return "", err
	}
	result := fmt.Sprint(resp.Candidates[0].Content.Parts[0])
	return result, nil
}

func (g *gemini) HandleText(msg string) (string, error) {
	input := msg
	resp, err := g.client.Models.GenerateContent(g.ctx,
		g.modelName,
		genai.Text(input), nil)
	if err != nil {
		log.Error().Err(err).Msg("could not get response from gemini")
		return "", err
	}
	result := fmt.Sprint(resp.Candidates[0].Content.Parts[0])
	return result, nil
}

func (g *gemini) ChatWithImg(chatId string, msg string, imgType string, imgData []byte) (string, error) {
	var resp *genai.GenerateContentResponse
	var err error
	cs := g.chats[chatId]
	if cs == nil {
		cs, err = g.client.Chats.Create(g.ctx, g.modelName, nil, nil)
		if err != nil {
			log.Error().Err(err).Msg("failed to create chat")
			return "", err
		}
		g.chats[chatId] = cs
	}
	for range 3 {
		if len(imgData) > 0 {
			part := genai.NewPartFromBytes(imgData, imgType)
			part.Text = msg
			resp, err = cs.SendMessage(g.ctx, *part)
		} else {
			part := genai.NewPartFromText(msg)
			resp, err = cs.SendMessage(g.ctx, *part)
		}

		if err != nil {
			log.Error().Err(err).Msg("failed to send message to gemini")
		} else {
			result := resp.Candidates[0].Content.Parts[0].Text
			return result, nil
		}
	}
	hs := cs.History(true)
	hs = append(hs, genai.Text("处理错误，我忽略这个回答")...)
	return "", errors.New("failed to send message to gemini")
}

func (g *gemini) Chat(chatId string, msg string) (string, error) {
	return g.ChatWithImg(chatId, msg, "", nil)
}
