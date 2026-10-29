#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <KLocalizedString>
#include <klocalizedqmlcontext.h>

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

    QQmlApplicationEngine engine;
    auto *localizedContext = KLocalization::setupLocalizedContext(&engine);
    if (localizedContext) {
        localizedContext->setTranslationDomain(QStringLiteral("plasmahabit"));
    }

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
