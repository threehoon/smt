#include "cadimport/cadcolumns.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

bool CadMapping::isComplete() const
{
    return column(CadField::Designator) >= 0 && column(CadField::X) >= 0
        && column(CadField::Y) >= 0;
}

namespace CadColumns {

namespace {

struct Alias
{
    CadField field;
    int rank;  // lower wins
};

// Lower case, without a unit in parentheses and without separators:
// "Center-X(mm)" -> "centerx", "# Ref" -> "ref", "Mid X" -> "midx".
QString normalize(const QString &title)
{
    static const QRegularExpression unit(QStringLiteral("\\([^)]*\\)"));
    QString t = title.toLower();
    t.remove(unit);
    QString out;
    out.reserve(t.size());
    for (const QChar c : t) {
        if (c.isLetterOrNumber())
            out += c;
    }
    return out;
}

const QHash<QString, Alias> &aliases()
{
    static const QHash<QString, Alias> table = [] {
        QHash<QString, Alias> h;
        auto add = [&h](CadField field, int rank, std::initializer_list<const char16_t *> names) {
            for (const char16_t *n : names)
                h.insert(QString::fromUtf16(n), {field, rank});
        };
        add(CadField::Designator, 0,
            {u"designator", u"ref", u"refdes", u"reference", u"位号", u"元件位号", u"标号"});
        add(CadField::Designator, 1, {u"part", u"name", u"partname", u"component"});
        // Body center first; KiCad's PosX is the footprint anchor, which is
        // the center for SMD footprints; pads last.
        add(CadField::X, 0, {u"centerx", u"midx", u"cx", u"centroidx", u"中心x"});
        add(CadField::X, 1, {u"x", u"posx", u"refx", u"locationx", u"坐标x"});
        add(CadField::X, 2, {u"padx"});
        add(CadField::Y, 0, {u"centery", u"midy", u"cy", u"centroidy", u"中心y"});
        add(CadField::Y, 1, {u"y", u"posy", u"refy", u"locationy", u"坐标y"});
        add(CadField::Y, 2, {u"pady"});
        add(CadField::Rotation, 0,
            {u"rotation", u"rot", u"angle", u"orientation", u"rotate", u"角度", u"旋转角度"});
        add(CadField::Side, 0, {u"side", u"layer", u"tb", u"面", u"层", u"贴装面"});
        add(CadField::Footprint, 0,
            {u"footprint", u"package", u"pattern", u"pcbfootprint", u"封装"});
        add(CadField::Value, 0, {u"value", u"val", u"值", u"参数"});
        add(CadField::Value, 1, {u"comment"});
        return h;
    }();
    return table;
}

}  // namespace

bool parseUnit(const QString &word, CadUnit *unit)
{
    const QString w = word.toLower();
    if (w == QLatin1String("mm") || w == QLatin1String("metric")) {
        *unit = CadUnit::Millimeter;
    } else if (w == QLatin1String("mil") || w == QLatin1String("mils")
               || w == QLatin1String("imperial")) {
        *unit = CadUnit::Mil;
    } else if (w == QLatin1String("in") || w == QLatin1String("inch")
               || w == QLatin1String("inches")) {
        *unit = CadUnit::Inch;
    } else {
        return false;
    }
    return true;
}

bool recognize(const QString &title, CadField *field, int *rank)
{
    const auto it = aliases().constFind(normalize(title));
    if (it == aliases().cend())
        return false;
    *field = it->field;
    if (rank)
        *rank = it->rank;
    return true;
}

CadMapping guess(const QStringList &titles)
{
    CadMapping mapping;
    std::array<int, kCadFieldCount> best;
    best.fill(-1);
    for (int i = 0; i < titles.size(); ++i) {
        CadField field;
        int rank = 0;
        if (!recognize(titles.at(i), &field, &rank))
            continue;
        const int f = static_cast<int>(field);
        if (mapping.columns[f] < 0 || rank < best[f]) {
            mapping.columns[f] = i;
            best[f] = rank;
        }
    }
    return mapping;
}

int recognizedCount(const QStringList &titles)
{
    QSet<int> fields;
    for (const QString &t : titles) {
        CadField field;
        if (recognize(t, &field))
            fields.insert(static_cast<int>(field));
    }
    return fields.size();
}

bool unitInTitle(const QString &title, CadUnit *unit)
{
    static const QRegularExpression re(QStringLiteral("\\(\\s*([a-z]+)\\s*\\)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(title);
    return m.hasMatch() && parseUnit(m.captured(1), unit);
}

bool unitInText(const QString &text, CadUnit *unit)
{
    // The word after "unit ... :" or "unit ... =" on the same line.
    static const QRegularExpression re(QStringLiteral("\\bunits?\\b[^:=\\n]*[:=]\\s*([a-z]+)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(text);
    return m.hasMatch() && parseUnit(m.captured(1), unit);
}

QString formatName(const QStringList &titles)
{
    QSet<QString> n;
    for (const QString &t : titles)
        n.insert(normalize(t));
    if (n.contains(QStringLiteral("posx")))
        return QStringLiteral("KiCad");
    if (n.contains(QStringLiteral("midx")))
        return QStringLiteral("EasyEDA / JLC");
    if (n.contains(QStringLiteral("designator"))
        && (n.contains(QStringLiteral("centerx")) || n.contains(QStringLiteral("refx"))))
        return QStringLiteral("Altium");
    return {};
}

double toMillimeter(CadUnit unit)
{
    switch (unit) {
    case CadUnit::Millimeter:
        return 1.0;
    case CadUnit::Mil:
        return 0.0254;
    case CadUnit::Inch:
        return 25.4;
    }
    return 1.0;
}

}  // namespace CadColumns
