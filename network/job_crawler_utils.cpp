#include "job_crawler.h"
#include <QDebug>
#include <iomanip>
#include <sstream>
#include <regex>
#include <algorithm>
// helpers are declared in job_crawler.h

// Debug info print function
void print_debug_info(const std::string& stage, const std::string& message,
                      const std::string& data, DebugLevel level) {
    QString prefix = (level == DebugLevel::DL_ERROR) ? "[ERROR]" : "[DEBUG]";
    qDebug() << prefix << "[" << stage.c_str() << "]" << message.c_str();

    if (!data.empty()) {
        qDebug() << "   Data:" << data.c_str();
    }
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
        print_debug_info("Timestamp", std::string("Conversion failed: ") + e.what(),
                         std::to_string(timestamp), DebugLevel::DL_ERROR);
        return "";
    }
}

static inline void replace_all(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = s.find(from, start_pos)) != std::string::npos) {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::string sanitize_html_to_text(const std::string& html) {
    try {
        std::string s = html;

        // Normalize line breaks for common block tags
        s = std::regex_replace(s, std::regex("<br\\s*/?>", std::regex_constants::icase), "\n");
        s = std::regex_replace(s, std::regex("</(p|div|li|h[1-6])>", std::regex_constants::icase), "\n");

        // Remove all remaining tags
        s = std::regex_replace(s, std::regex("<[^>]*>"), "");

        // Decode common entities
        replace_all(s, "&nbsp;", " ");
        replace_all(s, "&amp;", "&");
        replace_all(s, "&lt;", "<");
        replace_all(s, "&gt;", ">");
        replace_all(s, "&quot;", "\"");
        replace_all(s, "&#39;", "'");

        // Trim spaces on each line and collapse multiple blank lines
        std::string out;
        out.reserve(s.size());
        bool prev_nl = false;
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] == '\r') { i++; continue; }
            if (s[i] == '\n') {
                if (!prev_nl) out.push_back('\n');
                prev_nl = true;
                i++;
                continue;
            }
            prev_nl = false;
            out.push_back(s[i]);
            i++;
        }

        // Trim leading/trailing whitespace
        auto l = out.find_first_not_of(" \t\n");
        auto r = out.find_last_not_of(" \t\n");
        if (l == std::string::npos) return "";
        std::string trimmed = out.substr(l, r - l + 1);

        return trimmed;
    } catch (const std::exception& e) {
        print_debug_info("Sanitize", std::string("Failed: ") + e.what());
        return html;
    }
}

int get_int_safe(const json& obj, const char* key, int def) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return def;
    if (it->is_number_integer()) return it->get<int>();
    if (it->is_number()) return static_cast<int>(it->get<double>());
    if (it->is_string()) {
        try { return std::stoi(it->get<std::string>()); } catch (...) { return def; }
    }
    return def;
}

int64_t get_int64_safe(const json& obj, const char* key, int64_t def) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return def;
    if (it->is_number_integer()) return it->get<int64_t>();
    if (it->is_number_float()) return static_cast<int64_t>(it->get<double>());
    if (it->is_string()) {
        try { return std::stoll(it->get<std::string>()); } catch (...) { return def; }
    }
    return def;
}

double get_double_safe(const json& obj, const char* key, double def) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return def;
    if (it->is_number()) return it->get<double>();
    return def;
}

std::string get_string_safe(const json& obj, const char* key, const std::string& def) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) return def;
    if (it->is_string()) return it->get<std::string>();
    return def;
}
