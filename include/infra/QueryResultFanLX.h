#pragma once
#include <string>
#include <vector>
// 不保存领域对象、内部指针或会话，只保存查询值副本。
struct QueryResultFanLX {
    std::vector<std::string> values;
};
