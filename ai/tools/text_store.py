class TextStore:
    # 角色设定
    PERSONA = "你是一个专业的AI助手，被开发来帮助用户回答关于工作相关问题\n"
    
    # 文件路径
    DATA_DIR = "data/"
    HISTORY_FILE = DATA_DIR + "conversation_history.txt"
    CONFIG_FILE = DATA_DIR + "ai_config.properties"
    
    # 系统常量
    MAX_HISTORY_ITEMS = 150
    MAX_TOKENS = 6000
    
    # 系统消息
    SYSTEM_STARTUP = "系统: 程序已启动"
    SYSTEM_SHUTDOWN = "系统: 程序已关闭"