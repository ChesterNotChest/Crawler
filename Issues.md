JobCity、Company都只增加了测试用例时候的内容，但事实上集成测试也应该自动增加。
遵循的逻辑应该是：

JobCity不从json采集ID，而采集data.jobCity（是字符串）。然后，进行插入。在数据库侧，进行比对，如果字符串搜不到，就创新的ID，并存入JobCity表为新城市。然后返回此城市ID，用于最终插入此job信息。
Company从data.user.identity.companyId获得companyId，从data.user.identity.companyName获得companyName，并存入Company表
JobTag从data.pcTagInfo里进行遍历。每个tag取其data.pcTagInfo.jobInfoTagList.tag.title进行存入表JobTag，如果表内能找到此title的内容，则不存，并都返回此tag的Id，用于最终插入此job信息。