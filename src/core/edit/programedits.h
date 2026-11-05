#pragma once

#include "model/program.h"

#include <QString>
#include <QStringList>
#include <QVector>

// An id that changed in one edit. List models use it to keep the row (and the
// view's selection) instead of seeing one row removed and another inserted.
struct IdRename
{
    enum class Scope {
        Package,
        Component,
        Board,
        Placement,      // within `board`
        BoardFiducial,  // within `board`
        BadMark,        // within `board`
        PanelFiducial,
        BoardInstance,
        Feeder,
    };

    Scope scope = Scope::Package;
    QString board;  // Board::id after the edit; only for board-scoped kinds
    QString from;
    QString to;
};

// What else a removal takes with it, for the confirmation shown to the user.
struct RemovalImpact
{
    int components = 0;
    int placements = 0;
    int feeders = 0;
    int instances = 0;

    bool isEmpty() const { return components + placements + feeders + instances == 0; }
};

// Edits that keep references consistent: renaming an id updates everything
// that refers to it, removing an element removes what depends on it. They do
// not validate; ProgramDocument runs ProgramValidator on the result.
namespace ProgramEdits {

// prefix + the smallest number >= 1 that is not in `existing`.
QString uniqueId(const QString &prefix, const QStringList &existing);

// A program that passes ProgramValidator: one board and one board instance.
// Timestamps are left to the caller.
Program blankProgram();

void renamePackage(Program &program, int index, const QString &id);
void renameComponent(Program &program, int index, const QString &id);
void renameBoard(Program &program, int index, const QString &id);
void renamePlacement(Program &program, int board, int index, const QString &id);

RemovalImpact packageRemovalImpact(const Program &program, int index);
RemovalImpact componentRemovalImpact(const Program &program, int index);
RemovalImpact boardRemovalImpact(const Program &program, int index);

// Also removes the components using it, and their placements and feeders.
void removePackage(Program &program, int index);
// Also removes the placements on every board and the feeders using it.
void removeComponent(Program &program, int index);
// Also removes the board instances of it.
void removeBoard(Program &program, int index);
// Also removes it from the skip lists of instances of that board.
void removePlacement(Program &program, int board, int index);

}  // namespace ProgramEdits
