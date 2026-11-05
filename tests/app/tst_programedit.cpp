#include "document/programdocument.h"

#include "io/programreader.h"
#include "validation/programvalidator.h"

#include <QAbstractItemModelTester>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <vector>

namespace {

const QString kSample = QStringLiteral(SMTPRO_SAMPLES_DIR "/example.smt.xml");

QVariant cell(const QAbstractItemModel *model, int row, int role)
{
    return model->data(model->index(row, 0), role);
}

QString text(const QAbstractItemModel *model, int row, int role)
{
    return cell(model, row, role).toString();
}

// Structural signals of one model. An edit must not reset a model: that
// would drop the view's selection and scroll position.
struct Spy
{
    explicit Spy(QAbstractItemModel *m)
        : reset(m, &QAbstractItemModel::modelReset),
          inserted(m, &QAbstractItemModel::rowsInserted),
          removed(m, &QAbstractItemModel::rowsRemoved),
          moved(m, &QAbstractItemModel::rowsMoved),
          changed(m, &QAbstractItemModel::dataChanged)
    {
    }

    int structural() const
    {
        return reset.count() + inserted.count() + removed.count() + moved.count();
    }
    void clear()
    {
        reset.clear();
        inserted.clear();
        removed.clear();
        moved.clear();
        changed.clear();
    }

    QSignalSpy reset;
    QSignalSpy inserted;
    QSignalSpy removed;
    QSignalSpy moved;
    QSignalSpy changed;
};

// The sample, opened with board 0 selected, every model under a tester.
struct Fixture
{
    ProgramDocument doc;
    std::vector<std::unique_ptr<QAbstractItemModelTester>> testers;
    QSignalSpy messages{&doc, &ProgramDocument::messageRaised};

    explicit Fixture(bool open = true)
    {
        const QList<QAbstractItemModel *> models{
            doc.packages(),        doc.components(),     doc.boards(),
            doc.boardPlacements(), doc.boardFiducials(), doc.boardBadMarks(),
            doc.panelFiducials(),  doc.boardInstances(), doc.jobPlacements(),
            doc.feeders()};
        for (QAbstractItemModel *m : models) {
            testers.push_back(std::make_unique<QAbstractItemModelTester>(
                m, QAbstractItemModelTester::FailureReportingMode::QtTest));
        }
        if (open && !doc.openFile(kSample))
            qFatal("cannot open the sample");
        selectBoard(0);
    }

    void selectBoard(int index)
    {
        doc.boardPlacements()->setBoardIndex(index);
        doc.boardFiducials()->setBoardIndex(index);
        doc.boardBadMarks()->setBoardIndex(index);
    }

    QString lastMessage() const
    {
        if (messages.isEmpty())
            return {};
        const QList<QVariant> &m = messages.last();
        return m.at(1).toString() + QLatin1Char('\n') + m.at(2).toString();
    }
};

}  // namespace

class TstProgramEdit : public QObject
{
    Q_OBJECT

private slots:
    void editValueAndUndo();
    void rejectDuplicateId();
    void rejectInvalidNumber();
    void renameComponentKeepsRows();
    void renamePlacementKeepsRows();
    void renameInstanceKeepsRows();
    void removeComponentCascades();
    void addRows();
    void moveInstance();
    void removeBoardBeforeSelected();
    void removeSelectedBoard();
    void removeLastInstanceRefused();
    void boardChangeKeepsValidSkips();
    void editSkips();
    void toggleSkip();
    void feederFields();
    void panelAndMachine();
    void warningReported();
    void modifiedAndSave();
    void saveUnmodifiedKeepsTime();
    void openClearsUndo();
    void newProgram();
};

void TstProgramEdit::editValueAndUndo()
{
    Fixture f;
    ComponentListModel *m = f.doc.components();
    Spy spy(m);
    QSignalSpy undoState(&f.doc, &ProgramDocument::undoStateChanged);

    QVERIFY(m->set(1, QStringLiteral("value"), QStringLiteral("4K7")));
    QCOMPARE(text(m, 1, ComponentListModel::ValueRole), QStringLiteral("4K7"));
    QVERIFY(f.doc.isModified());
    QVERIFY(f.doc.canUndo());
    QCOMPARE(f.doc.undoText(), QStringLiteral("修改元件 RC0603-10K"));
    QCOMPARE(spy.structural(), 0);
    QVERIFY(spy.changed.count() > 0);
    QVERIFY(undoState.count() > 0);

    f.doc.undo();
    QCOMPARE(text(m, 1, ComponentListModel::ValueRole), QStringLiteral("10K"));
    QVERIFY(!f.doc.isModified());
    QVERIFY(f.doc.canRedo());
    f.doc.redo();
    QCOMPARE(text(m, 1, ComponentListModel::ValueRole), QStringLiteral("4K7"));
    QCOMPARE(spy.structural(), 0);

    // Setting the same value is not a step.
    f.doc.undo();
    QVERIFY(m->set(1, QStringLiteral("value"), QStringLiteral("10K")));
    QVERIFY(!f.doc.canUndo());
    QCOMPARE(f.messages.count(), 0);
}

void TstProgramEdit::rejectDuplicateId()
{
    Fixture f;
    const Program before = f.doc.program();
    QVERIFY(!f.doc.components()->set(1, QStringLiteral("id"), QStringLiteral("CC0402-100NF")));
    QCOMPARE(f.messages.count(), 1);
    QCOMPARE(f.messages.first().at(0).toString(), QStringLiteral("修改未生效"));
    QVERIFY2(f.lastMessage().contains(QStringLiteral("重复")), qPrintable(f.lastMessage()));
    QCOMPARE(f.doc.program(), before);
    QVERIFY(!f.doc.canUndo());
    QVERIFY(!f.doc.isModified());

    QVERIFY(!f.doc.boardPlacements()->set(0, QStringLiteral("id"), QStringLiteral("  ")));
    QVERIFY(f.lastMessage().contains(QStringLiteral("不能为空")));
    QCOMPARE(f.doc.program(), before);
}

void TstProgramEdit::rejectInvalidNumber()
{
    Fixture f;
    PackageListModel *m = f.doc.packages();
    QVERIFY(!m->set(0, QStringLiteral("bodyLength"), QStringLiteral("abc")));
    QCOMPARE(f.messages.last().at(0).toString(), QStringLiteral("输入无效"));
    QVERIFY(!m->set(0, QStringLiteral("bodyLength"), QStringLiteral("1,5")));  // no locale comma
    QVERIFY(!m->set(0, QStringLiteral("bodyLength"), QStringLiteral("")));
    QVERIFY(!m->set(0, QStringLiteral("bodyLength"), QStringLiteral("0")));  // validator
    QVERIFY(f.lastMessage().contains(QStringLiteral("必须大于 0")));
    QVERIFY(!m->set(0, QStringLiteral("speed"), QStringLiteral("101")));
    QVERIFY(!m->set(0, QStringLiteral("pinCount"), QStringLiteral("2.5")));
    QVERIFY(!f.doc.canUndo());

    QVERIFY(m->set(0, QStringLiteral("bodyLength"), QStringLiteral(" 1.25 ")));
    QCOMPARE(cell(m, 0, PackageListModel::BodyLengthRole).toDouble(), 1.25);
    QVERIFY(m->set(0, QStringLiteral("bodyLength"), 3.0));
    QCOMPARE(cell(m, 0, PackageListModel::BodyLengthRole).toDouble(), 3.0);
    QVERIFY(m->set(0, QStringLiteral("pinCount"), 4.0));  // QML numbers are doubles
    QCOMPARE(cell(m, 0, PackageListModel::PinCountRole).toInt(), 4);
    QVERIFY(m->set(0, QStringLiteral("nozzles"), QStringLiteral(" N1 , ,N2 ")));
    QCOMPARE(text(m, 0, PackageListModel::NozzlesRole), QStringLiteral("N1,N2"));
    QVERIFY(m->set(0, QStringLiteral("visionMethod"), 2));
    QCOMPARE(f.doc.program().packages[0].vision.method, VisionMethod::Top);
    // Read-only and unknown roles.
    QVERIFY(!f.doc.components()->set(0, QStringLiteral("placementCount"), 3));
    QTest::ignoreMessage(QtWarningMsg, "ProgramListModel::set: unknown role nope");
    QVERIFY(!m->set(0, QStringLiteral("nope"), 1));
}

void TstProgramEdit::renameComponentKeepsRows()
{
    Fixture f;
    Spy components(f.doc.components());
    Spy jobs(f.doc.jobPlacements());
    Spy feeders(f.doc.feeders());

    QVERIFY(f.doc.components()->set(1, QStringLiteral("id"), QStringLiteral("RC0603-4K7")));
    QCOMPARE(f.doc.undoText(), QStringLiteral("重命名元件 RC0603-10K"));
    QCOMPARE(components.structural(), 0);
    QCOMPARE(jobs.structural(), 0);
    QCOMPARE(feeders.structural(), 0);
    // References follow: placements R1 (job row 3) and feeders FD2, FD3.
    QCOMPARE(text(f.doc.jobPlacements(), 3, JobPlacementListModel::ComponentRole),
             QStringLiteral("RC0603-4K7"));
    QCOMPARE(text(f.doc.feeders(), 1, FeederListModel::ComponentRole), QStringLiteral("RC0603-4K7"));
    QCOMPARE(text(f.doc.boardPlacements(), 3, PlacementListModel::ComponentRole),
             QStringLiteral("RC0603-4K7"));
    QCOMPARE(cell(f.doc.components(), 1, ComponentListModel::PlacementCountRole).toInt(), 3);

    f.doc.undo();
    QCOMPARE(components.structural(), 0);
    QCOMPARE(jobs.structural(), 0);
    QCOMPARE(text(f.doc.feeders(), 2, FeederListModel::ComponentRole), QStringLiteral("RC0603-10K"));
}

void TstProgramEdit::renamePlacementKeepsRows()
{
    Fixture f;
    Spy placements(f.doc.boardPlacements());
    Spy jobs(f.doc.jobPlacements());

    // R1 is skipped on instance 2; the skip follows the rename.
    QVERIFY(f.doc.boardPlacements()->set(3, QStringLiteral("id"), QStringLiteral("R10")));
    QCOMPARE(placements.structural(), 0);
    QCOMPARE(jobs.structural(), 0);
    QCOMPARE(text(f.doc.jobPlacements(), 9, JobPlacementListModel::IdRole), QStringLiteral("R10"));
    QCOMPARE(cell(f.doc.jobPlacements(), 9, JobPlacementListModel::EnabledRole).toBool(), false);
    QCOMPARE(text(f.doc.boardInstances(), 1, BoardInstanceListModel::SkipsRole), QStringLiteral("R10"));

    f.doc.undo();
    QCOMPARE(jobs.structural(), 0);
    QCOMPARE(text(f.doc.boardInstances(), 1, BoardInstanceListModel::SkipsRole), QStringLiteral("R1"));
}

void TstProgramEdit::renameInstanceKeepsRows()
{
    Fixture f;
    Spy instances(f.doc.boardInstances());
    Spy jobs(f.doc.jobPlacements());
    QVERIFY(f.doc.boardInstances()->set(1, QStringLiteral("id"), QStringLiteral("B")));
    QCOMPARE(instances.structural(), 0);
    QCOMPARE(jobs.structural(), 0);
    QCOMPARE(text(f.doc.jobPlacements(), 6, JobPlacementListModel::InstanceRole), QStringLiteral("B"));
}

void TstProgramEdit::removeComponentCascades()
{
    Fixture f;
    ComponentListModel *m = f.doc.components();
    // RC0603-10K: R1, R2, R5 on the board, FD2 and FD3.
    QCOMPARE(m->removalImpact(1), QStringLiteral("同时会删除：3 个贴片位、2 个送料器。"));
    QVERIFY(m->removalImpact(2).contains(QStringLiteral("1 个贴片位")));

    Spy components(m);
    Spy jobs(f.doc.jobPlacements());
    Spy feeders(f.doc.feeders());
    QVERIFY(m->remove(1));
    QCOMPARE(m->rowCount(), 2);
    QCOMPARE(f.doc.jobPlacements()->rowCount(), 6);
    QCOMPARE(f.doc.feeders()->rowCount(), 2);
    QCOMPARE(f.doc.boardPlacements()->rowCount(), 3);
    QCOMPARE(text(f.doc.boardInstances(), 1, BoardInstanceListModel::SkipsRole), QString());
    QCOMPARE(f.doc.jobPlacements()->enabledCount(), 6);
    QCOMPARE(components.reset.count() + jobs.reset.count() + feeders.reset.count(), 0);
    QCOMPARE(components.removed.count(), 1);
    QCOMPARE(feeders.removed.count(), 1);  // FD2, FD3 are adjacent
    QCOMPARE(jobs.removed.count(), 2);     // one run per instance

    f.doc.undo();
    QCOMPARE(f.doc.jobPlacements()->rowCount(), 12);
    QCOMPARE(f.doc.feeders()->rowCount(), 4);
    QCOMPARE(components.reset.count() + jobs.reset.count() + feeders.reset.count(), 0);
    QCOMPARE(jobs.inserted.count(), 2);
    QCOMPARE(text(f.doc.jobPlacements(), 9, JobPlacementListModel::IdRole), QStringLiteral("R1"));
    QCOMPARE(f.doc.program(), ProgramReader::readFile(kSample).program);
}

void TstProgramEdit::addRows()
{
    Fixture f;
    QCOMPARE(f.doc.components()->add(), 3);
    QCOMPARE(text(f.doc.components(), 3, ComponentListModel::IdRole), QStringLiteral("PART1"));
    QCOMPARE(text(f.doc.components(), 3, ComponentListModel::PackageRole), QStringLiteral("C0402"));
    QCOMPARE(f.doc.undoText(), QStringLiteral("添加元件 PART1"));
    QCOMPARE(f.doc.packages()->add(), 3);
    QCOMPARE(text(f.doc.packages(), 3, PackageListModel::IdRole), QStringLiteral("PKG1"));

    Spy jobs(f.doc.jobPlacements());
    QCOMPARE(f.doc.boardPlacements()->add(), 6);
    QCOMPARE(text(f.doc.boardPlacements(), 6, PlacementListModel::IdRole), QStringLiteral("P1"));
    // One new row per instance, after each instance's last placement.
    QCOMPARE(f.doc.jobPlacements()->rowCount(), 14);
    QCOMPARE(jobs.reset.count(), 0);
    QCOMPARE(jobs.inserted.count(), 2);
    QCOMPARE(text(f.doc.jobPlacements(), 6, JobPlacementListModel::IdRole), QStringLiteral("P1"));
    QCOMPARE(text(f.doc.jobPlacements(), 13, JobPlacementListModel::InstanceRole), QStringLiteral("2"));

    QCOMPARE(f.doc.boardFiducials()->add(), 2);
    QCOMPARE(text(f.doc.boardFiducials(), 2, FiducialListModel::IdRole), QStringLiteral("FID3"));
    QCOMPARE(f.doc.panelFiducials()->add(), 2);
    QCOMPARE(text(f.doc.panelFiducials(), 2, FiducialListModel::IdRole), QStringLiteral("PFID3"));
    QCOMPARE(f.doc.boardBadMarks()->add(), 1);
    QCOMPARE(f.doc.boardInstances()->add(), 2);
    QCOMPARE(text(f.doc.boardInstances(), 2, BoardInstanceListModel::IdRole), QStringLiteral("3"));
    QCOMPARE(f.doc.feeders()->add(), 4);
    QCOMPARE(text(f.doc.feeders(), 4, FeederListModel::SlotRole), QStringLiteral("S1"));
    QCOMPARE(f.doc.boards()->add(), 1);
    QCOMPARE(text(f.doc.boards(), 1, BoardListModel::IdRole), QStringLiteral("PCB1"));
    QCOMPARE(f.messages.count(), 0);
    QVERIFY(ProgramValidator::validate(f.doc.program()).isEmpty());
}

void TstProgramEdit::moveInstance()
{
    Fixture f;
    BoardInstanceListModel *m = f.doc.boardInstances();
    Spy instances(m);
    Spy jobs(f.doc.jobPlacements());
    QCOMPARE(m->move(1, -1), 0);
    QCOMPARE(text(m, 0, BoardInstanceListModel::IdRole), QStringLiteral("2"));
    QCOMPARE(instances.moved.count(), 1);
    QCOMPARE(instances.reset.count(), 0);
    QCOMPARE(jobs.reset.count(), 0);
    QCOMPARE(jobs.moved.count(), 6);
    QCOMPARE(text(f.doc.jobPlacements(), 0, JobPlacementListModel::InstanceRole), QStringLiteral("2"));
    QCOMPARE(m->move(0, -1), -1);  // already first
    QCOMPARE(m->move(1, 1), -1);   // already last
    f.doc.undo();
    QCOMPARE(text(m, 0, BoardInstanceListModel::IdRole), QStringLiteral("1"));
}

void TstProgramEdit::removeBoardBeforeSelected()
{
    Fixture f;
    // A second board PCB1 with one placement and one instance, selected.
    QCOMPARE(f.doc.boards()->add(), 1);
    f.selectBoard(1);
    QCOMPARE(f.doc.boardPlacements()->add(), 0);
    QCOMPARE(f.doc.boardInstances()->add(), 2);
    QVERIFY(f.doc.boardInstances()->set(2, QStringLiteral("board"), QStringLiteral("PCB1")));

    // Removing CTRL takes instances 1 and 2; PCB1 moves to row 0.
    QVERIFY(f.doc.boards()->removalImpact(0).contains(QStringLiteral("2 个子板实例")));
    Spy placements(f.doc.boardPlacements());
    QSignalSpy indexChanged(f.doc.boardPlacements(), &BoardContentModel::boardIndexChanged);
    QVERIFY(f.doc.boards()->remove(0));
    QCOMPARE(f.doc.boardPlacements()->boardIndex(), 0);
    QCOMPARE(indexChanged.count(), 1);
    QCOMPARE(placements.structural(), 0);
    QCOMPARE(f.doc.boardPlacements()->rowCount(), 1);
    QCOMPARE(f.doc.boardInstances()->rowCount(), 1);
    QCOMPARE(f.doc.jobPlacements()->rowCount(), 1);
    // The view follows the board to row 0: same rows, no reset.
    f.selectBoard(0);
    QCOMPARE(placements.structural(), 0);

    f.doc.undo();
    QCOMPARE(f.doc.boardPlacements()->boardIndex(), 1);
    QCOMPARE(text(f.doc.boardPlacements(), 0, PlacementListModel::IdRole), QStringLiteral("P1"));
    QCOMPARE(f.doc.jobPlacements()->rowCount(), 13);
}

void TstProgramEdit::removeSelectedBoard()
{
    Fixture f;
    QCOMPARE(f.doc.boards()->add(), 1);
    f.selectBoard(1);
    QVERIFY(f.doc.boards()->removalImpact(1).isEmpty());
    QVERIFY(f.doc.boards()->remove(1));
    // Nothing at row 1 any more; the view then selects the remaining board.
    QCOMPARE(f.doc.boardPlacements()->rowCount(), 0);
    f.selectBoard(0);
    QCOMPARE(f.doc.boardPlacements()->rowCount(), 6);
}

void TstProgramEdit::removeLastInstanceRefused()
{
    Fixture f;
    QVERIFY(f.doc.boardInstances()->remove(1));
    QVERIFY(!f.doc.boardInstances()->remove(0));
    QVERIFY(f.lastMessage().contains(QStringLiteral("子板实例")));
    QCOMPARE(f.doc.boardInstances()->rowCount(), 1);
    // Also when the board is removed and would take the last instance.
    QVERIFY(!f.doc.boards()->remove(0));
}

void TstProgramEdit::boardChangeKeepsValidSkips()
{
    Fixture f;
    QCOMPARE(f.doc.boards()->add(), 1);
    f.selectBoard(1);
    QCOMPARE(f.doc.boardPlacements()->add(), 0);
    QVERIFY(f.doc.boardPlacements()->set(0, QStringLiteral("id"), QStringLiteral("R1")));
    QVERIFY(f.doc.boardInstances()->set(1, QStringLiteral("skips"), QStringLiteral("R1, R2")));
    // PCB1 has R1 but no R2.
    QVERIFY(f.doc.boardInstances()->set(1, QStringLiteral("board"), QStringLiteral("PCB1")));
    QCOMPARE(text(f.doc.boardInstances(), 1, BoardInstanceListModel::SkipsRole), QStringLiteral("R1"));
    QCOMPARE(f.doc.jobPlacements()->rowCount(), 7);
}

void TstProgramEdit::editSkips()
{
    Fixture f;
    BoardInstanceListModel *m = f.doc.boardInstances();
    QVERIFY(m->set(0, QStringLiteral("skips"), QStringLiteral("U1，C1  C2")));
    QCOMPARE(text(m, 0, BoardInstanceListModel::SkipsRole), QStringLiteral("U1, C1, C2"));
    QCOMPARE(cell(m, 0, BoardInstanceListModel::PlacementCountRole).toInt(), 2);
    QVERIFY(!m->set(0, QStringLiteral("skips"), QStringLiteral("R1, R1")));
    QVERIFY(f.lastMessage().contains(QStringLiteral("重复")));
    QVERIFY(!m->set(0, QStringLiteral("skips"), QStringLiteral("X9")));
    QVERIFY(f.lastMessage().contains(QStringLiteral("X9")));
    QVERIFY(m->set(0, QStringLiteral("skips"), QString()));
    QCOMPARE(cell(m, 0, BoardInstanceListModel::PlacementCountRole).toInt(), 5);
}

void TstProgramEdit::toggleSkip()
{
    Fixture f;
    JobPlacementListModel *m = f.doc.jobPlacements();
    QSignalSpy enabled(m, &JobPlacementListModel::enabledCountChanged);
    QVERIFY(m->toggleSkip(9));  // instance 2 R1: restore
    QCOMPARE(m->enabledCount(), 10);
    QCOMPARE(cell(m, 9, JobPlacementListModel::SkippedRole).toBool(), false);
    QVERIFY(enabled.count() > 0);
    QCOMPARE(f.doc.undoText(), QStringLiteral("恢复子板 2 的贴片位 R1"));
    QVERIFY(m->toggleSkip(0));  // instance 1 U1: skip
    QCOMPARE(m->enabledCount(), 9);
    QCOMPARE(text(f.doc.boardInstances(), 0, BoardInstanceListModel::SkipsRole), QStringLiteral("U1"));
    QVERIFY(f.doc.boardInstances()->set(1, QStringLiteral("enabled"), false));
    QCOMPARE(m->enabledCount(), 4);
}

void TstProgramEdit::feederFields()
{
    Fixture f;
    FeederListModel *m = f.doc.feeders();
    QCOMPARE(cell(m, 3, FeederListModel::TypeIndexRole).toInt(), int(FeederType::Tray));
    QVERIFY(!m->set(3, QStringLiteral("trayStartIndex"), 50));  // 5 x 10 pockets
    QVERIFY(f.lastMessage().contains(QStringLiteral("startIndex")));
    QVERIFY(m->set(3, QStringLiteral("trayStartIndex"), 49));
    QVERIFY(m->set(0, QStringLiteral("rotation"), QStringLiteral("-90")));
    QCOMPARE(cell(m, 0, FeederListModel::RotationRole).toDouble(), 270.0);
    QVERIFY(m->set(0, QStringLiteral("typeIndex"), int(FeederType::Tray)));
    QCOMPARE(text(m, 0, FeederListModel::TypeRole), QStringLiteral("料盘"));
    QVERIFY(m->set(0, QStringLiteral("component"), QStringLiteral("STM32G031K8U6")));
    QVERIFY(!m->set(0, QStringLiteral("component"), QStringLiteral("NOPE")));
    QCOMPARE(cell(f.doc.components(), 2, ComponentListModel::FeederCountRole).toInt(), 2);
    QVERIFY(m->remove(0));
    QCOMPARE(m->rowCount(), 3);
}

void TstProgramEdit::panelAndMachine()
{
    Fixture f;
    QSignalSpy content(&f.doc, &ProgramDocument::contentChanged);
    QVERIFY(f.doc.setPanelWidth(QStringLiteral("150")));
    QCOMPARE(f.doc.panelWidth(), 150.0);
    QVERIFY(content.count() > 0);
    QVERIFY(!f.doc.setPanelHeight(QStringLiteral("-1")));
    QCOMPARE(f.doc.panelHeight(), 50.0);
    QVERIFY(f.doc.setMachineModel(QStringLiteral(" M2 ")));
    QCOMPARE(f.doc.machineModel(), QStringLiteral("M2"));
    f.doc.undo();
    QCOMPARE(f.doc.machineModel(), QStringLiteral("SMT-DEMO-01"));

    // A program without FeederSetup gets one only for a non-empty model.
    Fixture blank(false);
    blank.doc.newProgram();
    QVERIFY(blank.doc.setMachineModel(QString()));
    QVERIFY(!blank.doc.program().feederSetup);
    QVERIFY(blank.doc.setMachineModel(QStringLiteral("M1")));
    QVERIFY(blank.doc.program().feederSetup);
}

void TstProgramEdit::warningReported()
{
    Fixture f;
    QSignalSpy status(&f.doc, &ProgramDocument::statusMessage);
    QVERIFY(f.doc.boardPlacements()->set(0, QStringLiteral("x"), QStringLiteral("60")));
    QCOMPARE(f.messages.count(), 0);  // accepted
    QCOMPARE(status.count(), 1);
    QVERIFY(status.first().at(0).toString().contains(QStringLiteral("之外")));
    // An existing warning is not repeated on the next edit.
    QVERIFY(f.doc.boardPlacements()->set(1, QStringLiteral("x"), QStringLiteral("21")));
    QCOMPARE(status.count(), 1);
}

void TstProgramEdit::modifiedAndSave()
{
    QTemporaryDir dir;
    Fixture f;
    QSignalSpy status(&f.doc, &ProgramDocument::statusMessage);
    const QString path = dir.filePath(QStringLiteral("edited.smt.xml"));
    QVERIFY(f.doc.saveAs(QUrl::fromLocalFile(path)));

    QVERIFY(f.doc.components()->set(0, QStringLiteral("description"), QStringLiteral("X5R")));
    QVERIFY(f.doc.isModified());
    QVERIFY(f.doc.save());
    QVERIFY(!f.doc.isModified());
    QCOMPARE(status.last().at(0).toString(), QStringLiteral("已保存 edited.smt.xml"));

    const ReadResult r = ProgramReader::readFile(path);
    QVERIFY(r.ok());
    QCOMPARE(r.program.components[0].description, QStringLiteral("X5R"));
    QVERIFY(r.program.info.modifiedAt != QStringLiteral("2026-11-06T10:00:00+08:00"));
    QVERIFY2(QRegularExpression(QStringLiteral("^\\d{4}-\\d\\d-\\d\\dT\\d\\d:\\d\\d:\\d\\d([+-]\\d\\d:\\d\\d|Z)$"))
                 .match(r.program.info.modifiedAt)
                 .hasMatch(),
             qPrintable(r.program.info.modifiedAt));
    QCOMPARE(r.program, f.doc.program());

    // Undo past the save point is modified again; redo returns to clean.
    f.doc.undo();
    QVERIFY(f.doc.isModified());
    f.doc.redo();
    QVERIFY(!f.doc.isModified());
    // The save time is file metadata and survives undo.
    f.doc.undo();
    QCOMPARE(f.doc.program().info.modifiedAt, r.program.info.modifiedAt);
}

void TstProgramEdit::saveUnmodifiedKeepsTime()
{
    QTemporaryDir dir;
    Fixture f;
    const QString path = dir.filePath(QStringLiteral("same.smt.xml"));
    QVERIFY(f.doc.saveAs(QUrl::fromLocalFile(path)));
    QCOMPARE(ProgramReader::readFile(path).program.info.modifiedAt,
             QStringLiteral("2026-11-06T10:00:00+08:00"));
}

void TstProgramEdit::openClearsUndo()
{
    Fixture f;
    QVERIFY(f.doc.components()->set(0, QStringLiteral("value"), QStringLiteral("1uF")));
    QVERIFY(f.doc.isModified());
    QVERIFY(f.doc.openFile(kSample));
    QVERIFY(!f.doc.canUndo());
    QVERIFY(!f.doc.isModified());
}

void TstProgramEdit::newProgram()
{
    Fixture f(false);
    QSignalSpy path(&f.doc, &ProgramDocument::filePathChanged);
    f.doc.newProgram();
    QVERIFY(f.doc.isLoaded());
    QVERIFY(!f.doc.isModified());
    QVERIFY(f.doc.displayName().isEmpty());
    QVERIFY(!f.doc.save());  // no path yet: the view asks for one
    QCOMPARE(f.doc.boards()->rowCount(), 1);
    QCOMPARE(f.doc.boardInstances()->rowCount(), 1);
    QCOMPARE(f.doc.jobPlacements()->rowCount(), 0);
    QVERIFY(!f.doc.program().info.createdAt.isEmpty());

    // Placements and feeders need a component, components need a package.
    QCOMPARE(f.doc.components()->add(), -1);
    QVERIFY(f.lastMessage().contains(QStringLiteral("封装")));
    f.selectBoard(0);
    QCOMPARE(f.doc.boardPlacements()->add(), -1);
    QCOMPARE(f.doc.feeders()->add(), -1);
    QCOMPARE(f.doc.packages()->add(), 0);
    QCOMPARE(f.doc.components()->add(), 0);
    QCOMPARE(f.doc.boardPlacements()->add(), 0);
    QCOMPARE(f.doc.feeders()->add(), 0);
    QCOMPARE(f.doc.jobPlacements()->rowCount(), 1);
    QVERIFY(f.doc.isModified());

    QTemporaryDir dir;
    const QString file = dir.filePath(QStringLiteral("new.smt.xml"));
    QVERIFY(f.doc.saveAs(QUrl::fromLocalFile(file)));
    const ReadResult r = ProgramReader::readFile(file);
    QVERIFY(r.diagnostics.isEmpty());
    QCOMPARE(r.program, f.doc.program());

    // Undo all the way back to the blank program.
    while (f.doc.canUndo())
        f.doc.undo();
    QCOMPARE(f.doc.packages()->rowCount(), 0);
    QVERIFY(!f.doc.program().feederSetup);
}

QTEST_GUILESS_MAIN(TstProgramEdit)
#include "tst_programedit.moc"
