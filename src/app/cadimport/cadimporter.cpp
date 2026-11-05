#include "cadimport/cadimporter.h"

#include "document/programdocument.h"

#include "edit/programedits.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <iterator>

namespace {

// A placement file is a few hundred KB; anything much bigger is the wrong file.
constexpr qint64 kMaxFileSize = 16 * 1024 * 1024;

const CadTable::Delimiter kDelimiters[] = {
    CadTable::Delimiter::Comma, CadTable::Delimiter::Tab, CadTable::Delimiter::Semicolon,
    CadTable::Delimiter::Whitespace};

const CadUnit kUnits[] = {CadUnit::Millimeter, CadUnit::Mil, CadUnit::Inch};

QString delimiterName(CadTable::Delimiter d)
{
    switch (d) {
    case CadTable::Delimiter::Comma:
        return CadImporter::tr("逗号");
    case CadTable::Delimiter::Tab:
        return CadImporter::tr("制表符");
    case CadTable::Delimiter::Semicolon:
        return CadImporter::tr("分号");
    case CadTable::Delimiter::Whitespace:
        return CadImporter::tr("空格");
    }
    return {};
}

QString unitName(CadUnit u)
{
    switch (u) {
    case CadUnit::Millimeter:
        return QStringLiteral("mm");
    case CadUnit::Mil:
        return QStringLiteral("mil");
    case CadUnit::Inch:
        return QStringLiteral("inch");
    }
    return {};
}

}  // namespace

CadImporter::CadImporter(ProgramDocument *document, QObject *parent)
    : QObject(parent),
      m_document(document),
      m_blank(ProgramEdits::blankProgram()),
      m_rowModel(new CadRowListModel(this, this)),
      m_partModel(new CadPartListModel(this, this))
{
    // The suggestions and the preview depend on the program.
    connect(document, &ProgramDocument::programChanged, this, [this] {
        if (m_loaded)
            regroup();
    });
    connect(document, &ProgramDocument::programEdited, this, [this] {
        if (m_loaded)
            regroup();
    });
}

QString CadImporter::formatName() const
{
    if (!m_loaded)
        return {};
    const QString name = CadColumns::formatName(m_table.titles);
    if (!name.isEmpty())
        return name;
    return m_table.titles.isEmpty() ? tr("无表头") : tr("通用表格");
}

QStringList CadImporter::delimiterChoices() const
{
    QStringList choices{tr("自动（%1）").arg(delimiterName(m_table.delimiter))};
    for (const CadTable::Delimiter d : kDelimiters)
        choices << delimiterName(d);
    return choices;
}

int CadImporter::delimiterChoice() const
{
    if (m_tableOptions.autoDelimiter)
        return 0;
    return int(std::find(std::begin(kDelimiters), std::end(kDelimiters), m_tableOptions.delimiter)
               - std::begin(kDelimiters)) + 1;
}

void CadImporter::setDelimiterChoice(int choice)
{
    if (choice < 0 || choice > int(std::size(kDelimiters)) || choice == delimiterChoice())
        return;
    m_tableOptions.autoDelimiter = choice == 0;
    if (choice > 0)
        m_tableOptions.delimiter = kDelimiters[choice - 1];
    // Another delimiter makes other columns: look for the header again.
    m_tableOptions.autoHeader = true;
    reparse();
}

void CadImporter::setAutoHeader(bool on)
{
    if (on == m_tableOptions.autoHeader)
        return;
    m_tableOptions.autoHeader = on;
    m_tableOptions.headerLine = m_table.headerLine;
    reparse();
}

void CadImporter::setHeaderLine(int line)
{
    line = qBound(0, line, m_lineCount);
    if (!m_tableOptions.autoHeader && line == m_tableOptions.headerLine)
        return;
    m_tableOptions.autoHeader = false;
    m_tableOptions.headerLine = line;
    reparse();
}

QStringList CadImporter::unitChoices() const
{
    QStringList choices{tr("自动（%1）").arg(unitName(m_detectedUnit))};
    for (const CadUnit u : kUnits)
        choices << unitName(u);
    return choices;
}

int CadImporter::unitChoice() const
{
    if (m_autoUnit)
        return 0;
    return int(std::find(std::begin(kUnits), std::end(kUnits), m_extract.unit) - std::begin(kUnits))
           + 1;
}

void CadImporter::setUnitChoice(int choice)
{
    if (choice < 0 || choice > int(std::size(kUnits)) || choice == unitChoice())
        return;
    m_autoUnit = choice == 0;
    m_extract.unit = m_autoUnit ? m_detectedUnit : kUnits[choice - 1];
    reextract();
}

QStringList CadImporter::fieldNames() const
{
    return {tr("位号 *"), tr("X *"), tr("Y *"), tr("角度"), tr("面"), tr("封装"), tr("值")};
}

QStringList CadImporter::columnChoices() const
{
    QStringList choices{tr("（无）")};
    const CadTable::Row *first = m_table.rows.isEmpty() ? nullptr : &m_table.rows.first();
    for (int c = 0; c < m_table.columnCount; ++c) {
        QString text = tr("%1. %2").arg(QString::number(c + 1), m_table.columnName(c));
        // Without a header the column name says nothing; show a value.
        if ((c >= m_table.titles.size() || m_table.titles.at(c).isEmpty()) && first
            && c < first->fields.size())
            text += tr("（%1）").arg(first->fields.at(c).left(20));
        choices << text;
    }
    return choices;
}

QVariantList CadImporter::fieldColumns() const
{
    QVariantList list;
    for (const int c : m_extract.mapping.columns)
        list << c + 1;
    return list;
}

void CadImporter::setFieldColumn(int field, int column)
{
    if (field < 0 || field >= kCadFieldCount || column < 0 || column > m_table.columnCount)
        return;
    if (m_extract.mapping.columns[field] == column - 1)
        return;
    m_extract.mapping.columns[field] = column - 1;
    // The unit may be in the title of the X / Y column.
    m_detectedUnit = CadPlacements::detectUnit(m_table, m_extract.mapping);
    if (m_autoUnit)
        m_extract.unit = m_detectedUnit;
    reextract();
}

bool CadImporter::setOrigin(const QVariant &value, double *target)
{
    double v = 0.0;
    if (!m_document->parseNumber(value, &v))
        return false;
    if (v != *target) {
        *target = v;
        reextract();
    }
    return true;
}

bool CadImporter::setOriginX(const QVariant &value)
{
    return setOrigin(value, &m_extract.originX);
}

bool CadImporter::setOriginY(const QVariant &value)
{
    return setOrigin(value, &m_extract.originY);
}

void CadImporter::setBoardIndex(int index)
{
    if (index == m_board || index < 0 || index >= program().boards.size())
        return;
    m_board = index;
    preview();
}

QStringList CadImporter::boardChoices() const
{
    QStringList ids;
    for (const Board &b : program().boards)
        ids << b.id;
    return ids;
}

void CadImporter::setReplace(bool replace)
{
    const CadImportMode mode = replace ? CadImportMode::Replace : CadImportMode::Merge;
    if (mode == m_mode)
        return;
    m_mode = mode;
    preview();
}

QStringList CadImporter::componentChoices() const
{
    QStringList choices{tr("新建元件")};
    for (const Component &c : program().components)
        choices << c.id;
    return choices;
}

QStringList CadImporter::packageChoices() const
{
    QStringList choices{tr("新建封装")};
    for (const Package &p : program().packages)
        choices << p.id;
    return choices;
}

void CadImporter::setPartComponent(int part, int choice)
{
    if (part < 0 || part >= m_parts.size())
        return;
    const QStringList ids = componentChoices();
    if (choice < 0 || choice >= ids.size())
        return;
    const CadPart &p = m_parts.at(part);
    PartChoice &c = m_choices[{p.footprint, p.value}];
    c.component = choice == 0 ? QString() : ids.at(choice);
    c.package = p.package;
    regroup();
}

void CadImporter::setPartPackage(int part, int choice)
{
    if (part < 0 || part >= m_parts.size())
        return;
    const QStringList ids = packageChoices();
    if (choice < 0 || choice >= ids.size())
        return;
    const CadPart &p = m_parts.at(part);
    PartChoice &c = m_choices[{p.footprint, p.value}];
    c.component = p.component;
    c.package = choice == 0 ? QString() : ids.at(choice);
    regroup();
}

int CadImporter::errorCount() const
{
    return int(std::count_if(m_rows.cbegin(), m_rows.cend(),
                             [](const CadPlacement &p) { return !p.isValid(); }));
}

bool CadImporter::canImport() const
{
    return m_loaded && m_extract.mapping.isComplete() && m_board >= 0
           && m_board < program().boards.size()
           && m_summary.added + m_summary.updated + m_summary.removed > 0;
}

QString CadImporter::summaryText() const
{
    if (!m_loaded)
        return {};
    if (!m_extract.mapping.isComplete())
        return tr("请为位号、X、Y 选择所在的列。");
    if (m_board < 0 || m_board >= program().boards.size())
        return tr("请选择目标单板。");
    const int valid = m_rows.size() - errorCount();
    if (valid == 0)
        return tr("没有可导入的行。");

    const CadImportSummary &s = m_summary;
    QStringList parts;
    QString line = tr("新增 %1 个、更新 %2 个、不变 %3 个贴片位").arg(s.added).arg(s.updated).arg(s.unchanged);
    if (s.removed > 0)
        line += tr("，删除 %1 个").arg(s.removed);
    parts << line + tr("。");
    if (!s.newComponents.isEmpty() || !s.newPackages.isEmpty()) {
        parts << tr("新建 %1 个元件、%2 个封装（封装尺寸为占位值 %3 mm，导入后请在封装页补全）。")
                     .arg(s.newComponents.size())
                     .arg(s.newPackages.size())
                     .arg(CadImport::kNewPackageSize);
    }
    if (errorCount() > 0)
        parts << tr("%1 行有错误，不会导入。").arg(errorCount());
    if (s.outsideBoard > 0)
        parts << tr("%1 个贴片位在板外，请检查原点。").arg(s.outsideBoard);
    if (!canImport())
        parts << tr("单板已与文件一致，无需导入。");
    return parts.join(QLatin1Char('\n'));
}

bool CadImporter::open(const QUrl &url)
{
    if (!url.isLocalFile()) {
        m_document->reject(tr("无法导入"), tr("只能导入本机文件：%1").arg(url.toString()));
        return false;
    }
    return openFile(url.toLocalFile());
}

bool CadImporter::openFile(const QString &path)
{
    QFile file(path);
    const QString name = QFileInfo(path).fileName();
    if (file.size() > kMaxFileSize) {
        m_document->reject(tr("无法导入"), tr("%1 太大（超过 16 MB），不像贴片坐标文件。").arg(name));
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_document->reject(tr("无法导入"),
                           tr("无法打开文件 %1：%2").arg(name, file.errorString()));
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        m_document->reject(tr("无法导入"),
                           tr("读取文件 %1 失败：%2").arg(name, file.errorString()));
        return false;
    }
    if (!openText(CadTableParser::decode(bytes), name))
        return false;
    return true;
}

bool CadImporter::openText(const QString &text, const QString &fileName)
{
    const CadTable table = CadTableParser::parse(text);
    if (table.rows.isEmpty()) {
        m_document->reject(tr("无法导入"), tr("%1 中没有数据行。").arg(fileName));
        return false;
    }

    static const QRegularExpression eol(QStringLiteral("\\r\\n|\\n|\\r"));
    m_loaded = true;
    m_fileName = fileName;
    m_text = text;
    m_lineCount = text.split(eol).size();
    m_tableOptions = CadTableOptions();
    m_autoUnit = true;
    m_extract = CadExtractOptions();
    m_choices.clear();
    m_mode = CadImportMode::Merge;
    reparse();
    return true;
}

void CadImporter::close()
{
    if (!m_loaded)
        return;
    // The rows stay in the preview models until the next file: emptying them
    // now would reset the tables of the dialog while it closes, and a
    // ListView with a row still being created reports a stale index then.
    m_loaded = false;
    m_text.clear();
    m_choices.clear();
    emitChanged();
}

void CadImporter::reparse()
{
    m_table = CadTableParser::parse(m_text, m_tableOptions);
    remap();
}

void CadImporter::remap()
{
    m_extract.mapping = CadColumns::guess(m_table.titles);
    m_detectedUnit = CadPlacements::detectUnit(m_table, m_extract.mapping);
    if (m_autoUnit)
        m_extract.unit = m_detectedUnit;
    reextract();
}

void CadImporter::reextract()
{
    m_rows = CadPlacements::extract(m_table, m_extract);
    regroup();
}

void CadImporter::regroup()
{
    if (!m_loaded)
        return;
    const Program &program = this->program();
    m_parts = CadImport::groupParts(m_rows, program);
    for (CadPart &part : m_parts) {
        const auto it = m_choices.constFind({part.footprint, part.value});
        if (it == m_choices.cend())
            continue;
        // A chosen id that was removed meanwhile falls back to "create".
        const bool component = std::any_of(program.components.cbegin(), program.components.cend(),
                                           [&](const Component &c) { return c.id == it->component; });
        const bool package = std::any_of(program.packages.cbegin(), program.packages.cend(),
                                         [&](const Package &p) { return p.id == it->package; });
        part.component = component ? it->component : QString();
        part.package = package ? it->package : QString();
    }
    preview();
}

void CadImporter::preview()
{
    const int boards = program().boards.size();
    if (m_board >= boards)
        m_board = boards - 1;
    if (m_board < 0 && boards > 0)
        m_board = 0;
    // Implicit sharing: only the vectors apply() changes are copied.
    Program copy = program();
    m_summary = CadImport::apply(copy, m_board, m_rows, m_parts, m_mode);
    m_rowModel->refresh();
    m_partModel->refresh();
    emitChanged();
}

const Program &CadImporter::program() const
{
    return m_document->isLoaded() ? m_document->program() : m_blank;
}

void CadImporter::emitChanged()
{
    const QVector<QStringList> choices{delimiterChoices(), unitChoices(),      columnChoices(),
                                       boardChoices(),     componentChoices(), packageChoices()};
    if (choices != m_lastChoices) {
        m_lastChoices = choices;
        emit choicesChanged();
    }
    emit changed();
}

bool CadImporter::importPlacements()
{
    if (!canImport()) {
        m_document->reject(tr("无法导入"), summaryText());
        return false;
    }
    const QString boardId = program().boards.at(m_board).id;
    const QVector<CadPlacement> rows = m_rows;
    const QVector<CadPart> parts = m_parts;
    const int board = m_board;
    const CadImportMode mode = m_mode;
    // The preview was made against m_blank, which newProgram() starts from,
    // so the rows and part choices still fit. The import stays an undo step,
    // and the new program counts as modified until it is saved.
    const bool created = !m_document->isLoaded();
    if (created)
        m_document->newProgram();
    CadImportSummary s;
    const bool ok = m_document->edit(
        tr("导入贴片坐标 %1").arg(m_fileName), [&](Program &p, QVector<IdRename> &) {
            s = CadImport::apply(p, board, rows, parts, mode);
        });
    if (!ok)
        return false;

    QStringList lines;
    if (created)
        lines << tr("已新建程序，请记得保存。");
    lines << tr("单板 %1：新增 %2 个、更新 %3 个贴片位").arg(boardId).arg(s.added).arg(s.updated)
                 + (s.removed > 0 ? tr("，删除 %1 个").arg(s.removed) : QString()) + tr("。");
    if (!s.newComponents.isEmpty())
        lines << tr("新建元件：%1").arg(s.newComponents.join(QStringLiteral("、")));
    if (!s.newPackages.isEmpty()) {
        lines << tr("新建封装：%1。尺寸是占位值 %2 mm，请在封装页填写真实的长、宽、高和取放参数。")
                     .arg(s.newPackages.join(QStringLiteral("、")))
                     .arg(CadImport::kNewPackageSize);
    }
    if (s.outsideBoard > 0)
        lines << tr("%1 个贴片位在板外，请检查原点是否正确。").arg(s.outsideBoard);
    lines << tr("可以用“撤销”一步恢复。");
    close();
    emit imported(tr("导入完成"), lines.join(QLatin1Char('\n')));
    return true;
}

void CadImporter::request(int board)
{
    if (board >= 0 && board < program().boards.size())
        m_board = board;
    emit requested();
}
