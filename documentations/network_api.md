# 项目网络通讯 API 文档（简洁版）

目的：描述本项目各组件之间的网络/HTTP 接口与约定，便于联调与扩展。

## 一、外部服务

- Ollama (本地/远端 LLM 服务)
  - Base URL: 配置于 `config/settings.py` → `OLLAMA_BASE_URL`
  - 健康检查: GET `{base}/` → 200 表示可达
  - 列表模型: GET `{base}/api/tags` → 返回 models 列表（`brain.list_available_models()` 使用）
  - 生成接口: POST `{base}/api/generate`
    - 请求体（JSON）示例: { "model": "<name>", "prompt": "...", "stream": false, "temperature": 0.7 }
    - 响应: JSON，主要字段 `response` 包含生成文本（brain 使用 `response.json().get("response")`）
    - 超时：代码中设置请求 timeout=300s

## 二、爬虫相关（示例：Chinahr）

- Chinahr 搜索 API
  - URL: POST `https://www.chinahr.com/newchr/open/job/search`
  - 请求体示例: { "page": <int>, "pageSize": <int>, "localId": "<id>", "minSalary": null, "maxSalary": null, "keyWord": "" }
  - 响应: JSON，主数据路径 `data.jobItems[]`，每条含 `jobId`, `jobName`, `comName`, `salary`, `workPlace` 等
  - 使用点: `network/crawl_chinahr.cpp` 构造 POST（`buildChinahrPostData`）并通过 `fetch_job_data()` 发起请求

- Chinahr 详情页
  - URL: GET `https://www.chinahr.com/detail/{jobId}`
  - 返回 HTML，用于解析职位描述与更准确的地区/要求字段（`fetch_text_page()`）

- 其它来源（liepin/nowcode/zhipin/wuyi/boss 等）
  - 实现方式：优先通过 WebView2 拦截（`WebResourceRequested`）捕获页面发出的 API 请求；或直接模拟 HTTP 请求（若接口明确可用）。
  - 捕获内容建议保存：完整请求 URL、headers、body、响应 body、Set-Cookie/Cookie

## 三、内部网络/模块接口（项目级约定）

- fetch_job_data(url, headers, post_data)
  - 用途：爬虫模块的统一 HTTP POST 获取并解析 JSON 返回（见 `crawl_chinahr`）
  - 返回：可选的 JSON/parsed object（或 null 表示失败）

- fetch_text_page(url)
  - 用途：GET 获取详情页 HTML 字符串
  - 备注：内部使用 libcurl，超时/重试策略有限（默认 timeout 20s）

- C++ ↔ Python（Brain） 数据传输
  - 约定：Python `Brain._fetch_latest_job_data()` 不直接读 DB 文件，期望通过网络接口或消息（例如 `receive_database_data`）将最新职位数据推送给 Python
  - 推荐契约（JSON 列表）：每条职位对象包含字段：
    - `id`, `title`, `company_id`, `company_name`, `city_id`, `city_name`, `source_id`, `source_name`, `salary_min`, `salary_max`, `salary_slab_id`, `requirements`, `create_time`, `update_time`, `tags`
  - 传输方式：可选 HTTP POST 到 Python 服务、或通过本地消息队列/Unix socket（需在项目中定义具体实现）

## 四、AI-相关调用约定（Brain 模块）

- Query 扩展（LLM）
  - Brain 在 `get_ai_response()` 中可选调用 `_llm_expand_query()`（同样通过 Ollama）获得若干检索变体，用于提高召回

- 检索接口（向量库）
  - `VectorStore` 需要实现：
    - `initialize()`
    - `search(query, max_results=20)` → 返回列表，每项含 `text`, `metadata`（包含 `id` / `source` 等）, `score`
    - `add_document(text, source)` → 插入文档
    - `get_documents_metadata()` → 返回已存文档的元数据列表
    - `delete_document(doc_id)` → 删除文档
    - `get_stats()` → 返回统计信息
  - 返回值示例（search）: [ { "text": "...", "metadata": {"id": "...","source":"job_..."}, "score": 0.92 }, ... ]

## 五、WebView2 捕获约定

- 使用场景：当站点采用前端 XHR/Fetch 且带复杂签名/动态 token 时，优先通过 WebView2 拦截真实浏览器请求。
- 捕获内容：原始请求/响应、headers、cookies、请求时间戳
- 存储格式：建议保存为 JSON 文件或发回后端做统一解析（字段：url, method, headers, request_body, response_body, cookies）

## 六、超时、重试与错误处理（实践要点）

- libcurl GET/POST 超时示例：`fetch_text_page()` 使用 timeout=20s
- Ollama 请求 timeout=300s；应在调用端做好超时/网络不可达的友好降级提示
- 对关键网络调用建议：指数退避 + 限制重试次数 + 日志记录（包含请求上下文）

## 七、示例：Chinahr POST 请求 / Ollama 生成请求

- Chinahr POST body:
```
{"page":1,"pageSize":20,"localId":"1","minSalary":null,"maxSalary":null,"keyWord":""}
```

- Ollama generate request (简化):
```
POST {OLLAMA_BASE_URL}/api/generate
{ "model": "<name>", "prompt": "...", "stream": false, "temperature": 0.7 }
```

## 八、建议（快速）

- 明确定义 C++ -> Python 的 "receive_database_data" HTTP contract（URL、认证、JSON schema），便于 `Brain._fetch_latest_job_data()` 对接。 
- 在 `VectorStore` 层保证返回结果包含稳定的 `metadata.id` 以便合并/去重。
- 对 WebView2 捕获的原始 payload 做统一保存，并提供用于后续 replay 的最小工具。

---
文档简洁汇总了项目内常用的网络调用与接口契约。如需我把 `receive_database_data` 的 JSON schema 与示例 API 实现草案一并生成，我可以继续补充。