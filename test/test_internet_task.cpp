#include "test.h"
#include "tasks/internet_task.h"
#include <QDebug>
#include <iostream>

/**
 * @brief InternetTask单元测试
 * 测试范围: 网络爬取功能（不涉及数据库）
 * 测试场景:
 *   1. 单页数据爬取
 *   2. 多页批量爬取
 *   3. 数据结构验证
 *   4. 映射数据完整性
 */
void test_internet_task_unit() {
    qDebug() << "\n========== InternetTask 单元测试 ==========\n";
    
    InternetTask internetTask;
    
    // ========== 场景1: 单页爬取 ==========
    qDebug() << "[场景1] 单页数据爬取";
    auto [jobs1, mapping1] = internetTask.fetchJobData(1, 20);
    
    qDebug() << "✓ 爬取结果:";
    qDebug() << "  - JobInfo数量:" << jobs1.size();
    qDebug() << "  - 类型映射数量:" << mapping1.type_list.size();
    qDebug() << "  - 地区映射数量:" << mapping1.area_list.size();
    qDebug() << "  - 薪资档次数量:" << mapping1.salary_level_list.size();
    
    if (!jobs1.empty()) {
        qDebug() << "\n✓ 第一条数据样本:";
        qDebug() << "  - 职位ID:" << jobs1[0].info_id;
        qDebug() << "  - 职位名称:" << QString::fromStdString(jobs1[0].info_name);
        qDebug() << "  - 公司ID:" << jobs1[0].company_id;
        qDebug() << "  - 薪资范围:" << jobs1[0].salary_min << "K -" << jobs1[0].salary_max << "K";
        qDebug() << "  - 标签数量:" << jobs1[0].tag_ids.size();
    }
    
    // ========== 场景2: 多页爬取 ==========
    qDebug() << "\n[场景2] 多页批量爬取 (第2-3页)";
    auto [jobs2, mapping2] = internetTask.fetchJobDataMultiPage(2, 5, 20);
    
    qDebug() << "✓ 批量爬取结果:";
    qDebug() << "  - 总JobInfo数量:" << jobs2.size();
    qDebug() << "  - 预期数量: ~40条 (2页 × 20条/页)";
    qDebug() << "  - 合并后映射数据量:" << mapping2.type_list.size() + mapping2.area_list.size();
    
    // ========== 场景3: 数据结构验证 ==========
    qDebug() << "\n[场景3] 数据结构完整性验证";
    bool dataValid = true;
    
    for (size_t i = 0; i < std::min(jobs1.size(), size_t(3)); ++i) {
        const auto& job = jobs1[i];
        
        // 验证必填字段
        if (job.info_id <= 0) {
            qDebug() << "❌ 职位ID无效:" << job.info_id;
            dataValid = false;
        }
        if (job.info_name.empty()) {
            qDebug() << "❌ 职位名称为空";
            dataValid = false;
        }
        if (job.create_time.empty()) {
            qDebug() << "❌ 创建时间为空";
            dataValid = false;
        }
    }
    
    if (dataValid) {
        qDebug() << "✓ 数据结构验证通过";
    }
    
    // ========== 场景4: 映射数据检查 ==========
    qDebug() << "\n[场景4] 映射数据详情";
    if (!mapping1.type_list.empty()) {
        qDebug() << "✓ 招聘类型:";
        for (const auto& type : mapping1.type_list) {
            qDebug() << "  - ID" << type.type_id << ":" 
                     << QString::fromStdString(type.type_name);
        }
    }
    
    if (!mapping1.area_list.empty()) {
        qDebug() << "\n✓ 地区列表 (前5个):";
        for (size_t i = 0; i < std::min(mapping1.area_list.size(), size_t(5)); ++i) {
            qDebug() << "  - ID" << mapping1.area_list[i].area_id << ":" 
                     << QString::fromStdString(mapping1.area_list[i].area_name);
        }
    }
    
    qDebug() << "\n✅ InternetTask 单元测试完成!\n";
}
