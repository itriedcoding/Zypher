// Zypher editor entry point (Qt 6 / QML).
#include "EditorController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    application.setApplicationName("Zypher Editor");
    application.setOrganizationName("Zypher");

    EditorController controller;

    // `zypher_editor <project.json>` opens that project; with no argument the
    // controller looks for the demo project shipped next to the application.
    const QStringList arguments = application.arguments();
    if (arguments.size() > 1) {
        controller.setInitialProject(arguments.at(1));
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("editor", &controller);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    // The QML files are embedded in this executable (editor/CMakeLists.txt).
    engine.loadFromModule("Zypher.Editor", "Main");
    if (engine.rootObjects().isEmpty()) {
        // Fall back to the resource URL in case the module layout differs.
        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Zypher/Editor/qml/Main.qml")));
    }
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return application.exec();
}
