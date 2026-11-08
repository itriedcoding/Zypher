// Zypher editor entry point (Qt 6 / QML).
#include "EditorController.h"

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStringList>

#include <cstdio>

namespace {

QStringList g_qtMessages;

void captureMessages(QtMsgType, const QMessageLogContext&, const QString& message) {
    g_qtMessages.append(message);
}

// Prints where the embedded QML landed plus the Qt warnings raised while
// loading it; used when loading fails so CI reports the real cause.
void reportFailure(const QQmlApplicationEngine& engine) {
    std::fprintf(stderr, "error: the editor UI failed to load (module Zypher.Editor, Main.qml)\n");
    std::fprintf(stderr, "root objects: %lld\n", static_cast<long long>(engine.rootObjects().size()));
    for (const QString& message : g_qtMessages) {
        std::fprintf(stderr, "qt: %s\n", message.toUtf8().constData());
    }

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

    QDir importPath(qEnvironmentVariable("QML2_IMPORT_PATH", QString()));
    std::fprintf(stderr, "QML import paths: %s\n",
                 importPath.path().toUtf8().constData());
    std::fflush(stderr);
}

} // namespace

int main(int argc, char* argv[]) {
    qInstallMessageHandler(captureMessages);

    QGuiApplication application(argc, argv);
    application.setApplicationName("Zypher Editor");
    application.setOrganizationName("Zypher");

    for (const QString& path : QCoreApplication::libraryPaths()) {
        std::fprintf(stderr, "library path: %s\n", path.toUtf8().constData());
    }

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

    // The QML files are embedded in this executable (editor/CMakeLists.txt).
    engine.loadFromModule("Zypher.Editor", "Main");
    if (engine.rootObjects().isEmpty()) {
        // Fall back to the resource URL in case the module layout differs.
        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Zypher/Editor/Main.qml")));
    }
    if (engine.rootObjects().isEmpty()) {
        reportFailure(engine);
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
