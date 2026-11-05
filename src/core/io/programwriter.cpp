#include "io/programwriter.h"

#include "io/formatversion.h"
#include "io/rawxml.h"
#include "io/xmlnames.h"
#include "model/angle.h"

#include <QDir>
#include <QSaveFile>
#include <QXmlStreamWriter>

namespace {

// Fixed 4 decimals with '.', independent of the system locale.
QString formatNumber(double v)
{
    const QString s = QString::number(v, 'f', 4);
    return s == QLatin1String("-0.0000") ? s.mid(1) : s;
}

QString formatAngle(double degrees)
{
    const QString s = formatNumber(normalizeAngle(degrees));
    // 359.99996 rounds up to 360.0000, which is outside [0, 360).
    return s == QLatin1String("360.0000") ? QStringLiteral("0.0000") : s;
}

class Writer
{
public:
    explicit Writer(QByteArray *out) : m_xml(out)
    {
        m_xml.setAutoFormatting(true);
        m_xml.setAutoFormattingIndent(2);
    }

    void write(const Program &program);

private:
    void text(const char *name, const QString &value)
    {
        m_xml.writeAttribute(QLatin1String(name), value);
    }
    void optionalText(const char *name, const QString &value)
    {
        if (!value.isEmpty())
            text(name, value);
    }
    void number(const char *name, double value) { text(name, formatNumber(value)); }
    void angle(const char *name, double value) { text(name, formatAngle(value)); }
    void integer(const char *name, int value) { text(name, QString::number(value)); }
    void boolean(const char *name, bool value)
    {
        text(name, value ? QStringLiteral("true") : QStringLiteral("false"));
    }

    void writeInfo(const ProgramInfo &info);
    void writePackage(const Package &p);
    void writeBoard(const Board &b);
    void writeFiducials(const QVector<Fiducial> &list);
    void writePanel(const Panel &panel);
    void writeFeeder(const Feeder &f);
    void writeExtensions(const QVector<QString> &list);

    QXmlStreamWriter m_xml;
};

void Writer::write(const Program &program)
{
    m_xml.writeStartDocument();
    m_xml.writeStartElement(QStringLiteral("SmtProgram"));
    text("formatVersion", QStringLiteral("%1.%2").arg(kFormatMajor).arg(kFormatMinor));

    writeInfo(program.info);

    m_xml.writeStartElement(QStringLiteral("Packages"));
    for (const Package &p : program.packages)
        writePackage(p);
    m_xml.writeEndElement();

    m_xml.writeStartElement(QStringLiteral("Components"));
    for (const Component &c : program.components) {
        m_xml.writeStartElement(QStringLiteral("Component"));
        text("id", c.id);
        text("package", c.package);
        optionalText("value", c.value);
        optionalText("description", c.description);
        m_xml.writeEndElement();
    }
    m_xml.writeEndElement();

    m_xml.writeStartElement(QStringLiteral("Boards"));
    for (const Board &b : program.boards)
        writeBoard(b);
    m_xml.writeEndElement();

    writePanel(program.panel);

    if (program.feederSetup) {
        m_xml.writeStartElement(QStringLiteral("FeederSetup"));
        optionalText("machineModel", program.feederSetup->machineModel);
        for (const Feeder &f : program.feederSetup->feeders)
            writeFeeder(f);
        m_xml.writeEndElement();
    }

    writeExtensions(program.extensions);

    m_xml.writeEndElement();
    m_xml.writeEndDocument();
}

void Writer::writeInfo(const ProgramInfo &info)
{
    m_xml.writeStartElement(QStringLiteral("Info"));
    text("name", info.name);
    optionalText("productCode", info.productCode);
    optionalText("revision", info.revision);
    optionalText("author", info.author);
    optionalText("createdAt", info.createdAt);
    optionalText("modifiedAt", info.modifiedAt);
    if (!info.description.isEmpty())
        m_xml.writeTextElement(QStringLiteral("Description"), info.description);
    m_xml.writeEndElement();
}

void Writer::writePackage(const Package &p)
{
    m_xml.writeStartElement(QStringLiteral("Package"));
    text("id", p.id);
    number("bodyLength", p.bodyLength);
    number("bodyWidth", p.bodyWidth);
    number("height", p.height);
    integer("pinCount", p.pinCount);

    m_xml.writeStartElement(QStringLiteral("Pick"));
    optionalText("nozzle", p.pick.nozzles.join(QLatin1Char(',')));
    number("pickDepth", p.pick.pickDepth);
    boolean("vacuumCheck", p.pick.vacuumCheck);
    m_xml.writeEndElement();

    m_xml.writeStartElement(QStringLiteral("Place"));
    number("placeDepth", p.place.placeDepth);
    integer("speed", p.place.speed);
    m_xml.writeEndElement();

    m_xml.writeStartElement(QStringLiteral("Vision"));
    text("method", visionMethodNames().at(int(p.vision.method)));
    number("tolerance", p.vision.tolerance);
    m_xml.writeEndElement();

    writeExtensions(p.extensions);
    m_xml.writeEndElement();
}

void Writer::writeBoard(const Board &b)
{
    m_xml.writeStartElement(QStringLiteral("Board"));
    text("id", b.id);
    number("width", b.width);
    number("height", b.height);
    number("thickness", b.thickness);

    writeFiducials(b.fiducials);

    if (!b.badMarks.isEmpty()) {
        m_xml.writeStartElement(QStringLiteral("BadMarks"));
        for (const BadMark &m : b.badMarks) {
            m_xml.writeStartElement(QStringLiteral("BadMark"));
            text("id", m.id);
            number("x", m.x);
            number("y", m.y);
            m_xml.writeEndElement();
        }
        m_xml.writeEndElement();
    }

    if (!b.placements.isEmpty()) {
        m_xml.writeStartElement(QStringLiteral("Placements"));
        for (const Placement &p : b.placements) {
            m_xml.writeStartElement(QStringLiteral("Placement"));
            text("id", p.id);
            text("component", p.component);
            number("x", p.x);
            number("y", p.y);
            angle("rotation", p.rotation);
            text("side", sideNames().at(int(p.side)));
            boolean("enabled", p.enabled);
            m_xml.writeEndElement();
        }
        m_xml.writeEndElement();
    }
    m_xml.writeEndElement();
}

void Writer::writeFiducials(const QVector<Fiducial> &list)
{
    if (list.isEmpty())
        return;
    m_xml.writeStartElement(QStringLiteral("Fiducials"));
    for (const Fiducial &f : list) {
        m_xml.writeStartElement(QStringLiteral("Fiducial"));
        text("id", f.id);
        number("x", f.x);
        number("y", f.y);
        text("shape", fiducialShapeNames().at(int(f.shape)));
        number("size", f.size);
        m_xml.writeEndElement();
    }
    m_xml.writeEndElement();
}

void Writer::writePanel(const Panel &panel)
{
    m_xml.writeStartElement(QStringLiteral("Panel"));
    number("width", panel.width);
    number("height", panel.height);
    writeFiducials(panel.fiducials);

    m_xml.writeStartElement(QStringLiteral("BoardInstances"));
    for (const BoardInstance &bi : panel.instances) {
        m_xml.writeStartElement(QStringLiteral("BoardInstance"));
        text("id", bi.id);
        text("board", bi.board);
        number("x", bi.x);
        number("y", bi.y);
        angle("rotation", bi.rotation);
        boolean("enabled", bi.enabled);
        for (const QString &placement : bi.skippedPlacements) {
            m_xml.writeStartElement(QStringLiteral("Skip"));
            text("placement", placement);
            m_xml.writeEndElement();
        }
        m_xml.writeEndElement();
    }
    m_xml.writeEndElement();
    m_xml.writeEndElement();
}

void Writer::writeFeeder(const Feeder &f)
{
    m_xml.writeStartElement(QStringLiteral("Feeder"));
    text("id", f.id);
    text("type", f.type == FeederType::Unknown ? f.unknownType
                                               : feederTypeNames().at(int(f.type)));
    text("slot", f.slot);
    text("component", f.component);
    number("pickX", f.pickX);
    number("pickY", f.pickY);
    number("pickZ", f.pickZ);
    angle("rotation", f.rotation);
    boolean("enabled", f.enabled);
    integer("priority", f.priority);

    switch (f.type) {
    case FeederType::Tape:
        m_xml.writeStartElement(QStringLiteral("Tape"));
        number("width", f.tape.width);
        number("pitch", f.tape.pitch);
        integer("feedCount", f.tape.feedCount);
        m_xml.writeEndElement();
        break;
    case FeederType::Tray:
        m_xml.writeStartElement(QStringLiteral("Tray"));
        integer("rows", f.tray.rows);
        integer("columns", f.tray.columns);
        number("rowPitch", f.tray.rowPitch);
        number("columnPitch", f.tray.columnPitch);
        integer("startIndex", f.tray.startIndex);
        m_xml.writeEndElement();
        break;
    case FeederType::Tube:
        m_xml.writeStartElement(QStringLiteral("Tube"));
        number("pitch", f.tube.pitch);
        m_xml.writeEndElement();
        break;
    case FeederType::Bulk:
        break;
    case FeederType::Unknown:
        for (const QString &raw : f.rawParams)
            writeRawElement(m_xml, raw);
        break;
    }

    writeExtensions(f.extensions);
    m_xml.writeEndElement();
}

void Writer::writeExtensions(const QVector<QString> &list)
{
    if (list.isEmpty())
        return;
    m_xml.writeStartElement(QStringLiteral("Extensions"));
    for (const QString &raw : list)
        writeRawElement(m_xml, raw);
    m_xml.writeEndElement();
}

} // namespace

QByteArray ProgramWriter::toXml(const Program &program)
{
    QByteArray out;
    Writer writer(&out);
    writer.write(program);
    return out;
}

bool ProgramWriter::writeFile(const Program &program, const QString &path,
                              QString *errorMessage)
{
    QSaveFile file(path);
    const auto fail = [&](const QString &reason) {
        if (errorMessage)
            *errorMessage = tr("无法保存 %1：%2").arg(QDir::toNativeSeparators(path), reason);
        return false;
    };
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());
    if (file.write(toXml(program)) < 0)
        return fail(file.errorString());
    if (!file.commit())
        return fail(file.errorString());
    return true;
}
