import logging
import os
from datetime import datetime
from logging.handlers import RotatingFileHandler

class Logger:
    def __init__(self, name: str = "PythonAI"):
        self.logger = logging.getLogger(name)
        self.logger.setLevel(logging.DEBUG)
        
        # 确保日志目录存在
        log_dir = "logs"
        os.makedirs(log_dir, exist_ok=True)
        
        # 日志文件路径
        log_file = os.path.join(log_dir, f"python_ai_{datetime.now().strftime('%Y-%m-%d')}.log")
        
        # 文件处理器
        # Force UTF-8 encoding to avoid garbled non-ASCII text in log files
        file_handler = RotatingFileHandler(
            log_file,
            maxBytes=10 * 1024 * 1024,  # 10MB
            backupCount=5,
            encoding='utf-8'
        )
        file_handler.setLevel(logging.DEBUG)
        
        # 控制台处理器
        console_handler = logging.StreamHandler()
        console_handler.setLevel(logging.INFO)
        
        # 日志格式
        formatter = logging.Formatter(
            '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )
        file_handler.setFormatter(formatter)
        console_handler.setFormatter(formatter)
        
        # 避免重复添加处理器
        if not self.logger.hasHandlers():
            self.logger.addHandler(file_handler)
            self.logger.addHandler(console_handler)

        # Small startup message to confirm logger encoding and path
        self.logger.debug(f"Logger initialized, file={log_file}, encoding=utf-8")
    
    def debug(self, message: str):
        self.logger.debug(message)
    
    def info(self, message: str):
        self.logger.info(message)
    
    def warning(self, message: str):
        self.logger.warning(message)
    
    def error(self, message: str, exc_info: bool = False):
        self.logger.error(message, exc_info=exc_info)
    
    def critical(self, message: str, exc_info: bool = False):
        self.logger.critical(message, exc_info=exc_info)

# 创建全局日志实例
logger = Logger()
