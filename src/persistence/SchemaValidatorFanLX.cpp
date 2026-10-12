#include "persistence/SchemaValidatorFanLX.h"
#include "persistence/PersistenceErrorFanLX.h"
#include <cctype>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace {
void requireFanLX(bool condition, const char *message) {
    if (!condition)
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::InvalidData, message);
}
void idFanLX(const std::string &s) {
    requireFanLX(!s.empty() && s.size() <= 128, "ID 长度错误");
    for (unsigned char c : s)
        requireFanLX(c >= 32 && c != 127 && c != '|' && c != '\n', "ID 含非法分隔符");
}
bool dateFanLX(const std::string &s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-')
        return false;
    for (std::size_t i = 0; i < s.size(); ++i)
        if (i != 4 && i != 7 && !std::isdigit(static_cast<unsigned char>(s[i])))
            return false;
    int y = std::stoi(s.substr(0, 4)), m = std::stoi(s.substr(5, 2)), d = std::stoi(s.substr(8, 2));
    if (y < 1 || m < 1 || m > 12)
        return false;
    const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int max = days[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    return d >= 1 && d <= max;
}
void timeFanLX(const std::string &s) {
    requireFanLX(s.size() == 20 && dateFanLX(s.substr(0, 10)) && s[10] == 'T' && s[13] == ':' &&
                     s[16] == ':' && s[19] == 'Z',
                 "UTC 时间格式错误");
    for (auto i : {11, 12, 14, 15, 17, 18})
        requireFanLX(std::isdigit(static_cast<unsigned char>(s[i])) != 0, "UTC 时间非数字");
    requireFanLX(std::stoi(s.substr(11, 2)) < 24 && std::stoi(s.substr(14, 2)) < 60 &&
                     std::stoi(s.substr(17, 2)) < 60,
                 "UTC 时间越界");
}
void optionalTimeFanLX(const std::optional<std::string> &s) {
    if (s)
        timeFanLX(*s);
}
using RefKeyFanLX = std::pair<ServiceTypeFanLX, std::string>;
RefKeyFanLX refFanLX(const AccountRefFanLX &r) {
    idFanLX(r.accountId);
    return {r.service, r.accountId};
}
void workflowFanLX(bool pending, const std::string &status, const std::optional<std::string> &created,
                   const std::optional<std::string> &expires) {
    const std::set<std::string> allowed{"Pending", "Approved", "Rejected", "Cancelled", "Expired"};
    requireFanLX(allowed.count(status) != 0 && pending == (status == "Pending"), "工作流状态不一致");
    optionalTimeFanLX(created);
    optionalTimeFanLX(expires);
    requireFanLX(!created || !expires || *created <= *expires, "失效时间早于创建时间");
}
} // namespace
void SchemaValidatorFanLX::validate(const PlatformSnapshotFanLX &s) {
    requireFanLX((s.generation == 0 && s.parentGeneration == 0) ||
                     (s.generation > 0 && s.parentGeneration == s.generation - 1),
                 "提交代数不连续");
    if (s.generation)
        timeFanLX(s.committedAtUtc);
    requireFanLX(s.users.size() <= 10000 && s.accounts.size() <= 30000 && s.friendships.size() <= 100000 &&
                     s.groups.size() <= 1000 && s.archivedGroups.size() <= 1000,
                 "记录总量超限");
    std::set<std::string> users;
    std::map<RefKeyFanLX, ServiceAccountFanLX> accounts;
    std::map<std::pair<std::string, ServiceTypeFanLX>, ServiceAccountFanLX> owned;
    for (const auto &u : s.users) {
        idFanLX(u.userId);
        requireFanLX(users.insert(u.userId).second, "用户重复");
        requireFanLX(u.nickname.size() <= 4096 && u.location.size() <= 4096, "资料过长");
        requireFanLX(dateFanLX(u.birthDate) && dateFanLX(u.applyDate) && u.birthDate <= u.applyDate,
                     "用户日期不合法");
        // 密码是登录唯一凭据：空密码的账号库里没有任何账号可以登录，属于无效状态。
        // 旧快照（本次改造前写出的文件）没有 password 字段，加载到这里会命中这条错误，
        // 提示用户重新 --init-demo，而不是让程序后面用"密码永远是错的"来表现故障。
        requireFanLX(!u.password.empty(), "用户缺少登录密码（旧数据请重新 --init-demo 初始化）");
        requireFanLX(u.password.size() <= 4096, "密码过长");
    }
    for (const auto &a : s.accounts) {
        idFanLX(a.accountId);
        requireFanLX(users.count(a.userId) != 0, "账号用户不存在");
        requireFanLX(a.service == ServiceTypeFanLX::QQ || a.service == ServiceTypeFanLX::Wechat ||
                         a.service == ServiceTypeFanLX::Weibo || a.service == ServiceTypeFanLX::WeiX,
                     "服务错误");
        requireFanLX(accounts.emplace(RefKeyFanLX{a.service, a.accountId}, a).second &&
                         owned.emplace(std::make_pair(a.userId, a.service), a).second,
                     "账号重复");
        requireFanLX(a.service != ServiceTypeFanLX::Wechat || !a.sharedId, "微信不能使用共享 ID 标记");
        // 微X 是上层账号，不参与"共享号码"语义（共享号码说的是 QQ 与微博这一对下级服务）。
        requireFanLX(a.service != ServiceTypeFanLX::WeiX || !a.sharedId, "微X 不能使用共享 ID 标记");
    }
    for (const auto &user : users) {
        auto q = owned.find({user, ServiceTypeFanLX::QQ}), w = owned.find({user, ServiceTypeFanLX::Weibo});
        if (q != owned.end() && w != owned.end())
            requireFanLX(q->second.accountId == w->second.accountId && q->second.sharedId &&
                             w->second.sharedId,
                         "QQ 与微博必须共享 ID");
    }
    std::set<RefKeyFanLX> subscribed, fromBindings;
    for (const auto &r : s.subscriptions)
        requireFanLX(accounts.count(refFanLX(r)) && subscribed.insert(refFanLX(r)).second,
                     "开通引用无效/重复");
    for (const auto &b : s.bindings) {
        auto l = accounts.find(refFanLX(b.left)), r = accounts.find(refFanLX(b.right));
        requireFanLX(l != accounts.end() && r != accounts.end(), "绑定账号不存在");
        requireFanLX(l->second.userId == r->second.userId && b.left.service != b.right.service &&
                         fromBindings.insert(refFanLX(b.left)).second,
                     "绑定归属或方向重复");
    }
    std::set<std::pair<RefKeyFanLX, RefKeyFanLX>> friendships;
    for (const auto &f : s.friendships) {
        auto l = refFanLX(f.left), r = refFanLX(f.right);
        requireFanLX(l != r && l.first == r.first && subscribed.count(l) && subscribed.count(r),
                     "好友端点无效/未开通/跨平台");
        if (r < l)
            std::swap(l, r);
        requireFanLX(friendships.emplace(l, r).second, "好友关系重复");
        requireFanLX(f.leftRemark.size() <= 4096 && f.rightRemark.size() <= 4096 &&
                         f.leftTag.size() <= 4096 && f.rightTag.size() <= 4096,
                     "好友资料过长");
    }
    std::set<std::pair<GroupModeFanLX, std::string>> keys;
    std::size_t total = 0;
    auto validateGroup = [&](const GroupRecordFanLX &g, bool archived) {
        idFanLX(g.key.id);
        requireFanLX(g.name.size() <= 4096, "群名过长");
        requireFanLX((g.key.mode == GroupModeFanLX::QQ || g.key.mode == GroupModeFanLX::Wechat) &&
                         (g.currentMode == GroupModeFanLX::QQ || g.currentMode == GroupModeFanLX::Wechat),
                     "非法群模式");
        if (!archived)
            requireFanLX(keys.emplace(g.key.mode, g.key.id).second, "群键重复");
        auto service = g.key.mode == GroupModeFanLX::QQ ? ServiceTypeFanLX::QQ : ServiceTypeFanLX::Wechat;
        auto validUser = [&](const std::string &id) {
            auto a = owned.find({id, service});
            requireFanLX(a != owned.end() && subscribed.count({service, a->second.accountId}),
                         "群引用成员无服务账号/未开通");
        };
        std::size_t owners = 0;
        std::set<std::size_t> orders;
        for (const auto &m : g.state.members) {
            validUser(m.first);
            requireFanLX(m.second.role == GroupRoleFanLX::Owner || m.second.role == GroupRoleFanLX::Admin ||
                             m.second.role == GroupRoleFanLX::Member,
                         "成员角色错误");
            owners += m.second.role == GroupRoleFanLX::Owner;
            requireFanLX(orders.insert(m.second.joinOrder).second &&
                             m.second.joinOrder < g.state.nextJoinOrder,
                         "成员序号重复/下一序号错误");
            optionalTimeFanLX(m.second.joinedAtUtc);
        }
        requireFanLX(owners == 1, "群必须恰有一个群主");
        std::set<std::string> apps, invs, discs;
        for (const auto &a : g.applications) {
            validUser(a.applicant);
            requireFanLX(apps.insert(a.applicant).second, "重复申请");
            workflowFanLX(a.pending, a.status, a.createdAtUtc, a.expiresAtUtc);
        }
        for (const auto &a : g.invitations) {
            validUser(a.inviter);
            validUser(a.invitee);
            requireFanLX(invs.insert(a.invitee).second, "重复邀请");
            workflowFanLX(a.pending, a.status, a.createdAtUtc, a.expiresAtUtc);
        }
        for (const auto &d : g.discussions) {
            idFanLX(d.id);
            requireFanLX(discs.insert(d.id).second, "子群重复");
            std::set<std::string> members;
            for (const auto &m : d.members) {
                validUser(m);
                requireFanLX(members.insert(m).second, "子群成员重复");
                if (!d.archived)
                    requireFanLX(g.state.members.count(m) != 0, "子群不满足父群子集");
            }
            requireFanLX((d.members.empty() && d.owner.empty()) || members.count(d.owner), "子群负责人无效");
            requireFanLX(!archived || d.archived, "解散群包含活动子群");
            total += d.members.size();
        }
        total += g.state.members.size() + g.applications.size() + g.invitations.size() + g.discussions.size();
        requireFanLX(total <= 1000000, "群域累计记录超过一百万");
    };
    for (const auto &g : s.groups)
        validateGroup(g, false);
    for (const auto &g : s.archivedGroups)
        validateGroup(g, true);
    for (const auto &v : s.versions)
        requireFanLX(!v.first.empty() && v.first.size() <= 4096 && v.second <= s.generation, "局部版本非法");
}
