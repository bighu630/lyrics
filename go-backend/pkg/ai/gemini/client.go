package gemini

import (
	"context"
	"errors"
	"fmt"
	"go-backend/pkg/ai"
	"maps"
	"time"

	"github.com/google/generative-ai-go/genai"
	"github.com/rs/zerolog/log"
	"google.golang.org/api/option"
)

const (
	saveTime = 100 * time.Hour
)

var _ ai.AiInterface = (*gemini)(nil)

type gemini struct {
	model *genai.GenerativeModel
	chats map[string]*genai.ChatSession
	ctx   context.Context
}

func NewGemini(apiKey, modelName string) *gemini {
	ctx := context.Background()
	client, err := genai.NewClient(ctx, option.WithAPIKey(apiKey))
	if err != nil {
		log.Panic().Err(err)
	}
	if modelName == "" {
		modelName = "gemini-2.5-flash"
	}
	model := client.GenerativeModel(modelName)
	g := &gemini{model, make(map[string]*genai.ChatSession), ctx}
	return g
}

func (g *gemini) LoadContext(chatCtx map[string]*genai.ChatSession) {
	maps.Copy(g.chats, chatCtx)
}

func (g gemini) Name() string {
	return "gemini"
}

func (g *gemini) HandleTextWithImg(msg string, imgType string, imgData []byte) (string, error) {
	input := msg
	resp, err := g.model.GenerateContent(g.ctx,
		genai.Text(input), genai.ImageData(imgType, imgData))
	if err != nil {
		log.Error().Err(err).Msg("could not get response from gemini")
		return "", err
	}
	result := fmt.Sprint(resp.Candidates[0].Content.Parts[0])
	return result, nil
}

func (g *gemini) HandleText(msg string) (string, error) {
	input := msg
	resp, err := g.model.GenerateContent(g.ctx,
		genai.Text(input))
	if err != nil {
		log.Error().Err(err).Msg("could not get response from gemini")
		return "", err
	}
	result := fmt.Sprint(resp.Candidates[0].Content.Parts[0])
	return result, nil
}

func (g *gemini) ChatWithImg(chatId string, msg string, imgType string, imgData []byte) (string, error) {
	var cs *genai.ChatSession
	var ok bool
	var resp *genai.GenerateContentResponse
	var err error
	if cs, ok = g.chats[chatId]; !ok {
		cs = g.model.StartChat()
		g.chats[chatId] = cs
	}
	if len(cs.History) > 29 {
		cs.History = cs.History[len(cs.History)-30:]
	}
	for range 3 {
		if len(imgData) > 0 {
			resp, err = cs.SendMessage(g.ctx, genai.Text(msg), genai.ImageData(imgType, imgData))
		} else {
			resp, err = cs.SendMessage(g.ctx, genai.Text(msg))
		}

		if err != nil {
			log.Error().Err(err).Msg("failed to send message to gemini")
		} else {
			result := fmt.Sprint(resp.Candidates[0].Content.Parts[0])
			return result, nil
		}
	}
	cs.History = append(cs.History, &genai.Content{
		Parts: []genai.Part{genai.Text("I got something wrong. I'll try again.")},
		Role:  "model",
	})
	return "", errors.New("failed to send message to gemini")
}

func (g *gemini) Chat(chatId string, msg string) (string, error) {
	return g.ChatWithImg(chatId, msg, "", nil)
}

func (g *gemini) AddChatMsg(chatId string, userSay string, botSay string) error {
	var cs *genai.ChatSession
	var ok bool
	if cs, ok = g.chats[chatId]; !ok {
		return nil
	}
	cs.History = append(cs.History, &genai.Content{
		Parts: []genai.Part{genai.Text(userSay)},
		Role:  "user",
	}, &genai.Content{
		Parts: []genai.Part{genai.Text(botSay)},
		Role:  "model",
	})
	return nil
}
