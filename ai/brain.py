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
            
            # 从向量存储检索相关知识
            relevant_knowledge = self.vector_store.search(user_input, max_results=5)
            
            # 构建提示词
            prompt = self._build_prompt(user_input, relevant_knowledge, user_id)
            
            # 调用Ollama API
            response = self._call_ollama(prompt)
            
            # 记录AI回复
            self._add_to_history(f"AI: {response}", user_id)
            
            # 智能判断是否添加到知识库
            if self._is_worth_adding(user_input, response):
                self.add_to_knowledge_base(user_input, "user_question")
                self.add_to_knowledge_base(response, "ai_response")
            
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
    
    def _build_prompt(self, user_input: str, knowledge: List[str], user_id: str) -> str:
        """构建提示词"""
        try:
            persona = TextStore.PERSONA
            history = self._get_recent_history(user_id)
            knowledge_text = "\n".join(knowledge) if knowledge else ""
            
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

历史对话:
{history}

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
            return prompt
        except Exception as e:
            logger.error(f"构建提示词失败: {e}", exc_info=True)
            raise
    
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