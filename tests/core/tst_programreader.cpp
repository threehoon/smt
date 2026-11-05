#include "io/programreader.h"

#include <QtTest>

#include <cstring>

namespace {

// Smallest valid program. Error cases below change one fragment and expect a
// diagnostic on a given line, so keep the line layout stable when editing.
const char kMinimal[] = R"(<?xml version="1.0" encoding="UTF-8"?>
<SmtProgram formatVersion="1.0">
  <Info name="MIN"/>
  <Packages>
    <Package id="R0603" bodyLength="1.6" bodyWidth="0.8" height="0.45"/>
  </Packages>
  <Components>
    <Component id="RC-10K" package="R0603"/>
  </Components>
  <Boards>
    <Board id="PCB" width="50" height="40" thickness="1.6">
      <Placements>
        <Placement id="R1" component="RC-10K" x="10" y="10" rotation="0"/>
        <Placement id="R2" component="RC-10K" x="20" y="10" rotation="90"/>
      </Placements>
    </Board>
  </Boards>
  <Panel width="60" height="50">
    <BoardInstances>
      <BoardInstance id="1" board="PCB" x="5" y="5" rotation="0"/>
    </BoardInstances>
  </Panel>
  <FeederSetup>
    <Feeder id="F1" type="tape" slot="S1" component="RC-10K" pickX="100" pickY="-35" pickZ="-12.5">
      <Tape width="8" pitch="4"/>
    </Feeder>
  </FeederSetup>
</SmtProgram>
)";

// Returns `base` with the single occurrence of `from` replaced by `to`.
QByteArray with(const char *from, const char *to, QByteArray base = kMinimal)
{
    const int at = base.indexOf(from);
    if (at < 0 || base.indexOf(from, at + 1) >= 0)
        qFatal("fixture fragment not found exactly once: %s", from);
    return base.replace(at, int(std::strlen(from)), to);
}

QString describe(const ReadResult &r)
{
    QStringList lines;
    for (const Diagnostic &d : r.diagnostics) {
        lines << QStringLiteral("%1 line %2: %3")
                     .arg(d.severity == Diagnostic::Severity::Error ? QStringLiteral("error")
                                                                    : QStringLiteral("warning"))
                     .arg(d.line)
                     .arg(d.message);
    }
    return lines.isEmpty() ? QStringLiteral("(no diagnostics)") : lines.join(QLatin1Char('\n'));
}

bool hasDiagnostic(const QVector<Diagnostic> &list, int line, const QString &fragment)
{
    return std::any_of(list.cbegin(), list.cend(), [&](const Diagnostic &d) {
        return (line < 0 || d.line == line) && d.message.contains(fragment);
    });
}

} // namespace

class TstProgramReader : public QObject
{
    Q_OBJECT

private slots:
    void minimalLoads();
    void defaultsApplied();
    void anglesNormalized();
    void feederSetupOptional();
    void idScopes();
    void sampleContent();
    void errors_data();
    void errors();
    void reportsAllErrors();
    void oneErrorPerProblem_data();
    void oneErrorPerProblem();
    void wrongRoot();
    void warnings_data();
    void warnings();
    void newerMinorVersion();
    void unknownFeederTypeKept();
    void missingFile();
};

void TstProgramReader::minimalLoads()
{
    const ReadResult r = ProgramReader::read(QByteArray(kMinimal));
    QVERIFY2(r.diagnostics.isEmpty(), qPrintable(describe(r)));
    QVERIFY(r.ok());
    QVERIFY(!r.newerMinorVersion);

    const Program &p = r.program;
    QCOMPARE(p.info.name, QStringLiteral("MIN"));
    QCOMPARE(p.packages.size(), 1);
    QCOMPARE(p.packages[0].bodyWidth, 0.8);
    QCOMPARE(p.components[0].package, QStringLiteral("R0603"));
    QCOMPARE(p.boards[0].placements.size(), 2);
    QCOMPARE(p.boards[0].placements[1].rotation, 90.0);
    QCOMPARE(p.panel.instances.size(), 1);
    QCOMPARE(p.panel.instances[0].board, QStringLiteral("PCB"));
    QVERIFY(p.feederSetup.has_value());
    QCOMPARE(p.feederSetup->feeders[0].pickZ, -12.5);
}

void TstProgramReader::defaultsApplied()
{
    const ReadResult r = ProgramReader::read(QByteArray(kMinimal));
    QVERIFY2(r.ok(), qPrintable(describe(r)));
    const Program &p = r.program;

    const Package &pkg = p.packages[0];
    QCOMPARE(pkg.pinCount, 0);
    QVERIFY(pkg.pick.nozzles.isEmpty());
    QCOMPARE(pkg.pick.pickDepth, 0.0);
    QVERIFY(pkg.pick.vacuumCheck);
    QCOMPARE(pkg.place.speed, 100);
    QCOMPARE(pkg.vision.method, VisionMethod::Bottom);
    QCOMPARE(pkg.vision.tolerance, 0.1);

    const Placement &pl = p.boards[0].placements[0];
    QCOMPARE(pl.side, Side::Top);
    QVERIFY(pl.enabled);
    QVERIFY(p.panel.instances[0].enabled);
    QVERIFY(p.panel.instances[0].skippedPlacements.isEmpty());

    const Feeder &f = p.feederSetup->feeders[0];
    QCOMPARE(f.type, FeederType::Tape);
    QCOMPARE(f.rotation, 0.0);
    QVERIFY(f.enabled);
    QCOMPARE(f.priority, 0);
    QCOMPARE(f.tape.feedCount, 1);
    QVERIFY(f.canFeed());
}

void TstProgramReader::anglesNormalized()
{
    QByteArray xml = with(R"(rotation="90")", R"(rotation="-90")");
    xml = with(R"(x="5" y="5" rotation="0")", R"(x="5" y="5" rotation="720")", xml);
    const ReadResult r = ProgramReader::read(xml);
    QVERIFY2(r.diagnostics.isEmpty(), qPrintable(describe(r)));
    QCOMPARE(r.program.boards[0].placements[1].rotation, 270.0);
    QCOMPARE(r.program.panel.instances[0].rotation, 0.0);
}

void TstProgramReader::feederSetupOptional()
{
    const char *const block = R"(  <FeederSetup>
    <Feeder id="F1" type="tape" slot="S1" component="RC-10K" pickX="100" pickY="-35" pickZ="-12.5">
      <Tape width="8" pitch="4"/>
    </Feeder>
  </FeederSetup>
)";
    const ReadResult r = ProgramReader::read(with(block, ""));
    QVERIFY2(r.diagnostics.isEmpty(), qPrintable(describe(r)));
    QVERIFY(!r.program.feederSetup.has_value());
}

void TstProgramReader::idScopes()
{
    // Same id in different scopes is allowed (docs/program-format.md 5.1).
    QByteArray xml = with(R"(<Component id="RC-10K" package="R0603"/>)",
                          R"(<Component id="RC-10K" package="R0603"/>
    <Component id="R0603" package="R0603"/>)");
    xml = with("      <Placements>\n",
               "      <Fiducials><Fiducial id=\"M1\" x=\"3\" y=\"3\"/></Fiducials>\n"
               "      <BadMarks><BadMark id=\"M1\" x=\"4\" y=\"4\"/></BadMarks>\n"
               "      <Placements>\n",
               xml);
    xml = with("    <BoardInstances>\n",
               "    <Fiducials><Fiducial id=\"M1\" x=\"2\" y=\"2\"/></Fiducials>\n"
               "    <BoardInstances>\n",
               xml);
    xml = with(R"(<Feeder id="F1")", R"(<Feeder id="R1")", xml);
    const ReadResult r = ProgramReader::read(xml);
    QVERIFY2(r.diagnostics.isEmpty(), qPrintable(describe(r)));
}

void TstProgramReader::sampleContent()
{
    const ReadResult r =
        ProgramReader::readFile(QStringLiteral(SMTPRO_SAMPLES_DIR "/example.smt.xml"));
    QVERIFY2(r.diagnostics.isEmpty(), qPrintable(describe(r)));
    const Program &p = r.program;

    QCOMPARE(p.info.name, QStringLiteral("DEMO-CTRL-V1"));
    QCOMPARE(p.info.description, QStringLiteral("示例程序：2 拼板，阻容 + 一颗 QFN"));
    QCOMPARE(p.info.createdAt, QStringLiteral("2026-11-06T10:00:00+08:00"));

    QCOMPARE(p.packages.size(), 3);
    QCOMPARE(p.packages[1].pick.nozzles,
             (QStringList{QStringLiteral("N502"), QStringLiteral("N503")}));
    QCOMPARE(p.packages[2].place.speed, 50);
    QCOMPARE(p.packages[2].vision.tolerance, 0.03);
    QCOMPARE(p.components.size(), 3);

    const Board &b = p.boards[0];
    QCOMPARE(b.fiducials.size(), 2);
    QCOMPARE(b.badMarks.size(), 1);
    QCOMPARE(b.placements.size(), 6);
    QVERIFY(!b.placements[4].enabled);  // R2 is DNP
    QCOMPARE(b.placements[5].side, Side::Bottom);  // R5

    QCOMPARE(p.panel.fiducials.size(), 2);
    QCOMPARE(p.panel.instances.size(), 2);
    QCOMPARE(p.panel.instances[1].rotation, 180.0);
    QCOMPARE(p.panel.instances[1].skippedPlacements, QStringList{QStringLiteral("R1")});

    QVERIFY(p.feederSetup.has_value());
    QCOMPARE(p.feederSetup->machineModel, QStringLiteral("SMT-DEMO-01"));
    const QVector<Feeder> &feeders = p.feederSetup->feeders;
    QCOMPARE(feeders.size(), 4);
    QCOMPARE(feeders[2].priority, -1);
    QCOMPARE(feeders[3].type, FeederType::Tray);
    QCOMPARE(feeders[3].tray.columns, 10);

    QCOMPARE(p.extensions.size(), 1);
    QVERIFY(p.extensions[0].contains(QStringLiteral("com.smtpro.editor")));
}

void TstProgramReader::errors_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("line");  // -1 = any line
    QTest::addColumn<QString>("fragment");

    const QByteArray tray = with(R"(type="tape")", R"(type="tray")",
                                 with(R"(<Tape width="8" pitch="4"/>)",
                                      R"(<Tray rows="2" columns="3" rowPitch="12" columnPitch="12"/>)"));

    // Attributes
    QTest::newRow("missing required")
        << with(R"( bodyWidth="0.8")", "") << 5 << QStringLiteral("bodyWidth");
    QTest::newRow("not a number")
        << with(R"(height="0.45")", R"(height="abc")") << 5 << QStringLiteral("abc");
    QTest::newRow("locale decimal comma")
        << with(R"(height="0.45")", R"(height="0,45")") << 5 << QStringLiteral("0,45");
    QTest::newRow("infinite")
        << with(R"(x="10" y="10")", R"(x="inf" y="10")") << 13 << QStringLiteral("inf");
    QTest::newRow("zero size")
        << with(R"(<Panel width="60")", R"(<Panel width="0")") << 18 << QStringLiteral("width");
    QTest::newRow("negative size")
        << with(R"(bodyLength="1.6")", R"(bodyLength="-1.6")") << 5
        << QStringLiteral("bodyLength");
    QTest::newRow("bad enum")
        << with(R"(rotation="90")", R"(rotation="90" side="up")") << 14 << QStringLiteral("up");
    QTest::newRow("bad bool")
        << with(R"(rotation="90")", R"(rotation="90" enabled="yes")") << 14
        << QStringLiteral("yes");
    QTest::newRow("not an integer")
        << with(R"(<Tape width="8" pitch="4"/>)", R"(<Tape width="8" pitch="4" feedCount="1.5"/>)")
        << 25 << QStringLiteral("feedCount");
    QTest::newRow("integer below range")
        << with(R"(<Tape width="8" pitch="4"/>)", R"(<Tape width="8" pitch="4" feedCount="0"/>)")
        << 25 << QStringLiteral("feedCount");
    QTest::newRow("speed out of range")
        << with(R"(height="0.45"/>)", R"(height="0.45"><Place speed="150"/></Package>)") << 5
        << QStringLiteral("speed");
    QTest::newRow("empty id")
        << with(R"(<Placement id="R1")", R"(<Placement id=" ")") << 13 << QStringLiteral("id");

    // References
    QTest::newRow("unknown package")
        << with(R"(package="R0603")", R"(package="R0805")") << 8 << QStringLiteral("R0805");
    QTest::newRow("unknown component in placement")
        << with(R"(<Placement id="R1" component="RC-10K")", R"(<Placement id="R1" component="X")")
        << 13 << QStringLiteral("\"X\"");
    QTest::newRow("unknown component in feeder")
        << with(R"(slot="S1" component="RC-10K")", R"(slot="S1" component="X")") << 24
        << QStringLiteral("\"X\"");
    QTest::newRow("empty package reference")
        << with(R"(package="R0603")", R"(package="")") << 8 << QStringLiteral("未指定封装");
    QTest::newRow("empty skip")
        << with(R"(rotation="0"/>
    </BoardInstances>)",
                R"(rotation="0"><Skip placement=""/></BoardInstance>
    </BoardInstances>)")
        << 20 << QStringLiteral("为空");
    QTest::newRow("unknown board")
        << with(R"(board="PCB")", R"(board="PCB-B")") << 20 << QStringLiteral("PCB-B");
    QTest::newRow("unknown skip")
        << with(R"(rotation="0"/>
    </BoardInstances>)",
                R"(rotation="0"><Skip placement="R9"/></BoardInstance>
    </BoardInstances>)")
        << 20 << QStringLiteral("R9");
    QTest::newRow("duplicate skip")
        << with(R"(rotation="0"/>
    </BoardInstances>)",
                R"(rotation="0"><Skip placement="R1"/><Skip placement="R1"/></BoardInstance>
    </BoardInstances>)")
        << 20 << QStringLiteral("R1");

    // Ids
    QTest::newRow("duplicate package")
        << with(R"(<Package id="R0603" bodyLength="1.6" bodyWidth="0.8" height="0.45"/>)",
                R"(<Package id="R0603" bodyLength="1.6" bodyWidth="0.8" height="0.45"/>
    <Package id="R0603" bodyLength="1" bodyWidth="1" height="1"/>)")
        << 6 << QStringLiteral("第 5 行");
    QTest::newRow("duplicate placement")
        << with(R"(<Placement id="R2")", R"(<Placement id="R1")") << 14 << QStringLiteral("R1");

    // Sections and structure
    QTest::newRow("missing section")
        << with("  <Info name=\"MIN\"/>\n", "") << 2 << QStringLiteral("<Info>");
    QTest::newRow("duplicate section")
        << with("  <Info name=\"MIN\"/>\n", "  <Info name=\"MIN\"/>\n  <Info name=\"B\"/>\n")
        << 4 << QStringLiteral("<Info>");
    QTest::newRow("no board instance")
        << with(R"(      <BoardInstance id="1" board="PCB" x="5" y="5" rotation="0"/>
)",
                "")
        << 18 << QStringLiteral("BoardInstance");
    QTest::newRow("missing tape params")
        << with(R"(      <Tape width="8" pitch="4"/>
)",
                "")
        << 24 << QStringLiteral("<Tape>");
    QTest::newRow("missing tray rows")
        << with(R"(rows="2" )", "", tray) << 25 << QStringLiteral("rows");
    QTest::newRow("tray start index")
        << with(R"(columnPitch="12")", R"(columnPitch="12" startIndex="6")", tray) << 25
        << QStringLiteral("startIndex");

    // Version and syntax
    QTest::newRow("newer major")
        << with(R"(formatVersion="1.0")", R"(formatVersion="2.0")") << 2
        << QStringLiteral("升级");
    QTest::newRow("older major")
        << with(R"(formatVersion="1.0")", R"(formatVersion="0.9")") << 2
        << QStringLiteral("0.9");
    QTest::newRow("malformed version")
        << with(R"(formatVersion="1.0")", R"(formatVersion="v1")") << 2 << QStringLiteral("v1");
    QTest::newRow("missing version")
        << with(R"( formatVersion="1.0")", "") << 2 << QStringLiteral("formatVersion");
    QTest::newRow("syntax error")
        << with("    </Placements>", "    </Placement>") << 15 << QStringLiteral("XML");
    QTest::newRow("empty file") << QByteArray() << -1 << QStringLiteral("XML");
}

void TstProgramReader::errors()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, line);
    QFETCH(QString, fragment);

    const ReadResult r = ProgramReader::read(xml);
    QVERIFY2(!r.ok(), qPrintable(describe(r)));
    QVERIFY2(hasDiagnostic(r.errors(), line, fragment), qPrintable(describe(r)));
}

void TstProgramReader::reportsAllErrors()
{
    QByteArray xml = with(R"(package="R0603")", R"(package="R0805")");
    xml = with(R"(board="PCB")", R"(board="PCB-B")", xml);
    xml = with(R"(height="0.45")", R"(height="abc")", xml);
    const ReadResult r = ProgramReader::read(xml);
    const QVector<Diagnostic> errors = r.errors();
    QCOMPARE(errors.size(), 3);
    // Sorted by line, so the UI can list them in file order.
    QCOMPARE(errors[0].line, qint64(5));
    QCOMPARE(errors[1].line, qint64(8));
    QCOMPARE(errors[2].line, qint64(20));
}

void TstProgramReader::oneErrorPerProblem_data()
{
    // The parser reports a missing or unreadable attribute; the validator must
    // not report its fallback value a second time.
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("line");

    QTest::newRow("missing size") << with(R"( bodyWidth="0.8")", "") << 5;
    QTest::newRow("unreadable size") << with(R"(height="0.45")", R"(height="abc")") << 5;
    QTest::newRow("missing id")
        << with(R"(<Placement id="R2" component)", R"(<Placement component)") << 14;
    QTest::newRow("missing reference")
        << with(R"( board="PCB")", "") << 20;
    QTest::newRow("unreadable coordinate")
        << with(R"(x="20" y="10")", R"(x="-" y="10")") << 14;
    QTest::newRow("missing panel")
        << with(R"(  <Panel width="60" height="50">
    <BoardInstances>
      <BoardInstance id="1" board="PCB" x="5" y="5" rotation="0"/>
    </BoardInstances>
  </Panel>
)",
                "")
        << 2;
}

void TstProgramReader::oneErrorPerProblem()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, line);

    const ReadResult r = ProgramReader::read(xml);
    QVERIFY2(r.errors().size() == 1, qPrintable(describe(r)));
    QCOMPARE(r.errors()[0].line, qint64(line));
}

void TstProgramReader::wrongRoot()
{
    const ReadResult r = ProgramReader::read(QByteArray("<Job formatVersion=\"1.0\"/>"));
    QVERIFY(!r.ok());
    QCOMPARE(r.errors().size(), 1);
    QVERIFY(r.errors()[0].message.contains(QStringLiteral("SmtProgram")));
}

void TstProgramReader::warnings_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("line");
    QTest::addColumn<QString>("fragment");

    QTest::newRow("placement outside board")
        << with(R"(x="20" y="10")", R"(x="55" y="10")") << 14 << QStringLiteral("R2");
    QTest::newRow("unknown attribute")
        << with(R"(<Info name="MIN"/>)", R"(<Info name="MIN" colour="red"/>)") << 3
        << QStringLiteral("colour");
    QTest::newRow("unknown element")
        << with("      <Placements>\n", "      <Notes/><Placements>\n") << 12
        << QStringLiteral("Notes");
    QTest::newRow("unknown top-level element")
        << with("  <Info name=\"MIN\"/>\n", "  <Info name=\"MIN\"/><Routing/>\n") << 3
        << QStringLiteral("Routing");
}

void TstProgramReader::warnings()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, line);
    QFETCH(QString, fragment);

    const ReadResult r = ProgramReader::read(xml);
    QVERIFY2(r.ok(), qPrintable(describe(r)));
    QCOMPARE(r.warnings().size(), 1);
    QVERIFY2(hasDiagnostic(r.warnings(), line, fragment), qPrintable(describe(r)));
}

void TstProgramReader::newerMinorVersion()
{
    QByteArray xml = with(R"(formatVersion="1.0")", R"(formatVersion="1.7")");
    xml = with(R"(<Info name="MIN"/>)", R"(<Info name="MIN" since17="x"/>)", xml);
    const ReadResult r = ProgramReader::read(xml);
    QVERIFY2(r.ok(), qPrintable(describe(r)));
    QVERIFY(r.newerMinorVersion);
    QVERIFY(hasDiagnostic(r.warnings(), 2, QStringLiteral("1.7")));
    QVERIFY(hasDiagnostic(r.warnings(), 3, QStringLiteral("since17")));
}

void TstProgramReader::unknownFeederTypeKept()
{
    const QByteArray xml = with(R"(type="tape")", R"(type="vibration")",
                                with(R"(<Tape width="8" pitch="4"/>)",
                                     R"(<Bowl track="2"><Speed v="3"/></Bowl>)"));
    const ReadResult r = ProgramReader::read(xml);
    QVERIFY2(r.ok(), qPrintable(describe(r)));
    QVERIFY(hasDiagnostic(r.warnings(), 24, QStringLiteral("vibration")));

    const Feeder &f = r.program.feederSetup->feeders[0];
    QCOMPARE(f.type, FeederType::Unknown);
    QCOMPARE(f.unknownType, QStringLiteral("vibration"));
    QVERIFY(f.enabled);  // as written; canFeed() is what run time checks
    QVERIFY(!f.canFeed());
    QCOMPARE(f.rawParams.size(), 1);
    QVERIFY(f.rawParams[0].contains(QStringLiteral("<Speed v=\"3\"/>")));
}

void TstProgramReader::missingFile()
{
    const ReadResult r = ProgramReader::readFile(QStringLiteral("no/such/file.smt.xml"));
    QVERIFY(!r.ok());
    QCOMPARE(r.errors().size(), 1);
    QCOMPARE(r.errors()[0].line, qint64(0));
}

QTEST_GUILESS_MAIN(TstProgramReader)
#include "tst_programreader.moc"
