#include "listmodels/feederlistmodel.h"

#include "display/displaytext.h"
#include "document/programdocument.h"
#include "model/angle.h"
#include "model/program.h"

#include <QStringList>

QVariant FeederListModel::data(const QModelIndex &index, int role) const
{
    const Row *r = rowAt(index);
    const auto &setup = program().feederSetup;
    const Feeder *f = r && setup ? itemAt(setup->feeders, r->index) : nullptr;
    if (!f)
        return {};
    switch (role) {
    case IdRole:
        return f->id;
    case TypeRole:
        return DisplayText::feederType(*f);
    case TypeIndexRole:
        return f->type == FeederType::Unknown ? -1 : static_cast<int>(f->type);
    case SlotRole:
        return f->slot;
    case ComponentRole:
        return f->component;
    case PickXRole:
        return f->pickX;
    case PickYRole:
        return f->pickY;
    case PickZRole:
        return f->pickZ;
    case RotationRole:
        return f->rotation;
    case EnabledRole:
        return f->enabled;
    case PriorityRole:
        return f->priority;
    case TapeWidthRole:
        return f->tape.width;
    case TapePitchRole:
        return f->tape.pitch;
    case TapeFeedCountRole:
        return f->tape.feedCount;
    case TrayRowsRole:
        return f->tray.rows;
    case TrayColumnsRole:
        return f->tray.columns;
    case TrayRowPitchRole:
        return f->tray.rowPitch;
    case TrayColumnPitchRole:
        return f->tray.columnPitch;
    case TrayStartIndexRole:
        return f->tray.startIndex;
    case TubePitchRole:
        return f->tube.pitch;
    }
    return {};
}

QHash<int, QByteArray> FeederListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TypeRole, "type"},
        {TypeIndexRole, "typeIndex"},
        {SlotRole, "slot"},
        {ComponentRole, "component"},
        {PickXRole, "pickX"},
        {PickYRole, "pickY"},
        {PickZRole, "pickZ"},
        {RotationRole, "rotation"},
        {EnabledRole, "enabled"},
        {PriorityRole, "priority"},
        {TapeWidthRole, "tapeWidth"},
        {TapePitchRole, "tapePitch"},
        {TapeFeedCountRole, "tapeFeedCount"},
        {TrayRowsRole, "trayRows"},
        {TrayColumnsRole, "trayColumns"},
        {TrayRowPitchRole, "trayRowPitch"},
        {TrayColumnPitchRole, "trayColumnPitch"},
        {TrayStartIndexRole, "trayStartIndex"},
        {TubePitchRole, "tubePitch"},
    };
}

QVector<ProgramListModel::Row> FeederListModel::buildRows() const
{
    QVector<Row> rows;
    if (const auto &setup = program().feederSetup) {
        for (int i = 0; i < setup->feeders.size(); ++i)
            rows.append({setup->feeders.at(i).id, i, -1});
    }
    return rows;
}

QString FeederListModel::renamedKey(const QString &key, const QVector<IdRename> &renames) const
{
    return followRenames(key, renames, IdRename::Scope::Feeder);
}

bool FeederListModel::setRole(const Row &row, int role, const QVariant &value)
{
    if (!program().feederSetup)
        return false;
    const int i = row.index;
    const QString text = tr("修改送料器 %1").arg(row.key);
    auto feeder = [i](Program &p) -> Feeder & { return p.feederSetup->feeders[i]; };
    auto number = [&](double Feeder::*member) {
        return editNumber(text, value, [&](Program &p, double v) { feeder(p).*member = v; });
    };
    switch (role) {
    case IdRole: {
        const QString id = value.toString().trimmed();
        return editRename(tr("重命名送料器 %1").arg(row.key),
                          {IdRename::Scope::Feeder, {}, row.key, id},
                          [&](Program &p) { feeder(p).id = id; });
    }
    case TypeIndexRole: {
        const int type = value.toInt();
        if (type < int(FeederType::Tape) || type > int(FeederType::Bulk))
            return false;
        return edit(text, [&](Program &p) {
            Feeder &f = feeder(p);
            f.type = static_cast<FeederType>(type);
            // Parameters of a type this version did not know are gone now.
            f.unknownType.clear();
            f.rawParams.clear();
        });
    }
    case SlotRole: {
        const QString slot = value.toString().trimmed();
        return edit(text, [&](Program &p) { feeder(p).slot = slot; });
    }
    case ComponentRole: {
        const QString component = value.toString().trimmed();
        return edit(text, [&](Program &p) { feeder(p).component = component; });
    }
    case PickXRole:
        return number(&Feeder::pickX);
    case PickYRole:
        return number(&Feeder::pickY);
    case PickZRole:
        return number(&Feeder::pickZ);
    case RotationRole:
        return editNumber(text, value,
                          [&](Program &p, double v) { feeder(p).rotation = normalizeAngle(v); });
    case EnabledRole:
        return edit(text, [&](Program &p) { feeder(p).enabled = value.toBool(); });
    case PriorityRole:
        return editInteger(text, value, [&](Program &p, int v) { feeder(p).priority = v; });
    case TapeWidthRole:
        return editNumber(text, value, [&](Program &p, double v) { feeder(p).tape.width = v; });
    case TapePitchRole:
        return editNumber(text, value, [&](Program &p, double v) { feeder(p).tape.pitch = v; });
    case TapeFeedCountRole:
        return editInteger(text, value, [&](Program &p, int v) { feeder(p).tape.feedCount = v; });
    case TrayRowsRole:
        return editInteger(text, value, [&](Program &p, int v) { feeder(p).tray.rows = v; });
    case TrayColumnsRole:
        return editInteger(text, value, [&](Program &p, int v) { feeder(p).tray.columns = v; });
    case TrayRowPitchRole:
        return editNumber(text, value, [&](Program &p, double v) { feeder(p).tray.rowPitch = v; });
    case TrayColumnPitchRole:
        return editNumber(text, value,
                          [&](Program &p, double v) { feeder(p).tray.columnPitch = v; });
    case TrayStartIndexRole:
        return editInteger(text, value, [&](Program &p, int v) { feeder(p).tray.startIndex = v; });
    case TubePitchRole:
        return editNumber(text, value, [&](Program &p, double v) { feeder(p).tube.pitch = v; });
    }
    return false;
}

int FeederListModel::add()
{
    const Program &prog = program();
    if (prog.components.isEmpty()) {
        document()->reject(tr("不能添加送料器"), tr("送料器必须指定元件，请先在元件页添加元件。"));
        return -1;
    }
    QStringList ids;
    QStringList slots_;  // `slots` is a Qt keyword macro
    if (prog.feederSetup) {
        for (const Feeder &f : prog.feederSetup->feeders) {
            ids << f.id;
            slots_ << f.slot;
        }
    }
    Feeder f;
    f.id = ProgramEdits::uniqueId(QStringLiteral("F"), ids);
    f.slot = ProgramEdits::uniqueId(QStringLiteral("S"), slots_);
    f.component = prog.components.first().id;
    const int row = prog.feederSetup ? prog.feederSetup->feeders.size() : 0;
    if (!edit(tr("添加送料器 %1").arg(f.id), [&](Program &p) {
            if (!p.feederSetup)
                p.feederSetup = FeederSetup();
            p.feederSetup->feeders.append(f);
        }))
        return -1;
    return row;
}

bool FeederListModel::remove(int row)
{
    const Row *r = rowAt(row);
    if (!r || !program().feederSetup)
        return false;
    const int i = r->index;
    return edit(tr("删除送料器 %1").arg(r->key),
                [&](Program &p) { p.feederSetup->feeders.removeAt(i); });
}
