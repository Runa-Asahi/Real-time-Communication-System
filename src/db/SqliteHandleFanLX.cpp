#include "db/SqliteHandleFanLX.h"
#include <sqlite3.h>

namespace sqliteFanLX {

SqliteErrorFanLX::SqliteErrorFanLX(int code, int extended, const std::string &message)
    : std::runtime_error(message), codeFanLX(code), extendedFanLX(extended) {}

bool SqliteErrorFanLX::retryable() const noexcept {
    return codeFanLX == SQLITE_BUSY || codeFanLX == SQLITE_LOCKED || extendedFanLX == SQLITE_BUSY_SNAPSHOT ||
           extendedFanLX == SQLITE_BUSY_RECOVERY;
}

namespace {
std::string messageOf(sqlite3 *handle, int code) {
    const char *text = handle ? sqlite3_errmsg(handle) : nullptr;
    return std::string("SQLite(") + std::to_string(code) + "): " + (text ? text : "未知错误");
}
[[noreturn]] void fail(sqlite3 *handle, int code) {
    throw SqliteErrorFanLX(code, handle ? sqlite3_extended_errcode(handle) : code, messageOf(handle, code));
}

// 只折叠 ASCII 的大小写，其余字节原样保留。
// 这样"仅 ASCII 大小写不敏感"是一条实测契约：任何不以 0x80 开头的字节序列一旦不同，
// 就一定不相等，不会被误匹配成"大小写等价"。UTF-8 多字节序列不含 ASCII 码位，
// 因此中文与西里尔字母按字节精确匹配。
void asciiFoldFanLX(sqlite3_context *context, int count, sqlite3_value **values) {
    if (count != 1) {
        sqlite3_result_null(context);
        return;
    }
    const auto *text = sqlite3_value_text(values[0]);
    if (!text) {
        sqlite3_result_null(context);
        return;
    }
    const int bytes = sqlite3_value_bytes(values[0]);
    std::string folded(reinterpret_cast<const char *>(text), static_cast<std::size_t>(bytes));
    for (char &character : folded) {
        const unsigned char code = static_cast<unsigned char>(character);
        if (code >= 'A' && code <= 'Z')
            character = static_cast<char>(code - 'A' + 'a');
    }
    sqlite3_result_text(context, folded.c_str(), static_cast<int>(folded.size()), SQLITE_TRANSIENT);
}
} // namespace

ConnectionFanLX::ConnectionFanLX(const std::filesystem::path &path) {
    if (path.empty())
        throw std::invalid_argument("数据库路径为空");
    const auto utf8 = path.u8string();
    const std::string file(utf8.begin(), utf8.end());
    const int code = sqlite3_open_v2(file.c_str(), &handleFanLX,
                                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (code != SQLITE_OK) {
        // open 失败时句柄仍需释放，不能泄漏后再次打开同一文件。
        const auto message = messageOf(handleFanLX, code);
        if (handleFanLX) sqlite3_close_v2(handleFanLX);
        handleFanLX = nullptr;
        throw SqliteErrorFanLX(code, code, message);
    }
    sqlite3_extended_result_codes(handleFanLX, 1);
    sqlite3_busy_timeout(handleFanLX, 5000);
    // WAL + FULL：提交返回即已写入日志；进程崩溃可由 SQLite 日志恢复。
    execute("PRAGMA foreign_keys = ON");
    execute("PRAGMA journal_mode = WAL");
    execute("PRAGMA synchronous = FULL");
    execute("PRAGMA temp_store = MEMORY");
    // 历史查询的 ASCII 大小写折叠函数：确定性（SQLITE_DETERMINISTIC）以便绑定参数参与比较。
    if (sqlite3_create_function_v2(handleFanLX, "fanlx_fold", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                                   nullptr, asciiFoldFanLX, nullptr, nullptr, nullptr) != SQLITE_OK) {
        const auto code = sqlite3_errcode(handleFanLX);
        const auto message = messageOf(handleFanLX, code);
        sqlite3_close_v2(handleFanLX);
        handleFanLX = nullptr;
        throw SqliteErrorFanLX(code, code, message);
    }
}

ConnectionFanLX::~ConnectionFanLX() {
    if (!handleFanLX)
        return;
    // 关闭前做一次 TRUNCATE 检查点：把 WAL 内容合并回主库并截断日志。
    // 否则异常退出或直接复制主 .db（忽略 -wal）会丢掉已提交记录。
    char *error = nullptr;
    sqlite3_exec(handleFanLX, "PRAGMA wal_checkpoint(TRUNCATE)", nullptr, nullptr, &error);
    if (error) sqlite3_free(error);
    sqlite3_close_v2(handleFanLX);
    handleFanLX = nullptr;
}

std::string ConnectionFanLX::libraryVersion() const {
    return sqlite3_libversion();
}

void ConnectionFanLX::execute(const std::string &sql) {
    char *error = nullptr;
    const int code = sqlite3_exec(handleFanLX, sql.c_str(), nullptr, nullptr, &error);
    if (code != SQLITE_OK) {
        const std::string detail = error ? error : messageOf(handleFanLX, code);
        if (error) sqlite3_free(error);
        throw SqliteErrorFanLX(code, sqlite3_extended_errcode(handleFanLX), detail);
    }
}

void ConnectionFanLX::begin() { execute("BEGIN IMMEDIATE"); }
void ConnectionFanLX::commit() { execute("COMMIT"); }

void ConnectionFanLX::rollback() noexcept {
    // 回滚失败不得抛异常：它只出现在错误清理路径上，异常会造成二次故障。
    char *error = nullptr;
    sqlite3_exec(handleFanLX, "ROLLBACK", nullptr, nullptr, &error);
    if (error) sqlite3_free(error);
}

int ConnectionFanLX::changes() const noexcept { return sqlite3_changes(handleFanLX); }

std::int64_t ConnectionFanLX::lastInsertRowId() const noexcept {
    return static_cast<std::int64_t>(sqlite3_last_insert_rowid(handleFanLX));
}

bool ConnectionFanLX::isConstraintViolation(const SqliteErrorFanLX &error) noexcept {
    return error.code() == SQLITE_CONSTRAINT;
}

StatementFanLX::StatementFanLX(ConnectionFanLX &connection, const std::string &sql)
    : StatementFanLX(static_cast<const ConnectionFanLX &>(connection), sql) {}

StatementFanLX::StatementFanLX(const ConnectionFanLX &connection, const std::string &sql)
    : connectionFanLX(connection.raw()) {
    const int code = sqlite3_prepare_v2(connectionFanLX, sql.c_str(), static_cast<int>(sql.size()),
                                        &statementFanLX, nullptr);
    if (code != SQLITE_OK) {
        statementFanLX = nullptr;
        fail(connectionFanLX, code);
    }
}

StatementFanLX::~StatementFanLX() {
    if (statementFanLX)
        sqlite3_finalize(statementFanLX);
}

StatementFanLX &StatementFanLX::bind(int index, const std::string &value) {
    const int code = sqlite3_bind_text(statementFanLX, index, value.c_str(), static_cast<int>(value.size()),
                                       SQLITE_TRANSIENT);
    if (code != SQLITE_OK) fail(connectionFanLX, code);
    return *this;
}

StatementFanLX &StatementFanLX::bind(int index, std::int64_t value) {
    const int code = sqlite3_bind_int64(statementFanLX, index, value);
    if (code != SQLITE_OK) fail(connectionFanLX, code);
    return *this;
}

StatementFanLX &StatementFanLX::bind(int index, int value) {
    const int code = sqlite3_bind_int(statementFanLX, index, value);
    if (code != SQLITE_OK) fail(connectionFanLX, code);
    return *this;
}

StatementFanLX &StatementFanLX::bindNull(int index) {
    const int code = sqlite3_bind_null(statementFanLX, index);
    if (code != SQLITE_OK) fail(connectionFanLX, code);
    return *this;
}

StatementFanLX &StatementFanLX::bindBlob(int index, const std::vector<std::uint8_t> &value) {
    const int code = sqlite3_bind_blob(statementFanLX, index, value.data(), static_cast<int>(value.size()),
                                       SQLITE_TRANSIENT);
    if (code != SQLITE_OK) fail(connectionFanLX, code);
    return *this;
}

bool StatementFanLX::step() {
    const int code = sqlite3_step(statementFanLX);
    if (code == SQLITE_ROW) return true;
    if (code == SQLITE_DONE) return false;
    fail(connectionFanLX, code);
}

void StatementFanLX::reset() {
    const int code = sqlite3_reset(statementFanLX);
    if (code != SQLITE_OK) fail(connectionFanLX, code);
    const int cleared = sqlite3_clear_bindings(statementFanLX);
    if (cleared != SQLITE_OK) fail(connectionFanLX, cleared);
}

std::string StatementFanLX::columnText(int index) const {
    const auto *text = sqlite3_column_text(statementFanLX, index);
    if (!text)
        return {};
    const int bytes = sqlite3_column_bytes(statementFanLX, index);
    return std::string(reinterpret_cast<const char *>(text), static_cast<std::size_t>(bytes));
}

std::int64_t StatementFanLX::columnInt64(int index) const {
    return static_cast<std::int64_t>(sqlite3_column_int64(statementFanLX, index));
}

double StatementFanLX::columnDouble(int index) const {
    return sqlite3_column_double(statementFanLX, index);
}

bool StatementFanLX::columnIsNull(int index) const {
    return sqlite3_column_type(statementFanLX, index) == SQLITE_NULL;
}

TransactionFanLX::TransactionFanLX(ConnectionFanLX &connection) : connectionFanLX(connection) {
    connectionFanLX.begin();
}

TransactionFanLX::~TransactionFanLX() {
    if (!finishedFanLX)
        connectionFanLX.rollback();
}

void TransactionFanLX::commit() {
    connectionFanLX.commit();
    finishedFanLX = true;
}

} // namespace sqliteFanLX
