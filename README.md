# Crawler 项目

一个基于Qt 6.9的高效网页爬虫工具，支持并行任务处理、数据库存储和SQL查询。

## 📋 项目特性

- **Qt 6 框架** - 使用Qt 6.9进行GUI开发
- **HTTP 请求** - 基于libcurl库实现网络爬取
- **JSON 处理** - 使用nlohmann-json进行数据解析
- **数据库支持** - SQLite数据库集成，支持任务和结果存储
- **并行处理** - 多任务并行爬取，提高效率
- **跨平台** - 支持Windows、Linux、macOS等平台

## 🗂️ 项目结构

```
Crawler/
├── main.cpp                 # 程序入口
├── mainwindow.cpp/.h/.ui    # 主窗口UI
├── CMakeLists.txt           # CMake构建配置
├── network/                 # 网络爬虫核心模块
│   ├── job_crawler.cpp/.h   # 爬虫主类
│   ├── job_crawler_network.cpp
│   ├── job_crawler_parser.cpp
│   ├── job_crawler_printer.cpp
│   └── job_crawler_utils.cpp
├── db/                      # 数据库模块
│   ├── sqlinterface.cpp/.h  # SQL接口
│   └── sqltask.cpp/.h       # SQL任务
├── tasks/                   # 任务管理
│   ├── sqltask.cpp/.h
├── test/                    # 测试模块
│   ├── test_job_crawler.cpp
│   ├── test_sql.cpp
│   └── test.h
└── include/                 # 第三方库 (需要自行补充)
    ├── curl-8.17.0_5-win64-mingw/
    └── nlohmann-json-develop/
```

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

## 🚀 快速开始

### 环境要求
- Qt 6.9 或更高版本
- CMake 3.20+
- MinGW 64-bit 编译器
- Windows 平台

### 构建步骤

1. **克隆项目**
```bash
git clone <repository-url>
cd Crawler
```

2. **创建构建目录**
```bash
mkdir build
cd build
```

3. **使用CMake配置**
```bash
cmake ..
```

4. **编译项目**
```bash
cmake --build . --config Debug
```

5. **运行程序**
```bash
./Crawler.exe
```

## 📚 模块说明

### 网络爬虫模块 (network/)
- **job_crawler.h/cpp** - 主爬虫类，管理爬虫任务
- **job_crawler_network.cpp** - 网络请求实现
- **job_crawler_parser.cpp** - 响应数据解析
- **job_crawler_printer.cpp** - 数据输出处理
- **job_crawler_utils.cpp** - 工具函数

### 数据库模块 (db/)
- **sqlinterface.h/cpp** - SQL执行接口
- 支持SQLite数据库操作
- 提供任务和结果存储

### 任务模块 (tasks/)
- **sqltask.h/cpp** - SQL任务定义
- 支持任务队列管理
- 异步任务执行

## 🧪 测试

项目包含测试代码在 `test/` 目录：
- `test_job_crawler.cpp` - 爬虫功能测试
- `test_sql.cpp` - 数据库操作测试

运行测试：
```bash
cmake --build . --target test
```

## 🔧 配置

### CMakeLists.txt 主要配置
- Qt 6 组件支持 (Core, Gui, Widgets, Sql)
- 包含路径配置
- libcurl 和 nlohmann-json 集成

## 📝 使用示例

### 创建爬虫任务
```cpp
#include "network/job_crawler.h"

// 创建爬虫实例
JobCrawler crawler;

// 设置目标URL
crawler.setUrl("https://example.com");

// 启动爬取
crawler.start();
```

### 数据库操作
```cpp
#include "db/sqlinterface.h"

SqlInterface db;
db.connect("crawler.db");

// 执行查询
QSqlQuery result = db.query("SELECT * FROM tasks");
```

## 📄 许可证

MIT License

## 👥 贡献

欢迎提交 Issue 和 Pull Request！

## 📧 联系方式

如有问题，请提交 Issue 或联系项目维护者。
