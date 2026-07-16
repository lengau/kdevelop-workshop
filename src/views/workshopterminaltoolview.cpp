#include "workshopterminaltoolview.h"
#include "kdevelop-workshop.h"
#include "api/workshopapi.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDir>
#include <QPointer>
#include <KMessageBox>
#include <KLocalizedString>

#include <KPluginFactory>
#include <KParts/ReadOnlyPart>
#include <kde_terminal_interface.h>

#include <interfaces/icore.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iproject.h>
#include <util/path.h>

WorkshopTerminalToolView::WorkshopTerminalToolView(kdevelop_workshop* plugin, QWidget* parent)
    : QWidget(parent)
    , m_plugin(plugin)
    , m_refreshing(false)
{
    setWindowIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Header bar layout
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(4, 4, 4, 4);

    headerLayout->addWidget(new QLabel(QStringLiteral("Project:"), this));
    m_projectCombo = new QComboBox(this);
    connect(m_projectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &WorkshopTerminalToolView::onProjectChanged);
    headerLayout->addWidget(m_projectCombo);

    headerLayout->addWidget(new QLabel(QStringLiteral("Workshop:"), this));
    m_workshopCombo = new QComboBox(this);
    connect(m_workshopCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &WorkshopTerminalToolView::updateWorkshopState);
    headerLayout->addWidget(m_workshopCombo);

    m_startBtn = new QPushButton(this);
    m_startBtn->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    m_startBtn->hide();
    connect(m_startBtn, &QPushButton::clicked, this, &WorkshopTerminalToolView::startWorkshop);
    headerLayout->addWidget(m_startBtn);

    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // Terminal container layout
    m_terminalContainer = new QWidget(this);
    m_terminalLayout = new QVBoxLayout(m_terminalContainer);
    m_terminalLayout->setContentsMargins(0, 0, 0, 0);

    // Status Message Label
    m_statusMessageLabel = new QLabel(m_terminalContainer);
    m_statusMessageLabel->setAlignment(Qt::AlignCenter);
    m_statusMessageLabel->setTextFormat(Qt::RichText);
    m_terminalLayout->addWidget(m_statusMessageLabel);

    mainLayout->addWidget(m_terminalContainer, 1);

    // Connect to project controller changes (single shot timer to allow controller lists to settle)
    connect(KDevelop::ICore::self()->projectController(), &KDevelop::IProjectController::projectOpened, this,
            [this](KDevelop::IProject*) {
                QTimer::singleShot(100, this, &WorkshopTerminalToolView::refresh);
            });
    connect(KDevelop::ICore::self()->projectController(), &KDevelop::IProjectController::projectClosed, this,
            [this](KDevelop::IProject*) {
                QTimer::singleShot(100, this, &WorkshopTerminalToolView::refresh);
            });

    // Connect to plugin refresh signal to auto-update active workshops dropdown
    connect(m_plugin, &kdevelop_workshop::workshopsRefreshed, this, [this]() {
        onProjectChanged(m_projectCombo->currentIndex());
    });

    refresh();
}

WorkshopTerminalToolView::~WorkshopTerminalToolView()
{
    clearTerminal();
}

void WorkshopTerminalToolView::refresh()
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

void WorkshopTerminalToolView::onProjectChanged(int index)
{
    m_workshopCombo->blockSignals(true);
    m_workshopCombo->clear();
    m_startBtn->hide();
    m_workshopCombo->blockSignals(false);

    if (index < 0 || m_projectCombo->count() == 0) {
        m_refreshing = false;
        clearTerminal();
        showStatusMessage(QStringLiteral("No open projects."));
        return;
    }

    QString projectPath = m_projectCombo->itemData(index).toString();

    WorkshopApi::queryAsync(QStringLiteral("/v1/projects"), this, [this, projectPath](const QJsonDocument& doc) {
        if (m_projectCombo->currentData().toString() != projectPath) {
            return;
        }

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
            m_refreshing = false;
            m_workshopCombo->blockSignals(true);
            m_workshopCombo->clear();
            m_workshopCombo->blockSignals(false);
            updateWorkshopState();
            return;
        }

        WorkshopApi::queryAsync(QStringLiteral("/v1/projects/%1/workshops").arg(projectId), this,
                                [this, projectPath](const QJsonDocument& workshopsDoc) {
                                    if (m_projectCombo->currentData().toString() != projectPath) {
                                        return;
                                    }

                                    QJsonArray workshops;
                                    if (!workshopsDoc.isEmpty()) {
                                        QJsonObject result =
                                            workshopsDoc.object().value(QStringLiteral("result")).toObject();
                                        workshops = result.value(QStringLiteral("workshops")).toArray();
                                    }

                                    m_refreshing = false;
                                    m_workshopCombo->blockSignals(true);
                                    m_workshopCombo->clear();

                                    int firstReadyIndex = -1;
                                    int idx = 0;

                                    for (const QJsonValue& val : workshops) {
                                        QJsonObject ws = val.toObject();
                                        QString name = ws.value(QStringLiteral("name")).toString();
                                        QString status = ws.value(QStringLiteral("status")).toString();

                                        bool isReady = (status.toLower() == QLatin1String("ready")
                                                        || status.toLower() == QLatin1String("running"));
                                        bool needsLaunch = status.isEmpty() || status.toLower() == QLatin1String("off");

                                        m_workshopCombo->addItem(name, QVariantList{status, needsLaunch});

                                        if (isReady && firstReadyIndex == -1) {
                                            firstReadyIndex = idx;
                                        }
                                        idx++;
                                    }

                                    if (m_workshopCombo->count() > 0) {
                                        m_workshopCombo->setCurrentIndex(firstReadyIndex != -1 ? firstReadyIndex : 0);
                                    }

                                    m_workshopCombo->blockSignals(false);
                                    updateWorkshopState();
                                });
    });
}

void WorkshopTerminalToolView::updateWorkshopState()
{
    int index = m_workshopCombo->currentIndex();
    if (index < 0) {
        m_startBtn->hide();
        clearTerminal();
        showStatusMessage(QStringLiteral("No workshops available."));
        return;
    }

    QString workshopName = m_workshopCombo->currentText();
    QVariant dataVal = m_workshopCombo->itemData(index);

    QVariantList data = dataVal.toList();

    QString status;
    bool needsLaunch = false;
    if (data.count() >= 2) {
        status = data.at(0).toString();
        needsLaunch = data.at(1).toBool();
    }

    bool isReady = (status.toLower() == QLatin1String("ready") || status.toLower() == QLatin1String("running"));

    m_startBtn->show();
    m_startBtn->setEnabled(true);

    if (isReady) {
        m_startBtn->setText(QStringLiteral("Stop Workshop"));
        m_startBtn->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-stop")));
        if (m_connectedWorkshop != workshopName) {
            connectToWorkshop(workshopName);
        }
    } else {
        m_connectedWorkshop.clear();
        clearTerminal();

        m_startBtn->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        if (needsLaunch) {
            m_startBtn->setText(QStringLiteral("Launch Workshop"));
            showStatusMessage(QStringLiteral("<h3>Workshop <b>%1</b> is not launched</h3><p>Click 'Launch Workshop' "
                                             "above to build and start it.</p>")
                                  .arg(workshopName));
        } else if (status.toLower() == QLatin1String("starting") || status.toLower() == QLatin1String("launching")) {
            m_startBtn->setEnabled(false);
            m_startBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
            m_startBtn->setText(status.toLower() == QLatin1String("launching") ? QStringLiteral("Launching...")
                                                                               : QStringLiteral("Starting..."));
            showStatusMessage(
                QStringLiteral("<h3>Workshop <b>%1</b> is starting...</h3><p>Please wait while the container runs.</p>")
                    .arg(workshopName));
        } else if (status.toLower() == QLatin1String("stopping")) {
            m_startBtn->setEnabled(false);
            m_startBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
            m_startBtn->setText(QStringLiteral("Stopping..."));
            showStatusMessage(
                QStringLiteral("<h3>Workshop <b>%1</b> is stopping...</h3><p>Please wait.</p>").arg(workshopName));
        } else {
            m_startBtn->setText(QStringLiteral("Start Workshop"));
            showStatusMessage(
                QStringLiteral("<h3>Workshop <b>%1</b> is stopped</h3><p>Click 'Start Workshop' above to run it.</p>")
                    .arg(workshopName));
        }
    }
}

void WorkshopTerminalToolView::clearTerminal()
{
    if (m_part) {
        if (auto* widget = m_part->widget()) {
            m_terminalLayout->removeWidget(widget);
            widget->hide();
        }
        m_part->deleteLater();
        m_part = nullptr;
    }
}

void WorkshopTerminalToolView::showStatusMessage(const QString& html)
{
    m_statusMessageLabel->setText(html);
    m_statusMessageLabel->show();
}

void WorkshopTerminalToolView::connectToWorkshop(const QString& workshopName)
{
    clearTerminal();
    m_statusMessageLabel->hide();

    // Embed the KonsolePart
    const auto result = KPluginFactory::loadFactory(
        KPluginMetaData::findPluginById(QStringLiteral("kf6/parts"), QStringLiteral("konsolepart")));

    if (result) {
        m_part = result.plugin->create<KParts::ReadOnlyPart>(m_terminalContainer);
        auto* terminalIface = m_part ? qobject_cast<TerminalInterface*>(m_part) : nullptr;
        if (m_part && terminalIface) {
            m_terminalLayout->addWidget(m_part->widget());
            m_part->widget()->show();

            QString projectPath = m_projectCombo->currentData().toString();
            if (projectPath.isEmpty()) {
                projectPath = QDir::currentPath();
            }

            terminalIface->showShellInDir(projectPath);

            m_connectedWorkshop = workshopName;

            connect(m_part, &QObject::destroyed, this, [this]() {
                m_connectedWorkshop.clear();
                m_part = nullptr;
                updateWorkshopState();
            });

            QPointer<KParts::ReadOnlyPart> partPtr(m_part);
            QTimer::singleShot(500, this, [partPtr, workshopName]() {
                if (!partPtr)
                    return;
                if (auto* iface = qobject_cast<TerminalInterface*>(partPtr.data())) {
                    iface->sendInput(QStringLiteral("clear && workshop shell %1; exit\n").arg(workshopName));
                }
            });
        } else {
            showStatusMessage(QStringLiteral("Failed to load Konsole Terminal Interface."));
        }
    } else {
        showStatusMessage(QStringLiteral("Failed to load konsolepart plugin (kf6/parts/konsolepart)."));
    }
}

void WorkshopTerminalToolView::startWorkshop()
{
    int index = m_workshopCombo->currentIndex();
    if (index < 0)
        return;

    QString workshopName = m_workshopCombo->currentText();
    QVariantList data = m_workshopCombo->itemData(index).toList();

    bool needsLaunch = false;
    if (data.count() >= 2) {
        needsLaunch = data.at(1).toBool();
    }

    QString status;
    if (data.count() >= 1) {
        status = data.at(0).toString();
    }

    bool isReady = (status.toLower() == QLatin1String("ready") || status.toLower() == QLatin1String("running"));

    QString action;
    if (isReady) {
        action = QStringLiteral("stop");
    } else if (needsLaunch) {
        action = QStringLiteral("launch");
    } else {
        action = QStringLiteral("start");
    }

    m_startBtn->setEnabled(false);
    if (action == QStringLiteral("stop")) {
        m_startBtn->setText(QStringLiteral("Stopping..."));
    } else if (action == QStringLiteral("launch")) {
        m_startBtn->setText(QStringLiteral("Launching..."));
    } else {
        m_startBtn->setText(QStringLiteral("Starting..."));
    }

    QString projectPath = m_projectCombo->currentData().toString();
    if (projectPath.isEmpty())
        return;

    if (action == QStringLiteral("stop")) {
        showStatusMessage(
            QStringLiteral("<h3>Stopping workshop <b>%1</b>...</h3><p>Connecting to API...</p>").arg(workshopName));
    } else {
        showStatusMessage(
            QStringLiteral("<h3>%1 workshop <b>%2</b>...</h3><p>Connecting to API...</p>")
                .arg(action == QStringLiteral("launch") ? QStringLiteral("Launching") : QStringLiteral("Starting"))
                .arg(workshopName));
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
                m_startBtn->setEnabled(true);
                updateWorkshopState();
                KMessageBox::error(this,
                                   i18n("Failed to start/stop/launch workshop %1: %2", workshopName,
                                        QStringLiteral("Project is not registered in workshopd.")),
                                   i18n("Action Failed"));
                return;
            }

            QJsonObject req;
            req.insert(QStringLiteral("names"), QJsonArray{workshopName});
            req.insert(QStringLiteral("action"), action);
            WorkshopApi::queryAsync(
                QStringLiteral("/v1/projects/%1/workshops").arg(projectId),
                QJsonDocument(req).toJson(QJsonDocument::Compact), QStringLiteral("POST"), this,
                [this, workshopName](const QJsonDocument& resp) {
                    bool success = false;
                    QString errorMessage;
                    QString changeId;

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
                        m_startBtn->setEnabled(true);
                        updateWorkshopState();
                        KMessageBox::error(
                            this, i18n("Failed to start/stop/launch workshop %1: %2", workshopName, errorMessage),
                            i18n("Action Failed"));
                        return;
                    }

                    if (changeId.isEmpty()) {
                        Q_EMIT m_plugin->workshopsRefreshed();
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
                                errorMessage = QStringLiteral("Timed out waiting for change to complete.");
                            }

                            if (!waitSuccess) {
                                m_startBtn->setEnabled(true);
                                updateWorkshopState();
                                KMessageBox::error(
                                    this,
                                    i18n("Failed to start/stop/launch workshop %1: %2", workshopName, errorMessage),
                                    i18n("Action Failed"));
                            } else {
                                Q_EMIT m_plugin->workshopsRefreshed();
                            }
                        });
                });
        });
}
