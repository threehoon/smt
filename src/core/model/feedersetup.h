#pragma once

#include "model/feeder.h"

#include <QString>
#include <QVector>

// Which feeder holds which component on a given machine model.
struct FeederSetup
{
    QString machineModel;
    QVector<Feeder> feeders;
};

inline bool operator==(const FeederSetup &a, const FeederSetup &b)
{
    return a.machineModel == b.machineModel && a.feeders == b.feeders;
}
