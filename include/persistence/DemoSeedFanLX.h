#pragma once
#include "persistence/PlatformSnapshotFanLX.h"
#include <string>
// 确定性初始数据仅用于显式初始化/隔离演示，不覆盖用户默认数据。
PlatformSnapshotFanLX demoSeedFanLX();
// 演示数据的密码规则（pw-<QQ号>）。测试、脚本与文档都引用这一处，避免各处手抄口令写歪。
std::string demoPasswordFanLX(const std::string &number);
