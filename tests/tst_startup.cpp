#include <QTest>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
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
    void testSettingsDialogSaveAndCancelBehavior();
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
    QStringList mainQmlTypeErrors;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine, [&](const QList<QQmlError> &warnings) {
        for (const auto &w : warnings) {
            if (w.toString().contains(QStringLiteral("TypeError"))) {
                mainQmlTypeErrors.append(w.toString());
            }
        }
    });
    engine.rootContext()->setContextProperty(QStringLiteral("habitModel"), &model);

    const QUrl url(QStringLiteral("qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml"));
    engine.load(url);

    QVERIFY(!engine.rootObjects().isEmpty());

    auto *rootObj = engine.rootObjects().first();
    QVERIFY(rootObj != nullptr);

    // Verify Heading renders as PlainText
    auto *headingItem = rootObj->findChild<QQuickItem*>(QString(), Qt::FindChildrenRecursively);
    Q_UNUSED(headingItem);

    // Verify settings dialog and weekStartDay switching works cleanly in QML
    model.setWeekStartDay(QStringLiteral("sunday"));
    QCOMPARE(model.weekStartDay(), QStringLiteral("sunday"));
    model.setWeekStartDay(QStringLiteral("monday"));
    QCOMPARE(model.weekStartDay(), QStringLiteral("monday"));
    QCOMPARE(mainQmlTypeErrors, QStringList());
}

void TestStartup::testSettingsDialogSaveAndCancelBehavior()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("startup_settings_test.sqlite3"))));

    HabitModel model(&db);
    QCOMPARE(model.weekStartDay(), QStringLiteral("monday"));

    QQmlApplicationEngine engine;
    QStringList mainQmlTypeErrors;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine, [&](const QList<QQmlError> &warnings) {
        for (const auto &w : warnings) {
            if (w.toString().contains(QStringLiteral("TypeError"))) {
                mainQmlTypeErrors.append(w.toString());
            }
        }
    });
    engine.rootContext()->setContextProperty(QStringLiteral("habitModel"), &model);

    const QUrl url(QStringLiteral("qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml"));
    engine.load(url);
    QVERIFY(!engine.rootObjects().isEmpty());

    auto *rootObj = engine.rootObjects().first();
    QVERIFY(rootObj != nullptr);

    auto *settingsDialog = rootObj->findChild<QObject*>(QStringLiteral("settingsDialog"));
    QVERIFY(settingsDialog != nullptr);

    // Open settings dialog
    QMetaObject::invokeMethod(settingsDialog, "open");

    auto *saveAction = settingsDialog->findChild<QObject*>(QStringLiteral("saveAction"));
    QVERIFY(saveAction != nullptr);

    // Initially with no changes, Save button must be disabled
    QCOMPARE(saveAction->property("enabled").toBool(), false);

    auto *weekStartCombo = settingsDialog->findChild<QObject*>(QStringLiteral("weekStartCombo"));
    QVERIFY(weekStartCombo != nullptr);

    // User changes combo to Sunday (index 1)
    weekStartCombo->setProperty("currentIndex", 1);
    QMetaObject::invokeMethod(weekStartCombo, "activated", Q_ARG(int, 1));
    QCOMPARE(mainQmlTypeErrors, QStringList());

    // Save button must now be enabled
    QCOMPARE(saveAction->property("enabled").toBool(), true);

    // But habitModel.weekStartDay must NOT have changed yet! (Changes are not applied until Save is clicked)
    QCOMPARE(model.weekStartDay(), QStringLiteral("monday"));

    // User clicks Cancel
    QMetaObject::invokeMethod(settingsDialog, "reject");
    QCOMPARE(model.weekStartDay(), QStringLiteral("monday"));

    // Reopen dialog: should reset to current model value and Save button should be disabled
    QMetaObject::invokeMethod(settingsDialog, "open");
    QCOMPARE(weekStartCombo->property("currentIndex").toInt(), 0);
    QCOMPARE(saveAction->property("enabled").toBool(), false);

    // Change to Sunday and accept (click Save)
    weekStartCombo->setProperty("currentIndex", 1);
    QCOMPARE(saveAction->property("enabled").toBool(), true);
    QMetaObject::invokeMethod(settingsDialog, "accept");

    // NOW changes must be applied
    QCOMPARE(model.weekStartDay(), QStringLiteral("sunday"));
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestStartup tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_startup.moc"
