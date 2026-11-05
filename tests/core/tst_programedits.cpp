#include "edit/commandstack.h"
#include "edit/programedits.h"
#include "io/programreader.h"
#include "io/programwriter.h"
#include "validation/programvalidator.h"

#include "testprograms.h"

#include <QtTest>

namespace {

QStringList placementIds(const Board &b)
{
    QStringList ids;
    for (const Placement &p : b.placements)
        ids << p.id;
    return ids;
}

QStringList feederIds(const Program &p)
{
    QStringList ids;
    for (const Feeder &f : p.feederSetup->feeders)
        ids << f.id;
    return ids;
}

void expectValid(const Program &p)
{
    const QVector<Diagnostic> d = ProgramValidator::validate(p);
    QVERIFY2(d.isEmpty(), qPrintable(TestPrograms::describe(d)));
}

EditStep step(const QString &text, int marker)
{
    EditStep s;
    s.text = text;
    s.after.panel.width = marker;
    return s;
}

}  // namespace

class TstProgramEdits : public QObject
{
    Q_OBJECT

private slots:
    void uniqueId();
    void blankProgramIsValid();
    void renameUpdatesReferences();
    void removeComponentCascades();
    void removePackageCascades();
    void removeBoardCascades();
    void removePlacementClearsSkips();
    void removalImpact();
    void stackUndoRedo();
    void stackClean();
    void stackLimit();
};

void TstProgramEdits::uniqueId()
{
    QCOMPARE(ProgramEdits::uniqueId(QStringLiteral("R"), {}), QStringLiteral("R1"));
    QCOMPARE(ProgramEdits::uniqueId(QStringLiteral("R"),
                                    {QStringLiteral("R1"), QStringLiteral("R3")}),
             QStringLiteral("R2"));
    QCOMPARE(ProgramEdits::uniqueId(QString(), {QStringLiteral("1")}), QStringLiteral("2"));
}

void TstProgramEdits::blankProgramIsValid()
{
    const Program p = ProgramEdits::blankProgram();
    expectValid(p);
    QCOMPARE(p.boards.size(), 1);
    QCOMPARE(p.panel.instances.size(), 1);
    QVERIFY(!p.feederSetup);

    // Survives a save and reopen without any diagnostic.
    const ReadResult r = ProgramReader::read(ProgramWriter::toXml(p));
    QVERIFY(r.diagnostics.isEmpty());
    QVERIFY(r.program == p);
}

void TstProgramEdits::renameUpdatesReferences()
{
    Program p = TestPrograms::minimal();
    ProgramEdits::renamePackage(p, 0, QStringLiteral("R0603-X"));
    QCOMPARE(p.components[0].package, QStringLiteral("R0603-X"));
    QCOMPARE(p.components[1].package, QStringLiteral("R0603-X"));
    QCOMPARE(p.components[2].package, QStringLiteral("C0402"));

    ProgramEdits::renameComponent(p, 0, QStringLiteral("RC-10K-X"));
    QCOMPARE(p.boards[0].placements[0].component, QStringLiteral("RC-10K-X"));
    QCOMPARE(p.boards[0].placements[1].component, QStringLiteral("RC-10K-X"));
    QCOMPARE(p.boards[0].placements[2].component, QStringLiteral("RC-1K"));
    QCOMPARE(feederIds(p).size(), 3);
    QCOMPARE(p.feederSetup->feeders[0].component, QStringLiteral("RC-10K-X"));
    QCOMPARE(p.feederSetup->feeders[2].component, QStringLiteral("RC-10K-X"));

    ProgramEdits::renameBoard(p, 0, QStringLiteral("PCB-X"));
    QCOMPARE(p.panel.instances[0].board, QStringLiteral("PCB-X"));
    QCOMPARE(p.panel.instances[1].board, QStringLiteral("PCB-X"));

    ProgramEdits::renamePlacement(p, 0, 1, QStringLiteral("R2-X"));
    QCOMPARE(p.panel.instances[1].skippedPlacements, QStringList{QStringLiteral("R2-X")});
    expectValid(p);
}

void TstProgramEdits::removeComponentCascades()
{
    Program p = TestPrograms::minimal();
    ProgramEdits::removeComponent(p, 0);  // RC-10K: R1, R2, F1, F3
    QCOMPARE(p.components.size(), 2);
    QCOMPARE(placementIds(p.boards[0]), (QStringList{QStringLiteral("R3"), QStringLiteral("C1")}));
    QCOMPARE(feederIds(p), QStringList{QStringLiteral("F2")});
    // R2 was skipped on instance 2; the skip goes with it.
    QVERIFY(p.panel.instances[1].skippedPlacements.isEmpty());
    expectValid(p);
}

void TstProgramEdits::removePackageCascades()
{
    Program p = TestPrograms::minimal();
    ProgramEdits::removePackage(p, 0);  // R0603: RC-10K, RC-1K
    QCOMPARE(p.packages.size(), 1);
    QCOMPARE(p.components.size(), 1);
    QCOMPARE(p.components[0].id, QStringLiteral("CC-100N"));
    QCOMPARE(placementIds(p.boards[0]), QStringList{QStringLiteral("C1")});
    QVERIFY(p.feederSetup->feeders.isEmpty());
    QVERIFY(p.panel.instances[1].skippedPlacements.isEmpty());
    expectValid(p);
}

void TstProgramEdits::removeBoardCascades()
{
    Program p = TestPrograms::minimal();
    Board second = p.boards[0];
    second.id = QStringLiteral("PCB2");
    p.boards.append(second);
    BoardInstance bi = p.panel.instances[0];
    bi.id = QStringLiteral("3");
    bi.board = QStringLiteral("PCB2");
    p.panel.instances.append(bi);

    ProgramEdits::removeBoard(p, 0);
    QCOMPARE(p.boards.size(), 1);
    QCOMPARE(p.panel.instances.size(), 1);
    QCOMPARE(p.panel.instances[0].id, QStringLiteral("3"));
    expectValid(p);

    // Removing the only board leaves no instance, which the validator rejects.
    ProgramEdits::removeBoard(p, 0);
    QVERIFY(hasErrors(ProgramValidator::validate(p)));
}

void TstProgramEdits::removePlacementClearsSkips()
{
    Program p = TestPrograms::minimal();
    ProgramEdits::removePlacement(p, 0, 1);  // R2
    QCOMPARE(placementIds(p.boards[0]).size(), 3);
    QVERIFY(p.panel.instances[1].skippedPlacements.isEmpty());
    expectValid(p);
}

void TstProgramEdits::removalImpact()
{
    const Program p = TestPrograms::minimal();
    RemovalImpact i = ProgramEdits::componentRemovalImpact(p, 0);
    QCOMPARE(i.placements, 2);
    QCOMPARE(i.feeders, 2);
    QCOMPARE(i.components, 0);

    i = ProgramEdits::componentRemovalImpact(p, 2);  // CC-100N: C1, no feeder
    QCOMPARE(i.placements, 1);
    QCOMPARE(i.feeders, 0);

    i = ProgramEdits::packageRemovalImpact(p, 0);
    QCOMPARE(i.components, 2);
    QCOMPARE(i.placements, 3);
    QCOMPARE(i.feeders, 3);

    i = ProgramEdits::boardRemovalImpact(p, 0);
    QCOMPARE(i.placements, 4);
    QCOMPARE(i.instances, 2);

    Program unused = p;
    unused.components.append({QStringLiteral("SPARE"), QStringLiteral("C0402"), {}, {}});
    QVERIFY(ProgramEdits::componentRemovalImpact(unused, 3).isEmpty());
}

void TstProgramEdits::stackUndoRedo()
{
    CommandStack s;
    QVERIFY(!s.canUndo());
    QVERIFY(!s.canRedo());
    s.push(step(QStringLiteral("a"), 1));
    s.push(step(QStringLiteral("b"), 2));
    QCOMPARE(s.undoText(), QStringLiteral("b"));
    QCOMPARE(s.undo().after.panel.width, 2.0);
    QCOMPARE(s.undoText(), QStringLiteral("a"));
    QCOMPARE(s.redoText(), QStringLiteral("b"));
    QCOMPARE(s.redo().after.panel.width, 2.0);
    QVERIFY(!s.canRedo());

    // A new step after undo discards the redo branch.
    s.undo();
    s.push(step(QStringLiteral("c"), 3));
    QVERIFY(!s.canRedo());
    QCOMPARE(s.count(), 2);
    QCOMPARE(s.undoText(), QStringLiteral("c"));
}

void TstProgramEdits::stackClean()
{
    CommandStack s;
    QVERIFY(s.isClean());
    s.push(step(QStringLiteral("a"), 1));
    QVERIFY(!s.isClean());
    s.undo();
    QVERIFY(s.isClean());  // back to the opened state
    s.redo();
    s.setClean();          // saved after "a"
    s.push(step(QStringLiteral("b"), 2));
    s.undo();
    QVERIFY(s.isClean());
    s.undo();
    QVERIFY(!s.isClean());

    // Saved state undone and then replaced: it cannot be reached again.
    s.push(step(QStringLiteral("c"), 3));
    QVERIFY(!s.isClean());
    s.undo();
    QVERIFY(!s.isClean());

    s.clear();
    QVERIFY(s.isClean());
    QVERIFY(!s.canUndo());
}

void TstProgramEdits::stackLimit()
{
    CommandStack s(3);
    for (int i = 1; i <= 5; ++i)
        s.push(step(QString::number(i), i));
    QCOMPARE(s.count(), 3);
    QCOMPARE(s.undo().text, QStringLiteral("5"));
    QCOMPARE(s.undo().text, QStringLiteral("4"));
    QCOMPARE(s.undo().text, QStringLiteral("3"));
    QVERIFY(!s.canUndo());
    // The opened state was dropped with the oldest steps.
    QVERIFY(!s.isClean());
}

QTEST_GUILESS_MAIN(TstProgramEdits)
#include "tst_programedits.moc"
