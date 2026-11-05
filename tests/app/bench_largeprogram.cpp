#include "document/programdocument.h"

#include "validation/programvalidator.h"

#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

// Timings of the editor on a large program: a CAD import of kPlacements
// designators into the sample board, on a panel of kInstances boards, then
// single edits, undo / redo, save and open. Not run by ctest (no pass / fail
// threshold, timings depend on the machine); build with
// -DSMTPRO_BENCHMARKS=ON, preferably in Release, and run bench_largeprogram.
// List models are measured without views; QML delegates are not included.
namespace {

const QString kSample = QStringLiteral(SMTPRO_SAMPLES_DIR "/example.smt.xml");
constexpr int kPlacements = 2000;
constexpr int kInstances = 4;
constexpr int kRepeats = 10;

QString generatedCsv()
{
    const char *footprints[] = {"C0402", "C0603", "R0402", "R0603", "SOT23", "SOIC8",
                                "QFN32-5x5", "SOD123", "LED0603", "TSSOP20"};
    const char *values[] = {"A", "B", "C", "D", "E"};
    QString text = QStringLiteral("Ref,Val,Package,PosX,PosY,Rot,Side\n");
    for (int i = 0; i < kPlacements; ++i) {
        const int group = i % 50;
        text += QStringLiteral("P%1,%2,%3,%4,%5,%6,%7\n")
                    .arg(i + 1)
                    .arg(QLatin1String(values[group % 5]))
                    .arg(QLatin1String(footprints[group / 5]))
                    .arg(1.0 + (i % 48), 0, 'f', 4)
                    .arg(1.0 + (i / 48) % 38, 0, 'f', 4)
                    .arg((i % 4) * 90)
                    .arg(i % 3 == 0 ? QStringLiteral("bottom") : QStringLiteral("top"));
    }
    return text;
}

class Timer
{
public:
    explicit Timer(const char *what) : m_what(what) { m_timer.start(); }
    ~Timer() { qInfo("%-40s %8.1f ms", m_what, m_timer.nsecsElapsed() / 1e6); }

private:
    const char *m_what;
    QElapsedTimer m_timer;
};

}  // namespace

class BenchLargeProgram : public QObject
{
    Q_OBJECT

private slots:
    void run();
};

void BenchLargeProgram::run()
{
    ProgramDocument doc;
    QVERIFY(doc.openFile(kSample));
    QVERIFY(doc.edit(QStringLiteral("instances"), [](Program &p, QVector<IdRename> &) {
        while (p.panel.instances.size() < kInstances) {
            BoardInstance bi = p.panel.instances.first();
            bi.id = QString::number(p.panel.instances.size() + 1);
            bi.skippedPlacements.clear();
            p.panel.instances.append(bi);
        }
    }));
    doc.boardPlacements()->setBoardIndex(0);
    const QString csv = generatedCsv();
    CadImporter *imp = doc.cadImport();
    qInfo("%d placements x %d instances = %d job rows", kPlacements, kInstances,
          kPlacements * kInstances);

    {
        Timer t("import dialog: open + preview");
        QVERIFY(imp->openText(csv, QStringLiteral("big.csv")));
    }
    QCOMPARE(imp->rowCount(), kPlacements);
    QCOMPARE(imp->partModel()->rowCount(), 50);
    {
        Timer t("import dialog: change origin");
        QVERIFY(imp->setOriginX(0.5));
    }
    {
        Timer t("import dialog: change a part");
        imp->setPartComponent(0, 1);
    }
    {
        Timer t("import (edit + validate + sync)");
        QVERIFY(imp->importPlacements());
    }
    QCOMPARE(doc.program().boards.first().placements.size(), kPlacements + 6);
    QCOMPARE(doc.jobPlacements()->rowCount(), (kPlacements + 6) * kInstances);

    {
        Timer t("validate once");
        QVERIFY(!hasErrors(ProgramValidator::validate(doc.program())));
    }

    QElapsedTimer timer;
    QVector<double> edits;
    for (int i = 0; i < kRepeats; ++i) {
        timer.start();
        QVERIFY(doc.boardPlacements()->set(100, QStringLiteral("x"), 10.0 + i));
        edits << timer.nsecsElapsed() / 1e6;
    }
    std::sort(edits.begin(), edits.end());
    qInfo("%-40s %8.1f ms (median of %d)", "edit one placement field", edits.at(kRepeats / 2),
          kRepeats);
    {
        Timer t("undo");
        doc.undo();
    }
    {
        Timer t("redo");
        doc.redo();
    }
    {
        Timer t("toggle skip on job page");
        QVERIFY(doc.jobPlacements()->toggleSkip(kPlacements));
    }

    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("big.smt.xml"));
    {
        Timer t("save");
        QVERIFY(doc.saveFile(path));
    }
    {
        Timer t("open (read + validate + reset models)");
        QVERIFY(doc.openFile(path));
    }
    QCOMPARE(doc.jobPlacements()->rowCount(), (kPlacements + 6) * kInstances);
}

QTEST_GUILESS_MAIN(BenchLargeProgram)
#include "bench_largeprogram.moc"
