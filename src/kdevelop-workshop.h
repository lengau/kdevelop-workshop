#ifndef KDEVELOP_WORKSHOP_H
#define KDEVELOP_WORKSHOP_H

#include <interfaces/iplugin.h>

namespace KDevelop {
class IProject;
}

class WorkshopToolViewFactory;
class WorkshopTerminalToolViewFactory;

class kdevelop_workshop : public KDevelop::IPlugin
{
    Q_OBJECT

public:
    // KPluginFactory-based plugin wants constructor with this signature
    kdevelop_workshop(QObject* parent, const KPluginMetaData& metaData, const QVariantList& args);

    int configPages() const override;
    KDevelop::ConfigPage* configPage(int number, QWidget* parent) override;

    void unload() override;

private Q_SLOTS:
    void projectOpened(KDevelop::IProject* project);

Q_SIGNALS:
    void workshopsRefreshed();

private:
    void addWorkshopsForProject(KDevelop::IProject* project);

    WorkshopToolViewFactory* m_factory = nullptr;
    WorkshopTerminalToolViewFactory* m_terminalFactory = nullptr;
};

#endif // KDEVELOP_WORKSHOP_H
