现在，我需要增加一个数据来源的爬取。这个站点不进行cookie获取。

由于几乎相同的业务已经存在，我不会给出过多的指导，需要参考已有完成。
要求：
0. 可以多次暂停，确定是否继续。
1. 该复用的复用，该分离的分离。
2. 在做parser之前向我确认一遍映射，然后再继续。
3. 不产生新的测试，在原有的上面增加一个来源即可。
4. 不做安全、小批量调整。 

步骤：
1. 完成基础请求构建。
2. 完成crawl_chinahr.cpp/.h。
3. 完成internet_task.cpp/.h 部分的if体。
4. 完成crawler_task部分的来源增加。


总工作列表：
fetch:
```
fetch("https://www.chinahr.com/newchr/open/job/search", {
  "headers": {
    "accept": "application/json, text/plain, */*",
    "accept-language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "content-type": "application/json;charset=UTF-8",
    "priority": "u=1, i",
    "sec-ch-ua": "\"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"",
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": "\"Windows\"",
    "sec-fetch-dest": "empty",
    "sec-fetch-mode": "cors",
    "sec-fetch-site": "same-origin"
  },
  "referrer": "https://www.chinahr.com/job",
  "body": "{\"page\":1,\"pageSize\":20,\"localId\":\"1\",\"minSalary\":null,\"maxSalary\":null,\"keyWord\":\"\"}",
  "method": "POST",
  "mode": "cors",
  "credentials": "include"
});
```


json:
```
{
    "code": 1,
    "data": {
        "jobItems": [
            {
                "jobId": "693780bdc03f5b09db727f4f",
                "workType": "全职",
                "jobName": "股票交易员",
                "salary": "5k-8k元/月",
                "degree": "本科",
                "workExp": "应届毕业生",
                "recruitNumber": "招3人",
                "workPlace": "天津-武清区",
                "comId": "69378023e4b022b1e5c2501d",
                "comName": "天津和顺祥合科技有限公司",
                "industry": "其他",
                "comSize": "1-49人",
                "comLogo": "https://pic1.58cdn.com.cn/nowater/newchinahr/n_v354e4f8bd86c244ecaae79e7376bd20d7.png",
                "collected": false,
                "vip": 0
            },
        ],
        "totalCount": 6816
    },
    "error": null
}
```

单个工作详情：（在parse的过程中顺带完成请求）
```
fetch("https://www.chinahr.com/detail/693780bdc03f5b09db727f4f", {
  "headers": {
    "accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7",
    "accept-language": "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "cache-control": "max-age=0",
    "priority": "u=0, i",
    "sec-ch-ua": "\"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"",
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": "\"Windows\"",
    "sec-fetch-dest": "document",
    "sec-fetch-mode": "navigate",
    "sec-fetch-site": "same-origin",
    "sec-fetch-user": "?1",
    "upgrade-insecure-requests": "1"
  },
  "referrer": "https://www.chinahr.com/job",
  "body": null,
  "method": "GET",
  "mode": "cors",
  "credentials": "include"
});
```

寻找detail-block detail-des和detail-block detail-work
```html
                        <div data-v-327cb73c data-v-6b444eb1>
                            <div class="detail-block detail-des" data-v-327cb73c>
                                <div style="font-size: 16px;font-weight:500;margin-bottom:12px;color:rgba(51, 51, 51, 1);" data-v-327cb73c>职位描述</div>
                                <div class="detail-des_lists" data-v-327cb73c>
                                    <p>
                                        现在需要的是，协助基金经理下单的岗位 <br>
                                        <br>
                                        1、按基金经理要求下单，全程听指令，并执行 <br>
                                        <br>
                                        2、熟悉交易软件，熟悉沪深两市的交易规则 <br>
                                        <br>
                                        3、1年以上交易经验，有打过涨停板者优先 <br>
                                        <br>该职位要求强烈的责任心，执行力强，自律，反应迅速
                                    </p>
                                </div>
                            </div>
                            <div class="detail-block detail-work" data-v-327cb73c>
                                <div style="font-size: 16px;font-weight:500;margin-bottom:12px;color:rgba(51, 51, 51, 1);" data-v-327cb73c>工作地点</div>
                                <div class="detail-work_address" data-v-327cb73c>
                                    <p class="workplace_name" data-v-327cb73c>天津-武清区 华德广场
        
                                    <!---->
                                    </p>
                                    <!---->
                                </div>
                            </div>
                        </div>
```