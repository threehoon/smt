#include "io/programreader.h"

#include "io/formatversion.h"
#include "io/rawxml.h"
#include "io/xmlnames.h"
#include "model/angle.h"
#include "validation/programvalidator.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <optional>

bool ReadResult::ok() const
{
    return !hasErrors(diagnostics);
}

QVector<Diagnostic> ReadResult::errors() const
{
    return filterSeverity(diagnostics, Diagnostic::Severity::Error);
}

QVector<Diagnostic> ReadResult::warnings() const
{
    return filterSeverity(diagnostics, Diagnostic::Severity::Warning);
}

namespace {

using Kind = ProgramLocation::Kind;

ProgramLocation at(Kind kind, int index = -1, int subIndex = -1)
{
    ProgramLocation l;
    l.kind = kind;
    l.index = index;
    l.subIndex = subIndex;
    return l;
}

// Parses the file into a Program, checking only what cannot be represented in
// memory: syntax, required attributes, number / enum spelling, sections. Every
// element that becomes part of the Program records its line, so the
// ProgramValidator diagnostics run afterwards can be reported with lines.
class Parser
{
    Q_DECLARE_TR_FUNCTIONS(ProgramReader)

public:
    explicit Parser(QIODevice *device) : m_xml(device) { m_xml.setNamespaceProcessing(false); }

    ReadResult run();

private:
    // Attribute access for the element the reader is currently on. Records
    // which attributes were consumed so finish() can warn about the rest.
    //
    // `location` is the Program element this XML element is read into. An
    // attribute that is missing or unreadable is reported here and marked as
    // failed, so the validator's complaint about the fallback value is dropped.
    class Element
    {
    public:
        explicit Element(Parser &parser, const ProgramLocation &location = {})
            : m_parser(parser),
              m_attributes(parser.m_xml.attributes()),
              m_name(parser.m_xml.name().toString()),
              m_line(parser.m_xml.lineNumber()),
              m_location(location)
        {
            if (location.isValid())
                parser.m_lines.insert(location, m_line);
        }

        qint64 line() const { return m_line; }

        QString text(const char *name, const QString &fallback = QString());
        std::optional<QString> requiredText(const char *name);
        double number(const char *name, double fallback);
        double requiredNumber(const char *name);
        int integer(const char *name, int fallback);
        int requiredInteger(const char *name, int fallback);
        bool boolean(const char *name, bool fallback);
        int choice(const char *name, const QStringList &names, int fallback);
        void finish();

    private:
        std::optional<QString> take(const char *name);
        double toNumber(const char *name, const QString &value, double fallback);
        int toInteger(const char *name, const QString &value, int fallback);
        void error(const char *name, const QString &message);

        Parser &m_parser;
        QXmlStreamAttributes m_attributes;
        QString m_name;
        qint64 m_line;
        ProgramLocation m_location;
        QSet<QString> m_used;
    };

    // Reads an element whose only content is a list of `child` elements.
    template <typename Fn>
    void forEachChild(const char *child, Fn &&parseChild)
    {
        Element e(*this);
        e.finish();
        while (m_xml.readNextStartElement()) {
            if (m_xml.name() == QLatin1String(child))
                parseChild();
            else
                unknownElement();
        }
    }

    void error(qint64 line, const QString &message);
    void warning(qint64 line, const QString &message);
    void unknownElement();
    void noChildren();

    void parseRoot();
    bool checkVersion(const QString &version, qint64 line);
    void checkSections();
    void parseInfo();
    void parsePackage();
    void parseComponent();
    void parseBoard();
    void parseFiducials(QVector<Fiducial> &list, int board);
    void parseBadMarks(QVector<BadMark> &list, int board);
    void parsePlacements(Board &board, int boardIndex);
    void parsePanel();
    void parseBoardInstance();
    void parseFeederSetup();
    void parseFeeder(FeederSetup &setup);
    void parseFeederParams(Feeder &feeder, int index);
    QVector<QString> parseExtensions();
    void validate();

    QXmlStreamReader m_xml;
    ReadResult m_result;
    Program &m_program = m_result.program;
    bool m_aborted = false;  // a fatal error made the rest of the file meaningless
    qint64 m_rootLine = 0;
    QHash<QString, qint64> m_sections;  // top-level section -> line of first occurrence

    QHash<ProgramLocation, qint64> m_lines;  // where each Program element was read
    // Attributes already reported as missing or unreadable, per element.
    QHash<ProgramLocation, QSet<QString>> m_failed;
};

// ---- Element ---------------------------------------------------------------

std::optional<QString> Parser::Element::take(const char *name)
{
    const QLatin1String key(name);
    m_used.insert(key);
    if (!m_attributes.hasAttribute(key))
        return std::nullopt;
    return m_attributes.value(key).toString();
}

QString Parser::Element::text(const char *name, const QString &fallback)
{
    return take(name).value_or(fallback);
}

std::optional<QString> Parser::Element::requiredText(const char *name)
{
    std::optional<QString> value = take(name);
    if (!value)
        error(name, tr("<%1> 缺少必填属性 %2").arg(m_name, QLatin1String(name)));
    return value;
}

void Parser::Element::error(const char *name, const QString &message)
{
    m_parser.error(m_line, message);
    if (m_location.isValid())
        m_parser.m_failed[m_location].insert(QLatin1String(name));
}

// Range checks are ProgramValidator's; this only reads the spelling.
double Parser::Element::toNumber(const char *name, const QString &value, double fallback)
{
    bool ok = false;
    const double v = value.toDouble(&ok);
    if (!ok || !std::isfinite(v)) {
        error(name, tr("<%1> 的 %2=\"%3\" 不是有效数值").arg(m_name, QLatin1String(name), value));
        return fallback;
    }
    return v;
}

double Parser::Element::number(const char *name, double fallback)
{
    const std::optional<QString> value = take(name);
    return value ? toNumber(name, *value, fallback) : fallback;
}

double Parser::Element::requiredNumber(const char *name)
{
    const std::optional<QString> value = requiredText(name);
    return value ? toNumber(name, *value, 0.0) : 0.0;
}

int Parser::Element::toInteger(const char *name, const QString &value, int fallback)
{
    bool ok = false;
    const int v = value.toInt(&ok);
    if (!ok) {
        error(name, tr("<%1> 的 %2=\"%3\" 不是整数").arg(m_name, QLatin1String(name), value));
        return fallback;
    }
    return v;
}

int Parser::Element::integer(const char *name, int fallback)
{
    const std::optional<QString> value = take(name);
    return value ? toInteger(name, *value, fallback) : fallback;
}

int Parser::Element::requiredInteger(const char *name, int fallback)
{
    const std::optional<QString> value = requiredText(name);
    return value ? toInteger(name, *value, fallback) : fallback;
}

bool Parser::Element::boolean(const char *name, bool fallback)
{
    const std::optional<QString> value = take(name);
    if (!value)
        return fallback;
    if (*value == QLatin1String("true"))
        return true;
    if (*value == QLatin1String("false"))
        return false;
    error(name,
          tr("<%1> 的 %2=\"%3\" 只能是 true 或 false").arg(m_name, QLatin1String(name), *value));
    return fallback;
}

int Parser::Element::choice(const char *name, const QStringList &names, int fallback)
{
    const std::optional<QString> value = take(name);
    if (!value)
        return fallback;
    const int index = names.indexOf(*value);
    if (index < 0) {
        error(name, tr("<%1> 的 %2=\"%3\" 无效，可选值：%4")
                  .arg(m_name, QLatin1String(name), *value, names.join(QStringLiteral("、"))));
        return fallback;
    }
    return index;
}

void Parser::Element::finish()
{
    for (const QXmlStreamAttribute &a : m_attributes) {
        const QString name = a.qualifiedName().toString();
        if (!m_used.contains(name))
            m_parser.warning(m_line, tr("<%1> 的属性 %2 无法识别，已忽略").arg(m_name, name));
    }
}

// ---- Parser: infrastructure -------------------------------------------------

void Parser::error(qint64 line, const QString &message)
{
    Diagnostic d;
    d.severity = Diagnostic::Severity::Error;
    d.line = line;
    d.message = message;
    m_result.diagnostics.append(d);
}

void Parser::warning(qint64 line, const QString &message)
{
    Diagnostic d;
    d.severity = Diagnostic::Severity::Warning;
    d.line = line;
    d.message = message;
    m_result.diagnostics.append(d);
}

void Parser::unknownElement()
{
    warning(m_xml.lineNumber(),
            tr("无法识别的元素 <%1>，已忽略").arg(m_xml.name().toString()));
    m_xml.skipCurrentElement();
}

void Parser::noChildren()
{
    while (m_xml.readNextStartElement())
        unknownElement();
}

ReadResult Parser::run()
{
    if (m_xml.readNextStartElement()) {
        if (m_xml.name() == QLatin1String("SmtProgram")) {
            parseRoot();
        } else {
            error(m_xml.lineNumber(), tr("根元素必须是 <SmtProgram>，实际是 <%1>")
                                          .arg(m_xml.name().toString()));
            m_aborted = true;
        }
    }
    // Read to the end so syntax errors after the root are reported too.
    while (!m_xml.atEnd())
        m_xml.readNext();

    if (m_xml.hasError()) {
        // Everything after the syntax error is missing, so section and
        // reference errors would only be noise.
        error(m_xml.lineNumber(), tr("XML 语法错误：%1").arg(m_xml.errorString()));
    } else if (!m_aborted) {
        checkSections();
        validate();
    }

    std::stable_sort(m_result.diagnostics.begin(), m_result.diagnostics.end(),
                     [](const Diagnostic &a, const Diagnostic &b) { return a.line < b.line; });
    return m_result;
}

// ---- Parser: structure ------------------------------------------------------

void Parser::parseRoot()
{
    Element root(*this);
    m_rootLine = root.line();
    const std::optional<QString> version = root.requiredText("formatVersion");
    root.finish();
    if (!version || !checkVersion(*version, m_rootLine)) {
        m_aborted = true;
        m_xml.skipCurrentElement();
        return;
    }

    while (m_xml.readNextStartElement()) {
        const QString name = m_xml.name().toString();
        static const QStringList kSections{
            QStringLiteral("Info"),   QStringLiteral("Packages"),    QStringLiteral("Components"),
            QStringLiteral("Boards"), QStringLiteral("Panel"),       QStringLiteral("FeederSetup"),
            QStringLiteral("Extensions")};
        if (!kSections.contains(name)) {
            unknownElement();
            continue;
        }
        const auto seen = m_sections.constFind(name);
        if (seen != m_sections.constEnd()) {
            error(m_xml.lineNumber(), tr("<%1> 重复出现，第 %2 行已有")
                                          .arg(name, QString::number(*seen)));
            m_xml.skipCurrentElement();
            continue;
        }
        m_sections.insert(name, m_xml.lineNumber());

        if (name == QLatin1String("Info"))
            parseInfo();
        else if (name == QLatin1String("Packages"))
            forEachChild("Package", [this] { parsePackage(); });
        else if (name == QLatin1String("Components"))
            forEachChild("Component", [this] { parseComponent(); });
        else if (name == QLatin1String("Boards"))
            forEachChild("Board", [this] { parseBoard(); });
        else if (name == QLatin1String("Panel"))
            parsePanel();
        else if (name == QLatin1String("FeederSetup"))
            parseFeederSetup();
        else
            m_program.extensions = parseExtensions();
    }
}

bool Parser::checkVersion(const QString &version, qint64 line)
{
    static const QRegularExpression kPattern(QStringLiteral("^([0-9]+)\\.([0-9]+)$"));
    const QRegularExpressionMatch match = kPattern.match(version);
    if (!match.hasMatch()) {
        error(line, tr("formatVersion=\"%1\" 格式错误，应为 主版本.次版本，例如 1.0")
                        .arg(version));
        return false;
    }
    const int major = match.captured(1).toInt();
    const int minor = match.captured(2).toInt();
    if (major > kFormatMajor) {
        error(line, tr("文件格式版本 %1 比本软件支持的 %2.%3 新，请升级软件")
                        .arg(version, QString::number(kFormatMajor),
                             QString::number(kFormatMinor)));
        return false;
    }
    if (major < kFormatMajor) {
        error(line, tr("不支持文件格式版本 %1").arg(version));
        return false;
    }
    if (minor > kFormatMinor) {
        m_result.newerMinorVersion = true;
        warning(line, tr("文件由更新版本的软件创建（格式 %1），本软件无法识别的内容会被忽略，"
                         "保存会丢失这些内容，建议另存为")
                          .arg(version));
    }
    return true;
}

void Parser::checkSections()
{
    static const char *const kRequired[] = {"Info", "Packages", "Components", "Boards", "Panel"};
    for (const char *name : kRequired) {
        if (!m_sections.contains(QLatin1String(name)))
            error(m_rootLine, tr("缺少 <%1> 段").arg(QLatin1String(name)));
    }
}

void Parser::parseInfo()
{
    Element e(*this);
    ProgramInfo &info = m_program.info;
    info.name = e.requiredText("name").value_or(QString());
    info.productCode = e.text("productCode");
    info.revision = e.text("revision");
    info.createdAt = e.text("createdAt");
    info.modifiedAt = e.text("modifiedAt");
    info.author = e.text("author");
    e.finish();
    while (m_xml.readNextStartElement()) {
        if (m_xml.name() == QLatin1String("Description"))
            info.description = m_xml.readElementText(QXmlStreamReader::SkipChildElements);
        else
            unknownElement();
    }
}

// ---- Parser: product data ---------------------------------------------------

void Parser::parsePackage()
{
    const int index = m_program.packages.size();
    Element e(*this, at(Kind::Package, index));
    Package p;
    p.id = e.requiredText("id").value_or(QString());
    p.bodyLength = e.requiredNumber("bodyLength");
    p.bodyWidth = e.requiredNumber("bodyWidth");
    p.height = e.requiredNumber("height");
    p.pinCount = e.integer("pinCount", 0);
    e.finish();

    while (m_xml.readNextStartElement()) {
        const QStringRef name = m_xml.name();
        if (name == QLatin1String("Pick")) {
            Element c(*this, at(Kind::PackagePick, index));
            p.pick.nozzles.clear();
            for (const QString &n : c.text("nozzle").split(QLatin1Char(','))) {
                if (!n.trimmed().isEmpty())
                    p.pick.nozzles.append(n.trimmed());
            }
            p.pick.pickDepth = c.number("pickDepth", 0.0);
            p.pick.vacuumCheck = c.boolean("vacuumCheck", true);
            c.finish();
            noChildren();
        } else if (name == QLatin1String("Place")) {
            Element c(*this, at(Kind::PackagePlace, index));
            p.place.placeDepth = c.number("placeDepth", 0.0);
            p.place.speed = c.integer("speed", 100);
            c.finish();
            noChildren();
        } else if (name == QLatin1String("Vision")) {
            Element c(*this, at(Kind::PackageVision, index));
            p.vision.method = static_cast<VisionMethod>(
                c.choice("method", visionMethodNames(), int(VisionMethod::Bottom)));
            p.vision.tolerance = c.number("tolerance", 0.1);
            c.finish();
            noChildren();
        } else if (name == QLatin1String("Extensions")) {
            p.extensions = parseExtensions();
        } else {
            unknownElement();
        }
    }
    m_program.packages.append(p);
}

void Parser::parseComponent()
{
    Element e(*this, at(Kind::Component, m_program.components.size()));
    Component c;
    c.id = e.requiredText("id").value_or(QString());
    c.package = e.requiredText("package").value_or(QString());
    c.value = e.text("value");
    c.description = e.text("description");
    e.finish();
    noChildren();
    m_program.components.append(c);
}

void Parser::parseBoard()
{
    const int index = m_program.boards.size();
    Element e(*this, at(Kind::Board, index));
    Board b;
    b.id = e.requiredText("id").value_or(QString());
    b.width = e.requiredNumber("width");
    b.height = e.requiredNumber("height");
    b.thickness = e.requiredNumber("thickness");
    e.finish();

    while (m_xml.readNextStartElement()) {
        const QStringRef name = m_xml.name();
        if (name == QLatin1String("Fiducials"))
            parseFiducials(b.fiducials, index);
        else if (name == QLatin1String("BadMarks"))
            parseBadMarks(b.badMarks, index);
        else if (name == QLatin1String("Placements"))
            parsePlacements(b, index);
        else
            unknownElement();
    }
    m_program.boards.append(b);
}

// board < 0: the panel's fiducials.
void Parser::parseFiducials(QVector<Fiducial> &list, int board)
{
    forEachChild("Fiducial", [&] {
        Element e(*this, board < 0 ? at(Kind::PanelFiducial, list.size())
                                   : at(Kind::BoardFiducial, board, list.size()));
        Fiducial f;
        f.id = e.requiredText("id").value_or(QString());
        f.x = e.requiredNumber("x");
        f.y = e.requiredNumber("y");
        f.shape = static_cast<FiducialShape>(
            e.choice("shape", fiducialShapeNames(), int(FiducialShape::Circle)));
        f.size = e.number("size", 1.0);
        e.finish();
        noChildren();
        list.append(f);
    });
}

void Parser::parseBadMarks(QVector<BadMark> &list, int board)
{
    forEachChild("BadMark", [&] {
        Element e(*this, at(Kind::BadMark, board, list.size()));
        BadMark m;
        m.id = e.requiredText("id").value_or(QString());
        m.x = e.requiredNumber("x");
        m.y = e.requiredNumber("y");
        e.finish();
        noChildren();
        list.append(m);
    });
}

void Parser::parsePlacements(Board &board, int boardIndex)
{
    forEachChild("Placement", [&] {
        Element e(*this, at(Kind::Placement, boardIndex, board.placements.size()));
        Placement p;
        p.id = e.requiredText("id").value_or(QString());
        p.component = e.requiredText("component").value_or(QString());
        p.x = e.requiredNumber("x");
        p.y = e.requiredNumber("y");
        p.rotation = normalizeAngle(e.requiredNumber("rotation"));
        p.side = static_cast<Side>(e.choice("side", sideNames(), int(Side::Top)));
        p.enabled = e.boolean("enabled", true);
        e.finish();
        noChildren();
        board.placements.append(p);
    });
}

void Parser::parsePanel()
{
    Element e(*this, at(Kind::Panel));
    Panel &panel = m_program.panel;
    panel.width = e.requiredNumber("width");
    panel.height = e.requiredNumber("height");
    e.finish();

    while (m_xml.readNextStartElement()) {
        const QStringRef name = m_xml.name();
        if (name == QLatin1String("Fiducials"))
            parseFiducials(panel.fiducials, -1);
        else if (name == QLatin1String("BoardInstances"))
            forEachChild("BoardInstance", [this] { parseBoardInstance(); });
        else
            unknownElement();
    }
}

void Parser::parseBoardInstance()
{
    const int index = m_program.panel.instances.size();
    Element e(*this, at(Kind::BoardInstance, index));
    BoardInstance bi;
    bi.id = e.requiredText("id").value_or(QString());
    bi.board = e.requiredText("board").value_or(QString());
    bi.x = e.requiredNumber("x");
    bi.y = e.requiredNumber("y");
    bi.rotation = normalizeAngle(e.requiredNumber("rotation"));
    bi.enabled = e.boolean("enabled", true);
    e.finish();

    while (m_xml.readNextStartElement()) {
        if (m_xml.name() != QLatin1String("Skip")) {
            unknownElement();
            continue;
        }
        Element s(*this, at(Kind::Skip, index, bi.skippedPlacements.size()));
        const std::optional<QString> placement = s.requiredText("placement");
        s.finish();
        noChildren();
        // Kept even when repeated or unknown, so the validator reports it.
        if (placement)
            bi.skippedPlacements.append(*placement);
    }
    m_program.panel.instances.append(bi);
}

// ---- Parser: feeders --------------------------------------------------------

void Parser::parseFeederSetup()
{
    Element e(*this);
    FeederSetup setup;
    setup.machineModel = e.text("machineModel");
    e.finish();
    while (m_xml.readNextStartElement()) {
        if (m_xml.name() == QLatin1String("Feeder"))
            parseFeeder(setup);
        else
            unknownElement();
    }
    m_program.feederSetup = setup;
}

void Parser::parseFeeder(FeederSetup &setup)
{
    const int index = setup.feeders.size();
    Element e(*this, at(Kind::Feeder, index));
    Feeder f;
    f.id = e.requiredText("id").value_or(QString());
    if (const std::optional<QString> type = e.requiredText("type")) {
        const int known = feederTypeNames().indexOf(*type);
        if (known >= 0) {
            f.type = static_cast<FeederType>(known);
        } else {
            f.type = FeederType::Unknown;
            f.unknownType = *type;
        }
    }
    f.slot = e.requiredText("slot").value_or(QString());
    f.component = e.requiredText("component").value_or(QString());
    f.pickX = e.requiredNumber("pickX");
    f.pickY = e.requiredNumber("pickY");
    f.pickZ = e.requiredNumber("pickZ");
    f.rotation = normalizeAngle(e.number("rotation", 0.0));
    f.enabled = e.boolean("enabled", true);
    f.priority = e.integer("priority", 0);
    e.finish();
    if (f.type == FeederType::Unknown) {
        warning(e.line(), tr("送料器 %1 的类型 \"%2\" 无法识别，已保留原样，运行时不能供料")
                              .arg(f.id, f.unknownType));
    }

    QString paramsName;
    if (f.type == FeederType::Tape)
        paramsName = QStringLiteral("Tape");
    else if (f.type == FeederType::Tray)
        paramsName = QStringLiteral("Tray");
    else if (f.type == FeederType::Tube)
        paramsName = QStringLiteral("Tube");

    bool hasParams = false;
    while (m_xml.readNextStartElement()) {
        if (m_xml.name() == QLatin1String("Extensions")) {
            f.extensions = parseExtensions();
        } else if (f.type == FeederType::Unknown) {
            f.rawParams.append(captureElement(m_xml));
        } else if (!hasParams && !paramsName.isEmpty() && m_xml.name() == paramsName) {
            hasParams = true;
            parseFeederParams(f, index);
        } else {
            unknownElement();
        }
    }
    if (!hasParams && (f.type == FeederType::Tape || f.type == FeederType::Tray))
        error(e.line(), tr("送料器 %1 缺少 <%2> 参数").arg(f.id, paramsName));
    setup.feeders.append(f);
}

void Parser::parseFeederParams(Feeder &feeder, int index)
{
    Element e(*this, at(Kind::FeederParams, index));
    switch (feeder.type) {
    case FeederType::Tape:
        feeder.tape.width = e.requiredNumber("width");
        feeder.tape.pitch = e.requiredNumber("pitch");
        feeder.tape.feedCount = e.integer("feedCount", 1);
        break;
    case FeederType::Tray: {
        TrayParams &t = feeder.tray;
        t.rows = e.requiredInteger("rows", 1);
        t.columns = e.requiredInteger("columns", 1);
        t.rowPitch = e.requiredNumber("rowPitch");
        t.columnPitch = e.requiredNumber("columnPitch");
        t.startIndex = e.integer("startIndex", 0);
        break;
    }
    case FeederType::Tube:
        feeder.tube.pitch = e.number("pitch", 0.0);
        break;
    case FeederType::Bulk:
    case FeederType::Unknown:
        break;
    }
    e.finish();
    noChildren();
}

QVector<QString> Parser::parseExtensions()
{
    QVector<QString> list;
    forEachChild("Extension", [&] { list.append(captureElement(m_xml)); });
    return list;
}

// ---- Parser: consistency ----------------------------------------------------

void Parser::validate()
{
    // Without a <Panel> section, its size and "no board instance" errors only
    // repeat the missing-section error.
    const bool hasPanel = m_sections.contains(QStringLiteral("Panel"));
    const QVector<Diagnostic> found = ProgramValidator::validate(m_program);
    for (Diagnostic d : found) {
        if (!hasPanel && d.location.kind == Kind::Panel)
            continue;
        // A missing or unreadable attribute was already reported; the
        // validator only saw its fallback value.
        const auto failed = m_failed.constFind(d.location);
        if (!d.field.isEmpty() && failed != m_failed.constEnd() && failed->contains(d.field))
            continue;
        d.line = m_lines.value(d.location, 0);
        if (d.related.isValid()) {
            if (const qint64 first = m_lines.value(d.related, 0))
                d.message = tr("%1，第 %2 行已定义").arg(d.message, QString::number(first));
        }
        m_result.diagnostics.append(d);
    }
}

} // namespace

ReadResult ProgramReader::read(QIODevice *device)
{
    Parser parser(device);
    return parser.run();
}

ReadResult ProgramReader::read(const QByteArray &data)
{
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    return read(&buffer);
}

ReadResult ProgramReader::readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        ReadResult result;
        Diagnostic d;
        d.severity = Diagnostic::Severity::Error;
        d.message = tr("无法打开文件 %1：%2").arg(QDir::toNativeSeparators(path),
                                                 file.errorString());
        result.diagnostics.append(d);
        return result;
    }
    return read(&file);
}
