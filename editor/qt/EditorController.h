// Qt adapter around zy::editor::EditorSession, exposed to QML as `editor`.
#pragma once

#include "core/EditorSession.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>

class EditorController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString projectName READ projectName NOTIFY changed)
    Q_PROPERTY(QString sceneName READ sceneName NOTIFY changed)
    Q_PROPERTY(QString assetRoot READ assetRoot NOTIFY changed)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY changed)
    Q_PROPERTY(QStringList entityNames READ entityNames NOTIFY changed)
    Q_PROPERTY(QStringList assetPaths READ assetPaths NOTIFY changed)
    Q_PROPERTY(int assetCount READ assetCount NOTIFY changed)
    Q_PROPERTY(int entityCount READ entityCount NOTIFY changed)
    Q_PROPERTY(int selection READ selection NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(QString undoLabel READ undoLabel NOTIFY changed)
    Q_PROPERTY(QString redoLabel READ redoLabel NOTIFY changed)
    Q_PROPERTY(QVariantMap selectedPosition READ selectedPosition NOTIFY changed)
    Q_PROPERTY(QVariantMap selectedScale READ selectedScale NOTIFY changed)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY changed)
    Q_PROPERTY(QString selectedMesh READ selectedMesh NOTIFY changed)
    Q_PROPERTY(QString selectedScript READ selectedScript NOTIFY changed)
    Q_PROPERTY(bool demoAvailable READ demoAvailable NOTIFY changed)

public:
    explicit EditorController(QObject* parent = nullptr);

    // Set from the command line before the QML shell loads.
    void setInitialProject(const QString& path) { m_initialProject = path; }

    Q_INVOKABLE void startup();
    Q_INVOKABLE bool openProject(const QString& path);
    Q_INVOKABLE void openDemoProject();
    Q_INVOKABLE void saveScene();
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void addEntity(const QString& name, const QString& meshPath);
    Q_INVOKABLE void removeSelected();
    Q_INVOKABLE void moveSelected(int delta);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void setSelectedTransform(float px, float py, float pz, float sx, float sy, float sz);
    Q_INVOKABLE void selectAsset(const QString& path);

    QString projectName() const;
    QString sceneName() const;
    QString assetRoot() const;
    QString statusMessage() const;
    QStringList entityNames() const;
    QStringList assetPaths() const;
    int assetCount() const;
    int entityCount() const;
    int selection() const;
    bool dirty() const;
    bool canUndo() const;
    bool canRedo() const;
    QString undoLabel() const;
    QString redoLabel() const;
    QVariantMap selectedPosition() const;
    QVariantMap selectedScale() const;
    QString selectedName() const;
    QString selectedMesh() const;
    QString selectedScript() const;
    bool demoAvailable() const;

signals:
    void changed();
    void logMessage(const QString& message);

private:
    void publish(const QString& message = QString());
    void loadAssets();
    [[nodiscard]] QString bundledDemoProject() const;

    zy::editor::EditorSession m_session;
    QStringList m_assetPaths;
    QString m_status;
    QString m_initialProject;
};
