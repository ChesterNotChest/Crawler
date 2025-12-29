# AI 模块（AI Module）

## 概述
- 目的：将自然语言处理、向量化、检索与推理能力集成到系统中，用于职位语义搜索、对话接口和辅助解析/去噪。
- 位置示例：`ai/brain.py`, `ai/main.py`, `ai/config/`。

## 职责
- 文本预处理（清洗、标准化、语言检测）。
- 将职位描述与候选句子向量化并保存到向量存储。
- 提供语义检索接口（top-k 检索、相似度阈值过滤）。
- 提供对话/问答能力（可选接入外部 LLM）。

## 接口
- `embed(text)` — 返回向量表示。
- `index(jobId, vector)` — 将向量与 job 关联并保存到向量存储。
- `semanticSearch(query, k)` — 返回最相似的 job 列表。
- `chat(query, context)` — 基于历史对话与检索结果生成回答。

## 配置与依赖
- 向量模型（本地/远端）：可选 `sentence-transformers`、OpenAI embeddings 或自研模型。
- 向量存储：简单实现可用文件或 SQLite blob；生产建议使用 Milvus、Faiss 或兼容的服务。
- 模型配置（`ai/config/settings.py`）：模型类型、向量维度、批量大小、缓存策略。

## 运行与更新流程
- 新 job 入库后触发向量化任务：`onJobParsed` -> `ai.embed` -> `index`。
- 支持批量重建索引（offline rebuild）与增量更新。

# AI 模块（AI Module） — ASCII 流程图

流程图（ASCII）：

    Start
      |
      v
    onJobParsed -> Preprocess text
      |
      v
    embed(text) -> get vector
      |
      v
    store vector -> update index
      |
      v
    semanticSearch / chat -> return results
      |
      v
     End


## 变更记录
- v1.0 — 初始说明（2025-12-29）。
