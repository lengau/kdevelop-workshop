#include "views/projectselectionguard.h"

#include <QtTest>

class TestProjectSelectionGuard : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void selectionMatches_data();
    void selectionMatches();
};

void TestProjectSelectionGuard::selectionMatches_data()
{
    QTest::addColumn<QString>("expected");
    QTest::addColumn<QString>("current");
    QTest::addColumn<bool>("matches");

    QTest::newRow("same path") << QStringLiteral("/tmp/project") << QStringLiteral("/tmp/project") << true;
    QTest::newRow("different path") << QStringLiteral("/tmp/project-a") << QStringLiteral("/tmp/project-b") << false;
    QTest::newRow("both empty") << QString() << QString() << true;
    QTest::newRow("empty current") << QStringLiteral("/tmp/project") << QString() << false;
}

void TestProjectSelectionGuard::selectionMatches()
{
    QFETCH(QString, expected);
    QFETCH(QString, current);
    QFETCH(bool, matches);

    QCOMPARE(ProjectSelectionGuard::selectionMatches(expected, current), matches);
}

QTEST_MAIN(TestProjectSelectionGuard)
#include "test_projectselectionguard.moc"
