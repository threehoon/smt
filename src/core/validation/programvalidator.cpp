#include "validation/programvalidator.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>

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

class Checker
{
    Q_DECLARE_TR_FUNCTIONS(ProgramValidator)

public:
    explicit Checker(const Program &program) : m_program(program) {}

    QVector<Diagnostic> run();

private:
    void add(Diagnostic::Severity severity, const ProgramLocation &location, const char *field,
             const QString &message, const ProgramLocation &related = {});
    void error(const ProgramLocation &location, const char *field, const QString &message,
               const ProgramLocation &related = {})
    {
        add(Diagnostic::Severity::Error, location, field, message, related);
    }

    // An id shown in a message; an empty id would leave a gap in the sentence.
    static QString name(const QString &id) { return id.isEmpty() ? tr("（空）") : id; }

    // Reports an empty id or one already in `seen`, then records it.
    // `what` names the kind and its scope, e.g. "封装" or "单板 PCB 中的贴片位".
    void checkId(QHash<QString, ProgramLocation> &seen, const QString &id,
                 const ProgramLocation &location, const QString &what);

    // Value checks. `owner` ends with 的, e.g. "封装 R0603 的"; `label` is the
    // user-facing field name, `field` the attribute name in the file.
    bool finite(const ProgramLocation &location, const char *field, const QString &owner,
                const QString &label, double value);
    void positive(const ProgramLocation &location, const char *field, const QString &owner,
                  const QString &label, double value);
    void nonNegative(const ProgramLocation &location, const char *field, const QString &owner,
                     const QString &label, double value);
    void atLeast(const ProgramLocation &location, const char *field, const QString &owner,
                 const QString &label, int value, int min);
    void between(const ProgramLocation &location, const char *field, const QString &owner,
                 const QString &label, int value, int min, int max);

    void checkPackages();
    void checkComponents();
    void checkBoards();
    void checkFiducials(const QVector<Fiducial> &list, int board, const QString &what,
                        const QString &ownerPrefix);
    void checkPanel();
    void checkFeeders();
    void checkFeederParams(const Feeder &feeder, int index);

    const Program &m_program;
    QVector<Diagnostic> m_result;
    QSet<QString> m_packageIds;
    QSet<QString> m_componentIds;
    QHash<QString, int> m_boardIndex;  // Board::id -> first board with it
};

void Checker::add(Diagnostic::Severity severity, const ProgramLocation &location,
                  const char *field, const QString &message, const ProgramLocation &related)
{
    Diagnostic d;
    d.severity = severity;
    d.message = message;
    d.location = location;
    d.field = QLatin1String(field);
    d.related = related;
    m_result.append(d);
}

void Checker::checkId(QHash<QString, ProgramLocation> &seen, const QString &id,
                      const ProgramLocation &location, const QString &what)
{
    if (id.trimmed().isEmpty()) {
        error(location, "id", tr("%1 id 不能为空").arg(what));
        return;
    }
    const auto it = seen.constFind(id);
    if (it != seen.constEnd())
        error(location, "id", tr("%1 id \"%2\" 重复").arg(what, id), *it);
    else
        seen.insert(id, location);
}

bool Checker::finite(const ProgramLocation &location, const char *field, const QString &owner,
                     const QString &label, double value)
{
    if (std::isfinite(value))
        return true;
    error(location, field,
          tr("%1%2（%3）不是有效数值").arg(owner, label, QLatin1String(field)));
    return false;
}

void Checker::positive(const ProgramLocation &location, const char *field, const QString &owner,
                       const QString &label, double value)
{
    if (finite(location, field, owner, label, value) && !(value > 0.0)) {
        error(location, field,
              tr("%1%2（%3）必须大于 0，当前为 %4")
                  .arg(owner, label, QLatin1String(field), QString::number(value)));
    }
}

void Checker::nonNegative(const ProgramLocation &location, const char *field,
                          const QString &owner, const QString &label, double value)
{
    if (finite(location, field, owner, label, value) && value < 0.0) {
        error(location, field,
              tr("%1%2（%3）不能小于 0，当前为 %4")
                  .arg(owner, label, QLatin1String(field), QString::number(value)));
    }
}

void Checker::atLeast(const ProgramLocation &location, const char *field, const QString &owner,
                      const QString &label, int value, int min)
{
    if (value < min) {
        error(location, field,
              tr("%1%2（%3）不能小于 %4，当前为 %5")
                  .arg(owner, label, QLatin1String(field), QString::number(min),
                       QString::number(value)));
    }
}

void Checker::between(const ProgramLocation &location, const char *field, const QString &owner,
                      const QString &label, int value, int min, int max)
{
    if (value < min || value > max) {
        error(location, field,
              tr("%1%2（%3）必须在 %4 到 %5 之间，当前为 %6")
                  .arg(owner, label, QLatin1String(field), QString::number(min),
                       QString::number(max), QString::number(value)));
    }
}

QVector<Diagnostic> Checker::run()
{
    checkPackages();
    checkComponents();
    checkBoards();
    checkPanel();
    checkFeeders();
    return m_result;
}

void Checker::checkPackages()
{
    QHash<QString, ProgramLocation> seen;
    for (int i = 0; i < m_program.packages.size(); ++i) {
        const Package &p = m_program.packages.at(i);
        const ProgramLocation loc = at(Kind::Package, i);
        checkId(seen, p.id, loc, tr("封装"));
        m_packageIds.insert(p.id);

        const QString owner = tr("封装 %1 的").arg(name(p.id));
        positive(loc, "bodyLength", owner, tr("长度"), p.bodyLength);
        positive(loc, "bodyWidth", owner, tr("宽度"), p.bodyWidth);
        positive(loc, "height", owner, tr("高度"), p.height);
        atLeast(loc, "pinCount", owner, tr("引脚数"), p.pinCount, 0);
        finite(at(Kind::PackagePick, i), "pickDepth", owner, tr("取料下压量"),
               p.pick.pickDepth);
        finite(at(Kind::PackagePlace, i), "placeDepth", owner, tr("贴装下压量"),
               p.place.placeDepth);
        between(at(Kind::PackagePlace, i), "speed", owner, tr("贴装速度"), p.place.speed, 1, 100);
        nonNegative(at(Kind::PackageVision, i), "tolerance", owner, tr("视觉容差"),
                    p.vision.tolerance);
    }
}

void Checker::checkComponents()
{
    QHash<QString, ProgramLocation> seen;
    for (int i = 0; i < m_program.components.size(); ++i) {
        const Component &c = m_program.components.at(i);
        const ProgramLocation loc = at(Kind::Component, i);
        checkId(seen, c.id, loc, tr("元件"));
        m_componentIds.insert(c.id);

        if (c.package.isEmpty())
            error(loc, "package", tr("元件 %1 未指定封装").arg(name(c.id)));
        else if (!m_packageIds.contains(c.package))
            error(loc, "package",
                  tr("元件 %1 引用的封装 \"%2\" 不存在").arg(name(c.id), c.package));
    }
}

void Checker::checkFiducials(const QVector<Fiducial> &list, int board, const QString &what,
                             const QString &ownerPrefix)
{
    QHash<QString, ProgramLocation> seen;
    for (int i = 0; i < list.size(); ++i) {
        const Fiducial &f = list.at(i);
        const ProgramLocation loc =
            board < 0 ? at(Kind::PanelFiducial, i) : at(Kind::BoardFiducial, board, i);
        checkId(seen, f.id, loc, what);
        const QString owner = tr("%1Mark 点 %2 的").arg(ownerPrefix, name(f.id));
        finite(loc, "x", owner, tr("X 坐标"), f.x);
        finite(loc, "y", owner, tr("Y 坐标"), f.y);
        positive(loc, "size", owner, tr("尺寸"), f.size);
    }
}

void Checker::checkBoards()
{
    QHash<QString, ProgramLocation> seen;
    for (int i = 0; i < m_program.boards.size(); ++i) {
        const Board &b = m_program.boards.at(i);
        const ProgramLocation loc = at(Kind::Board, i);
        checkId(seen, b.id, loc, tr("单板"));
        if (!m_boardIndex.contains(b.id))
            m_boardIndex.insert(b.id, i);

        const QString owner = tr("单板 %1 的").arg(name(b.id));
        positive(loc, "width", owner, tr("宽度"), b.width);
        positive(loc, "height", owner, tr("长度"), b.height);
        positive(loc, "thickness", owner, tr("厚度"), b.thickness);

        const QString scope = tr("单板 %1 中的").arg(name(b.id));
        checkFiducials(b.fiducials, i, scope + tr("Mark 点"), scope);

        QHash<QString, ProgramLocation> badMarkIds;
        for (int j = 0; j < b.badMarks.size(); ++j) {
            const BadMark &m = b.badMarks.at(j);
            const ProgramLocation mloc = at(Kind::BadMark, i, j);
            checkId(badMarkIds, m.id, mloc, scope + tr("坏板标记"));
            const QString mowner = tr("%1坏板标记 %2 的").arg(scope, name(m.id));
            finite(mloc, "x", mowner, tr("X 坐标"), m.x);
            finite(mloc, "y", mowner, tr("Y 坐标"), m.y);
        }

        QHash<QString, ProgramLocation> placementIds;
        for (int j = 0; j < b.placements.size(); ++j) {
            const Placement &p = b.placements.at(j);
            const ProgramLocation ploc = at(Kind::Placement, i, j);
            checkId(placementIds, p.id, ploc, scope + tr("贴片位"));

            if (p.component.isEmpty())
                error(ploc, "component", tr("贴片位 %1 未指定元件").arg(name(p.id)));
            else if (!m_componentIds.contains(p.component))
                error(ploc, "component",
                      tr("贴片位 %1 引用的元件 \"%2\" 不存在").arg(name(p.id), p.component));

            const QString powner = tr("贴片位 %1 的").arg(name(p.id));
            const bool xOk = finite(ploc, "x", powner, tr("X 坐标"), p.x);
            const bool yOk = finite(ploc, "y", powner, tr("Y 坐标"), p.y);
            finite(ploc, "rotation", powner, tr("角度"), p.rotation);
            // Some parts overhang the board edge on purpose, so only a warning.
            if (xOk && yOk && b.width > 0.0 && b.height > 0.0
                && (p.x < 0.0 || p.y < 0.0 || p.x > b.width || p.y > b.height)) {
                add(Diagnostic::Severity::Warning, ploc, "",
                    tr("贴片位 %1 的坐标 (%2, %3) 在单板 %4 之外")
                        .arg(p.id, QString::number(p.x), QString::number(p.y), name(b.id)));
            }
        }
    }
}

void Checker::checkPanel()
{
    const Panel &panel = m_program.panel;
    const ProgramLocation loc = at(Kind::Panel);
    const QString owner = tr("拼板的");
    positive(loc, "width", owner, tr("宽度"), panel.width);
    positive(loc, "height", owner, tr("长度"), panel.height);
    checkFiducials(panel.fiducials, -1, tr("拼板 Mark 点"), tr("拼板"));

    if (panel.instances.isEmpty())
        error(loc, "", tr("拼板至少需要一个子板实例（BoardInstance）"));

    QHash<QString, ProgramLocation> seen;
    for (int i = 0; i < panel.instances.size(); ++i) {
        const BoardInstance &bi = panel.instances.at(i);
        const ProgramLocation iloc = at(Kind::BoardInstance, i);
        checkId(seen, bi.id, iloc, tr("子板实例"));
        const QString iowner = tr("子板实例 %1 的").arg(name(bi.id));
        finite(iloc, "x", iowner, tr("X 坐标"), bi.x);
        finite(iloc, "y", iowner, tr("Y 坐标"), bi.y);
        finite(iloc, "rotation", iowner, tr("角度"), bi.rotation);

        const Board *board = nullptr;
        if (bi.board.isEmpty()) {
            error(iloc, "board", tr("子板实例 %1 未指定单板").arg(name(bi.id)));
        } else {
            const auto b = m_boardIndex.constFind(bi.board);
            if (b == m_boardIndex.constEnd())
                error(iloc, "board",
                      tr("子板实例 %1 引用的单板 \"%2\" 不存在").arg(name(bi.id), bi.board));
            else
                board = &m_program.boards.at(*b);
        }

        QSet<QString> skipped;
        for (int j = 0; j < bi.skippedPlacements.size(); ++j) {
            const QString &placement = bi.skippedPlacements.at(j);
            const ProgramLocation sloc = at(Kind::Skip, i, j);
            if (placement.isEmpty()) {
                error(sloc, "placement", tr("子板实例 %1 的跳过位号为空").arg(name(bi.id)));
                continue;
            }
            if (skipped.contains(placement)) {
                error(sloc, "placement",
                      tr("子板实例 %1 重复跳过位号 %2").arg(name(bi.id), placement));
                continue;
            }
            skipped.insert(placement);
            // An unknown board is reported above; nothing to look the id up in.
            if (board
                && std::none_of(board->placements.cbegin(), board->placements.cend(),
                                [&](const Placement &p) { return p.id == placement; })) {
                error(sloc, "placement",
                      tr("子板实例 %1 跳过的位号 \"%2\" 在单板 %3 中不存在")
                          .arg(name(bi.id), placement, bi.board));
            }
        }
    }
}

void Checker::checkFeeders()
{
    if (!m_program.feederSetup)
        return;
    QHash<QString, ProgramLocation> seen;
    const QVector<Feeder> &feeders = m_program.feederSetup->feeders;
    for (int i = 0; i < feeders.size(); ++i) {
        const Feeder &f = feeders.at(i);
        const ProgramLocation loc = at(Kind::Feeder, i);
        checkId(seen, f.id, loc, tr("送料器"));

        if (f.component.isEmpty())
            error(loc, "component", tr("送料器 %1 未指定元件").arg(name(f.id)));
        else if (!m_componentIds.contains(f.component))
            error(loc, "component",
                  tr("送料器 %1 引用的元件 \"%2\" 不存在").arg(name(f.id), f.component));

        const QString owner = tr("送料器 %1 的").arg(name(f.id));
        finite(loc, "pickX", owner, tr("取料 X 坐标"), f.pickX);
        finite(loc, "pickY", owner, tr("取料 Y 坐标"), f.pickY);
        finite(loc, "pickZ", owner, tr("取料 Z 坐标"), f.pickZ);
        finite(loc, "rotation", owner, tr("角度"), f.rotation);
        checkFeederParams(f, i);
    }
}

void Checker::checkFeederParams(const Feeder &f, int index)
{
    const ProgramLocation loc = at(Kind::FeederParams, index);
    const QString owner = tr("送料器 %1 的").arg(name(f.id));
    switch (f.type) {
    case FeederType::Tape:
        positive(loc, "width", owner, tr("料带宽度"), f.tape.width);
        positive(loc, "pitch", owner, tr("料距"), f.tape.pitch);
        atLeast(loc, "feedCount", owner, tr("每次送料数"), f.tape.feedCount, 1);
        break;
    case FeederType::Tray: {
        const TrayParams &t = f.tray;
        atLeast(loc, "rows", owner, tr("料盘行数"), t.rows, 1);
        atLeast(loc, "columns", owner, tr("料盘列数"), t.columns, 1);
        positive(loc, "rowPitch", owner, tr("料盘行距"), t.rowPitch);
        positive(loc, "columnPitch", owner, tr("料盘列距"), t.columnPitch);
        const qint64 pockets = qint64(t.rows) * t.columns;
        if (t.startIndex < 0 || (pockets > 0 && t.startIndex >= pockets)) {
            error(loc, "startIndex",
                  tr("%1料盘起始穴位（startIndex）为 %2，超出范围，应在 0 到 %3 之间")
                      .arg(owner, QString::number(t.startIndex),
                           QString::number(qMax<qint64>(0, pockets - 1))));
        }
        break;
    }
    case FeederType::Tube:
        nonNegative(loc, "pitch", owner, tr("料管间距"), f.tube.pitch);
        break;
    case FeederType::Bulk:
    case FeederType::Unknown:
        break;
    }
}

}  // namespace

QVector<Diagnostic> ProgramValidator::validate(const Program &program)
{
    return Checker(program).run();
}
