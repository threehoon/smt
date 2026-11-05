#pragma once

#include <QString>
#include <QStringList>

#include <array>

// The fields a CAD placement file can provide. Designator, X and Y are
// required; the others have defaults (rotation 0, top side, empty texts).
enum class CadField { Designator, X, Y, Rotation, Side, Footprint, Value };

constexpr int kCadFieldCount = 7;

enum class CadUnit { Millimeter, Mil, Inch };

// Which file column holds each field; -1 = not in the file.
struct CadMapping
{
    std::array<int, kCadFieldCount> columns{{-1, -1, -1, -1, -1, -1, -1}};

    int column(CadField field) const { return columns[static_cast<int>(field)]; }
    void setColumn(CadField field, int column) { columns[static_cast<int>(field)] = column; }
    // Designator, X and Y all mapped.
    bool isComplete() const;
};

inline bool operator==(const CadMapping &a, const CadMapping &b)
{
    return a.columns == b.columns;
}

// Recognises the header names CAD tools write into their placement files
// (KiCad .pos / .csv, Altium Pick Place, EasyEDA / JLC, Eagle, generic ones).
namespace CadColumns {

// The field a header title stands for, with a rank when several titles map to
// the same field (Altium writes Center-X, Ref-X and Pad-X: the body center
// wins). Returns false when the title is not recognised.
bool recognize(const QString &title, CadField *field, int *rank = nullptr);

// Best column per field from the header titles; unrecognised fields stay -1.
CadMapping guess(const QStringList &titles);

// Number of fields recognised in `titles`.
int recognizedCount(const QStringList &titles);

// A unit written into a title, e.g. "Center-X(mm)", "Ref X (mil)".
bool unitInTitle(const QString &title, CadUnit *unit);

// A unit stated in a comment, e.g. KiCad's "## Unit = mm, Angle = deg." or
// Altium's "Units used in this file: Metric".
bool unitInText(const QString &text, CadUnit *unit);

// Name of the tool that wrote a file with these titles, or empty.
QString formatName(const QStringList &titles);

// "mm", "mil", "mils", "in", "inch", "inches", "metric", "imperial" (any case).
bool parseUnit(const QString &word, CadUnit *unit);

// Factor from `unit` to mm.
double toMillimeter(CadUnit unit);

}  // namespace CadColumns
