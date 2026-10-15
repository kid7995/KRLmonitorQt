#pragma once

#include <QString>

class QApplication;

// 全局界面样式（现代浅色工业风）
// 之所以集中在一处：控件样式只在这里定义，面板代码里不再写死颜色，
// 以后想整体换主题（含深色）只需要改这一份 + theme.h 的色值。
namespace uistyle {

QString styleSheet();
void apply(QApplication& app);      // 应用调色板 + 样式表

}  // namespace uistyle
