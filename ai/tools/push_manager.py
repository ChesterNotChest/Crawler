import threading
from typing import List, Callable, Any
from abc import ABC, abstractmethod
from datetime import datetime

class Observer(ABC):
    @abstractmethod
    def update(self, observable, arg: Any):
        pass

class Observable:
    def __init__(self):
        self._observers = []
        self._changed = False
        self._lock = threading.RLock()
    
    def add_observer(self, observer: Observer):
        with self._lock:
            if observer not in self._observers:
                self._observers.append(observer)
    
    def delete_observer(self, observer: Observer):
        with self._lock:
            if observer in self._observers:
                self._observers.remove(observer)
    
    def notify_observers(self, arg=None):
        observers = []
        with self._lock:
            if not self._changed:
                return
            observers = self._observers[:]
            self.clear_changed()
        
        for observer in observers:
            observer.update(self, arg)
    
    def delete_observers(self):
        with self._lock:
            self._observers.clear()
    
    def set_changed(self):
        self._changed = True
    
    def clear_changed(self):
        self._changed = False
    
    def has_changed(self):
        return self._changed
    
    def count_observers(self):
        return len(self._observers)

class PushMessageManager(Observable):
    _instance = None
    _lock = threading.Lock()
    
    def __new__(cls):
        with cls._lock:
            if cls._instance is None:
                cls._instance = super().__new__(cls)
                cls._instance._initialized = False
            return cls._instance
    
    def __init__(self):
        if not self._initialized:
            super().__init__()
            self.push_messages = []
            self.video_messages = []
            self._initialized = True
    
    def add_push_message(self, message: str):
        """添加文案推送"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        formatted_message = f"[{timestamp}] {message}"
        self.push_messages.append(formatted_message)
        self.set_changed()
        self.notify_observers("push")
    
    def add_video_message(self, message: str):
        """添加视频推送"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        formatted_message = f"[{timestamp}] {message}"
        self.video_messages.append(formatted_message)
        self.set_changed()
        self.notify_observers("video")
    
    def get_push_messages(self) -> List[str]:
        """获取文案推送列表"""
        return self.push_messages.copy()
    
    def get_video_messages(self) -> List[str]:
        """获取视频推送列表"""
        return self.video_messages.copy()
    
    def clear_all_messages(self):
        """清空所有消息"""
        self.push_messages.clear()
        self.video_messages.clear()
        self.set_changed()
        self.notify_observers("clear")
    
    def get_message_stats(self) -> dict:
        """获取消息统计"""
        return {
            "push_count": len(self.push_messages),
            "video_count": len(self.video_messages),
            "total_count": len(self.push_messages) + len(self.video_messages)
        }