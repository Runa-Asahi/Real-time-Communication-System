#pragma once
#include "friend/FriendshipFanLX.h"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// FriendDirectoryFanLX：从原领域聚合头文件拆出，保留已验证的业务接口。
class FriendDirectoryFanLX {
    friend class SnapshotCodecFanLX;
    friend class PreparedCommitFanLX;
    // 邻接表：账号键 -> 与其互为好友的账号键集合（无向图，双向登记）
    std::unordered_map<std::string, std::unordered_set<std::string>> adjacencyFanLX;
    // 关系细节表：key 为 "owner\npeer"（detailKeyFanLX），value 存备注/标签
    std::unordered_map<std::string, FriendshipFanLX> detailsFanLX;
    // 由有序对 (owner, peer) 生成细节表键（用换行符分隔，避免与账号键冲突）
    static std::string detailKeyFanLX(const std::string &owner, const std::string &peer) {
        return owner + "\n" + peer;
    }

  public:
    // 查询：owner 与 peer 之间是否存在好友关系。
    bool contains(const std::string &owner, const std::string &peer) const {
        auto it = adjacencyFanLX.find(owner);
        return it != adjacencyFanLX.end() && it->second.count(peer) != 0;
    }
    // 添加好友：校验账号有效性后双向登记邻接，并写两条方向的细节记录。
    // 关系已存在时返回 false（幂等，不抛异常）。
    bool add(const std::string &left, const std::string &right) {
        if (left.empty() || right.empty() || left == right)
            throw std::invalid_argument("好友账号无效");
        if (contains(left, right))
            return false;
        adjacencyFanLX[left].insert(right);
        adjacencyFanLX[right].insert(left);                                // 无向图：双向写入
        detailsFanLX[detailKeyFanLX(left, right)] = {left, right, {}, {}}; // 默认无备注/标签
        detailsFanLX[detailKeyFanLX(right, left)] = {right, left, {}, {}};
        return true;
    }
    // 删除好友：双向删除邻接与细节记录；关系不存在时返回 false。
    bool remove(const std::string &left, const std::string &right) {
        if (!contains(left, right))
            return false;
        adjacencyFanLX[left].erase(right);
        adjacencyFanLX[right].erase(left);
        detailsFanLX.erase(detailKeyFanLX(left, right));
        detailsFanLX.erase(detailKeyFanLX(right, left));
        return true;
    }
    // 更新备注/标签：只允许"主人"更新自己视角下的细节（对方视角的记录不受影响）。
    void update(const std::string &owner, const std::string &peer, std::string remark, std::string tag) {
        if (!contains(owner, peer))
            throw std::out_of_range("好友关系不存在");
        auto &detail = detailsFanLX.at(detailKeyFanLX(owner, peer));
        detail.remark = std::move(remark);
        detail.tag = std::move(tag);
    }
    // 查询：取"owner 视角下"与 peer 的关系细节（不存在会抛出 out_of_range）。
    FriendshipFanLX detail(const std::string &owner, const std::string &peer) const {
        return detailsFanLX.at(detailKeyFanLX(owner, peer));
    }
    // 查询：列出某账号的全部好友账号键，按字典序返回（保证输出稳定可测）。
    std::vector<std::string> peers(const std::string &owner) const {
        std::vector<std::string> result;
        auto it = adjacencyFanLX.find(owner);
        if (it != adjacencyFanLX.end())
            result.assign(it->second.begin(), it->second.end());
        std::sort(result.begin(), result.end());
        return result;
    }
    // 关系对数量：邻接表是双向登记的，因此全部邻接边数除以 2 就是关系对数。
    std::size_t pairCount() const {
        std::size_t edges = 0;
        for (const auto &item : adjacencyFanLX)
            edges += item.second.size();
        return edges / 2;
    }
    // 清空目录：删除全部好友关系与细节。
    void clear() {
        adjacencyFanLX.clear();
        detailsFanLX.clear();
    }
};
