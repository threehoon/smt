#include "document/programdocument.h"

#include "io/programreader.h"

#include <QGuiApplication>
#include <QList>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QtTest>

namespace {

const QString kSample = QStringLiteral(SMTPRO_SAMPLES_DIR "/example.smt.xml");

// The real main.qml from the compiled-in qrc, wired to a ProgramDocument the
// same way main.cpp does it. Collects every QML warning.
struct MainWindow
{
    // Declared before the engine: both must outlive it, the engine can still
    // report warnings while it is destroyed.
    ProgramDocument document;
    QList<QQmlError> warnings;
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;

    MainWindow()
    {
        QObject::connect(&engine, &QQmlEngine::warnings,
                         [this](const QList<QQmlError> &list) { warnings += list; });
        engine.rootContext()->setContextProperty(QStringLiteral("programDocument"), &document);
        engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
        if (engine.rootObjects().size() == 1)
            window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    }

    // Shows a tab of the workspace by its index (生产 = 0, 拼板, 单板, 元件, ...).
    void showTab(int index)
    {
        QObject *tabs = window->findChild<QObject *>(QStringLiteral("workspaceTabs"));
        if (!tabs)
            qFatal("workspaceTabs not found");
        tabs->setProperty("currentIndex", index);
    }

    QString warningText() const
    {
        QStringList messages;
        for (const QQmlError &e : warnings)
            messages << e.toString();
        return messages.join(QLatin1Char('\n'));
    }
};

// Items made by a Repeater or Loader are not QObject children of the page,
// so search the visual tree.
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (!root)
        return nullptr;
    if (root->objectName() == name)
        return root;
    const QList<QQuickItem *> children = root->childItems();
    for (QQuickItem *child : children) {
        if (QQuickItem *found = findItem(child, name))
            return found;
    }
    return nullptr;
}

// QTest::keyClicks has no QWindow overload in Qt 5. Sends each character as
// a key press with its text, which is what TextInput inserts.
void typeText(QWindow *window, const QString &text)
{
    for (const QChar c : text) {
        QTest::sendKeyEvent(QTest::KeyAction::Click, window, Qt::Key_unknown, QString(c),
                            Qt::NoModifier);
    }
}

QQuickItem *findItem(QQuickWindow *window, const QStringList &path)
{
    QQuickItem *item = window->contentItem();
    for (const QString &name : path) {
        item = findItem(item, name);
        if (!item)
            return nullptr;
    }
    return item;
}

}  // namespace

// Fails on any QML warning: unknown types, bad imports, binding errors.
class TstMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void loadsWithoutWarnings();
    void showsOpenedProgram();
    void reportsOpenFailure();
    void everyPageShowsProgram();
    void formEditsCommit();
    void removeAsksWhenReferenced();
    void closeAsksToSave();
    void feederFormFollowsType();
    void cadImportDialog();
};

void TstMainWindow::loadsWithoutWarnings()
{
    MainWindow w;
    QVERIFY(w.window);
    // Let layouts and deferred bindings run.
    QTest::qWait(200);
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
    QCOMPARE(w.window->title(), QStringLiteral("未命名 - 迅贴 SwiftPlace"));
}

void TstMainWindow::showsOpenedProgram()
{
    MainWindow w;
    QVERIFY(w.window);
    QTest::qWait(100);

    // Opened after the window exists, as from the menu, so every table goes
    // through a model reset and creates delegates for real rows.
    QVERIFY(w.document.openFile(kSample));
    QTest::qWait(200);
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));

    QCOMPARE(w.window->title(), QStringLiteral("example.smt.xml - 迅贴 SwiftPlace"));
    // The board table selected its first row and the content tables follow it.
    QCOMPARE(w.document.boardPlacements()->boardIndex(), 0);
    QCOMPARE(w.document.boardFiducials()->boardIndex(), 0);
    QCOMPARE(w.document.boardBadMarks()->boardIndex(), 0);
    QCOMPARE(w.document.boardPlacements()->rowCount(), 6);

    // Reloading resets the selection to the first board again.
    QVERIFY(w.document.openFile(kSample));
    QTest::qWait(100);
    QCOMPARE(w.document.boardPlacements()->boardIndex(), 0);
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
}

void TstMainWindow::reportsOpenFailure()
{
    MainWindow w;
    QVERIFY(w.window);
    QObject *box = w.window->findChild<QObject *>(QStringLiteral("messageBox"));
    QVERIFY(box);
    QCOMPARE(box->property("opened").toBool(), false);

    QTemporaryDir dir;
    QVERIFY(!w.document.openFile(dir.filePath(QStringLiteral("none.smt.xml"))));
    QTRY_COMPARE(box->property("opened").toBool(), true);
    QCOMPARE(box->property("title").toString(), QStringLiteral("打开失败"));
    QVERIFY(box->property("details").toString().contains(QStringLiteral("无法打开文件")));
    QTest::qWait(100);
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
}

void TstMainWindow::everyPageShowsProgram()
{
    MainWindow w;
    QVERIFY(w.window);
    QVERIFY(w.document.openFile(kSample));
    // Each page creates its table rows and forms only when shown.
    for (int tab = 0; tab < 9; ++tab) {
        w.showTab(tab);
        QTest::qWait(50);
    }
    w.document.newProgram();
    for (int tab = 0; tab < 9; ++tab) {
        w.showTab(tab);
        QTest::qWait(50);
    }
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
    QCOMPARE(w.window->title(), QStringLiteral("未命名 - 迅贴 SwiftPlace"));
}

void TstMainWindow::formEditsCommit()
{
    MainWindow w;
    QVERIFY(w.window);
    QVERIFY(w.document.openFile(kSample));
    w.showTab(3);  // 元件
    w.window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(w.window));
    QTest::qWait(100);

    QQuickItem *value = findItem(w.window, {QStringLiteral("componentForm"),
                                            QStringLiteral("field_value")});
    QQuickItem *description = findItem(w.window, {QStringLiteral("componentForm"),
                                                  QStringLiteral("field_description")});
    QVERIFY(value);
    QVERIFY(description);
    // The table selected its first row; the form shows it.
    QCOMPARE(value->property("text").toString(), QStringLiteral("100nF"));

    // Typing and Enter commits.
    value->forceActiveFocus();
    QTRY_VERIFY(value->hasActiveFocus());
    QTest::keyClick(w.window, Qt::Key_A, Qt::ControlModifier);
    typeText(w.window, QStringLiteral("220nF"));
    QTest::keyClick(w.window, Qt::Key_Return);
    QCOMPARE(w.document.program().components[0].value, QStringLiteral("220nF"));
    QCOMPARE(w.window->title(), QStringLiteral("example.smt.xml * - 迅贴 SwiftPlace"));

    // Leaving the field commits too, to the row it was typed for.
    description->forceActiveFocus();
    QTRY_VERIFY(description->hasActiveFocus());
    QTest::keyClick(w.window, Qt::Key_A, Qt::ControlModifier);
    typeText(w.window, QStringLiteral("X5R"));
    QQuickItem *table = findItem(w.window, {QStringLiteral("componentTable")});
    QVERIFY(table);
    table->forceActiveFocus();
    table->setProperty("currentIndex", 1);
    QCOMPARE(w.document.program().components[0].description, QStringLiteral("X5R"));
    QCOMPARE(w.document.program().components[1].description, QStringLiteral("1%"));
    QTRY_COMPARE(description->property("text").toString(), QStringLiteral("1%"));

    // A refused value is reported and the field shows the model again.
    QObject *box = w.window->findChild<QObject *>(QStringLiteral("messageBox"));
    QQuickItem *id = findItem(w.window, {QStringLiteral("componentForm"),
                                         QStringLiteral("field_id")});
    QVERIFY(id);
    id->forceActiveFocus();
    QTRY_VERIFY(id->hasActiveFocus());
    QTest::keyClick(w.window, Qt::Key_A, Qt::ControlModifier);
    typeText(w.window, QStringLiteral("CC0402-100NF"));  // row 0's id
    QTest::keyClick(w.window, Qt::Key_Return);
    QTRY_COMPARE(box->property("opened").toBool(), true);
    QCOMPARE(box->property("title").toString(), QStringLiteral("修改未生效"));
    QCOMPARE(id->property("text").toString(), QStringLiteral("RC0603-10K"));
    QCOMPARE(w.document.program().components[1].id, QStringLiteral("RC0603-10K"));
    QMetaObject::invokeMethod(box, "close");

    w.document.undo();
    QTRY_COMPARE(w.document.program().components[0].description, QStringLiteral("X7R 16V"));
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
}

void TstMainWindow::removeAsksWhenReferenced()
{
    MainWindow w;
    QVERIFY(w.window);
    QVERIFY(w.document.openFile(kSample));
    w.showTab(3);
    QTest::qWait(100);
    QQuickItem *table = findItem(w.window, {QStringLiteral("componentTable")});
    QQuickItem *remove = findItem(w.window, {QStringLiteral("removeComponent")});
    QVERIFY(table);
    QVERIFY(remove);
    QObject *confirm = remove->findChild<QObject *>(QStringLiteral("removeConfirm"));
    QVERIFY(confirm);

    table->setProperty("currentIndex", 1);  // RC0603-10K: 3 placements, 2 feeders
    QMetaObject::invokeMethod(remove, "clicked");
    QTRY_COMPARE(confirm->property("opened").toBool(), true);
    QVERIFY(confirm->property("text").toString().contains(QStringLiteral("3 个贴片位")));
    QCOMPARE(w.document.components()->rowCount(), 3);  // nothing yet
    QMetaObject::invokeMethod(confirm, "accept");
    QCOMPARE(w.document.components()->rowCount(), 2);
    QCOMPARE(w.document.jobPlacements()->rowCount(), 6);
    QCOMPARE(table->property("currentIndex").toInt(), 1);

    // A component nothing uses goes without asking.
    QQuickItem *add = findItem(w.window, {QStringLiteral("addComponent")});
    QVERIFY(add);
    QMetaObject::invokeMethod(add, "clicked");
    QCOMPARE(w.document.components()->rowCount(), 3);
    QCOMPARE(table->property("currentIndex").toInt(), 2);
    QTRY_COMPARE(confirm->property("opened").toBool(), false);
    QMetaObject::invokeMethod(remove, "clicked");
    QCOMPARE(w.document.components()->rowCount(), 2);
    QCOMPARE(confirm->property("opened").toBool(), false);
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
}

void TstMainWindow::closeAsksToSave()
{
    MainWindow w;
    QVERIFY(w.window);
    QVERIFY(w.document.openFile(kSample));
    QObject *prompt = w.window->findChild<QObject *>(QStringLiteral("savePrompt"));
    QVERIFY(prompt);

    // Unmodified: closes without asking.
    w.window->close();
    QTRY_VERIFY(!w.window->isVisible());
    w.window->show();

    QVERIFY(w.document.components()->set(0, QStringLiteral("value"), QStringLiteral("1uF")));
    w.window->close();
    QTRY_COMPARE(prompt->property("opened").toBool(), true);
    QVERIFY(w.window->isVisible());

    // Cancel keeps the window and the change.
    QMetaObject::invokeMethod(prompt, "reject");
    QTRY_COMPARE(prompt->property("opened").toBool(), false);
    QVERIFY(w.window->isVisible());
    QVERIFY(w.document.isModified());

    // Don't save: closes, the file is untouched.
    w.window->close();
    QTRY_COMPARE(prompt->property("opened").toBool(), true);
    QQuickItem *discard = findItem(w.window, {QStringLiteral("discardButton")});
    QVERIFY(discard);
    QMetaObject::invokeMethod(discard, "clicked");
    QTRY_VERIFY(!w.window->isVisible());
    QCOMPARE(ProgramReader::readFile(kSample).program.components[0].value,
             QStringLiteral("100nF"));
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
}

void TstMainWindow::feederFormFollowsType()
{
    MainWindow w;
    QVERIFY(w.window);
    QVERIFY(w.document.openFile(kSample));
    w.showTab(5);  // 送料器
    QTest::qWait(100);
    QQuickItem *page = findItem(w.window, {QStringLiteral("feedersPage")});
    QQuickItem *table = findItem(w.window, {QStringLiteral("feederTable")});
    QVERIFY(page);
    QVERIFY(table);
    // The form row is the editor's parent RowLayout (through the Loader).
    auto shown = [&](const char *role) {
        QQuickItem *field = findItem(page, QStringLiteral("field_%1").arg(QLatin1String(role)));
        return field && field->parentItem() && field->parentItem()->parentItem()
               && field->parentItem()->parentItem()->isVisible();
    };
    QTRY_VERIFY(shown("tapeWidth"));  // FD1: tape
    QVERIFY(!shown("trayRows"));
    table->setProperty("currentIndex", 3);  // FD4: tray
    QTRY_VERIFY(shown("trayRows"));
    QVERIFY(!shown("tapeWidth"));
    QVERIFY(!shown("tubePitch"));
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
}

void TstMainWindow::cadImportDialog()
{
    MainWindow w;
    QVERIFY(w.window);
    QVERIFY(w.document.openFile(kSample));
    w.showTab(2);  // 单板
    QTest::qWait(100);
    QQuickItem *boardButton = findItem(w.window, {QStringLiteral("boardCadImportButton")});
    QVERIFY(boardButton);
    QVERIFY(boardButton->isEnabled());

    QObject *dialog = w.window->findChild<QObject *>(QStringLiteral("cadImportDialog"));
    QObject *box = w.window->findChild<QObject *>(QStringLiteral("messageBox"));
    QVERIFY(dialog);
    QVERIFY(box);
    QCOMPARE(dialog->property("opened").toBool(), false);

    // Loading a file opens the dialog (the file dialog in front of it is native).
    CadImporter *imp = w.document.cadImport();
    const QString text = QStringLiteral("# Ref Val Package PosX PosY Rot Side\n"
                                        "C1 100nF C0402 20.5 24.0 90 top\n"
                                        "C10 100nF C0402 30.0 30.0 0 top\n"
                                        "U9 LM358 SOIC8 40.0 10.0 0 bottom\n");
    QVERIFY(imp->openText(text, QStringLiteral("ctrl.pos")));
    QTRY_COMPARE(dialog->property("opened").toBool(), true);
    QCOMPARE(dialog->property("title").toString(), QStringLiteral("导入贴片坐标 - ctrl.pos"));
    QTest::qWait(100);
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));

    QQuickItem *summary = findItem(w.window, {QStringLiteral("cadSummary")});
    QVERIFY(summary);
    QVERIFY(summary->property("text").toString().contains(QStringLiteral("新增 2 个")));
    QQuickItem *xColumn = findItem(w.window, {QStringLiteral("column_1")});
    QVERIFY(xColumn);
    QCOMPARE(xColumn->property("currentIndex").toInt(), 4);  // "4. PosX"

    // The part form follows the part table and edits the choice.
    QQuickItem *tabs = findItem(w.window, {QStringLiteral("cadTabs")});
    QVERIFY(tabs);
    tabs->setProperty("currentIndex", 1);
    QQuickItem *partTable = findItem(w.window, {QStringLiteral("cadPartTable")});
    QQuickItem *form = findItem(w.window, {QStringLiteral("cadPartForm")});
    QVERIFY(partTable);
    QVERIFY(form);
    QCOMPARE(partTable->property("currentIndex").toInt(), 0);
    partTable->setProperty("currentIndex", 1);  // SOIC8 / LM358, new
    QQuickItem *componentField = findItem(form, QStringLiteral("field_componentIndex"));
    QVERIFY(componentField);
    QTRY_COMPARE(componentField->property("currentIndex").toInt(), 0);
    QQuickItem *packageField = findItem(form, QStringLiteral("field_packageIndex"));
    QVERIFY(packageField);
    QTRY_VERIFY(packageField->parentItem()->parentItem()->isVisible());
    const int mcu = imp->componentChoices().indexOf(QStringLiteral("STM32G031K8U6"));
    imp->partModel()->set(1, QStringLiteral("componentIndex"), mcu);
    QTRY_COMPARE(componentField->property("currentIndex").toInt(), mcu);
    QTRY_VERIFY(!packageField->parentItem()->parentItem()->isVisible());
    QCOMPARE(partTable->property("currentIndex").toInt(), 1);

    // Replace asks first; then one import closes the dialog and reports.
    imp->setReplace(true);
    QQuickItem *importButton = findItem(w.window, {QStringLiteral("cadImportButton")});
    QVERIFY(importButton);
    QVERIFY(importButton->isEnabled());
    QObject *confirm = w.window->findChild<QObject *>(QStringLiteral("cadReplaceConfirm"));
    QVERIFY(confirm);
    QMetaObject::invokeMethod(importButton, "clicked");
    QTRY_COMPARE(confirm->property("opened").toBool(), true);
    QVERIFY(confirm->property("text").toString().contains(QStringLiteral("5 个贴片位")));
    QCOMPARE(w.document.program().boards.first().placements.size(), 6);  // not yet
    QVERIFY(!w.document.isModified());
    QMetaObject::invokeMethod(confirm, "accept");

    QTRY_COMPARE(dialog->property("opened").toBool(), false);
    QVERIFY(!imp->isLoaded());
    QCOMPARE(w.document.program().boards.first().placements.size(), 3);
    QVERIFY(w.document.isModified());
    QTRY_COMPARE(box->property("opened").toBool(), true);
    QCOMPARE(box->property("title").toString(), QStringLiteral("导入完成"));
    QVERIFY(box->property("text").toString().contains(QStringLiteral("删除 5 个")));

    // Cancel forgets the file.
    QVERIFY(imp->openText(text, QStringLiteral("ctrl.pos")));
    QTRY_COMPARE(dialog->property("opened").toBool(), true);
    QMetaObject::invokeMethod(dialog, "reject");
    QTRY_VERIFY(!imp->isLoaded());
    QTest::qWait(100);
    QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
}

int main(int argc, char *argv[])
{
    // Headless by default so the test runs without a desktop session.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    // The offscreen platform does not use the Windows font database.
    if (!qEnvironmentVariableIsSet("QT_QPA_FONTDIR"))
        qputenv("QT_QPA_FONTDIR", qgetenv("WINDIR") + "/Fonts");
    QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Software);

    QGuiApplication app(argc, argv);
    // closeAsksToSave closes the window; the test goes on.
    app.setQuitOnLastWindowClosed(false);
    TstMainWindow tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_mainwindow.moc"
