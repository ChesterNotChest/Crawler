import numpy as np
from sklearn.metrics.pairwise import cosine_similarity
from sklearn.feature_extraction.text import TfidfVectorizer
from typing import List, Dict, Any, Tuple, Optional
import json
import os
import time
import re
from collections import OrderedDict
from datetime import datetime

# 设置huggingface镜像源，解决模型下载超时问题
os.environ['HF_ENDPOINT'] = 'https://hf-mirror.com'

from sentence_transformers import SentenceTransformer
from .logger import logger

class VectorStore:
    def __init__(self, index_path: str = "data/vector_index"):
        self.index_path = index_path
        self.documents = []
        self.embeddings = []
        self.metadata = []
        self.model = None
        self.model_name = "paraphrase-multilingual-MiniLM-L12-v2"
        # TF-IDF fallback
        self._tfidf_vectorizer: Optional[TfidfVectorizer] = None
        self._tfidf_matrix = None

        # Simple in-memory LRU cache for recent queries
        self._cache_max = 128
        self._query_cache: OrderedDict = OrderedDict()
        
        logger.info("向量存储初始化成功")
    
    def initialize(self):
        """初始化向量存储"""
        try:
            os.makedirs(self.index_path, exist_ok=True)
            
            # 加载嵌入模型
            try:
                self.model = SentenceTransformer(self.model_name)
                logger.info(f"嵌入模型 {self.model_name} 加载成功")
            except Exception as e:
                logger.warning(f"嵌入模型加载失败: {e}")
                # 回退到简单嵌入
                self.model = None
                logger.info("将使用简单嵌入模式")
            
            self._load_index()
            # 初始化TF-IDF矩阵（用于语义模型不可用时或作为补充）
            self._rebuild_tfidf()
            logger.info("向量存储初始化完成")
        except Exception as e:
            logger.error(f"向量存储初始化失败: {e}", exc_info=True)
    
    def add_document(self, text: str, source: str):
        """添加文档到向量存储"""
        try:
            # 生成嵌入向量
            embedding = self._generate_embedding(text)
            
            self.documents.append(text)
            self.embeddings.append(embedding)
            self.metadata.append({
                "source": source,
                "timestamp": datetime.now().isoformat(),
                "id": len(self.documents)
            })
            
            self._save_index()
            # 更新TF-IDF矩阵
            try:
                self._rebuild_tfidf()
            except Exception:
                pass
            logger.info(f"文档添加成功: {text[:50]}... (来源: {source})")
        except Exception as e:
            logger.error(f"添加文档失败: {e}", exc_info=True)
    
    def search(self, query: str, max_results: int = 20, min_score: float = 0.25) -> List[str]:
        """搜索相似文档"""
        try:
            if not self.documents:
                logger.info("向量存储为空，未找到匹配文档")
                return []

            # 查询缓存
            cache_key = (query.strip().lower(), max_results)
            if cache_key in self._query_cache:
                # move to end (most recently used)
                self._query_cache.move_to_end(cache_key)
                return self._query_cache[cache_key]

            query_embedding = self._generate_embedding(query)
            
            # 处理嵌入向量维度不一致的情况
            try:
                # 尝试直接计算相似度
                similarities = cosine_similarity([query_embedding], self.embeddings)[0]
            except ValueError as e:
                # 如果嵌入维度不一致，尝试转换为统一维度
                logger.warning(f"嵌入向量维度不一致，进行特殊处理: {e}")
                
                # 使用最小公共维度或固定维度
                target_dim = min(len(emb) for emb in self.embeddings + [query_embedding])
                
                # 截断或填充向量到目标维度
                def normalize_embedding(emb):
                    if len(emb) > target_dim:
                        return emb[:target_dim]
                    elif len(emb) < target_dim:
                        return emb + [0.0] * (target_dim - len(emb))
                    return emb
                
                normalized_query = normalize_embedding(query_embedding)
                normalized_embeddings = [normalize_embedding(emb) for emb in self.embeddings]
                
                # 再次计算相似度
                similarities = cosine_similarity([normalized_query], normalized_embeddings)[0]
            
            # 语义分数
            semantic_scores = similarities

            # 词汇/TF-IDF 余弦得分（作为补充）
            lexical_scores = np.zeros(len(self.documents))
            if self._tfidf_matrix is not None and self._tfidf_vectorizer is not None:
                try:
                    q_vec = self._tfidf_vectorizer.transform([query])
                    lexical_sim = cosine_similarity(q_vec, self._tfidf_matrix)[0]
                    lexical_scores = lexical_sim
                except Exception:
                    lexical_scores = np.zeros(len(self.documents))

            # 组合评分：优先语义，再辅以词汇，再考虑时间衰减
            combined_scores = []
            now_ts = time.time()
            for idx in range(len(self.documents)):
                sem = float(semantic_scores[idx]) if idx < len(semantic_scores) else 0.0
                lex = float(lexical_scores[idx]) if idx < len(lexical_scores) else 0.0

                # recency score (0..0.1)
                meta_ts = 0.0
                try:
                    meta_time = self.metadata[idx].get("timestamp")
                    if meta_time:
                        # try ISO format
                        meta_ts = time.mktime(datetime.fromisoformat(meta_time).timetuple())
                except Exception:
                    meta_ts = 0.0

                recency = 0.0
                if meta_ts > 0:
                    age_days = (now_ts - meta_ts) / 86400.0
                    recency = max(0.0, 1.0 - min(age_days / 365.0, 1.0)) * 0.1

                combined = 0.75 * sem + 0.15 * lex + recency
                combined_scores.append((idx, combined, sem, lex, recency))

            # 排序并筛选
            combined_scores.sort(key=lambda x: x[1], reverse=True)

            results: List[Dict[str, Any]] = []
            for idx, combined, sem, lex, rec in combined_scores:
                if combined >= min_score and len(results) < max_results:
                    results.append({
                        "text": self.documents[idx],
                        "score": float(combined),
                        "semantic_score": float(sem),
                        "lexical_score": float(lex),
                        "recency_score": float(rec),
                        "metadata": self.metadata[idx]
                    })

            # 更新查询缓存
            self._query_cache[cache_key] = results
            # 保持缓存大小
            while len(self._query_cache) > self._cache_max:
                self._query_cache.popitem(last=False)

            logger.info(f"搜索完成，找到 {len(results)} 个匹配文档")
            return results
        except Exception as e:
            logger.error(f"搜索文档失败: {e}", exc_info=True)
            return []
    
    def _generate_embedding(self, text: str) -> List[float]:
        """生成文本嵌入"""
        try:
            if self.model is not None:
                # 使用真实的嵌入模型
                # Ensure we always return a python list (not numpy) and handle single string or list
                embedding = self.model.encode(text, show_progress_bar=False)
                if hasattr(embedding, 'tolist'):
                    return embedding.tolist()
                return list(embedding)
        except Exception as e:
            logger.error(f"生成嵌入时出错: {e}")
            # 出错时回退到简单嵌入
        
        # 简单的哈希嵌入作为回退
        words = text.lower().split()
        embedding = [0.0] * 300  # 假设300维
        
        for word in words:
            hash_val = hash(word) % 1000
            embedding[hash_val % 300] += 1
            
        # 归一化
        norm = np.linalg.norm(embedding)
        if norm > 0:
            embedding = [x / norm for x in embedding]
            
        return embedding

    def _rebuild_tfidf(self):
        """构建或重建TF-IDF矩阵，用于词汇级别的相似度计算"""
        try:
            if not self.documents:
                self._tfidf_vectorizer = None
                self._tfidf_matrix = None
                return

            self._tfidf_vectorizer = TfidfVectorizer(max_features=5000, ngram_range=(1,2))
            self._tfidf_matrix = self._tfidf_vectorizer.fit_transform(self.documents)
            logger.debug("TF-IDF 矩阵构建完成")
        except Exception as e:
            logger.error(f"构建TF-IDF矩阵失败: {e}", exc_info=True)
            self._tfidf_vectorizer = None
            self._tfidf_matrix = None
    
    def _save_index(self):
        """保存索引"""
        try:
            data = {
                "documents": self.documents,
                "embeddings": self.embeddings,
                "metadata": self.metadata
            }
            
            with open(f"{self.index_path}/index.json", "w", encoding="utf-8") as f:
                json.dump(data, f, ensure_ascii=False, indent=2)
            
            logger.debug("向量索引保存成功")
        except Exception as e:
            logger.error(f"保存向量索引失败: {e}", exc_info=True)

    
    def _load_index(self):
        """加载索引"""
        try:
            with open(f"{self.index_path}/index.json", "r", encoding="utf-8") as f:
                data = json.load(f)
                self.documents = data.get("documents", [])
                self.embeddings = data.get("embeddings", [])
                self.metadata = data.get("metadata", [])
            
            logger.info(f"加载了 {len(self.documents)} 个文档到向量存储")
        except FileNotFoundError:
            logger.info("向量索引文件不存在，创建新的索引")
            self.documents = []
            self.embeddings = []
            self.metadata = []
        except Exception as e:
            logger.error(f"加载向量索引失败: {e}", exc_info=True)
            self.documents = []
            self.embeddings = []
            self.metadata = []
    
    def get_stats(self) -> Dict[str, Any]:
        """获取统计信息"""
        try:
            average_length = np.mean([len(doc) for doc in self.documents]) if self.documents else 0
            # 提取所有来源标识
            sources = [meta.get("source", "") for meta in self.metadata if "source" in meta]
            # 将numpy类型转换为标准Python类型
            stats = {
                "total_documents": len(self.documents),
                "average_length": float(average_length) if hasattr(average_length, 'tolist') else average_length,
                "model_used": self.model_name if self.model else "simple_embedding",
                "sources": sources  # 添加来源字段
            }
            logger.debug(f"获取向量存储统计信息: {stats}")
            return stats
        except Exception as e:
            logger.error(f"获取向量存储统计信息失败: {e}", exc_info=True)
            return {}
    
    def clear(self):
        """清空向量存储"""
        try:
            self.documents.clear()
            self.embeddings.clear()
            self.metadata.clear()
            self._save_index()
            logger.info("向量存储已清空")
        except Exception as e:
            logger.error(f"清空向量存储失败: {e}", exc_info=True)
        finally:
            try:
                self._rebuild_tfidf()
            except Exception:
                pass
    
    def delete_document(self, doc_id: int) -> bool:
        """根据文档ID删除文档"""
        try:
            if 0 <= doc_id < len(self.documents):
                doc = self.documents[doc_id]
                self.documents.pop(doc_id)
                self.embeddings.pop(doc_id)
                self.metadata.pop(doc_id)
                
                # 更新剩余文档的ID
                for i, meta in enumerate(self.metadata):
                    meta["id"] = i
                    
                self._save_index()
                logger.info(f"文档删除成功: ID={doc_id}, 内容={doc[:50]}...")
                return True
            logger.warning(f"文档ID {doc_id} 不存在")
            return False
        except Exception as e:
            logger.error(f"删除文档失败: {e}", exc_info=True)
            return False
        finally:
            try:
                self._rebuild_tfidf()
            except Exception:
                pass
    
    def update_document(self, doc_id: int, new_content: str) -> bool:
        """根据文档ID更新文档内容"""
        try:
            if 0 <= doc_id < len(self.documents):
                old_content = self.documents[doc_id]
                self.documents[doc_id] = new_content
                self.embeddings[doc_id] = self._generate_embedding(new_content)
                self.metadata[doc_id]["timestamp"] = datetime.now().isoformat()
                
                self._save_index()
                logger.info(f"文档更新成功: ID={doc_id}, 旧内容={old_content[:50]}..., 新内容={new_content[:50]}...")
                return True
            logger.warning(f"文档ID {doc_id} 不存在")
            return False
        except Exception as e:
            logger.error(f"更新文档失败: {e}", exc_info=True)
            return False
        finally:
            try:
                self._rebuild_tfidf()
            except Exception:
                pass
    
    def get_documents_metadata(self) -> List[Dict[str, Any]]:
        """获取所有文档的元数据"""
        try:
            metadata = self.metadata.copy()
            logger.debug(f"获取了 {len(metadata)} 个文档的元数据")
            return metadata
        except Exception as e:
            logger.error(f"获取文档元数据失败: {e}", exc_info=True)
            return []
    
    def get_documents_by_source(self, source: str) -> List[Dict[str, Any]]:
        """根据来源获取文档"""
        try:
            results = []
            for i, meta in enumerate(self.metadata):
                if meta["source"] == source:
                    results.append({
                        "id": meta["id"],
                        "content": self.documents[i],
                        "metadata": meta
                    })
            logger.info(f"根据来源 {source} 找到了 {len(results)} 个文档")
            return results
        except Exception as e:
            logger.error(f"根据来源获取文档失败: {e}", exc_info=True)
            return []