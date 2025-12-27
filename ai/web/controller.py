import os
import sys
import json
import time
import tempfile
import threading
from typing import Dict, Any, List
from fastapi import UploadFile, HTTPException
import aiofiles

# 添加当前目录到Python路径
current_dir = os.path.dirname(os.path.abspath(__file__))
ai_dir = os.path.dirname(current_dir)
sys.path.insert(0, ai_dir)

from brain import Brain
from tools.push_manager import PushMessageManager


class MiniProgramController:
    def __init__(self):
        self.brain = Brain()
        self.push_manager = PushMessageManager()
    
    async def chat(self, request: Dict[str, str]) -> Dict[str, Any]:
        """用户聊天接口"""
        message = request.get("message", "")
        user_id = request.get("user_id", "default")
        
        if not message:
            return {"success": False, "message": "消息不能为空"}
        
        try:
            ai_response = self.brain.get_ai_response(message, user_id)
            return {
                "success": True,
                "message": ai_response
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"抱歉，AI服务暂时不可用: {str(e)}"
            }
    
    async def developer_chat(self, request: Dict[str, str]) -> Dict[str, Any]:
        """开发者聊天接口"""
        return await self.chat(request)
    
    async def get_push_messages(self) -> Dict[str, Any]:
        """获取推送消息"""
        return {
            "success": True,
            "pushMessages": self.push_manager.get_push_messages(),
            "videoMessages": self.push_manager.get_video_messages()
        }
    
    async def feed_document(self, file: UploadFile) -> Dict[str, Any]:
        """文档投喂接口"""
        if not file.filename.endswith('.txt'):
            return {"success": False, "message": "只支持txt文件"}
        
        try:
            content = await file.read()
            text_content = content.decode('utf-8')
            
            # 添加到知识库
            self.brain.add_to_knowledge_base(text_content, file.filename)
            
            return {
                "success": True,
                "message": "文档投喂成功"
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"文档处理失败: {str(e)}"
            }
    
    async def send_push(self, request: Dict[str, str]) -> Dict[str, Any]:
        """发送文案推送"""
        content = request.get("content", "")
        if not content:
            return {"success": False, "message": "内容不能为空"}
        
        self.push_manager.add_push_message(content)
        return {
            "success": True,
            "message": "推送发送成功"
        }
    
    async def send_video(self, request: Dict[str, str]) -> Dict[str, Any]:
        """发送视频推送"""
        content = request.get("content", "")
        if not content:
            return {"success": False, "message": "内容不能为空"}
        
        self.push_manager.add_video_message(content)
        return {
            "success": True,
            "message": "视频推送发送成功"
        }
    
    async def generate_image(self, request: Dict[str, str]) -> Dict[str, Any]:
        """生成图像 - 功能已移除"""
        return {"success": False, "message": "图像生成功能已移除"}
    
    async def generate_video(self, request: Dict[str, str]) -> Dict[str, Any]:
        """生成视频 - 功能已移除"""
        return {"success": False, "message": "视频生成功能已移除"}
    
    async def generate_prompts(self, request: Dict[str, str]) -> Dict[str, Any]:
        """生成提示词 - 功能已移除"""
        return {"success": False, "message": "提示词生成功能已移除"}
    
    async def get_generated_content(self) -> Dict[str, Any]:
        """获取生成的内容列表 - 功能已移除"""
        return {"success": False, "message": "内容生成功能已移除"}
    
    async def list_models(self) -> Dict[str, Any]:
        """列出可用的模型"""
        try:
            models = self.brain.list_available_models()
            current_model = self.brain.get_current_model()
            return {
                "success": True,
                "models": models,
                "current_model": current_model
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }
    
    async def switch_model(self, request: Dict[str, str]) -> Dict[str, Any]:
        """切换模型"""
        model_name = request.get("model_name", "")
        if not model_name:
            return {"success": False, "message": "模型名称不能为空"}
        
        success = self.brain.switch_model(model_name)
        if success:
            return {
                "success": True,
                "message": f"模型切换成功: {model_name}",
                "current_model": self.brain.get_current_model()
            }
        else:
            return {
                "success": False,
                "message": f"模型切换失败: {model_name} 不可用"
            }
    
    async def health_check(self) -> Dict[str, Any]:
        """健康检查接口"""
        return {
            "status": "ok",
            "service": "AI助手后端服务",
            "knowledge_stats": self.brain.get_knowledge_stats(),
            "current_model": self.brain.get_current_model()
        }
    
    async def update_knowledge(self) -> Dict[str, Any]:
        """更新知识库"""
        try:
            # 调用Brain的更新知识库方法
            result = await self.brain.update_knowledge_base()
            return {
                "success": True,
                "message": "知识库更新成功",
                "stats": result
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"知识库更新失败: {str(e)}"
            }
    
    async def receive_database_data(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """接收C++客户端发送的数据库数据并添加到知识库
        添加了保护机制：当JSON格式不正确时，直接将传输的json内容转化为string并向量化投喂给ai存进知识库
        """
        try:
            # 首先尝试解析请求数据
            data = request.get("data", [])
            if not isinstance(data, list):
                # 保护机制：当数据不是数组格式时，将整个请求转换为字符串
                return await self._add_raw_data_to_knowledge_base(request, "invalid_data_format")
            
            added_count = 0
            skipped_count = 0  # 添加跳过计数字段
            errors = []
            fallback_data = []  # 用于收集无法正确解析的数据
            log_info = []  # 用于存储日志信息
            
            # 记录开始处理数据
            log_info.append(f"开始处理 {len(data)} 条职位数据")
            
            # 获取知识库中的所有来源标识，用于检测重复
            existing_sources = set()
            try:
                stats = self.brain.get_knowledge_stats()
                if "sources" in stats and isinstance(stats["sources"], list):
                    existing_sources = set(stats["sources"])
            except Exception as e:
                log_info.append(f"无法获取知识库来源信息: {str(e)}")
            
            for i, job_item in enumerate(data):
                try:
                    # 验证数据类型
                    if not isinstance(job_item, dict):
                        errors.append(f"第{i+1}项不是有效的数据对象，将作为原始数据处理")
                        fallback_data.append(job_item)
                        log_info.append(f"跳过第{i+1}项: 不是有效的数据对象")
                        continue
                    
                    # 尝试获取标准字段
                    job_id = job_item.get("jobId", "")
                    info = job_item.get("info", "")
                    
                    # 记录正在处理的职位信息
                    log_info.append(f"正在处理第 {i+1}/{len(data)} 条职位数据，ID: {job_id}")
                    
                    # 如果缺少必需字段，记录错误但尝试其他处理方式
                    if not job_id or not info:
                        errors.append(f"第{i+1}项缺少jobId或info字段，将作为原始数据处理")
                        fallback_data.append(job_item)
                        log_info.append(f"跳过第{i+1}项: 缺少jobId或info字段")
                        continue
                    
                    # 格式化职位信息为文本
                    job_text = f"职位ID: {job_id}\n职位信息: {info}"
                    source_id = f"job_{job_id}"
                    
                    # 检查职位是否已经存在于知识库中
                    if source_id in existing_sources:
                        skipped_count += 1
                        log_info.append(f"跳过职位 {job_id}: 已存在于知识库中")
                        continue
                    
                    # 只显示前50个字符，避免输出过长
                    preview = info[:50] + "..." if len(info) > 50 else info
                    log_info.append(f"职位内容预览: {preview}")
                    
                    # 添加到知识库
                    self.brain.add_to_knowledge_base(job_text, source_id)
                    added_count += 1
                    
                    # 记录成功添加，但只记录到最近10条详细记录
                    if added_count <= 10 or added_count % 10 == 0:
                        log_info.append(f"成功添加职位 {job_id} 到知识库，已添加: {added_count}/{len(data)}")
                        print(f"成功添加职位 {job_id} 到知识库，已添加: {added_count}/{len(data)}")
                    elif added_count == len(data):
                        # 记录最后一条
                        log_info.append(f"成功添加职位 {job_id} 到知识库，已添加: {added_count}/{len(data)}")
                        print(f"成功添加职位 {job_id} 到知识库，已添加: {added_count}/{len(data)}")
                    
                except Exception as e:
                    # 捕获处理单个项目时的任何异常
                    errors.append(f"处理第{i+1}项时出错: {str(e)}，将作为原始数据处理")
                    fallback_data.append(job_item)
                    log_info.append(f"处理职位 {job_item.get('jobId', f'item_{i+1}')} 时出错: {str(e)}")
            
            # 如果有需要保护机制处理的数据，将其作为原始字符串添加
            if fallback_data:
                log_info.append(f"处理解析失败的数据: {len(fallback_data)} 条")
                
                try:
                    # 将无法正确解析的数据转换为字符串
                    raw_data_text = f"原始数据内容: {json.dumps(fallback_data, ensure_ascii=False, indent=2)}"
                    self.brain.add_to_knowledge_base(raw_data_text, f"raw_data_{int(time.time())}")
                    added_count += 1
                    errors.append("已将无法解析的数据作为原始内容添加到知识库")
                except Exception as fallback_error:
                    errors.append(f"保护机制失败: {str(fallback_error)}")
            
            # 记录处理完成
            log_info.append(f"知识库更新完成，新增: {added_count} 条，跳过: {skipped_count} 条，总处理: {len(data)} 条")
            log_info.append("=" * 50)  # 添加分隔线，方便阅读
            
            # 返回处理结果
            if added_count > 0 or skipped_count > 0:
                stats = self.brain.get_knowledge_stats()
                return {
                    "success": True,
                    "message": f"成功处理 {added_count} 条数据，跳过 {skipped_count} 条已存在数据",
                    "total_processed": len(data),
                    "success_count": added_count,
                    "skipped_count": skipped_count,
                    "error_count": len(errors),
                    "errors": errors,
                    "knowledge_stats": stats,
                    "fallback_processed": len(fallback_data) if fallback_data else 0,
                    "log_info": "\n".join(log_info)  # 将日志信息作为字符串返回
                }
            else:
                # 如果没有任何成功处理的数据，尝试将整个请求作为原始数据
                if data:
                    return await self._add_raw_data_to_knowledge_base(request, "all_data_failed")
                else:
                    return {
                        "success": False,
                        "message": "没有提供有效数据",
                        "total_processed": 0,
                        "errors": errors,
                        "log_info": "\n".join(log_info)
                    }
                
        except Exception as e:
            # 最终保护机制：将整个请求作为原始数据处理
            log_info.append(f"处理请求时发生异常: {str(e)}")
            return await self._add_raw_data_to_knowledge_base(request, "exception_fallback")
    
    async def process_single_data_item(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """处理单个数据项，实现实时逐个数据处理流程
        这个接口专门用于C++端逐个发送数据进行处理
        """
        try:
            # 获取单个数据项
            job_item = request.get("data_item", {})
            if not job_item:
                return {
                    "success": False,
                    "message": "没有提供数据项",
                    "processed_item": None,
                    "log_info": "错误：请求中缺少data_item字段"
                }
            
            # 记录开始处理单个数据项
            log_info = []
            log_info.append("开始处理单个数据项")
            
            # 验证数据类型
            if not isinstance(job_item, dict):
                log_info.append(f"数据项不是有效的字典格式: {type(job_item)}")
                return await self._process_single_fallback_data(job_item, log_info, "invalid_item_format")
            
            # 尝试获取标准字段
            job_id = job_item.get("jobId", "")
            info = job_item.get("info", "")
            
            # 记录数据项信息
            log_info.append(f"数据项ID: {job_id}")
            if info:
                preview = info[:50] + "..." if len(info) > 50 else info
                log_info.append(f"内容预览: {preview}")
            else:
                log_info.append("内容为空")
            
            # 如果缺少必需字段，尝试其他处理方式
            if not job_id and not info:
                log_info.append("缺少jobId和info字段，将作为原始数据处理")
                return await self._process_single_fallback_data(job_item, log_info, "missing_required_fields")
            
            # 格式化数据项信息为文本
            if job_id and info:
                job_text = f"职位ID: {job_id}\n职位信息: {info}"
                source_id = f"job_{job_id}"
            else:
                # 如果只有部分字段，尝试组合所有信息
                job_text = f"数据项内容: {json.dumps(job_item, ensure_ascii=False, indent=2)}"
                source_id = f"item_{int(time.time())}"
            
            # 检查是否已存在于知识库中
            try:
                stats = self.brain.get_knowledge_stats()
                if "sources" in stats and isinstance(stats["sources"], list):
                    if source_id in stats["sources"]:
                        log_info.append(f"数据项已存在于知识库中，跳过处理")
                        return {
                            "success": True,
                            "message": "数据项已存在，跳过处理",
                            "processed_item": job_item,
                            "job_id": job_id,
                            "skipped": True,
                            "log_info": "\n".join(log_info)
                        }
            except Exception as e:
                log_info.append(f"无法检查知识库状态: {str(e)}")
            
            # 添加到知识库
            try:
                self.brain.add_to_knowledge_base(job_text, source_id)
                log_info.append("成功添加到知识库")
                
                # 获取更新后的统计信息
                stats = self.brain.get_knowledge_stats()
                
                return {
                    "success": True,
                    "message": "数据项处理成功",
                    "processed_item": job_item,
                    "job_id": job_id,
                    "skipped": False,
                    "knowledge_stats": stats,
                    "log_info": "\n".join(log_info)
                }
                
            except Exception as e:
                log_info.append(f"添加到知识库失败: {str(e)}")
                return {
                    "success": False,
                    "message": f"添加到知识库失败: {str(e)}",
                    "processed_item": job_item,
                    "job_id": job_id,
                    "error": str(e),
                    "log_info": "\n".join(log_info)
                }
                
        except Exception as e:
            log_info.append(f"处理单个数据项时发生异常: {str(e)}")
            return {
                "success": False,
                "message": f"处理单个数据项时发生异常: {str(e)}",
                "processed_item": job_item if 'job_item' in locals() else None,
                "error": str(e),
                "log_info": "\n".join(log_info) if 'log_info' in locals() else f"异常: {str(e)}"
            }
    
    async def _process_single_fallback_data(self, data: Any, log_info: List[str], data_type: str) -> Dict[str, Any]:
        """处理单个数据项的降级处理"""
        try:
            # 将数据转换为可读的字符串格式
            if isinstance(data, dict):
                raw_text = f"原始数据({data_type}): {json.dumps(data, ensure_ascii=False, indent=2)}"
            else:
                raw_text = f"原始数据({data_type}): {str(data)}"
            
            # 添加到知识库
            self.brain.add_to_knowledge_base(raw_text, f"raw_{data_type}_{int(time.time())}")
            log_info.append("原始数据已添加到知识库")
            
            return {
                "success": True,
                "message": "数据项已作为原始内容添加到知识库",
                "processed_item": data,
                "job_id": "",
                "skipped": False,
                "fallback_processed": True,
                "log_info": "\n".join(log_info)
            }
            
        except Exception as e:
            log_info.append(f"降级处理失败: {str(e)}")
            return {
                "success": False,
                "message": f"降级处理失败: {str(e)}",
                "processed_item": data,
                "error": str(e),
                "log_info": "\n".join(log_info)
            }
    
    async def _add_raw_data_to_knowledge_base(self, data: Any, data_type: str) -> Dict[str, Any]:
        """保护机制：将原始数据转换为字符串并添加到知识库"""
        try:
            # 将数据转换为可读的字符串格式
            if isinstance(data, dict):
                raw_text = f"原始数据({data_type}): {json.dumps(data, ensure_ascii=False, indent=2)}"
            else:
                raw_text = f"原始数据({data_type}): {str(data)}"
            
            # 添加到知识库
            self.brain.add_to_knowledge_base(raw_text, f"raw_{data_type}_{int(time.time())}")
            
            return {
                "success": True,
                "message": f"数据格式异常，已将原始内容作为文本添加到知识库",
                "processed_as_raw": True,
                "data_type": data_type,
                "stats": self.brain.get_knowledge_stats()
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"保护机制失败: {str(e)}",
                "processed_as_raw": False
            }
    
    async def get_knowledge_stats(self) -> Dict[str, Any]:
        """获取知识库统计信息"""
        try:
            stats = self.brain.get_knowledge_stats()
            return {
                "success": True,
                "stats": stats
            }
        except Exception as e:
            return {
                "success": False,
                "message": f"获取统计信息失败: {str(e)}"
            }