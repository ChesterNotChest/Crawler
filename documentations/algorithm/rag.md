# RAG（Retrieval-Augmented Generation）算法设计文档

## 概述
RAG（检索增强生成）将检索模块与生成模块结合：先从文档集合中检索相关片段（context），再把这些片段作为条件（prompt/context）交给生成模型（LLM）以形成更准确、有凭据的回答。RAG 提升了知识覆盖、回答准确性与可解释性，适用于知识库问答、对话助手、文档摘要与企业内部检索场景。

## 目标与约束
- 目标：在保证业务语义与响应准确性的前提下，提供可追溯、低错误率的生成结果。\
- 约束：响应延迟需可控（< 1s — 2s 视业务可接受度）、检索结果对隐私敏感数据需可控、支持增量更新与缓存。

## 高层架构
- 文档预处理：分片（chunking）、文本清洗、元数据标注（source、时间、doc_id、段落索引）。
- 向量化 / 索引：使用 embedding 模型生成向量，保存到向量数据库（Faiss/Annoy/Pinecone/Weaviate/RedisVector 等）。
- 检索器（Retriever）：负责近似最近邻检索（ANN），并返回 top-K 候选片段及其相似度分数。
- 过滤/重排序（Reranker）：可选，用轻量模型或使用交叉编码器对候选进行精排；也可基于业务规则（时间、来源）过滤。
- 构造 Prompt：将若干条高质量片段拼接到 prompt 中（注意长度管理、去重与提示模板）。
- 生成器（Generator / LLM）：接收 prompt 输出自然语言响应，并可返回参考片段引用。
- 后处理与可解释性：将生成结果与检索到的来源映射，记录日志与证据。

## 关键设计细节

### 文本分片（Chunking）
- 目标：既要保证片段语义完整，又要兼顾 embedding 长度与检索效率。常用策略：按段落、按句子组合（512–1024 token）或滑动窗口（overlap 10–30%）。
- 元数据必须包含源文档 id、段序号与原始偏移，用于生成结果的可追溯性。

### Embedding 与索引
- Embedding 模型选择：与 LLM 兼容或通用型（OpenAI、Ada/Embedding、SentenceTransformers 等）。
- 向量维度、归一化：对相似度使用余弦或点积，建议对向量做 L2 归一化以便用内积/余弦统一度量。
- 索引与持久化：本地可选 Faiss（CPU / GPU），线上可选托管服务（Pinecone、Weaviate）。支持增量插入与删除。

### 检索策略
- Dense Retriever（向量）：对 query 做 embedding，再用 ANN 查询。通常返回 top-K（K=5..50，视片段长度与 prompt 限制）。
- Sparse Retriever（倒排、BM25）：对长文档集合或关键字匹配有价值，可与 dense 做混合检索（hybrid）。
- Hybrid：先用 BM25 过滤候选集，再用向量检索或交叉编码器精排，提高精确度并减少计算量。

### 重排序（Reranking）
- 交叉编码器（cross-encoder）成本高但精度高：对 top-N 重新打分（N 比 K 小，例如 K=50、N=10）。
- 轻量规则：基于时间戳（新优先）、来源可信度、匹配字段数量等加权。

### Prompt 构造与长度管理
- 模板包含：任务说明、检索到的证据片段（按相关度或可信度排序）、用户问题、生成要求（简明、引用来源、格式）。
- 控制 token：先按分数选取片段，再按长度从高到低/低到高截取，保证 prompt 与生成模型上下文窗口不超限。
- 避免信息泄露：当片段包含敏感字段时，应先进行屏蔽或脱敏，或在检索阶段排除敏感来源。

### 证据引用与可追溯性
- 生成结果应同时返回：生成文本、用于支撑回答的片段 id 列表、每个片段的来源与匹配分数。
- 日志：保存 query、检索 IDs、prompt（或其 hash）、生成结果、时间戳以便审计与回放。

### 缓存与性能优化
- Query-level 缓存：对相同 query 或 prompt hash 缓存生成结果。对于高吞吐场景显著提升性能。
- Segment-level 热点缓存：热点文档或片段的 embedding / 特征预先加载到内存。
- 批量 embedding：对多 query 或多文档做批量 embedding 以提高吞吐。

### 数据刷新与一致性
- 增量更新：支持插入/删除/更新片段并定期（或实时）刷新索引。
- TTL/版本控制：对时效性强的数据设置版本或时间窗口，以便老旧信息不会误导生成模型。

### 隐私与安全
- 敏感信息屏蔽：建立敏感字段规则并在索引或检索时屏蔽/脱敏。\
- 访问控制：不同用户/角色看到的检索结果需基于权限过滤。
- 输出过滤器：对生成的自然语言进行质量与政策检测（例如敏感/有害内容过滤）。

## 评估指标
- 精确性（Accuracy / F1）在有标签 QA 集上评估。\
- 覆盖率：检索系统是否返回包含正确答案的片段（Recall@K）。\
- 生成质量：人工打分或 BLEU/ROUGE（仅参考）。\
- 延迟：P95、P99 响应时延。\
- 可靠性：错误率与失败情况分析（检索无候选、生成模型超时等）。

## 端到端伪代码

1. 文档入库：clean -> chunk -> embed -> upsert(index)

2. 处理请求(query):
```
query_embedding = embed(query)
candidates = ANN_search(query_embedding, top=K)
if use_bm25:
  bm_candidates = BM25_search(query, top=M)
  candidates = hybrid_merge(candidates, bm_candidates)
candidates = rerank(candidates, query)  # optional
selected = select_by_token_budget(candidates, max_tokens)
prompt = build_prompt(template, selected, query)
answer = LLM_generate(prompt)
return {answer, evidence: selected}
```

## 集成与部署建议（针对本项目）
- Embedding：可先接入托管 embedding（OpenAI / HuggingFace）以快速验证；生产可迁移到本地 SBERT 模型以降低成本。\
- 向量库：开发阶段使用 Faiss（嵌入文件或轻量 DB），上线建议使用托管向量 DB 支持持久化和并发。\
- 可插拔检索器：实现 Retriever 抽象（Dense/Sparse/Hybrid），便于未来替换或打 A/B。\
- 可视化与监控：保存 query/response 并提供 Dashboard（查询成功率、热点 query），用于持续优化检索与 prompt。

## 示例 Prompt 模板（中文）
```
任务：你是一个基于文档的智能助手。请根据下面的证据片段回答用户问题，回答时注明所依据的片段编号。如果无法从证据中回答，请说“无法从已检索到的文档中确定答案”。

证据：
[1] {doc1 text}
[2] {doc2 text}
...

问题：{user_query}

回答：
```

## 迁移/扩展计划
- 第 1 阶段：实现最小可用 RAG（dense retriever + simple prompt + LLM）。验证准确度与延迟。\
- 第 2 阶段：加入 BM25 hybrid 与 reranker，提高精确度。\
- 第 3 阶段：上线向量 DB、缓存与监控，完善隐私与访问控制。

## 参考资料
- 原始论文："Retrieval-Augmented Generation for Knowledge-Intensive NLP Tasks" (Lewis et al.)\
- 向量数据库与 ANN：Faiss, Annoy, HNSW, ScaNN\
- Embedding 模型：SentenceTransformers, OpenAI Embeddings

---
文档由工程团队模板生成，若需要我把示例代码片段改成项目中的实际 API（例如 `SQLInterface` / `vector_store` 的方法调用），我可以继续补充。
