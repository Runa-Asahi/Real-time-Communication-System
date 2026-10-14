// 口令哈希（PBKDF2-HMAC-SHA256，CNG）与旧版明文透明迁移的测试。
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "account/PasswordHashFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>

namespace fs = std::filesystem;
namespace {
std::string fixedTimeFanLX() { return "2026-09-14T12:00:00Z"; }
const AccountRefFanLX ownerWeiX{ServiceTypeFanLX::WeiX, "2026001"};
const AccountRefFanLX ownerQQ{ServiceTypeFanLX::QQ, "2026001"};

template <class FnFanLX> bool throwsFanLX(FnFanLX fn) {
    try {
        fn();
    } catch (const std::exception &) {
        return true;
    }
    return false;
}
fs::path tempDirFanLX(const std::string &tag) {
    std::random_device rd;
    auto dir = fs::temp_directory_path() / ("fanlx-pwhash-" + tag + "-" + std::to_string(rd()));
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}
// 数据目录下所有文件的文本拼接，用来断言磁盘上没有明文口令字段。
std::string allFilesFanLX(const fs::path &dir) {
    std::string all;
    for (const auto &e : fs::recursive_directory_iterator(dir))
        if (e.is_regular_file()) {
            std::ifstream in(e.path(), std::ios::binary);
            all.append(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
    return all;
}
std::string storedFanLX(const PersistenceApplicationFanLX &app, const std::string &userId) {
    return app.state().accounts.profile(userId).password;
}
void check(bool ok, const char *what) {
    std::cout << (ok ? "[ok] " : "[FAIL] ") << what << '\n';
    assert(ok);
}

void hasherBasicsFanLX() {
    namespace ph = passwordHashFanLX;
    const auto a = ph::hash("123"), b = ph::hash("123");
    check(ph::isHashed(a) && a.rfind("pbkdf2-sha256$120000$", 0) == 0, "哈希记录格式与默认迭代次数");
    check(a != b, "同一口令两次哈希的盐不同");
    check(ph::verify(a, "123") && ph::verify(b, "123"), "正确口令通过");
    check(!ph::verify(a, "1234") && !ph::verify(a, "") && !ph::verify(a, "12"), "错误口令被拒");
    check(!ph::needsRehash(a), "默认参数的哈希不需要重算");
    const auto weak = ph::hash("x", 1000);
    check(ph::verify(weak, "x") && ph::needsRehash(weak), "低迭代哈希可校验但标记为需要升级");
    check(throwsFanLX([] { ph::hash("x", 10); }), "迭代次数过低时拒绝生成");
    // 旧版明文与占位
    check(ph::verify("123", "123") && !ph::verify("123", "1234"), "旧版明文按明文比对");
    check(ph::needsRehash("123"), "旧版明文需要迁移");
    check(!ph::verify("", "") && !ph::verify(ph::remotePlaceholder(), ph::remotePlaceholder()),
          "空记录与远端占位永不放行");
    check(!ph::needsRehash(ph::remotePlaceholder()), "远端占位不迁移");
    // 畸形记录不被当作哈希（于是按明文处理，只有与记录逐字节相同的输入才能通过）
    auto upper = a;
    for (auto &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    check(!ph::isHashed(upper), "大写 hex 不是合法哈希");
    check(!ph::isHashed("pbkdf2-sha256$120000$00$00"), "盐/密钥长度不对不是合法哈希");
    check(!ph::isHashed(a + "$"), "多余分段不是合法哈希");
    check(!ph::isHashed("pbkdf2-sha256$99999999999$" + a.substr(a.find('$', 14) + 1)), "迭代次数越界不是合法哈希");
}

void domainFanLX() {
    AccountManagerFanLX m;
    m.registerWeiXAccount({"U1", "甲", "2005-01-01", "2026-09-01", "吉林", "secret"}, "9000001");
    const auto stored = m.profile("U1").password;
    check(passwordHashFanLX::isHashed(stored) && stored.find("secret") == std::string::npos, "注册只存哈希");
    check(m.verifyPassword(ServiceTypeFanLX::WeiX, "9000001", "secret"), "注册后口令可校验");
    check(!m.verifyPassword(ServiceTypeFanLX::WeiX, "9000001", stored), "拿哈希记录本身当口令不能登录");
    check(throwsFanLX([&] { m.registerWeiXAccount({"U2", "乙", "2005-01-01", "2026-09-01", "吉林", ""}, "9000002"); }),
          "空口令注册仍被拒");
    m.changePassword(ServiceTypeFanLX::WeiX, "9000001", "secret", "next");
    check(passwordHashFanLX::isHashed(m.profile("U1").password), "改密后只存哈希");
    check(!m.verifyPassword(ServiceTypeFanLX::WeiX, "9000001", "secret") &&
              m.verifyPassword(ServiceTypeFanLX::WeiX, "9000001", "next"),
          "改密后旧口令失效、新口令可用");
    m.importRemoteAccount(ServiceTypeFanLX::QQ, "7000001", "R1", "远端", "2026-09-01");
    check(!m.verifyPassword(ServiceTypeFanLX::QQ, "7000001", "<remote-account>"), "远端导入账号不能用占位串登录");
}

void migrationFanLX() {
    const auto dir = tempDirFanLX("migrate");
    // 模拟旧版数据：快照里的口令是明文 "123"。
    auto seed = demoSeedFanLX();
    bool allHashed = true;
    for (auto &u : seed.users) {
        allHashed = allHashed && passwordHashFanLX::isHashed(u.password);
        u.password = demoPasswordFanLX("");
    }
    check(allHashed, "演示种子口令已是哈希");
    {
        PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
        app.initialize();
        app.importInitial(seed);
        check(storedFanLX(app, "U1") == "123", "旧版明文数据可以导入");
        check(throwsFanLX([&] { app.login(ownerWeiX, "wrong"); }), "旧版明文：错误口令被拒");
        check(storedFanLX(app, "U1") == "123", "登录失败不触发迁移");
        app.login(ownerWeiX, "123");
        check(passwordHashFanLX::isHashed(storedFanLX(app, "U1")), "登录成功后内存中已迁移为哈希");
        check(storedFanLX(app, "U2") == "123", "未登录的用户不受影响");
        app.saveAndClose();
    }
    {
        PersistenceApplicationFanLX app(dir, fixedTimeFanLX); // 构造时从磁盘加载
        const auto stored = storedFanLX(app, "U1");
        check(passwordHashFanLX::isHashed(stored), "迁移结果已落盘");
        app.login(ownerQQ, "123"); // 下级服务号码同样按所属用户的口令校验
        check(storedFanLX(app, "U1") == stored, "已是哈希时登录不再改写");
        check(throwsFanLX([&] { app.login(ownerWeiX, "1234"); }), "迁移后错误口令仍被拒");
        app.saveAndClose();
    }
    fs::remove_all(dir);
}

void freshDataHasNoPlaintextFanLX() {
    const auto dir = tempDirFanLX("fresh");
    {
        PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
        app.initialize();
        app.importInitial(demoSeedFanLX());
        app.registerWeiXUser({"U99", "新人", "2005-01-01", "2026-09-01", "吉林", "pass-word-99"}, "2026099");
        app.login({ServiceTypeFanLX::WeiX, "2026099"}, "pass-word-99");
        app.login(ownerWeiX, demoPasswordFanLX("2026001")); // 演示账号 123 仍可登录
        app.saveAndClose();
    }
    const auto text = allFilesFanLX(dir);
    check(text.find("pass-word-99") == std::string::npos, "注册口令不以明文落盘");
    check(text.find("\"password\":\"123\"") == std::string::npos &&
              text.find("\"password\": \"123\"") == std::string::npos,
          "演示口令不以明文落盘");
    check(text.find("pbkdf2-sha256$") != std::string::npos, "磁盘上是哈希记录");
    fs::remove_all(dir);
}
} // namespace

int main() {
    hasherBasicsFanLX();
    domainFanLX();
    migrationFanLX();
    freshDataHasNoPlaintextFanLX();
    std::cout << "password hash tests passed\n";
    return 0;
}
