#include "cadimport/cadtable.h"

#include "cadimport/cadcolumns.h"

#include <QCoreApplication>
#include <QHash>
#include <QPair>
#include <QRegularExpression>
#include <QTextCodec>

#include <algorithm>
#include <tuple>

QString CadTable::columnName(int column) const
{
    if (column >= 0 && column < titles.size() && !titles.at(column).isEmpty())
        return titles.at(column);
    return QCoreApplication::translate("CadTable", "第 %1 列").arg(column + 1);
}

namespace CadTableParser {

namespace {

// Lines the header search looks at; preambles are a few lines long.
constexpr int kHeaderSearchLines = 60;

bool isComment(const QString &trimmed)
{
    return trimmed.startsWith(QLatin1Char('#')) || trimmed.startsWith(QLatin1String("//"));
}

// KiCad writes its header as a comment: "# Ref Val Package ...".
QString withoutCommentMarker(const QString &line)
{
    QString t = line.trimmed();
    int i = 0;
    while (i < t.size() && t.at(i) == QLatin1Char('#'))
        ++i;
    return t.mid(i).trimmed();
}

QStringList splitText(const QString &text)
{
    static const QRegularExpression eol(QStringLiteral("\\r\\n|\\n|\\r"));
    QString t = text;
    if (t.startsWith(QChar(0xFEFF)))
        t.remove(0, 1);
    return t.split(eol);
}

int findHeader(const QStringList &lines, CadTable::Delimiter delimiter)
{
    int best = 0;
    int bestCount = 0;
    const int n = std::min<int>(lines.size(), kHeaderSearchLines);
    for (int i = 0; i < n; ++i) {
        const QString t = withoutCommentMarker(lines.at(i));
        if (t.isEmpty())
            continue;
        const QStringList titles = splitLine(t, delimiter);
        if (!CadColumns::guess(titles).isComplete())
            continue;
        const int count = CadColumns::recognizedCount(titles);
        if (count > bestCount) {
            best = i + 1;
            bestCount = count;
        }
    }
    return best;
}

CadTable build(const QStringList &lines, CadTable::Delimiter delimiter, int headerLine)
{
    CadTable table;
    table.delimiter = delimiter;
    if (headerLine > 0 && headerLine <= lines.size()) {
        table.headerLine = headerLine;
        table.titles = splitLine(withoutCommentMarker(lines.at(headerLine - 1)), delimiter);
    }

    for (int i = 0; i < lines.size(); ++i) {
        const int lineNo = i + 1;
        if (lineNo == table.headerLine)
            continue;
        const QString t = lines.at(i).trimmed();
        if (t.isEmpty())
            continue;
        if (lineNo < table.headerLine || isComment(t)) {
            table.notes << t;
            continue;
        }
        const QStringList fields = splitLine(lines.at(i), delimiter);
        if (std::all_of(fields.cbegin(), fields.cend(),
                        [](const QString &f) { return f.isEmpty(); }))
            continue;
        table.rows.append({lineNo, fields});
    }

    // Without a header, text before the data (a title line, a date) would
    // look like broken rows: lines before the first row of the usual width
    // become notes.
    if (table.headerLine == 0 && !table.rows.isEmpty()) {
        QHash<int, int> widths;
        for (const CadTable::Row &r : table.rows)
            ++widths[r.fields.size()];
        int width = 0;
        int widthCount = 0;
        for (auto it = widths.cbegin(); it != widths.cend(); ++it) {
            // Most common; on a tie the wider one (a title line is narrow).
            if (it.value() > widthCount || (it.value() == widthCount && it.key() > width)) {
                width = it.key();
                widthCount = it.value();
            }
        }
        int first = 0;
        while (first < table.rows.size() && table.rows.at(first).fields.size() != width)
            ++first;
        for (int i = 0; i < first; ++i)
            table.notes << table.rows.at(i).fields.join(QLatin1Char(' '));
        table.rows.remove(0, first);
    }

    table.columnCount = table.titles.size();
    for (const CadTable::Row &r : table.rows)
        table.columnCount = std::max(table.columnCount, r.fields.size());
    return table;
}

// Number of rows of the most common width (at least 3 columns), and that width.
QPair<int, int> consistentRows(const CadTable &table)
{
    QHash<int, int> widths;
    for (const CadTable::Row &r : table.rows) {
        if (r.fields.size() >= 3)
            ++widths[r.fields.size()];
    }
    QPair<int, int> best{0, 0};
    for (auto it = widths.cbegin(); it != widths.cend(); ++it)
        best = std::max(best, qMakePair(it.value(), it.key()));
    return best;
}

// Fields that read as a number ('.' or one ',' as decimal separator).
// Placement data is mostly numbers: with decimal commas and few columns,
// "U1;25,0;20,0" split at commas is as regular as split at semicolons, but
// only the semicolon split gives numbers.
int numericFields(const CadTable &table)
{
    int n = 0;
    for (const CadTable::Row &r : table.rows) {
        for (QString f : r.fields) {
            if (!f.contains(QLatin1Char('.')) && f.count(QLatin1Char(',')) == 1)
                f.replace(QLatin1Char(','), QLatin1Char('.'));
            bool ok = false;
            f.toDouble(&ok);
            n += ok ? 1 : 0;
        }
    }
    return n;
}

QChar delimiterChar(CadTable::Delimiter delimiter)
{
    switch (delimiter) {
    case CadTable::Delimiter::Comma:
        return QLatin1Char(',');
    case CadTable::Delimiter::Tab:
        return QLatin1Char('\t');
    case CadTable::Delimiter::Semicolon:
        return QLatin1Char(';');
    case CadTable::Delimiter::Whitespace:
        break;
    }
    return QChar();
}

}  // namespace

QString decode(const QByteArray &bytes)
{
    QString text;
    if (QTextCodec *bom = QTextCodec::codecForUtfText(bytes, nullptr)) {
        text = bom->toUnicode(bytes);
    } else {
        QTextCodec *utf8 = QTextCodec::codecForName("UTF-8");
        QTextCodec::ConverterState state;
        text = utf8->toUnicode(bytes.constData(), bytes.size(), &state);
        // Not UTF-8: an ANSI file. GB18030 (a superset of GBK) rather than
        // the system code page, so the result does not depend on the
        // machine; ASCII-only files decode the same either way.
        if (state.invalidChars > 0)
            text = QTextCodec::codecForName("GB18030")->toUnicode(bytes);
    }
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);
    return text;
}

QStringList splitLine(const QString &line, CadTable::Delimiter delimiter)
{
    const bool whitespace = delimiter == CadTable::Delimiter::Whitespace;
    const QChar sep = delimiterChar(delimiter);
    QStringList out;
    QString cur;
    bool inQuotes = false;
    bool started = false;  // whitespace mode: a token (maybe "") has begun

    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    cur += c;
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur += c;
            }
            continue;
        }
        if (c == QLatin1Char('"') && cur.trimmed().isEmpty()) {
            inQuotes = true;
            started = true;
            cur.clear();
            continue;
        }
        if (whitespace) {
            if (c.isSpace()) {
                if (started || !cur.isEmpty())
                    out << cur;
                cur.clear();
                started = false;
            } else {
                cur += c;
            }
        } else if (c == sep) {
            out << cur.trimmed();
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (whitespace) {
        if (started || !cur.isEmpty())
            out << cur;
    } else {
        out << cur.trimmed();
    }
    return out;
}

CadTable parse(const QString &text, const CadTableOptions &options)
{
    const QStringList lines = splitText(text);

    QVector<CadTable::Delimiter> candidates;
    if (options.autoDelimiter) {
        // On a tie the earlier one wins; whitespace splits almost anything.
        candidates = {CadTable::Delimiter::Comma, CadTable::Delimiter::Tab,
                      CadTable::Delimiter::Semicolon, CadTable::Delimiter::Whitespace};
    } else {
        candidates = {options.delimiter};
    }

    CadTable best;
    std::tuple<int, int, int, int, int> bestScore{-1, -1, -1, -1, -1};
    for (const CadTable::Delimiter d : candidates) {
        const int header = options.autoHeader ? findHeader(lines, d) : options.headerLine;
        CadTable table = build(lines, d, header);
        const QPair<int, int> rows = consistentRows(table);
        const std::tuple<int, int, int, int, int> score{
            CadColumns::guess(table.titles).isComplete() ? 1 : 0,
            CadColumns::recognizedCount(table.titles), rows.first, numericFields(table),
            rows.second};
        if (score > bestScore) {
            bestScore = score;
            best = std::move(table);
        }
    }
    return best;
}

}  // namespace CadTableParser
