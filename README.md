# Crawler 项目

一个基于Qt 6.9的高效网页爬虫工具，支持并行任务处理、数据库存储和SQL查询。

## 📋 项目特性

- **Qt 6 框架** - 使用Qt 6.9进行GUI开发
- **HTTP 请求** - 基于libcurl库实现网络爬取
- **JSON 处理** - 使用nlohmann-json进行数据解析
- **数据库支持** - SQLite数据库集成，支持任务和结果存储
- **并行处理** - 多任务并行爬取，提高效率
- **跨平台** - 支持Windows、Linux、macOS等平台
- **配置管理** - config.json统一管理敏感数据（Cookie等）

## 🔐 Cookie管理方案

**推荐方式：使用config.json配置文件**

Qt WebView在Windows桌面平台依赖Qt WebEngine，而WebEngine不支持MinGW（仅MSVC），因此采用配置文件方案。

### 使用步骤：

1. **从浏览器获取Cookie**
   - 访问BOSS直聘网站并登录
   - 打开开发者工具（F12）→ Application/存储 → Cookies
   - 复制完整Cookie字符串

2. **编辑config.json文件**
```json
{
  "zhipin": {
    "cookie": "你的完整Cookie字符串（包含__zp_stoken__等）",
    "city": "101010100",
    "updateTime": "2025-12-19"
  }
}
```

3. **程序自动使用**
   - 程序启动时自动加载config.json
   - API请求中自动注入Cookie
   - 无需修改代码

## 🗂️ 项目结构

```
Crawler/
├── main.cpp                 # 程序入口
├── mainwindow.cpp/.h/.ui    # 主窗口UI
├── CMakeLists.txt           # CMake构建配置
├── network/                 # 网络爬虫核心模块
│   ├── job_crawler.h        # 爬虫数据结构定义
│   ├── job_crawler_main.cpp # 爬虫主入口函数
│   ├── job_crawler_network.cpp
│   ├── job_crawler_parser.cpp
│   ├── job_crawler_printer.cpp
│   └── job_crawler_utils.cpp
├── db/                      # 数据库模块
│   ├── sqlinterface.cpp/.h  # SQL接口 (SQLNS命名空间)
├── tasks/                   # 任务管理层
│   ├── sql_task.cpp/.h      # SQL存储任务 (Network→SQL桥梁)
│   ├── internet_task.cpp/.h # 网络爬取任务 (封装爬虫调用)
│   └── crawler_task.cpp/.h  # 总任务协调器 (InternetTask+SqlTask)
├── constants/               # 数据结构定义
│   ├── network_types.h      # 网络模块数据结构
│   └── db_types.h           # 数据库模块数据结构
├── test/                    # 测试模块
│   ├── test_job_crawler.cpp
│   ├── test_sql.cpp
│   ├── test_integration.cpp
│   ├── test_crawler_task.cpp
│   └── test.h
└── include/                 # 第三方库 (需要自行补充)
    ├── curl-8.17.0_5-win64-mingw/
    └── nlohmann-json-develop/
```

## 🏗️ 架构设计

项目采用分层架构，通过任务层实现模块间的解耦：

```
┌─────────────────────────────────────────────┐
│          CrawlerTask (总协调器)             │
│  提供: crawlAndStore()自动遍历所有类型      │
└──────────────┬─────────────┬────────────────┘
               │             │
       ┌───────▼─────┐  ┌────▼────────┐
       │InternetTask │  │  SqlTask    │
       │  (网络层)   │  │  (存储层)   │
       └──────┬──────┘  └────┬────────┘
              │              │
       ┌──────▼──────┐  ┌────▼────────┐
       │job_crawler  │  │SQLInterface │
       │ (爬虫核心)  │  │ (数据库)    │
       └─────────────┘  └─────────────┘
```

## 数据来源与SQL对齐
```

PK: 岗位ID (int) data.id
岗位名称 (char[]) data.jobName
SK: 类型ID (int) data.recruitType
SK: 来源ID (int) (TODO)
SK: 地区ID (int): 自增ID（见下）
SK: TagID (int): 自增ID（见下）
SK: 薪资档次ID (int): 自设计ID（见下）

岗位要求 (char[5000]) data.ext.requirements
最高薪资 (double) data.salaryMax
最低薪资 (double) data.salaryMin
创建日期(dateTime) data.createTime
更新日期(dateTime) data.updateTime
HR上次线时间(dateTime) data.user.loginTime

PK: 公司ID data.companyId
公司名称 (char[]) data.user.identity[].companyName（仅从identity列表获取，缺失视为错误）

PK: 类型ID data.recruitType
类型名称 (char[]):   （固定枚举，1 校招，2 实习，3 社招）

PK: 地区ID: 自增ID
地区名称: data.jobCity（字符串）；在数据库侧按名称插入/查找，返回自增cityId

PK: TagID: 自增ID
Tag内容：遍历 data.pcTagInfo.jobInfoTagList，优先取 `tag.title`（兼容 `content`/`name`），对标题进行去重后插入 JobTag（INSERT OR IGNORE），并返回对应 tagId 用于 JobTagMapping

PK: 薪资档次ID: 自设计ID
薪资上限 (int)（设计阶梯，按Max与上限比较，阈值分类存储）

## 🛠️ 技术栈

| 组件 | 版本 | 用途 |
|------|------|------|
| Qt | 6.9.2 | GUI框架 |
| CMake | 3.30.5 | 项目构建 |
| libcurl | 8.17.0 | HTTP请求 |
| nlohmann-json | - | JSON解析 |
| SQLite | - | 数据库 |
| MinGW | 64-bit | C++编译器 |

## 📦 依赖库

### 外部依赖
- **libcurl** - 网络HTTP库
- **nlohmann-json** - 现代C++ JSON库
- **SQLite** - 轻量级数据库

### 内置模块
- **job_crawler** - 爬虫核心实现
- **sqlinterface** - 数据库操作接口
- **sqltask** - SQL任务管理



## 📚 模块说明

### 常量定义模块 (constants/)
- **network_types.h** - 网络模块数据结构（`JobInfo`、`MappingData`、`DebugLevel`等）
- **db_types.h** - 数据库模块数据结构（`SQLNS::JobInfo`、`SQLNS::SalaryRange`等）

### 网络爬虫模块 (network/)
- **job_crawler.h** - 爬虫接口函数声明（引用`constants/network_types.h`）
- **job_crawler_main.cpp** - 爬虫主入口（聚合网络、解析与打印）
- **job_crawler_network.cpp** - 网络请求实现（libcurl，启用SSL配置与30s超时，附带User-Agent）
- **job_crawler_parser.cpp** - 响应数据解析（支持多种JSON格式、原始JSON调试打印、安全类型转换）
- **job_crawler_printer.cpp** - 数据输出（统一使用 `qDebug()`，英文标签）
- **job_crawler_utils.cpp** - 工具函数（`print_debug_info`、时间戳转换、HTML清理、CURL写回调）

### 数据库模块 (db/)
- **sqlinterface.h/cpp** - SQL执行接口（SQLNS命名空间）
- 支持SQLite数据库操作，采用INSERT OR IGNORE模式避免重复
- 提供岗位、公司、城市、标签等表的增删查改
- 实现64位jobId支持，避免整型溢出

### 任务模块 (tasks/)
- **crawler_task.h/cpp** - 总任务协调器，自动遍历所有招聘类型（校招/实习/社招）
- **internet_task.h/cpp** - 网络爬取任务，封装HTTP请求与JSON解析
- **sql_task.h/cpp** - SQL存储任务，负责数据类型转换、薪资档次计算（支持实习元/天与全职K/月）

## 🧪 测试

项目包含测试代码在 `test/` 目录：
- `test_internet_task.cpp` - 网络爬取单元测试
- `test_sql_task.cpp` - SQL任务/映射与存储测试
- `test_crawler_task.cpp` - 爬取+存储集成测试

运行测试：
```bash
cmake --build . --target test
```

## 📄 许可证

MIT License

## 👥 贡献

欢迎提交 Issue 和 Pull Request！

## 📧 联系方式

如有问题，请提交 Issue 或联系项目维护者。

## 更新日志

2025年12月18日
* Chester: 创建 `constants/` 文件夹统一数据结构定义；删除 `SalarySlab` 表，薪资档次由代码逻辑定义。
* Chester: 实习薪资独立计算（元/天）；`CrawlerTask` 自动遍历所有招聘类型。

2025年12月17日
* Chester: 新增HTML标签清理；`requirements` 字段扩容至5000字符。
* Chester: 支持新JSON格式（`jobTitle`/`city`/`extraInfo`/顶层公司字段）。

2025年12月16日
* Chester: 日志统一为 `qDebug()` 英文标签；清理过时文件，明确模块边界。
* Chester: 规范数据映射：城市按名称自增，公司从 `identity[]` 获取，标签取 `title` 去重；修复时间戳溢出。

2025年12月14日
* 程序员A: 实现网络通讯部分编写与单元测试。
* Chester: 完成数据库单元测试与项目整合。

2025年12月13日
* Chester: 完成数据库部分初步编写。
* 程序员A: 调整请求头；引入随机时间戳；实现JSON自动解析与映射。




## 任务指南 FaIL
1. 第一步
queryAllJobs()

=====  QVector 等价于 高级数组
-Job1 {}
-Job2 {}
-Job3 {}
-Job4 {}
....
=====


2. 第二步
=====  QVector 等价于 高级数组
-Job1 {}
-Job2 {}
-Job3 {}
-Job4 {}
....
===== 初始

↓

==== QVector 
       ====== QVector (包含20条)
       -Job1 {}
       -Job2 {}
       -Job3 {}
       -Job4 {}
       ....
       =====
       ====== QVector (包含20条)
       -Job21 {}
       -Job22 {}
       -Job23 {}
       -Job24 {}
       ....
       =====
==== 分页后

3. 第三步
打印上面的内容