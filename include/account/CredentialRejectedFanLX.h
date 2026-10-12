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

// CredentialRejectedFanLX：用户名或密码不正确。
//
// 账号不存在、账号未设密码、密码不符三种情况统一用这一条措辞，调用方（控制台 / GUI /
// IM 服务端）因此无法从异常文本里区分"这个人不存在"和"密码打错了"。
struct CredentialRejectedFanLX : std::runtime_error {
    using std::runtime_error::runtime_error;
};
