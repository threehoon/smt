#pragma once

#include "cadimport/cadimporter.h"
#include "listmodels/badmarklistmodel.h"
#include "listmodels/boardinstancelistmodel.h"
#include "listmodels/boardlistmodel.h"
#include "listmodels/componentlistmodel.h"
#include "listmodels/feederlistmodel.h"
#include "listmodels/fiduciallistmodel.h"
#include "listmodels/jobplacementlistmodel.h"
#include "listmodels/packagelistmodel.h"
#include "listmodels/placementlistmodel.h"

#include "edit/commandstack.h"
#include "edit/programedits.h"
#include "model/program.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QVariant>

#include <functional>

// The open program file and the list models QML shows it through.
// Exposed to QML as the context property "programDocument".
//
// Every change goes through edit(): the change is applied to a copy, the copy
// must pass ProgramValidator without errors, and only then it replaces the
// program and becomes an undo step. So the program in memory, and every file
// saved from it, can always be opened again.
class ProgramDocument : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY programChanged)
    // File name without directory; empty for a new program not saved yet.
    Q_PROPERTY(QString displayName READ displayName NOTIFY filePathChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    // The file came from a newer minor format version: saving over it would
    // drop content this version does not know, so only Save As is offered.
    Q_PROPERTY(bool saveAsRequired READ saveAsRequired NOTIFY saveAsRequiredChanged)

    // Changes not saved yet; false right after open, new, save, or after
    // undoing back to the saved state.
    Q_PROPERTY(bool modified READ isModified NOTIFY undoStateChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoStateChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoStateChanged)

    Q_PROPERTY(QString programName READ programName NOTIFY contentChanged)
    Q_PROPERTY(QString machineModel READ machineModel NOTIFY contentChanged)
    Q_PROPERTY(double panelWidth READ panelWidth NOTIFY contentChanged)
    Q_PROPERTY(double panelHeight READ panelHeight NOTIFY contentChanged)

    Q_PROPERTY(PackageListModel *packages READ packages CONSTANT)
    Q_PROPERTY(ComponentListModel *components READ components CONSTANT)
    Q_PROPERTY(BoardListModel *boards READ boards CONSTANT)
    Q_PROPERTY(PlacementListModel *boardPlacements READ boardPlacements CONSTANT)
    Q_PROPERTY(FiducialListModel *boardFiducials READ boardFiducials CONSTANT)
    Q_PROPERTY(BadMarkListModel *boardBadMarks READ boardBadMarks CONSTANT)
    Q_PROPERTY(FiducialListModel *panelFiducials READ panelFiducials CONSTANT)
    Q_PROPERTY(BoardInstanceListModel *boardInstances READ boardInstances CONSTANT)
    Q_PROPERTY(JobPlacementListModel *jobPlacements READ jobPlacements CONSTANT)
    Q_PROPERTY(FeederListModel *feeders READ feeders CONSTANT)
    // The "import placements" dialog.
    Q_PROPERTY(CadImporter *cadImport READ cadImport CONSTANT)

public:
    explicit ProgramDocument(QObject *parent = nullptr);

    const Program &program() const { return m_program; }

    bool isLoaded() const { return m_loaded; }
    QString displayName() const;
    QString filePath() const { return m_filePath; }
    bool saveAsRequired() const { return m_saveAsRequired; }

    bool isModified() const { return m_loaded && !m_undo.isClean(); }
    bool canUndo() const { return m_undo.canUndo(); }
    bool canRedo() const { return m_undo.canRedo(); }
    QString undoText() const { return m_undo.undoText(); }
    QString redoText() const { return m_undo.redoText(); }

    QString programName() const { return m_program.info.name; }
    QString machineModel() const;
    double panelWidth() const { return m_program.panel.width; }
    double panelHeight() const { return m_program.panel.height; }

    PackageListModel *packages() const { return m_packages; }
    ComponentListModel *components() const { return m_components; }
    BoardListModel *boards() const { return m_boards; }
    PlacementListModel *boardPlacements() const { return m_boardPlacements; }
    FiducialListModel *boardFiducials() const { return m_boardFiducials; }
    BadMarkListModel *boardBadMarks() const { return m_boardBadMarks; }
    FiducialListModel *panelFiducials() const { return m_panelFiducials; }
    BoardInstanceListModel *boardInstances() const { return m_boardInstances; }
    JobPlacementListModel *jobPlacements() const { return m_jobPlacements; }
    FeederListModel *feeders() const { return m_feeders; }
    CadImporter *cadImport() const { return m_cadImport; }

    // A minimal valid program (one board, one instance), not saved anywhere.
    Q_INVOKABLE void newProgram();
    // URL overloads take what a QML FileDialog returns; only local files.
    // On failure the current program stays open and messageRaised explains why.
    Q_INVOKABLE bool open(const QUrl &url);
    // Needs a file path; a new program must use saveAs.
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveAs(const QUrl &url);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Fields that are not rows of a list model. Values may be typed text.
    Q_INVOKABLE bool setPanelWidth(const QVariant &value);
    Q_INVOKABLE bool setPanelHeight(const QVariant &value);
    Q_INVOKABLE bool setMachineModel(const QString &model);

    bool openFile(const QString &path);
    bool saveFile(const QString &path);

    // Applies `change` to a copy of the program. If the copy has validation
    // errors it is discarded and the user is told why; otherwise it replaces
    // the program as one undo step named `text`. `change` appends the ids it
    // renamed so list models keep those rows. Returns whether it was applied
    // (also true for a change that changes nothing, which adds no step).
    bool edit(const QString &text,
              const std::function<void(Program &, QVector<IdRename> &)> &change);

    // Tells the user an edit was refused.
    void reject(const QString &title, const QString &text, const QString &details = {});

    // Parse a field value: a number, or text with '.' as decimal separator
    // regardless of the system locale. On failure the user is told.
    bool parseNumber(const QVariant &value, double *result);
    bool parseInteger(const QVariant &value, int *result);

signals:
    // Emitted right before / after m_program is replaced by open or new, so
    // models can wrap the change in beginResetModel / endResetModel.
    void programAboutToChange();
    void programChanged();
    // After an edit, undo or redo; models update row by row.
    void programEdited(const QVector<IdRename> &renames);
    // After any change of the program, whether replaced or edited.
    void contentChanged();
    void filePathChanged();
    void saveAsRequiredChanged();
    void undoStateChanged();

    // Something the user must be told. details may hold one diagnostic per line.
    void messageRaised(const QString &title, const QString &text, const QString &details);
    // Short feedback for the status bar, e.g. "已保存 example.smt.xml".
    void statusMessage(const QString &text);

private:
    void replaceProgram(Program program, const QString &path, bool saveAsRequired);
    void applyProgram(const Program &program, const QVector<IdRename> &renames);
    void setFilePath(const QString &path);
    void setSaveAsRequired(bool required);

    Program m_program;
    bool m_loaded = false;
    QString m_filePath;
    bool m_saveAsRequired = false;
    CommandStack m_undo;
    QSet<QString> m_warnings;  // validator warnings of m_program, to spot new ones

    PackageListModel *m_packages;
    ComponentListModel *m_components;
    BoardListModel *m_boards;
    PlacementListModel *m_boardPlacements;
    FiducialListModel *m_boardFiducials;
    BadMarkListModel *m_boardBadMarks;
    FiducialListModel *m_panelFiducials;
    BoardInstanceListModel *m_boardInstances;
    JobPlacementListModel *m_jobPlacements;
    FeederListModel *m_feeders;
    CadImporter *m_cadImport;
};
