#include "network/FriendRequestBookFanLX.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

FriendRequestBookFanLX::FriendRequestBookFanLX(std::filesystem::path file) : JsonStoreFanLX(std::move(file)) {}

FriendRequestFanLX *FriendRequestBookFanLX::findPendingFanLX(const std::string &service, const std::string &from,
                                                             const std::string &to) {
    const auto found = requestsFanLX.find(KeyFanLX{service, from, to});
    if (found == requestsFanLX.end() || !found->second.isPending())
        return nullptr;
    return &found->second;
}

FriendRequestBookFanLX::SubmitOutcomeFanLX
FriendRequestBookFanLX::submit(const std::string &service, const std::string &from, const std::string &to,
                               const std::string &note, const std::string &nowUtc) {
    if (from.empty() || to.empty() || from == to)
        throw std::runtime_error("不能向自己发送好友申请");
    if (note.size() > 256)
        throw std::runtime_error("申请附言过长（不超过 256 字节）");
    // 对方早已向我申请过：我再发申请就等于同意，直接把那条标记为已同意。
    if (auto *reverse = findPendingFanLX(service, to, from)) {
        reverse->status = FriendRequestStatusFanLX::Accepted;
        reverse->respondedAtUtc = nowUtc;
        return SubmitOutcomeFanLX::MutualAccept;
    }
    auto &slot = requestsFanLX[KeyFanLX{service, from, to}];
    const bool refreshed = slot.isPending() && !slot.from.empty();
    slot.service = service;
    slot.from = from;
    slot.to = to;
    slot.note = note;
    slot.status = FriendRequestStatusFanLX::Pending;
    slot.requestedAtUtc = nowUtc;
    slot.respondedAtUtc.clear();
    return refreshed ? SubmitOutcomeFanLX::Refreshed : SubmitOutcomeFanLX::Created;
}

void FriendRequestBookFanLX::respond(const std::string &service, const std::string &from, const std::string &to,
                                     bool accept, const std::string &nowUtc) {
    auto *request = findPendingFanLX(service, from, to);
    if (!request)
        throw std::runtime_error("没有来自 " + from + " 的待处理好友申请");
    request->status = accept ? FriendRequestStatusFanLX::Accepted : FriendRequestStatusFanLX::Rejected;
    request->respondedAtUtc = nowUtc;
}

void FriendRequestBookFanLX::cancel(const std::string &service, const std::string &from, const std::string &to,
                                    const std::string &nowUtc) {
    auto *request = findPendingFanLX(service, from, to);
    if (!request)
        throw std::runtime_error("没有发给 " + to + " 的待处理好友申请可撤回");
    request->status = FriendRequestStatusFanLX::Cancelled;
    request->respondedAtUtc = nowUtc;
}

bool FriendRequestBookFanLX::hasPending(const std::string &service, const std::string &from,
                                        const std::string &to) const {
    const auto found = requestsFanLX.find(KeyFanLX{service, from, to});
    return found != requestsFanLX.end() && found->second.isPending();
}

namespace {
void sortByTimeFanLX(std::vector<FriendRequestFanLX> &rows) {
    std::sort(rows.begin(), rows.end(), [](const FriendRequestFanLX &a, const FriendRequestFanLX &b) {
        return a.requestedAtUtc == b.requestedAtUtc ? a.from + a.to < b.from + b.to
                                                    : a.requestedAtUtc < b.requestedAtUtc;
    });
}
} // namespace

std::vector<FriendRequestFanLX> FriendRequestBookFanLX::incoming(const std::string &service,
                                                                 const std::string &user) const {
    std::vector<FriendRequestFanLX> rows;
    for (const auto &item : requestsFanLX)
        if (item.second.isPending() && item.second.service == service && item.second.to == user)
            rows.push_back(item.second);
    sortByTimeFanLX(rows);
    return rows;
}

std::vector<FriendRequestFanLX> FriendRequestBookFanLX::outgoing(const std::string &service,
                                                                 const std::string &user) const {
    std::vector<FriendRequestFanLX> rows;
    for (const auto &item : requestsFanLX)
        if (item.second.isPending() && item.second.service == service && item.second.from == user)
            rows.push_back(item.second);
    sortByTimeFanLX(rows);
    return rows;
}

void FriendRequestBookFanLX::resolvePair(const std::string &service, const std::string &a, const std::string &b,
                                         const std::string &nowUtc) {
    for (auto *request : {findPendingFanLX(service, a, b), findPendingFanLX(service, b, a)}) {
        if (!request)
            continue;
        request->status = FriendRequestStatusFanLX::Accepted;
        request->respondedAtUtc = nowUtc;
    }
}

nlohmann::json FriendRequestBookFanLX::toJsonFanLX() const {
    nlohmann::json rows = nlohmann::json::array();
    for (const auto &item : requestsFanLX) {
        const auto &r = item.second;
        rows.push_back({{"service", r.service},
                        {"from", r.from},
                        {"to", r.to},
                        {"note", r.note},
                        {"status", friendRequestStatusNameFanLX(r.status)},
                        {"requestedAtUtc", r.requestedAtUtc},
                        {"respondedAtUtc", r.respondedAtUtc}});
    }
    return {{"requests", rows}};
}

void FriendRequestBookFanLX::fromJsonFanLX(const nlohmann::json &value) {
    if (!value.contains("requests") || !value.at("requests").is_array())
        throw std::runtime_error("好友申请簿缺少 requests 数组");
    for (const auto &row : value.at("requests")) {
        FriendRequestFanLX r;
        r.service = row.at("service").get<std::string>();
        r.from = row.at("from").get<std::string>();
        r.to = row.at("to").get<std::string>();
        r.note = row.value("note", std::string());
        r.status = friendRequestStatusFromNameFanLX(row.value("status", std::string("pending")));
        r.requestedAtUtc = row.value("requestedAtUtc", std::string());
        r.respondedAtUtc = row.value("respondedAtUtc", std::string());
        if (r.service.empty() || r.from.empty() || r.to.empty())
            throw std::runtime_error("好友申请记录缺少服务或号码");
        requestsFanLX[KeyFanLX{r.service, r.from, r.to}] = std::move(r);
    }
}
