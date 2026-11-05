#include "cadimport/cadplacements.h"
#include "cadimport/cadtable.h"
#include "io/programreader.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QtTest>

#include <cmath>

// Every file in samples/cad/ must be read by the current CAD importer as
// expected, and the designators it shares with board CTRL of
// samples/example.smt.xml must come out at the same place, side and angle.
// A new file needs an entry in kExpected.
namespace {

const QString kCadDir = QStringLiteral(SMTPRO_SAMPLES_DIR "/cad");
const QString kSample = QStringLiteral(SMTPRO_SAMPLES_DIR "/example.smt.xml");

// The files hold two decimals of mil; that is 0.0002 mm.
constexpr double kTolerance = 0.001;

struct Expected
{
    const char *file;
    const char *format;  // CadColumns::formatName; empty for a generic table
    CadTable::Delimiter delimiter;
    int headerLine;
    CadUnit unit;
    int rows;
    int errors;
    // Board origin in the file, mm: CAD tools export absolute coordinates.
    double originX;
    double originY;
};

const Expected kExpected[] = {
    {"altium-pickplace.csv", "Altium", CadTable::Delimiter::Comma, 12, CadUnit::Mil, 6, 0,
     100.0, 50.0},
    {"jlc-cpl.csv", "EasyEDA / JLC", CadTable::Delimiter::Comma, 1, CadUnit::Millimeter, 5, 0,
     0.0, 0.0},
    {"kicad-pos.csv", "KiCad", CadTable::Delimiter::Comma, 1, CadUnit::Millimeter, 8, 0, 0.0,
     0.0},
    {"kicad.pos", "KiCad", CadTable::Delimiter::Whitespace, 4, CadUnit::Millimeter, 5, 0, 0.0,
     0.0},
    {"with-errors.csv", "", CadTable::Delimiter::Comma, 1, CadUnit::Millimeter, 7, 4, 0.0, 0.0},
};

const Expected *expectedFor(const QString &file)
{
    for (const Expected &e : kExpected) {
        if (file == QLatin1String(e.file))
            return &e;
    }
    return nullptr;
}

struct Read
{
    CadTable table;
    CadExtractOptions options;
    QVector<CadPlacement> rows;
};

Read readCad(const QString &path, double originX, double originY)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("cannot open %s", qPrintable(path));
    Read r;
    r.table = CadTableParser::parse(CadTableParser::decode(f.readAll()));
    r.options.mapping = CadColumns::guess(r.table.titles);
    r.options.unit = CadPlacements::detectUnit(r.table, r.options.mapping);
    r.options.originX = originX;
    r.options.originY = originY;
    r.rows = CadPlacements::extract(r.table, r.options);
    return r;
}

int errorCount(const QVector<CadPlacement> &rows)
{
    return int(std::count_if(rows.cbegin(), rows.cend(),
                             [](const CadPlacement &p) { return !p.isValid(); }));
}

}  // namespace

class TstCadSamples : public QObject
{
    Q_OBJECT

private slots:
    void everyFileHasExpectations();
    void reads_data();
    void reads();
    void errorRows();
};

void TstCadSamples::everyFileHasExpectations()
{
    const QStringList files = QDir(kCadDir).entryList(QDir::Files);
    QVERIFY2(!files.isEmpty(), qPrintable(QStringLiteral("no files in ") + kCadDir));
    for (const QString &f : files)
        QVERIFY2(expectedFor(f), qPrintable(QStringLiteral("no expectation for ") + f));
    for (const Expected &e : kExpected)
        QVERIFY2(files.contains(QLatin1String(e.file)), e.file);
}

void TstCadSamples::reads_data()
{
    QTest::addColumn<QString>("file");
    for (const Expected &e : kExpected)
        QTest::newRow(e.file) << QString::fromLatin1(e.file);
}

void TstCadSamples::reads()
{
    QFETCH(QString, file);
    const Expected &e = *expectedFor(file);
    const Read r = readCad(kCadDir + QLatin1Char('/') + file, e.originX, e.originY);

    QCOMPARE(CadColumns::formatName(r.table.titles), QString::fromUtf8(e.format));
    QCOMPARE(r.table.delimiter, e.delimiter);
    QCOMPARE(r.table.headerLine, e.headerLine);
    QVERIFY(r.options.mapping.isComplete());
    QCOMPARE(r.options.unit, e.unit);
    QCOMPARE(r.rows.size(), e.rows);
    QCOMPARE(errorCount(r.rows), e.errors);

    const ReadResult sample = ProgramReader::readFile(kSample);
    QVERIFY(sample.ok());
    QHash<QString, Placement> board;
    for (const Placement &p : sample.program.boards.first().placements)
        board.insert(p.id, p);

    int shared = 0;
    for (const CadPlacement &row : r.rows) {
        if (!row.isValid() || !board.contains(row.designator))
            continue;
        const Placement &p = board.value(row.designator);
        const QString where = file + QLatin1Char(' ') + row.designator;
        QVERIFY2(std::abs(row.x - p.x) < kTolerance,
                 qPrintable(where + QStringLiteral(" x %1 != %2").arg(row.x).arg(p.x)));
        QVERIFY2(std::abs(row.y - p.y) < kTolerance,
                 qPrintable(where + QStringLiteral(" y %1 != %2").arg(row.y).arg(p.y)));
        QVERIFY2(row.rotation == p.rotation, qPrintable(where + QStringLiteral(" rotation")));
        QVERIFY2(row.side == p.side, qPrintable(where + QStringLiteral(" side")));
        ++shared;
    }
    // Each file describes board CTRL, not just any board.
    QVERIFY2(shared >= 2, qPrintable(file + QStringLiteral(": %1 shared designators").arg(shared)));
}

void TstCadSamples::errorRows()
{
    const Read r = readCad(kCadDir + QStringLiteral("/with-errors.csv"), 0.0, 0.0);
    QStringList errors;
    for (const CadPlacement &p : r.rows)
        errors << p.error;
    QCOMPARE(errors, (QStringList{
                         QString(),
                         QString(),
                         QStringLiteral("位号与第 3 行重复"),
                         QStringLiteral("缺少位号"),
                         QStringLiteral("X 不是有效数值：abc"),
                         QStringLiteral("无法识别的面：sideways"),
                         QString(),  // R4 is outside the board, which is only a warning
                     }));
}

QTEST_GUILESS_MAIN(TstCadSamples)
#include "tst_cadsamples.moc"
