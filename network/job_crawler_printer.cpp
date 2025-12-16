#include "job_crawler.h"
#include <QDebug>
#include <algorithm>

// Data formatted print function
void print_data_formatted(const std::vector<JobInfo>& job_info_list,
                          const std::vector<TypeInfo>& type_list,
                          const std::vector<AreaInfo>& area_list,
                          const std::vector<SalaryLevelInfo>& salary_level_list) {
    try {
        qDebug() << QString(60, '=');
        qDebug() << "Job Info Table:";
        qDebug() << QString(60, '=');

        if (job_info_list.empty()) {
            qDebug() << "Job info table is empty";
        } else {
            size_t display_count = std::min(job_info_list.size(), size_t(5));
            for (size_t i = 0; i < display_count; ++i) {
                const auto& job = job_info_list[i];
                qDebug() << "Record" << (i + 1) << ":";
                qDebug() << "PK: Job ID (" << job.info_id << ")";
                qDebug() << "Job Name:" << job.info_name.c_str();
                qDebug() << "SK: Type ID (" << job.type_id << ")";
                qDebug() << "SK: Area ID (" << job.area_id << ")";
                qDebug() << "SK: Salary Level ID (" << job.salary_level_id << ")";

                if (!job.requirements.empty()) {
                    std::string preview = job.requirements.substr(0, 100);
                    qDebug() << "Requirements:" << QString::fromStdString(preview) << "...";
                } else {
                    qDebug() << "Requirements: None";
                }

                qDebug() << "Salary:" << job.salary_min << "-" << job.salary_max << "K";
                qDebug() << "Create Time:" << job.create_time.c_str();
                qDebug() << QString(40, '-');
            }
            if (job_info_list.size() > 5) {
                qDebug() << "..." << (job_info_list.size() - 5) << "more records not displayed";
            }
        }

        qDebug() << "\n" << QString(60, '=');
        qDebug() << "Recruit Type Table:";
        qDebug() << QString(60, '=');
        if (type_list.empty()) {
            qDebug() << "Type table is empty";
        } else {
            for (const auto& type_item : type_list) {
                qDebug() << "PK: Type ID (" << type_item.type_id << ")";
                qDebug() << "Type Name:" << type_item.type_name.c_str();
                qDebug() << QString(20, '-');
            }
        }

        qDebug() << "\n" << QString(60, '=');
        qDebug() << "City Table:";
        qDebug() << QString(60, '=');
        if (area_list.empty()) {
            qDebug() << "Area table is empty";
        } else {
            for (const auto& area_item : area_list) {
                qDebug() << "PK: Area ID (" << area_item.area_id << ")";
                qDebug() << "Area Name:" << area_item.area_name.c_str();
                qDebug() << QString(20, '-');
            }
        }

        qDebug() << "\n" << QString(60, '=');
        qDebug() << "Salary Level Table:";
        qDebug() << QString(60, '=');
        if (salary_level_list.empty()) {
            qDebug() << "Salary level table is empty";
        } else {
            for (const auto& salary_item : salary_level_list) {
                qDebug() << "PK: Salary Level ID (" << salary_item.salary_level_id << ")";
                qDebug() << "Upper Limit:" << salary_item.upper_limit;
                qDebug() << QString(20, '-');
            }
        }

    } catch (const std::exception& e) {
        print_debug_info("DataPrint",
                         "Exception occurred while printing: " + std::string(e.what()),
                         "", DebugLevel::DL_ERROR);
    }
}
