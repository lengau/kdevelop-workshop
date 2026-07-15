#include "kdevelop-workshop.h"

#include <debug.h>

#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(kdevelop_workshopFactory, "kdevelop-workshop.json", registerPlugin<kdevelop_workshop>(); )

kdevelop_workshop::kdevelop_workshop(QObject* parent, const KPluginMetaData& metaData, const QVariantList& args)
    : KDevelop::IPlugin(QStringLiteral("kdevelop-workshop"), parent, metaData)
{
    Q_UNUSED(args);

    qCDebug(PLUGIN_KDEVELOP_WORKSHOP) << "Hello world, my plugin is loaded!";
}

// needed for QObject class created from K_PLUGIN_FACTORY_WITH_JSON
#include "kdevelop-workshop.moc"
#include "moc_kdevelop-workshop.cpp"
