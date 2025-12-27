import os
import sys
import threading
import uvicorn

# 添加当前目录到Python路径，支持直接运行脚本
current_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, current_dir)

import tkinter as tk
from gui.login_frame import LoginSelectionFrame

# 导入app模块
from web.app import app as fastapi_app

def start_server():
    """启动FastAPI服务器"""
    uvicorn.run(fastapi_app, host="0.0.0.0", port=8000)

def main():
    # 启动FastAPI服务器线程
    server_thread = threading.Thread(target=start_server, daemon=True)
    server_thread.start()
    
    # 启动GUI应用
    root = tk.Tk()
    app = LoginSelectionFrame(root)
    root.mainloop()

if __name__ == "__main__":
    main()