现在，我需要将parser部分的主要方法按职业分离到 crawl_nowcode.cpp 和 crawl_zhipin.cpp 里。
不保留任何回退。

1. 主要区别内容均作分离
2. 部分通用的可复用的部分：
    常量，放在constant/network_types.h 
    方法，放在network/job_crawl_utils.cpp
3. 产生的依旧应该是constant/network_types.h里定义的各类结构体

如下是它们的单个职位的json部分内容。

nowcode  (注意，有两种格式)
```json
{
    "code": 0,
    "msg": "OK",
    "data": {
        "totalCount": 282,
        "totalPage": 15,
        "currentPage": 1,
        "datas": [
            {
                "data": {
                    "id": 403492,
                    "status": 0,
                    "createTime": 1753769069000,
                    "updateTime": 1764739568000,
                    "companyId": 4579,
                    "bossUid": 105053,
                    "recruitType": 3,
                    "jobName": "游戏开发工程师（cocos）",
                    "ext": "{\"requirements\":\"1.本科及以上学历，计算机相关专业;\\n2.熟练使用C++\\n3.熟悉并灵活使用常用的设计模式，有一定的架构能力;\\n4.完整参与过1款以上上线游戏项目(手游/PC/主机均可)\\n5.三年以上工作经验。（有团队管理经验优先）\\n6.具有良好的学习能力，沟通技能，团队合作能力。\",\"jobStrength\":\"\",\"infos\":\"1.使用cocos2dx进行客户端核心模块设计和开发\\n2.参与游戏架构设计，主导模块化开发\\n3.制定并不断优化开发流程和标准\\n4.带队完成需求、设计、开发、验收\"}",
                    "jobCity": "北京",
                    "jobCityList": [
                        "北京"
                    ],
                    "jobAddress": "北京朝阳区红海无限科技有限公司金星祈园f2",
                    "careerJobId": 11003,
                    "careerJobName": null,
                    "deliverBegin": 1751954583000,
                    "deliverEnd": 4907577600000,
                    "graduationYear": "毕业不限",
                    "refreshTime": 1753769069000,
                    "latestProcessTime": 1755596599000,
                    "feedbackDays": 0,
                    "durationDays": 0,
                    "durationMonths": 0,
                    "jobOffer": 0,
                    "eduLevel": 5000,
                    "salaryType": 2,
                    "salaryMin": 25,
                    "salaryMax": 30,
                    "salaryMonth": 14,
                    "salaryShow": null,
                    "workYearType": 3,
                    "dockSource": 0,
                    "bigChance": false,
                    "projectId": 0,
                    "openConversation": 1,
                    "deliverId": 0,
                    "jobKeys": "cocos2dx,C++,游戏开发",
                    "avgProcessRate": 100,
                    "autoInviteSwitch": 0,
                    "addInternRecruit": 0,
                    "jobFlagList": [],
                    "jobFlag": 0,
                    "hotJob": false,
                    "avgProcessDay": 4,
                    "avgProcessSec": 321011,
                    "hasSigned": false,
                    "user": {
                        "id": 698817245,
                        "nickname": null,
                        "tinnyHeaderUrl": "https://static.nowcoder.com/head/header0002.png",
                        "mainHeaderUrl": "https://static.nowcoder.com/head/header0002.png",
                        "largeHeaderUrl": "https://static.nowcoder.com/head/header0002.png",
                        "loginTime": 1751857698000,
                        "status": 0,
                        "introduction": null,
                        "gender": "女",
                        "identity": [
                            {
                                "companyId": 4579,
                                "companyName": "红海无限",
                                "idenClass": "identity-hr-icon",
                                "level": 8,
                                "name": "红海无限_人力主管",
                                "jobName": "人力主管",
                                "email": null
                            }
                        ]
                    },
                    "recommendInternCompany": {
                        "creditCode": null,
                        "tagId": 4579,
                        "picUrl": "https://uploadfiles.nowcoder.com/files/20220211/127456592_1644570033596/164448084111.jpg",
                        "companyName": "红海无限",
                        "industryTagNameList": [
                            "游戏"
                        ],
                        "scaleTagName": "不需要融资",
                        "personScales": "100-499人",
                        "companyShortName": "红海无限",
                        "address": "北京",
                        "companyId": 4579,
                        "followCount": 0,
                        "projectId": 0,
                        "searchSource": 0,
                        "companyJobCount": 0,
                        "cityList": null,
                        "followed": false
                    },
                    "companyFinancing": null,
                    "industryName": null,
                    "allowDeliverCount": 0,
                    "secondJobType": null,
                    "dockSourceProjectName": null,
                    "dockSourceProjectId": null,
                    "topTag": null,
                    "greeting": null,
                    "jobExpInfo": {
                        "expId": null,
                        "expTag": null,
                        "recReason": null,
                        "traceId": "2fre3gavd5s649flye1xf",
                        "trackId": null,
                        "entityId": 30100000403492
                    },
                    "apiActSpecialActivity": null,
                    "apiSimpleBossUser": {
                        "id": 105053,
                        "userId": 698817245,
                        "avgProcessRate": 39,
                        "avgProcessDay": 2,
                        "activeTime": 1759223350000,
                        "userAppellation": "艾女士"
                    },
                    "videoIntroductionImage": null,
                    "videoIntroductionUrl": null,
                    "newestOrderScore": 70,
                    "jobBoostScore": null,
                    "adTop": false,
                    "mark": false,
                    "conversationId": 0,
                    "hot": false,
                    "showRecruitType": false,
                    "entityId": 30100000403492,
                    "extraInfo": {
                        "trackID_var": "2fre3gavd5s649flye1xf",
                        "entityID_var": "30100000403492",
                        "positionChannel_var": "牛聘",
                        "identityType_var": "",
                        "positionType_var": "社招",
                        "jobCity_var": "北京"
                    },
                    "pcTagInfo": {
                        "titleTagList": [],
                        "jobInfoTagList": [
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "北京",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "3-5年",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "本科",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "cocos2dx",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "C++",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "游戏开发",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            }
                        ],
                        "companyInfoTagList": [
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "游戏",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "100-499人",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": "",
                                    "iconDark": null,
                                    "title": "高新技术",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": {
                                    "extraInfoType": "官方榜单",
                                    "extraInfoContent": "高新技术"
                                },
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": "",
                                    "iconDark": null,
                                    "title": "员工素质高",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": {
                                    "extraInfoType": "福利榜单",
                                    "extraInfoContent": "员工素质高"
                                },
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": "",
                                    "iconDark": null,
                                    "title": "背景雄厚",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": {
                                    "extraInfoType": "福利榜单",
                                    "extraInfoContent": "背景雄厚"
                                },
                                "transParam": null
                            }
                        ]
                    },
                    "cardTitleBgUrl": "",
                    "redirectExternalUrl": null,
                    "adLabel": null,
                    "specialType": null,
                    "deliveryConfirm": {
                        "title": null,
                        "message": null,
                        "agreementLink": null,
                        "type": null,
                        "shouldShow": false
                    },
                    "resumeSyncConfirm": {
                        "title": null,
                        "message": null,
                        "agreementLink": null,
                        "type": null,
                        "shouldShow": false
                    },
                    "companyTopJob": false
                },
                "rc_type": 3010
            },
                        {
                "data": {
                    "id": 30300000145107,
                    "jobId": "7577258175608555781",
                    "createTime": null,
                    "updateTime": 1764234392000,
                    "jobTitle": "客户端开发工程师-搜索",
                    "description": "<div class=\"tw-text-size-head-pure fw-5\">岗位职责</div> <div class=\"ptb-2 pre-line\">团队介绍：字节跳动搜索团队主要负责抖音、今日头条、西瓜视频等产品的搜索工程、算法创新和架构研发工作。我们的职责是用前沿的技术去打造一个用户体验佳、信息效率高的搜索引擎产品。我们的愿景是做一款用户首选的搜索引擎，我们的使命是可以充分整合内容，高效连接人与信息。 团队主要负责抖音、今日头条、西瓜视频、问答和百科等产品的业务研发和架构研发工作。 我们使用前沿的前端、客户端和服务端技术赋能于搜索业务的快速迭代，并在技术上不断创新和突破。同时专注于大流量、高并发、低延时的搜索系统的构建，在性能优化上，追求从内存、Disk等优化到业务架构和网络协议的创新探索，在迭代效能上不断探索容器化、动态化、搭建化等方案的创新，技术氛围强，充分给同学们提供自我成长的机会。<br/><br/>1、和产品、设计团队一起配合，负责抖音搜索的客户端上的功能研发与优化；<br/>2、参与设计和打造高扩展性、高效率的搜索客户端框架；<br/>3、参与客户端基础组件及架构设计，推进研发效率；<br/>4、参与Hybrid容器搭建，插件、React Native等动态技术调研。</div> </br><div class=\"tw-text-size-head-pure fw-5\">岗位要求</div><div class=\"ptb-2 pre-line\">1、2026届获得本科及以上学历，计算机、软件工程等相关专业优先；<br/>2、热爱计算机科学和互联网技术，对移动产品有浓厚兴趣；<br/>3、扎实的数据结构和算法基础；精通至少一门编程语言，包括但不限于：Java、Objective-C、Swift、C、C++；<br/>4、关注用户体验，能够积极把技术转化到用户体验改进上；<br/>5、有优秀的逻辑分析能力，能够对业务逻辑进行合理的抽象和拆分；<br/>6、有强烈的求知欲，优秀的学习和沟通能力。</div>",
                    "salary": "薪资面议",
                    "companyId": 665,
                    "companyName": "字节跳动",
                    "companyLogo": "https://uploadfiles.nowcoder.com/files/20250321/132429596_1742539476222/%E5%AD%97%E8%8A%82.png",
                    "industry": "互联网综合 ",
                    "scale": "10000人以上",
                    "financing": "D轮及以上",
                    "platform": "feishu",
                    "city": "深圳",
                    "education": null,
                    "entityId": 30300000145107,
                    "extraInfo": {
                        "trackID_var": "2fre6ex2rh26bx2wpqpgw",
                        "entityID_var": "30300000145107",
                        "positionChannel_var": "官网职位",
                        "identityType_var": "feishu",
                        "positionType_var": "校招",
                        "jobCity_var": "深圳"
                    },
                    "router": "https://www.nowcoder.com/approuternav?commandInfo=%5B%7B%22commandKey%22%3A%22goto%22%2C%22commandValue%22%3A%7B%22path%22%3A%22nowpick-mobile-c%3A%2F%2Fjob%2FofficialJobDetail%22%2C%22needAuth%22%3A%22false%22%2C%22param%22%3A%5B%7B%22value%22%3A%227577258175608555781%22%2C%22type%22%3A%22string%22%2C%22key%22%3A%22jobId%22%7D%2C%7B%22value%22%3A%22665%22%2C%22type%22%3A%22string%22%2C%22key%22%3A%22companyId%22%7D%5D%7D%7D%5D",
                    "hasSigned": false,
                    "recruitType": 0,
                    "skills": [],
                    "appBasicInfoResourceLocation": [
                        {
                            "tagType": 1,
                            "tag": {
                                "icon": null,
                                "iconDark": null,
                                "title": "深圳",
                                "txtColor": "#888888",
                                "bgColor": "#F7F8F9",
                                "background": null,
                                "width": 0,
                                "height": 0,
                                "selectedTxtColor": null,
                                "router": null
                            },
                            "extraInfo": null,
                            "transParam": null
                        },
                        {
                            "tagType": 1,
                            "tag": {
                                "icon": null,
                                "iconDark": null,
                                "title": "直达官网",
                                "txtColor": "#888888",
                                "bgColor": "#F7F8F9",
                                "background": null,
                                "width": 0,
                                "height": 0,
                                "selectedTxtColor": null,
                                "router": null
                            },
                            "extraInfo": null,
                            "transParam": null
                        },
                        {
                            "tagType": 1,
                            "tag": {
                                "icon": null,
                                "iconDark": null,
                                "title": "投后必反馈",
                                "txtColor": "#888888",
                                "bgColor": "#F7F8F9",
                                "background": null,
                                "width": 0,
                                "height": 0,
                                "selectedTxtColor": null,
                                "router": null
                            },
                            "extraInfo": null,
                            "transParam": null
                        }
                    ],
                    "pcTagInfo": {
                        "titleTagList": [],
                        "jobInfoTagList": [
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "深圳",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "直达官网",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "投后必反馈",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "直达官网",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "牛客专属内推通道",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "简历优先筛选",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            }
                        ],
                        "companyInfoTagList": [
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "互联网综合 ",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": null,
                                    "iconDark": null,
                                    "title": "10000人以上",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": null,
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": "",
                                    "iconDark": null,
                                    "title": "校招高薪榜",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": {
                                    "extraInfoType": "运营类榜单",
                                    "extraInfoContent": "校招高薪榜"
                                },
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": "",
                                    "iconDark": null,
                                    "title": "福利待遇不错",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": {
                                    "extraInfoType": "福利榜单",
                                    "extraInfoContent": "福利待遇不错"
                                },
                                "transParam": null
                            },
                            {
                                "tagType": 1,
                                "tag": {
                                    "icon": "",
                                    "iconDark": null,
                                    "title": "待遇好",
                                    "txtColor": "#888888",
                                    "bgColor": "#F7F8F9",
                                    "background": null,
                                    "width": 0,
                                    "height": 0,
                                    "selectedTxtColor": null,
                                    "router": null
                                },
                                "extraInfo": {
                                    "extraInfoType": "福利榜单",
                                    "extraInfoContent": "待遇好"
                                },
                                "transParam": null
                            }
                        ]
                    }
                },
                "rc_type": 3030
            },
        ]
    }
}

```

zhipin
```json
{
    "code": 0,
    "message": "Success",
    "zpData": {
        "hasMore": true,
        "jobList": [
            {
                "securityId": "uhjUQhLk24w4a-P1vxNrWjeFPj0vFv8-7iVlP5qs3OpOXaXo9PaG3T_JQ4Pnvb04ZevTTNNfPcBY4vs0uW8bcM5ysa-hO49iM3i-umk2bvkoVXwBgk7IDrrgZJozE9M0iw~~",
                "bossAvatar": "https://img.bosszhipin.com/boss/avatar/avatar_9.png",
                "bossCert": 3,
                "encryptBossId": "4bc81b7bd63068ef0XRz2dW6F1FU",
                "bossName": "孙女士",
                "bossTitle": "HR",
                "goldHunter": 0,
                "bossOnline": true,
                "encryptJobId": "1ee7475d0f2f86e103Nz29q1GFdQ",
                "expectId": 0,
                "jobName": "机械臂算法专家",
                "lid": "2BcIXbzmXpK.search.1",
                "salaryDesc": "50-80K·15薪",
                "jobLabels": [
                    "经验不限",
                    "硕士",
                    "C/C++",
                    "融合感知算法",
                    "SLAM算法"
                ],
                "jobValidStatus": 1,
                "iconWord": null,
                "skills": [
                    "C/C++",
                    "融合感知算法",
                    "SLAM算法",
                    "Python"
                ],
                "jobExperience": "经验不限",
                "daysPerWeekDesc": "",
                "leastMonthDesc": "",
                "jobDegree": "硕士",
                "cityName": "上海",
                "areaDistrict": "浦东新区",
                "businessDistrict": "金桥",
                "jobType": 0,
                "proxyJob": 0,
                "proxyType": 0,
                "anonymous": 0,
                "outland": 0,
                "optimal": 0,
                "iconFlagList": [],
                "itemId": 1,
                "city": 101020100,
                "isShield": 0,
                "atsDirectPost": false,
                "gps": {
                    "longitude": 121.624098,
                    "latitude": 31.244423
                },
                "afterNameIcons": [],
                "beforeNameIcons": [],
                "encryptBrandId": "368a58109c0fe09b33N52tW5Ew~~",
                "brandName": "仙工",
                "brandLogo": "https://img.bosszhipin.com/beijin/upload/com/workfeel/20220302/7bf6f160950405e9fae4d3868c0ae17bbd7eed723f49762f5df7c9cfa57a924032aee3b8f8086faa.jpg",
                "brandStageName": "C轮",
                "brandIndustry": "计算机软件",
                "brandScaleName": "100-499人",
                "welfareList": [],
                "industry": 100021,
                "contact": false,
                "showTopPosition": false
            },
        ],
        "type": 2
    }
}
```