#pragma once
#include "group/GroupPolicyFanLX.h"
#include <stdexcept>

// 仅供 T10e 注入构造失败；生产代码不需要显式回滚分支。每类独立文件。
class ThrowingGroupPolicyFanLX final : public GroupPolicyFanLX {
  public:
    ThrowingGroupPolicyFanLX() { throw std::runtime_error("模拟策略构造失败"); }
    bool canCreateDiscussion() const override { return true; }
    std::string name() const override { return "Throwing"; }
    GroupModeFanLX mode() const override { return GroupModeFanLX::QQ; }
    bool canAssignAdmin() const override { return true; }
};
