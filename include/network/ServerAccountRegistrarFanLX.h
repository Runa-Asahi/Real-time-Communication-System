#pragma once
#include <string>
#include <utility>
#include <vector>

class PersistenceApplicationFanLX;

// ServerAccountRegistrarFanLX：服务端的**账号登记入口**（IM 的 REGISTER 消息在这里落地）。
//
// 要解决的问题（用户实测反馈）：在本机注册了微X账号、密码也没输错，却登不上聊天。
// 原因是"两份账号库"：注册只写了**客户端自己的**领域库
// （<数据根>/im-client/<服务器>_<端口>/<微X号>/domain/state.json），而聊天认证查的是
// **服务端**的持久化账号库（<服务端数据目录>/state.json）。两份文件互不相通，
// 于是无论等多久、重启多少次服务端，服务端都不认识这个新账号。
//
// 本类把登记这件事做到服务端的持久化存储里，并且让认证不再依赖"启动那一刻"的快照：
//   · 登记写进服务端快照（候选事务 + 原子发布），提交成功即已落盘；
//   · `refreshAccounts()` 供服务端在每次认证前调用：只有磁盘内容变了才重新解析（读穿），
//     因此账号数据的权威副本始终是数据目录里的 state.json，**注册后不需要重启服务端**；
//   · 幂等：微X号已存在且密码一致 → 成功（客户端可以"先登记再登录"，重复调用无副作用）；
//     密码不一致 → 拒绝，并且不区分"账号不存在"与"密码错"，避免被用来枚举账号；
//   · 可携带客户端本机已开通的服务清单：不补建这些服务账号，登录仍会卡在"该服务未开通"。
//     已存在的服务保持服务端现有号码，不被改写。
class ServerAccountRegistrarFanLX {
  public:
    explicit ServerAccountRegistrarFanLX(PersistenceApplicationFanLX &app) : appFanLX(app) {}

    // 读穿：账号库以持久化存储为准。返回是否真的重新加载了。
    bool refreshAccounts() const;

    // 登记（含可选的服务开通与好友关系）；成功返回空串，失败返回可直接显示给用户的原因。
    // services：{服务名, 号码}，服务端为本人补建该服务账号并开通；已存在的不改写。
    // friends： {服务名, 对端号码}，服务端在本人该服务下补登好友关系（对端必须已存在于该服务）；
    //           不补登的话，客户端本机加的好友发消息会被服务端判为"不是好友"。
    std::string registerAccountFanLX(
        const std::string &weixId, const std::string &password, const std::string &nickname,
        const std::string &birthDate, const std::string &location,
        const std::vector<std::pair<std::string, std::string>> &services,
        const std::vector<std::pair<std::string, std::string>> &friends) const;

    // 好友关系的服务端落地（好友申请被同意 / 一方删除好友时由 IM 服务端调用）。
    // service 是聊天通道服务名（QQ / Wechat / Weibo），self、peer 是该服务内的号码。
    // add=true 建立关系，add=false 解除关系；均幂等。成功返回空串，失败返回原因。
    std::string updateFriendshipFanLX(const std::string &service, const std::string &self,
                                      const std::string &peer, bool add) const;

  private:
    PersistenceApplicationFanLX &appFanLX;
};
