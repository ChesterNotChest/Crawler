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

/**
 * @brief BOSS直聘Cookie自动获取测试
 * 测试功能: 
 *   1. 访问主页面获取基础Cookie
 *   2. 展示Cookie获取的局限性
 *   3. 提供实用建议
 */
void test_zhipin_cookie_fetch();

/**
 * @brief QtWebEngine Cookie完整获取测试
 * 测试功能:
 *   1. 使用真实浏览器环境加载页面
 *   2. 等待JavaScript执行完成
 *   3. 提取所有Cookie（包括动态生成的__zp_stoken__）
 * 要求: 需要安装Qt WebEngine模块
 */
// 已移除Qt WebView相关测试（Windows桌面需Qt WebEngine，MinGW不支持）

/**
 * @brief WebView2 Cookie 提取测试（Windows专用）
 * 功能:
 *   1. 使用WebView2加载页面
 *   2. 执行JavaScript读取document.cookie
 *   3. 输出Cookie信息
 */

/**
 * @brief WebView2 WRL Cookie 提取测试（推荐新架构）
 * 功能:
 *   1. 使用WebView2 WRL模式加载页面
 *   2. 异步信号获取Cookie
 *   3. 输出Cookie信息
 */
// WebView2 WRL Cookie 提取测试（推荐新架构）
// 功能:
//   1. 使用WebView2 WRL模式加载页面
//   2. 异步信号获取Cookie
//   3. 输出Cookie信息
void test_webview2_cookie_wrl();

// 批量爬取集成测试（轻量）
// 注意：该测试仅用于开发/集成验证，不应作为生产作业调度入口
void test_batch_crawl();


#endif // TEST_H
