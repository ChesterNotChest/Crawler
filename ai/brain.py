import os
import json
import time
import threading
from datetime import datetime
from typing import List, Dict, Any
import requests

from config.settings import Settings
from tools.vector_store import VectorStore
from tools.text_store import TextStore
from tools.logger import logger

class Brain:
    def __init__(self):
        self.settings = Settings()
        self.vector_store = VectorStore()
        self._expansion_cache = {}
        self.conversation_history = {}  # {user_id: [messages]}
        self.current_token_count = {}  # {user_id: token_count}
        
        # 初始化模型连接
        self.ollama_url = self.settings.OLLAMA_BASE_URL
        self.model_name = self.settings.MODEL_NAME
        
        logger.info("Brain初始化开始")
        self._initialize()
    
    def _initialize(self):
        """初始化大脑"""
        try:
            # 加载对话历史
            self._load_conversation_history()
            # 加载向量存储
            self.vector_store.initialize()
            
            logger.info("Brain初始化成功")
        except Exception as e:
            logger.error(f"Brain初始化失败: {e}", exc_info=True)
    
    def get_ai_response(self, user_input: str, user_id: str = "default") -> str:
        """获取AI回复"""
        try:
            # 确保用户ID存在
            self._ensure_user_id(user_id)
            
            logger.info(f"用户 {user_id} 输入: {user_input}")
            
            # 记录用户输入
            self._add_to_history(f"用户: {user_input}", user_id)
            
            # 若启用 LLM 查询扩展（提高召回），先从 LLM 获取若干查询变体

            use_expansion = getattr(self.settings, 'LLM_QUERY_EXPAND_ENABLED', False)
            expansion_count = getattr(self.settings, 'QUERY_EXPANSION_COUNT', 4)

            candidate_map = {}  # key -> aggregated candidate

            queries_to_run = [user_input]
            if use_expansion:
                try:
                    expansions = self._llm_expand_query(user_input, expansion_count)
                    # prepend original query, then unique expansions
                    queries_to_run = [user_input] + [q for q in expansions if q and q.strip() and q.strip().lower() != user_input.strip().lower()]
                except Exception as e:
                    logger.warning(f"Query expansion failed: {e}")

            # For each query variant, perform recall and merge results to improve recall
            for q in queries_to_run:
                try:
                    results = self.vector_store.search(q, max_results=20)
                except Exception as e:
                    logger.warning(f"Vector search failed for query '{q}': {e}")
                    results = []

                for r in results:
                    # r is expected to be dict with 'text' and 'metadata'
                    key = None
                    # prefer metadata id if available
                    meta = r.get('metadata', {}) if isinstance(r, dict) else {}
                    if isinstance(meta, dict) and meta.get('id') is not None:
                        key = f"id:{meta.get('id')}"
                    else:
                        # fallback to snippet hash
                        txt = r.get('text') if isinstance(r, dict) else str(r)
                        key = f"txt:{hash(txt[:200])}"

                    if key not in candidate_map:
                        # initialize aggregation
                        agg = r.copy() if isinstance(r, dict) else { 'text': r }
                        agg['matched_queries'] = [q]
                        agg['occurrences'] = 1
                        candidate_map[key] = agg
                    else:
                        # merge: increase occurrences, keep max scores
                        agg = candidate_map[key]
                        agg['occurrences'] = agg.get('occurrences', 1) + 1
                        agg.setdefault('matched_queries', []).append(q)
                        # update score heuristics
                        try:
                            existing_score = float(agg.get('score', 0.0))
                            new_score = float(r.get('score', 0.0)) if isinstance(r, dict) else 0.0
                            agg['score'] = max(existing_score, new_score)
                        except Exception:
                            pass

            # Build final candidate list
            merged_candidates = list(candidate_map.values())

            # Boost by occurrences (from multiple expansions) to favor recall hits
            for c in merged_candidates:
                base = float(c.get('score', 0.0))
                occ = float(c.get('occurrences', 1))
                # combine: base score + 0.05 per extra occurrence
                c['combined_score'] = min(1.0, base + 0.05 * (occ - 1))

            # Sort by combined_score desc and trim
            merged_candidates.sort(key=lambda x: x.get('combined_score', x.get('score', 0.0)), reverse=True)
            relevant_knowledge = merged_candidates

            
            # 构建提示词
            prompt = self._build_prompt(user_input, relevant_knowledge, user_id)
            
            # 调用Ollama API
            response = self._call_ollama(prompt)
            
            # 记录AI回复
            self._add_to_history(f"AI: {response}", user_id)
            
            logger.info(f"AI回复用户 {user_id}: {response[:50]}...")
            return response
            
        except Exception as e:
            error_msg = f"获取AI回复时出错: {e}"
            logger.error(f"获取AI回复时出错: {e}", exc_info=True)
            self._add_to_history(f"系统: {error_msg}", user_id)
            return error_msg
    
    def add_to_knowledge_base(self, content: str, source: str):
        """添加知识到向量库"""
        try:
            if len(content.strip()) < 10:  # 过滤过短内容
                logger.debug(f"内容过短，不添加到知识库: {content}")
                return
                
            self.vector_store.add_document(content, source)
            logger.info(f"知识添加到知识库: {content[:50]}... (来源: {source})")
        except Exception as e:
            logger.error(f"添加知识到知识库失败: {e}", exc_info=True)
    
    def _call_ollama(self, prompt: str) -> str:
        """调用Ollama API"""
        try:
            # 检查Ollama服务是否可达
            try:
                health_check = requests.get(f"{self.ollama_url}/", timeout=5)
                if health_check.status_code != 200:
                    logger.warning(f"Ollama服务健康检查失败: {health_check.status_code}")
            except requests.ConnectionError:
                logger.warning(f"Ollama服务不可达: {self.ollama_url}")
                # 提供友好的回退响应
                return "AI服务暂时不可用，请确保Ollama服务正在运行。\n\n如需启动Ollama服务，请运行: `ollama serve`"
            
            # 根据模型类型调整参数
            payload = {
                "model": self.model_name,
                "prompt": prompt,
                "stream": False,
                "temperature": 0.7,  # 默认温度
            }
            
            # 为千问0.6B模型优化参数
            if "qwen" in self.model_name.lower() and "0.6b" in self.model_name.lower():
                payload["temperature"] = 0.5  # 降低温度以获得更一致的输出
                payload["top_p"] = 0.8  # 控制采样的多样性
                payload["num_predict"] = 512  # 限制生成的token数
            
            logger.debug(f"调用Ollama API，模型: {self.model_name}, 参数: {payload}")
            
            response = requests.post(
                f"{self.ollama_url}/api/generate",
                json=payload,
                timeout=300
            )
            
            if response.status_code == 200:
                result = response.json().get("response", "抱歉，我没有理解您的问题。")
                logger.debug(f"Ollama API调用成功")
                return result
            else:
                error_msg = f"API调用失败: {response.status_code}"
                logger.warning(f"Ollama API调用失败: {response.status_code}")
                return error_msg
                
        except Exception as e:
            logger.error(f"连接AI服务失败: {e}", exc_info=True)
            return f"连接AI服务失败: {e}"
    
    def _build_prompt(self, user_input: str, knowledge: List[Any], user_id: str) -> str:
        """构建提示词"""
        try:
            persona = TextStore.PERSONA
            history = self._get_recent_history(user_id)
            # 支持两种 knowledge 格式：
            # - List[str]
            # - List[dict] (包含 text/score/metadata)
            knowledge_text = ""
            if knowledge:
                if isinstance(knowledge, list) and isinstance(knowledge[0], dict):
                    pieces = []
                    for item in knowledge:
                        text = item.get("text") if isinstance(item, dict) else str(item)
                        score = item.get("score", 0.0) if isinstance(item, dict) else 0.0
                        src = (item.get("metadata") or {}).get("source") if isinstance(item, dict) else None
                        snippet = text[:800].replace('\n', ' ')
                        pieces.append(f"- (score:{score:.3f}) [{src if src else 'unknown'}]: {snippet}")
                    knowledge_text = "\n".join(pieces)
                else:
                    # assume list of strings
                    knowledge_text = "\n".join(ks for ks in knowledge if ks)
            
            # 如果有知识库信息，添加特殊标记和指令
            if knowledge_text:
                knowledge_instructions = """
【知识库使用说明】
以下是从知识库检索到的相关信息，请仔细阅读并从中提取详细的工作信息：

**重要字段提取指令**：
请首先从知识库中提取并重点关注以下数据库字段信息：

【必提字段信息】
1. 职位ID: 标识职位的唯一编号
2. 职位标题/职位名称: 职位的正式名称
3. 公司ID和公司名称: 招聘公司的标识和名称
4. 城市ID和工作地点: 工作所在的城市/地区信息
5. 招聘类型ID和招聘类型: 职位的招聘方式（如全职、兼职等）
6. 薪资信息: 最低薪资、最高薪资、薪资档次ID以及格式化薪资待遇
7. 职位描述和要求: 详细的工作职责和任职要求
8. 时间信息: 创建时间、更新时间（判断职位有效性的关键）


【选择性抛弃策略】
如果某条知识的信息与用户问题无关，或者内容过于模糊，请选择性地忽略该条知识，避免引入噪音。
除此外，请尽可能多地展示信息。

"""
                knowledge_text = f"{knowledge_instructions}{knowledge_text}\n\n【工作信息提取指导】\n请按以下优先级顺序使用知识库信息回答：\n1. **首先提取数据库字段**：务必从知识库中提取上述必提字段信息\n2. **结合历史对话**：参考用户的历史对话，了解用户关注的重点\n3. **综合回答**：在知识库信息基础上，结合用户的具体问题进行回答\n\n**特别要求**：\n- 对于薪资问题，必须提供最低薪资、最高薪资和格式化薪资待遇\n- 对于地点问题，必须提供城市ID和具体工作地点\n- 对于公司问题，必须提供公司ID和公司名称\n- **重要提醒**：请务必提取并显示时间信息，这是判断职位是否仍然有效的关键依据！"
            else:
                knowledge_text = "\n【注意】当前没有找到相关的知识库信息，请基于一般知识回答。"
            
            # 根据模型类型调整提示词结构
            if "qwen" in self.model_name.lower() and "0.6b" in self.model_name.lower():
                # 为千问0.6B优化的简洁提示词
                prompt = f"""{persona}

相关知识:
{knowledge_text}

问题: {user_input}

回答:"""
            else:
                # 默认提示词结构
                prompt = f"""{persona}

相关知识:
{knowledge_text}

对话历史:
{history}

用户问题: {user_input}

请根据以上信息回答:"""
            
            logger.debug(f"构建提示词完成，长度: {len(prompt)} 字符")
            # 将完整 prompt 以及相关知识字段打印到标准输出，方便测试检索效果（不使用 debug flag）
            try:
                # 同时单独打印知识字段以便快速检查
                print("===KNOWLEDGE_FIELD_START===\n")
                print(knowledge_text[:20] if knowledge_text else "<EMPTY>")
                print("\n===KNOWLEDGE_FIELD_END===\n")
            except Exception:
                # 不阻塞主流程
                pass
            return prompt
        except Exception as e:
            logger.error(f"构建提示词失败: {e}", exc_info=True)
            raise

    def _llm_expand_query(self, query: str, max_variants: int = 4) -> List[str]:
        """使用 LLM 生成查询变体（改写、同义词、扩展关键词），以提高召回率。

        返回一个字符串列表（可能少于请求的数量）。出错时返回空列表。
        """
        try:
            cache_key = (query.strip().lower(), max_variants)
            if cache_key in self._expansion_cache:
                return self._expansion_cache[cache_key]

            # Updated strict prompt: use exact, unique prefixes from the real index.json
            instruction = (
                "你的任务：仅为用户查询中‘明确可提炼’的信息生成检索友好的辅助查询变体，提升召回；绝不猜测或虚构字段值。\n"
                "请使用下面列出的、与知识库中条目严格一致的中文前缀（前缀唯一且必须精确匹配）：\n"
                "  - 职位标题 → 【职位标题】\n"
                "  - 公司 → 【公司名称】\n"
                "  - 工作地点 → 【工作地点】\n"
                "  - 招聘类型 → 【招聘类型】\n"
                "  - 薪资待遇 → 【薪资待遇】\n"
                "  - 数据来源 → 【数据来源】\n"
                "  - 职位标签 → 【职位标签】\n"
                "  - 职位描述和要求 → 【职位描述和要求】\n"
                "  - 发布时间/最后更新时间/数据提取时间 → 【发布时间】、【最后更新时间】、【数据提取时间】\n\n"
                "生成规则：\n"
                "1) 强匹配原则：仅当用户原始查询文本中明确包含某个字段的值或可以无歧义提取该值时，才生成对应的带前缀标签（例如用户写了“上海”或“在上海”，则可生成“【工作地点】: 上海”）。绝不基于常识或上下文猜测字段值。\n"
                "2) 生成的每个变体只能是两种形式之一（且仅在该值确实来自用户查询时生成）：\n"
                "   - 字段前缀形式（优先）：例如 \"【工作地点】: 上海\"、\"【公司名称】: 腾讯\"（前缀必须与上面列出的精确匹配）。\n"
                "   - 简洁检索关键词：例如 \"上海 职位\"、\"后端 Java\"（仅当这些关键词直接来源于用户查询时）。\n"
                "3) 输出格式严格要求：必须只返回一个完整的 JSON 数组文本（以 '[' 开始并以 ']' 结束），数组元素为字符串；响应中不得包含任何其他说明性文字或字符。\n"
                "4) 长度与风格：每个变体简洁（建议 ≤20 字或 ≤8 单词），避免重复与冗余。\n"
                "示例：\n  - 输入：上海有哪些职位？\n  - 合法输出： [\"【工作地点】: 上海\"]\n\n"
                "仅返回 JSON 数组，且数组元素必须来自用户查询可直接提取的信息。"
            )

            prompt = instruction + f"\n原始查询：{query}\n\n请严格返回最多 {max_variants} 个变体，且仅返回以方括号包裹的 JSON 数组。"

            raw = self._call_ollama(prompt)

            # Extract JSON array from LLM output
            start = raw.find('[')
            end = raw.rfind(']')
            variants = []
            if start != -1 and end != -1 and end > start:
                json_text = raw[start:end+1]
                try:
                    parsed = json.loads(json_text)
                    for p in parsed:
                        if isinstance(p, str):
                            variants.append(p.strip())
                except Exception:
                    # fallback: try to split lines
                    for line in raw.splitlines():
                        line = line.strip('-* \t')
                        if line:
                            variants.append(line)
            else:
                # fallback: split by newline and take top lines
                for line in raw.splitlines():
                    line = line.strip()
                    if line:
                        variants.append(line)

            variants = [v for v in variants if v and v.strip()]
            # Print parsed variants to console
            try:
                print(f"[LLM Expand] parsed variants for query '{query}': {variants}")
                print("raw response was: ", raw)
            except Exception:
                pass
            variants = variants[:max_variants]
            # cache
            self._expansion_cache[cache_key] = variants
            # keep cache small
            if len(self._expansion_cache) > 256:
                # drop oldest
                self._expansion_cache.pop(next(iter(self._expansion_cache)))
            return variants
        except Exception as e:
            logger.error(f"_llm_expand_query failed: {e}", exc_info=True)
            return []
    
    def _get_recent_history(self, user_id: str, max_items: int = 10) -> str:
        """获取最近的对话历史"""
        try:
            recent = self.conversation_history[user_id][-max_items:] if self.conversation_history.get(user_id) else []
            return "\n".join(recent)
        except Exception as e:
            logger.error(f"获取最近对话历史失败: {e}", exc_info=True)
            return ""
    
    def _add_to_history(self, message: str, user_id: str):
        """添加消息到历史记录"""
        try:
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            full_message = f"{timestamp} - {message}"
            
            if user_id not in self.conversation_history:
                self.conversation_history[user_id] = []
                
            self.conversation_history[user_id].append(full_message)
            self._trim_history(user_id)
            self._save_conversation_history()
            
            logger.debug(f"消息添加到用户 {user_id} 的对话历史")
        except Exception as e:
            logger.error(f"添加消息到历史记录失败: {e}", exc_info=True)
    
    def _trim_history(self, user_id: str):
        """修剪历史记录"""
        try:
            max_items = TextStore.MAX_HISTORY_ITEMS
            max_tokens = TextStore.MAX_TOKENS
            
            if user_id in self.conversation_history:
                # 基于条数修剪
                if len(self.conversation_history[user_id]) > max_items:
                    removed = len(self.conversation_history[user_id]) - max_items
                    self.conversation_history[user_id] = self.conversation_history[user_id][-max_items:]
                    logger.debug(f"基于条数修剪用户 {user_id} 的对话历史，删除了 {removed} 条消息")
                
                # 基于token数量修剪
                current_tokens = self._count_tokens('\n'.join(self.conversation_history[user_id]))
                if current_tokens > max_tokens:
                    # 从开头开始删除消息，直到token数在限制内
                    while self.conversation_history[user_id] and current_tokens > max_tokens:
                        removed_message = self.conversation_history[user_id].pop(0)
                        current_tokens -= self._count_tokens(removed_message)
                    
                    logger.debug(f"基于token数量修剪用户 {user_id} 的对话历史，剩余token数: {current_tokens}")
        except Exception as e:
            logger.error(f"修剪历史记录失败: {e}", exc_info=True)
    
    def _count_tokens(self, text: str) -> int:
        """估算文本的token数量"""
        # 简单的token计数实现
        # 实际应用中应使用与模型匹配的tokenizer
        return len(text.split())
    
    def _load_conversation_history(self):
        """加载对话历史"""
        try:
            if os.path.exists(TextStore.HISTORY_FILE):
                with open(TextStore.HISTORY_FILE, 'r', encoding='utf-8') as f:
                    self.conversation_history = json.load(f)
                logger.info(f"加载了对话历史，共 {len(self.conversation_history)} 个用户会话")
            else:
                logger.info("对话历史文件不存在，创建新的历史记录")
                self.conversation_history = {}
        except Exception as e:
            logger.error(f"加载历史记录失败: {e}", exc_info=True)
            self.conversation_history = {}
    
    def _save_conversation_history(self):
        """保存对话历史"""
        try:
            os.makedirs(os.path.dirname(TextStore.HISTORY_FILE), exist_ok=True)
            with open(TextStore.HISTORY_FILE, 'w', encoding='utf-8') as f:
                json.dump(self.conversation_history, f, ensure_ascii=False, indent=2)
            logger.debug("对话历史保存成功")
        except Exception as e:
            logger.error(f"保存历史记录失败: {e}", exc_info=True)
    
    def _is_worth_adding(self, user_input: str, response: str) -> bool:
        """判断是否值得添加到知识库"""
        try:
            # 简单的质量检查
            if len(response.strip()) < 20:
                return False
            if response.startswith("抱歉") or "失败" in response or "错误" in response:
                return False
            return True
        except Exception as e:
            logger.error(f"判断是否添加到知识库失败: {e}", exc_info=True)
            return False
    
    def clear_history(self, user_id: str = None):
        """清空对话历史"""
        try:
            if user_id:
                if user_id in self.conversation_history:
                    self.conversation_history[user_id].clear()
                    logger.info(f"清空了用户 {user_id} 的对话历史")
            else:
                self.conversation_history.clear()
                logger.info("清空了所有对话历史")
            
            self._save_conversation_history()
        except Exception as e:
            logger.error(f"清空对话历史失败: {e}", exc_info=True)
    
    def get_knowledge_stats(self) -> Dict[str, Any]:
        """获取知识库统计"""
        try:
            stats = self.vector_store.get_stats()
            logger.debug(f"获取知识库统计: {stats}")
            return stats
        except Exception as e:
            logger.error(f"获取知识库统计失败: {e}", exc_info=True)
            return {}
    
    def _ensure_user_id(self, user_id: str):
        """确保用户ID存在"""
        try:
            if user_id not in self.conversation_history:
                self.conversation_history[user_id] = []
            if user_id not in self.current_token_count:
                self.current_token_count[user_id] = 0
        except Exception as e:
            logger.error(f"确保用户ID存在失败: {e}", exc_info=True)
    
    def list_available_models(self) -> List[str]:
        """列出可用的模型"""
        try:
            # 调用Ollama API获取可用模型列表
            response = requests.get(f"{self.ollama_url}/api/tags")
            if response.status_code == 200:
                data = response.json()
                models = [model["name"] for model in data.get("models", [])]
                logger.info(f"获取到可用模型列表: {models}")
                return models
            else:
                logger.warning(f"获取可用模型列表失败: {response.status_code}")
                return [self.model_name]  # 返回当前使用的模型作为默认值
        except Exception as e:
            logger.error(f"获取可用模型列表失败: {e}", exc_info=True)
            return [self.model_name]
    
    def switch_model(self, model_name: str) -> bool:
        """切换使用的模型"""
        try:
            # 检查模型是否可用
            available_models = self.list_available_models()
            if model_name in available_models:
                old_model = self.model_name
                self.model_name = model_name
                logger.info(f"模型切换成功: {old_model} -> {model_name}")
                return True
            else:
                logger.warning(f"模型 {model_name} 不可用")
                return False
        except Exception as e:
            logger.error(f"切换模型失败: {e}", exc_info=True)
            return False
    
    def get_current_model(self) -> str:
        """获取当前使用的模型"""
        return self.model_name
    
    async def update_knowledge_base(self) -> Dict[str, Any]:
        """更新知识库 - 从数据库爬取最新职位信息并向量化存储"""
        try:
            logger.info("开始更新知识库...")
            
            # 调用爬虫模块获取最新的职位数据
            job_data = self._fetch_latest_job_data()
            
            # 解析职位数据
            parsed_jobs = self._parse_job_data(job_data)
            
            # 清除旧的职位相关知识
            self._clear_job_knowledge()
            
            # 将新职位信息添加到向量存储
            added_count = 0
            for job in parsed_jobs:
                job_description = self._format_job_for_knowledge(job)
                self.add_to_knowledge_base(job_description, f"job_{job.get('id', 'unknown')}")
                added_count += 1
            
            # 获取更新后的统计信息
            final_stats = self.get_knowledge_stats()
            
            logger.info(f"知识库更新完成，新增 {added_count} 个职位信息")
            return {
                "total_documents": final_stats.get("total_documents", 0),
                "new_documents": added_count,
                "model_used": final_stats.get("model_used", "unknown"),
                "update_time": datetime.now().isoformat()
            }
            
        except Exception as e:
            logger.error(f"知识库更新失败: {e}", exc_info=True)
            raise
    
    def _fetch_latest_job_data(self) -> List[Dict[str, Any]]:
        """通过HTTP接口从C++获取最新的职位数据
        注意：Python不再直接访问数据库文件，数据通过receive_database_data接口传输"""
        try:
            logger.info("从C++端获取职位数据（通过网络传输）")
            
            # 这里应该返回空数组，因为数据通过receive_database_data接口获取
            # 这个方法保留是为了兼容，但实际数据流应该通过receive_database_data接口
            logger.info("等待从C++端通过网络传输的数据")
            return []
            
        except Exception as e:
            logger.error(f"获取C++端数据失败: {e}", exc_info=True)
            return []
    
    def _parse_job_data(self, job_data: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """解析职位数据"""
        try:
            parsed_jobs = []
            recruit_type_map = {1: "校招", 2: "实习", 3: "社招"}
            
            for job in job_data:
                # 根据数据库结构体进行数据清洗和格式化
                cleaned_job = {
                    "id": job.get("id", ""),
                    "title": job.get("title", "").strip(),
                    "company_id": job.get("company_id", 0),
                    "recruit_type_id": job.get("recruit_type_id", 0),
                    "city_id": job.get("city_id", 0),
                    "source_id": job.get("source_id", 0),
                    "requirements": job.get("requirements", "").strip(),
                    "salary_min": job.get("salary_min", 0),
                    "salary_max": job.get("salary_max", 0),
                    "salary_slab_id": job.get("salary_slab_id", 0),
                    "create_time": job.get("create_time", "").strip(),
                    "update_time": job.get("update_time", "").strip(),
                    "hr_last_login_time": job.get("hr_last_login_time", "").strip(),
                    "company_name": job.get("company_name", "").strip(),
                    "city_name": job.get("city_name", "").strip(),
                    "source_name": job.get("source_name", "").strip(),
                    "tags": job.get("tags", "").strip(),
                    "recruit_type": recruit_type_map.get(job.get("recruit_type_id", 0), "未知"),
                    "salary_range": self._format_salary_range(job.get("salary_min", 0), job.get("salary_max", 0))
                }
                
                # 过滤掉无效的职位数据
                if cleaned_job["title"] and cleaned_job["company_name"]:
                    parsed_jobs.append(cleaned_job)
            
            logger.info(f"解析完成，共 {len(parsed_jobs)} 个有效职位")
            return parsed_jobs
            
        except Exception as e:
            logger.error(f"解析职位数据失败: {e}", exc_info=True)
            return []
    
    def _format_salary_range(self, min_salary: float, max_salary: float) -> str:
        """格式化薪资范围"""
        try:
            if min_salary == 0 and max_salary == 0:
                return "面议"
            elif min_salary > 0 and max_salary > 0:
                return f"{int(min_salary)}-{int(max_salary)}K"
            elif min_salary > 0:
                return f"{int(min_salary)}K以上"
            elif max_salary > 0:
                return f"最高{int(max_salary)}K"
            else:
                return "面议"
        except Exception as e:
            logger.error(f"格式化薪资范围失败: {e}", exc_info=True)
            return "面议"
    
    def _clear_job_knowledge(self):
        """清除旧的职位相关知识"""
        try:
            # 获取所有文档的元数据
            metadata_list = self.vector_store.get_documents_metadata()
            
            # 删除以"job_"开头的文档
            deleted_count = 0
            for meta in metadata_list[:]:  # 使用切片创建副本以避免修改时出错
                if meta.get("source", "").startswith("job_"):
                    doc_id = meta.get("id")
                    if self.vector_store.delete_document(doc_id):
                        deleted_count += 1
            
            logger.info(f"清除了 {deleted_count} 个旧的职位信息")
            
        except Exception as e:
            logger.error(f"清除职位知识失败: {e}", exc_info=True)
    
    def _format_job_for_knowledge(self, job: Dict[str, Any]) -> str:
        """格式化职位信息为知识库文本"""
        try:
            formatted_text = f"""
职位信息：
职位名称：{job.get('title', '')}
公司名称：{job.get('company', '')}
工作地点：{job.get('location', '')}
薪资待遇：{job.get('salary', '')}
工作经验：{job.get('experience', '')}
学历要求：{job.get('education', '')}

职位描述：
{job.get('description', '')}

任职要求：
{job.get('requirements', '')}

技能要求：
{', '.join(job.get('skills', []))}
"""
            return formatted_text.strip()
            
        except Exception as e:
            logger.error(f"格式化职位信息失败: {e}", exc_info=True)
            return f"职位：{job.get('title', '')} 在 {job.get('company', '')}"