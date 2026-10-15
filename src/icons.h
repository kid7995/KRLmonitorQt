#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

// 轻量矢量图标（不依赖外部资源文件，按 16×16 单位坐标系绘制，内部 32px 抗锯齿）
// 命名与用途见 icons.cpp。颜色默认取 theme::C_TEXT，主操作用强调色。
namespace icons {

QIcon get(const QString& name, const QColor& color = QColor());

}  // namespace icons
