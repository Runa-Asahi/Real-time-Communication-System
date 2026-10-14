#pragma once
#include "network/TcpListenerFanLX.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

struct PlatformStateFanLX;

// 中心转发服务端。tick 与领域状态修改必须在同一控制线程执行；
// 接收线程只负责收帧，公共状态由控制线程独占（不使用跨线程共享容器）。
//
// 认证：LOGIN 的 sender 是服务账号（QQ 号 / 微信号 / 微博号），text 是用户自选密码，
// 服务端直接查领域账号库（PlatformStateFanLX::accounts）校验，不再需要令牌表 /
// 预共享密钥文件——账号库本身就在领域快照里，随 state 一起加载。
//
// 账号数据的权威副本在**持久化存储**（服务端数据目录的 state.json）里，服务端不把它当成
// "只在内存里"的状态：OptionsFanLX::refreshAccounts 会在每次认证前被调用（调用方按"文件变了才重读"
// 实现，见 PersistenceApplicationFanLX::reloadIfChangedFromDisk），
// OptionsFanLX::registerAccount 让客户端通过 IM 的 REGISTER 直接在这台服务器上建微X账号，
// 因此**注册之后不需要重启服务端**。
//
// 存储后端：
// - chatDatabasePath 非空时使用 SQLite 消息库（提交成功后才发 SEND_ACK）；
// - 否则退回既有 JSON 日志（保持旧调用方与客户端生命周期重构用例不变）。
//
// 慢客户端：每连接有待发送缓冲，主循环按预算非阻塞推进，不再为单个客户端等待数秒。
class ImServerFanLX {
  public:
    struct OptionsFanLX {
        std::filesystem::path chatDatabasePath; // 空 = 使用旧 JSON 日志
        std::string instanceId;                 // 服务器持久实例号；为空时自动生成并保存
        // 账号库读穿：每次认证之前调用一次，由调用方决定是否需要重新读取持久化快照。
        // 为空时不做任何刷新（进程内测试直接共享内存状态，本来就不需要）。
        std::function<void()> refreshAccounts;
        // 服务端注册入口：客户端 REGISTER 时调用。成功返回空串，失败返回可直接显示的原因。
        // services 是客户端**本机已开通**的下级服务清单（服务名 + 号码），服务端据此在
        // 持久化账号库里补建/确认这些服务账号——否则"注册 → 开通 → 登录"仍会在
        // "该账号未开通 QQ 服务"这一步失败（开通也必须落到服务端）。
        // friends 是客户端**本机已加**的好友（服务名 + 对端号码），服务端据此补登关系，
        // 否则本机加的好友发消息会被服务端判为"不是好友"。
        // 已存在的服务与关系不会被改写，因此该调用是幂等的。
        // 为空表示本进程不提供注册（测试 / 只读演示），此时 REGISTER 会被明确拒绝。
        std::function<std::string(const std::string &weixId, const std::string &password,
                                  const std::string &nickname, const std::string &birthDate,
                                  const std::string &location,
                                  const std::vector<std::pair<std::string, std::string>> &services,
                                  const std::vector<std::pair<std::string, std::string>> &friends)>
            registerAccount;
        // 好友关系落地：好友申请被同意（add=true）或一方删除好友（add=false）时调用，
        // 由调用方写进持久化账号库（见 ServerAccountRegistrarFanLX::updateFriendshipFanLX）。
        // service 是通道服务名（QQ / Wechat / Weibo）。成功返回空串，失败返回原因。
        // 为空时（进程内测试共享内存状态）好友申请仍可提交，但"同意"会被明确拒绝。
        std::function<std::string(const std::string &service, const std::string &self,
                                  const std::string &peer, bool add)>
            updateFriendship;
        // 非好友临时会话每个方向允许的消息条数（课设新增：允许一定限额的临时对话）。
        int temporaryMessageLimit = 10;
    };

    ImServerFanLX(const PlatformStateFanLX &state, const std::filesystem::path &directory,
                  unsigned short port, const std::string &ipv4 = "127.0.0.1");
    ImServerFanLX(const PlatformStateFanLX &state, const std::filesystem::path &directory,
                  unsigned short port, const std::string &ipv4, OptionsFanLX options);
    ~ImServerFanLX();
    ImServerFanLX(const ImServerFanLX &) = delete;
    ImServerFanLX &operator=(const ImServerFanLX &) = delete;

    unsigned short port() const;
    void tick();
    std::size_t connectionsAccepted() const;
    // 服务器实例标识：客户端据此判断本地聊天库是否属于本服务器。
    std::string instanceId() const;
    std::string storageBackend() const;
    // 因待发送缓冲满而被判定为慢端并关闭的连接数（验收统计）。
    std::size_t slowPeerDisconnects() const;

    // 直连是在线好友单聊的数据面。群、离线、非好友和发送方自己的历史回显仍走中转。
    // peerRelayBodies 只统计“正文经由服务器发给接收方”的次数，不含发送方回显。
    struct DirectRouteStatsFanLX {
        std::size_t directSuccess = 0;
        std::size_t directFail = 0;
        std::size_t fallbacks = 0;
        std::size_t p2pMessages = 0;
        std::size_t relayMessages = 0;
        std::size_t relayDirectMessages = 0;
        std::size_t peerRelayBodies = 0;
        std::size_t groupRelayMessages = 0;
    };
    DirectRouteStatsFanLX directRouteStats() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> implFanLX;
};
