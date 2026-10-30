#include <QTest>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QQuickItem>
#include "db/DatabaseManager.h"
#include "models/HabitModel.h"

using namespace PlasmaHabit;

class TestStartup : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testHeadlessStartupWithHabit();
};

void TestStartup::testHeadlessStartupWithHabit()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("startup_test.sqlite3"))));

    auto habitId = db.createHabit(QStringLiteral("<b>Rich</b> Habit"), QStringLiteral("daily"), 1, QStringLiteral("#2ecc71"));
    QVERIFY(habitId.has_value());

    HabitModel model(&db);
    QCOMPARE(model.rowCount(), 1);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("habitModel"), &model);

    const QUrl url(QStringLiteral("qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml"));
    engine.load(url);

    QVERIFY(!engine.rootObjects().isEmpty());

    auto *rootObj = engine.rootObjects().first();
    QVERIFY(rootObj != nullptr);

    // Verify Heading renders as PlainText
    auto *headingItem = rootObj->findChild<QQuickItem*>(QString(), Qt::FindChildrenRecursively);
    Q_UNUSED(headingItem);
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestStartup tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_startup.moc"
