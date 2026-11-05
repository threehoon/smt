#include "document/programdocument.h"

#include "io/programreader.h"

#include <QAbstractItemModelTester>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <vector>

namespace {

const QString kSample = QStringLiteral(SMTPRO_SAMPLES_DIR "/example.smt.xml");

QByteArray sampleData()
{
    QFile f(kSample);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("cannot read %s", qPrintable(kSample));
    return f.readAll();
}

// Replaces a fragment that must occur exactly once in the sample.
QByteArray sampleWith(const QByteArray &from, const QByteArray &to)
{
    QByteArray data = sampleData();
    if (data.count(from) != 1)
        qFatal("fragment must occur once: %s", from.constData());
    return data.replace(from, to);
}

QString writeTemp(const QTemporaryDir &dir, const QString &name, const QByteArray &data)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size())
        qFatal("cannot write %s", qPrintable(path));
    return path;
}

QVariant cell(const QAbstractItemModel *model, int row, int role)
{
    return model->data(model->index(row, 0), role);
}

}  // namespace

class TstProgramDocument : public QObject
{
    Q_OBJECT

private slots:
    void emptyDocument();
    void openSample();
    void componentCounts();
    void boardInstances();
    void jobPlacements();
    void boardContentFollowsBoardIndex();
    void getReturnsRoles();
    void openInvalidKeepsCurrentProgram();
    void openMissingFile();
    void openWithWarnings();
    void newerMinorVersionRequiresSaveAs();
    void saveRoundTrip();
    void rejectsNonLocalUrls();
    void modelsStayConsistent();
};

void TstProgramDocument::emptyDocument()
{
    ProgramDocument doc;
    QVERIFY(!doc.isLoaded());
    QVERIFY(doc.displayName().isEmpty());
    QVERIFY(!doc.saveAsRequired());
    QCOMPARE(doc.packages()->rowCount(), 0);
    QCOMPARE(doc.boardInstances()->rowCount(), 0);
    QCOMPARE(doc.jobPlacements()->rowCount(), 0);
    QCOMPARE(doc.feeders()->rowCount(), 0);
    QCOMPARE(doc.boardPlacements()->rowCount(), 0);
    QVERIFY(!doc.save());

    QTemporaryDir dir;
    QVERIFY(!doc.saveFile(dir.filePath(QStringLiteral("x.smt.xml"))));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("x.smt.xml"))));
}

void TstProgramDocument::openSample()
{
    ProgramDocument doc;
    QSignalSpy messages(&doc, &ProgramDocument::messageRaised);
    QSignalSpy changed(&doc, &ProgramDocument::programChanged);
    QSignalSpy resets(doc.packages(), &QAbstractItemModel::modelReset);
    QSignalSpy counts(doc.packages(), &ProgramListModel::countChanged);

    QVERIFY(doc.open(QUrl::fromLocalFile(kSample)));
    QCOMPARE(messages.count(), 0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(resets.count(), 1);
    QCOMPARE(counts.count(), 1);

    QVERIFY(doc.isLoaded());
    QCOMPARE(doc.displayName(), QStringLiteral("example.smt.xml"));
    QCOMPARE(doc.filePath(), QFileInfo(kSample).absoluteFilePath());
    QVERIFY(!doc.saveAsRequired());
    QCOMPARE(doc.programName(), QStringLiteral("DEMO-CTRL-V1"));
    QCOMPARE(doc.machineModel(), QStringLiteral("SMT-DEMO-01"));
    QCOMPARE(doc.panelWidth(), 120.0);
    QCOMPARE(doc.panelHeight(), 50.0);

    QCOMPARE(doc.packages()->rowCount(), 3);
    QCOMPARE(doc.components()->rowCount(), 3);
    QCOMPARE(doc.boards()->rowCount(), 1);
    QCOMPARE(doc.boardInstances()->rowCount(), 2);
    QCOMPARE(doc.panelFiducials()->rowCount(), 2);
    QCOMPARE(doc.feeders()->rowCount(), 4);
    QCOMPARE(doc.packages()->count(), 3);

    QCOMPARE(cell(doc.boards(), 0, BoardListModel::PlacementCountRole).toInt(), 6);
    QCOMPARE(cell(doc.boards(), 0, BoardListModel::FiducialCountRole).toInt(), 2);
    QCOMPARE(cell(doc.feeders(), 3, FeederListModel::TypeRole).toString(), QStringLiteral("料盘"));
    QCOMPARE(cell(doc.feeders(), 2, FeederListModel::PriorityRole).toInt(), -1);
    QCOMPARE(cell(doc.panelFiducials(), 1, FiducialListModel::XRole).toDouble(), 117.5);
    QCOMPARE(cell(doc.panelFiducials(), 1, FiducialListModel::ShapeRole).toString(),
             QStringLiteral("圆形"));
}

void TstProgramDocument::componentCounts()
{
    ProgramDocument doc;
    QVERIFY(doc.openFile(kSample));
    const ComponentListModel *m = doc.components();
    QCOMPARE(cell(m, 0, ComponentListModel::IdRole).toString(), QStringLiteral("CC0402-100NF"));
    QCOMPARE(cell(m, 0, ComponentListModel::FeederCountRole).toInt(), 1);
    QCOMPARE(cell(m, 0, ComponentListModel::PlacementCountRole).toInt(), 2);
    // R1, R2 (DNP) and R5 on the board definition; FD2 and FD3.
    QCOMPARE(cell(m, 1, ComponentListModel::FeederCountRole).toInt(), 2);
    QCOMPARE(cell(m, 1, ComponentListModel::PlacementCountRole).toInt(), 3);
    QCOMPARE(cell(m, 2, ComponentListModel::PackageRole).toString(), QStringLiteral("QFN32-5x5"));
}

void TstProgramDocument::boardInstances()
{
    ProgramDocument doc;
    QVERIFY(doc.openFile(kSample));
    const BoardInstanceListModel *m = doc.boardInstances();
    // Instance 1: six placements minus R2 (DNP). Instance 2: also skips R1.
    QCOMPARE(cell(m, 0, BoardInstanceListModel::PlacementCountRole).toInt(), 5);
    QCOMPARE(cell(m, 1, BoardInstanceListModel::PlacementCountRole).toInt(), 4);
    QCOMPARE(cell(m, 1, BoardInstanceListModel::SkipsRole).toString(), QStringLiteral("R1"));
    QCOMPARE(cell(m, 0, BoardInstanceListModel::SkipsRole).toString(), QString());
    QCOMPARE(cell(m, 1, BoardInstanceListModel::RotationRole).toDouble(), 180.0);
    QCOMPARE(cell(m, 1, BoardInstanceListModel::EnabledRole).toBool(), true);
}

void TstProgramDocument::jobPlacements()
{
    ProgramDocument doc;
    QVERIFY(doc.openFile(kSample));
    const JobPlacementListModel *m = doc.jobPlacements();
    QCOMPARE(m->rowCount(), 12);  // 2 instances x 6 placements
    QCOMPARE(m->enabledCount(), 9);

    QCOMPARE(cell(m, 0, JobPlacementListModel::InstanceRole).toString(), QStringLiteral("1"));
    QCOMPARE(cell(m, 0, JobPlacementListModel::IdRole).toString(), QStringLiteral("U1"));
    QCOMPARE(cell(m, 0, JobPlacementListModel::FeederRole).toString(), QStringLiteral("T1"));
    QCOMPARE(cell(m, 0, JobPlacementListModel::SideRole).toString(), QStringLiteral("顶面"));
    QCOMPARE(cell(m, 0, JobPlacementListModel::EnabledRole).toBool(), true);

    // Instance 1 R2: DNP.
    QCOMPARE(cell(m, 4, JobPlacementListModel::IdRole).toString(), QStringLiteral("R2"));
    QCOMPARE(cell(m, 4, JobPlacementListModel::EnabledRole).toBool(), false);
    // Instance 1 R5: bottom side, two feeders, higher priority first.
    QCOMPARE(cell(m, 5, JobPlacementListModel::SideRole).toString(), QStringLiteral("底面"));
    QCOMPARE(cell(m, 5, JobPlacementListModel::FeederRole).toString(),
             QStringLiteral("F1-03, F1-04"));
    // Instance 2 R1: skipped on this instance only.
    QCOMPARE(cell(m, 9, JobPlacementListModel::InstanceRole).toString(), QStringLiteral("2"));
    QCOMPARE(cell(m, 9, JobPlacementListModel::IdRole).toString(), QStringLiteral("R1"));
    QCOMPARE(cell(m, 9, JobPlacementListModel::EnabledRole).toBool(), false);
    QCOMPARE(cell(m, 3, JobPlacementListModel::EnabledRole).toBool(), true);
}

void TstProgramDocument::boardContentFollowsBoardIndex()
{
    ProgramDocument doc;
    QVERIFY(doc.openFile(kSample));
    PlacementListModel *placements = doc.boardPlacements();
    QCOMPARE(placements->boardIndex(), -1);
    QCOMPARE(placements->rowCount(), 0);

    QSignalSpy resets(placements, &QAbstractItemModel::modelReset);
    QSignalSpy indexChanged(placements, &BoardContentModel::boardIndexChanged);
    placements->setBoardIndex(0);
    doc.boardFiducials()->setBoardIndex(0);
    doc.boardBadMarks()->setBoardIndex(0);
    QCOMPARE(resets.count(), 1);
    QCOMPARE(indexChanged.count(), 1);
    QCOMPARE(placements->rowCount(), 6);
    QCOMPARE(doc.boardFiducials()->rowCount(), 2);
    QCOMPARE(doc.boardBadMarks()->rowCount(), 1);
    QCOMPARE(cell(placements, 5, PlacementListModel::IdRole).toString(), QStringLiteral("R5"));
    QCOMPARE(cell(placements, 1, PlacementListModel::RotationRole).toDouble(), 90.0);
    QCOMPARE(cell(placements, 4, PlacementListModel::EnabledRole).toBool(), false);
    QCOMPARE(cell(doc.boardBadMarks(), 0, BadMarkListModel::XRole).toDouble(), 47.0);

    placements->setBoardIndex(0);  // unchanged: no reset
    QCOMPARE(resets.count(), 1);

    placements->setBoardIndex(5);  // out of range
    QCOMPARE(placements->rowCount(), 0);
    QVERIFY(!cell(placements, 0, PlacementListModel::IdRole).isValid());
}

void TstProgramDocument::getReturnsRoles()
{
    ProgramDocument doc;
    QVERIFY(doc.openFile(kSample));
    const QVariantMap p = doc.packages()->get(2);
    QCOMPARE(p.value(QStringLiteral("id")).toString(), QStringLiteral("QFN32-5x5"));
    QCOMPARE(p.value(QStringLiteral("nozzles")).toString(), QStringLiteral("N505"));
    QCOMPARE(p.value(QStringLiteral("speed")).toInt(), 50);
    QCOMPARE(p.value(QStringLiteral("visionMethod")).toInt(),
             static_cast<int>(VisionMethod::Bottom));
    QCOMPARE(p.value(QStringLiteral("tolerance")).toDouble(), 0.03);
    QCOMPARE(doc.packages()->get(1).value(QStringLiteral("nozzles")).toString(),
             QStringLiteral("N502,N503"));
    QVERIFY(doc.packages()->get(-1).isEmpty());
    QVERIFY(doc.packages()->get(3).isEmpty());
}

void TstProgramDocument::openInvalidKeepsCurrentProgram()
{
    QTemporaryDir dir;
    const QString bad = writeTemp(dir, QStringLiteral("bad.smt.xml"),
                                  "<SmtProgram formatVersion=\"1.0\">\n"
                                  "  <Info name=\"x\"/>\n"
                                  "</SmtProgram>\n");
    ProgramDocument doc;
    QVERIFY(doc.openFile(kSample));
    QSignalSpy messages(&doc, &ProgramDocument::messageRaised);
    QSignalSpy changed(&doc, &ProgramDocument::programChanged);

    QVERIFY(!doc.open(QUrl::fromLocalFile(bad)));
    QCOMPARE(changed.count(), 0);
    QCOMPARE(messages.count(), 1);
    const QList<QVariant> args = messages.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("打开失败"));
    QVERIFY2(args.at(1).toString().contains(QStringLiteral("4 个错误")),
             qPrintable(args.at(1).toString()));
    QVERIFY2(args.at(2).toString().contains(QStringLiteral("第 1 行：缺少 <Packages> 段")),
             qPrintable(args.at(2).toString()));

    QCOMPARE(doc.displayName(), QStringLiteral("example.smt.xml"));
    QCOMPARE(doc.jobPlacements()->rowCount(), 12);
}

void TstProgramDocument::openMissingFile()
{
    QTemporaryDir dir;
    ProgramDocument doc;
    QSignalSpy messages(&doc, &ProgramDocument::messageRaised);
    QVERIFY(!doc.openFile(dir.filePath(QStringLiteral("none.smt.xml"))));
    QCOMPARE(messages.count(), 1);
    QVERIFY(messages.first().at(2).toString().contains(QStringLiteral("无法打开文件")));
    QVERIFY(!doc.isLoaded());
}

void TstProgramDocument::openWithWarnings()
{
    QTemporaryDir dir;
    const QString path = writeTemp(dir, QStringLiteral("warn.smt.xml"),
                                   sampleWith("<Info name=", "<Info color=\"red\" name="));
    ProgramDocument doc;
    QSignalSpy messages(&doc, &ProgramDocument::messageRaised);
    QVERIFY(doc.openFile(path));
    QVERIFY(doc.isLoaded());
    QVERIFY(!doc.saveAsRequired());
    QCOMPARE(messages.count(), 1);
    QCOMPARE(messages.first().at(0).toString(), QStringLiteral("打开时有警告"));
    QVERIFY(messages.first().at(2).toString().contains(QStringLiteral("color")));
}

void TstProgramDocument::newerMinorVersionRequiresSaveAs()
{
    QTemporaryDir dir;
    const QByteArray original = sampleWith("formatVersion=\"1.0\"", "formatVersion=\"1.9\"");
    const QString path = writeTemp(dir, QStringLiteral("newer.smt.xml"), original);

    ProgramDocument doc;
    QSignalSpy messages(&doc, &ProgramDocument::messageRaised);
    QVERIFY(doc.openFile(path));
    QVERIFY(doc.saveAsRequired());
    QCOMPARE(messages.count(), 1);
    QCOMPARE(messages.takeFirst().at(0).toString(), QStringLiteral("文件由更新版本创建"));

    // Save is refused and the original file is untouched.
    QVERIFY(!doc.save());
    QCOMPARE(messages.count(), 1);
    QCOMPARE(messages.takeFirst().at(0).toString(), QStringLiteral("不能直接保存"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), original);
    f.close();

    QSignalSpy saveAsChanged(&doc, &ProgramDocument::saveAsRequiredChanged);
    const QString copy = dir.filePath(QStringLiteral("copy.smt.xml"));
    QVERIFY(doc.saveAs(QUrl::fromLocalFile(copy)));
    QCOMPARE(saveAsChanged.count(), 1);
    QVERIFY(!doc.saveAsRequired());
    QCOMPARE(doc.filePath(), QFileInfo(copy).absoluteFilePath());
    QVERIFY(doc.save());

    const ReadResult reread = ProgramReader::readFile(copy);
    QVERIFY(reread.ok());
    QVERIFY(!reread.newerMinorVersion);
    QCOMPARE(reread.program, doc.program());
}

void TstProgramDocument::saveRoundTrip()
{
    QTemporaryDir dir;
    ProgramDocument doc;
    QVERIFY(doc.openFile(kSample));
    QSignalSpy pathChanged(&doc, &ProgramDocument::filePathChanged);
    QSignalSpy messages(&doc, &ProgramDocument::messageRaised);

    const QString path = dir.filePath(QStringLiteral("out.smt.xml"));
    QVERIFY(doc.saveAs(QUrl::fromLocalFile(path)));
    QCOMPARE(messages.count(), 0);
    QCOMPARE(pathChanged.count(), 1);
    QCOMPARE(doc.displayName(), QStringLiteral("out.smt.xml"));

    const ReadResult reread = ProgramReader::readFile(path);
    QVERIFY(reread.ok());
    QVERIFY(reread.warnings().isEmpty());
    QCOMPARE(reread.program, doc.program());
    QCOMPARE(reread.program, ProgramReader::readFile(kSample).program);

    // Save writes back to the new path.
    QVERIFY(QFile::remove(path));
    QVERIFY(doc.save());
    QVERIFY(QFile::exists(path));

    // A failed save leaves the path alone and reports why.
    const QString unwritable = dir.filePath(QStringLiteral("no/such/dir/x.smt.xml"));
    QVERIFY(!doc.saveFile(unwritable));
    QCOMPARE(messages.count(), 1);
    QCOMPARE(messages.first().at(0).toString(), QStringLiteral("保存失败"));
    QCOMPARE(doc.filePath(), QFileInfo(path).absoluteFilePath());
}

void TstProgramDocument::rejectsNonLocalUrls()
{
    ProgramDocument doc;
    QVERIFY(doc.openFile(kSample));
    QSignalSpy messages(&doc, &ProgramDocument::messageRaised);
    QVERIFY(!doc.open(QUrl(QStringLiteral("http://example.com/a.smt.xml"))));
    QVERIFY(!doc.saveAs(QUrl(QStringLiteral("http://example.com/a.smt.xml"))));
    QCOMPARE(messages.count(), 2);
    QCOMPARE(doc.filePath(), QFileInfo(kSample).absoluteFilePath());
}

void TstProgramDocument::modelsStayConsistent()
{
    ProgramDocument doc;
    const QList<QAbstractItemModel *> models{
        doc.packages(),       doc.components(),     doc.boards(),
        doc.boardPlacements(), doc.boardFiducials(), doc.boardBadMarks(),
        doc.panelFiducials(), doc.boardInstances(), doc.jobPlacements(),
        doc.feeders()};
    std::vector<std::unique_ptr<QAbstractItemModelTester>> testers;
    for (QAbstractItemModel *m : models) {
        testers.push_back(std::make_unique<QAbstractItemModelTester>(
            m, QAbstractItemModelTester::FailureReportingMode::QtTest));
    }

    QVERIFY(doc.openFile(kSample));
    doc.boardPlacements()->setBoardIndex(0);
    doc.boardFiducials()->setBoardIndex(0);
    doc.boardBadMarks()->setBoardIndex(0);
    QVERIFY(doc.openFile(kSample));  // reload with a board selected
    doc.boardPlacements()->setBoardIndex(-1);
    doc.boardFiducials()->setBoardIndex(3);
}

QTEST_GUILESS_MAIN(TstProgramDocument)

#include "tst_programdocument.moc"
