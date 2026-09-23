#pragma once
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
    std::filesystem::path imTokensFanLX, imDirectoryFanLX;
    std::unique_ptr<ImClientFanLX> imFanLX;
    std::string fieldFanLX(const char *prompt, bool allowEmpty = false);
    int choiceFanLX(const char *prompt, int max);
    ServiceTypeFanLX serviceFanLX();
    AccountRefFanLX accountFanLX();
    AccountRefFanLX currentFanLX() const;
    GroupKeyFanLX groupFanLX();
    void showFanLX(const std::vector<std::string> &values);
    void accountsFanLX();
    void friendsFanLX();
    void groupsFanLX();
    void chatFanLX();
  public:
    ConsoleAppFanLX(PersistenceApplicationFanLX &app, std::istream &in, std::ostream &out)
        : appFanLX(app), inFanLX(in), outFanLX(out) {}
    int run();
    void configureIm(std::string host, unsigned short port, std::filesystem::path tokens,
                     std::filesystem::path directory) {
        imHostFanLX = std::move(host); imPortFanLX = port;
        imTokensFanLX = std::move(tokens); imDirectoryFanLX = std::move(directory);
    }
};
