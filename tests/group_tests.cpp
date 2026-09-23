// 即使采用 Release 构建，也执行断言及断言内的业务调用。
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "GroupDomainFanLX.h"
#include "fixtures/ThrowingGroupPolicyFanLX.h"
#include <cassert>

template<class ExceptionFanLX = std::runtime_error, class ActionFanLX>
void expectExceptionFanLX(ActionFanLX action) {
    bool caught = false;
    try { action(); }
    catch (const ExceptionFanLX&) { caught = true; }
    assert(caught);
}

int main() {
    GroupServiceFanLX service;
    const GroupKeyFanLX qq{GroupModeFanLX::QQ, "1001"};
    const GroupKeyFanLX wx{GroupModeFanLX::Wechat, "1001"};
    auto& group = service.create(qq);
    group.addMember("owner", GroupRoleFanLX::Owner);
    group.addMember("admin", GroupRoleFanLX::Admin);
    group.addMember("admin2", GroupRoleFanLX::Admin);
    group.addMember("member");

    // T06：各角色的踢人权限；失败操作必须保持成员不变。
    for (const auto& target : {"owner", "admin", "admin2", "member"})
        assert(!group.removeMember("member", target));
    assert(!group.removeMember("admin", "owner"));
    assert(!group.removeMember("admin", "admin2"));
    assert(!group.removeMember("admin", "admin"));
    assert(!group.removeMember("owner", "owner"));
    assert(group.removeMember("admin", "member"));
    group.addMember("member");
    assert(group.removeMember("owner", "member"));
    assert(group.removeMember("owner", "admin2"));
    group.addMember("member");
    group.setRole("member", GroupRoleFanLX::Admin);
    assert(group.roleOf("member") == GroupRoleFanLX::Admin);
    group.setRole("member", GroupRoleFanLX::Member);
    assert(group.roleOf("member") == GroupRoleFanLX::Member);
    assert(service.leave(qq, "member"));
    assert(!service.leave(qq, "member"));
    assert(service.leave(qq, "admin"));
    expectExceptionFanLX([&] { service.leave(qq, "owner"); });
    group.addMember("admin", GroupRoleFanLX::Admin);
    group.addMember("member");

    // T07：申请不等于入群，审批必须通过权限验证。
    service.applyToJoin(qq, "applicant");
    service.applyToJoin(qq, "applicant");
    assert(service.pendingApplicationCount(qq) == 1);
    assert(!group.hasMember("applicant"));
    expectExceptionFanLX([&] { service.approveApplication(qq, "member", "applicant"); });
    expectExceptionFanLX([&] { service.rejectApplication(qq, "outsider", "applicant"); });
    assert(service.approveApplication(qq, "owner", "applicant"));
    assert(group.roleOf("applicant") == GroupRoleFanLX::Member);
    assert(!service.approveApplication(qq, "owner", "applicant"));
    service.applyToJoin(qq, "applicant");
    assert(service.pendingApplicationCount(qq) == 0);
    service.applyToJoin(qq, "denied");
    assert(service.rejectApplication(qq, "owner", "denied"));
    assert(!service.rejectApplication(qq, "owner", "denied"));
    assert(!group.hasMember("denied"));
    service.applyToJoin(qq, "byAdmin");
    assert(service.approveApplication(qq, "admin", "byAdmin"));
    service.applyToJoin(qq, "deniedByAdmin");
    assert(service.rejectApplication(qq, "admin", "deniedByAdmin"));

    // T08：同号但不同平台的群互不影响；普通成员可以推荐。
    auto& wechat = service.create(wx);
    wechat.addMember("wxOwner", GroupRoleFanLX::Owner);
    wechat.addMember("inviter");
    expectExceptionFanLX([&] { service.applyToJoin(wx, "outside"); });
    expectExceptionFanLX([&] { service.approveApplication(wx, "wxOwner", "outside"); });
    expectExceptionFanLX([&] { service.rejectApplication(wx, "wxOwner", "outside"); });
    expectExceptionFanLX([&] { service.inviteMember(qq, "owner", "outside"); });
    expectExceptionFanLX([&] { service.confirmInvitation(qq, "outside"); });
    expectExceptionFanLX([&] { service.rejectInvitation(qq, "outside"); });
    expectExceptionFanLX([&] { service.inviteMember(wx, "outsider", "guest"); });
    service.inviteMember(wx, "inviter", "guest");
    service.inviteMember(wx, "wxOwner", "guest");
    assert(service.pendingInvitationCount(wx) == 1);
    assert(!wechat.hasMember("guest"));
    assert(service.confirmInvitation(wx, "guest"));
    assert(wechat.roleOf("guest") == GroupRoleFanLX::Member);
    assert(!service.confirmInvitation(wx, "guest"));
    service.inviteMember(wx, "inviter", "guest");
    assert(service.pendingInvitationCount(wx) == 0);
    service.inviteMember(wx, "inviter", "refused");
    assert(service.rejectInvitation(wx, "refused"));
    assert(!service.rejectInvitation(wx, "refused"));
    assert(!wechat.hasMember("refused"));
    service.inviteMember(wx, "inviter", "cancelled");
    service.inviteMember(wx, "wxOwner", "preserved");
    assert(service.leave(wx, "inviter"));
    assert(service.pendingInvitationCount(wx) == 1);
    assert(!service.confirmInvitation(wx, "cancelled"));
    assert(service.confirmInvitation(wx, "preserved"));

    // T09：子群资格、父群退群同步与生命周期。
    auto& child = service.createDiscussion(qq, "discussion", "owner");
    service.createDiscussion(qq, "adminDiscussion", "admin");
    assert(child.addMember(group, "member"));
    assert(!child.addMember(group, "outside"));
    assert(service.leave(qq, "member"));
    assert(!child.hasMember("member"));
    expectExceptionFanLX([&] { service.createDiscussion(wx, "x", "wxOwner"); });
    expectExceptionFanLX([&] { service.createDiscussion(qq, "x", "applicant"); });
    expectExceptionFanLX([&] { service.createDiscussion(qq, "discussion", "owner"); });
    expectExceptionFanLX([&] { service.dissolve(qq, "admin"); });
    expectExceptionFanLX([&] { service.transferOwnership(qq, "admin", "applicant"); });
    expectExceptionFanLX<std::out_of_range>([&] {
        service.transferOwnership(qq, "owner", "missing");
    });
    assert(group.roleOf("owner") == GroupRoleFanLX::Owner);
    service.transferOwnership(qq, "owner", "applicant");
    assert(group.roleOf("owner") == GroupRoleFanLX::Member);
    assert(group.roleOf("applicant") == GroupRoleFanLX::Owner);
    assert(service.leave(qq, "owner"));
    service.applyToJoin(qq, "pendingAtDissolve");
    service.dissolve(qq, "applicant");
    expectExceptionFanLX<std::out_of_range>([&] { service.get(qq); });
    expectExceptionFanLX<std::out_of_range>([&] { service.getDiscussion(qq, "discussion"); });
    service.create(qq);
    assert(service.pendingApplicationCount(qq) == 0);
    expectExceptionFanLX<std::out_of_range>([&] { service.getDiscussion(qq, "discussion"); });
    assert(service.get(wx).hasMember("wxOwner"));
    service.inviteMember(wx, "wxOwner", "pendingAtDissolve");
    service.dissolve(wx, "wxOwner");
    service.create(wx);
    assert(service.pendingInvitationCount(wx) == 0);
    assert(!service.confirmInvitation(wx, "pendingAtDissolve"));
    // T10：群模式无损切换；独立数据不改变 T06–T09 的原有场景。
    {
        GroupServiceFanLX switching;
        const GroupKeyFanLX key{GroupModeFanLX::QQ, "switch"};
        auto& current = switching.create(key);
        current.addMember("owner", GroupRoleFanLX::Owner);
        for (const auto& id : {"admin", "revoked", "member", "childMember", "other"})
            current.addMember(id);
        const auto initial = current.snapshot();
        assert(!current.addMember("admin"));
        assert(current.snapshot() == initial);
        current.addMember("next");
        for (const auto& row : current.snapshot())
            if (row.id == "next") assert(row.joinOrder == initial.size());

        // T10a：管理员的存储角色不变，有效权限随当前策略变化。
        switching.assignAdmin(key, "owner", "admin");
        switching.assignAdmin(key, "owner", "revoked");
        assert(current.removeMember("admin", "member"));
        current.addMember("member");
        auto& discussion = switching.createDiscussion(key, "D1", "admin");
        assert(switching.joinDiscussion(key, "D1", "admin", "childMember"));
        auto* stableGroup = &current;
        auto* stableDiscussion = &discussion;
        switching.switchToWechat(key, "owner");
        assert(current.currentMode() == GroupModeFanLX::Wechat);
        assert(current.key() == key && &switching.get(key) == stableGroup);
        assert(current.roleOf("admin") == GroupRoleFanLX::Admin);
        assert(!current.removeMember("admin", "member"));
        expectExceptionFanLX([&] { switching.createDiscussion(key, "blocked", "admin"); });
        assert(current.removeMember("owner", "member"));
        current.addMember("member");

        // T10b：双向往返比较完整快照，且群键、子群引用保持稳定。
        switching.switchToQQ(key, "owner");
        const auto qqSnapshot = current.snapshot();
        switching.switchToWechat(key, "owner");
        assert(current.snapshot() == qqSnapshot);
        switching.switchToQQ(key, "owner");
        assert(current.snapshot() == qqSnapshot);
        switching.switchToWechat(key, "owner");
        const auto wxSnapshot = current.snapshot();
        switching.switchToQQ(key, "owner");
        switching.switchToWechat(key, "owner");
        assert(current.snapshot() == wxSnapshot);
        assert(&switching.getDiscussion(key, "D1") == stableDiscussion);
        assert(discussion.hasMember("childMember"));

        // T10c：休眠管理员回切后恢复；撤销/退群/被踢者不恢复。
        assert(!current.removeMember("admin", "member"));
        expectExceptionFanLX([&] { switching.assignAdmin(key, "owner", "other"); });
        switching.revokeAdmin(key, "owner", "revoked");
        switching.switchToQQ(key, "owner");
        assert(current.removeMember("admin", "member"));
        assert(current.roleOf("revoked") == GroupRoleFanLX::Member);
        assert(!current.removeMember("revoked", "other"));
        switching.assignAdmin(key, "owner", "other");
        switching.assignAdmin(key, "owner", "next");
        switching.switchToWechat(key, "owner");
        assert(switching.leave(key, "other"));
        assert(current.removeMember("owner", "next"));
        switching.switchToQQ(key, "owner");
        assert(!current.hasMember("other") && !current.hasMember("next"));

        // T10d：子群冻结限制新建和扩员，不阻止维护父子成员子集。
        switching.switchToWechat(key, "owner");
        expectExceptionFanLX([&] { switching.joinDiscussion(key, "D1", "owner", "revoked"); });
        expectExceptionFanLX([&] { switching.joinDiscussion(key, "D1", "admin", "revoked"); });
        expectExceptionFanLX([&] { switching.createDiscussion(key, "blocked", "owner"); });
        assert(switching.leave(key, "childMember"));
        assert(!discussion.hasMember("childMember"));
        switching.switchToQQ(key, "owner");
        assert(switching.joinDiscussion(key, "D1", "owner", "revoked"));

        // T10e：拒绝切换与构造失败均保留原策略对象及完整成员快照。
        const auto beforeFailure = current.snapshot();
        const auto beforeName = current.policy().name();
        const auto* beforePolicy = &current.policy();
        expectExceptionFanLX([&] {
            switching.switchPolicy(key, "revoked", std::make_unique<WechatGroupPolicyFanLX>());
        });
        assert(current.snapshot() == beforeFailure && current.policy().name() == beforeName);
        assert(&current.policy() == beforePolicy);
        expectExceptionFanLX<std::invalid_argument>([&] {
            switching.switchPolicy(key, "owner", nullptr);
        });
        assert(current.snapshot() == beforeFailure && current.policy().name() == beforeName);
        assert(&current.policy() == beforePolicy);
        expectExceptionFanLX([&] {
            switching.switchPolicy(key, "owner", std::make_unique<ThrowingGroupPolicyFanLX>());
        });
        assert(current.snapshot() == beforeFailure && current.policy().name() == beforeName);
        assert(&current.policy() == beforePolicy);

        // 管理员任免/转让只改变角色，不改变加入序号。
        expectExceptionFanLX([&] { switching.assignAdmin(key, "admin", "revoked"); });
        expectExceptionFanLX([&] { switching.assignAdmin(key, "owner", "owner"); });
        expectExceptionFanLX([&] { switching.revokeAdmin(key, "admin", "admin"); });
        expectExceptionFanLX([&] { switching.revokeAdmin(key, "owner", "revoked"); });
        expectExceptionFanLX<std::out_of_range>([&] {
            switching.assignAdmin(key, "owner", "missing");
        });
        switching.transferOwnership(key, "owner", "revoked");
        const auto afterTransfer = current.snapshot();
        assert(afterTransfer.size() == beforeFailure.size());
        for (std::size_t i = 0; i < afterTransfer.size(); ++i) {
            assert(afterTransfer[i].id == beforeFailure[i].id);
            assert(afterTransfer[i].joinOrder == beforeFailure[i].joinOrder);
        }

        // T10f：六个工作流入口均按当前模式门控；暂停记录原位保留。
        switching.applyToJoin(key, "waiting");
        switching.switchToWechat(key, "revoked");
        expectExceptionFanLX([&] { switching.applyToJoin(key, "newApplicant"); });
        expectExceptionFanLX([&] { switching.approveApplication(key, "revoked", "waiting"); });
        expectExceptionFanLX([&] { switching.rejectApplication(key, "revoked", "waiting"); });
        assert(switching.pendingApplicationCount(key) == 1);
        switching.inviteMember(key, "owner", "invited");
        switching.switchToQQ(key, "revoked");
        expectExceptionFanLX([&] { switching.inviteMember(key, "owner", "newInvitee"); });
        expectExceptionFanLX([&] { switching.confirmInvitation(key, "invited"); });
        expectExceptionFanLX([&] { switching.rejectInvitation(key, "invited"); });
        assert(switching.pendingInvitationCount(key) == 1);
        assert(switching.approveApplication(key, "admin", "waiting"));
        switching.switchToWechat(key, "revoked");
        assert(switching.confirmInvitation(key, "invited"));
        assert(current.hasMember("waiting") && current.hasMember("invited"));
        assert(switching.pendingApplicationCount(key) == 0);
        assert(switching.pendingInvitationCount(key) == 0);

        // 微信命名空间也能采用 QQ 治理，同 ID 的原 QQ 群仍独立存在。
        const GroupKeyFanLX reverseKey{GroupModeFanLX::Wechat, "switch"};
        auto& reverse = switching.create(reverseKey);
        reverse.addMember("wxOwner", GroupRoleFanLX::Owner);
        switching.switchToQQ(reverseKey, "wxOwner");
        assert(reverse.key() == reverseKey && reverse.currentMode() == GroupModeFanLX::QQ);
        switching.applyToJoin(reverseKey, "wxApplicant");
        assert(switching.approveApplication(reverseKey, "wxOwner", "wxApplicant"));
        assert(&switching.get(key) == stableGroup);
    }
    return 0;
}
