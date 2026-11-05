#include "io/programreader.h"
#include "validation/programvalidator.h"

#include "testprograms.h"

#include <QtTest>

#include <cmath>
#include <functional>
#include <limits>

using Mutation = std::function<void(Program &)>;
Q_DECLARE_METATYPE(Mutation)

namespace {

using Kind = ProgramLocation::Kind;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

Board &board(Program &p) { return p.boards[0]; }
Feeder &feeder(Program &p, int i) { return p.feederSetup->feeders[i]; }

}  // namespace

class TstProgramValidator : public QObject
{
    Q_OBJECT

private slots:
    void minimalIsValid();
    void sampleIsValid();
    void sameIdInDifferentScopes();
    void edgeValuesAreValid();
    void unknownFeederTypeNotChecked();
    void diagnostics_data();
    void diagnostics();
};

void TstProgramValidator::minimalIsValid()
{
    const QVector<Diagnostic> d = ProgramValidator::validate(TestPrograms::minimal());
    QVERIFY2(d.isEmpty(), qPrintable(TestPrograms::describe(d)));

    Program p = TestPrograms::minimal();
    p.feederSetup.reset();
    QVERIFY(ProgramValidator::validate(p).isEmpty());
}

void TstProgramValidator::sampleIsValid()
{
    const ReadResult r =
        ProgramReader::readFile(QStringLiteral(SMTPRO_SAMPLES_DIR "/example.smt.xml"));
    QVERIFY(r.ok());
    const QVector<Diagnostic> d = ProgramValidator::validate(r.program);
    QVERIFY2(d.isEmpty(), qPrintable(TestPrograms::describe(d)));
}

void TstProgramValidator::sameIdInDifferentScopes()
{
    // docs/program-format.md 5.1: ids are unique per kind and scope only.
    Program p = TestPrograms::minimal();
    p.packages[0].id = QStringLiteral("X");
    p.components[0].package = QStringLiteral("X");
    p.components[1].package = QStringLiteral("X");
    p.components.append({QStringLiteral("X"), QStringLiteral("X"), {}, {}});
    board(p).id = QStringLiteral("X");
    for (BoardInstance &bi : p.panel.instances)
        bi.board = QStringLiteral("X");
    board(p).fiducials[0].id = QStringLiteral("X");
    board(p).badMarks[0].id = QStringLiteral("X");
    board(p).placements[2].id = QStringLiteral("X");
    p.panel.fiducials[0].id = QStringLiteral("X");
    p.panel.instances[0].id = QStringLiteral("X");
    feeder(p, 0).id = QStringLiteral("X");

    // A second board may reuse placement ids of the first.
    Board second = board(p);
    second.id = QStringLiteral("Y");
    p.boards.append(second);

    const QVector<Diagnostic> d = ProgramValidator::validate(p);
    QVERIFY2(d.isEmpty(), qPrintable(TestPrograms::describe(d)));
}

void TstProgramValidator::edgeValuesAreValid()
{
    Program p = TestPrograms::minimal();
    board(p).placements[0].x = 0.0;   // on the board edge
    board(p).placements[1].x = 50.0;  // == width
    board(p).placements[1].y = 40.0;  // == height
    p.packages[0].pinCount = 0;
    p.packages[0].place.speed = 1;
    p.packages[1].place.speed = 100;
    p.packages[0].vision.tolerance = 0.0;
    feeder(p, 0).type = FeederType::Tray;
    feeder(p, 0).tray.rows = 2;
    feeder(p, 0).tray.columns = 3;
    feeder(p, 0).tray.startIndex = 5;
    feeder(p, 1).type = FeederType::Tube;
    feeder(p, 1).tube.pitch = 0.0;
    feeder(p, 2).type = FeederType::Bulk;
    const QVector<Diagnostic> d = ProgramValidator::validate(p);
    QVERIFY2(d.isEmpty(), qPrintable(TestPrograms::describe(d)));
}

void TstProgramValidator::unknownFeederTypeNotChecked()
{
    // Its parameters are kept as raw XML; the model values mean nothing.
    Program p = TestPrograms::minimal();
    feeder(p, 0).type = FeederType::Unknown;
    feeder(p, 0).unknownType = QStringLiteral("vibration");
    feeder(p, 0).tape.width = 0.0;
    const QVector<Diagnostic> d = ProgramValidator::validate(p);
    QVERIFY2(d.isEmpty(), qPrintable(TestPrograms::describe(d)));
}

void TstProgramValidator::diagnostics_data()
{
    // Each mutation of the minimal program must give exactly this diagnostic.
    QTest::addColumn<Mutation>("mutate");
    QTest::addColumn<bool>("error");  // false = warning
    QTest::addColumn<int>("kind");
    QTest::addColumn<int>("index");
    QTest::addColumn<int>("subIndex");
    QTest::addColumn<QString>("field");
    QTest::addColumn<QString>("fragment");
    QTest::addColumn<int>("relatedIndex");     // duplicates: first use; -2 = none
    QTest::addColumn<int>("relatedSubIndex");

    auto row = [](const char *name, Mutation m, bool error, Kind kind, int index, int sub,
                  const char *field, const QString &fragment, int relatedIndex = -2,
                  int relatedSub = -1) {
        QTest::newRow(name) << m << error << int(kind) << index << sub << QString(field)
                            << fragment << relatedIndex << relatedSub;
    };
    const bool E = true;
    const bool W = false;

    // Ids
    row("duplicate package", [](Program &p) {
        p.packages[1].id = QStringLiteral("R0603");
        p.components[2].package = QStringLiteral("R0603");
    }, E, Kind::Package, 1, -1, "id", QStringLiteral("封装 id \"R0603\" 重复"), 0);
    row("duplicate component", [](Program &p) { p.components.append(p.components[0]); },
        E, Kind::Component, 3, -1, "id", QStringLiteral("RC-10K"), 0);
    row("duplicate board", [](Program &p) { p.boards.append(p.boards[0]); },
        E, Kind::Board, 1, -1, "id", QStringLiteral("PCB"), 0);
    row("duplicate placement", [](Program &p) { board(p).placements[3].id = QStringLiteral("R1"); },
        E, Kind::Placement, 0, 3, "id", QStringLiteral("单板 PCB 中的贴片位 id \"R1\""), 0, 0);
    row("duplicate board fiducial",
        [](Program &p) { board(p).fiducials[1].id = QStringLiteral("M1"); },
        E, Kind::BoardFiducial, 0, 1, "id", QStringLiteral("M1"), 0, 0);
    row("duplicate bad mark", [](Program &p) { board(p).badMarks.append(board(p).badMarks[0]); },
        E, Kind::BadMark, 0, 1, "id", QStringLiteral("BM1"), 0, 0);
    row("duplicate panel fiducial",
        [](Program &p) { p.panel.fiducials.append(p.panel.fiducials[0]); },
        E, Kind::PanelFiducial, 1, -1, "id", QStringLiteral("PM1"), 0);
    row("duplicate instance", [](Program &p) { p.panel.instances[1].id = QStringLiteral("1"); },
        E, Kind::BoardInstance, 1, -1, "id", QStringLiteral("子板实例 id \"1\""), 0);
    row("duplicate feeder", [](Program &p) { feeder(p, 1).id = QStringLiteral("F1"); },
        E, Kind::Feeder, 1, -1, "id", QStringLiteral("F1"), 0);
    row("blank package id", [](Program &p) {
        p.packages.append(p.packages[0]);
        p.packages[2].id = QStringLiteral("  ");
    }, E, Kind::Package, 2, -1, "id", QStringLiteral("不能为空"));
    row("empty placement id", [](Program &p) { board(p).placements[3].id.clear(); },
        E, Kind::Placement, 0, 3, "id", QStringLiteral("不能为空"));

    // References
    row("component without package", [](Program &p) { p.components[0].package.clear(); },
        E, Kind::Component, 0, -1, "package", QStringLiteral("未指定封装"));
    row("unknown package", [](Program &p) { p.components[0].package = QStringLiteral("R9"); },
        E, Kind::Component, 0, -1, "package", QStringLiteral("\"R9\" 不存在"));
    row("placement without component", [](Program &p) { board(p).placements[0].component.clear(); },
        E, Kind::Placement, 0, 0, "component", QStringLiteral("未指定元件"));
    row("unknown component in placement",
        [](Program &p) { board(p).placements[0].component = QStringLiteral("X"); },
        E, Kind::Placement, 0, 0, "component", QStringLiteral("\"X\" 不存在"));
    row("unknown component in feeder",
        [](Program &p) { feeder(p, 0).component = QStringLiteral("X"); },
        E, Kind::Feeder, 0, -1, "component", QStringLiteral("\"X\" 不存在"));
    row("instance without board", [](Program &p) { p.panel.instances[0].board.clear(); },
        E, Kind::BoardInstance, 0, -1, "board", QStringLiteral("未指定单板"));
    // Its skip of R2 cannot be checked without the board; no second error.
    row("unknown board", [](Program &p) { p.panel.instances[1].board = QStringLiteral("Q"); },
        E, Kind::BoardInstance, 1, -1, "board", QStringLiteral("\"Q\" 不存在"));
    row("unknown skip",
        [](Program &p) { p.panel.instances[0].skippedPlacements = QStringList{QStringLiteral("R9")}; },
        E, Kind::Skip, 0, 0, "placement", QStringLiteral("R9"));
    row("duplicate skip", [](Program &p) {
        p.panel.instances[1].skippedPlacements = QStringList{QStringLiteral("R2"), QStringLiteral("R2")};
    }, E, Kind::Skip, 1, 1, "placement", QStringLiteral("重复跳过位号 R2"));
    row("empty skip", [](Program &p) { p.panel.instances[0].skippedPlacements = QStringList{QString()}; },
        E, Kind::Skip, 0, 0, "placement", QStringLiteral("为空"));
    row("no instances", [](Program &p) { p.panel.instances.clear(); },
        E, Kind::Panel, -1, -1, "", QStringLiteral("BoardInstance"));

    // Ranges
    row("package length zero", [](Program &p) { p.packages[0].bodyLength = 0.0; },
        E, Kind::Package, 0, -1, "bodyLength", QStringLiteral("必须大于 0"));
    row("package width nan", [](Program &p) { p.packages[0].bodyWidth = kNaN; },
        E, Kind::Package, 0, -1, "bodyWidth", QStringLiteral("不是有效数值"));
    row("package height inf", [](Program &p) { p.packages[0].height = kInf; },
        E, Kind::Package, 0, -1, "height", QStringLiteral("不是有效数值"));
    row("pin count negative", [](Program &p) { p.packages[0].pinCount = -1; },
        E, Kind::Package, 0, -1, "pinCount", QStringLiteral("不能小于 0"));
    row("pick depth nan", [](Program &p) { p.packages[0].pick.pickDepth = kNaN; },
        E, Kind::PackagePick, 0, -1, "pickDepth", QStringLiteral("pickDepth"));
    row("place depth inf", [](Program &p) { p.packages[0].place.placeDepth = kInf; },
        E, Kind::PackagePlace, 0, -1, "placeDepth", QStringLiteral("placeDepth"));
    row("speed zero", [](Program &p) { p.packages[1].place.speed = 0; },
        E, Kind::PackagePlace, 1, -1, "speed", QStringLiteral("1 到 100"));
    row("speed above 100", [](Program &p) { p.packages[1].place.speed = 101; },
        E, Kind::PackagePlace, 1, -1, "speed", QStringLiteral("101"));
    row("tolerance negative", [](Program &p) { p.packages[0].vision.tolerance = -0.01; },
        E, Kind::PackageVision, 0, -1, "tolerance", QStringLiteral("不能小于 0"));
    row("board thickness negative", [](Program &p) { board(p).thickness = -1.6; },
        E, Kind::Board, 0, -1, "thickness", QStringLiteral("必须大于 0"));
    row("panel width zero", [](Program &p) { p.panel.width = 0.0; },
        E, Kind::Panel, -1, -1, "width", QStringLiteral("拼板的宽度"));
    row("board fiducial size zero", [](Program &p) { board(p).fiducials[0].size = 0.0; },
        E, Kind::BoardFiducial, 0, 0, "size", QStringLiteral("必须大于 0"));
    row("panel fiducial x nan", [](Program &p) { p.panel.fiducials[0].x = kNaN; },
        E, Kind::PanelFiducial, 0, -1, "x", QStringLiteral("不是有效数值"));
    row("bad mark y inf", [](Program &p) { board(p).badMarks[0].y = kInf; },
        E, Kind::BadMark, 0, 0, "y", QStringLiteral("不是有效数值"));
    // Not also reported as outside the board.
    row("placement x nan", [](Program &p) { board(p).placements[0].x = kNaN; },
        E, Kind::Placement, 0, 0, "x", QStringLiteral("不是有效数值"));
    row("placement rotation nan", [](Program &p) { board(p).placements[1].rotation = kNaN; },
        E, Kind::Placement, 0, 1, "rotation", QStringLiteral("角度"));
    row("instance y inf", [](Program &p) { p.panel.instances[0].y = kInf; },
        E, Kind::BoardInstance, 0, -1, "y", QStringLiteral("不是有效数值"));
    row("feeder pickZ nan", [](Program &p) { feeder(p, 0).pickZ = kNaN; },
        E, Kind::Feeder, 0, -1, "pickZ", QStringLiteral("pickZ"));
    row("tape width zero", [](Program &p) { feeder(p, 0).tape.width = 0.0; },
        E, Kind::FeederParams, 0, -1, "width", QStringLiteral("料带宽度"));
    row("tape pitch negative", [](Program &p) { feeder(p, 1).tape.pitch = -4.0; },
        E, Kind::FeederParams, 1, -1, "pitch", QStringLiteral("必须大于 0"));
    row("tape feed count zero", [](Program &p) { feeder(p, 0).tape.feedCount = 0; },
        E, Kind::FeederParams, 0, -1, "feedCount", QStringLiteral("不能小于 1"));
    row("tray rows zero", [](Program &p) {
        feeder(p, 0).type = FeederType::Tray;
        feeder(p, 0).tray.rows = 0;
    }, E, Kind::FeederParams, 0, -1, "rows", QStringLiteral("不能小于 1"));
    row("tray column pitch zero", [](Program &p) {
        feeder(p, 0).type = FeederType::Tray;
        feeder(p, 0).tray.columnPitch = 0.0;
    }, E, Kind::FeederParams, 0, -1, "columnPitch", QStringLiteral("必须大于 0"));
    row("tray start index past end", [](Program &p) {
        feeder(p, 0).type = FeederType::Tray;
        feeder(p, 0).tray.rows = 2;
        feeder(p, 0).tray.columns = 3;
        feeder(p, 0).tray.startIndex = 6;
    }, E, Kind::FeederParams, 0, -1, "startIndex", QStringLiteral("0 到 5"));
    row("tray start index negative", [](Program &p) {
        feeder(p, 0).type = FeederType::Tray;
        feeder(p, 0).tray.startIndex = -1;
    }, E, Kind::FeederParams, 0, -1, "startIndex", QStringLiteral("超出范围"));
    row("tube pitch negative", [](Program &p) {
        feeder(p, 2).type = FeederType::Tube;
        feeder(p, 2).tube.pitch = -1.0;
    }, E, Kind::FeederParams, 2, -1, "pitch", QStringLiteral("不能小于 0"));

    // Warnings
    row("placement right of board", [](Program &p) { board(p).placements[0].x = 50.01; },
        W, Kind::Placement, 0, 0, "", QStringLiteral("之外"));
    row("placement below board", [](Program &p) { board(p).placements[2].y = -0.1; },
        W, Kind::Placement, 0, 2, "", QStringLiteral("R3"));
}

void TstProgramValidator::diagnostics()
{
    QFETCH(Mutation, mutate);
    QFETCH(bool, error);
    QFETCH(int, kind);
    QFETCH(int, index);
    QFETCH(int, subIndex);
    QFETCH(QString, field);
    QFETCH(QString, fragment);
    QFETCH(int, relatedIndex);
    QFETCH(int, relatedSubIndex);

    Program p = TestPrograms::minimal();
    mutate(p);
    const QVector<Diagnostic> list = ProgramValidator::validate(p);
    QVERIFY2(list.size() == 1, qPrintable(TestPrograms::describe(list)));
    const Diagnostic &d = list.first();
    QCOMPARE(d.severity, error ? Diagnostic::Severity::Error : Diagnostic::Severity::Warning);
    QCOMPARE(int(d.location.kind), kind);
    QCOMPARE(d.location.index, index);
    QCOMPARE(d.location.subIndex, subIndex);
    QCOMPARE(d.field, field);
    QCOMPARE(d.line, qint64(0));
    QVERIFY2(d.message.contains(fragment), qPrintable(d.message));
    if (relatedIndex == -2) {
        QVERIFY(!d.related.isValid());
    } else {
        QCOMPARE(int(d.related.kind), kind);
        QCOMPARE(d.related.index, relatedIndex);
        QCOMPARE(d.related.subIndex, relatedSubIndex);
    }
}

QTEST_GUILESS_MAIN(TstProgramValidator)
#include "tst_programvalidator.moc"
