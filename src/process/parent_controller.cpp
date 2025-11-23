#include "banking_system/process/parent_controller.h"
#include "banking_system/shard/shard_manager.h"
#include "banking_system/common/clock.h"
#include "labs_headers/message.h"
#include "labs_headers/process.h"
#include "labs_headers/banking.h"
#include "labs_headers/log.h"
#include <iostream>
#include <cstring>
#include <chrono>

ParentController::ParentController(int count_nodes, int num_shards)
    : count_nodes_(count_nodes)
    , num_shards_(num_shards)
{
}

void ParentController::run() {
    phase1_wait_startup();
    phase2_execute_transfers();
    phase3_stop_all();
    phase4_collect_history();
}

void ParentController::phase1_wait_startup() {
    std::cout << "=== 阶段1: 等待所有账户启动 ===" << std::endl;
    for (int i = 1; i < count_nodes_; i++) {
        Message msg;
        receive(i, &msg);
        update_lamport_time(msg.s_header.s_local_time);
    }
    std::cout << "所有账户已就绪！\n" << std::endl;
}

void ParentController::phase2_execute_transfers() {
    std::cout << "=== 阶段2: 开始分片并发转账 ===" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    {
        ShardManager manager(num_shards_);
        
        std::cout << "提交转账任务..." << std::endl;
        for (int i = 1; i < count_nodes_ - 1; ++i) {
            manager.submit_transfer(i, i + 1, i);
        }
        if (count_nodes_ - 1 > 1) {
            manager.submit_transfer(count_nodes_ - 1, 1, 1);
        }
        
        std::cout << "等待所有分片完成...\n" << std::endl;
        manager.wait_all_complete();
        
        manager.print_statistics();
        
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    std::cout << "\n总耗时: " << duration.count() << " 毫秒\n" << std::endl;
}

void ParentController::phase3_stop_all() {
    std::cout << "=== 阶段3: 通知所有账户停止 ===" << std::endl;
    Message msg;
    timestamp_t current = update_lamport_time();
    fill_message(&msg, STOP, current, nullptr, 0);
    send_multicast(&msg);

    for (int i = 1; i < count_nodes_; i++) {
        Message msg;
        receive(i, &msg);
        update_lamport_time(msg.s_header.s_local_time);
    }
    std::cout << "所有账户已停止\n" << std::endl;
}

void ParentController::phase4_collect_history() {
    std::cout << "=== 阶段4: 收集余额历史记录 ===" << std::endl;
    
    AllHistory all_history;
    all_history.s_history_len = count_nodes_ - 1;
    
    for (int i = 1; i < count_nodes_; i++) {
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

void parent_work(int count_nodes, int num_shards) {
    ParentController controller(count_nodes, num_shards);
    controller.run();
}