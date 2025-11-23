#include "banking_system/common/utils.h"

void update_history(BalanceHistory* history, 
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