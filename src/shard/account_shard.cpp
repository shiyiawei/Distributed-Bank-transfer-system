#include "banking_system/shard/account_shard.h"
#include "banking_system/shard/shard_manager.h"
#include "banking_system/common/clock.h"
#include "labs_headers/message.h"
#include "labs_headers/process.h"
#include "labs_headers/banking.h"
#include <iostream>
#include <exception>

AccountShard::AccountShard(int shard_id, ShardManager* manager)
    : shard_id_(shard_id)
    , manager_(manager)
    , stop_flag_(false)
    , local_transfers_(0)
    , cross_shard_transfers_(0)
    , failed_transfers_(0)
{
    worker_thread_ = std::thread(&AccountShard::worker_loop, this);
}

AccountShard::~AccountShard() {
    stop_flag_.store(true);
    queue_cv_.notify_one();
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void AccountShard::submit_task(const TransferTask& task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(task);
    }
    queue_cv_.notify_one();
}

void AccountShard::wait_completion() {
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

void AccountShard::print_statistics() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cout << "  分片 " << shard_id_ << ": "
              << "本地=" << local_transfers_.load()
              << ", 跨分片=" << cross_shard_transfers_.load()
              << ", 失败=" << failed_transfers_.load()
              << std::endl;
}

void AccountShard::worker_loop() {
    while (true) {
        TransferTask task(0, 0, 0);
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            queue_cv_.wait(lock, [this] {
                return stop_flag_.load() || !task_queue_.empty();
            });
            
            if (stop_flag_.load() && task_queue_.empty()) {
                break;
            }
            
            if (!task_queue_.empty()) {
                task = task_queue_.front();
                task_queue_.pop();
            } else {
                continue;
            }
        }
        
        process_task(task);
    }
}

void AccountShard::process_task(const TransferTask& task) {
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

void AccountShard::handle_local_transfer(const TransferTask& task) {
    try {
        TransferOrder order = {task.src_account, task.dst_account, task.amount};
        
        Message msg;
        timestamp_t current_time = update_lamport_time();
        fill_message(&msg, TRANSFER, current_time, &order, sizeof(order));
        send(task.src_account, &msg);
        
        Message ack_msg;
        receive(task.dst_account, &ack_msg);
        update_lamport_time(ack_msg.s_header.s_local_time);
        
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
        TransferOrder order = {task.src_account, task.dst_account, task.amount};
        
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
        
        manager_->submit_cross_shard_step2(task.correlation_id);
        
    } catch (const std::exception& e) {
        failed_transfers_++;
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::cerr << "✗ [分片" << shard_id_ << "] 跨分片Step1异常: " << e.what() << std::endl;
    }
}

void AccountShard::handle_cross_shard_step2(const TransferTask& task) {
    try {
        TransferOrder order = {task.src_account, task.dst_account, task.amount};
        
        Message msg;
        timestamp_t current_time = update_lamport_time();
        fill_message(&msg, TRANSFER, current_time, &order, sizeof(order));
        send(task.dst_account, &msg);
        
        Message ack_msg;
        receive(task.dst_account, &ack_msg);
        update_lamport_time(ack_msg.s_header.s_local_time);
        
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
        
        manager_->cleanup_cross_shard_context(task.correlation_id);
        
    } catch (const std::exception& e) {
        failed_transfers_++;
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::cerr << "✗ [分片" << shard_id_ << "] 跨分片Step2异常: " << e.what() << std::endl;
    }
}