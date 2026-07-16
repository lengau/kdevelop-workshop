#include "workshoptoolview.h"
#include "kdevelop-workshop.h"
#include "api/workshopapi.h"
#include "wizard/workshopwizard.h"
#include "sketchsdk/sketchsdkpanel.h"
#include "sketchsdk/parsesketchsdk.h"
#include "views/workshopcontextmenu.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QProcess>
#include <QLabel>
#include <QScrollArea>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QIcon>
#include <QTimer>
#include <QSplitter>
#include <QMenu>
#include <KMessageBox>
#include <KStandardGuiItem>
#include <KLocalizedString>
#include <QFile>
#include <QDir>
#include <QPointer>
#include <QStandardPaths>
#include <interfaces/icore.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iproject.h>
#include <util/path.h>

WorkshopToolView::WorkshopToolView(kdevelop_workshop* plugin, QWidget* parent)
    : QWidget(parent)
    , m_plugin(plugin)
    , m_refreshing(false)
{
    setWindowIcon(QIcon::fromTheme(QStringLiteral("services")));

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Use QSplitter to make the console output resizable
    auto* splitter = new QSplitter(Qt::Vertical, this);
    mainLayout->addWidget(splitter);

    // Top Container Widget (Controls & List)
    auto* topWidget = new QWidget(this);
    auto* topLayout = new QVBoxLayout(topWidget);
    topLayout->setContentsMargins(4, 4, 4, 4);

    // Project Selection
    topLayout->addWidget(new QLabel(QStringLiteral("Project:"), topWidget));
    m_projectCombo = new QComboBox(topWidget);
    connect(m_projectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &WorkshopToolView::onProjectChanged);
    topLayout->addWidget(m_projectCombo);

    // Workshops List Header
    topLayout->addWidget(new QLabel(QStringLiteral("Workshops:"), topWidget));

    // Scroll Area for Workshops
    auto* scrollArea = new QScrollArea(topWidget);
    scrollArea->setWidgetResizable(true);
    m_workshopsContainer = new QWidget(scrollArea);
    m_workshopsLayout = new QVBoxLayout(m_workshopsContainer);
    m_workshopsLayout->addStretch();
    scrollArea->setWidget(m_workshopsContainer);
    topLayout->addWidget(scrollArea);

    // Control Buttons
    auto* btnLayout = new QHBoxLayout();

    auto* refreshBtn = new QPushButton(QStringLiteral("Refresh"), topWidget);
    connect(refreshBtn, &QPushButton::clicked, this, &WorkshopToolView::refresh);
    btnLayout->addWidget(refreshBtn);

    auto* initBtn = new QPushButton(QStringLiteral("Init Workshop"), topWidget);
    connect(initBtn, &QPushButton::clicked, this, [this]() {
        QString projectPath = m_projectCombo->currentData().toString();
        if (projectPath.isEmpty())
            return;

        auto* wizard = new WorkshopWizard(projectPath, QString(), this);
        if (wizard->exec() == QDialog::Accepted) {
            m_output->setText(QStringLiteral("Workshop configuration initialized successfully."));
            refresh();
        }
    });
    btnLayout->addWidget(initBtn);

    topLayout->addLayout(btnLayout);
    splitter->addWidget(topWidget);

    // Bottom Container Widget (Console Output)
    auto* bottomWidget = new QWidget(this);
    auto* bottomLayout = new QVBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(4, 4, 4, 4);

    bottomLayout->addWidget(new QLabel(QStringLiteral("Console Output:"), bottomWidget));
    m_output = new QTextEdit(bottomWidget);
    m_output->setReadOnly(true);
    bottomLayout->addWidget(m_output);

    splitter->addWidget(bottomWidget);

    // Give top widget 75% and bottom widget 25% height by default
    splitter->setSizes({375, 125});

    // Setup animation timer for transitioning workshops (dots loader animation)
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(500);
    connect(m_animationTimer, &QTimer::timeout, this, &WorkshopToolView::animateTransitions);

    // Setup polling timer (checks every 2 seconds when transition is active)
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, &WorkshopToolView::refresh);

    // Automatically refresh the list when projects are opened or closed
    connect(KDevelop::ICore::self()->projectController(), &KDevelop::IProjectController::projectOpened, this,
            [this](KDevelop::IProject*) {
                refresh();
            });
    connect(KDevelop::ICore::self()->projectController(), &KDevelop::IProjectController::projectClosed, this,
            [this](KDevelop::IProject*) {
                refresh();
            });

    refresh();
}

void WorkshopToolView::clearLayout()
{
    QLayoutItem* child;
    while ((child = m_workshopsLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    m_workshopsLayout->addStretch();
    m_workshopWidgets.clear();
}

void WorkshopToolView::refresh()
{
    if (m_refreshing)
        return;
    m_refreshing = true;

    m_projectCombo->blockSignals(true);

    QString currentSelectedPath = m_projectCombo->currentData().toString();
    m_projectCombo->clear();

    const auto projects = KDevelop::ICore::self()->projectController()->projects();
    int selectIndex = 0;
    int index = 0;
    for (auto* p : projects) {
        QString path = p->path().toLocalFile();
        m_projectCombo->addItem(p->name(), path);
        if (path == currentSelectedPath) {
            selectIndex = index;
        }
        index++;
    }

    if (m_projectCombo->count() > 0) {
        m_projectCombo->setCurrentIndex(selectIndex);
    }

    m_projectCombo->blockSignals(false);

    onProjectChanged(m_projectCombo->currentIndex());
}

void WorkshopToolView::onProjectChanged(int index)
{
    if (index < 0 || m_projectCombo->count() == 0) {
        clearLayout();
        m_workshopsLayout->insertWidget(0, new QLabel(QStringLiteral("No open projects."), m_workshopsContainer));
        m_refreshing = false;
        return;
    }

    QString projectPath = m_projectCombo->itemData(index).toString();

    // Only display the connecting loader if there is currently no list rendered (avoids flickering on updates)
    bool listWasEmpty = m_workshopWidgets.isEmpty();
    QPointer<QLabel> loadingLabel = nullptr;
    if (listWasEmpty) {
        clearLayout();
        loadingLabel = new QLabel(QStringLiteral("Connecting to workshopd API..."), m_workshopsContainer);
        m_workshopsLayout->insertWidget(0, loadingLabel);
    }

    WorkshopApi::queryAsync(
        QStringLiteral("/v1/projects"), this, [this, projectPath, loadingLabel](const QJsonDocument& doc) {
            QJsonArray projects;
            QString projectId;
            if (!doc.isEmpty()) {
                projects = doc.object().value(QStringLiteral("result")).toArray();
                for (const QJsonValue& val : projects) {
                    QJsonObject proj = val.toObject();
                    if (proj.value(QStringLiteral("path")).toString() == projectPath) {
                        projectId = proj.value(QStringLiteral("id")).toString();
                        break;
                    }
                }
            }

            if (projectId.isEmpty()) {
                m_projectId = projectId;
                if (loadingLabel) {
                    delete loadingLabel;
                }
                m_refreshing = false;
                if (doc.isEmpty()) {
                    clearLayout();
                    m_workshopsLayout->insertWidget(
                        0, new QLabel(QStringLiteral("Failed to connect to workshopd API."), m_workshopsContainer));
                } else {
                    clearLayout();
                    m_workshopsLayout->insertWidget(
                        0,
                        new QLabel(QStringLiteral("No workshops registered for this project."), m_workshopsContainer));
                }
                m_animationTimer->stop();
                m_pollTimer->stop();
                return;
            }

            WorkshopApi::queryAsync(
                QStringLiteral("/v1/projects/%1/workshops").arg(projectId), this,
                [this, projectId, loadingLabel](const QJsonDocument& workshopsDoc) {
                    QJsonArray workshops;
                    QJsonArray files;
                    bool success = false;
                    if (!workshopsDoc.isEmpty()) {
                        QJsonObject result = workshopsDoc.object().value(QStringLiteral("result")).toObject();
                        workshops = result.value(QStringLiteral("workshops")).toArray();
                        files = result.value(QStringLiteral("files")).toArray();
                        success = true;
                    }

                    // Return to GUI thread to populate the layout
                    m_projectId = projectId;
                    if (loadingLabel) {
                        delete loadingLabel;
                    }
                    m_refreshing = false;

                    if (!success) {
                        clearLayout();
                        m_workshopsLayout->insertWidget(
                            0, new QLabel(QStringLiteral("Failed to retrieve workshops list."), m_workshopsContainer));
                        m_animationTimer->stop();
                        m_pollTimer->stop();
                        return;
                    }
                    if (workshops.isEmpty() && files.isEmpty()) {
                        clearLayout();
                        m_workshopsLayout->insertWidget(
                            0,
                            new QLabel(QStringLiteral("No workshops found. Click 'Init' to create one."),
                                       m_workshopsContainer));
                        m_animationTimer->stop();
                        m_pollTimer->stop();
                        return;
                    }

                    // Safely clear the old layout only now, right before populating the new elements
                    clearLayout();

                    // Collect active workshop names to distinguish between started/stopped vs not yet launched
                    QStringList activeNames;
                    for (const QJsonValue& val : workshops) {
                        activeNames << val.toObject().value(QStringLiteral("name")).toString();
                    }

                    // Merge and deduplicate active workshops and defined files
                    QMap<QString, QJsonObject> workshopMap;

                    // 1. Populate from files as "Off" by default
                    for (const QJsonValue& val : files) {
                        QJsonObject f = val.toObject();
                        QString name = f.value(QStringLiteral("name")).toString();
                        if (!name.isEmpty()) {
                            QJsonObject ws;
                            ws.insert(QStringLiteral("name"), name);
                            ws.insert(QStringLiteral("status"), QStringLiteral("Off"));
                            workshopMap.insert(name, ws);
                        }
                    }

                    // 2. Supplement/Overwrite with running instances (normalizing status string values)
                    for (const QJsonValue& val : workshops) {
                        QJsonObject ws = val.toObject();
                        QString name = ws.value(QStringLiteral("name")).toString();
                        if (!name.isEmpty()) {
                            QString rawStatus = ws.value(QStringLiteral("status")).toString().trimmed();
                            QString normalizedStatus = QStringLiteral("Off");

                            if (rawStatus == QLatin1String("running")) {
                                normalizedStatus = QStringLiteral("Running");
                            } else if (rawStatus == QLatin1String("ready")) {
                                normalizedStatus = QStringLiteral("Ready");
                            } else if (rawStatus == QLatin1String("stopped")) {
                                normalizedStatus = QStringLiteral("Stopped");
                            } else if (rawStatus.isEmpty()) {
                                normalizedStatus = QStringLiteral("Off");
                            } else {
                                // Keep raw status (e.g. "starting", "stopping", etc.) but capitalize it for display
                                normalizedStatus = rawStatus;
                                if (!normalizedStatus.isEmpty()) {
                                    normalizedStatus[0] = normalizedStatus[0].toUpper();
                                }
                            }

                            ws.insert(QStringLiteral("status"), normalizedStatus);
                            workshopMap.insert(name, ws);
                        }
                    }

                    int insertIdx = 0;
                    for (auto it = workshopMap.begin(); it != workshopMap.end(); ++it) {
                        QJsonObject ws = it.value();
                        QString name = ws.value(QStringLiteral("name")).toString();
                        QString status = ws.value(QStringLiteral("status")).toString();

                        // If it exists in 'files' but not in 'workshops', it needs to be launched first
                        bool needsLaunch = !activeNames.contains(name);

                        // REST states classification
                        bool isRunningState = (status == QLatin1String("Ready") || status == QLatin1String("Running"));
                        bool isStoppedState = (status == QLatin1String("Off") || status == QLatin1String("Stopped"));
                        bool isTransitioning = (!isRunningState && !isStoppedState);

                        if (isTransitioning) {
                            if (!m_transitioningWorkshops.contains(name)) {
                                m_transitioningWorkshops.insert(name, status);
                            }
                        } else {
                            // Remove if it completed transitioning
                            m_transitioningWorkshops.remove(name);
                        }

                        auto* rowWidget = new QWidget(m_workshopsContainer);
                        rowWidget->setContextMenuPolicy(Qt::CustomContextMenu);
                        connect(rowWidget, &QWidget::customContextMenuRequested, this, [this, name](const QPoint& pos) {
                            showContextMenu(name, pos);
                        });

                        auto* rowLayout = new QHBoxLayout(rowWidget);
                        rowLayout->setContentsMargins(0, 4, 0, 4);

                        auto* nameLabel = new QLabel(name, rowWidget);
                        QFont f = nameLabel->font();
                        f.setBold(true);
                        nameLabel->setFont(f);
                        rowLayout->addWidget(nameLabel);

                        auto* statusLabel = new QLabel(status, rowWidget);
                        rowLayout->addWidget(statusLabel);

                        auto* actionBtn = new QPushButton(rowWidget);
                        actionBtn->setFlat(true);
                        rowLayout->addWidget(actionBtn);

                        // Track pointers for real-time animations
                        m_workshopWidgets.insert(name, {statusLabel, actionBtn});

                        // Check if this workshop is currently transitioning
                        if (m_transitioningWorkshops.contains(name)) {
                            statusLabel->setText(m_transitioningWorkshops.value(name) + QStringLiteral("..."));
                            statusLabel->setStyleSheet(QStringLiteral("color: #f39c12; font-weight: bold;"));
                            actionBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
                            actionBtn->setEnabled(false);
                            actionBtn->setToolTip(m_transitioningWorkshops.value(name)
                                                  + QStringLiteral(" workshop..."));
                        } else {
                            // Normal state rendering
                            if (isRunningState) {
                                statusLabel->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));
                                actionBtn->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-stop")));
                                actionBtn->setToolTip(QStringLiteral("Stop Workshop"));
                                connect(actionBtn, &QPushButton::clicked, this, [this, name]() {
                                    performAction(name, QStringLiteral("stop"));
                                });
                            } else if (isStoppedState) {
                                statusLabel->setStyleSheet(QStringLiteral("color: #7f8c8d;"));
                                actionBtn->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
                                actionBtn->setToolTip(needsLaunch ? QStringLiteral("Launch Workshop")
                                                                  : QStringLiteral("Start Workshop"));
                                connect(actionBtn, &QPushButton::clicked, this, [this, name, needsLaunch]() {
                                    performAction(name,
                                                  needsLaunch ? QStringLiteral("launch") : QStringLiteral("start"));
                                });
                            } else {
                                statusLabel->setStyleSheet(QStringLiteral("color: #f39c12; font-weight: bold;"));
                                actionBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
                                actionBtn->setEnabled(false);
                            }
                        }

                        m_workshopsLayout->insertWidget(insertIdx++, rowWidget);
                    }

                    // Sync timers with transitioning workshops state
                    if (!m_transitioningWorkshops.isEmpty()) {
                        if (!m_animationTimer->isActive())
                            m_animationTimer->start();
                        if (!m_pollTimer->isActive())
                            m_pollTimer->start();
                    } else {
                        m_animationTimer->stop();
                        m_pollTimer->stop();
                    }

                    Q_EMIT m_plugin->workshopsRefreshed();
                });
        });
}

void WorkshopToolView::showContextMenu(const QString& workshopName, const QPoint& pos)
{
    auto* rowWidget = qobject_cast<QWidget*>(sender());
    if (!rowWidget)
        return;

    auto* menu = new QMenu(this);
    populateWorkshopContextMenu(
        menu,
        [this, workshopName]() {
            QString projectPath = m_projectCombo->currentData().toString();
            if (projectPath.isEmpty())
                return;

            auto* wizard = new WorkshopWizard(projectPath, workshopName, this);
            if (wizard->exec() == QDialog::Accepted) {
                m_output->setText(QStringLiteral("Workshop configuration updated successfully."));
                refresh();
            }
        },
        [this, workshopName]() {
            QString projectPath = m_projectCombo->currentData().toString();
            if (projectPath.isEmpty())
                return;

            QString status = QStringLiteral("Off");
            if (m_workshopWidgets.contains(workshopName)) {
                status = m_workshopWidgets[workshopName].statusLabel->text();
            }

            // Load existing sketch SDK data from the XDG data location if it exists
            SketchSdkData sketchData;
            QString sketchYamlPath;
            if (!m_projectId.isEmpty()) {
                const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
                if (!dataPath.isEmpty()) {
                    sketchYamlPath = QDir(dataPath).filePath(
                        QStringLiteral("workshop/id/%1/%2/sdk/sketch/current/sdk.yaml").arg(m_projectId, workshopName));
                }
            }

            if (!sketchYamlPath.isEmpty() && QFile::exists(sketchYamlPath)) {
                QFile file(sketchYamlPath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
                    sketchData = parseSketchSdkMap(lines);
                }
            }

            auto* panel = new SketchSdkPanel(workshopName, projectPath, status, sketchData, this);
            connect(panel, &SketchSdkPanel::applied, this, &WorkshopToolView::refresh);
            panel->exec();
            panel->deleteLater();
        },
        [this, workshopName]() {
            removeWorkshop(workshopName);
        });

    menu->exec(rowWidget->mapToGlobal(pos));
    menu->deleteLater();
}

void WorkshopToolView::removeWorkshop(const QString& workshopName)
{
    QString projectPath = m_projectCombo->currentData().toString();
    if (projectPath.isEmpty())
        return;

    auto result = KMessageBox::questionTwoActions(this,
                                                  i18n("Are you sure you want to remove the workshop \"%1\"?\nThis "
                                                       "will delete the configuration file and destroy the container.",
                                                       workshopName),
                                                  i18n("Remove Workshop"), KStandardGuiItem::remove(),
                                                  KStandardGuiItem::cancel());

    if (result != KMessageBox::PrimaryAction) {
        return;
    }

    // Set transition state to Removing
    m_transitioningWorkshops.insert(workshopName, QStringLiteral("Removing"));
    if (m_workshopWidgets.contains(workshopName)) {
        auto widgets = m_workshopWidgets.value(workshopName);
        widgets.statusLabel->setText(QStringLiteral("Removing..."));
        widgets.statusLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-weight: bold;"));
        widgets.actionBtn->setEnabled(false);
        widgets.actionBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
        widgets.actionBtn->setToolTip(QStringLiteral("Removing workshop..."));
    }

    if (!m_animationTimer->isActive()) {
        m_animationTimer->start();
    }
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }

    QDir dir(projectPath);
    QFile file(dir.filePath(QStringLiteral(".workshop/%1.yaml").arg(workshopName)));
    if (file.exists()) {
        file.remove();
    }

    WorkshopApi::queryAsync(
        QStringLiteral("/v1/projects"), this, [this, projectPath, workshopName](const QJsonDocument& doc) {
            QString projectId;
            if (!doc.isEmpty()) {
                QJsonArray projects = doc.object().value(QStringLiteral("result")).toArray();
                for (const QJsonValue& val : projects) {
                    QJsonObject proj = val.toObject();
                    if (proj.value(QStringLiteral("path")).toString() == projectPath) {
                        projectId = proj.value(QStringLiteral("id")).toString();
                        break;
                    }
                }
            }

            if (projectId.isEmpty()) {
                m_transitioningWorkshops.remove(workshopName);
                m_output->setText(QStringLiteral("Failed to fully remove workshop %1: %2")
                                      .arg(workshopName, QStringLiteral("Project is not registered in workshopd.")));
                refresh();
                return;
            }

            QJsonObject req;
            req.insert(QStringLiteral("names"), QJsonArray{workshopName});
            req.insert(QStringLiteral("action"), QStringLiteral("remove"));
            WorkshopApi::queryAsync(
                QStringLiteral("/v1/projects/%1/workshops").arg(projectId),
                QJsonDocument(req).toJson(QJsonDocument::Compact), QStringLiteral("POST"), this,
                [this, workshopName](const QJsonDocument& resp) {
                    bool success = false;
                    QString changeId;
                    QString errorMessage;

                    if (!resp.isEmpty()) {
                        QJsonObject respObj = resp.object();
                        if (respObj.value(QStringLiteral("type")).toString() == QLatin1String("error")) {
                            errorMessage = respObj.value(QStringLiteral("result"))
                                               .toObject()
                                               .value(QStringLiteral("message"))
                                               .toString();
                            if (errorMessage.contains(QStringLiteral("not launched"))
                                || errorMessage.contains(QStringLiteral("not found"))) {
                                success = true;
                                errorMessage.clear();
                            }
                        } else {
                            success = true;
                            changeId = respObj.value(QStringLiteral("result"))
                                           .toObject()
                                           .value(QStringLiteral("id"))
                                           .toString();
                        }
                    } else {
                        errorMessage = QStringLiteral("Connection failed or invalid response from workshopd API.");
                    }

                    if (!success) {
                        m_transitioningWorkshops.remove(workshopName);
                        m_output->setText(
                            QStringLiteral("Failed to fully remove workshop %1: %2").arg(workshopName, errorMessage));
                        refresh();
                        return;
                    }

                    if (changeId.isEmpty()) {
                        m_transitioningWorkshops.remove(workshopName);
                        m_output->setText(QStringLiteral("Workshop %1 removed successfully.").arg(workshopName));
                        refresh();
                        return;
                    }

                    WorkshopApi::queryAsync(
                        QStringLiteral("/v1/changes/%1/wait").arg(changeId), this,
                        [this, workshopName](const QJsonDocument& waitResp) {
                            bool waitSuccess = true;
                            QString errorMessage;
                            if (!waitResp.isEmpty()) {
                                QJsonObject waitResult = waitResp.object().value(QStringLiteral("result")).toObject();
                                QString changeStatus = waitResult.value(QStringLiteral("status")).toString();
                                if (changeStatus == QLatin1String("Error")) {
                                    waitSuccess = false;
                                    errorMessage = waitResult.value(QStringLiteral("err")).toString();
                                }
                            } else {
                                waitSuccess = false;
                                errorMessage = QStringLiteral("Timed out waiting for container removal.");
                            }

                            m_transitioningWorkshops.remove(workshopName);
                            if (waitSuccess) {
                                m_output->setText(
                                    QStringLiteral("Workshop %1 removed successfully.").arg(workshopName));
                            } else {
                                m_output->setText(QStringLiteral("Failed to fully remove workshop %1: %2")
                                                      .arg(workshopName, errorMessage));
                            }
                            refresh();
                        });
                });
        });
}

void WorkshopToolView::performAction(const QString& workshopName, const QString& action)
{
    QString projectPath = m_projectCombo->currentData().toString();
    if (projectPath.isEmpty())
        return;

    m_output->setText(QStringLiteral("Performing action %1 on workshop %2...").arg(action).arg(workshopName));

    // Determine transitioning text dynamically
    QString transitionText;
    if (action == QStringLiteral("launch")) {
        transitionText = QStringLiteral("Launching");
    } else if (action == QStringLiteral("start")) {
        transitionText = QStringLiteral("Starting");
    } else {
        transitionText = QStringLiteral("Stopping");
    }

    m_transitioningWorkshops.insert(workshopName, transitionText);

    if (m_workshopWidgets.contains(workshopName)) {
        auto widgets = m_workshopWidgets.value(workshopName);
        widgets.statusLabel->setText(transitionText + QStringLiteral("..."));
        widgets.statusLabel->setStyleSheet(QStringLiteral("color: #f39c12; font-weight: bold;"));
        widgets.actionBtn->setEnabled(false);
        widgets.actionBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
        widgets.actionBtn->setToolTip(transitionText + QStringLiteral(" workshop..."));
    }

    if (!m_animationTimer->isActive()) {
        m_animationTimer->start();
    }
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }

    WorkshopApi::queryAsync(
        QStringLiteral("/v1/projects"), this, [this, projectPath, workshopName, action](const QJsonDocument& doc) {
            QString projectId;
            if (!doc.isEmpty()) {
                QJsonArray projects = doc.object().value(QStringLiteral("result")).toArray();
                for (const QJsonValue& val : projects) {
                    QJsonObject proj = val.toObject();
                    if (proj.value(QStringLiteral("path")).toString() == projectPath) {
                        projectId = proj.value(QStringLiteral("id")).toString();
                        break;
                    }
                }
            }

            if (projectId.isEmpty()) {
                m_transitioningWorkshops.remove(workshopName);
                m_output->setText(
                    QStringLiteral("Failed to perform action %1 on workshop %2: %3")
                        .arg(action, workshopName, QStringLiteral("Project is not registered in workshopd.")));
                refresh();
                return;
            }

            QJsonObject req;
            req.insert(QStringLiteral("names"), QJsonArray{workshopName});
            req.insert(QStringLiteral("action"), action);

            WorkshopApi::queryAsync(
                QStringLiteral("/v1/projects/%1/workshops").arg(projectId),
                QJsonDocument(req).toJson(QJsonDocument::Compact), QStringLiteral("POST"), this,
                [this, workshopName, action](const QJsonDocument& resp) {
                    bool success = false;
                    QString changeId;
                    QString errorMessage;

                    if (!resp.isEmpty()) {
                        QJsonObject respObj = resp.object();
                        if (respObj.value(QStringLiteral("type")).toString() == QLatin1String("error")) {
                            errorMessage = respObj.value(QStringLiteral("result"))
                                               .toObject()
                                               .value(QStringLiteral("message"))
                                               .toString();
                        } else {
                            success = true;
                            changeId = respObj.value(QStringLiteral("result"))
                                           .toObject()
                                           .value(QStringLiteral("id"))
                                           .toString();
                        }
                    } else {
                        errorMessage = QStringLiteral("Connection failed or invalid response from workshopd API.");
                    }

                    if (!success) {
                        m_transitioningWorkshops.remove(workshopName);
                        m_output->setText(QStringLiteral("Failed to perform action %1 on workshop %2: %3")
                                              .arg(action, workshopName, errorMessage));
                        refresh();
                        return;
                    }

                    if (changeId.isEmpty()) {
                        m_transitioningWorkshops.remove(workshopName);
                        m_output->setText(QStringLiteral("Action %1 performed successfully on workshop %2.")
                                              .arg(action, workshopName));
                        refresh();
                        return;
                    }

                    WorkshopApi::queryAsync(
                        QStringLiteral("/v1/changes/%1/wait").arg(changeId), this,
                        [this, workshopName, action](const QJsonDocument& waitResp) {
                            bool waitSuccess = true;
                            QString errorMessage;
                            if (!waitResp.isEmpty()) {
                                QJsonObject waitResult = waitResp.object().value(QStringLiteral("result")).toObject();
                                QString changeStatus = waitResult.value(QStringLiteral("status")).toString();
                                if (changeStatus == QLatin1String("Error")) {
                                    waitSuccess = false;
                                    errorMessage = waitResult.value(QStringLiteral("err")).toString();
                                    if (errorMessage.isEmpty()) {
                                        errorMessage = QStringLiteral("Unknown daemon error during transition.");
                                    }
                                }
                            } else {
                                waitSuccess = false;
                                errorMessage =
                                    QStringLiteral("Timed out or lost connection while waiting for daemon change.");
                            }

                            m_transitioningWorkshops.remove(workshopName);
                            if (waitSuccess) {
                                m_output->setText(QStringLiteral("Action %1 performed successfully on workshop %2.")
                                                      .arg(action, workshopName));
                            } else {
                                m_output->setText(QStringLiteral("Failed to perform action %1 on workshop %2: %3")
                                                      .arg(action, workshopName, errorMessage));
                            }
                            refresh();
                        });
                });
        });
}

void WorkshopToolView::animateTransitions()
{
    static int dotCount = 0;
    dotCount = (dotCount + 1) % 4;
    QString dots = QString(dotCount, QLatin1Char('.'));

    for (auto it = m_transitioningWorkshops.begin(); it != m_transitioningWorkshops.end(); ++it) {
        QString name = it.key();
        QString baseText = it.value();

        if (m_workshopWidgets.contains(name)) {
            m_workshopWidgets.value(name).statusLabel->setText(baseText + dots);
        }
    }
}

void WorkshopToolView::runCommand(const QString& cmd, const QStringList& args)
{
    QString projectPath = m_projectCombo->currentData().toString();
    if (projectPath.isEmpty())
        return;

    QStringList fullArgs = args;
    fullArgs << QStringLiteral("-p") << projectPath;

    QProcess p;
    p.start(cmd, fullArgs);
    p.waitForFinished();

    m_output->setText(QString::fromUtf8(p.readAllStandardOutput()) + QString::fromUtf8(p.readAllStandardError()));
    refresh();
}
