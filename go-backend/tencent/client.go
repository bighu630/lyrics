package tencent

import (
	"encoding/base64"
	"os"

	"github.com/google/uuid"
	"github.com/rs/zerolog/log"
	asr "github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/asr/v20190614"
	"github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/common"
	"github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/common/profile"
	"github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/common/regions"
	tmt "github.com/tencentcloud/tencentcloud-sdk-go/tencentcloud/tmt/v20180321"
)

var tencentClient TencentClient

type TencentClient interface {
	AudioToText(audioFile *string) (string, error)
	TranslateText(text string) string
	TranslateImg(imgPath string) (string, string)
}

type TencentClientImpl struct {
	asrClient *asr.Client
	tmtClient *tmt.Client
}

func NewClient(secretID, secretKey string) (TencentClient, error) {
	credential := common.NewCredential(
		secretID, secretKey,
	)

	cpf := profile.NewClientProfile()

	/* The SDK uses the POST method by default
	 * If you have to use the GET method, you can set it here, but the GET method cannot handle some large requests */
	cpf.HttpProfile.ReqMethod = "POST"
	cpf.HttpProfile.ReqTimeout = 10 // Request timeout time, in seconds (the default value is 60 seconds)
	/* Specifies the access region domain name. The default nearby region domain name is sms.tencentcloudapi.com. Specifying a region domain name for access is also supported. For example, the domain name for the Singapore region is sms.ap-singapore.tencentcloudapi.com  */
	cpf.HttpProfile.Endpoint = "asr.tencentcloudapi.com"

	/* The SDK uses `TC3-HMAC-SHA256` to sign by default. Do not modify this field unless absolutely necessary */
	cpf.SignMethod = "HmacSHA1"

	asrClient, err := asr.NewClient(credential, regions.Shanghai, cpf)
	if err != nil {
		log.Error().Err(err).Msg("new tencent client error")
		return nil, err
	}
	tmtClient, err := tmt.NewClient(credential, regions.Guangzhou, profile.NewClientProfile())
	if err != nil {
		log.Error().Err(err).Msg("new tencent client error")
		return nil, err
	}
	tencentClient = &TencentClientImpl{asrClient: asrClient, tmtClient: tmtClient}

	return tencentClient, nil
}

func GetTencentClient() TencentClient {
	return tencentClient
}

func (t *TencentClientImpl) TranslateText(text string) string {
	// 硬编码密钥到代码中有可能随代码泄露而暴露，有安全隐患，并不推荐。
	// 为了保护密钥安全，建议将密钥设置在环境变量中或者配置文件中，请参考本文凭证管理章节。
	languageRequest := tmt.NewLanguageDetectRequest()
	id := int64(0)
	languageRequest.Text = &text
	languageRequest.ProjectId = &id
	languageResponse, _ := t.tmtClient.LanguageDetect(languageRequest)
	lang := *languageResponse.Response.Lang

	var tar string
	request := tmt.NewTextTranslateRequest()
	request.Source = &lang
	if lang == "zh" {
		tar = "en"
	} else {
		tar = "zh"
	}
	request.SourceText = &text
	request.Target = &tar
	request.ProjectId = &id
	response, err := t.tmtClient.TextTranslate(request)
	if err != nil {
		log.Error().Err(err).Msg("failed to send request")
		return ""
	}

	return *response.Response.TargetText
}

// 翻译img 返回原string 和翻译结果
func (t *TencentClientImpl) TranslateImg(imgPath string) (string, string) {
	languageRequest := tmt.NewLanguageDetectRequest()
	id := int64(0)
	languageRequest.ProjectId = &id
	fimg, err := os.ReadFile(imgPath)
	if err != nil {
		log.Error().Err(err).Msg("failed to read image file")
	}
	base64Img := base64.StdEncoding.EncodeToString(fimg)

	request := tmt.NewImageTranslateRequest()
	auto, target, scene := "auto", "zh", "doc"
	uuid := uuid.New().String()
	request.Source = &auto
	request.Target = &target
	request.ProjectId = &id
	request.Data = &base64Img
	request.Scene = &scene
	request.SessionUuid = &uuid
	response, err := t.tmtClient.ImageTranslate(request)
	if err != nil {
		log.Error().Err(err).Msg("failed to send request")
		return "", ""
	}
	sor, resp := "", ""
	for _, v := range response.Response.ImageRecord.Value {
		sor += *v.SourceText + "\n"
		resp += *v.TargetText + "\n"
	}
	return sor, resp
}

func (t *TencentClientImpl) AudioToText(audioFile *string) (string, error) {
	file, err := os.ReadFile(*audioFile)
	if err != nil {
		log.Error().Err(err).Msg("failed to read audio file")
		return "", err
	}
	datalen := len(file)
	data := base64.RawStdEncoding.EncodeToString(file)
	request := asr.NewSentenceRecognitionRequest()
	request.EngSerViceType = common.StringPtr("8k_zh")
	request.SourceType = common.Uint64Ptr(1)
	request.VoiceFormat = common.StringPtr("mp3")
	request.Data = common.StringPtr(data)
	request.DataLen = common.Int64Ptr(int64(datalen))

	resp, err := t.asrClient.SentenceRecognition(request)
	if err != nil {
		log.Error().Err(err).Msg("failed to send request")
		return "", err
	}
	return *resp.Response.Result, nil
}
