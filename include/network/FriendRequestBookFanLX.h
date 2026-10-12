#pragma once
#include "friend/FriendRequestFanLX.h"
#include "network/JsonStoreFanLX.h"
#include <map>
#include <string>
#include <tuple>
#include <vector>

// FriendRequestBookFanLX：服务端的好友申请簿（持久化到 friend-requests.json）。
//
// 为什么放在服务端：申请方与被申请方是两台客户端、各自一份领域库，
// 只有服务端能把"A 发出的申请"可靠地交给 B，并在 B 离线期间保存下来（服务端中转）。
//
// 只记录"申请"这件事本身；申请被同意后真正的好友关系由调用方写进账号库，
// 本类不直接修改好友目录（单一职责）。
class FriendRequestBookFanLX : public JsonStoreFanLX {
  public:
    // submit 的结果：新建 / 刷新了附言 / 对方早已向我申请过（双方互相申请 → 直接视为同意）。
    enum class SubmitOutcomeFanLX { Created, Refreshed, MutualAccept };

    explicit FriendRequestBookFanLX(std::filesystem::path file);

    // 发起申请（from → to）。调用方负责先确认双方账号存在、尚不是好友。
    // 返回 MutualAccept 时，反向的那条申请已被标记为 Accepted，调用方应建立好友关系。
    SubmitOutcomeFanLX submit(const std::string &service, const std::string &from, const std::string &to,
                              const std::string &note, const std::string &nowUtc);
    // 被申请方回应（同意 / 拒绝）：只能回应"发给自己的、待处理的"申请，否则抛异常。
    void respond(const std::string &service, const std::string &from, const std::string &to, bool accept,
                 const std::string &nowUtc);
    // 发起方撤回自己发出的、待处理的申请；不存在则抛异常。
    void cancel(const std::string &service, const std::string &from, const std::string &to,
                const std::string &nowUtc);
    // 是否存在 from → to 的待处理申请。
    bool hasPending(const std::string &service, const std::string &from, const std::string &to) const;
    // 发给 user 的待处理申请（按申请时间排序）。
    std::vector<FriendRequestFanLX> incoming(const std::string &service, const std::string &user) const;
    // user 发出的待处理申请。
    std::vector<FriendRequestFanLX> outgoing(const std::string &service, const std::string &user) const;
    // 两人之间所有仍处于待处理状态的申请一律标记为已同意（成为好友后调用）。
    void resolvePair(const std::string &service, const std::string &a, const std::string &b,
                     const std::string &nowUtc);

  protected:
    std::string schemaFanLX() const override { return "fanlx-friend-requests-1"; }
    nlohmann::json toJsonFanLX() const override;
    void fromJsonFanLX(const nlohmann::json &value) override;
    void clearFanLX() override { requestsFanLX.clear(); }

  private:
    using KeyFanLX = std::tuple<std::string, std::string, std::string>; // (服务, 发起方, 接收方)
    FriendRequestFanLX *findPendingFanLX(const std::string &service, const std::string &from,
                                         const std::string &to);
    std::map<KeyFanLX, FriendRequestFanLX> requestsFanLX;
};
