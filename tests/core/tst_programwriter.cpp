#include "io/programreader.h"
#include "io/programwriter.h"
#include "model/angle.h"

#include <QLocale>
#include <QTemporaryDir>
#include <QtTest>

namespace {

const QString kSample = QStringLiteral(SMTPRO_SAMPLES_DIR "/example.smt.xml");

QString describe(const ReadResult &r)
{
    QStringList lines;
    for (const Diagnostic &d : r.diagnostics)
        lines << QStringLiteral("line %1: %2").arg(d.line).arg(d.message);
    return lines.join(QLatin1Char('\n'));
}

// A small valid program built in code, independent of the reader.
Program makeProgram()
{
    Program p;
    p.info.name = QStringLiteral("W");
    Package pkg;
    pkg.id = QStringLiteral("R0603");
    pkg.bodyLength = 1.6;
    pkg.bodyWidth = 0.8;
    pkg.height = 0.45;
    p.packages.append(pkg);
    Component c;
    c.id = QStringLiteral("RC-10K");
    c.package = pkg.id;
    p.components.append(c);
    Board b;
    b.id = QStringLiteral("PCB");
    b.width = 50.0;
    b.height = 40.0;
    b.thickness = 1.6;
    Placement pl;
    pl.id = QStringLiteral("R1");
    pl.component = c.id;
    pl.x = 10.0;
    pl.y = 10.0;
    b.placements.append(pl);
    p.boards.append(b);
    p.panel.width = 60.0;
    p.panel.height = 50.0;
    BoardInstance bi;
    bi.id = QStringLiteral("1");
    bi.board = b.id;
    p.panel.instances.append(bi);
    return p;
}

} // namespace

class TstProgramWriter : public QObject
{
    Q_OBJECT

private slots:
    void sampleRoundTrip();
    void builtProgramRoundTrip();
    void preservesUnknownContent();
    void numberFormat();
    void localeIndependent();
    void omitsEmptyOptional();
    void writeFile();
    void writeFileFails();
    void normalizeAngle_data();
    void normalizeAngle();
};

void TstProgramWriter::sampleRoundTrip()
{
    const ReadResult first = ProgramReader::readFile(kSample);
    QVERIFY2(first.diagnostics.isEmpty(), qPrintable(describe(first)));

    const QByteArray xml = ProgramWriter::toXml(first.program);
    const ReadResult second = ProgramReader::read(xml);
    QVERIFY2(second.diagnostics.isEmpty(), qPrintable(describe(second)));
    QVERIFY(second.program == first.program);
    // Writing is stable: no drift on repeated save.
    QCOMPARE(QString::fromUtf8(ProgramWriter::toXml(second.program)), QString::fromUtf8(xml));
}

void TstProgramWriter::builtProgramRoundTrip()
{
    Program p = makeProgram();
    FeederSetup setup;
    Feeder tray;
    tray.id = QStringLiteral("T1");
    tray.type = FeederType::Tray;
    tray.slot = QStringLiteral("T1");
    tray.component = QStringLiteral("RC-10K");
    tray.tray = {4, 6, 12.5, 10.0, 7};
    tray.priority = 3;
    setup.feeders.append(tray);
    Feeder tube = tray;
    tube.id = QStringLiteral("U1");
    tube.type = FeederType::Tube;
    tube.tray = TrayParams();
    tube.tube.pitch = 2.5;
    setup.feeders.append(tube);
    Feeder bulk = tube;
    bulk.id = QStringLiteral("K1");
    bulk.type = FeederType::Bulk;
    bulk.tube = TubeParams();
    bulk.enabled = false;
    setup.feeders.append(bulk);
    p.feederSetup = setup;
    p.boards[0].placements[0].side = Side::Bottom;
    p.boards[0].placements[0].enabled = false;
    p.panel.instances[0].skippedPlacements << QStringLiteral("R1");
    p.panel.instances[0].enabled = false;
    p.packages[0].vision.method = VisionMethod::None;
    p.packages[0].pick.vacuumCheck = false;

    const ReadResult r = ProgramReader::read(ProgramWriter::toXml(p));
    QVERIFY2(r.diagnostics.isEmpty(), qPrintable(describe(r)));
    QVERIFY(r.program == p);
}

void TstProgramWriter::preservesUnknownContent()
{
    const QByteArray input = R"(<?xml version="1.0" encoding="UTF-8"?>
<SmtProgram formatVersion="1.0">
  <Info name="X"/>
  <Packages>
    <Package id="P" bodyLength="1" bodyWidth="1" height="1">
      <Extensions>
        <Extension name="com.vendor.pkg" version="2"><Hint level="3">密 &amp; 实</Hint></Extension>
      </Extensions>
    </Package>
  </Packages>
  <Components><Component id="C" package="P"/></Components>
  <Boards><Board id="B" width="10" height="10" thickness="1"/></Boards>
  <Panel width="10" height="10">
    <BoardInstances><BoardInstance id="1" board="B" x="0" y="0" rotation="0"/></BoardInstances>
  </Panel>
  <FeederSetup>
    <Feeder id="F" type="vibration" slot="V1" component="C" pickX="1" pickY="2" pickZ="3" enabled="false">
      <Bowl track="2"><Speed v="3"/><!-- tuned --></Bowl>
      <Extensions><Extension name="com.vendor.feeder" version="1"/></Extensions>
    </Feeder>
  </FeederSetup>
  <Extensions>
    <Extension name="com.vendor.root" version="1"><Script><![CDATA[a < b]]></Script></Extension>
  </Extensions>
</SmtProgram>
)";
    const ReadResult first = ProgramReader::read(input);
    QVERIFY2(first.ok(), qPrintable(describe(first)));
    QCOMPARE(first.warnings().size(), 1);  // unknown feeder type

    const QString xml = QString::fromUtf8(ProgramWriter::toXml(first.program));
    QVERIFY2(xml.contains(QStringLiteral("<Hint level=\"3\">密 &amp; 实</Hint>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("type=\"vibration\"")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<Speed v=\"3\"/>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<!-- tuned -->")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("com.vendor.feeder")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<![CDATA[a < b]]>")), qPrintable(xml));

    const ReadResult second = ProgramReader::read(xml.toUtf8());
    QVERIFY(second.program == first.program);
    QVERIFY(!second.program.feederSetup->feeders[0].enabled);
}

void TstProgramWriter::numberFormat()
{
    Program p = makeProgram();
    Placement &pl = p.boards[0].placements[0];
    pl.x = -0.00001;  // rounds to -0.0000, must be written as 0.0000
    pl.y = 1.0 / 3.0;
    pl.rotation = -90.0;
    p.panel.instances[0].rotation = 359.99999;  // rounds to 360.0000, must wrap

    const QString xml = QString::fromUtf8(ProgramWriter::toXml(p));
    QVERIFY2(xml.contains(QStringLiteral(
                 R"(<Placement id="R1" component="RC-10K" x="0.0000" y="0.3333" rotation="270.0000" side="top" enabled="true"/>)")),
             qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral(R"(x="0.0000" y="0.0000" rotation="0.0000" enabled="true")")),
             qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral(R"(pinCount="0")")), qPrintable(xml));
    QVERIFY2(xml.startsWith(QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")),
             qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<SmtProgram formatVersion=\"1.0\">")), qPrintable(xml));
}

void TstProgramWriter::localeIndependent()
{
    const QLocale saved;
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    const Program p = makeProgram();
    const QByteArray xml = ProgramWriter::toXml(p);
    const ReadResult r = ProgramReader::read(xml);
    QLocale::setDefault(saved);

    QVERIFY2(xml.contains(R"(bodyLength="1.6000")"), xml.constData());
    QVERIFY2(r.diagnostics.isEmpty(), qPrintable(describe(r)));
    QVERIFY(r.program == p);
}

void TstProgramWriter::omitsEmptyOptional()
{
    const QString xml = QString::fromUtf8(ProgramWriter::toXml(makeProgram()));
    QVERIFY2(xml.contains(QStringLiteral("<Info name=\"W\"/>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<Component id=\"RC-10K\" package=\"R0603\"/>")),
             qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<Pick pickDepth=\"0.0000\" vacuumCheck=\"true\"/>")),
             qPrintable(xml));
    QVERIFY(!xml.contains(QStringLiteral("Fiducials")));
    QVERIFY(!xml.contains(QStringLiteral("BadMarks")));
    QVERIFY(!xml.contains(QStringLiteral("FeederSetup")));
    QVERIFY(!xml.contains(QStringLiteral("Extensions")));
}

void TstProgramWriter::writeFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("out.smt.xml"));
    const Program p = makeProgram();

    QString message;
    QVERIFY2(ProgramWriter::writeFile(p, path, &message), qPrintable(message));
    const ReadResult r = ProgramReader::readFile(path);
    QVERIFY2(r.diagnostics.isEmpty(), qPrintable(describe(r)));
    QVERIFY(r.program == p);
    // QSaveFile leaves no temporary file next to the target.
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{QStringLiteral("out.smt.xml")});
}

void TstProgramWriter::writeFileFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("missing/out.smt.xml"));
    QString message;
    QVERIFY(!ProgramWriter::writeFile(makeProgram(), path, &message));
    QVERIFY(message.contains(QStringLiteral("out.smt.xml")));
    QVERIFY(!QFile::exists(path));
}

void TstProgramWriter::normalizeAngle_data()
{
    QTest::addColumn<double>("input");
    QTest::addColumn<double>("expected");
    QTest::newRow("zero") << 0.0 << 0.0;
    QTest::newRow("in range") << 123.5 << 123.5;
    QTest::newRow("full turn") << 360.0 << 0.0;
    QTest::newRow("negative") << -90.0 << 270.0;
    QTest::newRow("several turns") << 1080.0 + 45.0 << 45.0;
    QTest::newRow("several negative turns") << -725.0 << 355.0;
    QTest::newRow("tiny negative") << -1e-15 << 0.0;
}

void TstProgramWriter::normalizeAngle()
{
    QFETCH(double, input);
    QFETCH(double, expected);
    const double result = ::normalizeAngle(input);
    QCOMPARE(result, expected);
    QVERIFY(result >= 0.0 && result < 360.0);
    QVERIFY(!std::signbit(result));
}

QTEST_GUILESS_MAIN(TstProgramWriter)
#include "tst_programwriter.moc"
