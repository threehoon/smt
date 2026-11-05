#pragma once

#include "model/board.h"
#include "model/component.h"
#include "model/feedersetup.h"
#include "model/package.h"
#include "model/panel.h"

#include <QString>
#include <QVector>

#include <optional>

struct ProgramInfo
{
    QString name;
    QString productCode;
    QString revision;  // maintained by the user, unrelated to the format version
    QString createdAt;  // ISO 8601, kept as written
    QString modifiedAt;
    QString author;
    QString description;
};

inline bool operator==(const ProgramInfo &a, const ProgramInfo &b)
{
    return a.name == b.name && a.productCode == b.productCode && a.revision == b.revision
        && a.createdAt == b.createdAt && a.modifiedAt == b.modifiedAt
        && a.author == b.author && a.description == b.description;
}

// One SMT program file (docs/program-format.md).
struct Program
{
    ProgramInfo info;
    QVector<Package> packages;
    QVector<Component> components;
    QVector<Board> boards;
    Panel panel;
    // Absent until feeders are assigned; a program can be edited offline first.
    std::optional<FeederSetup> feederSetup;
    QVector<QString> extensions;  // raw <Extension> elements
};

inline bool operator==(const Program &a, const Program &b)
{
    return a.info == b.info && a.packages == b.packages && a.components == b.components
        && a.boards == b.boards && a.panel == b.panel && a.feederSetup == b.feederSetup
        && a.extensions == b.extensions;
}
