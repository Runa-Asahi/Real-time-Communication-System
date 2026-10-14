#include "gui/AppThemeFanLX.h"
#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>

namespace fanlxGui::theme {

void apply(QApplication *app) {
    if (!app)
        return;
    // 回退链按顺序尝试。离屏插件经常枚举不到系统字体，这时不要改成没有中文的 Segoe UI。
    QFont font;
    font.setFamilies({QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei"),
                      QStringLiteral("微软雅黑"), QStringLiteral("PingFang SC"), QStringLiteral("DengXian"),
                      QStringLiteral("等线"), QStringLiteral("SimSun"), QStringLiteral("宋体"),
                      QStringLiteral("Segoe UI")});
    font.setPointSize(10);
    font.setStyleStrategy(QFont::PreferAntialias);
    const auto resolved = QFontInfo(font).family();
    const bool readable = resolved.contains(QStringLiteral("YaHei")) || resolved.contains(QStringLiteral("雅黑")) ||
                          resolved.contains(QStringLiteral("PingFang")) || resolved.contains(QStringLiteral("Deng")) ||
                          resolved.contains(QStringLiteral("等线")) || resolved.contains(QStringLiteral("SimSun")) ||
                          resolved.contains(QStringLiteral("宋")) || resolved == QStringLiteral("Segoe UI");
    if (readable)
        app->setFont(font);
    const QString tip = QStringLiteral(
        "QToolTip{background:%1;color:%2;border:1px solid %3;padding:4px 8px;}")
                            .arg(panel, text, line);
    if (!app->styleSheet().contains(QStringLiteral("QToolTip")))
        app->setStyleSheet(app->styleSheet() + tip);
}

QIcon lineIcon(const QString &name) {
    QPixmap pix(20, 20);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(text), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (name == QStringLiteral("people")) {
        painter.drawEllipse(QRectF(6.5, 2.5, 7, 7));
        painter.drawArc(QRectF(3, 11, 14, 12), 20 * 16, 140 * 16);
    } else if (name == QStringLiteral("group")) {
        painter.drawEllipse(QRectF(2.5, 4, 6, 6));
        painter.drawEllipse(QRectF(11, 4, 6, 6));
        painter.drawArc(QRectF(1, 12, 9, 8), 15 * 16, 150 * 16);
        painter.drawArc(QRectF(10, 12, 9, 8), 15 * 16, 150 * 16);
    } else if (name == QStringLiteral("account")) {
        painter.drawRoundedRect(QRectF(3.5, 2.5, 13, 15), 2, 2);
        painter.drawEllipse(QRectF(7.5, 5, 5, 5));
        painter.drawArc(QRectF(6, 11, 8, 7), 20 * 16, 140 * 16);
    } else {
        QPainterPath bubble;
        bubble.addRoundedRect(QRectF(2.5, 3, 15, 11), 3, 3);
        painter.drawPath(bubble);
        painter.drawLine(QPointF(6, 14), QPointF(5, 17.5));
        painter.drawLine(QPointF(5, 17.5), QPointF(9, 14));
    }
    return QIcon(pix);
}

QString shortStatus(const QString &status, bool failed) {
    if (status.isEmpty())
        return {};
    if (failed)
        return status;
    if (status == QStringLiteral("待发送"))
        return QStringLiteral("发送中");
    if (status == QStringLiteral("服务器已接收"))
        return QStringLiteral("已接收");
    return status;
}


QIcon appIcon() {
    QIcon icon(QStringLiteral(":/icons/ciallo.png"));
    if (!icon.isNull() && !icon.availableSizes().isEmpty())
        return icon;
    // 资源未嵌入时现场画一枚气泡，保证登录页仍有图标。
    QPixmap pix(128, 128);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(brand));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRectF(8, 8, 112, 112), 28, 28);
    painter.setBrush(QColor(QStringLiteral("#FFFFFF")));
    QPainterPath bubble;
    bubble.addRoundedRect(QRectF(28, 34, 72, 48), 14, 14);
    painter.drawPath(bubble);
    painter.drawEllipse(QRectF(58, 90, 12, 12));
    return QIcon(pix);
}

} // namespace fanlxGui::theme
