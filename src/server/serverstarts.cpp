#include"serverstarts.hpp"

std::atomic<int> g_total_recv_requests{0};
std::atomic<int> g_json_parse_errors{0};
std::atomic<int> g_valid_requests{0};
std::atomic<int> g_db_connect_success_count{0};