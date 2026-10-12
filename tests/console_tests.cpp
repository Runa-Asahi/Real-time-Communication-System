#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ui/ConsoleAppFanLX.h"
#include <cassert>
#include <sstream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// 用注入的输入输出流测试真实菜单，不依赖 GUI 或人工按键。
int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("fanlx-console-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
    {
        PersistenceApplicationFanLX app(root);
        std::istringstream input(
            "1\n" // 空库显式初始化
            "xyz\n999999999999999999999999\n-1\n1junk\n" // 无效菜单数字不得改变状态
            "1\n1\n2026001\nwrong-pw\n0\n" // 密码错：登录必须被拒（微X号对也没用）
            "1\n1\n2026001\n123\n1\n" // 用演示数据的密码登录微X，确认联动本人已开通服务
            "2\n1\n0\n" // 本人资料，返回
            "3\n1\n4\n2026002\n菜单中文备注\n同学\n0\n" // 更新私有备注
            "4\n2\n1001\n14\n1001\n2\n0\n" // 查询与动态切换
            "2\n6\n单项昵称\n\n0\n" // 单项修改：只改昵称（所在地留空 = 不改）
            "5\n0\n");
        std::ostringstream output;
        assert(ConsoleAppFanLX(app, input, output).run() == 0);
        const auto text = output.str();
        assert(text.find("完整数字") != std::string::npos);
        assert(text.find("用户名或密码错误") != std::string::npos); // 错的密码没有被当成登录成功
        assert(text.find("登录成功") != std::string::npos);
        assert(text.find("T龄") != std::string::npos);
        assert(text.find("只改了填写的那一项") != std::string::npos); // 单项修改只提交填写的那一项
        assert(text.find("保存检查完成") != std::string::npos);
    }
    {
        PersistenceApplicationFanLX app(root);
        const auto snapshot = app.snapshot();
        assert(snapshot.groups.front().currentMode == GroupModeFanLX::Wechat);
        assert(snapshot.friendships.front().leftRemark == "菜单中文备注");
        assert(snapshot.users.front().nickname == "单项昵称"); // 只改了昵称，所在地没被清空
        assert(snapshot.users.front().location == "吉林");
        // 子菜单中途 EOF：已提交数据保留，退出不陷入读失败循环。
        std::istringstream input("1\n1\n2026001\n123\n0\n4\n19\n1001\n");
        std::ostringstream output;
        assert(ConsoleAppFanLX(app, input, output).run() == 0);
        assert(output.str().find("保存检查完成") != std::string::npos);
    }
    {
        PersistenceApplicationFanLX app(root / "cancel");
        std::istringstream input("0\n"); std::ostringstream output;
        assert(ConsoleAppFanLX(app, input, output).run() == 0);
        assert(!std::filesystem::exists(root / "cancel" / "state.json"));
    }
    {
        PersistenceApplicationFanLX app(root);
        // 非成员/未登录尝试查询，不应输出群成员列表。
        std::istringstream input("4\n2\n0\n0\n"); std::ostringstream output;
        assert(ConsoleAppFanLX(app, input, output).run() == 0);
        assert(output.str().find("请先登录") != std::string::npos);
        assert(output.str().find("查询结果") == std::string::npos);
    }
    {
        PersistenceApplicationFanLX app(root);
        // 聊天入口走同一主菜单：未登录拒绝、未配置服务器时明确报错，不产生业务写入。
        const auto before = SnapshotCodecFanLX::encode(app.snapshot());
        std::istringstream input("6\n1\n1\n2026001\n123\n1\n6\n0\n");
        std::ostringstream output;
        assert(ConsoleAppFanLX(app, input, output).run() == 0);
        assert(output.str().find("请先登录") != std::string::npos);
        assert(output.str().find("聊天") != std::string::npos);
        assert(output.str().find("--im-host") != std::string::npos);
        assert(output.str().find("--im-tokens") == std::string::npos); // 令牌文件入口已彻底移除
        assert(output.str().find("正在监听") == std::string::npos);
        assert(SnapshotCodecFanLX::encode(app.snapshot()) == before);
    }
    {
        // 注册入口在登录流程里（不在登录后的"账号与服务"菜单里），且注册只建微X账号。
        PersistenceApplicationFanLX app(root);
        const auto before = SnapshotCodecFanLX::encode(app.snapshot()).dump();
        std::istringstream input(
            "1\n2\n2026077\n注册同学\n2006-01-02\n长春\npw-2026077\npw-2026077\n" // 主菜单1 → 注册微X账号
            "1\n1\n2026077\npw-2026077\n1\n"                                    // 再用新微X号登录
            "2\n1\n0\n"                                                         // 本人资料与服务
            "2\n2\n1\n\n0\n"                                                    // 开通 QQ（回车用建议号码）
            "0\n");
        std::ostringstream output;
        assert(ConsoleAppFanLX(app, input, output).run() == 0);
        const auto text = output.str();
        assert(text.find("未捆绑任何下级服务") != std::string::npos);
        assert(text.find("已开通 QQ") != std::string::npos);
        // 账号与服务菜单里不再有"注册"这一项。
        assert(text.find("9 注册") == std::string::npos);
        const auto after = app.snapshot();
        bool registeredWeix = false, qqActivated = false, weiboActivated = false;
        for (const auto &account : after.accounts) {
            if (account.userId != "u-2026077")
                continue;
            registeredWeix = registeredWeix || account.service == ServiceTypeFanLX::WeiX;
            qqActivated = qqActivated || account.service == ServiceTypeFanLX::QQ;
            weiboActivated = weiboActivated || account.service == ServiceTypeFanLX::Weibo;
        }
        assert(registeredWeix && qqActivated);
        assert(!weiboActivated); // 注册不捆绑服务：微博要用户自己开通
        assert(SnapshotCodecFanLX::encode(after).dump() != before);
    }
    std::filesystem::remove_all(root);
}