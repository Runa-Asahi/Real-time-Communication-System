#pragma once
// 全应用只有这一套浅色视觉规范。服务色只用于小型标识，不给整页换肤。
#include <QString>

class QApplication;
class QIcon;

namespace fanlxGui::theme {

inline const QString brand = QStringLiteral("#2563EB");
inline const QString brandHover = QStringLiteral("#1D4ED8");
inline const QString brandPressed = QStringLiteral("#1E40AF");
inline const QString brandSoft = QStringLiteral("#E8EFFD");
inline const QString page = QStringLiteral("#F6F7F9");
inline const QString panel = QStringLiteral("#FFFFFF");
inline const QString canvas = QStringLiteral("#F3F5F8");
inline const QString text = QStringLiteral("#1F2329");
inline const QString textSecondary = QStringLiteral("#5B6472");
inline const QString textMuted = QStringLiteral("#8A94A3");
inline const QString line = QStringLiteral("#E6E8EE");
inline const QString success = QStringLiteral("#15803D");
inline const QString warning = QStringLiteral("#B45309");
inline const QString warningSoft = QStringLiteral("#FFF4E5");
inline const QString danger = QStringLiteral("#DC2626");
inline const QString dangerSoft = QStringLiteral("#FEE2E2");
inline const QString selfBubble = QStringLiteral("#DBE6FE");

inline constexpr int space4 = 4;
inline constexpr int space8 = 8;
inline constexpr int space12 = 12;
inline constexpr int space16 = 16;
inline constexpr int space24 = 24;
inline constexpr int controlHeight = 36;
inline constexpr int radius = 8;

// 把字体和提示样式装到 QApplication。页面自己的样式表仍然可以覆盖局部规则。
void apply(QApplication *app);

// 20px 线性图标。name：chat / people / group / account。
QIcon lineIcon(const QString &name);

// Ciallo 品牌图标（资源 :/icons/ciallo.png；失败时退回绘制的气泡）。
QIcon appIcon();

// 对外展示名（窗口标题、登录页、关于）。
inline const QString appDisplayName = QStringLiteral("Ciallo");

// 同一个名字始终落到同一种头像底色。聊天列表和好友列表共用，不表示在线。
inline QString avatarColor(const QString &seed) {
    static const char *colors[] = {"#3B6FD8", "#2F7D6D", "#8A5A2B", "#6B4C9A",
                                   "#3D6B8A", "#A34B4B", "#4E6B3A", "#5C5F7A"};
    quint32 hash = 2166136261u;
    for (const auto ch : seed)
        hash = (hash ^ static_cast<quint32>(ch.unicode())) * 16777619u;
    return QString::fromLatin1(colors[hash % 8]);
}

// 消息区展示用的短状态。模型里的原文不改，长说明留给提示。
QString shortStatus(const QString &status, bool failed);

} // namespace fanlxGui::theme
