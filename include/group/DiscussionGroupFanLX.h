#pragma once
#include "group/GroupFanLX.h"
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

// DiscussionGroupFanLX：从原领域聚合头文件拆出，保留已验证的业务接口。
class DiscussionGroupFanLX {
    friend class SnapshotCodecFanLX;
    friend class PreparedCommitFanLX;
    std::string idFanLX;
    std::unordered_set<std::string> membersFanLX;
    std::string ownerFanLX;
    bool archivedFanLX = false;

  public:
    explicit DiscussionGroupFanLX(std::string id) : idFanLX(std::move(id)) {}
    const std::string &id() const noexcept {
        return idFanLX;
    }
    bool addMember(const GroupFanLX &parent, const std::string &id) {
        if (!parent.hasMember(id))
            return false;
        const bool added = membersFanLX.insert(id).second;
        if (added)
            archivedFanLX = false;
        if (ownerFanLX.empty())
            ownerFanLX = id;
        return added;
    }
    bool hasMember(const std::string &id) const {
        return membersFanLX.count(id) != 0;
    }
    bool removeMember(const std::string &id) {
        const bool removed = membersFanLX.erase(id) != 0;
        if (ownerFanLX == id) {
            ownerFanLX.clear();
            if (!membersFanLX.empty())
                ownerFanLX = *std::min_element(membersFanLX.begin(), membersFanLX.end());
        }
        if (removed && membersFanLX.empty())
            archivedFanLX = true;
        return removed;
    }
    std::vector<std::string> members() const {
        std::vector<std::string> out(membersFanLX.begin(), membersFanLX.end());
        std::sort(out.begin(), out.end());
        return out;
    }
    bool archived() const noexcept {
        return archivedFanLX;
    }
    void archive() noexcept {
        archivedFanLX = true;
    }
    const std::string &owner() const noexcept {
        return ownerFanLX;
    }
    void preferOwner(const std::string &id) {
        if (hasMember(id))
            ownerFanLX = id;
    }
};
