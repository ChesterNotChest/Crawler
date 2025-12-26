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
async def health_check():
    return await controller.health_check()

@app.post("/api/developer/generate-image")
async def generate_image(request: Dict[str, str], user: Dict = Depends(verify_token)):
    if not auth_manager.has_permission(user.get("role", "user"), "feed_document"):
        raise HTTPException(status_code=403, detail="权限不足")
    return await controller.generate_image(request)

@app.post("/api/developer/generate-video")
async def generate_video(request: Dict[str, str], user: Dict = Depends(verify_token)):
    if not auth_manager.has_permission(user.get("role", "user"), "feed_document"):
        raise HTTPException(status_code=403, detail="权限不足")
    return await controller.generate_video(request)

@app.post("/api/developer/generate-prompts")
async def generate_prompts(request: Dict[str, str], user: Dict = Depends(verify_token)):
    if not auth_manager.has_permission(user.get("role", "user"), "feed_document"):
        raise HTTPException(status_code=403, detail="权限不足")
    return await controller.generate_prompts(request)

@app.get("/api/developer/generated-content")
async def get_generated_content(user: Dict = Depends(verify_token)):
    if not auth_manager.has_permission(user.get("role", "user"), "feed_document"):
        raise HTTPException(status_code=403, detail="权限不足")
    return await controller.get_generated_content()

@app.get("/api/developer/list-models")
async def list_models(user: Dict = Depends(verify_token)):
    if not auth_manager.has_permission(user.get("role", "user"), "feed_document"):
        raise HTTPException(status_code=403, detail="权限不足")
    return await controller.list_models()

@app.post("/api/developer/switch-model")
async def switch_model(request: Dict[str, str], user: Dict = Depends(verify_token)):
    if not auth_manager.has_permission(user.get("role", "user"), "feed_document"):
        raise HTTPException(status_code=403, detail="权限不足")
    return await controller.switch_model(request)

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
