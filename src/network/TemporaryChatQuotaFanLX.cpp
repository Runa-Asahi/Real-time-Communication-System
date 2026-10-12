#include "network/TemporaryChatQuotaFanLX.h"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace {
// 每个方向保留的已计数 requestId 上限：额度只有 limit 条，多留无意义，防止文件无界增长。
constexpr std::size_t kMaxRememberedIdsFanLX = 64;
} // namespace

TemporaryChatQuotaFanLX::TemporaryChatQuotaFanLX(std::filesystem::path file, int limit)
    : JsonStoreFanLX(std::move(file)), limitFanLX(limit < 0 ? 0 : limit) {}

const TemporaryConversationFanLX *TemporaryChatQuotaFanLX::findFanLX(const std::string &service,
                                                                     const std::string &from,
                                                                     const std::string &to) const {
    const auto found = recordsFanLX.find(KeyFanLX{service, from, to});
    return found == recordsFanLX.end() ? nullptr : &found->second;
}

int TemporaryChatQuotaFanLX::sent(const std::string &service, const std::string &from,
                                  const std::string &to) const {
    const auto *record = findFanLX(service, from, to);
    return record ? record->sent : 0;
}

int TemporaryChatQuotaFanLX::remaining(const std::string &service, const std::string &from,
                                       const std::string &to) const {
    const auto *record = findFanLX(service, from, to);
    return record ? record->remaining(limitFanLX) : limitFanLX;
}

bool TemporaryChatQuotaFanLX::consumed(const std::string &service, const std::string &from,
                                       const std::string &to, const std::string &requestId) const {
    const auto *record = findFanLX(service, from, to);
    if (!record || requestId.empty())
        return false;
    const auto &ids = record->consumedRequestIds;
    return std::find(ids.begin(), ids.end(), requestId) != ids.end();
}

bool TemporaryChatQuotaFanLX::consume(const std::string &service, const std::string &from,
                                      const std::string &to, const std::string &requestId,
                                      const std::string &nowUtc) {
    if (consumed(service, from, to, requestId))
        return false;
    if (remaining(service, from, to) <= 0)
        throw std::runtime_error("临时会话已达上限（每方向 " + std::to_string(limitFanLX) +
                                 " 条）：请先发送好友申请并等待对方同意");
    auto &record = recordsFanLX[KeyFanLX{service, from, to}];
    record.service = service;
    record.from = from;
    record.to = to;
    ++record.sent;
    record.lastMessageAtUtc = nowUtc;
    if (!requestId.empty()) {
        record.consumedRequestIds.push_back(requestId);
        if (record.consumedRequestIds.size() > kMaxRememberedIdsFanLX)
            record.consumedRequestIds.erase(record.consumedRequestIds.begin());
    }
    return true;
}

bool TemporaryChatQuotaFanLX::clearPair(const std::string &service, const std::string &a, const std::string &b) {
    const auto first = recordsFanLX.erase(KeyFanLX{service, a, b});
    const auto second = recordsFanLX.erase(KeyFanLX{service, b, a});
    return first + second > 0;
}

std::vector<std::string> TemporaryChatQuotaFanLX::peersOf(const std::string &service,
                                                          const std::string &user) const {
    std::set<std::string> peers;
    for (const auto &item : recordsFanLX) {
        const auto &r = item.second;
        if (r.service != service)
            continue;
        if (r.from == user)
            peers.insert(r.to);
        else if (r.to == user)
            peers.insert(r.from);
    }
    return {peers.begin(), peers.end()};
}

nlohmann::json TemporaryChatQuotaFanLX::toJsonFanLX() const {
    nlohmann::json rows = nlohmann::json::array();
    for (const auto &item : recordsFanLX) {
        const auto &r = item.second;
        rows.push_back({{"service", r.service},
                        {"from", r.from},
                        {"to", r.to},
                        {"sent", r.sent},
                        {"requestIds", r.consumedRequestIds},
                        {"lastMessageAtUtc", r.lastMessageAtUtc}});
    }
    return {{"limit", limitFanLX}, {"records", rows}};
}

void TemporaryChatQuotaFanLX::fromJsonFanLX(const nlohmann::json &value) {
    if (!value.contains("records") || !value.at("records").is_array())
        throw std::runtime_error("临时会话额度表缺少 records 数组");
    for (const auto &row : value.at("records")) {
        TemporaryConversationFanLX r;
        r.service = row.at("service").get<std::string>();
        r.from = row.at("from").get<std::string>();
        r.to = row.at("to").get<std::string>();
        r.sent = row.value("sent", 0);
        if (row.contains("requestIds") && row.at("requestIds").is_array())
            r.consumedRequestIds = row.at("requestIds").get<std::vector<std::string>>();
        r.lastMessageAtUtc = row.value("lastMessageAtUtc", std::string());
        if (r.service.empty() || r.from.empty() || r.to.empty() || r.sent < 0)
            throw std::runtime_error("临时会话额度记录损坏");
        recordsFanLX[KeyFanLX{r.service, r.from, r.to}] = std::move(r);
    }
}
