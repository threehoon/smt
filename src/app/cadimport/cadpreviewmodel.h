#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantMap>

class CadImporter;

// Base of the import dialog's preview tables. The rows are the importer's
// working data, not the program, so a recompute may reset the model; but when
// the rows are still the same (only values changed, e.g. a part's choice)
// it only emits dataChanged, so the selected row and the form next to the
// table stay.
class CadPreviewModel : public QAbstractListModel
{
    Q_OBJECT
    // Emitted after every refresh, so a binding that calls get() re-evaluates.
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit CadPreviewModel(CadImporter *importer, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const { return rowCount(); }

    // Every role of one row, keyed by role name; empty for a row out of range.
    Q_INVOKABLE QVariantMap get(int row) const;

    // Call after the importer recomputed its data.
    void refresh();

signals:
    void countChanged();

protected:
    CadImporter *importer() const { return m_importer; }
    // One key per row of the importer's current data, in order.
    virtual QStringList keys() const = 0;

private:
    CadImporter *m_importer;
    QStringList m_keys;
};
