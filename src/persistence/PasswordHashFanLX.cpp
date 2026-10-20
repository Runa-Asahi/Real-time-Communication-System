#include "account/PasswordHashFanLX.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace passwordHashFanLX {
namespace {

constexpr const char *prefixFanLX = "pbkdf2-sha256$";
constexpr std::size_t saltBytesFanLX = 16;
constexpr std::size_t keyBytesFanLX = 32;

std::string toHexFanLX(const std::vector<unsigned char> &bytes) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) {
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0x0f]);
    }
    return out;
}

bool fromHexFanLX(const std::string &hex, std::vector<unsigned char> &out) {
    if (hex.size() % 2 != 0)
        return false;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1; // 只认小写：我们写出的就是小写，拒绝其他写法让格式保持唯一
    };
    out.clear();
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = nibble(hex[i]), lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out.push_back(static_cast<unsigned char>((hi << 4) | lo));
    }
    return true;
}

struct ParsedFanLX {
    unsigned long iterations = 0;
    std::vector<unsigned char> salt, key;
};

bool parseFanLX(const std::string &stored, ParsedFanLX &p) {
    const std::string prefix = prefixFanLX;
    if (stored.compare(0, prefix.size(), prefix) != 0)
        return false;
    const auto a = prefix.size();
    const auto b = stored.find('$', a);
    if (b == std::string::npos)
        return false;
    const auto c = stored.find('$', b + 1);
    if (c == std::string::npos || stored.find('$', c + 1) != std::string::npos)
        return false;
    const auto iter = stored.substr(a, b - a);
    if (iter.empty() || iter.size() > 9)
        return false;
    unsigned long n = 0;
    for (char ch : iter) {
        if (ch < '0' || ch > '9')
            return false;
        n = n * 10 + static_cast<unsigned long>(ch - '0');
    }
    if (n < minIterations || n > maxIterations)
        return false;
    if (!fromHexFanLX(stored.substr(b + 1, c - b - 1), p.salt) || p.salt.size() != saltBytesFanLX)
        return false;
    if (!fromHexFanLX(stored.substr(c + 1), p.key) || p.key.size() != keyBytesFanLX)
        return false;
    p.iterations = n;
    return true;
}

// 常量时间比较：长度不同也走完整循环，避免按前缀逐字节泄露时序。
bool equalConstantTimeFanLX(const unsigned char *x, std::size_t nx, const unsigned char *y, std::size_t ny) {
    const std::size_t n = nx > ny ? nx : ny;
    unsigned char diff = static_cast<unsigned char>(nx != ny);
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char cx = i < nx ? x[i] : 0, cy = i < ny ? y[i] : 0;
        diff = static_cast<unsigned char>(diff | (cx ^ cy));
    }
    return diff == 0;
}

class AlgFanLX {
  public:
    AlgFanLX() {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&handleFanLX, BCRYPT_SHA256_ALGORITHM, nullptr,
                                                        BCRYPT_ALG_HANDLE_HMAC_FLAG)))
            throw std::runtime_error("无法打开 CNG HMAC-SHA256 算法");
    }
    ~AlgFanLX() { BCryptCloseAlgorithmProvider(handleFanLX, 0); }
    AlgFanLX(const AlgFanLX &) = delete;
    AlgFanLX &operator=(const AlgFanLX &) = delete;
    BCRYPT_ALG_HANDLE get() const { return handleFanLX; }

  private:
    BCRYPT_ALG_HANDLE handleFanLX = nullptr;
};

std::vector<unsigned char> deriveFanLX(const std::string &password, const std::vector<unsigned char> &salt,
                                       unsigned long iterations) {
    AlgFanLX alg;
    std::vector<unsigned char> key(keyBytesFanLX);
    // CNG 的入参不是 const 指针，但不会写入口令与盐。
    auto *pw = reinterpret_cast<PUCHAR>(const_cast<char *>(password.data()));
    auto *sp = const_cast<PUCHAR>(salt.data());
    if (!BCRYPT_SUCCESS(BCryptDeriveKeyPBKDF2(alg.get(), pw, static_cast<ULONG>(password.size()), sp,
                                              static_cast<ULONG>(salt.size()), iterations, key.data(),
                                              static_cast<ULONG>(key.size()), 0)))
        throw std::runtime_error("PBKDF2 派生失败");
    return key;
}

} // namespace

bool isHashed(const std::string &stored) {
    ParsedFanLX p;
    return parseFanLX(stored, p);
}

std::string hash(const std::string &password, unsigned long iterations) {
    if (iterations < minIterations || iterations > maxIterations)
        throw std::invalid_argument("PBKDF2 迭代次数超出范围");
    if (password.size() > 4096)
        throw std::runtime_error("密码超过 4096 字节");
    std::vector<unsigned char> salt(saltBytesFanLX);
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, salt.data(), static_cast<ULONG>(salt.size()),
                                        BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        throw std::runtime_error("无法生成随机盐");
    const auto key = deriveFanLX(password, salt, iterations);
    return std::string(prefixFanLX) + std::to_string(iterations) + "$" + toHexFanLX(salt) + "$" + toHexFanLX(key);
}

bool verify(const std::string &stored, const std::string &password) {
    if (stored.empty() || stored == remotePlaceholder())
        return false;
    ParsedFanLX p;
    if (parseFanLX(stored, p)) {
        if (password.size() > 4096)
            return false;
        try {
            const auto key = deriveFanLX(password, p.salt, p.iterations);
            return equalConstantTimeFanLX(key.data(), key.size(), p.key.data(), p.key.size());
        } catch (const std::exception &) {
            return false; // 校验接口约定只回 true/false
        }
    }
    // 旧版明文记录：保持原有比对语义（逐字节相等），只是改成常量时间。
    return equalConstantTimeFanLX(reinterpret_cast<const unsigned char *>(stored.data()), stored.size(),
                                  reinterpret_cast<const unsigned char *>(password.data()), password.size());
}

bool needsRehash(const std::string &stored) {
    if (stored.empty() || stored == remotePlaceholder())
        return false;
    ParsedFanLX p;
    if (!parseFanLX(stored, p))
        return true;
    return p.iterations < defaultIterations;
}

} // namespace passwordHashFanLX
