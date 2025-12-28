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
 * @brief WebView2 WRL Cookie 提取测试（推荐新架构）
 * 功能:
 *   1. 使用WebView2 WRL模式加载页面
 *   2. 异步信号获取Cookie
 *   3. 输出Cookie信息
 */
void test_webview2_cookie_wrl();

// 批量爬取集成测试（轻量）
// 注意：该测试仅用于开发/集成验证，不应作为生产作业调度入口
void test_batch_crawl();

// PresenterTask 功能测试 - 数据查询与处理
void test_presenter_task();


#endif // TEST_H
