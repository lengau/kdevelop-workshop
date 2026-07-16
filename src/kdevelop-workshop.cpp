#include "kdevelop-workshop.h"
#include "runtime/workshopruntime.h"
#include "config/workshopconfigpage.h"
#include "views/workshoptoolview.h"
#include "views/workshopterminaltoolview.h"
#include <sublime/view.h>
#include <sublime/document.h>

#include <debug.h>
#include <KPluginFactory>
#include <interfaces/icore.h>
#include <interfaces/iruntimecontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iproject.h>
#include <interfaces/iuicontroller.h>
#include <util/path.h>
#include <QProcess>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <functional>
#include "api/workshopapi.h"

class WorkshopToolViewFactory : public KDevelop::IToolViewFactory
{
public:
    explicit WorkshopToolViewFactory(kdevelop_workshop* plugin)
        : m_plugin(plugin)
    {
    }
    QWidget* create(QWidget* parent = nullptr) override
    {
        qCDebug(PLUGIN_KDEVELOP_WORKSHOP) << "WorkshopToolViewFactory::create";
        return new WorkshopToolView(m_plugin, parent);
    }
    QString id() const override
    {
        return QStringLiteral("org.kdevelop.WorkshopToolView");
    }
    Qt::DockWidgetArea defaultPosition() const override
    {
        return Qt::RightDockWidgetArea;
    }
    void viewCreated(Sublime::View* view) override
    {
        if (view && view->document()) {
            view->document()->setStatusIcon(QIcon::fromTheme(QStringLiteral("services")));
        }
    }

private:
    kdevelop_workshop* m_plugin;
};

class WorkshopTerminalToolViewFactory : public KDevelop::IToolViewFactory
{
public:
    explicit WorkshopTerminalToolViewFactory(kdevelop_workshop* plugin)
        : m_plugin(plugin)
    {
    }
    QWidget* create(QWidget* parent = nullptr) override
    {
        qCDebug(PLUGIN_KDEVELOP_WORKSHOP) << "WorkshopTerminalToolViewFactory::create";
        return new WorkshopTerminalToolView(m_plugin, parent);
    }
    QString id() const override
    {
        return QStringLiteral("org.kdevelop.WorkshopTerminalToolView");
    }
    Qt::DockWidgetArea defaultPosition() const override
    {
        return Qt::BottomDockWidgetArea;
    }
    void viewCreated(Sublime::View* view) override
    {
        if (view && view->document()) {
            view->document()->setStatusIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));
        }
    }

private:
    kdevelop_workshop* m_plugin;
};

K_PLUGIN_FACTORY_WITH_JSON(kdevelop_workshopFactory, "kdevelop-workshop.json", registerPlugin<kdevelop_workshop>();)

kdevelop_workshop::kdevelop_workshop(QObject* parent, const KPluginMetaData& metaData, const QVariantList& args)
    : KDevelop::IPlugin(QStringLiteral("kdevelop-workshop"), parent, metaData)
{
    Q_UNUSED(args);

    qCInfo(PLUGIN_KDEVELOP_WORKSHOP) << "Plugin loaded from:" << metaData.fileName();

    connect(KDevelop::ICore::self()->projectController(), &KDevelop::IProjectController::projectOpened, this,
            &kdevelop_workshop::projectOpened);

    QTimer::singleShot(0, this, [this]() {
        qCDebug(PLUGIN_KDEVELOP_WORKSHOP) << "Registering Workshop tool view";
        m_factory = new WorkshopToolViewFactory(this);
        KDevelop::ICore::self()->uiController()->addToolView(QStringLiteral("Workshop"), m_factory,
                                                             KDevelop::IUiController::CreateAndRaise);

        qCDebug(PLUGIN_KDEVELOP_WORKSHOP) << "Registering Workshop Terminal tool view";
        m_terminalFactory = new WorkshopTerminalToolViewFactory(this);
        KDevelop::ICore::self()->uiController()->addToolView(QStringLiteral("Workshop Terminal"), m_terminalFactory,
                                                             KDevelop::IUiController::CreateAndRaise);
    });

    const auto projects = KDevelop::ICore::self()->projectController()->projects();
    for (KDevelop::IProject* proj : projects) {
        addWorkshopsForProject(proj);
    }
}

void kdevelop_workshop::unload()
{
    if (m_factory) {
        KDevelop::ICore::self()->uiController()->removeToolView(m_factory);
        m_factory = nullptr;
    }
    if (m_terminalFactory) {
        KDevelop::ICore::self()->uiController()->removeToolView(m_terminalFactory);
        m_terminalFactory = nullptr;
    }
    KDevelop::IPlugin::unload();
}

void kdevelop_workshop::projectOpened(KDevelop::IProject* project)
{
    addWorkshopsForProject(project);
}

void kdevelop_workshop::addWorkshopsForProject(KDevelop::IProject* project)
{
    QString projectPath = project->path().toLocalFile();
    auto extractProjectId = [projectPath](const QJsonDocument& doc) {
        if (doc.isEmpty()) {
            return QString();
        }

        const QJsonArray projects = doc.object().value(QStringLiteral("result")).toArray();
        for (const QJsonValue& val : projects) {
            const QJsonObject proj = val.toObject();
            if (proj.value(QStringLiteral("path")).toString() == projectPath) {
                return proj.value(QStringLiteral("id")).toString();
            }
        }
        return QString();
    };

    std::function<void(const QString&)> fetchAndRegisterWorkshops;
    fetchAndRegisterWorkshops = [this, projectPath](const QString& projectId) {
        if (projectId.isEmpty()) {
            return;
        }

        WorkshopApi::queryAsync(
            QStringLiteral("/v1/projects/%1/workshops").arg(projectId), this,
            [this, projectPath](const QJsonDocument& workshopsDoc) {
                if (workshopsDoc.isEmpty()) {
                    return;
                }

                const QJsonObject result = workshopsDoc.object().value(QStringLiteral("result")).toObject();
                const QJsonArray workshops = result.value(QStringLiteral("workshops")).toArray();
                const QJsonArray files = result.value(QStringLiteral("files")).toArray();

                QStringList workshopNames;
                for (const QJsonValue& val : workshops) {
                    const QString workshopName = val.toObject().value(QStringLiteral("name")).toString();
                    if (!workshopName.isEmpty() && !workshopNames.contains(workshopName)) {
                        workshopNames << workshopName;
                    }
                }
                for (const QJsonValue& val : files) {
                    const QString workshopName = val.toObject().value(QStringLiteral("name")).toString();
                    if (!workshopName.isEmpty() && !workshopNames.contains(workshopName)) {
                        workshopNames << workshopName;
                    }
                }

                for (const QString& name : workshopNames) {
                    bool alreadyExists = false;
                    const auto runtimes = KDevelop::ICore::self()->runtimeController()->availableRuntimes();
                    for (auto* rt : runtimes) {
                        if (rt->name() == QStringLiteral("Workshop: %1").arg(name)) {
                            alreadyExists = true;
                            break;
                        }
                    }
                    if (!alreadyExists) {
                        KDevelop::ICore::self()->runtimeController()->addRuntimes(
                            new WorkshopRuntime(name, projectPath, this));
                    }
                }
            });
    };

    WorkshopApi::queryAsync(
        QStringLiteral("/v1/projects"), this,
        [this, projectPath, extractProjectId, fetchAndRegisterWorkshops](const QJsonDocument& doc) {
            if (doc.isEmpty()) {
                qCWarning(PLUGIN_KDEVELOP_WORKSHOP)
                    << "Failed to connect to /v1/projects (daemon socket activation timeout?)";
                return;
            }

            QString projectId = extractProjectId(doc);
            if (!projectId.isEmpty()) {
                fetchAndRegisterWorkshops(projectId);
                return;
            }

            QDir dir(projectPath);
            if (!dir.exists(QStringLiteral(".workshop"))) {
                return;
            }

            QJsonObject req;
            req.insert(QStringLiteral("path"), projectPath);
            WorkshopApi::queryAsync(
                QStringLiteral("/v1/projects"), QJsonDocument(req).toJson(QJsonDocument::Compact),
                QStringLiteral("POST"), this,
                [this, extractProjectId, fetchAndRegisterWorkshops](const QJsonDocument& regDoc) {
                    if (regDoc.isEmpty()) {
                        return;
                    }

                    WorkshopApi::queryAsync(
                        QStringLiteral("/v1/projects"), this,
                        [extractProjectId, fetchAndRegisterWorkshops](const QJsonDocument& newProjectsDoc) {
                            fetchAndRegisterWorkshops(extractProjectId(newProjectsDoc));
                        });
                });
        });
}

KDevelop::ConfigPage* kdevelop_workshop::configPage(int number, QWidget* parent)
{
    if (number == 0) {
        return new WorkshopConfigPage(this, parent);
    }
    return nullptr;
}

int kdevelop_workshop::configPages() const
{
    return 1;
}

#include "kdevelop-workshop.moc"
