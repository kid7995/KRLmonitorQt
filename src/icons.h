#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

// 轻量矢量图标（不依赖外部资源文件）。
// 统一设计语言：24×24 网格、2 单位描边、圆头圆角、无多余填充（Lucide / Feather 风格），
// 内部按 48px 抗锯齿绘制后再缩放，小尺寸下依然清晰。
// 颜色默认 theme::C_TEXT；主操作用强调色，面板标题用 theme::C_INFO。
namespace icons {

QIcon get(const QString& name, const QColor& color = QColor());

}  // namespace icons
