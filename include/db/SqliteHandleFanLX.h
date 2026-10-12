#pragma once
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// 官方 sqlite3.h 为公共领域声明文件（third_party/sqlite）；这里只用于取得不透明句柄类型，
// 所有调用仍通过下方薄封装，禁止在仓储实现里散落裸 C API 调用。
#include <sqlite3.h>

struct sqlite3;

namespace sqliteFanLX {

// 数据库访问失败一律以本异常报告；携带扩展结果码与 SQLite 原始消息。
// 调用方不得把失败当作成功 ACK 的依据。
class SqliteErrorFanLX : public std::runtime_error {
    int codeFanLX;
    int extendedFanLX;

  public:
    SqliteErrorFanLX(int code, int extended, const std::string &message);
    int code() const noexcept { return codeFanLX; }
    int extended() const noexcept { return extendedFanLX; }
    // 忙碌/锁冲突可重试；其余错误重试无意义。
    bool retryable() const noexcept;
};

// 单连接 RAII 句柄。一个连接只允许其所属线程使用（SQLITE_OPEN_FULLMUTEX 仅保证内部安全，
// 不改变"数据库工作线程独占连接与语句"的所有权约定）。
class ConnectionFanLX {
    sqlite3 *handleFanLX = nullptr;

  public:
    // relativeOrAbsolute：UTF-8 字节路径，允许中文。目录必须已存在。
    explicit ConnectionFanLX(const std::filesystem::path &path);
    ~ConnectionFanLX();
    ConnectionFanLX(const ConnectionFanLX &) = delete;
    ConnectionFanLX &operator=(const ConnectionFanLX &) = delete;
    ConnectionFanLX(ConnectionFanLX &&) = delete;
    ConnectionFanLX &operator=(ConnectionFanLX &&) = delete;

    sqlite3 *raw() const noexcept { return handleFanLX; }
    std::string libraryVersion() const;
    void execute(const std::string &sql);
    void begin();
    void commit();
    void rollback() noexcept;

    int changes() const noexcept;
    std::int64_t lastInsertRowId() const noexcept;
    // 主键冲突等约束错误；用于幂等键冲突的精确分支。
    static bool isConstraintViolation(const SqliteErrorFanLX &error) noexcept;
};

// 预编译语句。绑定与取值本轮只做必要的字符串/整数/浮点；列按下标访问。
class StatementFanLX {
    sqlite3 *connectionFanLX = nullptr;
    ::sqlite3_stmt *statementFanLX = nullptr;

  public:
    StatementFanLX(ConnectionFanLX &connection, const std::string &sql);
    // 只读查询也允许在 const 成员函数中使用（连接对象本身仍是同一把独占连接）。
    StatementFanLX(const ConnectionFanLX &connection, const std::string &sql);
    ~StatementFanLX();
    StatementFanLX(const StatementFanLX &) = delete;
    StatementFanLX &operator=(const StatementFanLX &) = delete;
    StatementFanLX(StatementFanLX &&) = delete;
    StatementFanLX &operator=(StatementFanLX &&) = delete;

    StatementFanLX &bind(int index, const std::string &value);
    StatementFanLX &bind(int index, std::int64_t value);
    StatementFanLX &bind(int index, int value);
    StatementFanLX &bindNull(int index);
    StatementFanLX &bindBlob(int index, const std::vector<std::uint8_t> &value);

    // true 表示有行；false 表示结束。错误抛 SqliteErrorFanLX。
    bool step();
    void reset();

    std::string columnText(int index) const;
    std::int64_t columnInt64(int index) const;
    double columnDouble(int index) const;
    bool columnIsNull(int index) const;
};

// 事务守卫：构造即 BEGIN IMMEDIATE，析构未提交则回滚。
// 迁移与多表提交必须整体成功，禁止中途发布部分结果。
class TransactionFanLX {
    ConnectionFanLX &connectionFanLX;
    bool finishedFanLX = false;

  public:
    explicit TransactionFanLX(ConnectionFanLX &connection);
    ~TransactionFanLX();
    TransactionFanLX(const TransactionFanLX &) = delete;
    TransactionFanLX &operator=(const TransactionFanLX &) = delete;
    void commit();
};

// 有界忙碌重试：仅对可重试错误重复，超过次数后原样抛出。
template <typename CallableFanLX> auto retryBusyFanLX(CallableFanLX &&call, int attempts = 8) {
    for (int attempt = 0;; ++attempt) {
        try {
            return call();
        } catch (const SqliteErrorFanLX &error) {
            if (!error.retryable() || attempt + 1 >= attempts)
                throw;
            ::Sleep(static_cast<DWORD>(20 * (attempt + 1)));
        }
    }
}

namespace detailFanLX {
template <std::size_t IndexFanLX = 0, typename TupleFanLX>
void bindTupleFanLX(StatementFanLX &statement, const TupleFanLX &values) {
    if constexpr (IndexFanLX < std::tuple_size_v<TupleFanLX>) {
        const auto &value = std::get<IndexFanLX>(values);
        using ValueFanLX = std::decay_t<decltype(value)>;
        const int position = static_cast<int>(IndexFanLX) + 1;
        if constexpr (std::is_same_v<ValueFanLX, std::nullptr_t>) statement.bindNull(position);
        else if constexpr (std::is_same_v<ValueFanLX, std::string>) statement.bind(position, value);
        else if constexpr (std::is_same_v<ValueFanLX, int>) statement.bind(position, value);
        else statement.bind(position, static_cast<std::int64_t>(value));
        bindTupleFanLX<IndexFanLX + 1>(statement, values);
    }
}
} // namespace detailFanLX

// 位置参数绑定：按顺序对应 ?1..?n，避免手写序号错位（SQL 中不使用 ? 省略形式）。
template <typename... ArgumentFanLX>
void bindAllFanLX(StatementFanLX &statement, const ArgumentFanLX &...arguments) {
    detailFanLX::bindTupleFanLX(statement, std::forward_as_tuple(arguments...));
}

// 只关心副作用的语句：迭代到底以完成执行（DELETE/UPDATE 也会先返回一行）。
inline void runStatementFanLX(StatementFanLX &statement) {
    while (statement.step()) { /* 忽略返回行 */
    }
}

} // namespace sqliteFanLX
