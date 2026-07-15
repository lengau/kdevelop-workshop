#ifndef KDEVELOP_WORKSHOP_H
#define KDEVELOP_WORKSHOP_H

#include <interfaces/iplugin.h>

class kdevelop_workshop : public KDevelop::IPlugin
{
    Q_OBJECT

public:
    // KPluginFactory-based plugin wants constructor with this signature
    kdevelop_workshop(QObject* parent, const KPluginMetaData& metaData, const QVariantList& args);
};

#endif // KDEVELOP_WORKSHOP_H
