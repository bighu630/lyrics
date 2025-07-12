package redis

import (
	"context"
	"time"

	"github.com/go-redis/redis/v8"
)

// Client Redis客户端包装器
type Client struct {
	rdb *redis.Client
}

// NewClient 创建新的Redis客户端
func NewClient(addr string, password string, db int) (*Client, error) {
	rdb := redis.NewClient(&redis.Options{
		Addr:     addr,
		Password: password,
		DB:       db,
	})

	client := &Client{
		rdb: rdb,
	}

	// 测试连接
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := client.Ping(ctx); err != nil {
		return nil, err
	}

	return client, nil
}

// Ping 测试连接
func (c *Client) Ping(ctx context.Context) error {
	return c.rdb.Ping(ctx).Err()
}

// Set 设置键值对（永久有效）
func (c *Client) Set(ctx context.Context, key string, value interface{}) error {
	return c.rdb.Set(ctx, key, value, 0).Err()
}

// SetWithExpiration 设置键值对（带过期时间）
func (c *Client) SetWithExpiration(ctx context.Context, key string, value interface{}, expiration time.Duration) error {
	return c.rdb.Set(ctx, key, value, expiration).Err()
}

// Get 获取值
func (c *Client) Get(ctx context.Context, key string) (string, error) {
	result := c.rdb.Get(ctx, key)
	if result.Err() == redis.Nil {
		return "", nil
	}
	return result.Result()
}

// GetBytes 获取字节数组值
func (c *Client) GetBytes(ctx context.Context, key string) ([]byte, error) {
	result := c.rdb.Get(ctx, key)
	if result.Err() == redis.Nil {
		return nil, nil
	}
	return result.Bytes()
}

// Del 删除键
func (c *Client) Del(ctx context.Context, keys ...string) (int64, error) {
	return c.rdb.Del(ctx, keys...).Result()
}

// Exists 检查键是否存在
func (c *Client) Exists(ctx context.Context, keys ...string) (int64, error) {
	return c.rdb.Exists(ctx, keys...).Result()
}

// HSet 设置哈希字段
func (c *Client) HSet(ctx context.Context, key string, values ...interface{}) error {
	return c.rdb.HSet(ctx, key, values...).Err()
}

// HGet 获取哈希字段值
func (c *Client) HGet(ctx context.Context, key, field string) (string, error) {
	result := c.rdb.HGet(ctx, key, field)
	if result.Err() == redis.Nil {
		return "", nil
	}
	return result.Result()
}

// HGetAll 获取所有哈希字段
func (c *Client) HGetAll(ctx context.Context, key string) (map[string]string, error) {
	return c.rdb.HGetAll(ctx, key).Result()
}

// HDel 删除哈希字段
func (c *Client) HDel(ctx context.Context, key string, fields ...string) (int64, error) {
	return c.rdb.HDel(ctx, key, fields...).Result()
}

// LPush 左侧推入列表
func (c *Client) LPush(ctx context.Context, key string, values ...interface{}) (int64, error) {
	return c.rdb.LPush(ctx, key, values...).Result()
}

// RPush 右侧推入列表
func (c *Client) RPush(ctx context.Context, key string, values ...interface{}) (int64, error) {
	return c.rdb.RPush(ctx, key, values...).Result()
}

// LPop 左侧弹出列表元素
func (c *Client) LPop(ctx context.Context, key string) (string, error) {
	result := c.rdb.LPop(ctx, key)
	if result.Err() == redis.Nil {
		return "", nil
	}
	return result.Result()
}

// RPop 右侧弹出列表元素
func (c *Client) RPop(ctx context.Context, key string) (string, error) {
	result := c.rdb.RPop(ctx, key)
	if result.Err() == redis.Nil {
		return "", nil
	}
	return result.Result()
}

// LLen 获取列表长度
func (c *Client) LLen(ctx context.Context, key string) (int64, error) {
	return c.rdb.LLen(ctx, key).Result()
}

// SAdd 添加集合成员
func (c *Client) SAdd(ctx context.Context, key string, members ...interface{}) (int64, error) {
	return c.rdb.SAdd(ctx, key, members...).Result()
}

// SRem 移除集合成员
func (c *Client) SRem(ctx context.Context, key string, members ...interface{}) (int64, error) {
	return c.rdb.SRem(ctx, key, members...).Result()
}

// SMembers 获取集合所有成员
func (c *Client) SMembers(ctx context.Context, key string) ([]string, error) {
	return c.rdb.SMembers(ctx, key).Result()
}

// SIsMember 检查是否为集合成员
func (c *Client) SIsMember(ctx context.Context, key string, member interface{}) (bool, error) {
	return c.rdb.SIsMember(ctx, key, member).Result()
}

// ZAdd 添加有序集合成员
func (c *Client) ZAdd(ctx context.Context, key string, members ...*redis.Z) (int64, error) {
	return c.rdb.ZAdd(ctx, key, members...).Result()
}

// ZRange 获取有序集合范围内成员
func (c *Client) ZRange(ctx context.Context, key string, start, stop int64) ([]string, error) {
	return c.rdb.ZRange(ctx, key, start, stop).Result()
}

// ZRem 移除有序集合成员
func (c *Client) ZRem(ctx context.Context, key string, members ...interface{}) (int64, error) {
	return c.rdb.ZRem(ctx, key, members...).Result()
}

// Pipeline 创建管道
func (c *Client) Pipeline() redis.Pipeliner {
	return c.rdb.Pipeline()
}

// TxPipeline 创建事务管道
func (c *Client) TxPipeline() redis.Pipeliner {
	return c.rdb.TxPipeline()
}

// Close 关闭客户端连接
func (c *Client) Close() error {
	return c.rdb.Close()
}

// GetRedisClient 获取原始redis客户端（用于高级操作）
func (c *Client) GetRedisClient() *redis.Client {
	return c.rdb
}
