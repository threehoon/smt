#include "cadimport/cadimport.h"
#include "cadimport/cadplacements.h"
#include "cadimport/cadtable.h"

#include "io/programreader.h"
#include "io/programwriter.h"
#include "validation/programvalidator.h"

#include <QTextCodec>
#include <QtTest>

namespace {

const QString kSample = QStringLiteral(SMTPRO_SAMPLES_DIR "/example.smt.xml");

// KiCad 6+ "Fabrication outputs > Component placement", ASCII, mm.
const char kKicadPos[] =
    "### Footprint positions - created on 2026-11-06 10:00:00 ###\n"
    "### Printed by KiCad version 7.0.8\n"
    "## Unit = mm, Angle = deg.\n"
    "## Side : All\n"
    "# Ref     Val       Package                PosX       PosY       Rot  Side\n"
    "C1        100nF     C_0402_1005Metric   20.5000    24.0000   90.0000  top\n"
    "R1        10K       R_0603_1608Metric   12.0000    30.0000    0.0000  top\n"
    "R7        10K       R_0603_1608Metric   14.0000    30.0000  -90.0000  bottom\n"
    "## End\n";

// Altium "Pick and Place" CSV export, in mil, with its preamble.
const char kAltiumCsv[] =
    "Altium Designer Pick and Place Locations\r\n"
    "D:\\proj\\CTRL.PcbDoc\r\n"
    "\r\n"
    "========================================================================================\r\n"
    "File Design Information:\r\n"
    "\r\n"
    "Date:       06/11/26\r\n"
    "Time:       10:00\r\n"
    "Revision:   Not in VersionControl\r\n"
    "Variant:    No variations\r\n"
    "Units used: mil\r\n"
    "\r\n"
    "\"Designator\",\"Comment\",\"Layer\",\"Footprint\",\"Center-X(mil)\",\"Center-Y(mil)\","
    "\"Rotation\",\"Description\"\r\n"
    "\"U1\",\"STM32G031K8U6\",\"TopLayer\",\"QFN32-5x5\",\"1000\",\"800\",\"0\",\"MCU, 64K\"\r\n"
    "\"C9\",\"1uF\",\"BottomLayer\",\"C0402\",\"500\",\"250\",\"180\",\"\"\r\n";

// Generic file without a header.
const char kHeaderless[] =
    "Board CTRL\n"
    "U1;STM32G031K8U6;25,0;20,0;0\n"
    "C1;100nF;20,5;24,0;90\n"
    "C2;100nF;29,5;16,0;270\n";

CadTable parse(const char *text, const CadTableOptions &options = {})
{
    return CadTableParser::parse(QString::fromUtf8(text), options);
}

QVector<CadPlacement> extract(const CadTable &table)
{
    CadExtractOptions o;
    o.mapping = CadColumns::guess(table.titles);
    o.unit = CadPlacements::detectUnit(table, o.mapping);
    return CadPlacements::extract(table, o);
}

Program sample()
{
    const ReadResult r = ProgramReader::readFile(kSample);
    if (!r.ok())
        qFatal("sample does not load");
    return r.program;
}

int errorCount(const Program &p)
{
    int n = 0;
    for (const Diagnostic &d : ProgramValidator::validate(p))
        n += d.severity == Diagnostic::Severity::Error ? 1 : 0;
    return n;
}

const Placement *placement(const Board &b, const QString &id)
{
    for (const Placement &p : b.placements) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

const Component *component(const Program &prog, const QString &id)
{
    for (const Component &c : prog.components) {
        if (c.id == id)
            return &c;
    }
    return nullptr;
}

CadPlacement row(const QString &designator, const QString &footprint, const QString &value,
                 double x, double y)
{
    CadPlacement p;
    p.line = 1;
    p.designator = designator;
    p.footprint = footprint;
    p.value = value;
    p.x = x;
    p.y = y;
    return p;
}

}  // namespace

class TstCadImport : public QObject
{
    Q_OBJECT

private slots:
    void splitLine_data();
    void splitLine();
    void parseLength_data();
    void parseLength();
    void parseSide();
    void recognizesTitles();

    void kicadPos();
    void altiumCsv();
    void easyedaTab();
    void headerless();
    void manualOptions();
    void decodesEncodings();

    void rowErrors();
    void origin();

    void groupsAndSuggests();
    void mergeKeepsOthers();
    void replaceFollowsFile();
    void createsUniqueIds();
    void sharesNewPackage();
    void resultRoundTrips();
};

void TstCadImport::splitLine_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("delimiter");
    QTest::addColumn<QStringList>("fields");

    const int comma = int(CadTable::Delimiter::Comma);
    const int ws = int(CadTable::Delimiter::Whitespace);
    QTest::newRow("plain") << "a,b,c" << comma << QStringList{"a", "b", "c"};
    QTest::newRow("trims") << " a , b ,c " << comma << QStringList{"a", "b", "c"};
    QTest::newRow("empty fields") << "a,,c," << comma << QStringList{"a", "", "c", ""};
    QTest::newRow("quoted comma") << "\"a,1\",b" << comma << QStringList{"a,1", "b"};
    QTest::newRow("escaped quote") << "\"say \"\"hi\"\"\",x" << comma
                                   << QStringList{"say \"hi\"", "x"};
    QTest::newRow("quoted empty") << "\"\",x" << comma << QStringList{"", "x"};
    QTest::newRow("whitespace") << "  C1   100nF\tC0402  1.0 " << ws
                                << QStringList{"C1", "100nF", "C0402", "1.0"};
    QTest::newRow("whitespace quoted") << "C1 \"10 K\" \"\" 2" << ws
                                       << QStringList{"C1", "10 K", "", "2"};
}

void TstCadImport::splitLine()
{
    QFETCH(QString, line);
    QFETCH(int, delimiter);
    QFETCH(QStringList, fields);
    QCOMPARE(CadTableParser::splitLine(line, CadTable::Delimiter(delimiter)), fields);
}

void TstCadImport::parseLength_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("unit");
    QTest::addColumn<bool>("ok");
    QTest::addColumn<double>("mm");

    const int mm = int(CadUnit::Millimeter);
    const int mil = int(CadUnit::Mil);
    QTest::newRow("mm") << "12.5" << mm << true << 12.5;
    QTest::newRow("negative") << "-3.25" << mm << true << -3.25;
    QTest::newRow("decimal comma") << "12,5" << mm << true << 12.5;
    QTest::newRow("mil") << "1000" << mil << true << 25.4;
    QTest::newRow("suffix mm") << "12.70mm" << mil << true << 12.7;
    QTest::newRow("suffix mil") << "500mil" << mm << true << 12.7;
    QTest::newRow("suffix space") << "1 in" << mm << true << 25.4;
    QTest::newRow("exponent") << "1e1" << mm << true << 10.0;
    QTest::newRow("unknown suffix") << "12cm" << mm << false << 0.0;
    QTest::newRow("thousands") << "1,234.5" << mm << false << 0.0;
    QTest::newRow("text") << "abc" << mm << false << 0.0;
    QTest::newRow("empty") << "" << mm << false << 0.0;
    QTest::newRow("inf") << "inf" << mm << false << 0.0;
}

void TstCadImport::parseLength()
{
    QFETCH(QString, text);
    QFETCH(int, unit);
    QFETCH(bool, ok);
    QFETCH(double, mm);
    double v = 0.0;
    QCOMPARE(CadPlacements::parseLength(text, CadUnit(unit), &v), ok);
    if (ok)
        QVERIFY2(qAbs(v - mm) < 1e-9, qPrintable(QString::number(v)));
}

void TstCadImport::parseSide()
{
    const QStringList top{"top", "TOP", "T", "TopLayer", "Top Layer", "F.Cu", "front", "顶面"};
    const QStringList bottom{"bottom", "Bot", "B", "BottomLayer", "B.Cu", "back", "底层"};
    for (const QString &s : top) {
        Side side = Side::Bottom;
        QVERIFY2(CadPlacements::parseSide(s, &side), qPrintable(s));
        QCOMPARE(side, Side::Top);
    }
    for (const QString &s : bottom) {
        Side side = Side::Top;
        QVERIFY2(CadPlacements::parseSide(s, &side), qPrintable(s));
        QCOMPARE(side, Side::Bottom);
    }
    Side side;
    QVERIFY(!CadPlacements::parseSide(QStringLiteral("inner1"), &side));
    QVERIFY(!CadPlacements::parseSide(QString(), &side));
}

void TstCadImport::recognizesTitles()
{
    // Altium writes three X columns; the body center wins wherever it is.
    const QStringList titles{"Designator", "Pad-X(mm)", "Ref-X(mm)", "Center-X(mm)",
                             "Center-Y(mm)", "Ref-Y(mm)", "Comment", "Description"};
    const CadMapping m = CadColumns::guess(titles);
    QCOMPARE(m.column(CadField::Designator), 0);
    QCOMPARE(m.column(CadField::X), 3);
    QCOMPARE(m.column(CadField::Y), 4);
    QCOMPARE(m.column(CadField::Value), 6);
    QCOMPARE(m.column(CadField::Footprint), -1);
    QVERIFY(m.isComplete());

    // A real "Value" column beats Altium's "Comment".
    QCOMPARE(CadColumns::guess({"Comment", "Value"}).column(CadField::Value), 1);

    CadUnit unit;
    QVERIFY(CadColumns::unitInTitle(QStringLiteral("Center-X(mil)"), &unit));
    QCOMPARE(unit, CadUnit::Mil);
    QVERIFY(!CadColumns::unitInTitle(QStringLiteral("PosX"), &unit));
    QVERIFY(CadColumns::unitInText(QStringLiteral("## Unit = inches, Angle = deg."), &unit));
    QCOMPARE(unit, CadUnit::Inch);
    QVERIFY(CadColumns::unitInText(QStringLiteral("Units used: Metric"), &unit));
    QCOMPARE(unit, CadUnit::Millimeter);
    // "in" in an ordinary sentence is not a unit.
    QVERIFY(!CadColumns::unitInText(QStringLiteral("Units used in this file"), &unit));
}

void TstCadImport::kicadPos()
{
    const CadTable t = parse(kKicadPos);
    QCOMPARE(t.delimiter, CadTable::Delimiter::Whitespace);
    QCOMPARE(t.headerLine, 5);
    QCOMPARE(t.titles, (QStringList{"Ref", "Val", "Package", "PosX", "PosY", "Rot", "Side"}));
    QCOMPARE(t.rows.size(), 3);
    QCOMPARE(t.rows.first().line, 6);
    QCOMPARE(CadColumns::formatName(t.titles), QStringLiteral("KiCad"));

    const CadMapping m = CadColumns::guess(t.titles);
    for (int f = 0; f < kCadFieldCount; ++f)
        QCOMPARE(m.columns[f], (std::array<int, 7>{0, 3, 4, 5, 6, 2, 1})[f]);
    bool found = false;
    QCOMPARE(CadPlacements::detectUnit(t, m, &found), CadUnit::Millimeter);
    QVERIFY(found);

    const QVector<CadPlacement> rows = extract(t);
    QCOMPARE(rows.size(), 3);
    for (const CadPlacement &r : rows)
        QVERIFY2(r.isValid(), qPrintable(r.error));
    QCOMPARE(rows[0].designator, QStringLiteral("C1"));
    QCOMPARE(rows[0].value, QStringLiteral("100nF"));
    QCOMPARE(rows[0].footprint, QStringLiteral("C_0402_1005Metric"));
    QCOMPARE(rows[0].x, 20.5);
    QCOMPARE(rows[0].y, 24.0);
    QCOMPARE(rows[0].rotation, 90.0);
    QCOMPARE(rows[0].side, Side::Top);
    // Angles are normalised; bottom side positions are taken as written.
    QCOMPARE(rows[2].rotation, 270.0);
    QCOMPARE(rows[2].side, Side::Bottom);
    QCOMPARE(rows[2].x, 14.0);
}

void TstCadImport::altiumCsv()
{
    const CadTable t = parse(kAltiumCsv);
    QCOMPARE(t.delimiter, CadTable::Delimiter::Comma);
    QCOMPARE(t.headerLine, 13);
    QCOMPARE(t.rows.size(), 2);
    QCOMPARE(CadColumns::formatName(t.titles), QStringLiteral("Altium"));
    QVERIFY(t.notes.contains(QStringLiteral("Units used: mil")));

    const CadMapping m = CadColumns::guess(t.titles);
    QCOMPARE(m.column(CadField::Value), 1);
    QCOMPARE(m.column(CadField::Footprint), 3);
    QCOMPARE(CadPlacements::detectUnit(t, m), CadUnit::Mil);

    const QVector<CadPlacement> rows = extract(t);
    QCOMPARE(rows.size(), 2);
    QVERIFY(rows[0].isValid());
    QCOMPARE(rows[0].designator, QStringLiteral("U1"));
    QCOMPARE(rows[0].value, QStringLiteral("STM32G031K8U6"));
    QVERIFY(qAbs(rows[0].x - 25.4) < 1e-9);
    QVERIFY(qAbs(rows[0].y - 20.32) < 1e-9);
    QCOMPARE(rows[1].side, Side::Bottom);
    QCOMPARE(rows[1].rotation, 180.0);
}

void TstCadImport::easyedaTab()
{
    // EasyEDA / JLC CPL, tab separated, values carry their unit.
    const char text[] = "Designator\tFootprint\tMid X\tMid Y\tRef X\tRef Y\tPad X\tPad Y\tLayer\t"
                        "Rotation\tComment\n"
                        "R1\tR0603\t12.7mm\t30mm\t1mm\t1mm\t1mm\t1mm\tT\t0\t10K\n";
    const CadTable t = parse(text);
    QCOMPARE(t.delimiter, CadTable::Delimiter::Tab);
    QCOMPARE(CadColumns::formatName(t.titles), QStringLiteral("EasyEDA / JLC"));
    const CadMapping m = CadColumns::guess(t.titles);
    QCOMPARE(m.column(CadField::X), 2);
    QCOMPARE(m.column(CadField::Y), 3);
    const QVector<CadPlacement> rows = extract(t);
    QCOMPARE(rows.size(), 1);
    QVERIFY2(rows[0].isValid(), qPrintable(rows[0].error));
    QCOMPARE(rows[0].x, 12.7);
    QCOMPARE(rows[0].value, QStringLiteral("10K"));
}

void TstCadImport::headerless()
{
    const CadTable t = parse(kHeaderless);
    QCOMPARE(t.delimiter, CadTable::Delimiter::Semicolon);
    QCOMPARE(t.headerLine, 0);
    QVERIFY(t.titles.isEmpty());
    QCOMPARE(t.columnCount, 5);
    // The title line before the data is not a broken row.
    QCOMPARE(t.rows.size(), 3);
    QCOMPARE(t.rows.first().line, 2);
    QCOMPARE(t.notes, QStringList{QStringLiteral("Board CTRL")});
    QCOMPARE(t.columnName(2), QStringLiteral("第 3 列"));
    QVERIFY(!CadColumns::guess(t.titles).isComplete());

    CadExtractOptions o;
    o.mapping.setColumn(CadField::Designator, 0);
    o.mapping.setColumn(CadField::Value, 1);
    o.mapping.setColumn(CadField::X, 2);
    o.mapping.setColumn(CadField::Y, 3);
    o.mapping.setColumn(CadField::Rotation, 4);
    const QVector<CadPlacement> rows = CadPlacements::extract(t, o);
    QCOMPARE(rows.size(), 3);
    QVERIFY(rows[1].isValid());
    QCOMPARE(rows[1].x, 20.5);  // decimal comma
    QCOMPARE(rows[2].rotation, 270.0);

    // Three columns with decimal commas: commas also split every line into
    // three, but only semicolons give numbers.
    const CadTable narrow = parse("U1;25,0;20,0\nC1;20,5;24,0\n");
    QCOMPARE(narrow.delimiter, CadTable::Delimiter::Semicolon);
}

void TstCadImport::manualOptions()
{
    // Forced header line and delimiter are used as given.
    CadTableOptions o;
    o.autoDelimiter = false;
    o.delimiter = CadTable::Delimiter::Comma;
    o.autoHeader = false;
    o.headerLine = 0;
    const CadTable t = parse(kAltiumCsv, o);
    QCOMPARE(t.delimiter, CadTable::Delimiter::Comma);
    QCOMPARE(t.headerLine, 0);
    QVERIFY(t.titles.isEmpty());

    o.headerLine = 13;
    const CadTable h = parse(kAltiumCsv, o);
    QCOMPARE(h.headerLine, 13);
    QCOMPARE(h.titles.first(), QStringLiteral("Designator"));
    QCOMPARE(h.rows.size(), 2);

    // Wrong delimiter: one column per line, no header found.
    o.autoHeader = true;
    o.delimiter = CadTable::Delimiter::Tab;
    const CadTable w = parse(kAltiumCsv, o);
    QCOMPARE(w.headerLine, 0);
    QVERIFY(!CadColumns::guess(w.titles).isComplete());
}

void TstCadImport::decodesEncodings()
{
    const QString text = QStringLiteral("位号,值,封装,X,Y,角度,面\nC1,100nF,C0402,1,2,90,底面\n");

    QTextCodec *gbk = QTextCodec::codecForName("GB18030");
    QVERIFY(gbk);
    const QStringList variants{
        CadTableParser::decode(text.toUtf8()),
        CadTableParser::decode(QByteArray("\xEF\xBB\xBF") + text.toUtf8()),
        CadTableParser::decode(gbk->fromUnicode(text)),
        CadTableParser::decode(QTextCodec::codecForName("UTF-16LE")->fromUnicode(text).prepend("\xFF\xFE")),
    };
    for (const QString &decoded : variants) {
        QCOMPARE(decoded, text);
        const CadTable t = CadTableParser::parse(decoded);
        const QVector<CadPlacement> rows = extract(t);
        QCOMPARE(rows.size(), 1);
        QVERIFY2(rows[0].isValid(), qPrintable(rows[0].error));
        QCOMPARE(rows[0].side, Side::Bottom);
        QCOMPARE(rows[0].footprint, QStringLiteral("C0402"));
    }
}

void TstCadImport::rowErrors()
{
    const char text[] = "Ref,X,Y,Rot,Side\n"
                        "C1,1,2,0,top\n"
                        ",1,2,0,top\n"
                        "C2,abc,2,0,top\n"
                        "C3,1,,0,top\n"
                        "C4,1,2,x,top\n"
                        "C5,1,2,0,inner\n"
                        "C1,5,5,0,top\n"
                        "C6,1\n"
                        "C7,1,2,,\n";
    const QVector<CadPlacement> rows = extract(parse(text));
    QCOMPARE(rows.size(), 9);
    QVERIFY(rows[0].isValid());
    QCOMPARE(rows[1].error, QStringLiteral("缺少位号"));
    QCOMPARE(rows[2].error, QStringLiteral("X 不是有效数值：abc"));
    QCOMPARE(rows[3].error, QStringLiteral("缺少 Y"));
    QCOMPARE(rows[4].error, QStringLiteral("角度不是有效数值：x"));
    QCOMPARE(rows[5].error, QStringLiteral("无法识别的面：inner"));
    QCOMPARE(rows[6].error, QStringLiteral("位号与第 2 行重复"));
    QCOMPARE(rows[7].error, QStringLiteral("缺少 Y"));
    // Empty rotation and side take their defaults.
    QVERIFY(rows[8].isValid());
    QCOMPARE(rows[8].rotation, 0.0);
    QCOMPARE(rows[8].side, Side::Top);
}

void TstCadImport::origin()
{
    const CadTable t = parse("Ref,X,Y\nC1,110.5,-45\n");
    CadExtractOptions o;
    o.mapping = CadColumns::guess(t.titles);
    o.unit = CadUnit::Mil;  // origin stays in mm
    o.originX = 100.0;
    o.originY = -50.0;
    const QVector<CadPlacement> rows = CadPlacements::extract(t, o);
    QVERIFY(qAbs(rows[0].x - (110.5 * 0.0254 - 100.0)) < 1e-9);
    QVERIFY(qAbs(rows[0].y - (-45 * 0.0254 + 50.0)) < 1e-9);
}

void TstCadImport::groupsAndSuggests()
{
    Program p = sample();
    p.components.append({QStringLiteral("SOT23-BC847"), QStringLiteral("R0603"), {}, {}});
    QVector<CadPlacement> rows{
        row("C1", "C0402", "100nF", 1, 1),      // matches CC0402-100NF by package + value
        row("C7", "C0402", "100nF", 2, 2),
        row("R9", "R0603", "22R", 3, 3),        // new component, existing package
        row("Q1", "SOT23", "BC847", 4, 4),      // id an earlier import created
        row("X1", "HC49", "8MHz", 5, 5),        // new component and package
        row("C8", "C0402", "100nF", 6, 6),
    };
    rows.append(row("E1", "C0402", "100nF", 7, 7));
    rows.last().error = QStringLiteral("bad");  // invalid rows are not grouped

    const QVector<CadPart> parts = CadImport::groupParts(rows, p);
    Program applied = p;
    const CadImportSummary s = CadImport::apply(applied, 0, rows, parts, CadImportMode::Merge);
    QCOMPARE(s.rowResults.last().outcome, CadRowOutcome::NotImported);
    QCOMPARE(s.added, 5);    // C1 is already on the board
    QCOMPARE(s.updated, 1);
    QCOMPARE(parts.size(), 4);
    QCOMPARE(parts[0].designators, (QStringList{"C1", "C7", "C8"}));
    QCOMPARE(parts[0].component, QStringLiteral("CC0402-100NF"));
    QCOMPARE(parts[1].component, QString());
    QCOMPARE(parts[1].package, QStringLiteral("R0603"));
    QCOMPARE(parts[2].component, QStringLiteral("SOT23-BC847"));
    QCOMPARE(parts[3].component, QString());
    QCOMPARE(parts[3].package, QString());

    QCOMPARE(CadImport::componentIdFor("C0402", "1uF"), QStringLiteral("C0402-1uF"));
    QCOMPARE(CadImport::componentIdFor("C0402", ""), QStringLiteral("C0402"));
    QCOMPARE(CadImport::componentIdFor("", "1uF"), QStringLiteral("1uF"));
    QCOMPARE(CadImport::componentIdFor("", ""), QStringLiteral("PART"));
}

void TstCadImport::mergeKeepsOthers()
{
    Program p = sample();
    const Board before = p.boards.first();
    QVector<CadPlacement> rows{
        row("C1", "C0402", "100nF", 20.5, 24.0),   // same position: unchanged
        row("R2", "R0603", "10K", 13.0, 27.0),     // moved; was disabled
        row("R9", "R0603", "22R", 60.0, 1.0),      // new, outside the 50 mm board
    };
    rows[0].rotation = 90.0;
    rows[1].side = Side::Bottom;
    const QVector<CadPart> parts = CadImport::groupParts(rows, p);
    QCOMPARE(parts.size(), 3);
    QCOMPARE(parts[1].component, QStringLiteral("RC0603-10K"));

    const CadImportSummary s = CadImport::apply(p, 0, rows, parts, CadImportMode::Merge);
    QCOMPARE(s.unchanged, 1);
    QCOMPARE(s.updated, 1);
    QCOMPARE(s.added, 1);
    QCOMPARE(s.removed, 0);
    QCOMPARE(s.outsideBoard, 1);
    QCOMPARE(s.newComponents, QStringList{QStringLiteral("R0603-22R")});
    QVERIFY(s.newPackages.isEmpty());
    QCOMPARE(s.partComponents,
             (QStringList{"CC0402-100NF", "RC0603-10K", "R0603-22R"}));
    QCOMPARE(s.partPackages, (QStringList{"C0402", "R0603", "R0603"}));
    QCOMPARE(s.rowResults.size(), 3);
    QCOMPARE(s.rowResults[0].outcome, CadRowOutcome::Unchanged);
    QCOMPARE(s.rowResults[1].outcome, CadRowOutcome::Updated);
    QCOMPARE(s.rowResults[2].outcome, CadRowOutcome::Added);
    QVERIFY(!s.rowResults[1].outsideBoard);
    QVERIFY(s.rowResults[2].outsideBoard);

    const Board &b = p.boards.first();
    QCOMPARE(b.placements.size(), before.placements.size() + 1);
    // Order kept, the new one appended.
    for (int i = 0; i < before.placements.size(); ++i)
        QCOMPARE(b.placements[i].id, before.placements[i].id);
    QCOMPARE(b.placements.last().id, QStringLiteral("R9"));
    const Placement *r2 = placement(b, QStringLiteral("R2"));
    QCOMPARE(r2->x, 13.0);
    QCOMPARE(r2->side, Side::Bottom);
    QCOMPARE(r2->enabled, false);
    QCOMPARE(*placement(b, QStringLiteral("U1")), *placement(before, QStringLiteral("U1")));
    QCOMPARE(component(p, QStringLiteral("R0603-22R"))->package, QStringLiteral("R0603"));
    QCOMPARE(component(p, QStringLiteral("R0603-22R"))->value, QStringLiteral("22R"));
    QCOMPARE(errorCount(p), 0);
}

void TstCadImport::replaceFollowsFile()
{
    Program p = sample();
    // The sample skips R1 on instance 2.
    QCOMPARE(p.panel.instances[1].skippedPlacements, QStringList{QStringLiteral("R1")});
    QVector<CadPlacement> rows{
        row("R2", "R0603", "10K", 1.0, 1.0),
        row("R1", "R0603", "10K", 2.0, 2.0),
        row("C1", "C0402", "100nF", 20.5, 24.0),
    };
    rows[2].rotation = 90.0;
    QVector<CadPart> parts = CadImport::groupParts(rows, p);
    CadImportSummary s = CadImport::apply(p, 0, rows, parts, CadImportMode::Replace);
    QCOMPARE(s.removed, 3);  // U1, C2, R5
    QCOMPARE(s.updated, 2);
    QCOMPARE(s.unchanged, 1);
    QCOMPARE(s.added, 0);
    const Board &b = p.boards.first();
    QCOMPARE(b.placements.size(), 3);
    QCOMPARE(b.placements[0].id, QStringLiteral("R2"));
    QCOMPARE(b.placements[0].enabled, false);  // kept
    QCOMPARE(b.placements[1].id, QStringLiteral("R1"));
    QCOMPARE(p.panel.instances[1].skippedPlacements, QStringList{QStringLiteral("R1")});
    QCOMPARE(errorCount(p), 0);

    // A skipped designator that is gone takes its skip entry with it.
    rows.removeAt(1);
    parts = CadImport::groupParts(rows, p);
    s = CadImport::apply(p, 0, rows, parts, CadImportMode::Replace);
    QCOMPARE(s.removed, 1);
    QVERIFY(p.panel.instances[1].skippedPlacements.isEmpty());
    QCOMPARE(errorCount(p), 0);
}

void TstCadImport::createsUniqueIds()
{
    Program p = sample();
    // Ids the new component and package would take already exist.
    p.packages.append(p.packages.first());
    p.packages.last().id = QStringLiteral("HC49");
    p.components.append({QStringLiteral("HC49-8MHz"), QStringLiteral("C0402"), {}, {}});
    p.components.append({QStringLiteral("PART"), QStringLiteral("C0402"), {}, {}});

    QVector<CadPlacement> rows{row("X1", "HC49", "8MHz", 1, 1), row("Y1", "", "", 2, 2)};
    QVector<CadPart> parts = CadImport::groupParts(rows, p);
    // HC49-8MHz exists: suggested, but the user asks for a new one with a new package.
    QCOMPARE(parts[0].component, QStringLiteral("HC49-8MHz"));
    parts[0].component.clear();
    parts[0].package.clear();
    parts[1].component.clear();

    const CadImportSummary s = CadImport::apply(p, 0, rows, parts, CadImportMode::Merge);
    QCOMPARE(s.newComponents, (QStringList{"HC49-8MHz-2", "PART-2"}));
    QCOMPARE(s.newPackages, (QStringList{"HC49-2", "PKG"}));
    QCOMPARE(component(p, QStringLiteral("PART-2"))->package, QStringLiteral("PKG"));
    const Package &pkg = p.packages.last();
    QCOMPARE(pkg.bodyLength, CadImport::kNewPackageSize);
    QCOMPARE(pkg.height, CadImport::kNewPackageSize);
    QCOMPARE(errorCount(p), 0);
}

void TstCadImport::sharesNewPackage()
{
    Program p = sample();
    const QVector<CadPlacement> rows{row("C20", "C0201", "1nF", 1, 1),
                                     row("C21", "C0201", "10nF", 2, 2)};
    const QVector<CadPart> parts = CadImport::groupParts(rows, p);
    const CadImportSummary s = CadImport::apply(p, 0, rows, parts, CadImportMode::Merge);
    QCOMPARE(s.newPackages, QStringList{QStringLiteral("C0201")});
    QCOMPARE(s.newComponents, (QStringList{"C0201-1nF", "C0201-10nF"}));
    QCOMPARE(component(p, QStringLiteral("C0201-10nF"))->package, QStringLiteral("C0201"));

    // Importing the same file again creates nothing new.
    const QVector<CadPart> again = CadImport::groupParts(rows, p);
    QCOMPARE(again[0].component, QStringLiteral("C0201-1nF"));
    const CadImportSummary s2 = CadImport::apply(p, 0, rows, again, CadImportMode::Merge);
    QVERIFY(s2.newComponents.isEmpty());
    QVERIFY(s2.newPackages.isEmpty());
    QCOMPARE(s2.unchanged, 2);
}

void TstCadImport::resultRoundTrips()
{
    Program p = sample();
    const QVector<CadPlacement> rows = extract(parse(kKicadPos));
    const QVector<CadPart> parts = CadImport::groupParts(rows, p);
    CadImport::apply(p, 0, rows, parts, CadImportMode::Replace);
    QCOMPARE(errorCount(p), 0);

    const ReadResult r = ProgramReader::read(ProgramWriter::toXml(p));
    QVERIFY(r.ok());
    QVERIFY(r.program == p);
}

QTEST_GUILESS_MAIN(TstCadImport)
#include "tst_cadimport.moc"
