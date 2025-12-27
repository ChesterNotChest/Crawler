import os

class Settings:
    # Ollama配置
    OLLAMA_BASE_URL = os.getenv("OLLAMA_BASE_URL", "http://localhost:11434")
    MODEL_NAME = os.getenv("MODEL_NAME", "qwen2:7b")
    
    # 应用配置
    MAX_HISTORY_ITEMS = 150
    MAX_TOKENS = 6000
    VECTOR_DIMENSION = 768
    
    # 认证配置
    SECRET_KEY = os.getenv("SECRET_KEY", "default_secret_key_for_development_only")
    