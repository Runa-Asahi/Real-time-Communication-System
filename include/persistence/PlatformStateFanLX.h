#pragma once
#include "account/AccountManagerFanLX.h"
#include "friend/FriendDirectorySetFanLX.h"
#include "group/GroupServiceFanLX.h"
#include <cstdint>
#include <map>
#include <string>

// 应用私有持有在线聚合体，外部只获得 const 视图。
// friends 是**按服务分区**的目录集合：QQ / 微信 / 微博 各自一份，互不共享（见
// FriendDirectorySetFanLX 的注释）。groups 的 GroupKeyFanLX 自带命名空间（QQ / 微信），
// 两种群本来就落在不同的键空间里。
struct PlatformStateFanLX {
    AccountManagerFanLX accounts;
    FriendDirectorySetFanLX friends;
    GroupServiceFanLX groups;
    std::uint64_t generation = 0, parentGeneration = 0;
    std::string committedAtUtc;
    std::map<std::string, std::uint64_t> versions;
};
