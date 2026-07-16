#include "kdevelop-workshop.h"
#include "workshopruntime.h"
#include "workshopconfigpage.h"
#include "workshoptoolview.h"
#include "workshopterminaltoolview.h"
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
#include <QThread>
#include <QDir>
#include "workshopapi.h"

class WorkshopToolViewFactory : public KDevelop::IToolViewFactory
{
public:
    explicit WorkshopToolViewFactory(kdevelop_workshop* plugin)
        : m_plugin(plugin)
    {
    }
    QWidget* create(QWidget* parent = nullptr) override
    {
        qWarning() << "=================> WorkshopToolViewFactory::create called! <=================";
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
        qWarning() << "=================> WorkshopTerminalToolViewFactory::create called! <=================";
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

    qWarning() << "=================> Workshop plugin constructor called! <=================";

    connect(KDevelop::ICore::self()->projectController(), &KDevelop::IProjectController::projectOpened, this,
            &kdevelop_workshop::projectOpened);

    QTimer::singleShot(0, this, [this]() {
        qWarning() << "=================> Registering Workshop ToolView <=================";
        m_factory = new WorkshopToolViewFactory(this);
        KDevelop::ICore::self()->uiController()->addToolView(QStringLiteral("Workshop"), m_factory,
                                                             KDevelop::IUiController::CreateAndRaise);

        qWarning() << "=================> Registering Workshop Terminal ToolView <=================";
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

    // Query the API in a background thread to prevent freezing the GUI on startup
    auto* thread = QThread::create([this, projectPath]() {
        QJsonDocument doc = WorkshopApi::query(QStringLiteral("/v1/projects"));
        if (doc.isEmpty()) {
            qWarning() << "Workshop API: Failed to connect to /v1/projects (daemon socket activation timeout?)";
            return;
        }

        QJsonArray projects = doc.object().value(QStringLiteral("result")).toArray();
        QString projectId;
        for (const QJsonValue& val : projects) {
            QJsonObject proj = val.toObject();
            if (proj.value(QStringLiteral("path")).toString() == projectPath) {
                projectId = proj.value(QStringLiteral("id")).toString();
                break;
            }
        }

        // If project is not registered but has a .workshop directory, register it automatically
        if (projectId.isEmpty()) {
            QDir dir(projectPath);
            if (dir.exists(QStringLiteral(".workshop"))) {
                QJsonObject req;
                req.insert(QStringLiteral("path"), projectPath);
                QJsonDocument reqDoc(req);
                QJsonDocument regDoc = WorkshopApi::query(
                    QStringLiteral("/v1/projects"), reqDoc.toJson(QJsonDocument::Compact), QStringLiteral("POST"));

                // Re-query projects list to get the newly generated project ID
                if (!regDoc.isEmpty()) {
                    QJsonDocument newProjectsDoc = WorkshopApi::query(QStringLiteral("/v1/projects"));
                    if (!newProjectsDoc.isEmpty()) {
                        QJsonArray newProjects = newProjectsDoc.object().value(QStringLiteral("result")).toArray();
                        for (const QJsonValue& val : newProjects) {
                            QJsonObject proj = val.toObject();
                            if (proj.value(QStringLiteral("path")).toString() == projectPath) {
                                projectId = proj.value(QStringLiteral("id")).toString();
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (projectId.isEmpty())
            return;

        QJsonDocument workshopsDoc = WorkshopApi::query(QStringLiteral("/v1/projects/%1/workshops").arg(projectId));
        if (workshopsDoc.isEmpty())
            return;

        QJsonObject result = workshopsDoc.object().value(QStringLiteral("result")).toObject();
        QJsonArray workshops = result.value(QStringLiteral("workshops")).toArray();
        QJsonArray files = result.value(QStringLiteral("files")).toArray();

        // Deduplicate and collect workshop names from both running instances and file declarations
        QStringList workshopNames;
        for (const QJsonValue& val : workshops) {
            QJsonObject ws = val.toObject();
            QString workshopName = ws.value(QStringLiteral("name")).toString();
            if (!workshopName.isEmpty() && !workshopNames.contains(workshopName)) {
                workshopNames << workshopName;
            }
        }
        for (const QJsonValue& val : files) {
            QJsonObject f = val.toObject();
            QString workshopName = f.value(QStringLiteral("name")).toString();
            if (!workshopName.isEmpty() && !workshopNames.contains(workshopName)) {
                workshopNames << workshopName;
            }
        }

        // Return to the main thread to register the runtimes safely
        QMetaObject::invokeMethod(
            this,
            [this, projectPath, workshopNames]() {
                for (const QString& name : workshopNames) {
                    // Ensure we don't register duplicate runtimes if they're already present
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
            },
            Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
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
