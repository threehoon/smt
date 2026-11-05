#include "cadimport/cadplacements.h"

#include "model/angle.h"

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>

#include <cmath>

namespace CadPlacements {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("CadPlacements", text);
}

// Field `column` of a row, trimmed; null when the row is too short.
QString field(const CadTable::Row &row, int column)
{
    if (column < 0 || column >= row.fields.size())
        return {};
    return row.fields.at(column).trimmed();
}

bool hasColumn(const CadTable::Row &row, int column)
{
    return column >= 0 && column < row.fields.size();
}

// Plain number with '.' or a single ',' as decimal separator.
bool parseNumber(QString text, double *value)
{
    if (!text.contains(QLatin1Char('.')) && text.count(QLatin1Char(',')) == 1)
        text.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = text.toDouble(&ok);
    if (!ok || !std::isfinite(v))
        return false;
    *value = v;
    return true;
}

}  // namespace

bool parseLength(const QString &text, CadUnit unit, double *mm)
{
    static const QRegularExpression suffix(QStringLiteral("^(.*?)\\s*([a-z]+)$"),
                                           QRegularExpression::CaseInsensitiveOption);
    QString number = text.trimmed();
    double plain = 0.0;
    if (parseNumber(number, &plain)) {  // also "1e-3", which looks like a suffix
        *mm = plain * CadColumns::toMillimeter(unit);
        return true;
    }
    const QRegularExpressionMatch m = suffix.match(number);
    if (m.hasMatch()) {
        CadUnit own;
        if (!CadColumns::parseUnit(m.captured(2), &own))
            return false;
        unit = own;
        number = m.captured(1);
    }
    double v = 0.0;
    if (!parseNumber(number, &v))
        return false;
    *mm = v * CadColumns::toMillimeter(unit);
    return true;
}

bool parseSide(const QString &text, Side *side)
{
    QString key;
    for (const QChar c : text.toLower()) {
        if (c.isLetterOrNumber())
            key += c;
    }
    static const QHash<QString, Side> names = [] {
        QHash<QString, Side> h;
        for (const char16_t *n : {u"top", u"t", u"toplayer", u"front", u"f", u"fcu", u"component",
                                  u"顶面", u"顶层", u"正面", u"顶"})
            h.insert(QString::fromUtf16(n), Side::Top);
        for (const char16_t *n : {u"bottom", u"bot", u"b", u"bottomlayer", u"back", u"bcu",
                                  u"solder", u"底面", u"底层", u"背面", u"反面", u"底"})
            h.insert(QString::fromUtf16(n), Side::Bottom);
        return h;
    }();
    const auto it = names.constFind(key);
    if (it == names.cend())
        return false;
    *side = *it;
    return true;
}

CadUnit detectUnit(const CadTable &table, const CadMapping &mapping, bool *found)
{
    CadUnit unit = CadUnit::Millimeter;
    bool ok = false;
    for (const CadField f : {CadField::X, CadField::Y}) {
        const int c = mapping.column(f);
        if (!ok && c >= 0 && c < table.titles.size())
            ok = CadColumns::unitInTitle(table.titles.at(c), &unit);
    }
    for (int i = 0; !ok && i < table.notes.size(); ++i)
        ok = CadColumns::unitInText(table.notes.at(i), &unit);
    if (found)
        *found = ok;
    return ok ? unit : CadUnit::Millimeter;
}

QVector<CadPlacement> extract(const CadTable &table, const CadExtractOptions &options)
{
    const CadMapping &m = options.mapping;
    QVector<CadPlacement> out;
    out.reserve(table.rows.size());
    QHash<QString, int> seen;  // designator -> line of its first row

    for (const CadTable::Row &row : table.rows) {
        CadPlacement p;
        p.line = row.line;
        p.designator = field(row, m.column(CadField::Designator));
        p.footprint = field(row, m.column(CadField::Footprint));
        p.value = field(row, m.column(CadField::Value));
        QStringList errors;

        if (p.designator.isEmpty())
            errors << tr("缺少位号");

        const struct
        {
            CadField field;
            double *target;
            double origin;
            const char *name;
        } coords[] = {{CadField::X, &p.x, options.originX, "X"},
                      {CadField::Y, &p.y, options.originY, "Y"}};
        for (const auto &c : coords) {
            const int column = m.column(c.field);
            const QString text = field(row, column);
            double v = 0.0;
            if (!hasColumn(row, column) || text.isEmpty())
                errors << tr("缺少 %1").arg(QLatin1String(c.name));
            else if (!parseLength(text, options.unit, &v))
                errors << tr("%1 不是有效数值：%2").arg(QLatin1String(c.name), text);
            else
                *c.target = v - c.origin;
        }

        const int rotColumn = m.column(CadField::Rotation);
        if (rotColumn >= 0) {
            QString text = field(row, rotColumn);
            text.remove(QChar(0x00B0));  // °
            double r = 0.0;
            if (!text.isEmpty() && !parseNumber(text, &r))
                errors << tr("角度不是有效数值：%1").arg(text);
            else
                p.rotation = normalizeAngle(r);
        }

        const int sideColumn = m.column(CadField::Side);
        if (sideColumn >= 0) {
            const QString text = field(row, sideColumn);
            if (!text.isEmpty() && !parseSide(text, &p.side))
                errors << tr("无法识别的面：%1").arg(text);
        }

        if (!p.designator.isEmpty()) {
            const auto it = seen.constFind(p.designator);
            if (it != seen.cend())
                errors << tr("位号与第 %1 行重复").arg(*it);
            else
                seen.insert(p.designator, p.line);
        }

        p.error = errors.join(tr("；"));
        out.append(p);
    }
    return out;
}

}  // namespace CadPlacements
