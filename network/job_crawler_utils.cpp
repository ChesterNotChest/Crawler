#include "job_crawler.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// 调试信息打印函数
void print_debug_info(const std::string& stage, const std::string& message,
                      const std::string& data, DebugLevel level) {
    std::string prefix = (level == DebugLevel::DL_ERROR) ? "[ERROR] " : "[DEBUG] ";
    std::cout << prefix << "[" << stage << "] " << message << std::endl;

    if (!data.empty()) {
        std::cout << "   数据: " << data << std::endl;
    }
    std::cout << std::endl;
}

// 时间戳转换函数
std::string timestamp_to_datetime(int64_t timestamp) {
    try {
        time_t time_value;
        if (timestamp > 1000000000000) { // 毫秒时间戳
            time_value = timestamp / 1000;
        } else { // 秒时间戳
            time_value = timestamp;
        }

        tm* time_info = localtime(&time_value);
        if (!time_info) {
            return "";
        }

        std::stringstream ss;
        ss << std::put_time(time_info, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    } catch (const std::exception& e) {
        print_debug_info("时间转换", std::string("时间转换失败: ") + e.what(),
                         std::to_string(timestamp), DebugLevel::DL_ERROR);
        return "";
    }
}

// CURL写回调函数
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}
