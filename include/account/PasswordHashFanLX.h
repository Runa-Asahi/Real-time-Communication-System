#pragma once
#include <string>

// 口令存储格式（PBKDF2-HMAC-SHA256，每用户随机盐）：
//   pbkdf2-sha256$<迭代次数>$<盐 hex>$<派生密钥 hex>
// 实现走 Windows 自带的 CNG（bcrypt.dll：BCryptGenRandom + BCryptDeriveKeyPBKDF2），
// 不引入第三方密码库；本项目本来就只面向 Windows（ws2_32 / winsqlite3）。
//
// 兼容旧数据：账号库里不是上述格式的口令一律视为旧版明文，校验时按明文比对，
// 并在一次成功登录后由持久化层透明地改写为哈希（见 PersistenceApplicationFanLX::login）。
namespace passwordHashFanLX {

// 新哈希使用的迭代次数。改大只影响之后写入的哈希；旧哈希按其自带的次数校验。
constexpr unsigned long defaultIterations = 120000;
constexpr unsigned long minIterations = 1000;
constexpr unsigned long maxIterations = 10000000;

// 远端账号导入时写入的占位口令（见 AccountManagerFanLX::importRemoteAccount），永远不得放行。
inline const char *remotePlaceholder() { return "<remote-account>"; }

// stored 是否是格式完整的哈希记录（前缀、迭代次数范围、盐与密钥长度都合法）。
bool isHashed(const std::string &stored);

// 用新随机盐生成哈希记录；CNG 失败时抛 std::runtime_error（不会退回明文）。
std::string hash(const std::string &password, unsigned long iterations = defaultIterations);

// 校验口令：stored 为哈希记录时按 PBKDF2 常量时间比对；为旧版明文时按常量时间明文比对；
// 空记录与远端占位记录一律 false。
bool verify(const std::string &stored, const std::string &password);

// stored 是否需要在下次成功登录后改写（旧版明文或迭代次数低于当前默认值）。
bool needsRehash(const std::string &stored);

} // namespace passwordHashFanLX
