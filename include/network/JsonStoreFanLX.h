#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

// JsonStoreFanLX：服务端"小型持久化表"的抽象基类（模板方法模式）。
//
// 好友申请簿与临时会话额度都是"一张内存表 + 一个 JSON 文件"：
//   - load() 读文件 → 校验 schema → 交给派生类 fromJsonFanLX 重建内存表；
//   - save() 由派生类 toJsonFanLX 生成内容 → 原子发布（临时文件 + 同卷替换），断电不会留下半个文件。
// 读写流程只在基类写一次，派生类只负责"自己的数据长什么样"（多态：两个派生类各自实现）。
//
// 线程约定：与 ImServerFanLX 相同，只在服务端控制线程使用。
class JsonStoreFanLX {
  public:
    explicit JsonStoreFanLX(std::filesystem::path file);
    virtual ~JsonStoreFanLX() = default;
    JsonStoreFanLX(const JsonStoreFanLX &) = delete;
    JsonStoreFanLX &operator=(const JsonStoreFanLX &) = delete;

    // 文件不存在 = 空表（首次启动）；存在但格式不对则抛异常，绝不静默覆盖已确认数据。
    void load();
    // 整表原子写回。
    void save() const;
    const std::filesystem::path &file() const noexcept { return fileFanLX; }

  protected:
    // 文件里 "schema" 字段的取值，用来识别格式与版本。
    virtual std::string schemaFanLX() const = 0;
    // 派生类：把内存表写成 JSON（不含 schema 字段，基类负责加上）。
    virtual nlohmann::json toJsonFanLX() const = 0;
    // 派生类：从 JSON 重建内存表（已确认 schema 匹配）；格式错误时抛异常。
    virtual void fromJsonFanLX(const nlohmann::json &value) = 0;
    // 派生类：清空内存表（load 前调用，保证重复加载结果一致）。
    virtual void clearFanLX() = 0;

  private:
    std::filesystem::path fileFanLX;
};
