#include "listmodels/componentlistmodel.h"

#include "document/programdocument.h"
#include "model/program.h"

#include <QStringList>

QVariant ComponentListModel::data(const QModelIndex &index, int role) const
{
    const Row *r = rowAt(index);
    const Component *c = r ? itemAt(program().components, r->index) : nullptr;
    if (!c)
        return {};
    switch (role) {
    case IdRole:
        return c->id;
    case PackageRole:
        return c->package;
    case ValueRole:
        return c->value;
    case DescriptionRole:
        return c->description;
    case FeederCountRole:
        return m_feederCount.value(c->id);
    case PlacementCountRole:
        return m_placementCount.value(c->id);
    }
    return {};
}

QHash<int, QByteArray> ComponentListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {PackageRole, "package"},
        {ValueRole, "value"},
        {DescriptionRole, "description"},
        {FeederCountRole, "feederCount"},
        {PlacementCountRole, "placementCount"},
    };
}

QVector<ProgramListModel::Row> ComponentListModel::buildRows() const
{
    QVector<Row> rows;
    const QVector<Component> &list = program().components;
    for (int i = 0; i < list.size(); ++i)
        rows.append({list.at(i).id, i, -1});
    return rows;
}

QString ComponentListModel::renamedKey(const QString &key,
                                       const QVector<IdRename> &renames) const
{
    return followRenames(key, renames, IdRename::Scope::Component);
}

bool ComponentListModel::rebuild(const QVector<IdRename> &)
{
    m_feederCount.clear();
    m_placementCount.clear();
    const Program &p = program();
    if (p.feederSetup) {
        for (const Feeder &f : p.feederSetup->feeders)
            ++m_feederCount[f.component];
    }
    for (const Board &b : p.boards) {
        for (const Placement &pl : b.placements)
            ++m_placementCount[pl.component];
    }
    return true;
}

bool ComponentListModel::setRole(const Row &row, int role, const QVariant &value)
{
    const int i = row.index;
    const QString text = tr("修改元件 %1").arg(row.key);
    const QString s = value.toString().trimmed();
    switch (role) {
    case IdRole:
        return editRename(tr("重命名元件 %1").arg(row.key),
                          {IdRename::Scope::Component, {}, row.key, s},
                          [&](Program &p) { ProgramEdits::renameComponent(p, i, s); });
    case PackageRole:
        return edit(text, [&](Program &p) { p.components[i].package = s; });
    case ValueRole:
        return edit(text, [&](Program &p) { p.components[i].value = s; });
    case DescriptionRole:
        return edit(text, [&](Program &p) { p.components[i].description = s; });
    }
    return false;
}

int ComponentListModel::add()
{
    const Program &p = program();
    if (p.packages.isEmpty()) {
        document()->reject(tr("不能添加元件"), tr("元件必须指定封装，请先在封装页添加封装。"));
        return -1;
    }
    QStringList ids;
    for (const Component &c : p.components)
        ids << c.id;
    Component c;
    c.id = ProgramEdits::uniqueId(QStringLiteral("PART"), ids);
    c.package = p.packages.first().id;
    const int row = p.components.size();
    if (!edit(tr("添加元件 %1").arg(c.id), [&](Program &prog) { prog.components.append(c); }))
        return -1;
    return row;
}

bool ComponentListModel::remove(int row)
{
    const Row *r = rowAt(row);
    if (!r)
        return false;
    const int i = r->index;
    return edit(tr("删除元件 %1").arg(r->key),
                [&](Program &p) { ProgramEdits::removeComponent(p, i); });
}

QString ComponentListModel::removalImpact(int row) const
{
    const Row *r = rowAt(row);
    return r ? describe(ProgramEdits::componentRemovalImpact(program(), r->index)) : QString();
}
