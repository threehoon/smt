#pragma once

#include <QString>

// A part number. Says what is placed; physical and process data live on Package.
struct Component
{
    QString id;       // usually the part number
    QString package;  // Package::id
    QString value;
    QString description;
};

inline bool operator==(const Component &a, const Component &b)
{
    return a.id == b.id && a.package == b.package && a.value == b.value
        && a.description == b.description;
}
