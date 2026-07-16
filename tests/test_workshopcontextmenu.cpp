#include "views/workshopcontextmenu.h"

#include <QAction>
#include <QMenu>
#include <QtTest>

class TestWorkshopContextMenu : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void createsExpectedActions();
    void triggersBoundCallbacks();
    void handlesNullMenu();
    void ignoresEmptyCallbacks();
};

void TestWorkshopContextMenu::createsExpectedActions()
{
    QMenu menu;
    auto actions = populateWorkshopContextMenu(&menu, []() {}, []() {}, []() {});

    QVERIFY(actions.editAction != nullptr);
    QVERIFY(actions.sketchSdkAction != nullptr);
    QVERIFY(actions.removeAction != nullptr);

    QCOMPARE(actions.editAction->text(), QStringLiteral("Edit Workshop"));
    QCOMPARE(actions.sketchSdkAction->text(), QStringLiteral("Sketch SDK..."));
    QCOMPARE(actions.removeAction->text(), QStringLiteral("Remove Workshop"));

    const QList<QAction*> menuActions = menu.actions();
    QCOMPARE(menuActions.size(), 4);
    QVERIFY(!menuActions.at(0)->isSeparator());
    QVERIFY(!menuActions.at(1)->isSeparator());
    QVERIFY(menuActions.at(2)->isSeparator());
    QVERIFY(!menuActions.at(3)->isSeparator());
}

void TestWorkshopContextMenu::triggersBoundCallbacks()
{
    QMenu menu;
    int editCount = 0;
    int sketchCount = 0;
    int removeCount = 0;

    auto actions = populateWorkshopContextMenu(
        &menu,
        [&editCount]() {
            editCount++;
        },
        [&sketchCount]() {
            sketchCount++;
        },
        [&removeCount]() {
            removeCount++;
        });

    actions.editAction->trigger();
    actions.sketchSdkAction->trigger();
    actions.removeAction->trigger();

    QCOMPARE(editCount, 1);
    QCOMPARE(sketchCount, 1);
    QCOMPARE(removeCount, 1);
}

void TestWorkshopContextMenu::handlesNullMenu()
{
    const auto actions = populateWorkshopContextMenu(nullptr, []() {}, []() {}, []() {});
    QVERIFY(actions.editAction == nullptr);
    QVERIFY(actions.sketchSdkAction == nullptr);
    QVERIFY(actions.removeAction == nullptr);
}

void TestWorkshopContextMenu::ignoresEmptyCallbacks()
{
    QMenu menu;
    const auto actions = populateWorkshopContextMenu(&menu, {}, {}, {});
    QVERIFY(actions.editAction != nullptr);
    QVERIFY(actions.sketchSdkAction != nullptr);
    QVERIFY(actions.removeAction != nullptr);

    actions.editAction->trigger();
    actions.sketchSdkAction->trigger();
    actions.removeAction->trigger();
}

QTEST_MAIN(TestWorkshopContextMenu)
#include "test_workshopcontextmenu.moc"
