#include "document/programdocument.h"

#include "io/programreader.h"
#include "validation/programvalidator.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

const QString kSample = QStringLiteral(SMTPRO_SAMPLES_DIR "/example.smt.xml");

// Board CTRL of the sample has U1, C1, C2, R1, R2, R5. C1 is unchanged, R1
// moves, C10 and U9 are new; U9's part does not exist yet.
const char kKicad[] = "## Unit = mm, Angle = deg.\n"
                      "# Ref Val Package PosX PosY Rot Side\n"
                      "C1 100nF C0402 20.5 24.0 90 top\n"
                      "R1 10K R0603 13.0 30.0 0 top\n"
                      "C10 100nF C0402 30.0 30.0 0 top\n"
                      "U9 LM358 SOIC8 40.0 10.0 0 bottom\n";

int find(const QStringList &list, const QString &text)
{
    return list.indexOf(text);
}

QString role(const QAbstractItemModel *m, int row, const char *name)
{
    const QHash<int, QByteArray> names = m->roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it) {
        if (it.value() == name)
            return m->data(m->index(row, 0), it.key()).toString();
    }
    qFatal("unknown role %s", name);
    return {};
}

struct Fixture
{
    ProgramDocument doc;
    CadImporter *imp = doc.cadImport();
    QSignalSpy messages{&doc, &ProgramDocument::messageRaised};
    QSignalSpy imported{imp, &CadImporter::imported};
    std::unique_ptr<QAbstractItemModelTester> rowTester{new QAbstractItemModelTester(
        imp->rowModel(), QAbstractItemModelTester::FailureReportingMode::QtTest)};
    std::unique_ptr<QAbstractItemModelTester> partTester{new QAbstractItemModelTester(
        imp->partModel(), QAbstractItemModelTester::FailureReportingMode::QtTest)};

    Fixture()
    {
        if (!doc.openFile(kSample))
            qFatal("sample does not open");
    }
};

}  // namespace

class TstCadImporter : public QObject
{
    Q_OBJECT

private slots:
    void opensFile();
    void openFailures();
    void preview();
    void partChoiceKeepsRows();
    void mappingAndOptions();
    void origin();
    void importIsOneUndoStep();
    void replaceRemoves();
    void followsProgramEdits();
    void request();
    void importCreatesProgram();
};

void TstCadImporter::opensFile()
{
    Fixture f;
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("ctrl-top.pos"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(kKicad);
    file.close();

    QSignalSpy changed(f.imp, &CadImporter::changed);
    QVERIFY(f.imp->open(QUrl::fromLocalFile(path)));
    QVERIFY(f.imp->isLoaded());
    QVERIFY(changed.count() > 0);
    QCOMPARE(f.imp->fileName(), QStringLiteral("ctrl-top.pos"));
    QCOMPARE(f.imp->formatName(), QStringLiteral("KiCad"));
    QCOMPARE(f.imp->delimiterChoices().first(), QStringLiteral("自动（空格）"));
    QCOMPARE(f.imp->unitChoices().first(), QStringLiteral("自动（mm）"));
    QCOMPARE(f.imp->headerLine(), 2);
    QCOMPARE(f.imp->lineCount(), 7);
    QCOMPARE(f.imp->rowCount(), 4);
    QCOMPARE(f.imp->rowModel()->rowCount(), 4);
    QCOMPARE(f.imp->boardChoices(), QStringList{QStringLiteral("CTRL")});
    // Column choice indices: 0 = none, n = column n - 1.
    QCOMPARE(f.imp->fieldColumns(), (QVariantList{1, 4, 5, 6, 7, 3, 2}));
    QCOMPARE(f.imp->columnChoices().at(4), QStringLiteral("4. PosX"));

    f.imp->close();
    QVERIFY(!f.imp->isLoaded());
    QVERIFY(!f.imp->canImport());
    QVERIFY(f.imp->summaryText().isEmpty());
    // Program edits no longer recompute anything.
    QVERIFY(f.doc.components()->remove(0));
    QCOMPARE(f.messages.count(), 0);
}

void TstCadImporter::openFailures()
{
    Fixture f;
    QTemporaryDir dir;
    QVERIFY(!f.imp->openFile(dir.filePath(QStringLiteral("none.pos"))));
    QCOMPARE(f.messages.count(), 1);
    QCOMPARE(f.messages.last().at(0).toString(), QStringLiteral("无法导入"));
    QVERIFY(!f.imp->isLoaded());

    QVERIFY(!f.imp->openText(QStringLiteral("# only a comment\n\n"), QStringLiteral("empty.pos")));
    QCOMPARE(f.messages.count(), 2);
    QVERIFY(f.messages.last().at(1).toString().contains(QStringLiteral("没有数据行")));

    QVERIFY(!f.imp->open(QUrl(QStringLiteral("https://example.com/a.pos"))));
    QCOMPARE(f.messages.count(), 3);

    // A failed open keeps the file already loaded.
    QVERIFY(f.imp->openText(QString::fromUtf8(kKicad), QStringLiteral("a.pos")));
    QVERIFY(!f.imp->openText(QString(), QStringLiteral("b.pos")));
    QCOMPARE(f.imp->fileName(), QStringLiteral("a.pos"));
    QCOMPARE(f.imp->rowCount(), 4);
}

void TstCadImporter::preview()
{
    Fixture f;
    QVERIFY(f.imp->openText(QString::fromUtf8(kKicad), QStringLiteral("a.pos")));
    QVERIFY(f.imp->canImport());
    const CadImportSummary &s = f.imp->summary();
    QCOMPARE(s.unchanged, 1);
    QCOMPARE(s.updated, 1);
    QCOMPARE(s.added, 2);
    QCOMPARE(s.newComponents, QStringList{QStringLiteral("SOIC8-LM358")});
    QCOMPARE(s.newPackages, QStringList{QStringLiteral("SOIC8")});
    QVERIFY(f.imp->summaryText().contains(QStringLiteral("新增 2 个、更新 1 个、不变 1 个贴片位")));
    QVERIFY(f.imp->summaryText().contains(QStringLiteral("新建 1 个元件、1 个封装")));

    const QAbstractItemModel *rows = f.imp->rowModel();
    QCOMPARE(role(rows, 0, "status"), QStringLiteral("不变"));
    QCOMPARE(role(rows, 1, "status"), QStringLiteral("更新"));
    QCOMPARE(role(rows, 2, "status"), QStringLiteral("新增"));
    QCOMPARE(role(rows, 2, "component"), QStringLiteral("CC0402-100NF"));
    QCOMPARE(role(rows, 3, "side"), QStringLiteral("底面"));
    QCOMPARE(role(rows, 3, "component"), QStringLiteral("SOIC8-LM358"));

    const QAbstractItemModel *parts = f.imp->partModel();
    QCOMPARE(parts->rowCount(), 3);
    QCOMPARE(role(parts, 0, "component"), QStringLiteral("CC0402-100NF"));
    QCOMPARE(role(parts, 0, "designators"), QStringLiteral("C1, C10"));
    QCOMPARE(role(parts, 2, "component"), QStringLiteral("新建 SOIC8-LM358"));
    QCOMPARE(role(parts, 2, "package"), QStringLiteral("新建 SOIC8"));

    // The preview does not touch the program.
    QVERIFY(!f.doc.isModified());
    QCOMPARE(f.doc.program().components.size(), 3);
}

void TstCadImporter::partChoiceKeepsRows()
{
    Fixture f;
    QVERIFY(f.imp->openText(QString::fromUtf8(kKicad), QStringLiteral("a.pos")));
    CadPartListModel *parts = f.imp->partModel();
    QSignalSpy reset(parts, &QAbstractItemModel::modelReset);
    QSignalSpy changed(parts, &QAbstractItemModel::dataChanged);

    // U9 uses the existing MCU instead of a new component.
    const int mcu = find(f.imp->componentChoices(), QStringLiteral("STM32G031K8U6"));
    QVERIFY(mcu > 0);
    QVERIFY(parts->set(2, QStringLiteral("componentIndex"), mcu));
    QCOMPARE(reset.count(), 0);
    QVERIFY(changed.count() > 0);
    QCOMPARE(parts->get(2).value(QStringLiteral("componentIndex")).toInt(), mcu);
    QCOMPARE(parts->get(2).value(QStringLiteral("createsComponent")).toBool(), false);
    QVERIFY(f.imp->summary().newComponents.isEmpty());
    QVERIFY(f.imp->summary().newPackages.isEmpty());

    // Back to "create", with an existing package.
    QVERIFY(parts->set(2, QStringLiteral("componentIndex"), 0));
    const int r0603 = find(f.imp->packageChoices(), QStringLiteral("R0603"));
    QVERIFY(parts->set(2, QStringLiteral("packageIndex"), r0603));
    QCOMPARE(parts->get(2).value(QStringLiteral("packageIndex")).toInt(), r0603);
    QCOMPARE(f.imp->summary().newComponents, QStringList{QStringLiteral("SOIC8-LM358")});
    QVERIFY(f.imp->summary().newPackages.isEmpty());
    QCOMPARE(role(parts, 2, "package"), QStringLiteral("R0603"));
    QCOMPARE(reset.count(), 0);

    // Read-only roles are refused.
    QVERIFY(!parts->set(2, QStringLiteral("footprint"), QStringLiteral("X")));

    // A choice survives a recompute that keeps the part.
    f.imp->setUnitChoice(1);
    QCOMPARE(parts->get(2).value(QStringLiteral("packageIndex")).toInt(), r0603);
}

void TstCadImporter::mappingAndOptions()
{
    Fixture f;
    // No header, semicolons, decimal commas.
    const QString text = QStringLiteral("U1;25,0;20,0\nC1;20,5;24,0\nC9;1,0;1,0\n");
    QVERIFY(f.imp->openText(text, QStringLiteral("plain.txt")));
    QCOMPARE(f.imp->headerLine(), 0);
    QCOMPARE(f.imp->formatName(), QStringLiteral("无表头"));
    QCOMPARE(f.imp->columnChoices().at(1), QStringLiteral("1. 第 1 列（U1）"));
    QVERIFY(!f.imp->canImport());
    QCOMPARE(f.imp->summaryText(), QStringLiteral("请为位号、X、Y 选择所在的列。"));
    // Every row is an error until the columns are chosen.
    QCOMPARE(f.imp->errorCount(), 3);

    f.imp->setFieldColumn(int(CadField::Designator), 1);
    f.imp->setFieldColumn(int(CadField::X), 2);
    f.imp->setFieldColumn(int(CadField::Y), 3);
    QCOMPARE(f.imp->errorCount(), 0);
    QVERIFY(f.imp->canImport());
    QCOMPARE(f.imp->summary().added, 1);
    // No footprint, no value: one new component and package for all rows.
    QCOMPARE(f.imp->partModel()->rowCount(), 1);

    // Unit: mil makes every coordinate 0.0254 times smaller.
    f.imp->setUnitChoice(2);
    QCOMPARE(f.imp->unitChoice(), 2);
    QVERIFY(qAbs(f.imp->placements().first().x - 25.0 * 0.0254) < 1e-9);
    f.imp->setUnitChoice(0);
    QCOMPARE(f.imp->placements().first().x, 25.0);

    // Forcing another delimiter looks for the header again; the mapping is
    // guessed again from the new columns and is incomplete.
    f.imp->setDelimiterChoice(1);
    QCOMPARE(f.imp->delimiterChoice(), 1);
    QVERIFY(!f.imp->canImport());
    f.imp->setDelimiterChoice(0);
    QCOMPARE(f.imp->delimiterChoice(), 0);

    // Header line 1 makes the first row the titles.
    f.imp->setHeaderLine(1);
    QVERIFY(!f.imp->autoHeader());
    QCOMPARE(f.imp->headerLine(), 1);
    QCOMPARE(f.imp->rowCount(), 2);
    QCOMPARE(f.imp->columnChoices().at(1), QStringLiteral("1. U1"));
    f.imp->setAutoHeader(true);
    QCOMPARE(f.imp->headerLine(), 0);
    QCOMPARE(f.imp->rowCount(), 3);
    QCOMPARE(f.messages.count(), 0);
}

void TstCadImporter::origin()
{
    Fixture f;
    QVERIFY(f.imp->openText(QString::fromUtf8(kKicad), QStringLiteral("a.pos")));
    QVERIFY(f.imp->setOriginX(QStringLiteral("10.5")));
    QVERIFY(f.imp->setOriginY(-2));
    QCOMPARE(f.imp->originX(), 10.5);
    QCOMPARE(f.imp->placements().first().x, 20.5 - 10.5);
    QCOMPARE(f.imp->placements().first().y, 24.0 + 2.0);
    QCOMPARE(f.imp->summary().unchanged, 0);

    QVERIFY(!f.imp->setOriginX(QStringLiteral("abc")));
    QCOMPARE(f.messages.count(), 1);
    QCOMPARE(f.imp->originX(), 10.5);
}

void TstCadImporter::importIsOneUndoStep()
{
    Fixture f;
    const Program before = f.doc.program();
    QVERIFY(f.imp->openText(QString::fromUtf8(kKicad), QStringLiteral("a.pos")));
    // The boards page shows board 0; an import must not reset its table.
    f.doc.boardPlacements()->setBoardIndex(0);
    QSignalSpy placementsReset(f.doc.boardPlacements(), &QAbstractItemModel::modelReset);
    QSignalSpy placementsInserted(f.doc.boardPlacements(), &QAbstractItemModel::rowsInserted);

    QVERIFY(f.imp->importPlacements());
    QCOMPARE(f.messages.count(), 0);
    QCOMPARE(f.imported.count(), 1);
    const QString text = f.imported.first().at(1).toString();
    QVERIFY2(text.contains(QStringLiteral("单板 CTRL：新增 2 个、更新 1 个贴片位")), qPrintable(text));
    QVERIFY(text.contains(QStringLiteral("新建元件：SOIC8-LM358")));
    QVERIFY(text.contains(QStringLiteral("新建封装：SOIC8")));
    // The dialog is done with the file.
    QVERIFY(!f.imp->isLoaded());

    QVERIFY(f.doc.isModified());
    QCOMPARE(f.doc.undoText(), QStringLiteral("导入贴片坐标 a.pos"));
    QCOMPARE(placementsReset.count(), 0);
    QCOMPARE(placementsInserted.count(), 1);
    const Board &b = f.doc.program().boards.first();
    QCOMPARE(b.placements.size(), 8);
    QCOMPARE(b.placements.last().id, QStringLiteral("U9"));
    QCOMPARE(b.placements.last().side, Side::Bottom);
    QVERIFY(!hasErrors(ProgramValidator::validate(f.doc.program())));

    f.doc.undo();
    QVERIFY(f.doc.program() == before);
    QVERIFY(!f.doc.isModified());
    f.doc.redo();
    QCOMPARE(f.doc.program().boards.first().placements.size(), 8);
}

void TstCadImporter::replaceRemoves()
{
    Fixture f;
    QVERIFY(f.imp->openText(QString::fromUtf8(kKicad), QStringLiteral("a.pos")));
    f.imp->setReplace(true);
    QVERIFY(f.imp->isReplace());
    QCOMPARE(f.imp->removedCount(), 4);  // U1, C2, R2, R5
    QVERIFY(f.imp->summaryText().contains(QStringLiteral("，删除 4 个")));
    QVERIFY(f.imp->importPlacements());
    const Board &b = f.doc.program().boards.first();
    QCOMPARE(b.placements.size(), 4);
    QCOMPARE(b.placements.first().id, QStringLiteral("C1"));
    // R1 stays, so instance 2 still skips it.
    QCOMPARE(f.doc.program().panel.instances.at(1).skippedPlacements,
             QStringList{QStringLiteral("R1")});

    // Importing the same file again changes nothing and is refused.
    QVERIFY(f.imp->openText(QString::fromUtf8(kKicad), QStringLiteral("a.pos")));
    f.imp->setReplace(true);
    QVERIFY(!f.imp->canImport());
    QVERIFY(f.imp->summaryText().contains(QStringLiteral("无需导入")));
    QVERIFY(!f.imp->importPlacements());
    QCOMPARE(f.messages.count(), 1);
}

void TstCadImporter::followsProgramEdits()
{
    Fixture f;
    QVERIFY(f.imp->openText(QString::fromUtf8(kKicad), QStringLiteral("a.pos")));
    const int mcu = find(f.imp->componentChoices(), QStringLiteral("STM32G031K8U6"));
    f.imp->setPartComponent(2, mcu);
    QCOMPARE(f.imp->parts().at(2).component, QStringLiteral("STM32G031K8U6"));

    // Removing the chosen component falls back to creating one.
    QSignalSpy choices(f.imp, &CadImporter::choicesChanged);
    QVERIFY(f.doc.components()->remove(2));
    QVERIFY(choices.count() > 0);
    QVERIFY(f.imp->parts().at(2).component.isEmpty());
    QCOMPARE(f.imp->componentChoices().indexOf(QStringLiteral("STM32G031K8U6")), -1);

    // A recompute that changes no list does not report the lists again.
    choices.clear();
    f.imp->setUnitChoice(1);
    f.imp->setReplace(true);
    QCOMPARE(choices.count(), 0);

    // A new program: still a valid board.
    f.doc.newProgram();
    QCOMPARE(f.imp->boardIndex(), 0);
    QCOMPARE(f.imp->boardChoices(), QStringList{QStringLiteral("PCB")});
    QCOMPARE(f.imp->summary().added, 4);
}

void TstCadImporter::request()
{
    Fixture f;
    f.doc.boards()->add();
    QSignalSpy requested(f.imp, &CadImporter::requested);
    f.imp->request(1);
    QCOMPARE(requested.count(), 1);
    QCOMPARE(f.imp->boardIndex(), 1);
    // Out of range keeps the board.
    f.imp->request(5);
    QCOMPARE(requested.count(), 2);
    QCOMPARE(f.imp->boardIndex(), 1);
}

void TstCadImporter::importCreatesProgram()
{
    ProgramDocument doc;
    CadImporter *imp = doc.cadImport();
    QSignalSpy messages(&doc, &ProgramDocument::messageRaised);
    QSignalSpy imported(imp, &CadImporter::imported);
    QVERIFY(!doc.isLoaded());

    // With no program open the preview goes into the blank program's board.
    QVERIFY(imp->openText(QString::fromUtf8(kKicad), QStringLiteral("a.pos")));
    QCOMPARE(imp->boardChoices(), QStringList{QStringLiteral("PCB")});
    QCOMPARE(imp->summary().added, 4);
    QVERIFY(imp->canImport());
    QVERIFY(!doc.isLoaded());

    // Importing creates the program; it is unsaved, and undo leaves it blank.
    QVERIFY(imp->importPlacements());
    QVERIFY(doc.isLoaded());
    QVERIFY(doc.filePath().isEmpty());
    QVERIFY(doc.isModified());
    QCOMPARE(messages.count(), 0);
    QCOMPARE(imported.count(), 1);
    const Program &p = doc.program();
    QCOMPARE(p.boards.size(), 1);
    QCOMPARE(p.boards.first().placements.size(), 4);
    QCOMPARE(p.components.size(), 3);
    QVERIFY(!hasErrors(ProgramValidator::validate(p)));

    doc.undo();
    QVERIFY(doc.isLoaded());
    QVERIFY(!doc.isModified());
    QVERIFY(doc.program().boards.first().placements.isEmpty());
}

QTEST_GUILESS_MAIN(TstCadImporter)
#include "tst_cadimporter.moc"
