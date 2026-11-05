#include "document/programdocument.h"

#include "io/programreader.h"
#include "io/programwriter.h"
#include "validation/programvalidator.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

QString formatDiagnostics(const QVector<Diagnostic> &list)
{
    QStringList lines;
    for (const Diagnostic &d : list) {
        lines << (d.line > 0 ? ProgramDocument::tr("第 %1 行：%2").arg(d.line).arg(d.message)
                             : d.message);
    }
    return lines.join(QLatin1Char('\n'));
}

QSet<QString> warningSet(const QVector<Diagnostic> &list)
{
    QSet<QString> set;
    for (const Diagnostic &d : list) {
        if (d.severity == Diagnostic::Severity::Warning)
            set.insert(d.message);
    }
    return set;
}

// ISO 8601 with the local UTC offset, e.g. 2026-11-06T10:00:00+08:00.
QString now()
{
    const QDateTime local = QDateTime::currentDateTime();
    return local.toOffsetFromUtc(local.offsetFromUtc()).toString(Qt::ISODate);
}

}  // namespace

ProgramDocument::ProgramDocument(QObject *parent)
    : QObject(parent),
      m_packages(new PackageListModel(this, this)),
      m_components(new ComponentListModel(this, this)),
      m_boards(new BoardListModel(this, this)),
      m_boardPlacements(new PlacementListModel(this, this)),
      m_boardFiducials(new FiducialListModel(this, FiducialListModel::Scope::Board, this)),
      m_boardBadMarks(new BadMarkListModel(this, this)),
      m_panelFiducials(new FiducialListModel(this, FiducialListModel::Scope::Panel, this)),
      m_boardInstances(new BoardInstanceListModel(this, this)),
      m_jobPlacements(new JobPlacementListModel(this, this)),
      m_feeders(new FeederListModel(this, this)),
      m_cadImport(new CadImporter(this, this))
{
}

QString ProgramDocument::displayName() const
{
    return m_filePath.isEmpty() ? QString() : QFileInfo(m_filePath).fileName();
}

QString ProgramDocument::machineModel() const
{
    return m_program.feederSetup ? m_program.feederSetup->machineModel : QString();
}

void ProgramDocument::newProgram()
{
    Program p = ProgramEdits::blankProgram();
    p.info.name = tr("新程序");
    p.info.createdAt = now();
    p.info.modifiedAt = p.info.createdAt;
    replaceProgram(std::move(p), {}, false);
    emit statusMessage(tr("已新建程序"));
}

bool ProgramDocument::open(const QUrl &url)
{
    if (!url.isLocalFile()) {
        emit messageRaised(tr("打开失败"), tr("只能打开本地文件：%1").arg(url.toString()), {});
        return false;
    }
    return openFile(url.toLocalFile());
}

bool ProgramDocument::save()
{
    if (!m_loaded || m_filePath.isEmpty())
        return false;
    if (m_saveAsRequired) {
        emit messageRaised(tr("不能直接保存"),
                           tr("此文件由更新版本的软件创建，覆盖保存会丢失本版本不认识的内容。"
                              "请使用“另存为”。"),
                           {});
        return false;
    }
    return saveFile(m_filePath);
}

bool ProgramDocument::saveAs(const QUrl &url)
{
    if (!url.isLocalFile()) {
        emit messageRaised(tr("保存失败"), tr("只能保存到本地文件：%1").arg(url.toString()), {});
        return false;
    }
    return saveFile(url.toLocalFile());
}

bool ProgramDocument::openFile(const QString &path)
{
    ReadResult result = ProgramReader::readFile(path);
    const QString nativePath = QDir::toNativeSeparators(path);
    if (!result.ok()) {
        emit messageRaised(tr("打开失败"),
                           tr("无法打开 %1，发现 %2 个错误。").arg(nativePath).arg(result.errors().size()),
                           formatDiagnostics(result.diagnostics));
        return false;
    }

    replaceProgram(std::move(result.program), QFileInfo(path).absoluteFilePath(),
                   result.newerMinorVersion);

    if (result.newerMinorVersion) {
        emit messageRaised(tr("文件由更新版本创建"),
                           tr("%1 由更新版本的软件创建，本版本不认识的内容已被忽略。"
                              "直接保存会丢失这些内容，请用“另存为”保存到新文件。")
                               .arg(nativePath),
                           formatDiagnostics(result.warnings()));
    } else if (!result.warnings().isEmpty()) {
        emit messageRaised(tr("打开时有警告"),
                           tr("%1 已打开，但有 %2 条警告。")
                               .arg(nativePath)
                               .arg(result.warnings().size()),
                           formatDiagnostics(result.warnings()));
    }
    return true;
}

bool ProgramDocument::saveFile(const QString &path)
{
    if (!m_loaded)
        return false;
    // edit() keeps the program valid, so this is only a guard: never write a
    // file the reader would refuse.
    const QVector<Diagnostic> diagnostics = ProgramValidator::validate(m_program);
    if (hasErrors(diagnostics)) {
        emit messageRaised(tr("保存失败"), tr("程序有错误，保存后将无法再打开。"),
                           formatDiagnostics(filterSeverity(diagnostics, Diagnostic::Severity::Error)));
        return false;
    }

    Program out = m_program;
    // Only content changes count; saving an unchanged file keeps its time.
    if (isModified())
        out.info.modifiedAt = now();
    QString error;
    if (!ProgramWriter::writeFile(out, path, &error)) {
        emit messageRaised(tr("保存失败"), error, {});
        return false;
    }
    // File metadata, not an edit: no undo step.
    m_program.info.modifiedAt = out.info.modifiedAt;
    m_undo.setClean();
    setFilePath(QFileInfo(path).absoluteFilePath());
    // What is on disk now is exactly what this version wrote.
    setSaveAsRequired(false);
    emit undoStateChanged();
    emit statusMessage(tr("已保存 %1").arg(displayName()));
    return true;
}

bool ProgramDocument::edit(const QString &text,
                           const std::function<void(Program &, QVector<IdRename> &)> &change)
{
    if (!m_loaded)
        return false;
    Program candidate = m_program;
    QVector<IdRename> renames;
    change(candidate, renames);
    if (candidate == m_program)
        return true;

    // m_program is valid, so every error comes from this change.
    const QVector<Diagnostic> diagnostics = ProgramValidator::validate(candidate);
    const QVector<Diagnostic> errors = filterSeverity(diagnostics, Diagnostic::Severity::Error);
    if (!errors.isEmpty()) {
        reject(tr("修改未生效"), errors.first().message,
               errors.size() > 1 ? formatDiagnostics(errors) : QString());
        return false;
    }

    renames.erase(std::remove_if(renames.begin(), renames.end(),
                                 [](const IdRename &r) { return r.from == r.to; }),
                  renames.end());
    EditStep step;
    step.text = text;
    step.before = m_program;
    step.after = candidate;
    step.renames = renames;
    m_undo.push(std::move(step));

    // A warning is no reason to refuse (a part may overhang the board edge),
    // but the user should notice one this edit introduced.
    const QSet<QString> warnings = warningSet(diagnostics);
    const QSet<QString> added = QSet<QString>(warnings).subtract(m_warnings);
    applyProgram(candidate, renames);
    m_warnings = warnings;
    if (!added.isEmpty())
        emit statusMessage(tr("警告：%1").arg(*added.cbegin()));
    return true;
}

void ProgramDocument::undo()
{
    if (!m_undo.canUndo())
        return;
    const EditStep &step = m_undo.undo();
    const Program before = step.before;
    QVector<IdRename> renames;
    for (auto it = step.renames.crbegin(); it != step.renames.crend(); ++it)
        renames.append({it->scope, it->board, it->to, it->from});
    applyProgram(before, renames);
    m_warnings = warningSet(ProgramValidator::validate(m_program));
}

void ProgramDocument::redo()
{
    if (!m_undo.canRedo())
        return;
    const EditStep &step = m_undo.redo();
    const Program after = step.after;
    const QVector<IdRename> renames = step.renames;
    applyProgram(after, renames);
    m_warnings = warningSet(ProgramValidator::validate(m_program));
}

void ProgramDocument::reject(const QString &title, const QString &text, const QString &details)
{
    emit messageRaised(title, text, details);
}

bool ProgramDocument::parseNumber(const QVariant &value, double *result)
{
    bool ok = false;
    double v = 0.0;
    if (value.userType() == QMetaType::QString)
        v = value.toString().trimmed().toDouble(&ok);
    else
        v = value.toDouble(&ok);
    if (!ok || !std::isfinite(v)) {
        reject(tr("输入无效"), tr("“%1”不是有效数值").arg(value.toString()));
        return false;
    }
    *result = v;
    return true;
}

bool ProgramDocument::parseInteger(const QVariant &value, int *result)
{
    bool ok = false;
    int v = 0;
    if (value.userType() == QMetaType::QString) {
        v = value.toString().trimmed().toInt(&ok);
    } else {
        // QML passes numbers as double.
        const double d = value.toDouble(&ok);
        ok = ok && std::isfinite(d) && d == std::floor(d)
             && std::abs(d) <= double(std::numeric_limits<int>::max());
        v = ok ? int(d) : 0;
    }
    if (!ok) {
        reject(tr("输入无效"), tr("“%1”不是整数").arg(value.toString()));
        return false;
    }
    *result = v;
    return true;
}

bool ProgramDocument::setPanelWidth(const QVariant &value)
{
    double v = 0.0;
    return parseNumber(value, &v)
        && edit(tr("修改拼板尺寸"), [&](Program &p, QVector<IdRename> &) { p.panel.width = v; });
}

bool ProgramDocument::setPanelHeight(const QVariant &value)
{
    double v = 0.0;
    return parseNumber(value, &v)
        && edit(tr("修改拼板尺寸"), [&](Program &p, QVector<IdRename> &) { p.panel.height = v; });
}

bool ProgramDocument::setMachineModel(const QString &model)
{
    const QString m = model.trimmed();
    return edit(tr("修改机型"), [&](Program &p, QVector<IdRename> &) {
        if (!p.feederSetup) {
            if (m.isEmpty())
                return;
            p.feederSetup = FeederSetup();
        }
        p.feederSetup->machineModel = m;
    });
}

void ProgramDocument::replaceProgram(Program program, const QString &path, bool saveAsRequired)
{
    emit programAboutToChange();
    m_program = std::move(program);
    m_loaded = true;
    m_undo.clear();
    m_warnings = warningSet(ProgramValidator::validate(m_program));
    emit programChanged();
    emit contentChanged();
    emit undoStateChanged();
    setFilePath(path);
    setSaveAsRequired(saveAsRequired);
}

void ProgramDocument::applyProgram(const Program &program, const QVector<IdRename> &renames)
{
    // modifiedAt follows the file, not the undo history.
    const QString modifiedAt = m_program.info.modifiedAt;
    m_program = program;
    m_program.info.modifiedAt = modifiedAt;
    emit programEdited(renames);
    emit contentChanged();
    emit undoStateChanged();
}

void ProgramDocument::setFilePath(const QString &path)
{
    if (path == m_filePath)
        return;
    m_filePath = path;
    emit filePathChanged();
}

void ProgramDocument::setSaveAsRequired(bool required)
{
    if (required == m_saveAsRequired)
        return;
    m_saveAsRequired = required;
    emit saveAsRequiredChanged();
}
