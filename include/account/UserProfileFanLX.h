#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// UserProfileFanLX：从原领域聚合头文件拆出，保留已验证的业务接口。
//
// 密码字段是本次"账号库"改造的核心：一个用户一个密码，注册时由用户自选，
// 本人各平台账号（QQ / 微博共享号码，微信独立号码但与 QQ 绑定）共用同一个密码，
// 因此"任一服务登录 + 简单确认"就能联动其它服务，不需要每平台各记一套口令。
//
// 明文保存：课程范围内不做哈希 / 加盐 / 传输加密，见 docs/network-validation.md「假设与边界」。
struct UserProfileFanLX {
    std::string userId;    // 全局唯一用户 ID（如 U001）
    std::string nickname;  // 昵称
    std::string birthDate; // 出生日期
    std::string applyDate; // 注册 / 申请日期
    std::string location;  // 所在地
    std::string password;  // 登录密码（明文，用户自选；为空表示尚未设密，禁止登录）
};
