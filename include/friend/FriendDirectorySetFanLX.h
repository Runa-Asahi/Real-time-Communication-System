#pragma once
#include "account/ServiceTypeFanLX.h"
#include "friend/FriendDirectoryFanLX.h"
#include <map>

// FriendDirectorySetFanLX：按服务分别存放的好友目录集合（QQ / 微信 / 微博 各一份）。
//
// 为什么要拆成"每个服务一份"：好友关系属于**某个服务的两个账号**，不是属于"这个人"。
// 以前三种服务共用一份邻接表，虽然键里带服务名不至于串号，但存储与遍历都在同一个容器里，
// "QQ 的好友出现在微信里"这类串服务的写法只要有一处没判服务就会成立。现在：
//   - 每个服务一个独立目录，写关系时只会写进"发起方服务"那一份；
//   - 跨服务建立关系在 FriendServiceFanLX 里被直接拒绝（QQ 号只能加 QQ 号）；
//   - 落盘时也按服务分成不同小节（见 SnapshotCodecFanLX::encode 的 friends 对象）。
class FriendDirectorySetFanLX {
    friend class SnapshotCodecFanLX;
    friend class PreparedCommitFanLX;
    std::map<ServiceTypeFanLX, FriendDirectoryFanLX> directoriesFanLX;

  public:
    // 取某服务的目录；不存在时创建（引用保持稳定，各服务互不影响）。
    FriendDirectoryFanLX &directory(ServiceTypeFanLX service) { return directoriesFanLX[service]; }
    // 只读取用：没有该服务的分区时返回一个空目录，调用方无需区分"没有"和"空"。
    const FriendDirectoryFanLX &directory(ServiceTypeFanLX service) const {
        static const FriendDirectoryFanLX emptyFanLX{};
        const auto found = directoriesFanLX.find(service);
        return found == directoriesFanLX.end() ? emptyFanLX : found->second;
    }
    // 已建立分区的服务 → 该服务的好友目录（按服务枚举升序，输出稳定）。
    const std::map<ServiceTypeFanLX, FriendDirectoryFanLX> &partitions() const {
        return directoriesFanLX;
    }
    // 某服务已登记的好友关系条数（邻接表双向登记，因此按关系对计数）。
    std::size_t pairCount(ServiceTypeFanLX service) const { return directory(service).pairCount(); }
    // 清空全部分区（候选事务回滚 / 重建状态时使用）。
    void clear() { directoriesFanLX.clear(); }
};
