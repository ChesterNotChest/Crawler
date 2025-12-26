import tkinter as tk
from tkinter import ttk
import threading
import time
import requests

class MonitorFrame:
    def __init__(self, root):
        self.root = root
        self.root.title("AI助手 - 后端监控")
        self.root.geometry("1000x600")
        
        # 创建主框架
        self.main_frame = ttk.Frame(self.root, padding="10")
        self.main_frame.pack(fill=tk.BOTH, expand=True)
        
        # 创建笔记本（标签页）
        self.notebook = ttk.Notebook(self.main_frame)
        self.notebook.pack(fill=tk.BOTH, expand=True)
        
        # 创建服务器状态标签页
        self.server_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.server_frame, text="服务器状态")
        
        # 创建用户状态标签页
        self.user_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.user_frame, text="用户状态")
        
        # 创建知识库状态标签页
        self.knowledge_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.knowledge_frame, text="知识库状态")
        
        # 初始化服务器状态界面
        self.init_server_frame()
        
        # 初始化用户状态界面
        self.init_user_frame()
        
        # 初始化知识库状态界面
        self.init_knowledge_frame()
        
        # 启动监控线程
        self.running = True
        self.monitor_thread = threading.Thread(target=self.monitor_status, daemon=True)
        self.monitor_thread.start()
        
    def init_server_frame(self):
        """初始化服务器状态界面"""
        # 服务器连接状态
        server_status_frame = ttk.LabelFrame(self.server_frame, text="服务器连接状态", padding="10")
        server_status_frame.pack(fill=tk.X, padx=10, pady=10)
        
        self.server_status_label = ttk.Label(server_status_frame, text="连接状态: 未知", font=("Arial", 12))
        self.server_status_label.pack(pady=10)
        
        # 服务器信息
        server_info_frame = ttk.LabelFrame(self.server_frame, text="服务器信息", padding="10")
        server_info_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(server_info_frame, text="服务器地址: http://localhost:8001").pack(anchor=tk.W, pady=5)
        self.request_count_label = ttk.Label(server_info_frame, text="请求计数: 0")
        self.request_count_label.pack(anchor=tk.W, pady=5)
        
    def init_user_frame(self):
        """初始化用户状态界面"""
        user_list_frame = ttk.LabelFrame(self.user_frame, text="在线用户列表", padding="10")
        user_list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # 创建用户列表树
        columns = ("ip", "username", "role", "login_time")
        self.user_tree = ttk.Treeview(user_list_frame, columns=columns, show="headings")
        
        # 设置列标题
        self.user_tree.heading("ip", text="IP地址")
        self.user_tree.heading("username", text="用户名")
        self.user_tree.heading("role", text="角色")
        self.user_tree.heading("login_time", text="登录时间")
        
        # 设置列宽
        self.user_tree.column("ip", width=150)
        self.user_tree.column("username", width=150)
        self.user_tree.column("role", width=100)
        self.user_tree.column("login_time", width=200)
        
        # 添加滚动条
        scrollbar = ttk.Scrollbar(user_list_frame, orient=tk.VERTICAL, command=self.user_tree.yview)
        self.user_tree.configure(yscroll=scrollbar.set)
        
        # 布局
        self.user_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
    def init_knowledge_frame(self):
        """初始化知识库状态界面"""
        # 知识库统计信息
        stats_frame = ttk.LabelFrame(self.knowledge_frame, text="知识库统计", padding="10")
        stats_frame.pack(fill=tk.X, padx=10, pady=10)
        
        self.doc_count_label = ttk.Label(stats_frame, text="文档数量: 0")
        self.doc_count_label.pack(anchor=tk.W, pady=5)
        
        self.vector_count_label = ttk.Label(stats_frame, text="向量数量: 0")
        self.vector_count_label.pack(anchor=tk.W, pady=5)
        
        # 最近更新的文档
        recent_docs_frame = ttk.LabelFrame(self.knowledge_frame, text="最近更新的文档", padding="10")
        recent_docs_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # 创建文档列表树
        columns = ("name", "size", "update_time")
        self.doc_tree = ttk.Treeview(recent_docs_frame, columns=columns, show="headings")
        
        # 设置列标题
        self.doc_tree.heading("name", text="文档名称")
        self.doc_tree.heading("size", text="大小")
        self.doc_tree.heading("update_time", text="更新时间")
        
        # 设置列宽
        self.doc_tree.column("name", width=300)
        self.doc_tree.column("size", width=100)
        self.doc_tree.column("update_time", width=200)
        
        # 添加滚动条
        scrollbar = ttk.Scrollbar(recent_docs_frame, orient=tk.VERTICAL, command=self.doc_tree.yview)
        self.doc_tree.configure(yscroll=scrollbar.set)
        
        # 布局
        self.doc_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
    def monitor_status(self):
        """监控状态的线程函数"""
        while self.running:
            # 检查服务器连接状态
            self.check_server_status()
            
            # 更新用户状态
            self.update_user_status()
            
            # 更新知识库状态
            self.update_knowledge_status()
            
            # 每5秒更新一次
            time.sleep(5)
        
    def check_server_status(self):
        """检查服务器连接状态"""
        try:
            response = requests.get("http://localhost:8001/api/health", timeout=2)
            if response.status_code == 200:
                self.server_status_label.config(text="连接状态: 正常", foreground="green")
                # 更新请求计数
                self.request_count_label.config(text=f"请求计数: {response.json().get('request_count', 0)}")
            else:
                self.server_status_label.config(text="连接状态: 异常", foreground="red")
        except requests.exceptions.RequestException:
            self.server_status_label.config(text="连接状态: 未连接", foreground="red")
        
    def update_user_status(self):
        """更新用户状态"""
        # 这里应该从服务器获取实际的用户数据
        # 目前使用模拟数据
        users = [
            ("192.168.1.100", "developer", "开发者", "2025-12-21 10:00:00"),
            ("192.168.1.101", "user1", "普通用户", "2025-12-21 10:05:00"),
        ]
        
        # 清空现有数据
        for item in self.user_tree.get_children():
            self.user_tree.delete(item)
        
        # 添加新数据
        for user in users:
            self.user_tree.insert("", tk.END, values=user)
        
    def update_knowledge_status(self):
        """更新知识库状态"""
        # 这里应该从服务器获取实际的知识库数据
        # 目前使用模拟数据
        self.doc_count_label.config(text="文档数量: 10")
        self.vector_count_label.config(text="向量数量: 10000")
        
        # 更新最近文档列表
        docs = [
            ("旅游指南.txt", "1.5MB", "2025-12-21 09:30:00"),
            ("历史文化.docx", "2.3MB", "2025-12-21 09:15:00"),
            ("景点介绍.pdf", "5.2MB", "2025-12-21 09:00:00"),
        ]
        
        # 清空现有数据
        for item in self.doc_tree.get_children():
            self.doc_tree.delete(item)
        
        # 添加新数据
        for doc in docs:
            self.doc_tree.insert("", tk.END, values=doc)
