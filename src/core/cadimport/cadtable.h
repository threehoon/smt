#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

// A placement (centroid) file exported by a CAD tool, split into columns.
// The formats differ in delimiter, quoting, comment lines and where the
// header is, but all are a table of text: KiCad .pos (spaces, header in a
// "# Ref Val ..." comment), KiCad / Altium / EasyEDA CSV (commas, quotes,
// a few lines of preamble before the header), generic tab or semicolon files.
struct CadTable
{
    enum class Delimiter { Comma, Tab, Semicolon, Whitespace };

    struct Row
    {
        int line = 0;  // 1-based line in the file
        QStringList fields;
    };

    Delimiter delimiter = Delimiter::Comma;
    int headerLine = 0;    // 1-based; 0 = the file has no header line
    QStringList titles;    // header fields; empty without a header
    int columnCount = 0;   // widest of header and rows
    QVector<Row> rows;     // data lines after the header
    QStringList notes;     // lines that are neither header nor data, e.g. "## Unit = mm"

    // Title of a column for the user: the header text, or "第 n 列".
    QString columnName(int column) const;
};

struct CadTableOptions
{
    bool autoDelimiter = true;
    CadTable::Delimiter delimiter = CadTable::Delimiter::Comma;  // when not auto
    bool autoHeader = true;
    int headerLine = 0;  // when not auto: 1-based, 0 = no header
};

namespace CadTableParser {

// File bytes to text: UTF-8 (with or without BOM), UTF-16 / 32 with BOM,
// else GB18030 / GBK (what Altium and others write as "ANSI" on Chinese Windows).
QString decode(const QByteArray &bytes);

// Splits the text into a table. With autoDelimiter / autoHeader, picks the
// delimiter and header line that recognise the most field names
// (CadColumns); without a recognisable header, the delimiter that gives the
// most rows of equal width, then the most numeric fields, then the most
// columns, and no header.
CadTable parse(const QString &text, const CadTableOptions &options = {});

// One line split by `delimiter`; double quotes group, "" is a literal quote.
QStringList splitLine(const QString &line, CadTable::Delimiter delimiter);

}  // namespace CadTableParser
