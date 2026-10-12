#pragma once
#include "friend/TemporaryConversationFanLX.h"
#include "network/JsonStoreFanLX.h"
#include <map>
#include <string>
#include <tuple>
#include <vector>

// TemporaryChatQuotaFanLX：非好友临时会话的额度表（持久化到 temporary-chat.json）。
//
// 规则：每个方向（发送方 → 接收方）最多 limit 条（默认 10）；成为好友后调用 clearPair 清除，
// 好友之间不受限。额度在服务端判定，客户端无法绕过；服务端重启后额度不会被"重置"。
// 计数按消息 requestId 幂等：断线重发同一条消息不会重复扣额度。
class TemporaryChatQuotaFanLX : public JsonStoreFanLX {
  public:
    TemporaryChatQuotaFanLX(std::filesystem::path file, int limit);

    int limit() const noexcept { return limitFanLX; }
    int sent(const std::string &service, const std::string &from, const std::string &to) const;
    int remaining(const std::string &service, const std::string &from, const std::string &to) const;
    // 该 requestId 是否已经计过数（重放判定）。
    bool consumed(const std::string &service, const std::string &from, const std::string &to,
                  const std::string &requestId) const;
    // 计一条（幂等）；返回 true 表示本次新计数，false 表示该 requestId 已计过。额度用满时抛异常。
    bool consume(const std::string &service, const std::string &from, const std::string &to,
                 const std::string &requestId, const std::string &nowUtc);
    // 清除两人之间两个方向的记录（成为好友后调用）；返回是否有记录被删除。
    bool clearPair(const std::string &service, const std::string &a, const std::string &b);
    // user 在该服务里有临时会话往来的对端号码（任一方向有记录即算），按号码排序。
    std::vector<std::string> peersOf(const std::string &service, const std::string &user) const;

  protected:
    std::string schemaFanLX() const override { return "fanlx-temporary-chat-1"; }
    nlohmann::json toJsonFanLX() const override;
    void fromJsonFanLX(const nlohmann::json &value) override;
    void clearFanLX() override { recordsFanLX.clear(); }

  private:
    using KeyFanLX = std::tuple<std::string, std::string, std::string>; // (服务, 发送方, 接收方)
    const TemporaryConversationFanLX *findFanLX(const std::string &service, const std::string &from,
                                                const std::string &to) const;
    int limitFanLX = 10;
    std::map<KeyFanLX, TemporaryConversationFanLX> recordsFanLX;
};
