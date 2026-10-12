#include "persistence/NamedFilePublisherFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <atomic>
#include <windows.h>

namespace {
std::filesystem::path temporaryFanLX(const std::filesystem::path &file) {
    static std::atomic<unsigned long long> sequence{0};
    return file.parent_path() / (file.filename().wstring() + L".tmp." +
                                 std::to_wstring(GetCurrentProcessId()) + L"." +
                                 std::to_wstring(GetTickCount64()) + L"." + std::to_wstring(++sequence));
}
} // namespace

void NamedFilePublisherFanLX::publish(const std::filesystem::path &file, const std::string &bytes) {
    if (file.empty())
        throw std::invalid_argument("发布目标路径为空");
    std::error_code ignored;
    std::filesystem::create_directories(file.parent_path(), ignored);
    const auto temporary = temporaryFanLX(file);
    // 先完整落盘并刷新，再原子替换；失败时删除临时文件，目标文件保持原样。
    AtomicFileWriterFanLX::writeNew(temporary, bytes);
    if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto error = GetLastError();
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("发布文件失败，Win32=" + std::to_string(error));
    }
}

std::string NamedFilePublisherFanLX::readIfExists(const std::filesystem::path &file) {
    if (file.empty() || !std::filesystem::exists(file))
        return {};
    return AtomicFileWriterFanLX::read(file);
}
