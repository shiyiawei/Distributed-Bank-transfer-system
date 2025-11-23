// class ClassExtrator {

// private: 
//     string className;
//     vector<string> variableNames;
//     vector<string> variableTypes;
//     vector<string> variableValues;

// public:
//     // build
//     ClassExtrator(const string& name) : className(name) {}

//     // add
//     void addVar(const string& name, const string& type, const string& value) {
//         variableNames.push_back(name);
//         variableTypes.push_back(type);
//         variableValues.push_back(value);
//     }

//     // fetch
//     string getClassName() {
//         return className;
//     }

//     vector<string> getVariableNames() {
//         return variableNames;
//     }   

//     vector<string> getVariableTypes() {
//         return variableTypes;
//     }   

//     vector<string> getVariableValues() {
//         return variableValues;
//     }
    
//     string getValeByName(const string& name){
//         for (int i = 0 ; i < variableNames.size(); i++) {
//             if (variableNames[i] == name) {
//                 return variableNames[i];
//             }
//             return "No result!";
//         }
//     }

// }

#include <iostream>
#include <cstring>
#include <algorithm>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <unistd.h>
#include <sys/types.h>

using namespace std;

#include "labs_headers/message.h"
#include "labs_headers/log.h"
#include "labs_headers/process.h"
#include "labs_headers/banking.h"


// ==================== 全局变量与线程安全机制 ====================

// Lamport逻辑时钟
timestamp_t Lamport_time = 0;
std::mutex lamport_mutex;

/**
 * @brief 线程安全的Lamport时钟更新
 */
timestamp_t update_lamport_time(timestamp_t received_time = 0) {
    std::lock_guard<std::mutex> lock(lamport_mutex);
    Lamport_time = std::max(Lamport_time, received_time) + 1;
    return Lamport_time;
}

timestamp_t get_lamport_time() {
    std::lock_guard<std::mutex> lock(lamport_mutex);
    return Lamport_time;
}

inline int max(int a, int b) {
    return (a > b) ? a : b;
}

// ==================== 辅助函数 ====================

void update_history(BalanceHistory *history, 
                   timestamp_t pending_start_time, 
                   timestamp_t pending_end_time, 
                   balance_t amount, 
                   balance_t pending_money)
{
    int last_time = history->s_history[history->s_history_len - 1].s_time;
    int last_balance = history->s_history[history->s_history_len - 1].s_balance;
    
    for (int i = last_time + 1; i < pending_start_time; i++) {
        history->s_history[history->s_history_len].s_time = i;
        history->s_history[history->s_history_len].s_balance = last_balance;
        history->s_history[history->s_history_len].s_balance_pending_in = 0;
        history->s_history_len++;
    }
    
    for (int i = pending_start_time; i < pending_end_time; i++) {
        history->s_history[history->s_history_len].s_time = i;
        history->s_history[history->s_history_len].s_balance = last_balance;
        history->s_history[history->s_history_len].s_balance_pending_in = pending_money;
        history->s_history_len++;
    }
    
    history->s_history[history->s_history_len].s_balance = amount;
    history->s_history[history->s_history_len].s_time = pending_end_time;
    history->s_history[history->s_history_len].s_balance_pending_in = 0;
    history->s_history_len++;
}

inline balance_t now_balance(const BalanceHistory *history) {
    return history->s_history[history->s_history_len - 1].s_balance;
}

// ==================== 前向声明 ====================
class ShardManager;

// ==================== 转账任务定义 ====================

/**
 * @brief 转账任务类型枚举
 */
enum class TaskType {
    LOCAL_TRANSFER,        // 分片内转账（src和dst在同一分片）
    CROSS_SHARD_STEP1,    // 跨分片转账第一步（源账户扣款）
    CROSS_SHARD_STEP2     // 跨分片转账第二步（目标账户入账）
};

/**
 * @brief 转账任务结构体
 * 封装一次转账所需的所有信息
 */
struct TransferTask {
    TaskType task_type;           // 任务类型
    local_id src_account;         // 源账户ID
    local_id dst_account;         // 目标账户ID
    balance_t amount;             // 转账金额
    
    // 跨分片转账协调信息
    uint64_t correlation_id;      // 关联ID，用于匹配跨分片的两步操作
    int src_shard_id;             // 源分片ID
    int dst_shard_id;             // 目标分片ID
    
    // 构造函数：分片内转账
    TransferTask(local_id src, local_id dst, balance_t amt)
        : task_type(TaskType::LOCAL_TRANSFER)
        , src_account(src)
        , dst_account(dst)
        , amount(amt)
        , correlation_id(0)
        , src_shard_id(-1)
        , dst_shard_id(-1)
    {}
    
    // 构造函数：跨分片转账
    TransferTask(TaskType type, local_id src, local_id dst, balance_t amt,
                 uint64_t corr_id, int src_shard, int dst_shard)
        : task_type(type)
        , src_account(src)
        , dst_account(dst)
        , amount(amt)
        , correlation_id(corr_id)
        , src_shard_id(src_shard)
        , dst_shard_id(dst_shard)
    {}
};

// ==================== 跨分片上下文 ====================

/**
 * @brief 跨分片转账的上下文信息
 * 用于跟踪跨分片转账的状态
 */
struct CrossShardContext {
    TransferTask task;                    // 原始任务
    bool step1_completed;                 // 第一步是否完成
    std::chrono::steady_clock::time_point timestamp;  // 创建时间
    
    CrossShardContext(const TransferTask& t)
        : task(t)
        , step1_completed(false)
        , timestamp(std::chrono::steady_clock::now())
    {}
};

// ==================== 账户分片类 ====================

/**
 * @brief 账户分片类
 * 
 * 每个分片管理一组账户，拥有独立的任务队列和工作线程。
 * 保证同一分片内的所有操作串行执行，不同分片可以并行执行。
 */
class AccountShard {
private:
    int shard_id_;                              // 分片ID
    ShardManager* manager_;                     // 指向管理器的指针
    
    // 任务队列相关
    std::queue<TransferTask> task_queue_;       // 任务队列
    std::mutex queue_mutex_;                    // 队列互斥锁
    std::condition_variable queue_cv_;          // 条件变量
    
    // 线程管理
    std::thread worker_thread_;                 // 工作线程
    std::atomic<bool> stop_flag_;               // 停止标志
    
    // 统计信息（无锁原子操作）
    std::atomic<int> local_transfers_;          // 分片内转账计数
    std::atomic<int> cross_shard_transfers_;    // 跨分片转账计数
    std::atomic<int> failed_transfers_;         // 失败计数
    std::mutex log_mutex_;                      // 日志输出互斥锁

public:
    /**
     * @brief 构造函数
     * @param shard_id 分片ID
     * @param manager 指向ShardManager的指针
     */
    AccountShard(int shard_id, ShardManager* manager)
        : shard_id_(shard_id)
        , manager_(manager)
        , stop_flag_(false)
        , local_transfers_(0)
        , cross_shard_transfers_(0)
        , failed_transfers_(0)
    {
        // 启动工作线程
        worker_thread_ = std::thread(&AccountShard::worker_loop, this);
    }
    
    /**
     * @brief 析构函数 - 优雅关闭线程
     */
    ~AccountShard() {
        // 设置停止标志
        stop_flag_.store(true);
        
        // 唤醒工作线程
        queue_cv_.notify_one();
        
        // 等待线程退出
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    // 禁止拷贝和赋值
    AccountShard(const AccountShard&) = delete;
    AccountShard& operator=(const AccountShard&) = delete;
    
    /**
     * @brief 提交任务到分片队列
     * @param task 转账任务
     */
    void submit_task(const TransferTask& task) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push(task);
        }
        // 在锁外通知，避免不必要的等待
        queue_cv_.notify_one();
    }
    
    /**
     * @brief 等待分片处理完所有任务
     */
    void wait_completion() {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (task_queue_.empty()) {
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    /**
     * @brief 获取统计信息
     */
    void print_statistics() {
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::cout << "  分片 " << shard_id_ << ": "
                  << "本地=" << local_transfers_.load()
                  << ", 跨分片=" << cross_shard_transfers_.load()
                  << ", 失败=" << failed_transfers_.load()
                  << std::endl;
    }

private:
    /**
     * @brief 工作线程主循环
     * 不断从队列取出任务并执行
     */
    void worker_loop() {
        while (true) {
            TransferTask task(0, 0, 0);  // 临时任务
            
            // === 临界区：获取任务 ===
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                
                // 等待任务或停止信号
                queue_cv_.wait(lock, [this] {
                    return stop_flag_.load() || !task_queue_.empty();
                });
                
                // 如果收到停止信号且队列为空，退出循环
                if (stop_flag_.load() && task_queue_.empty()) {
                    break;
                }
                
                // 取出任务
                if (!task_queue_.empty()) {
                    task = task_queue_.front();
                    task_queue_.pop();
                } else {
                    continue;
                }
            }
            // === 临界区结束 ===
            
            // 在锁外处理任务（避免长时间持锁）
            process_task(task);
        }
    }
    
    /**
     * @brief 处理单个任务（根据类型分发）
     */
    void process_task(const TransferTask& task) {
        switch (task.task_type) {
            case TaskType::LOCAL_TRANSFER:
                handle_local_transfer(task);
                break;
            case TaskType::CROSS_SHARD_STEP1:
                handle_cross_shard_step1(task);
                break;
            case TaskType::CROSS_SHARD_STEP2:
                handle_cross_shard_step2(task);
                break;
        }
    }
    
    /**
     * @brief 处理分片内转账
     * 
     * 流程：
     * 1. 发送TRANSFER消息给源账户
     * 2. 等待目标账户的ACK
     * 3. 更新统计信息
     */
    void handle_local_transfer(const TransferTask& task);
    
    /**
     * @brief 处理跨分片转账第一步（源账户扣款）
     * 
     * 流程：
     * 1. 发送TRANSFER消息给源账户
     * 2. 源账户扣款成功
     * 3. 回调manager执行第二步
     */
    void handle_cross_shard_step1(const TransferTask& task);
    
    /**
     * @brief 处理跨分片转账第二步（目标账户入账）
     * 
     * 流程：
     * 1. 转发TRANSFER消息给目标账户
     * 2. 等待ACK确认
     * 3. 更新统计信息
     */
    void handle_cross_shard_step2(const TransferTask& task);
};

// ==================== 分片管理器类 ====================

/**
 * @brief 分片管理器
 * 
 * 负责：
 * 1. 管理所有分片的生命周期
 * 2. 路由转账请求到正确的分片
 * 3. 协调跨分片转账的两步操作
 */
class ShardManager {
private:
    int num_shards_;                                        // 分片数量
    std::vector<std::unique_ptr<AccountShard>> shards_;    // 分片数组
    
    // 跨分片转账协调
    std::unordered_map<uint64_t, CrossShardContext> cross_shard_contexts_;
    std::mutex context_mutex_;                              // 保护contexts的互斥锁
    std::atomic<uint64_t> next_correlation_id_;            // 下一个关联ID

public:
    /**
     * @brief 构造函数
     * @param num_shards 分片数量（建议4, 8, 16等2的幂次）
     */
    explicit ShardManager(int num_shards)
        : num_shards_(num_shards)
        , next_correlation_id_(1)
    {
        std::cout << "\n=== 初始化分片管理器 ===" << std::endl;
        std::cout << "分片数量: " << num_shards_ << std::endl;
        
        // 创建所有分片
        for (int i = 0; i < num_shards_; ++i) {
            shards_.push_back(std::make_unique<AccountShard>(i, this));
        }
        
        std::cout << "所有分片已启动\n" << std::endl;
    }
    
    /**
     * @brief 析构函数
     * 智能指针会自动清理分片资源
     */
    ~ShardManager() = default;
    
    // 禁止拷贝和赋值
    ShardManager(const ShardManager&) = delete;
    ShardManager& operator=(const ShardManager&) = delete;
    
    /**
     * @brief 计算账户所属的分片ID
     * @param account_id 账户ID
     * @return 分片ID (0 到 num_shards-1)
     */
    int get_shard_id(local_id account_id) const {
        return account_id % num_shards_;
    }
    
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
    void submit_transfer(local_id src, local_id dst, balance_t amount) {
        int src_shard = get_shard_id(src);
        int dst_shard = get_shard_id(dst);
        
        if (src_shard == dst_shard) {
            // 分片内转账
            TransferTask task(src, dst, amount);
            shards_[src_shard]->submit_task(task);
        } else {
            // 跨分片转账
            handle_cross_shard_transfer(src, dst, amount, src_shard, dst_shard);
        }
    }
    
    /**
     * @brief 提交跨分片转账第二步（由分片回调）
     * @param correlation_id 关联ID
     */
    void submit_cross_shard_step2(uint64_t correlation_id) {
        CrossShardContext context(TransferTask(0, 0, 0));
        
        // 从map中查找并取出context
        {
            std::lock_guard<std::mutex> lock(context_mutex_);
            auto it = cross_shard_contexts_.find(correlation_id);
            if (it == cross_shard_contexts_.end()) {
                std::cerr << "错误: 找不到correlation_id=" << correlation_id << std::endl;
                return;
            }
            context = it->second;
            context.step1_completed = true;
        }
        
        // 创建第二步任务
        TransferTask step2_task(
            TaskType::CROSS_SHARD_STEP2,
            context.task.src_account,
            context.task.dst_account,
            context.task.amount,
            correlation_id,
            context.task.src_shard_id,
            context.task.dst_shard_id
        );
        
        // 提交到目标分片
        shards_[context.task.dst_shard_id]->submit_task(step2_task);
        
        // 清理context（第二步完成后由handle_cross_shard_step2清理）
    }
    
    /**
     * @brief 清理跨分片上下文
     * @param correlation_id 关联ID
     */
    void cleanup_cross_shard_context(uint64_t correlation_id) {
        std::lock_guard<std::mutex> lock(context_mutex_);
        cross_shard_contexts_.erase(correlation_id);
    }
    
    /**
     * @brief 等待所有分片完成
     */
    void wait_all_complete() {
        for (auto& shard : shards_) {
            shard->wait_completion();
        }
    }
    
    /**
     * @brief 打印所有分片的统计信息
     */
    void print_statistics() {
        std::cout << "\n=== 分片统计信息 ===" << std::endl;
        for (auto& shard : shards_) {
            shard->print_statistics();
        }
    }

private:
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
                                     int src_shard, int dst_shard) {
        // 生成唯一的关联ID
        uint64_t correlation_id = next_correlation_id_.fetch_add(1);
        
        // 创建第一步任务
        TransferTask step1_task(
            TaskType::CROSS_SHARD_STEP1,
            src, dst, amount,
            correlation_id,
            src_shard, dst_shard
        );
        
        // 保存上下文
        {
            std::lock_guard<std::mutex> lock(context_mutex_);
            cross_shard_contexts_.emplace(correlation_id, CrossShardContext(step1_task));
        }
        
        // 提交第一步任务到源分片
        shards_[src_shard]->submit_task(step1_task);
    }
};

// ==================== AccountShard成员函数实现 ====================

void AccountShard::handle_local_transfer(const TransferTask& task) {
    try {
        // 创建转账订单
        TransferOrder order = {task.src_account, task.dst_account, task.amount};
        
        // 发送TRANSFER消息给源账户
        Message msg;
        timestamp_t current_time = update_lamport_time();
        fill_message(&msg, TRANSFER, current_time, &order, sizeof(order));
        send(task.src_account, &msg);
        
        // 等待目标账户的ACK
        Message ack_msg;
        receive(task.dst_account, &ack_msg);
        update_lamport_time(ack_msg.s_header.s_local_time);
        
        // 验证ACK消息
        if (ack_msg.s_header.s_magic == MESSAGE_MAGIC && 
            ack_msg.s_header.s_type == ACK) {
            local_transfers_++;
            
            std::lock_guard<std::mutex> lock(log_mutex_);
            std::cout << "✓ [分片" << shard_id_ << "] 本地转账: " 
                     << static_cast<int>(task.src_account) << " → " 
                     << static_cast<int>(task.dst_account) 
                     << " (金额: " << static_cast<int>(task.amount) << ")" 
                     << std::endl;
        } else {
            failed_transfers_++;
            std::lock_guard<std::mutex> lock(log_mutex_);
            std::cerr << "✗ [分片" << shard_id_ << "] 本地转账失败: 无效ACK" << std::endl;
        }
    } catch (const std::exception& e) {
        failed_transfers_++;
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::cerr << "✗ [分片" << shard_id_ << "] 本地转账异常: " << e.what() << std::endl;
    }
}

void AccountShard::handle_cross_shard_step1(const TransferTask& task) {
    try {
        // 创建转账订单
        TransferOrder order = {task.src_account, task.dst_account, task.amount};
        
        // 发送TRANSFER消息给源账户（扣款）
        Message msg;
        timestamp_t current_time = update_lamport_time();
        fill_message(&msg, TRANSFER, current_time, &order, sizeof(order));
        send(task.src_account, &msg);
        
        {
            std::lock_guard<std::mutex> lock(log_mutex_);
            std::cout << "→ [分片" << shard_id_ << "] 跨分片Step1: " 
                     << static_cast<int>(task.src_account) << " 扣款 " 
                     << static_cast<int>(task.amount) 
                     << " (目标: " << static_cast<int>(task.dst_account) << ")" 
                     << std::endl;
        }
        
        // 扣款成功，回调manager执行第二步
        manager_->submit_cross_shard_step2(task.correlation_id);
        
    } catch (const std::exception& e) {
        failed_transfers_++;
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::cerr << "✗ [分片" << shard_id_ << "] 跨分片Step1异常: " << e.what() << std::endl;
    }
}

void AccountShard::handle_cross_shard_step2(const TransferTask& task) {
    try {
        // 创建转账订单
        TransferOrder order = {task.src_account, task.dst_account, task.amount};
        
        // 转发TRANSFER消息给目标账户（入账）
        Message msg;
        timestamp_t current_time = update_lamport_time();
        fill_message(&msg, TRANSFER, current_time, &order, sizeof(order));
        send(task.dst_account, &msg);
        
        // 等待ACK确认
        Message ack_msg;
        receive(task.dst_account, &ack_msg);
        update_lamport_time(ack_msg.s_header.s_local_time);
        
        // 验证ACK消息
        if (ack_msg.s_header.s_magic == MESSAGE_MAGIC && 
            ack_msg.s_header.s_type == ACK) {
            cross_shard_transfers_++;
            
            std::lock_guard<std::mutex> lock(log_mutex_);
            std::cout << "✓ [分片" << shard_id_ << "] 跨分片Step2完成: " 
                     << static_cast<int>(task.dst_account) << " 入账 " 
                     << static_cast<int>(task.amount) 
                     << " (来源: " << static_cast<int>(task.src_account) << ")" 
                     << std::endl;
        } else {
            failed_transfers_++;
            std::lock_guard<std::mutex> lock(log_mutex_);
            std::cerr << "✗ [分片" << shard_id_ << "] 跨分片Step2失败: 无效ACK" << std::endl;
        }
        
        // 清理跨分片上下文
        manager_->cleanup_cross_shard_context(task.correlation_id);
        
    } catch (const std::exception& e) {
        failed_transfers_++;
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::cerr << "✗ [分片" << shard_id_ << "] 跨分片Step2异常: " << e.what() << std::endl;
    }
}

// ==================== 父进程工作流程 ====================

/**
 * @brief 父进程主控流程（使用分片管理器）
 * 
 * @param count_nodes 节点总数（包括父进程）
 * @param num_shards 分片数量（默认8）
 */
void parent_work(int count_nodes, int num_shards = 8)
{
    AllHistory all_history;
    all_history.s_history_len = count_nodes - 1;

    // ========== 阶段1: 启动同步 ==========
    std::cout << "=== 阶段1: 等待所有账户启动 ===" << std::endl;
    for (int i = 1; i < count_nodes; i++) {
        Message msg;
        receive(i, &msg);
        update_lamport_time(msg.s_header.s_local_time);
    }
    std::cout << "所有账户已就绪！\n" << std::endl;

    // ========== 阶段2: 并发执行转账（分片模式）==========
    std::cout << "=== 阶段2: 开始分片并发转账 ===" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    {
        // 创建分片管理器
        ShardManager manager(num_shards);
        
        // 提交所有转账任务
        std::cout << "提交转账任务..." << std::endl;
        for (int i = 1; i < count_nodes - 1; ++i) {
            manager.submit_transfer(i, i + 1, i);
        }
        if (count_nodes - 1 > 1) {
            manager.submit_transfer(count_nodes - 1, 1, 1);
        }
        
        // 等待所有转账完成
        std::cout << "等待所有分片完成...\n" << std::endl;
        manager.wait_all_complete();
        
        // 打印统计信息
        manager.print_statistics();
        
    } // ShardManager在此处析构，所有分片线程会优雅退出
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    std::cout << "\n总耗时: " << duration.count() << " 毫秒\n" << std::endl;

    // ========== 阶段3: 结束同步 ==========
    std::cout << "=== 阶段3: 通知所有账户停止 ===" << std::endl;
    Message msg;
    timestamp_t current = update_lamport_time();
    fill_message(&msg, STOP, current, nullptr, 0);
    send_multicast(&msg);

    for (int i = 1; i < count_nodes; i++) {
        Message msg;
        receive(i, &msg);
        update_lamport_time(msg.s_header.s_local_time);
    }
    std::cout << "所有账户已停止\n" << std::endl;

    // ========== 阶段4: 收集历史记录 ==========
    std::cout << "=== 阶段4: 收集余额历史记录 ===" << std::endl;
    for (int i = 1; i < count_nodes; i++) {
        Message msg;
        receive(i, &msg);
        update_lamport_time(msg.s_header.s_local_time);
        
        if (msg.s_header.s_magic == MESSAGE_MAGIC && 
            msg.s_header.s_type == BALANCE_HISTORY) {
            BalanceHistory *history = reinterpret_cast<BalanceHistory*>(msg.s_payload);
            std::memcpy(&all_history.s_history[i - 1], history, 
                       msg.s_header.s_payload_len);
        }
    }
    
    print_history(&all_history);
}

// ==================== 子进程工作流程（保持不变）====================

void child_work(struct child_arguments args)
{
    local_id self_id = args.self_id;
    int count_nodes = args.count_nodes;
    uint8_t balance = args.balance;

    BalanceHistory history;
    history.s_history_len = 1;
    history.s_id = self_id;
    std::memset(history.s_history, 0, sizeof(history.s_history));
    history.s_history[0].s_balance = balance;
    
    for (int i = 0; i < MAX_T; ++i) {
        history.s_history[i].s_time = i;
    }

    pid_t self_pid = getpid();
    pid_t parent_pid = getppid();

    char buf[BUF_SIZE];
    Message msg;
    timestamp_t current = update_lamport_time();
    
    std::snprintf(buf, BUF_SIZE, log_started_fmt, current, self_id, 
                 self_pid, parent_pid, balance);
    fill_message(&msg, STARTED, current, buf, std::strlen(buf));
    send_multicast(&msg);
    shared_logger(buf);

    Message recv_msg;
    int count = 0;
    
    for (int i = 1; i < count_nodes; i++) {
        if (i == self_id) continue;
        
        receive(i, &recv_msg);
        update_lamport_time(recv_msg.s_header.s_local_time);
        
        if (recv_msg.s_header.s_magic == MESSAGE_MAGIC && 
            recv_msg.s_header.s_type == STARTED) {
            count++;
        }
    }
    
    if (count == count_nodes - 2) {
        current = get_lamport_time();
        std::snprintf(buf, BUF_SIZE, log_received_all_started_fmt, current, self_id);
        shared_logger(buf);
    }

    count = 0;
    
    while (true) {
        Message req_msg;
        receive_any(&req_msg);
        update_lamport_time(req_msg.s_header.s_local_time);
        
        if (req_msg.s_header.s_magic == MESSAGE_MAGIC && 
            req_msg.s_header.s_type == TRANSFER) {
            TransferOrder *order = reinterpret_cast<TransferOrder*>(req_msg.s_payload);
            
            if (order->s_src == self_id) {
                current = update_lamport_time();
                
                update_history(&history, current, current, 
                             now_balance(&history) - order->s_amount, 0);
                
                std::snprintf(buf, BUF_SIZE, log_transfer_out_fmt, 
                            current, self_id, order->s_amount, order->s_dst);
                shared_logger(buf);
                
                Message response_msg;
                fill_message(&response_msg, TRANSFER, current, 
                           order, sizeof(TransferOrder));
                send(order->s_dst, &response_msg);
            }
            else if (order->s_dst == self_id) {
                current = update_lamport_time(req_msg.s_header.s_local_time);
                
                update_history(&history, 
                             req_msg.s_header.s_local_time,
                             current,
                             now_balance(&history) + order->s_amount,
                             order->s_amount);
                
                std::snprintf(buf, BUF_SIZE, log_transfer_in_fmt, 
                            current, self_id, order->s_amount, order->s_src);
                shared_logger(buf);
                
                Message response_msg;
                fill_message(&response_msg, ACK, current, nullptr, 0);
                send(0, &response_msg);
            }
        }
        else if (req_msg.s_header.s_magic == MESSAGE_MAGIC && 
                 req_msg.s_header.s_type == STOP) {
            current = update_lamport_time(req_msg.s_header.s_local_time);
            
            std::snprintf(buf, BUF_SIZE, log_done_fmt, 
                        current, self_id, now_balance(&history));
            shared_logger(buf);
            
            Message response_msg;
            fill_message(&response_msg, DONE, current, buf, std::strlen(buf));
            send_multicast(&response_msg);
            break;
        }
        else if (req_msg.s_header.s_magic == MESSAGE_MAGIC && 
                 req_msg.s_header.s_type == DONE) {
            count++;
        }
    }

    while (count != count_nodes - 2) {
        Message msg;
        receive_any(&msg);
        update_lamport_time(msg.s_header.s_local_time);
        
        if (msg.s_header.s_magic == MESSAGE_MAGIC && 
            msg.s_header.s_type == DONE) {
            count++;
        }
    }

    if (count == count_nodes - 2) {
        current = get_lamport_time();
        std::snprintf(buf, BUF_SIZE, log_received_all_done_fmt, current, self_id);
        shared_logger(buf);
    }

    Message history_msg;
    current = update_lamport_time();
    fill_message(&history_msg, BALANCE_HISTORY, current, 
               &history, sizeof(history));
    send(0, &history_msg);
}

// ==================== 兼容性函数 ====================

void transfer(local_id src, local_id dst, balance_t amount)
{
    TransferOrder order = {src, dst, amount};
    
    Message msg;
    timestamp_t current = update_lamport_time();
    fill_message(&msg, TRANSFER, current, &order, sizeof(order));
    send(src, &msg);
    
    receive(dst, &msg);
    update_lamport_time(msg.s_header.s_local_time);
}

__attribute__((weak)) void bank_operations(local_id max_id)
{
    for (int i = 1; i < max_id; ++i) {
        transfer(i, i + 1, i);
    }
    if (max_id > 1) {
        transfer(max_id, 1, 1);
    }
}