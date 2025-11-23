#ifndef BANKING_SYSTEM_SHARD_ACCOUNT_SHARD_H
#define BANKING_SYSTEM_SHARD_ACCOUNT_SHARD_H

#include "banking_system/transfer/transfer_task.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

// 前向声明
class ShardManager;

// ==================== 账户分片类 ====================

/**
 * @brief 账户分片类
 * 
 * 每个分片管理一组账户，拥有独立的任务队列和工作线程。
 * 保证同一分片内的所有操作串行执行，不同分片可以并行执行。
 * 
 * 设计要点：
 * - 每个分片一个工作线程，避免分片内竞争
 * - 使用任务队列解耦任务提交和执行
 * - 支持分片内转账和跨分片转账协调
 */
class AccountShard {
public:
    /**
     * @brief 构造函数
     * @param shard_id 分片ID
     * @param manager 指向ShardManager的指针（用于回调）
     */
    AccountShard(int shard_id, ShardManager* manager);
    
    /**
     * @brief 析构函数 - 优雅关闭线程
     */
    ~AccountShard();
    
    // 禁止拷贝和赋值
    AccountShard(const AccountShard&) = delete;
    AccountShard& operator=(const AccountShard&) = delete;
    
    /**
     * @brief 提交任务到分片队列
     * 
     * 线程安全的任务提交接口
     * 
     * @param task 转账任务
     */
    void submit_task(const TransferTask& task);
    
    /**
     * @brief 等待分片处理完所有任务
     * 
     * 轮询检查队列是否为空，适用于同步等待场景
     */
    void wait_completion();
    
    /**
     * @brief 获取并打印统计信息
     * 
     * 输出本分片的转账统计：本地转账数、跨分片转账数、失败数
     */
    void print_statistics();

private:
    // ==================== 成员变量 ====================
    
    int shard_id_;                              ///< 分片ID
    ShardManager* manager_;                     ///< 指向管理器的指针
    
    // 任务队列相关
    std::queue<TransferTask> task_queue_;       ///< 任务队列
    std::mutex queue_mutex_;                    ///< 队列互斥锁
    std::condition_variable queue_cv_;          ///< 条件变量（用于线程同步）
    
    // 线程管理
    std::thread worker_thread_;                 ///< 工作线程
    std::atomic<bool> stop_flag_;               ///< 停止标志
    
    // 统计信息（无锁原子操作）
    std::atomic<int> local_transfers_;          ///< 分片内转账计数
    std::atomic<int> cross_shard_transfers_;    ///< 跨分片转账计数
    std::atomic<int> failed_transfers_;         ///< 失败计数
    std::mutex log_mutex_;                      ///< 日志输出互斥锁
    
    // ==================== 私有方法 ====================
    
    /**
     * @brief 工作线程主循环
     * 
     * 不断从队列取出任务并执行，支持优雅退出
     */
    void worker_loop();
    
    /**
     * @brief 处理单个任务（根据类型分发）
     * @param task 要处理的任务
     */
    void process_task(const TransferTask& task);
    
    /**
     * @brief 处理分片内转账
     * 
     * 流程：
     * 1. 发送TRANSFER消息给源账户
     * 2. 等待目标账户的ACK
     * 3. 更新统计信息
     * 
     * @param task 转账任务
     */
    void handle_local_transfer(const TransferTask& task);
    
    /**
     * @brief 处理跨分片转账第一步（源账户扣款）
     * 
     * 流程：
     * 1. 发送TRANSFER消息给源账户
     * 2. 源账户扣款成功
     * 3. 回调manager执行第二步
     * 
     * @param task 转账任务
     */
    void handle_cross_shard_step1(const TransferTask& task);
    
    /**
     * @brief 处理跨分片转账第二步（目标账户入账）
     * 
     * 流程：
     * 1. 转发TRANSFER消息给目标账户
     * 2. 等待ACK确认
     * 3. 更新统计信息
     * 
     * @param task 转账任务
     */
    void handle_cross_shard_step2(const TransferTask& task);
};

#endif // BANKING_SYSTEM_SHARD_ACCOUNT_SHARD_H