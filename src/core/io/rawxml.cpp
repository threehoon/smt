#include "io/rawxml.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

void copyToken(const QXmlStreamReader &reader, QXmlStreamWriter &writer)
{
    switch (reader.tokenType()) {
    case QXmlStreamReader::StartElement:
        writer.writeStartElement(reader.qualifiedName().toString());
        writer.writeAttributes(reader.attributes());
        break;
    case QXmlStreamReader::EndElement:
        writer.writeEndElement();
        break;
    case QXmlStreamReader::Characters:
        if (reader.isCDATA())
            writer.writeCDATA(reader.text().toString());
        else if (!reader.isWhitespace())
            writer.writeCharacters(reader.text().toString());
        break;
    case QXmlStreamReader::Comment:
        writer.writeComment(reader.text().toString());
        break;
    case QXmlStreamReader::ProcessingInstruction:
        writer.writeProcessingInstruction(reader.processingInstructionTarget().toString(),
                                          reader.processingInstructionData().toString());
        break;
    default:
        break;
    }
}

} // namespace

QString captureElement(QXmlStreamReader &reader)
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    int depth = 0;
    do {
        copyToken(reader, writer);
        if (reader.isStartElement())
            ++depth;
        else if (reader.isEndElement())
            --depth;
        if (depth == 0)
            break;
        reader.readNext();
    } while (!reader.atEnd());
    return xml;
}

void writeRawElement(QXmlStreamWriter &writer, const QString &xml)
{
    QXmlStreamReader reader(xml);
    reader.setNamespaceProcessing(false);
    while (!reader.atEnd()) {
        reader.readNext();
        copyToken(reader, writer);
    }
}
