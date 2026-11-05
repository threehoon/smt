#pragma once

#include "model/feeder.h"
#include "model/fiducial.h"
#include "model/placement.h"

#include <QCoreApplication>
#include <QString>

// User-visible names of model enums. The file spellings live in
// core/io/xmlnames.h; these are translated display texts.
class DisplayText
{
    Q_DECLARE_TR_FUNCTIONS(DisplayText)

public:
    static QString side(Side side);
    static QString fiducialShape(FiducialShape shape);
    // Shows the original type string for a type this version does not know.
    static QString feederType(const Feeder &feeder);
};
