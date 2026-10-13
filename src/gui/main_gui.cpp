#include "gui/ChatControllerFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "gui/LoginDialogFanLX.h"
#include "gui/MainWindowFanLX.h"
#include "account/ServiceTypeFanLX.h"
#include "network/ImClientFanLX.h"
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QString>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

// 控制台窗口是否对用户可见（由 prepareConsoleFanLX 设定），供出错时决定打印还是弹消息框。
bool consoleVisibleFanLX = true;

// 双击运行时 Windows 会先为控制台子系统程序分配一个控制台窗口，这里在启动最早处把它隐藏。
//
// 为什么用"隐藏"而不是把目标改成 Windows 子系统：后者需要入口点 WinMain，而 Qt 在本工具链下
// 提供的 libQt6EntryPoint.a 链接不过（undefined reference to `__imp___argc'）。隐藏窗口不需要
// 改入口点，exe 仍按已验证的控制台子系统链接，代价是双击瞬间可能闪一下窗口。
//
// 关键判断：从已有终端启动时，我们与该终端**共享**控制台（GetConsoleProcessList 会返回多个
// 进程），此时绝不能隐藏，否则会把用户正在用的终端窗口藏掉。
void prepareConsoleFanLX() {
    const HWND window = GetConsoleWindow();
    if (!window) {
        consoleVisibleFanLX = false; // 无控制台（例如由 GUI 进程启动）
        return;
    }
    DWORD owners[2] = {0, 0};
    const DWORD processCount = GetConsoleProcessList(owners, 2);
    // 本程序的中文输出是 UTF-8，而控制台默认代码页通常是 GBK，不切换就会显示成乱码。
    // 与 main.cpp 里的 fanlx_app 保持一致（它也是启动时切到 CP_UTF8）。
    (void)SetConsoleOutputCP(CP_UTF8);
    if (processCount != 1) {
        consoleVisibleFanLX = true; // 与父进程共享：保持可见，--help 与参数错误仍看得见
        return;
    }
    // 独自拥有控制台：双击，或由启动脚本 Start-Process -WindowStyle Normal 拉起。
    // 只 ShowWindow(SW_HIDE) 会输给竞态——conhost 随后按 STARTUPINFO 再把窗口显示出来，
    // 于是每登录一个账号就多一个空的黑色命令行，而且一直留着。
    // FreeConsole 把本进程从控制台摘掉，窗口随最后一个附着进程离开而关闭，不能再被显示回来。
    ShowWindow(window, SW_HIDE);
    FreeConsole();
    consoleVisibleFanLX = false;
}

// 启动期的致命错误必须两种启动方式都看得见：控制台可见就打印，否则弹消息框，
// 避免双击（或双击带参数的快捷方式）时无声退出，用户无从判断原因。
void reportStartupErrorFanLX(const QString &text) {
    std::cerr << text.toStdString() << "\n";
    if (!consoleVisibleFanLX)
        MessageBoxW(nullptr, text.toStdWString().c_str(), L"fanlx_gui 启动失败",
                    MB_OK | MB_ICONERROR);
}

// 非致命问题（例如领域库不可用，但聊天仍能连上）：仍然要让人看见，但不阻止进入界面。
void reportStartupWarningFanLX(const QString &text) {
    std::cerr << "警告：" << text.toStdString() << "\n";
    if (!consoleVisibleFanLX)
        MessageBoxW(nullptr, text.toStdWString().c_str(), L"fanlx_gui 提示",
                    MB_OK | MB_ICONWARNING);
}

// 安全网：槽函数里逼出来的 C++ 异常如果一路逃进事件循环，Qt 会直接 terminate（表现为界面卡死后闪退）。
// 在事件分发入口拦住它，弹窗告知原因并继续运行，而不是让整个程序退出。
// 已知的异常来源（注销后的查询）已在控制器里修掉，这里只是防止同类问题再次把进程带走。
class GuardedApplicationFanLX : public QApplication {
  public:
    using QApplication::QApplication;
    bool notify(QObject *receiver, QEvent *event) override {
        try {
            return QApplication::notify(receiver, event);
        } catch (const std::exception &error) {
            report(QString::fromUtf8(error.what()));
        } catch (...) {
            report(QStringLiteral("未知错误"));
        }
        return false;
    }

  private:
    static void report(const QString &what) {
        std::cerr << "界面操作失败：" << what.toStdString() << "\n";
        QMessageBox::warning(nullptr, QStringLiteral("操作失败"),
                             QStringLiteral("这一步操作没有完成：%1\n程序仍可继续使用。").arg(what));
    }
};

} // namespace

// fanlx_gui：可视化 QQ 客户端。
//
// 参数：
//   --host IPv4 --port 端口 --account 微X号 --password 登录密码
//   --data-root 数据根目录（默认 data-im-gui）
//   --json-storage 使用旧 JSON 存储（默认 SQLite 聊天库）
//   --auto-login 参数齐全时跳过登录窗口（用于脚本化演示与截图）
//
// 认证就是领域账号库里的「微X号 + 密码」，没有令牌文件：密码在登录窗口里输入，
// 或（仅脚本化演示时）用 --password 传入。
//
// 存储目录约定：用户不选目录。真实目录由
//   <数据根>/im-client/<服务器地址>_<端口>/<登录账号>
// 派生（见 network/ClientDataPathFanLX.h）；--data-root 只覆盖"根"，不指到具体账号目录。
//
// 线程约定：窗口与控制器只运行在 Qt 主线程；网络与 SQLite 都在 ImClientFanLX
// 内部的网络线程与数据库线程中执行，界面不执行任何阻塞操作。
int main(int argc, char **argv) {
    // 必须在任何输出与窗口创建之前：双击启动时先把自己独占的控制台窗口藏掉。
    prepareConsoleFanLX();
    GuardedApplicationFanLX app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("fanlx_gui"));
    QApplication::setApplicationVersion(QStringLiteral("1.0"));

    QString host = QStringLiteral("127.0.0.1");
    QString portText = QStringLiteral("9500");
    QString account, password, dataRoot;
    QString nickname, birthDate, location;
    bool useSqlite = true;
    bool autoLogin = false;
    bool useDomain = true;
    bool confirmOthers = true;
    bool registerOnly = false;
    bool activateOnly = false;
    QString activateService, serviceNumber;
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        const auto takeValue = [&](QString &target) {
            if (index + 1 < argc)
                target = QString::fromLocal8Bit(argv[++index]);
        };
        if (argument == QStringLiteral("--host")) takeValue(host);
        else if (argument == QStringLiteral("--port")) takeValue(portText);
        else if (argument == QStringLiteral("--account")) takeValue(account);
        else if (argument == QStringLiteral("--password")) takeValue(password);
        else if (argument == QStringLiteral("--data-root")) takeValue(dataRoot);
        // --register：只做"注册微X账号"（本机领域库 + 服务端登记）后退出，不开窗口。
        // 与登录窗口里的「注册微X账号…」是同一条路径，用于脚本化演示与回归验证。
        else if (argument == QStringLiteral("--register")) registerOnly = true;
        // --activate <QQ|微信|微博>：登录后开通该下级服务（顺带登记到服务端）后退出。
        // 与「账号与服务 → 开通管理 → 开通所选服务」是同一条路径，供无人值守演示使用。
        else if (argument == QStringLiteral("--activate")) {
            takeValue(activateService);
            activateOnly = true;
        } else if (argument == QStringLiteral("--service-number")) takeValue(serviceNumber);
        else if (argument == QStringLiteral("--nickname")) takeValue(nickname);
        else if (argument == QStringLiteral("--birth-date")) takeValue(birthDate);
        else if (argument == QStringLiteral("--location")) takeValue(location);
        else if (argument == QStringLiteral("--dir")) {
            // 兼容旧写法。语义已从"账号数据目录"改为"数据根目录"：
            // 继续传 data-gui\a 这类账号目录仍然可用（会被当成根），只是不再必要。
            takeValue(dataRoot);
            std::cerr << "提示：--dir 已更名为 --data-root；账号子目录由登录账号自动派生。\n";
        } else if (argument == QStringLiteral("--json-storage")) useSqlite = false;
        else if (argument == QStringLiteral("--auto-login")) autoLogin = true;
        // --skip-domain：只开聊天页签（账号/好友/群页面来自领域库，用于对比排查）。
        else if (argument == QStringLiteral("--skip-domain")) useDomain = false;
        // --no-confirm-others：只登录当前平台，不联动本人其它已开通服务。
        else if (argument == QStringLiteral("--no-confirm-others")) confirmOthers = false;
        else if (argument == QStringLiteral("--help") || argument == QStringLiteral("-h")) {
            std::cout << "用法: fanlx_gui [--host IPv4] [--port 端口] [--account 微X号或服务号] [--password 登录密码]\n"
                         "                [--data-root 数据根目录] [--json-storage] [--auto-login]\n"
                         "                [--skip-domain] [--no-confirm-others]\n"
                         "                [--register [--nickname 昵称] [--birth-date YYYY-MM-DD]\n"
                         "                            [--location 所在地]]\n"
                         "登录凭据是领域账号库里的微X号（也兼容已开通服务号）与用户自选密码，没有令牌文件。\n"
                         "存储目录由账号派生：<数据根>/im-client/<host>_<port>/<账号>\n"
                         "领域库（账号/好友/群）在上一层的 domain 子目录，首次启动写入演示数据。\n"
                         "--register 只注册（本机 + 服务端各登记一次）后退出，不开窗口；\n"
                         "新注册的微X号还没有下级服务，登录后可到「账号与服务 → 开通管理」开通 QQ/微信/微博。\n";
            return 0;
        } else {
            reportStartupErrorFanLX(QStringLiteral("未知参数：") + argument);
            return 2;
        }
    }
    bool portOk = false;
    const auto port = portText.toUShort(&portOk);
    if (!portOk || port == 0) {
        reportStartupErrorFanLX(QStringLiteral("端口非法：") + portText);
        return 2;
    }
    if (dataRoot.isEmpty())
        dataRoot = QDir::toNativeSeparators(QDir::current().filePath(QStringLiteral("data-im-gui")));

    // --register：与登录窗口里的「注册微X账号…」完全同一条两步走：
    //   ① 本机领域库：<数据根>/im-client/<host>_<port>/<微X号>/domain（资料与好友/群）；
    //   ② 服务端登记：IM 的 REGISTER 把账号写进服务端持久化账号库（聊天认证查的是那份）。
    // 注册**不捆绑**任何下级服务：QQ/微信/微博 由本人登录后在「开通管理」里自主开通。
    if (registerOnly) {
        if (account.isEmpty() || password.isEmpty()) {
            reportStartupErrorFanLX(QStringLiteral("--register 需要 --account（微X号）与 --password"));
            return 2;
        }
        QString detail;
        if (!fanlxGui::DomainControllerFanLX::registerWeiXUser(
                dataRoot, host, port, account, nickname.isEmpty() ? account : nickname,
                birthDate.isEmpty() ? QStringLiteral("2000-01-01") : birthDate, location, password,
                &detail)) {
            reportStartupErrorFanLX(QStringLiteral("本机注册失败：") + detail);
            return 1;
        }
        std::cout << "本机注册成功：微X号 " << account.toStdString() << "（未捆绑任何下级服务）\n";
        std::string reason;
        const auto pushed = ImClientFanLX::registerAccountFanLX(
            host.toStdString(), port, account.toStdString(), password.toStdString(),
            nickname.toStdString(), birthDate.toStdString(), location.toStdString(), reason);
        std::cout << (pushed ? "服务端登记成功：" : "服务端登记失败：") << reason << '\n';
        std::cout << "下一步：用该微X号登录，然后到「账号与服务 → 开通管理」开通 QQ / 微信 / 微博。\n";
        return pushed ? 0 : 1;
    }

    // 登录 → 主窗口。登录失败时窗口不打开，输入内容与配置保留以便重试。
    std::unique_ptr<fanlxGui::ChatControllerFanLX> controller;
    std::unique_ptr<fanlxGui::DomainControllerFanLX> domain;
    std::unique_ptr<fanlxGui::MainWindowFanLX> window;
    const auto beginSession = [&](const QString &hostValue, quint16 portValue, const QString &accountValue,
                                 const QString &passwordValue, const QString &dataRootValue, bool sqlite,
                                 bool confirmLinked) {
        // 领域登录先做：登录身份只有微X号（QQ/微信/微博 是登录后开通的下级服务）。
        // 身份被拒（微X号不存在 / 密码错）必须留在这里返回失败——调用方据此留在登录界面；
        // 只有"领域库本身不可用"才降级为"仅聊天"，而不是把没登录成功的会话放进主界面。
        QString domainDetail;
        if (useDomain) {
            domain = std::make_unique<fanlxGui::DomainControllerFanLX>();
            const auto outcome = domain->openWithOutcome(dataRootValue, hostValue, portValue,
                                                        accountValue, passwordValue, confirmLinked,
                                                        &domainDetail);
            if (outcome == fanlxGui::DomainControllerFanLX::OpenOutcomeFanLX::IdentityRejected) {
                domain.reset();
                return domainDetail;
            }
            if (outcome == fanlxGui::DomainControllerFanLX::OpenOutcomeFanLX::Unavailable) {
                // 库坏了不该以"密码错"的名义挡住登录：明确告知并降级为只有聊天页签。
                reportStartupWarningFanLX(QStringLiteral("账号/好友/群功能不可用：") + domainDetail);
                domain.reset();
            }
        }
        // 聊天连接按"当前服务"建立：默认是微X；本人已开通并登录了 QQ/微信/微博 时用该服务的号码建连。
        QString chatService = QStringLiteral("QQ");
        QString chatAccount = accountValue;
        if (domain && domain->opened()) {
            // 优先"已开通且已登录"的服务；没有就退到"已开通"的——刚开通、还没点逐项确认登录时
            // 也应当能连上聊天（凭据是服务账号 + 密码，由服务端独立认证）。
            auto preferred = domain->chatServices();
            if (preferred.isEmpty())
                preferred = domain->subscribedChatServices();
            if (!preferred.isEmpty()) {
                chatService = preferred.first();
                chatAccount = domain->accountIdOf(chatService);
            }
        }
        // 「注册完进不去」的死结：新注册的微X号没有任何下级服务（注册**不捆绑**服务账号），
        // 因此它不存在可用的聊天通道——但这不是登录失败，身份已经认证通过（领域登录成功）。
        // 这里必须放行进主界面，否则用户永远到不了「开通管理」，也就永远开不了聊天通道。
        // 做法：不建聊天连接（用微X号去连只会被服务端拒），把聊天页置为未连接并给出开通指引；
        // 用户在账号与服务页开通并登录后，identityChanged 会触发聊天页自动重连。
        if (domain && domain->opened() && !domain->hasChatChannel()) {
            controller = std::make_unique<fanlxGui::ChatControllerFanLX>();
            controller->configure(hostValue, portValue, accountValue, passwordValue, dataRootValue,
                                  sqlite, chatService);
            window = std::make_unique<fanlxGui::MainWindowFanLX>(controller.get(), domain.get(),
                                                                accountValue, QStringLiteral("未连接"));
            window->showNoticeFanLX(domain->chatChannelHint());
            window->show();
            return QString();
        }
        controller = std::make_unique<fanlxGui::ChatControllerFanLX>();
        controller->configure(hostValue, portValue, chatAccount, passwordValue, dataRootValue, sqlite,
                              chatService);
        QString detail;
        if (!controller->start(&detail)) {
            controller.reset();
            if (domain)
                domain->saveAndClose();
            return detail;
        }
        // 连上不等于登录成功：服务器可能拒绝凭据。这里等一次认证结果，
        // 失败就整体回退（不留一个"连上了但没登录"的界面）。
        if (!controller->waitForLogin(6000, &detail)) {
            // 「注册完登不上」的常见原因：账号只写在本机领域库，服务端的持久化账号库里还没有它
            // （或它还没有在服务端开通该服务）。用刚刚输入的同一份凭据自动登记一次，再重试登录；
            // 服务端是"读穿持久化存储"的，因此**不需要重启服务端**。
            // 领域控制器知道本机的账号/服务/好友与服务端地址，由它统一登记（幂等）。
            QString why;
            const auto registered = domain && domain->opened() && domain->pushAccountToServer(&why);
            if (registered) {
                controller->stop();
                QString retryDetail;
                if (controller->start(&retryDetail) && controller->waitForLogin(6000, &detail)) {
                    window = std::make_unique<fanlxGui::MainWindowFanLX>(
                        controller.get(), domain.get(), accountValue, controller->storageBackend());
                    window->show();
                    return QString();
                }
                if (detail.isEmpty())
                    detail = retryDetail;
            }
            controller.reset();
            if (domain)
                domain->saveAndClose();
            return registered
                       ? detail
                       : detail + QStringLiteral("；已尝试把该账号登记到服务端但未成功：") + why;
        }
        window = std::make_unique<fanlxGui::MainWindowFanLX>(controller.get(), domain.get(), accountValue,
                                                            controller->storageBackend());
        window->show();
        return QString();
    };

    // --activate：本地登录微X → 开通所选下级服务（领域层只允许微X会话开通）→ 自动登记到服务端。
    if (activateOnly) {
        if (account.isEmpty() || password.isEmpty() || activateService.isEmpty()) {
            reportStartupErrorFanLX(QStringLiteral("--activate 需要 --account、--password 与服务名"
                                                   "（如 --activate QQ）"));
            return 2;
        }
        fanlxGui::DomainControllerFanLX ctl;
        QString detail;
        const auto outcome =
            ctl.openWithOutcome(dataRoot, host, port, account, password, true, &detail);
        if (outcome != fanlxGui::DomainControllerFanLX::OpenOutcomeFanLX::Ok) {
            reportStartupErrorFanLX(QStringLiteral("登录失败：") + detail);
            return 1;
        }
        const auto number = serviceNumber.isEmpty() ? account : serviceNumber;
        if (!ctl.activateService(activateService, number)) {
            reportStartupErrorFanLX(QStringLiteral("开通失败：") + ctl.lastMessage());
            ctl.saveAndClose();
            return 1;
        }
        std::cout << "已开通 " << activateService.toStdString() << "（号码 " << number.toStdString()
                  << "）；" << ctl.lastMessage().toStdString() << '\n';
        ctl.saveAndClose();
        return 0;
    }
    if (autoLogin && !account.isEmpty() && !password.isEmpty()) {
        const auto failure =
            beginSession(host, port, account, password, dataRoot, useSqlite, confirmOthers);
        if (!failure.isEmpty()) {
            reportStartupErrorFanLX(QStringLiteral("自动登录失败：") + failure);
            return 1;
        }
        const auto code = app.exec();
        // 退出时收尾提交并写回文件（窗口 closeEvent 已做过一次，这里兜住未走正常关窗的路径）。
        if (domain)
            domain->saveAndClose();
        return code;
    }

    fanlxGui::LoginDialogFanLX login;
    login.prefill(host, portText, account, password, dataRoot, useSqlite);
    QObject::connect(&login, &fanlxGui::LoginDialogFanLX::submittedFanLX, [&] {
        const auto failure = beginSession(login.host(), login.port(), login.account(), login.password(),
                                          login.dataRoot(), login.useChatDatabase(),
                                          login.confirmOthers());
        if (!failure.isEmpty()) {
            login.showFailure(failure);
            return;
        }
        login.accept();
    });
    if (login.exec() != QDialog::Accepted)
        return 0;
    const auto code = app.exec();
    if (domain)
        domain->saveAndClose();
    return code;
}
