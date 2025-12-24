阶段一：自动更新cookie

之后cookie获取均在task内驱动。

0. 这部分期待在task里进行串联
1. 页面的cookie获取的全过程要包含home页面→list页面的过程
2. 自动获取到的cookie应该更新到对应的config.json中
3. 当爬取失败时，自动执行“获取cookie”逻辑

阶段二：完成批量爬取。

注意 此阶段尽量复用已有方法的名称

1. 像原来代码体现的那样，实现多页爬取。crawlAndStoreMultiPage。注意每页间留3秒的等待时间。这部分期待在task里进行串联。

2. 得到新的集成测试写入test文件夹。main部分驱动新的集成测试。