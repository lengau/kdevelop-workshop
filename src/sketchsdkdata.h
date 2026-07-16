#ifndef SKETCHSDKDATA_H
#define SKETCHSDKDATA_H

#include <QMap>
#include <QString>

struct SketchSdkData {
    struct Plug {
        QString interfaceName = QStringLiteral("mount");
        QString workshopTarget;
    };

    struct Slot {
        QString interfaceName = QStringLiteral("tunnel");
        int endpoint = 8080;
    };

    QString setupBase;
    QString setupProject;
    QMap<QString, Plug> plugs; // name -> plug details
    QMap<QString, Slot> slots; // name -> slot details
};

#endif // SKETCHSDKDATA_H
