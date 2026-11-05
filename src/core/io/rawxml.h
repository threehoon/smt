#pragma once

#include <QString>

class QXmlStreamReader;
class QXmlStreamWriter;

// Keeps XML this version does not interpret (extensions, unknown feeder
// parameters) so it can be written back. Content and structure are preserved;
// whitespace-only text, attribute quoting and indentation are not.
// Both readers must have namespace processing off, so prefixes and xmlns
// attributes pass through as plain names.

// Reader must be on a StartElement. Consumes the element up to its EndElement
// and returns it as a standalone XML string.
QString captureElement(QXmlStreamReader &reader);

// Writes an element previously returned by captureElement.
void writeRawElement(QXmlStreamWriter &writer, const QString &xml);
