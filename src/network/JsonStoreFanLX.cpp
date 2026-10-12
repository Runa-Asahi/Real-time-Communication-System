#include "network/JsonStoreFanLX.h"
#include "persistence/NamedFilePublisherFanLX.h"
#include <stdexcept>
#include <utility>

JsonStoreFanLX::JsonStoreFanLX(std::filesystem::path file) : fileFanLX(std::move(file)) {}

void JsonStoreFanLX::load() {
    clearFanLX();
    const auto bytes = NamedFilePublisherFanLX::readIfExists(fileFanLX);
    if (bytes.empty())
        return; // 首次启动：空表
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(bytes);
    } catch (const std::exception &) {
        throw std::runtime_error("数据文件损坏（不是合法 JSON）：" + fileFanLX.filename().string());
    }
    if (!value.is_object() || value.value("schema", std::string()) != schemaFanLX())
        throw std::runtime_error("数据文件格式不符：" + fileFanLX.filename().string());
    fromJsonFanLX(value);
}

void JsonStoreFanLX::save() const {
    auto value = toJsonFanLX();
    value["schema"] = schemaFanLX();
    NamedFilePublisherFanLX::publish(fileFanLX, value.dump(2));
}
