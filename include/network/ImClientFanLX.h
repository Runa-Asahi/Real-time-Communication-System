#pragma once
#include "db/ChatRepositoryFanLX.h"
#include "network/ChatMessageFanLX.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// 客户端事件：纯值对象，跨线程投递，不携带 socket、仓储或会话句柄。
struct ImClientEventFanLX {
    enum class KindFanLX {
        Sent,          // 本地已提交"待发送"（requestId 可用）
        Accepted,      // 服务器已接收（SEND_ACK 已落库）
        SendRejected,  // 服务器拒绝或无法组帧（消息保留在库中并标记失败，不自动重发）
        Delivered,     // 收到投递且已落库（落库成功后才回 ACK）
        History,       // 历史分页结果
        Conversations, // 会话列表
        Directory,     // 服务器授权的联系人/群目录（JSON 文本）
        Presence,      // 上下线
        SyncEnd,       // 同步结束
        Connection,    // 连接状态变化（detail 为可显示说明）
        Notice,        // 不是错误、但必须让用户知道的事（例如旧聊天库已被归档重建）
        FriendNotice,  // 好友动态（收到好友申请 / 申请被同意 / 被删除），detail 为可显示说明
        Error          // 其它错误
    };
    KindFanLX kind = KindFanLX::Error;
    std::int64_t queryId = 0;
    std::string conversationKey;
    std::string target;
    std::string requestId;
    std::string detail;   // 文本说明（错误原因 / 在线状态）
    bool value = false;   // 例如 appendDelivered 是否新增行
    std::int64_t sequence = 0;
    std::vector<chatDbFanLX::MessageRowFanLX> messages;
    chatDbFanLX::HistoryPageFanLX history;
    std::vector<chatDbFanLX::ConversationRowFanLX> conversations;
};

// IM 客户端。工作线程独占 socket、连接状态、心跳、重连和消息状态机；
// 投递入库由独立数据库工作线程完成，只有提交成功才回 ACK。
//
// 认证参数是「账号 + 用户自选密码」，与领域登录用的是同一套凭据：
// 客户端不读任何令牌文件，密码由用户在登录界面 / 菜单里输入后传进来。
//
// 接口分两层：
// - 异步接口（供 GUI 与控制台使用）：调用立即返回，结果经 pollClientEvent 回报；
// - 同步便捷接口（供既有测试使用）：send / history / pending / pollEvent 等，内部等待并返回结果，
//   只允许在非 UI 线程或明确接受短暂阻塞的场景调用。
class ImClientFanLX {
  public:
    struct ChatDbConfigFanLX {
        bool enabled = false;
        std::filesystem::path databasePath; // 默认 <目录>/chat.db
    };
    using EventFanLX = ImClientEventFanLX;

    // service：本连接所属的服务（"QQ" / "Wechat" / "Weibo"）。一条连接固定属于一个服务，
    // 服务端据此只在该服务的好友与群里鉴权和投递（微博只有好友、没有群）；
    // 微X 是上层登录身份，不承载聊天。缺省 "QQ" 保持既有调用方（测试 / 旧脚本）可用。
    ImClientFanLX(std::string host, unsigned short port, std::string account, std::string password,
                  const std::filesystem::path &directory, std::string service = "QQ");
    ImClientFanLX(std::string host, unsigned short port, std::string account, std::string password,
                  const std::filesystem::path &directory, ChatDbConfigFanLX chatDb,
                  std::string service = "QQ");
    ~ImClientFanLX();
    ImClientFanLX(const ImClientFanLX &) = delete;
    ImClientFanLX &operator=(const ImClientFanLX &) = delete;

    // ---- 同步便捷接口 ----
    std::string send(const std::string &target, const std::string &text, bool group = false);
    void queryOnline(const std::string &friendAccount);
    std::vector<ChatMessageFanLX> history() const;
    std::optional<ChatMessageFanLX> pollEvent();
    std::size_t pending() const;
    bool online() const;
    std::string error() const;
    void reconnect();

    // ---- 账号登记（服务端无状态：注册后无需重启服务端） ----
    // 把「微X号 + 密码 + 资料 + 本机已开通的下级服务」登记到服务端并**持久化**进服务端账号库；
    // 幂等：账号已存在且密码一致时同样算成功，因此客户端可以"先登记再登录"。
    // services 传本机已开通的服务（服务名 + 号码），服务端据此补建服务账号与开通关系；
    // 已存在的服务不会被改写。静态方法：登记发生在登录之前，不需要也不应该先建聊天会话。
    // 返回 true = 服务端已确认；false 时 reason 给出可直接显示的原因（连不上 / 被拒绝）。
    using ServiceEntryFanLX = std::pair<std::string, std::string>; // {服务名（QQ/Wechat/Weibo）, 号码}
    // services：本机已开通的服务；friends：本机已加的好友（服务名 + 对端号码）。
    // 两者都会补登到服务端，否则服务端会分别以"未开通该服务""不是好友"拒掉聊天。
    static bool registerAccountFanLX(const std::string &host, unsigned short port,
                                     const std::string &weixId, const std::string &password,
                                     const std::string &nickname, const std::string &birthDate,
                                     const std::string &location, std::string &reason,
                                     const std::vector<ServiceEntryFanLX> &services = {},
                                     const std::vector<ServiceEntryFanLX> &friends = {},
                                     int timeoutMs = 4000);

    // ---- 账号查询（添加好友前确认对端是否存在）----
    // 本机领域库只是"谁存在"的一份局部视图：别人在各自客户端注册/开通出来的账号不会自动出现，
    // 直接加好友就会得到"对端平台账号不存在"。这里问服务端（权威副本）要一个最小事实。
    struct LookupResultFanLX {
        bool exists = false;
        std::string service;   // 服务名（QQ / Wechat / Weibo）
        std::string accountId; // 对端号码
        std::string userId;    // 归属用户（导入本机副本时用）
        std::string nickname;  // 昵称（导入时作为占位资料）
        bool subscribed = false;
    };
    // 用"本人当前通道凭据"登录后查询一个号码；失败时 reason 给出可直接显示的原因。
    static bool lookupAccountFanLX(const std::string &host, unsigned short port,
                                   const std::string &service, const std::string &selfAccount,
                                   const std::string &password, const std::string &peerAccount,
                                   LookupResultFanLX &result, std::string &reason,
                                   int timeoutMs = 4000);

    // ---- 好友申请（服务端中转，课设新增：一方申请、对方同意后才成为好友）----
    // 与 LOOKUP 相同的一次性凭据请求：报文里带本人号码与密码，服务端校验后执行 op，不建立会话。
    // op：request（发起，note 为附言）/ accept / reject（回应 peer 发来的申请）/
    //     cancel（撤回发给 peer 的申请）/ remove（删除好友）/ list（只取列表，peer 可为空）。
    // 成功时 resultJson 为服务端应答（含 message / incoming / outgoing / friends / temporary / peer），
    // 失败时 reason 给出可直接显示的原因。
    // via 非空表示"依据本人在 via 服务里的好友 peerAccount 添加"：服务端换算成当前服务的号码
    // （应答的 peer 字段），只对 op=request 有效。
    // answered（可空）：服务端是否给出了应答。false = 连不上 / 超时，true = 服务端执行或明确拒绝。
    static bool friendOperationFanLX(const std::string &host, unsigned short port,
                                     const std::string &service, const std::string &selfAccount,
                                     const std::string &password, const std::string &op,
                                     const std::string &peerAccount, const std::string &note,
                                     std::string &resultJson, std::string &reason,
                                     const std::string &via = {}, bool *answered = nullptr,
                                     int timeoutMs = 4000);

    // ---- 异步接口 ----
    // 发送：先在数据库提交"待发送"，提交成功后才把 SEND 放入网络队列。
    // 队列满或库不可用返回 false（调用方应显示繁忙并保留输入文本）。
    bool submitSend(const std::string &target, const std::string &text, bool group,
                    std::string &requestIdOut);
    // 按会话键发送：收件人由会话键按方向推导（单聊取对端、群聊取群号），
    // 调用方不需要自己解析键——避免"把消息发给自己"这类只有一处写错的缺陷。
    bool submitSendToConversation(const std::string &conversationKey, const std::string &text,
                                  std::string &requestIdOut);    bool submitHistory(const chatDbFanLX::HistoryQueryFanLX &query, std::int64_t &queryIdOut);
    bool submitConversations();
    // 请求服务器授权目录（联系人/群）：服务端按权限计算，客户端不推断资格。
    bool submitDirectory();
    bool submitMarkRead(const std::string &conversationKey, std::int64_t sequence);
    // 作废指定查询：切换账号或会话后旧查询结果必须丢弃。
    void cancelQuery(std::int64_t queryId);

    std::optional<ImClientEventFanLX> pollClientEvent();
    std::size_t droppedEventCount() const;
    std::int64_t nextQueryId() const;
    // 本连接所属服务（"QQ" / "Wechat"）。
    std::string serviceName() const;
    // 服务器是否明确拒绝了本次登录（用户名或密码错误 / 未开通该服务等）。
    // 界面据此把"登录失败"留在登录窗口，而不是先进主界面再报错。
    bool authenticationRejected() const;
    std::string authenticationRejectionReason() const;
    // 客户端身份：账号会话标识（服务器实例 + 账号），历史结果据此丢弃过期数据。
    std::string sessionInstanceId() const;
    std::string storageBackend() const;
    // 实际使用的存储目录。由调用方传入（通常来自 imClientPathFanLX::clientDirectoryFanLX 派生），
    // 仅供展示与排查；界面不得据此反推账号归属，归属一律以 client-identity.json 校验为准。
    std::filesystem::path dataDirectory() const;
    std::size_t queuedSendFrames() const;
    bool databaseReady() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> implFanLX;
};
