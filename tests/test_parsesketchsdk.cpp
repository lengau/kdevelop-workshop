#include "parsesketchsdk.h"

#include <QtTest>

class TestParseSketchSdk : public QObject
{
    Q_OBJECT

private:
    using Parser = SketchSdkData (*)(const QStringList&);

    void verifyParsedSketch(Parser parser);

private Q_SLOTS:
    void parsesSketchBlock();
    void parsesSketchBlockMapVariant();
    void serializesSketchData();
    void ignoresUnrelatedEntries();
};

void TestParseSketchSdk::verifyParsedSketch(Parser parser)
{
    const QStringList input = {
        QStringLiteral("# comment before anything"),
        QStringLiteral("- name: other"),
        QStringLiteral("  hooks:"),
        QStringLiteral("    setup-base: ignored"),
        QStringLiteral("- name: 'sketch'"),
        QStringLiteral("hooks:"),
        QStringLiteral("  setup-base: |"),
        QStringLiteral("    apt-get update"),
        QStringLiteral("    apt-get install build-essential"),
        QStringLiteral("  setup-project: uv sync"),
        QStringLiteral("plugs:"),
        QStringLiteral("  editor:"),
        QStringLiteral("    interface: 'mount'"),
        QStringLiteral("    workshop-target: \"project-editor\""),
        QStringLiteral("slots:"),
        QStringLiteral("  server:"),
        QStringLiteral("    interface: \"tunnel\""),
        QStringLiteral("    endpoint: 9000"),
    };

    const SketchSdkData data = parser(input);

    QCOMPARE(data.setupBase, QStringLiteral("apt-get update\napt-get install build-essential"));
    QCOMPARE(data.setupProject, QStringLiteral("uv sync"));

    QCOMPARE(data.plugs.size(), 1);
    QVERIFY(data.plugs.contains(QStringLiteral("editor")));
    QCOMPARE(data.plugs.value(QStringLiteral("editor")).interfaceName, QStringLiteral("mount"));
    QCOMPARE(data.plugs.value(QStringLiteral("editor")).workshopTarget, QStringLiteral("project-editor"));

    QCOMPARE(data.slots.size(), 1);
    QVERIFY(data.slots.contains(QStringLiteral("server")));
    QCOMPARE(data.slots.value(QStringLiteral("server")).interfaceName, QStringLiteral("tunnel"));
    QCOMPARE(data.slots.value(QStringLiteral("server")).endpoint, 9000);
}

void TestParseSketchSdk::parsesSketchBlock()
{
    verifyParsedSketch(parseSketchSdk);
}

void TestParseSketchSdk::parsesSketchBlockMapVariant()
{
    verifyParsedSketch(parseSketchSdkMap);
}

void TestParseSketchSdk::serializesSketchData()
{
    SketchSdkData data;
    data.setupBase = QStringLiteral("apt-get update\napt-get install build-essential");
    data.setupProject = QStringLiteral("uv sync");
    data.plugs.insert(QStringLiteral("editor"),
                      SketchSdkData::Plug{QStringLiteral("mount"), QStringLiteral("project-editor")});
    data.slots.insert(QStringLiteral("server"), SketchSdkData::Slot{QStringLiteral("tunnel"), 9000});

    const QString expected = QStringLiteral(
        "name: sketch\n"
        "hooks:\n"
        "  setup-base: |\n"
        "    apt-get update\n"
        "    apt-get install build-essential\n"
        "  setup-project: |\n"
        "    uv sync\n"
        "plugs:\n"
        "  editor:\n"
        "    interface: 'mount'\n"
        "    workshop-target: 'project-editor'\n"
        "slots:\n"
        "  server:\n"
        "    interface: 'tunnel'\n"
        "    endpoint: 9000\n");

    QCOMPARE(serializeSketchSdk(data), expected);
}

void TestParseSketchSdk::ignoresUnrelatedEntries()
{
    const QStringList input = {
        QStringLiteral("- name: not-sketch"),
        QStringLiteral("hooks:"),
        QStringLiteral("  setup-base: should-not-appear"),
        QStringLiteral("plugs:"),
        QStringLiteral("  editor:"),
        QStringLiteral("    interface: mount"),
    };

    const SketchSdkData data = parseSketchSdk(input);

    QVERIFY(data.setupBase.isEmpty());
    QVERIFY(data.setupProject.isEmpty());
    QVERIFY(data.plugs.isEmpty());
    QVERIFY(data.slots.isEmpty());
}

QTEST_APPLESS_MAIN(TestParseSketchSdk)

#include "test_parsesketchsdk.moc"
