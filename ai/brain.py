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