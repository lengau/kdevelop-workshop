#ifndef PARSESKETCHSDK_H
#define PARSESKETCHSDK_H

#include "sketchsdkdata.h"
#include <QStringList>

SketchSdkData parseSketchSdk(const QStringList &lines);
SketchSdkData parseSketchSdkMap(const QStringList &lines);
QString serializeSketchSdk(const SketchSdkData &data);

#endif // PARSESKETCHSDK_H
