from fastapi import FastAPI, UploadFile, File, Depends, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from typing import Dict, Any
import uvicorn
import sys
import os

# 添加当前web目录到Python路径
current_dir = os.path.dirname(os.path.abspath(__file__))
ai_dir = os.path.dirname(current_dir)
sys.path.insert(0, current_dir)  # 添加web目录
sys.path.insert(0, ai_dir)       # 添加ai目录

# 从同目录下的controller.py导入
from controller import MiniProgramController
from config.auth import AuthManager

app = FastAPI(title="AI助手API")
auth_manager = AuthManager()

# CORS配置
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 初始化控制器
controller = MiniProgramController()

async def verify_token(request: Request):
    """验证令牌"""
    token = request.query_params.get("token") or request.headers.get("Authorization", "").replace("Bearer ", "")
    if not token:
        raise HTTPException(status_code=401, detail="未提供访问令牌")
    
    try:
        payload = auth_manager.verify_token(token)
        return payload
    except ValueError as e:
        raise HTTPException(status_code=401, detail=str(e))

@app.post("/api/chat")
async def chat(request: Dict[str, str]):
    return await controller.chat(request)

# C++ GUI专用接口 - 不需要认证
@app.post("/chat")
async def simple_chat(request: Dict[str, str]):
    """简化的聊天接口，专为C++ GUI设计"""
    return await controller.chat(request)

@app.get("/ping")
async def ping():
    """心跳检测接口"""
    return {"status": "pong", "timestamp": "2025-12-26T10:00:00Z"}

@app.post("/update_knowledge")
async def update_knowledge():
    """更新知识库接口"""
    try:
        result = await controller.update_knowledge()
        return result
    except Exception as e:
        return {
            "success": False,
            "message": f"知识库更新失败: {str(e)}"
        }

@app.post("/receive_database_data")
async def receive_database_data(request: Dict[str, Any]):
    """接收C++客户端数据库数据接口，包含保护机制
    当JSON格式不正确时，直接将传输的json内容转化为string并向量化投喂给AI存进知识库
    """
    try:
        result = await controller.receive_database_data(request)
        return result
    except Exception as e:
        return {
            "success": False,
            "message": f"处理数据库数据失败: {str(e)}",
            "processed_as_raw": False
        }

@app.post("/process_single_data")
async def process_single_data(request: Dict[str, Any]):
    """处理单个数据项接口，用于实时逐个数据处理流程
    C++端发送单个数据项，Python处理后返回结果，通知C++端显示
    """
    try:
        result = await controller.process_single_data_item(request)
        return result
    except Exception as e:
        return {
            "success": False,
            "message": f"处理单个数据项失败: {str(e)}",
            "processed_item": request.get("data_item", {}),
            "error": str(e)
        }

@app.get("/health")
async def health_check():
    """健康检查接口"""
    return await controller.health_check()

@app.post("/api/developer/chat")
async def developer_chat(request: Dict[str, str], user: Dict = Depends(verify_token)):
    if not auth_manager.has_permission(user.get("role", "user"), "chat"):
        raise HTTPException(status_code=403, detail="权限不足")
    return await controller.developer_chat(request)

@app.get("/api/push-messages")
async def get_push_messages():
    return await controller.get_push_messages()

@app.post("/api/developer/feed-document")
async def feed_document(file: UploadFile = File(...), user: Dict = Depends(verify_token)):
    if not auth_manager.has_permission(user.get("role", "user"), "feed_document"):
        raise HTTPException(status_code=403, detail="权限不足")
    return await controller.feed_document(file)

@app.post("/api/developer/send-push")
async def send_push(request: Dict[str, str], user: Dict = Depends(verify_token)):
    if not auth_manager.has_permission(user.get("role", "user"), "send_push"):
        raise HTTPException(status_code=403, detail="权限不足")
    return await controller.send_push(request)

@app.post("/api/developer/send-video")
async def send_video(request: Dict[str, str], user: Dict = Depends(verify_token)):
    if not auth_manager.has_permission(user.get("role", "user"), "send_video"):
        raise HTTPException(status_code=403, detail="权限不足")
    return await controller.send_video(request)

@app.get("/api/health")
async def api_health_check():
    return await controller.health_check()



@app.post("/api/auth/login")
async def login(request: Dict[str, str]):
    """登录接口"""
    username = request.get("username")
    password = request.get("password")
    
    # 简单的演示认证
    if username == "developer" and password == "developer123":
        access_token = auth_manager.create_access_token(data={"sub": username, "role": "developer"})
        return {"access_token": access_token, "token_type": "bearer"}
    elif username == "user" and password == "user123":
        access_token = auth_manager.create_access_token(data={"sub": username, "role": "user"})
        return {"access_token": access_token, "token_type": "bearer"}
    
    raise HTTPException(status_code=401, detail="用户名或密码错误")
