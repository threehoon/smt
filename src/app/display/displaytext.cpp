#include "display/displaytext.h"

QString DisplayText::side(Side side)
{
    switch (side) {
    case Side::Top:
        return tr("顶面");
    case Side::Bottom:
        return tr("底面");
    }
    return {};
}

QString DisplayText::fiducialShape(FiducialShape shape)
{
    switch (shape) {
    case FiducialShape::Circle:
        return tr("圆形");
    case FiducialShape::Square:
        return tr("方形");
    case FiducialShape::Cross:
        return tr("十字");
    }
    return {};
}

QString DisplayText::feederType(const Feeder &feeder)
{
    switch (feeder.type) {
    case FeederType::Tape:
        return tr("料带");
    case FeederType::Tray:
        return tr("料盘");
    case FeederType::Tube:
        return tr("料管");
    case FeederType::Bulk:
        return tr("散料");
    case FeederType::Unknown:
        return tr("%1（未知）").arg(feeder.unknownType);
    }
    return {};
}
