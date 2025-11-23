#ifndef BANKING_SYSTEM_SHARD_SHARD_MANAGER_H
#define BANKING_SYSTEM_SHARD_SHARD_MANAGER_H

#include "account_shard.h"
#include "banking_system/transfer/cross_shard_context.h"
#include "banking_system/common/types.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>

// ==================== 分片管理器类 ====================

/**
 * @brief 分片管理器
 * 
 * 负责：
 * 1. 管理所有分片的生命周期（创建、销毁）
 * 2. 路由转账请求到正确的分片
 * 3. 协调跨分片转账的两步操作（2PC简化版）
 * 
 * 架构特点：
 * - 采用哈希分片策略（account_id % num_shards）
 * - 每个分片独立工作线程，实现并行处理
 * - 跨分片转账使用correlation_id追踪状态
 */
class ShardManager {
public:
    /**
     * @brief 构造函数
     * 
     * 创建指定数量的分片，每个分片启动独立的工作线程
     * 
     * @param num_shards 分片数量（建议4, 8, 16等2的幂次）
     */
    explicit ShardManager(int num_shards);
    
    /**
     * @brief 析构函数
     * 
     * 智能指针会自动清理分片资源，确保所有线程正常退出
     */
    ~ShardManager() = default;
    
    // 禁止拷贝和赋值
    ShardManager(const ShardManager&) = delete;
    ShardManager& operator=(const ShardManager&) = delete;
    
    /**
     * @brief 计算账户所属的分片ID
     * 
     * 使用取模哈希分片策略
     * 
     * @param account_id 账户ID
     * @return 分片ID (0 到 num_shards-1)
     */
    int get_shard_id(local_id account_id) const;
    
    /**
     * @brief 提交转账请求（统一入口）
     * 
     * 智能路由：
     * - 如果src和dst在同一分片 → 分片内转账
     * - 如果src和dst在不同分片 → 跨分片转账
     * 
     * @param src 源账户ID
     * @param dst 目标账户ID
     * @param amount 转账金额
     */
    void submit_transfer(local_id src, local_id dst, balance_t amount);
    
    /**
     * @brief 提交跨分片转账第二步（由AccountShard回调）
     * 
     * 当源账户扣款成功后，AccountShard调用此方法触发目标账户入账
     * 
     * @param correlation_id 关联ID
     */
    void submit_cross_shard_step2(uint64_t correlation_id);
    
    /**
     * @brief 清理跨分片上下文
     * 
     * 在跨分片转账完成后清理追踪信息，释放内存
     * 
     * @param correlation_id 关联ID
     */
    void cleanup_cross_shard_context(uint64_t correlation_id);
    
    /**
     * @brief 等待所有分片完成
     * 
     * 轮询所有分片直到任务队列全部为空
     */
    void wait_all_complete();
    
    /**
     * @brief 打印所有分片的统计信息
     * 
     * 汇总输出每个分片的转账统计
     */
    void print_statistics();

private:
    // ==================== 成员变量 ====================
    
    int num_shards_;                                                  ///< 分片数量
    std::vector<std::unique_ptr<AccountShard>> shards_;              ///< 分片数组
    
    // 跨分片转账协调
    std::unordered_map<uint64_t, CrossShardContext> cross_shard_contexts_;  ///< 跨分片上下文映射
    std::mutex context_mutex_;                                        ///< 保护contexts的互斥锁
    std::atomic<uint64_t> next_correlation_id_;                      ///< 下一个关联ID（原子递增）
    
    // ==================== 私有方法 ====================
    
    /**
     * @brief 处理跨分片转账
     * 
     * 协调流程：
     * 1. 生成唯一的correlation_id
     * 2. 创建并保存跨分片上下文
     * 3. 提交第一步任务到源分片
     * 
     * @param src 源账户ID
     * @param dst 目标账户ID
     * @param amount 转账金额
     * @param src_shard 源分片ID
     * @param dst_shard 目标分片ID
     */
    void handle_cross_shard_transfer(local_id src, local_id dst, balance_t amount,
                                     int src_shard, int dst_shard);
};

#endif // BANKING_SYSTEM_SHARD_SHARD_MANAGER_H