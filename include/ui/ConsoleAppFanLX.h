#pragma once
#include "network/ImClientFanLX.h"
#include "network/TcpPeerFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include <istream>
#include <ostream>
#include <optional>

// 控制台只收集输入和展示结果；持久化写操作全部委托应用事务门面。
class ConsoleAppFanLX {
    PersistenceApplicationFanLX &appFanLX;
    std::istream &inFanLX;
    std::ostream &outFanLX;
    std::optional<AccountRefFanLX> actorFanLX;
    bool eofFanLX = false;
    std::string imHostFanLX;
    unsigned short imPortFanLX = 0;
    std::filesystem::path imDataRootFanLX;
    bool imUseChatDbFanLX = false;
    // 本次登录用的密码：领域登录与随后的 IM 连接用的是同一份凭据，因此在这里留存一份，
    // 避免"登录菜单输一次、进聊天又输一次"。退出登录或切换账号时清空。
    std::string imPasswordFanLX;
    std::unique_ptr<ImClientFanLX> imFanLX;
    std::string fieldFanLX(const char *prompt, bool allowEmpty = false);
    int choiceFanLX(const char *prompt, int max);
    ServiceTypeFanLX serviceFanLX();
    AccountRefFanLX accountFanLX();
    AccountRefFanLX currentFanLX() const;
    GroupKeyFanLX groupFanLX();
    void showFanLX(const std::vector<std::string> &values);
    // 登录微X / 注册微X账号（注册入口就在登录流程里，不在登录后的账号菜单里）。
    void loginFlowFanLX();
    // 切换"以哪个身份操作"：微X（上层，可开通服务）或本人某个已登录的下级服务。
    void switchCurrentServiceFanLX();
    // 本人已开通且已登录、可直接聊天的服务（QQ 优先于微信）；没有则返回空。
    AccountRefFanLX preferredChatServiceFanLX() const;
    // 按当前 --im-host / --im-port 与已留存的密码建立 IM 长连接（登录/切换服务时调用）。
    void openImClientFanLX(const AccountRefFanLX &actor);
    void accountsFanLX();
    void friendsFanLX();
    void groupsFanLX();
    void chatFanLX();
  public:
    ConsoleAppFanLX(PersistenceApplicationFanLX &app, std::istream &in, std::ostream &out)
        : appFanLX(app), inFanLX(in), outFanLX(out) {}
    int run();
    // dataRoot 是"数据根"，不是账号目录：真实存储目录由服务器身份 + 登录账号派生
    // （imClientPathFanLX::clientDirectoryFanLX），因此同一根目录可承载多个账号。
    // 认证不再需要令牌文件：用户名 + 用户自选密码在登录菜单里输入。
    void configureIm(std::string host, unsigned short port, std::filesystem::path dataRoot,
                     bool useChatDatabase = false) {
        imHostFanLX = std::move(host); imPortFanLX = port;
        imDataRootFanLX = std::move(dataRoot);
        imUseChatDbFanLX = useChatDatabase;
    }
};
