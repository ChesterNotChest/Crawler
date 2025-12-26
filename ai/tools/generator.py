import os
import requests
import uuid
from typing import Dict, Any, Optional
from config.settings import Settings
from .logger import logger

class ContentGenerator:
    def __init__(self):
        self.settings = Settings()
        self.output_dir = "output/generated_content"
        self.ollama_url = self.settings.OLLAMA_BASE_URL
        
        # 确保输出目录存在
        os.makedirs(self.output_dir, exist_ok=True)
        
        logger.info("内容生成器初始化成功")
    
    def generate_image(self, prompt: str, style: str = "realistic", size: str = "512x512") -> Dict[str, Any]:
        """生成图像"""
        try:
            logger.info(f"开始生成图像，提示词: {prompt}, 风格: {style}, 尺寸: {size}")
            
            # 这里应该调用Stable Diffusion或其他文生图模型
            # 暂时使用占位符实现
            
            # 生成唯一文件名
            filename = f"image_{uuid.uuid4()}.png"
            file_path = os.path.join(self.output_dir, filename)
            
            # 模拟生成图像（实际应调用AI模型）
            # 这里只是创建一个占位文件
            with open(file_path, 'w') as f:
                f.write(f"# 生成的图像占位符\n提示词: {prompt}\n风格: {style}\n尺寸: {size}")
            
            logger.info(f"图像生成成功，文件路径: {file_path}")
            
            return {
                "success": True,
                "file_path": file_path,
                "filename": filename,
                "prompt": prompt,
                "style": style,
                "size": size
            }
            
        except Exception as e:
            logger.error(f"生成图像失败: {e}", exc_info=True)
            return {
                "success": False,
                "error": str(e)
            }
    
    def generate_video(self, prompt: str, duration: int = 5, resolution: str = "720p") -> Dict[str, Any]:
        """生成视频"""
        try:
            logger.info(f"开始生成视频，提示词: {prompt}, 时长: {duration}秒, 分辨率: {resolution}")
            
            # 这里应该调用文生视频模型
            # 暂时使用占位符实现
            
            # 生成唯一文件名
            filename = f"video_{uuid.uuid4()}.mp4"
            file_path = os.path.join(self.output_dir, filename)
            
            # 模拟生成视频（实际应调用AI模型）
            with open(file_path, 'w') as f:
                f.write(f"# 生成的视频占位符\n提示词: {prompt}\n时长: {duration}秒\n分辨率: {resolution}")
            
            logger.info(f"视频生成成功，文件路径: {file_path}")
            
            return {
                "success": True,
                "file_path": file_path,
                "filename": filename,
                "prompt": prompt,
                "duration": duration,
                "resolution": resolution
            }
            
        except Exception as e:
            logger.error(f"生成视频失败: {e}", exc_info=True)
            return {
                "success": False,
                "error": str(e)
            }
    
    def generate_prompts(self, theme: str, count: int = 3) -> Dict[str, Any]:
        """生成文生图/文生视频的提示词"""
        try:
            logger.info(f"开始生成提示词，主题: {theme}, 数量: {count}")
            
            # 调用Ollama生成高质量提示词
            prompt = f"""请为文旅相关内容生成{count}个用于AI图像/视频生成的高质量提示词，主题是：{theme}。

每个提示词应包含：
1. 的特色元素（如古建筑、风景、民俗等）
2. 具体的场景描述
3. 艺术风格
4. 视觉效果要求

请以JSON格式返回，例如：
{
  "prompts": [
    "[提示词1]",
    "[提示词2]",
    "[提示词3]"
  ]
}
"""
            
            response = requests.post(
                f"{self.ollama_url}/api/generate",
                json={
                    "model": self.settings.MODEL_NAME,
                    "prompt": prompt,
                    "stream": False
                },
                timeout=300
            )
            
            if response.status_code == 200:
                result = response.json()
                prompts = eval(result.get("response", "{\"prompts\": []}")).get("prompts", [])
                logger.info(f"提示词生成成功，共 {len(prompts)} 个提示词")
                return {
                    "success": True,
                    "prompts": prompts
                }
            else:
                logger.warning(f"调用Ollama API生成提示词失败: {response.status_code}")
                return {
                    "success": False,
                    "error": f"API调用失败: {response.status_code}"
                }
                
        except Exception as e:
            logger.error(f"生成提示词失败: {e}", exc_info=True)
            return {
                "success": False,
                "error": str(e)
            }
    
    def get_generated_content_list(self) -> Dict[str, Any]:
        """获取生成的内容列表"""
        try:
            logger.info("获取生成的内容列表")
            content_list = []
            for filename in os.listdir(self.output_dir):
                file_path = os.path.join(self.output_dir, filename)
                if os.path.isfile(file_path):
                    content_list.append({
                        "filename": filename,
                        "file_path": file_path,
                        "size": os.path.getsize(file_path),
                        "type": "image" if filename.endswith(".png") else "video" if filename.endswith(".mp4") else "other"
                    })
            
            logger.info(f"获取到 {len(content_list)} 个生成的内容")
            return {
                "success": True,
                "content_list": content_list
            }
            
        except Exception as e:
            logger.error(f"获取生成的内容列表失败: {e}", exc_info=True)
            return {
                "success": False,
                "error": str(e)
            }
