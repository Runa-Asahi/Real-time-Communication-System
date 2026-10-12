#pragma once
#include "account/AccountRefFanLX.h"
#include "account/ServiceTypeFanLX.h"
#include <string>
#include <vector>

class PersistenceApplicationFanLX;

namespace peerAccountFanLX {

// 添加好友前的"对端账号解析"结果。
enum class ResolutionFanLX {
    Ready,    // 本机已经认识且已确认该对端，无需任何网络动作
    Imported, // 服务端确认存在，已把最小账号记录导入本机副本
    Missing,  // 服务端也没有这个号码（对端没注册过该服务）
    Unreachable // 连不上服务端 / 被拒（reason 里有可直接显示的原因）
};

// 解析"要添加的好友"这个号码。
//
// 为什么需要它：本机领域库只是"谁存在、谁开通了什么"的一份**局部视图**。别人在各自的客户端里
// 注册/开通出来的账号不会自动同步到本机，而领域的加好友前置校验查的正是本机账号表，于是直接
// 添加就会得到"对端平台账号不存在"——看起来像是号码填错了，其实对端在服务端是存在的。
// 这里用已经登录的聊天通道凭据向服务端查询一次（IM 的 LOOKUP），必要时把对端的最小账号记录
// 导入本机副本（见 AccountManagerFanLX::importRemoteAccount），之后原来的加好友流程即可继续。
//
// 所有调用方（GUI 的 DomainControllerFanLX、控制台 ConsoleAppFanLX）都走这一条，
// 保证界面与命令行行为一致。
ResolutionFanLX resolvePeerAccountFanLX(PersistenceApplicationFanLX &app, const std::string &host,
                                        unsigned short port, ServiceTypeFanLX service,
                                        const std::string &selfAccount, const std::string &password,
                                        const std::string &peerAccount, std::string &reason);

// 一次经服务端中转的好友操作（request / accept / reject / cancel / remove / list）的结果。
struct FriendOperationResultFanLX {
    bool ok = false;        // 服务端已执行
    bool reachable = false; // 服务端给出了应答（false = 连不上 / 超时）
    std::string reason;     // 失败原因（可直接显示）
    std::string message;    // 服务端的结果说明
    std::string peer;       // 实际对端号码（跨服务添加时为换算后的号码）
    std::vector<std::string> incoming, outgoing; // 待处理申请："号码 ｜ 附言 ｜ 时间"
    std::vector<std::string> temporary;          // 临时会话："号码 ｜ 我已发 n / 对方已发 m ｜ 剩余 k"
    int limit = 0;          // 临时会话每方向条数上限
    bool localChanged = false; // 本机好友副本是否因对账而改变
};

// 好友关系的权威副本在服务端（申请须对方同意后才成立），本机领域库只是副本：
// 执行操作后按服务端给出的好友清单对账本机分区——缺的补进来（必要时先导入对端账号），
// 清单完整时本机多出的也删掉。GUI 与控制台共用这一条实现。
// via 非空 = 依据本人在 via 服务里的好友 peer 添加（服务端换算成 actor 服务的号码）。
FriendOperationResultFanLX friendOperationFanLX(PersistenceApplicationFanLX &app, const std::string &host,
                                                unsigned short port, const AccountRefFanLX &actor,
                                                const std::string &password, const std::string &op,
                                                const std::string &peer, const std::string &note,
                                                const std::string &via = {});

} // namespace peerAccountFanLX
