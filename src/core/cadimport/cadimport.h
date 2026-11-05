#pragma once

#include "cadimport/cadplacements.h"
#include "model/program.h"

#include <QString>
#include <QStringList>
#include <QVector>

// The rows of a CAD file with the same footprint and value. They become
// placements of one component: an existing one, or one created by the import.
struct CadPart
{
    QString footprint;
    QString value;
    QStringList designators;  // valid rows only, in file order

    // Component the placements use; empty = create one.
    QString component;
    // Only when creating a component: its package; empty = create one too.
    QString package;
};

inline bool operator==(const CadPart &a, const CadPart &b)
{
    return a.footprint == b.footprint && a.value == b.value && a.designators == b.designators
        && a.component == b.component && a.package == b.package;
}

enum class CadImportMode {
    // Designators in the file are updated (component, position, rotation,
    // side; "enabled" is kept), new ones appended, the others kept.
    Merge,
    // The board's placements become exactly the file's, in file order.
    // Designators that stay keep "enabled" and their skip entries; skip
    // entries of designators that are gone are removed.
    Replace,
};

// What the import does with one row of the file.
enum class CadRowOutcome { NotImported, Added, Updated, Unchanged };

struct CadRowResult
{
    CadRowOutcome outcome = CadRowOutcome::NotImported;
    bool outsideBoard = false;
};

// What an import does or did to the board.
struct CadImportSummary
{
    int added = 0;
    int updated = 0;    // existing designator with a different value
    int unchanged = 0;  // existing designator, nothing to change
    int removed = 0;    // Replace only
    int outsideBoard = 0;
    QStringList newComponents;
    QStringList newPackages;
    // Component id per entry of `parts`, after creating the missing ones,
    // and the package of that component.
    QStringList partComponents;
    QStringList partPackages;
    // One per entry of `rows`.
    QVector<CadRowResult> rowResults;
};

// Turns CAD rows into placements of one board. Works on a Program; the
// caller validates the result (ProgramDocument::edit).
namespace CadImport {

// The valid rows grouped by (footprint, value), in order of first row, each
// with the suggestion of suggest().
QVector<CadPart> groupParts(const QVector<CadPlacement> &rows, const Program &program);

// Component (or, failing that, package) to use for a footprint / value:
//  1. a component with this value whose package id is the footprint;
//  2. a component whose id is the one the import creates for it
//     (componentIdFor), e.g. from importing the same file before;
//  3. no component; a package whose id is the footprint, if any.
void suggest(CadPart &part, const Program &program);

// Id the import gives a new component: "footprint-value", or the one that
// is not empty, or "PART". Not made unique.
QString componentIdFor(const QString &footprint, const QString &value);

// Placeholder size of packages the import creates; the user completes it on
// the packages page.
constexpr double kNewPackageSize = 1.0;

// Applies the valid rows of `rows` to board `board`, creating the packages
// and components the parts ask for. `parts` must come from groupParts on
// the same rows (choices may have been changed).
CadImportSummary apply(Program &program, int board, const QVector<CadPlacement> &rows,
                       const QVector<CadPart> &parts, CadImportMode mode);

}  // namespace CadImport
