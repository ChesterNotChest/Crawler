import tkinter as tk
from tkinter import ttk
from gui.monitor_frame import MonitorFrame

class LoginSelectionFrame:
    def __init__(self, root):
        self.root = root
        self.root.title("AI助手 - 后端监控")
        self.root.geometry("800x600")
        
        # 设置窗口图标（如果有的话）
        # self.root.iconbitmap("icon.ico")
        
        # 创建主框架
        self.main_frame = ttk.Frame(self.root, padding="10")
        self.main_frame.pack(fill=tk.BOTH, expand=True)
        
        # 创建标题
        self.title_label = ttk.Label(self.main_frame, text="AI助手后端监控系统", font=("Arial", 24))
        self.title_label.pack(pady=50)
        
        # 创建启动按钮
        self.start_button = ttk.Button(self.main_frame, text="启动监控", command=self.start_monitoring)
        self.start_button.pack(pady=20)
        
    def start_monitoring(self):
        """启动监控窗口"""
        # 隐藏当前窗口
        self.main_frame.pack_forget()
        
        # 创建监控窗口
        self.monitor_frame = MonitorFrame(self.root)
