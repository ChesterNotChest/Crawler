import os
import sys
from typing import Dict, Any, List
from fastapi import UploadFile, HTTPException
import aiofiles

# 添加当前目录到Python路径
current_dir = os.path.dirname(os.path.abspath(__file__))
ai_dir = os.path.dirname(current_dir)
sys.path.insert(0, ai_dir)

from brain import Brain
from tools.push_manager import PushMessageManager
from tools.generator import ContentGenerator

class MiniProgramController:
    def __init__(self):
        self.brain = Brain()
        self.push_manager = PushMessageManager()
        self.generator = ContentGenerator()
    
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
        """生成图像"""
        prompt = request.get("prompt", "")
        style = request.get("style", "realistic")
        size = request.get("size", "512x512")
        
        if not prompt:
            return {"success": False, "message": "提示词不能为空"}
        
        return self.generator.generate_image(prompt, style, size)
    
    async def generate_video(self, request: Dict[str, str]) -> Dict[str, Any]:
        """生成视频"""
        prompt = request.get("prompt", "")
        duration = int(request.get("duration", "5"))
        resolution = request.get("resolution", "720p")
        
        if not prompt:
            return {"success": False, "message": "提示词不能为空"}
        
        return self.generator.generate_video(prompt, duration, resolution)
    
    async def generate_prompts(self, request: Dict[str, str]) -> Dict[str, Any]:
        """生成提示词"""
        theme = request.get("theme", "")
        count = int(request.get("count", "3"))
        
        if not theme:
            return {"success": False, "message": "主题不能为空"}
        
        return self.generator.generate_prompts(theme, count)
    
    async def get_generated_content(self) -> Dict[str, Any]:
        """获取生成的内容列表"""
        return self.generator.get_generated_content_list()
    
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