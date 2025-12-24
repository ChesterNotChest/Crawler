#ifndef CRAWL_CHINAGR_H
#define CRAWL_CHINAGR_H

#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>
#include "constants/network_types.h"

using json = nlohmann::json;

namespace ChinahrCrawler {

std::map<std::string, std::string> getChinahrHeaders();
std::string buildChinahrUrl();
std::string buildChinahrPostData(int page, int pageSize, const std::string &localId = "1");
std::pair<std::vector<JobInfo>, MappingData> parseChinahrResponse(const json &json_data, int pageSize);
std::pair<std::vector<JobInfo>, MappingData> crawlChinahr(int page, int pageSize, const std::string &localId = "1");

} // namespace ChinahrCrawler

#endif // CRAWL_CHINAGR_H
