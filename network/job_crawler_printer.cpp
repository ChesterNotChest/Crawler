#include "job_crawler.h"
#include <iostream>
#include <algorithm>

// 数据格式化打印函数
void print_data_formatted(const std::vector<JobInfo>& job_info_list,
                          const std::vector<TypeInfo>& type_list,
                          const std::vector<AreaInfo>& area_list,
                          const std::vector<SalaryLevelInfo>& salary_level_list) {
    try {
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "主表信息 (职位信息):" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        if (job_info_list.empty()) {
            std::cout << "主表信息为空" << std::endl;
        } else {
            size_t display_count = std::min(job_info_list.size(), size_t(5));
            for (size_t i = 0; i < display_count; ++i) {
                const auto& job = job_info_list[i];
                std::cout << "记录 " << i + 1 << ":" << std::endl;
                std::cout << "PK: 信息ID (" << job.info_id << ")" << std::endl;
                std::cout << "信息名称: " << job.info_name << std::endl;
                std::cout << "SK: 类型ID (" << job.type_id << ")" << std::endl;
                std::cout << "SK: 地区ID (" << job.area_id << ")" << std::endl;
                std::cout << "SK: 薪资档次ID (" << job.salary_level_id << ")" << std::endl;

                if (!job.requirements.empty()) {
                    std::string preview = job.requirements.substr(0, 100);
                    std::cout << "岗位要求: " << preview << "..." << std::endl;
                } else {
                    std::cout << "岗位要求: 无" << std::endl;
                }

                std::cout << "薪资: " << job.salary_min << "-" << job.salary_max << "K" << std::endl;
                std::cout << "创建时间: " << job.create_time << std::endl;
                std::cout << std::string(40, '-') << std::endl;
            }
            if (job_info_list.size() > 5) {
                std::cout << "... 还有 " << job_info_list.size() - 5 << " 条记录未显示" << std::endl;
            }
        }

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "类型表:" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        if (type_list.empty()) {
            std::cout << "类型表为空" << std::endl;
        } else {
            for (const auto& type_item : type_list) {
                std::cout << "PK: 类型ID (" << type_item.type_id << ")" << std::endl;
                std::cout << "类型名称: " << type_item.type_name << std::endl;
                std::cout << std::string(20, '-') << std::endl;
            }
        }

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "地区表:" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        if (area_list.empty()) {
            std::cout << "地区表为空" << std::endl;
        } else {
            for (const auto& area_item : area_list) {
                std::cout << "PK: 地区ID (" << area_item.area_id << ")" << std::endl;
                std::cout << "地区名称: " << area_item.area_name << std::endl;
                std::cout << std::string(20, '-') << std::endl;
            }
        }

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "薪资档次表:" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        if (salary_level_list.empty()) {
            std::cout << "薪资档次表为空" << std::endl;
        } else {
            for (const auto& salary_item : salary_level_list) {
                std::cout << "PK: 薪资档次ID (" << salary_item.salary_level_id << ")" << std::endl;
                std::cout << "薪资上限: " << salary_item.upper_limit << std::endl;
                std::cout << std::string(20, '-') << std::endl;
            }
        }

    } catch (const std::exception& e) {
        print_debug_info("数据打印",
                         "打印数据时发生异常: " + std::string(e.what()),
                         "", DebugLevel::DL_ERROR);
    }
}
