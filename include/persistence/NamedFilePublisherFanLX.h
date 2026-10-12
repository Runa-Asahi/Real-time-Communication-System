#pragma once
#include <filesystem>
#include <string>

// 具名文件的原子发布：供"身份/标记"这类固定文件名使用。
//
// 为什么不直接用 AtomicFileWriterFanLX::publish：那个接口按平台快照约定固定发布为
// state.json（含备份语义），用它发布 client-identity.json 会意外覆盖 IM 的 state.json ——
// 这是实测踩到过的缺陷。这里只做"临时文件 + 同卷原子替换"，不改动目标目录里其它文件。
class NamedFilePublisherFanLX {
  public:
    static void publish(const std::filesystem::path &file, const std::string &bytes);
    // 目标已存在时返回其内容；不存在返回空字符串。
    static std::string readIfExists(const std::filesystem::path &file);
};
