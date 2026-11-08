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
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("editor", &controller);

    engine.load(QUrl::fromLocalFile(QStringLiteral(ZYPHER_EDITOR_QML_DIR) + "/Main.qml"));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return application.exec();
}
