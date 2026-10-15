#include <QtTest/QtTest>

#include "config.h"
#include "theme.h"

// 回归测试：数据组的配色下标绝不能越出 GROUP_PALETTES。
//
// 背景（2026-10-15 线上崩溃）：连续生产会创建远超 4 个数据组，
// 而 nextPaletteIdx() 曾按 MAX_GROUPS 取值、paletteOf() 又直接下标访问 4 元素的
// GROUP_PALETTES —— 第 5 组起就在读数组外的内存。Palette 的第一个成员是 QString，
// 越界读到的野 QString 在拷贝/析构时会访问非法指针，是否立刻崩溃取决于链接器布局，
// 所以这里用"返回引用的地址必须落在数组内"做确定性判据。
class ThemeTest : public QObject {
    Q_OBJECT

private slots:
    void paletteIndexAlwaysStaysInsideTheArray();
    void paletteIndexCoversEveryGroupWeMayKeepInMemory();
};

void ThemeTest::paletteIndexAlwaysStaysInsideTheArray() {
    const theme::Palette* begin = &theme::GROUP_PALETTES[0];
    const theme::Palette* end = &theme::GROUP_PALETTES[theme::PALETTE_COUNT - 1];

    for (int i = -8; i <= 256; ++i) {
        const theme::Palette& p = theme::paletteOf(i);
        QVERIFY2(&p >= begin && &p <= end,
                 qPrintable(QStringLiteral("paletteOf(%1) 返回了数组外的对象").arg(i)));
    }
    // 多组对比是常态：2026-10-15 把配色从 4 组扩到 6 组（白底上都能分辨的中饱和色）
    QCOMPARE(theme::PALETTE_COUNT, 6);
}

void ThemeTest::paletteIndexCoversEveryGroupWeMayKeepInMemory() {
    const theme::Palette* begin = &theme::GROUP_PALETTES[0];
    const theme::Palette* end = &theme::GROUP_PALETTES[theme::PALETTE_COUNT - 1];

    // 内存里最多可能同时存在 MAX_GROUPS 个组，每一个都必须能安全取到配色
    for (int idx = 0; idx < MAX_GROUPS; ++idx) {
        const theme::Palette& p = theme::paletteOf(idx);
        QVERIFY2(&p >= begin && &p <= end,
                 qPrintable(QStringLiteral("第 %1 组取配色时越界").arg(idx + 1)));
    }
}

QTEST_GUILESS_MAIN(ThemeTest)
#include "tst_theme.moc"
