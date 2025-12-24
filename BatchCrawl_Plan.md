# 批量爬取集成测试开发计划

# 重要
任何修改都不允许建立在已有的test部分。test不应该用于组装、牵动业务。所以现在test里的都是错误的。

## 方法释义
crawler侧唯一的作用是调动internet侧和sql侧组合工作。

internet侧负责调动网络通讯相关业务。

sql侧负责存储。


crawler_task.crawlAndStore：搜集单个页面的内涵

crawler_task.crawlAll：“启动”方法，用于完成如下循环：
（第一层循环：每个来源（第二层循环：此来源的每一页））
第一层循环：目前就是两种来源，牛客网和boss直聘。
第二层循环里应该：(读一页，存一页；每三页，更新一次cookie)
    *如果某次失败，做updateCookieBySource后重试。
    *每翻过3页，就做一次cookie获取。
    *页间自动等待3秒。
    1. 调取internet_task.fetchBySource (其实就是crawlBySource，只是改了名字)
    （牛客网：读取第一页，然后从返回内容里查看总页数，并决定要重复多少次）
    （boss直聘：一直读取，直到返回的json里zpData.hasMore为False。另外，这个来源的默认为"社招"）
    2. 调取sql_task.storeJobDataWithSource

fetchBySource：根据页码、来源两个内容，完成网络侧信息获取(其实就是crawlBySource，只是改了名字)

updateCookieBySource：指进行相关webview2_browser_wrl工作，完成最新cookie获取，写入 config.json对应位置。

## 阶段零：前期工作
- [ ] 1. 理解internet_task和sql_task里的所有方法。理解test_crawler_task_integration，并立刻弃用它。
- [ ] 2. 准备抛弃一些不再有用且未被方法释义描述的方法。

## 阶段一：修正task流程
- [ ] 1. 弃用只能爬取牛客网的方法。
- [ ] 2. crawler_task用来调用internet_task和sql_task。
- [ ] 3. 命名修正。internet侧的业务全部改叫fetch，不用crawl。
- [ ] 4. crawler_task用来调用internet_task和sql_task。
- [ ] 5. 在crawler_task里按上面的方法释义设计新crawl_all。
- [ ] 6. 大致完成循环结构书写。

## 阶段二：自动更新cookie
- [ ] 1. internet_task里要加一个updateCookieBySource的方法，它应该在得到cookie后，写入 config.json对应位置。
- [ ] 2. 完成crawlAll内更新cookie的方法调用。

## 阶段三：批量爬取与集成测试

- [ ] 1. 完成crawlAll的完整流程。
- [ ] 2. 新增集成测试用例，写入 test 文件夹，main 驱动集成测试。（在原有的集成测试上覆写即可）
- [ ] 3. 所有新代码与测试均复用原有命名风格与接口，便于维护。

---

后续将按此计划逐步实现与集成。