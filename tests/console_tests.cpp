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
            "1\n1\n2026001\n1\n" // 登录 QQ，确认已绑定平台
            "2\n1\n0\n" // 本人资料，返回
            "3\n1\n4\n2026002\n菜单中文备注\n同学\n0\n" // 更新私有备注
            "4\n2\n1001\n14\n1001\n2\n0\n" // 查询与动态切换
            "5\n0\n");
        std::ostringstream output;
        assert(ConsoleAppFanLX(app, input, output).run() == 0);
        const auto text = output.str();
        assert(text.find("完整数字") != std::string::npos);
        assert(text.find("登录成功") != std::string::npos);
        assert(text.find("T龄") != std::string::npos);
        assert(text.find("保存检查完成") != std::string::npos);
    }
    {
        PersistenceApplicationFanLX app(root);
        const auto snapshot = app.snapshot();
        assert(snapshot.groups.front().currentMode == GroupModeFanLX::Wechat);
        assert(snapshot.friendships.front().leftRemark == "菜单中文备注");
        // 子菜单中途 EOF：已提交数据保留，退出不陷入读失败循环。
        std::istringstream input("1\n1\n2026001\n0\n4\n19\n1001\n");
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
        // QQ IM 入口走同一主菜单：未登录拒绝、未配置服务器时明确报错，不产生业务写入。
        const auto before = SnapshotCodecFanLX::encode(app.snapshot());
        std::istringstream input("6\n1\n1\n2026001\n0\n6\n0\n");
        std::ostringstream output;
        assert(ConsoleAppFanLX(app, input, output).run() == 0);
        assert(output.str().find("请先登录") != std::string::npos);
        assert(output.str().find("QQ 聊天") != std::string::npos);
        assert(output.str().find("--im-host") != std::string::npos);
        assert(output.str().find("正在监听") == std::string::npos);
        assert(SnapshotCodecFanLX::encode(app.snapshot()) == before);
    }
    std::filesystem::remove_all(root);
}
