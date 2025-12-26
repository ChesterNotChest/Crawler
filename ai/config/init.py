import os
import json
from typing import Dict, Any

class Config:
    """配置管理类"""
    
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._loaded = False