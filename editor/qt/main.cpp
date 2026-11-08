// Zypher editor entry point (Qt 6 / QML).
#include "EditorController.h"

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <cstdio>

namespace {

// Prints where the embedded QML actually landed; used when loading fails so CI
// reports the real cause instead of a bare exit code.
void reportResources() {
    QDir root(QStringLiteral(":/qt/qml"));
    std::fprintf(stderr, "embedded QML root: %s\n", root.path().toUtf8().constData());
    for (const QString& entry : root.entryList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
        std::fprintf(stderr, "  module dir: %s\n", entry.toUtf8().constData());
    }
    QDir module(QStringLiteral(":/qt/qml/Zypher/Editor"));
    if (module.exists()) {
        for (const QString& file : module.entryList(QDir::Files)) {
            std::fprintf(stderr, "  file: %s\n", file.toUtf8().constData());
        }
    } else {
        std::fprintf(stderr, "  module dir :/qt/qml/Zypher/Editor is missing\n");
    }
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    application.setApplicationName("Zypher Editor");
    application.setOrganizationName("Zypher");

    EditorController controller;

    // `zypher_editor <project.json>` opens that project; with no argument the
    // controller looks for the demo project shipped next to the application.
    const QStringList arguments = application.arguments();
    bool checkOnly = false;
    for (int index = 1; index < arguments.size(); ++index) {
        const QString& argument = arguments.at(index);
        if (argument == QStringLiteral("--check")) {
            checkOnly = true;
        } else if (!argument.startsWith(QLatin1Char('-'))) {
            controller.setInitialProject(argument);
        }
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
        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Zypher/Editor/Main.qml")));
    }
    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "error: the editor UI failed to load (module Zypher.Editor, Main.qml)\n");
        reportResources();
        return 1;
    }

    if (checkOnly) {
        // Headless self check: proves the Qt runtime, platform plugin and QML
        // module all work without opening a window. Used by CI and releases.
        const int objects = static_cast<int>(engine.rootObjects().size());
        std::printf("zypher_editor check ok: %d root object(s), project '%s'\n", objects,
                    controller.projectName().toUtf8().constData());
        std::fflush(stdout);
        return 0;
    }
    return application.exec();
}
