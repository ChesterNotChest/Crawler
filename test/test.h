#ifndef TEST_H
#define TEST_H

// ============================================================
// 单元测试 - 测试单个模块的功能
// ============================================================

/**
 * @brief InternetTask 单元测试
 * 测试范围: 网络爬取功能（不涉及数据库）
 * 测试场景:
 *   - 单页数据爬取
 *   - 多页批量爬取
 *   - 数据结构验证
 *   - 映射数据完整性
 */
void test_internet_task_unit();

/**
 * @brief SqlTask 单元测试
 * 测试范围: 数据转换和存储功能（不涉及网络）
 * 测试场景:
 *   - 单条Mock数据存储
 *   - 批量Mock数据存储
 *   - 类型转换验证 (::JobInfo → SQLNS::JobInfo)
 *   - 数据库查询验证
 *   - 基础SQL操作测试
 */
void test_sql_task_unit();

// ============================================================
// 集成测试 - 测试模块间的协作
// ============================================================

/**
 * @brief CrawlerTask 集成测试
 * 测试范围: Crawler侧的完整工作流
 * 测试场景:
 *   1. InternetTask + SqlTask 协同工作
 *   2. 单页爬取并存储
 *   3. 多页批量爬取并存储
 *   4. 仅爬取模式
 *   5. 仅存储模式
 *   6. 数据库验证
 */
void test_crawler_task_integration();

// ============================================================
// 废弃的测试 - 可删除
// ============================================================

// [Deprecated] 以下测试已被新架构替代，可安全删除
void test_sql_database();           // 被 test_sql_task_unit 替代
void test_job_crawler();            // 被 test_internet_task_unit 替代
void test_integration_crawler_to_sql(); // 被 test_crawler_task_integration 替代
void test_crawler_task_workflow();  // 重命名为 test_crawler_task_integration

#endif // TEST_H
