#include "banking_system/process/child_worker.h"
#include "banking_system/common/clock.h"
#include "banking_system/common/utils.h"
#include "labs_headers/message.h"
#include "labs_headers/process.h"
#include "labs_headers/log.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>

ChildWorker::ChildWorker(const ChildArguments& args)
    : self_id_(args.self_id)
    , count_nodes_(args.count_nodes)
    , initial_balance_(args.balance)
{
    init_history();
}

void ChildWorker::run() {
    send_started_and_wait();
    message_loop();
    wait_all_done();
    send_history();
}

void ChildWorker::init_history() {
    history_.s_history_len = 1;
    history_.s_id = self_id_;
    std::memset(history_.s_history, 0, sizeof(history_.s_history));
    history_.s_history[0].s_balance = initial_balance_;
    
    for (int i = 0; i < MAX_T; ++i) {
        history_.s_history[i].s_time = i;
    }
}

void ChildWorker::send_started_and_wait() {
    pid_t self_pid = getpid();
    pid_t parent_pid = getppid();

    char buf[BUF_SIZE];
    Message msg;
    timestamp_t current = update_lamport_time();
    
    std::snprintf(buf, BUF_SIZE, log_started_fmt, current, self_id_, 
                 self_pid, parent_pid, initial_balance_);
    fill_message(&msg, STARTED, current, buf, std::strlen(buf));
    send_multicast(&msg);
    shared_logger(buf);

    Message recv_msg;
    int count = 0;
    
    for (int i = 1; i < count_nodes_; i++) {
        if (i == self_id_) continue;
        
        receive(i, &recv_msg);
        update_lamport_time(recv_msg.s_header.s_local_time);
        
        if (recv_msg.s_header.s_magic == MESSAGE_MAGIC && 
            recv_msg.s_header.s_type == STARTED) {
            count++;
        }
    }
    
    if (count == count_nodes_ - 2) {
        current = get_lamport_time();
        std::snprintf(buf, BUF_SIZE, log_received_all_started_fmt, current, self_id_);
        shared_logger(buf);
    }
}

void ChildWorker::message_loop() {
    int done_count = 0;
    
    while (true) {
        Message req_msg;
        receive_any(&req_msg);
        update_lamport_time(req_msg.s_header.s_local_time);
        
        if (req_msg.s_header.s_magic == MESSAGE_MAGIC && 
            req_msg.s_header.s_type == TRANSFER) {
            TransferOrder *order = reinterpret_cast<TransferOrder*>(req_msg.s_payload);
            
            if (order->s_src == self_id_) {
                handle_transfer_as_source(order);
            }
            else if (order->s_dst == self_id_) {
                handle_transfer_as_destination(order, req_msg.s_header.s_local_time);
            }
        }
        else if (req_msg.s_header.s_magic == MESSAGE_MAGIC && 
                 req_msg.s_header.s_type == STOP) {
            timestamp_t current = update_lamport_time(req_msg.s_header.s_local_time);
            
            char buf[BUF_SIZE];
            std::snprintf(buf, BUF_SIZE, log_done_fmt, 
                        current, self_id_, now_balance(&history_));
            shared_logger(buf);
            
            Message response_msg;
            fill_message(&response_msg, DONE, current, buf, std::strlen(buf));
            send_multicast(&response_msg);
            break;
        }
        else if (req_msg.s_header.s_magic == MESSAGE_MAGIC && 
                 req_msg.s_header.s_type == DONE) {
            done_count++;
        }
    }
}

void ChildWorker::wait_all_done() {
    int count = 0;

    while (count != count_nodes_ - 2) {
        Message msg;
        receive_any(&msg);
        update_lamport_time(msg.s_header.s_local_time);
        
        if (msg.s_header.s_magic == MESSAGE_MAGIC && 
            msg.s_header.s_type == DONE) {
            count++;
        }
    }

    if (count == count_nodes_ - 2) {
        timestamp_t current = get_lamport_time();
        char buf[BUF_SIZE];
        std::snprintf(buf, BUF_SIZE, log_received_all_done_fmt, current, self_id_);
        shared_logger(buf);
    }
}

void ChildWorker::send_history() {
    Message history_msg;
    timestamp_t current = update_lamport_time();
    fill_message(&history_msg, BALANCE_HISTORY, current, 
               &history_, sizeof(history_));
    send(0, &history_msg);
}

void ChildWorker::handle_transfer_as_source(const TransferOrder* order) {
    timestamp_t current = update_lamport_time();
    
    update_history(&history_, current, current, 
                 now_balance(&history_) - order->s_amount, 0);
    
    char buf[BUF_SIZE];
    std::snprintf(buf, BUF_SIZE, log_transfer_out_fmt, 
                current, self_id_, order->s_amount, order->s_dst);
    shared_logger(buf);
    
    Message response_msg;
    fill_message(&response_msg, TRANSFER, current, 
               const_cast<TransferOrder*>(order), sizeof(TransferOrder));
    send(order->s_dst, &response_msg);
}

void ChildWorker::handle_transfer_as_destination(const TransferOrder* order, 
                                                 timestamp_t received_time) {
    timestamp_t current = update_lamport_time(received_time);
    
    update_history(&history_, 
                 received_time,
                 current,
                 now_balance(&history_) + order->s_amount,
                 order->s_amount);
    
    char buf[BUF_SIZE];
    std::snprintf(buf, BUF_SIZE, log_transfer_in_fmt, 
                current, self_id_, order->s_amount, order->s_src);
    shared_logger(buf);
    
    Message response_msg;
    fill_message(&response_msg, ACK, current, nullptr, 0);
    send(0, &response_msg);
}

void child_work(struct child_arguments args) {
    ChildWorker worker(args);
    worker.run();
}