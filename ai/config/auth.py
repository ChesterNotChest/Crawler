from typing import Dict, List
import jwt
from datetime import datetime, timedelta
from .settings import Settings
from tools.logger import logger

class AuthManager:
    def __init__(self):
        self.settings = Settings()
        self.secret_key = self.settings.SECRET_KEY or "default_secret_key_change_in_production"
        self.algorithm = "HS256"
        self.access_token_expire_minutes = 30
        
        # 角色定义
        self.ROLES = {
            "user": ["chat", "get_push_messages"],
            "developer": ["chat", "get_push_messages", "feed_document", "send_push", "send_video"]
        }
        
        logger.info("认证管理器初始化成功")
    
    def create_access_token(self, data: Dict[str, any]) -> str:
        """创建访问令牌"""
        try:
            to_encode = data.copy()
            expire = datetime.utcnow() + timedelta(minutes=self.access_token_expire_minutes)
            to_encode.update({"exp": expire})
            encoded_jwt = jwt.encode(to_encode, self.secret_key, algorithm=self.algorithm)
            logger.info(f"为用户 {data.get('sub')} 创建了访问令牌")
            return encoded_jwt
        except Exception as e:
            logger.error(f"创建访问令牌失败: {e}", exc_info=True)
            raise
    
    def verify_token(self, token: str) -> Dict[str, any]:
        """验证令牌"""
        try:
            payload = jwt.decode(token, self.secret_key, algorithms=[self.algorithm])
            logger.info(f"验证令牌成功，用户: {payload.get('sub')}")
            return payload
        except jwt.ExpiredSignatureError as e:
            logger.warning(f"令牌已过期: {e}")
            raise ValueError("令牌已过期") from e
        except jwt.InvalidTokenError as e:
            logger.warning(f"无效的令牌: {e}")
            raise ValueError("无效的令牌") from e
    
    def has_permission(self, role: str, action: str) -> bool:
        """检查角色是否有权限执行操作"""
        if role not in self.ROLES:
            logger.warning(f"未定义的角色: {role}")
            return False
        
        has_perm = action in self.ROLES[role]
        if not has_perm:
            logger.warning(f"角色 {role} 没有执行 {action} 的权限")
        
        return has_perm
