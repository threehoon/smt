#pragma once

#include "cadimport/cadimport.h"
#include "cadimport/cadpartlistmodel.h"
#include "cadimport/cadrowlistmodel.h"
#include "cadimport/cadtable.h"

#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QVector>

class ProgramDocument;

// The "import placements" dialog: one CAD placement file, how to read it, and
// a preview of what it does to the target board. Exposed to QML as
// programDocument.cadImport.
//
// Every option change re-reads the file text and recomputes the preview by
// applying the import to a copy of the program, so the preview and the import
// use the same code. importPlacements() applies it for real as one undoable
// edit through ProgramDocument::edit().
//
// With no program open the preview uses a blank program, and the import first
// creates a new program, so importing a file is a way to start one.
//
// The lists for combo boxes (…Choices) notify with choicesChanged, emitted only
// when one of them really changed: a view rebuilds its items when a list is
// assigned again, which would close a popup on every recompute.
class CadImporter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY changed)
    Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
    // Tool that wrote the file, as recognised from its header.
    Q_PROPERTY(QString formatName READ formatName NOTIFY changed)

    // Choices: 0 = auto, then comma, tab, semicolon, whitespace.
    Q_PROPERTY(QStringList delimiterChoices READ delimiterChoices NOTIFY choicesChanged)
    Q_PROPERTY(int delimiterChoice READ delimiterChoice WRITE setDelimiterChoice NOTIFY changed)
    Q_PROPERTY(bool autoHeader READ autoHeader WRITE setAutoHeader NOTIFY changed)
    // 1-based line of the header in use; 0 = none. Setting it turns autoHeader off.
    Q_PROPERTY(int headerLine READ headerLine WRITE setHeaderLine NOTIFY changed)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY changed)
    // Choices: 0 = auto, then mm, mil, inch.
    Q_PROPERTY(QStringList unitChoices READ unitChoices NOTIFY choicesChanged)
    Q_PROPERTY(int unitChoice READ unitChoice WRITE setUnitChoice NOTIFY changed)

    // Field names (位号, X, Y, ...) in CadField order, and the column of each
    // as an index into columnChoices: 0 = not in the file, n = column n - 1.
    Q_PROPERTY(QStringList fieldNames READ fieldNames CONSTANT)
    Q_PROPERTY(QStringList columnChoices READ columnChoices NOTIFY choicesChanged)
    Q_PROPERTY(QVariantList fieldColumns READ fieldColumns NOTIFY changed)

    // Board origin in file coordinates, mm.
    Q_PROPERTY(double originX READ originX NOTIFY changed)
    Q_PROPERTY(double originY READ originY NOTIFY changed)

    Q_PROPERTY(int boardIndex READ boardIndex WRITE setBoardIndex NOTIFY changed)
    Q_PROPERTY(QStringList boardChoices READ boardChoices NOTIFY choicesChanged)
    Q_PROPERTY(bool replace READ isReplace WRITE setReplace NOTIFY changed)

    Q_PROPERTY(CadRowListModel *rows READ rowModel CONSTANT)
    Q_PROPERTY(CadPartListModel *parts READ partModel CONSTANT)
    // For the part form: 0 = create, then the existing ids.
    Q_PROPERTY(QStringList componentChoices READ componentChoices NOTIFY choicesChanged)
    Q_PROPERTY(QStringList packageChoices READ packageChoices NOTIFY choicesChanged)

    Q_PROPERTY(int rowCount READ rowCount NOTIFY changed)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY changed)
    Q_PROPERTY(int removedCount READ removedCount NOTIFY changed)
    // Placements the import adds or changes; 0 when it would change nothing.
    Q_PROPERTY(bool canImport READ canImport NOTIFY changed)
    // What the import will do, or why it cannot.
    Q_PROPERTY(QString summaryText READ summaryText NOTIFY changed)

public:
    explicit CadImporter(ProgramDocument *document, QObject *parent = nullptr);

    bool isLoaded() const { return m_loaded; }
    QString fileName() const { return m_fileName; }
    QString formatName() const;

    QStringList delimiterChoices() const;
    int delimiterChoice() const;
    void setDelimiterChoice(int choice);
    bool autoHeader() const { return m_tableOptions.autoHeader; }
    void setAutoHeader(bool on);
    int headerLine() const { return m_table.headerLine; }
    void setHeaderLine(int line);
    int lineCount() const { return m_lineCount; }
    QStringList unitChoices() const;
    int unitChoice() const;
    void setUnitChoice(int choice);

    QStringList fieldNames() const;
    QStringList columnChoices() const;
    QVariantList fieldColumns() const;

    double originX() const { return m_extract.originX; }
    double originY() const { return m_extract.originY; }

    int boardIndex() const { return m_board; }
    void setBoardIndex(int index);
    QStringList boardChoices() const;
    bool isReplace() const { return m_mode == CadImportMode::Replace; }
    void setReplace(bool replace);

    CadRowListModel *rowModel() const { return m_rowModel; }
    CadPartListModel *partModel() const { return m_partModel; }
    QStringList componentChoices() const;
    QStringList packageChoices() const;

    int rowCount() const { return m_rows.size(); }
    int errorCount() const;
    int removedCount() const { return m_summary.removed; }
    bool canImport() const;
    QString summaryText() const;

    const QVector<CadPlacement> &placements() const { return m_rows; }
    const QVector<CadPart> &parts() const { return m_parts; }
    const CadImportSummary &summary() const { return m_summary; }
    const CadTable &table() const { return m_table; }

    // Reads a file and resets the options to auto. On failure the user is
    // told why and the previous file stays.
    Q_INVOKABLE bool open(const QUrl &url);
    bool openFile(const QString &path);
    // Same as openFile, from text already in memory.
    bool openText(const QString &text, const QString &fileName);
    // Forgets the file; loaded becomes false. The preview models keep their
    // rows until the next file is opened.
    Q_INVOKABLE void close();

    // column: index into columnChoices (0 = not in the file).
    Q_INVOKABLE void setFieldColumn(int field, int column);
    // Typed text in mm; on a bad number the user is told and false returned.
    Q_INVOKABLE bool setOriginX(const QVariant &value);
    Q_INVOKABLE bool setOriginY(const QVariant &value);

    // Part choices, as indices into componentChoices / packageChoices.
    void setPartComponent(int part, int choice);
    void setPartPackage(int part, int choice);

    // Applies the import as one undoable edit. Returns false when nothing was
    // imported (the user was told why).
    Q_INVOKABLE bool importPlacements();

    // Asks the window to show the import dialog for board `board` (-1 = keep
    // the current choice); used by the menu and the boards page.
    Q_INVOKABLE void request(int board = -1);

signals:
    void changed();
    void choicesChanged();
    void requested();
    // After a successful import; text says what was done.
    void imported(const QString &title, const QString &text);

private:
    using PartKey = QPair<QString, QString>;  // footprint, value

    // A user's choice for a part, kept when the preview is recomputed.
    struct PartChoice
    {
        QString component;  // empty = create
        QString package;    // empty = create
    };

    void reparse();    // text or table options changed
    void remap();      // the columns changed: guess the mapping and unit
    void reextract();  // mapping, unit or origin changed
    void regroup();    // the rows or the program changed
    void preview();    // part choices, board or mode changed
    bool setOrigin(const QVariant &value, double *target);
    void emitChanged();
    // The program the import goes into: the open one, or m_blank if none.
    const Program &program() const;

    ProgramDocument *m_document;
    const Program m_blank;
    CadRowListModel *m_rowModel;
    CadPartListModel *m_partModel;

    bool m_loaded = false;
    QString m_fileName;
    QString m_text;
    int m_lineCount = 0;
    CadTableOptions m_tableOptions;
    CadTable m_table;
    bool m_autoUnit = true;
    CadUnit m_detectedUnit = CadUnit::Millimeter;
    CadExtractOptions m_extract;
    QVector<CadPlacement> m_rows;
    QVector<CadPart> m_parts;
    QHash<PartKey, PartChoice> m_choices;
    int m_board = 0;
    CadImportMode m_mode = CadImportMode::Merge;
    CadImportSummary m_summary;
    QVector<QStringList> m_lastChoices;
};
