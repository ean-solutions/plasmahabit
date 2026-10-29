#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <KLocalizedString>
#include <klocalizedqmlcontext.h>

#include "db/DatabaseManager.h"
#include "models/HabitModel.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("plasmahabit");

    app.setOrganizationName(QStringLiteral("eansolutions"));
    app.setOrganizationDomain(QStringLiteral("eansolutions.org"));
    app.setApplicationName(QStringLiteral("plasmahabit"));
    app.setDesktopFileName(QStringLiteral("org.eansolutions.plasmahabit"));

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    auto dbManager = new PlasmaHabit::DatabaseManager(&app);
    if (!dbManager->initialize()) {
        qWarning() << "Failed to initialize SQLite database:" << dbManager->lastError();
    }

    auto habitModel = new PlasmaHabit::HabitModel(dbManager, &app);

    QQmlApplicationEngine engine;
    auto *localizedContext = KLocalization::setupLocalizedContext(&engine);
    if (localizedContext) {
        localizedContext->setTranslationDomain(QStringLiteral("plasmahabit"));
    }

    engine.rootContext()->setContextProperty(QStringLiteral("habitModel"), habitModel);

    const QUrl url(QStringLiteral("qrc:/qt/qml/org/eansolutions/plasmahabit/ui/Main.qml"));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
