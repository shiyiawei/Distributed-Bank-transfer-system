#ifndef BANKING_SYSTEM_PROCESS_CHILD_WORKER_H
#define BANKING_SYSTEM_PROCESS_CHILD_WORKER_H

#include "banking_system/common/types.h"
#include "labs_headers/banking.h"
#include "labs_headers/process.h"

// ==================== 子进程参数结构体 ====================

/**
 * @brief 子进程参数结构体
 * 
 * 用于传递子进程初始化所需的参数
 */
struct ChildArguments {
    local_id self_id;      ///< 自身进程ID
    int count_nodes;       ///< 节点总数
    uint8_t balance;       ///< 初始余额
};

// 类型别名，保持与原代码兼容
using child_arguments = ChildArguments;

// ==================== 子进程工作器类 ====================

/**
 * @brief 子进程工作器类
 * 
 * 代表分布式系统中的一个账户进程，负责：
 * 1. 维护本地余额历史
 * 2. 处理转账请求（作为源账户或目标账户）
 * 3. 响应系统控制消息（STOP等）
 * 4. 与其他账户进程同步
 */
class ChildWorker {
public:
    /**
     * @brief 构造函数
     * @param args 子进程参数
     */
    explicit ChildWorker(const ChildArguments& args);
    
    /**
     * @brief 执行子进程主循环
     * 
     * 工作流程：
     * 1. 发送STARTED消息
     * 2. 接收并处理TRANSFER和STOP消息
     * 3. 同步DONE状态
     * 4. 发送余额历史
     */
    void run();

private:
    local_id self_id_;          ///< 自身进程ID
    int count_nodes_;           ///< 节点总数
    uint8_t initial_balance_;   ///< 初始余额
    BalanceHistory history_;    ///< 余额历史记录
    
    /**
     * @brief 初始化余额历史
     */
    void init_history();
    
    /**
     * @brief 发送启动消息并等待其他进程就绪
     */
    void send_started_and_wait();
    
    /**
     * @brief 主消息处理循环
     * 
     * 处理TRANSFER和STOP消息
     */
    void message_loop();
    
    /**
     * @brief 等待所有DONE消息
     */
    void wait_all_done();
    
    /**
     * @brief 发送余额历史给父进程
     */
    void send_history();
    
    /**
     * @brief 处理作为源账户的转账请求
     * @param order 转账订单
     */
    void handle_transfer_as_source(const TransferOrder* order);
    
    /**
     * @brief 处理作为目标账户的转账请求
     * @param order 转账订单
     * @param received_time 接收时间
     */
    void handle_transfer_as_destination(const TransferOrder* order, 
                                       timestamp_t received_time);
};

// ==================== 兼容性函数 ====================

/**
 * @brief 子进程工作流程（函数版本，保持向后兼容）
 * @param args 子进程参数
 */
void child_work(struct child_arguments args);

#endif // BANKING_SYSTEM_PROCESS_CHILD_WORKER_H