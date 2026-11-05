#pragma once

#include "edit/programedits.h"

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <functional>

class ProgramDocument;
struct Program;

// Base for the list models over the open program.
//
// Each row has a key (usually the element's id) and points into the program
// by index. When the document replaces its program the model resets; after an
// edit it matches old and new rows by key and reports only what changed
// (rows removed, moved, inserted, then dataChanged), so views keep their
// selection and scroll position. Renamed ids are followed through IdRename.
//
// Edits go through set() / setData() and the subclass's invokables, which
// call ProgramDocument::edit(); the model updates when the document reports
// the edit, like every other model.
class ProgramListModel : public QAbstractListModel
{
    Q_OBJECT
    // NOTIFY is emitted after every reset and every edit, also when the row
    // count stays the same, so a QML binding that calls get() re-evaluates.
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // add() can be tried: a program is open (and a board selected, for board
    // content). add() may still refuse and say why, e.g. no package exists.
    Q_PROPERTY(bool canAdd READ canAdd NOTIFY countChanged)

public:
    explicit ProgramListModel(ProgramDocument *document, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const { return m_rows.size(); }
    virtual bool canAdd() const;

    // Every role of one row, keyed by role name; empty for a row out of range.
    Q_INVOKABLE QVariantMap get(int row) const;
    // Changes one field of a row as an undoable edit. `value` may be text the
    // user typed; numbers are parsed here. Returns false when the value is
    // rejected (the document has told the user why) or the role is read-only.
    Q_INVOKABLE bool set(int row, const QString &role, const QVariant &value);

    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    // Adds a row with default values and a new id as an undoable edit.
    // Returns the new row, or -1 when it cannot be added (the user was told).
    Q_INVOKABLE virtual int add();
    // Removes a row and whatever depends on it (see removalImpact).
    Q_INVOKABLE virtual bool remove(int row);
    // What else remove(row) takes with it, as a sentence; empty when nothing,
    // so the view only asks for confirmation when it matters.
    Q_INVOKABLE virtual QString removalImpact(int row) const;

signals:
    void countChanged();

protected:
    struct Row
    {
        QString key;    // unique within the model
        int index = -1;  // meaning is up to the subclass
        int sub = -1;
    };

    const Program &program() const;
    ProgramDocument *document() const { return m_document; }

    // The row behind index; nullptr unless it is a row of this model.
    const Row *rowAt(const QModelIndex &index) const;
    const Row *rowAt(int i) const { return i >= 0 && i < m_rows.size() ? &m_rows.at(i) : nullptr; }

    template <typename T>
    static const T *itemAt(const QVector<T> &list, int i)
    {
        return i >= 0 && i < list.size() ? &list.at(i) : nullptr;
    }

    // The rows to show for program(), in order.
    virtual QVector<Row> buildRows() const = 0;
    // The key an existing row has after `renames`. Default: unchanged.
    virtual QString renamedKey(const QString &key, const QVector<IdRename> &renames) const;
    // Called when the document replaced its program, before rebuild().
    virtual void programReplaced() {}
    // Updates state that follows the program (caches, the selected board).
    // Runs before buildRows() on every reset and edit. Return false when old
    // rows can no longer be matched by key, which makes the edit a reset.
    virtual bool rebuild(const QVector<IdRename> &renames);
    // Applies a set() / setData() call. Default: read-only.
    virtual bool setRole(const Row &row, int role, const QVariant &value);

    // `key` after the renames of `scope`, for models keyed by a plain id.
    static QString followRenames(QString key, const QVector<IdRename> &renames,
                                 IdRename::Scope scope);

    // Resets the model; `prepare` runs between begin and end, before rebuild.
    void resetRows(const std::function<void()> &prepare = {});

    // Shorthands for ProgramDocument::edit().
    bool edit(const QString &text, const std::function<void(Program &)> &change);
    bool editNumber(const QString &text, const QVariant &value,
                    const std::function<void(Program &, double)> &assign);
    bool editInteger(const QString &text, const QVariant &value,
                     const std::function<void(Program &, int)> &assign);
    bool editRename(const QString &text, const IdRename &rename,
                    const std::function<void(Program &)> &change);

    // "同时会删除：2 个元件、3 个贴片位。" or empty.
    static QString describe(const RemovalImpact &impact);

private:
    void sync(const QVector<IdRename> &renames);
    void replaceRows(const QVector<Row> &rows);

    ProgramDocument *m_document;
    QVector<Row> m_rows;
};
