#include "workshopwizard.h"
#include "api/workshopapi.h"
#include "debug.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMessageBox>
#include <yaml-cpp/yaml.h>

namespace {
std::string toYamlString(const QString& value)
{
    return value.toUtf8().toStdString();
}

QString fromYamlString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QString serializeWorkshopConfiguration(const QString& name, const QString& base, const QStringList& sdks)
{
    YAML::Node root;
    root["name"] = toYamlString(name);
    root["base"] = toYamlString(base);

    if (!sdks.isEmpty()) {
        YAML::Node sdksNode(YAML::NodeType::Sequence);
        for (const QString& sdk : sdks) {
            YAML::Node sdkNode;
            sdkNode["name"] = toYamlString(sdk);
            sdksNode.push_back(sdkNode);
        }
        root["sdks"] = sdksNode;
    }

    YAML::Emitter emitter;
    emitter << root;
    return fromYamlString(emitter.c_str());
}
}

// WorkshopWizard implementation
WorkshopWizard::WorkshopWizard(const QString& projectPath, const QString& existingName, QWidget* parent)
    : QWizard(parent)
    , m_projectPath(projectPath)
    , m_existingName(existingName)
{
    if (!existingName.isEmpty()) {
        parseExistingYaml();
    }

    addPage(new GeneralPage(this));
    addPage(new SdkPage(this));
    addPage(new ReviewPage(projectPath, this, this));

    setWindowTitle(existingName.isEmpty() ? QStringLiteral("Create New Workshop Configuration")
                                          : QStringLiteral("Edit Workshop Configuration: %1").arg(existingName));
    resize(500, 400);
}

void WorkshopWizard::parseExistingYaml()
{
    QDir dir(m_projectPath);
    QFile file(dir.filePath(QStringLiteral(".workshop/%1.yaml").arg(m_existingName)));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_existingBase.clear();
        m_existingSdks.clear();

        try {
            const QByteArray yamlBytes = file.readAll();
            const YAML::Node doc =
                YAML::Load(std::string(yamlBytes.constData(), static_cast<size_t>(yamlBytes.size())));
            if (!doc || !doc.IsMap()) {
                return;
            }

            const YAML::Node base = doc["base"];
            if (base && base.IsScalar()) {
                m_existingBase = fromYamlString(base.as<std::string>()).trimmed();
            }

            const YAML::Node sdks = doc["sdks"];
            if (sdks && sdks.IsSequence()) {
                for (const auto& item : sdks) {
                    if (item.IsScalar()) {
                        m_existingSdks << fromYamlString(item.as<std::string>()).trimmed();
                    } else if (item.IsMap()) {
                        const YAML::Node name = item["name"];
                        if (name && name.IsScalar()) {
                            m_existingSdks << fromYamlString(name.as<std::string>()).trimmed();
                        }
                    }
                }
            }
        } catch (const YAML::Exception& e) {
            qCWarning(PLUGIN_KDEVELOP_WORKSHOP) << "Failed to parse workshop YAML:" << e.what();
        }
    }
}

QString WorkshopWizard::workshopName() const
{
    auto* pageObj = qobject_cast<GeneralPage*>(page(0));
    return pageObj ? pageObj->m_nameEdit->text().trimmed() : QString();
}

QString WorkshopWizard::baseImage() const
{
    auto* pageObj = qobject_cast<GeneralPage*>(page(0));
    return pageObj ? pageObj->m_baseCombo->currentText() : QString();
}

QStringList WorkshopWizard::selectedSdks() const
{
    auto* sdkPage = qobject_cast<SdkPage*>(page(1));
    return sdkPage ? sdkPage->m_selectedStoreSdks : QStringList();
}

// GeneralPage implementation
GeneralPage::GeneralPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle(QStringLiteral("General Settings"));
    setSubTitle(QStringLiteral("Configure the basic settings for the new workshop."));

    auto* layout = new QFormLayout(this);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setText(QStringLiteral("dev"));
    layout->addRow(QStringLiteral("Workshop Name:"), m_nameEdit);

    m_baseCombo = new QComboBox(this);
    m_baseCombo->addItem(QStringLiteral("ubuntu@24.04"));
    m_baseCombo->addItem(QStringLiteral("ubuntu@22.04"));
    layout->addRow(QStringLiteral("Base Image:"), m_baseCombo);
}

void GeneralPage::initializePage()
{
    auto* wizard = qobject_cast<WorkshopWizard*>(this->wizard());
    if (wizard && !wizard->existingName().isEmpty()) {
        m_nameEdit->setText(wizard->existingName());
        m_nameEdit->setReadOnly(true);
        m_baseCombo->setCurrentText(wizard->existingBase());
    } else {
        m_nameEdit->setText(QStringLiteral("dev"));
        m_nameEdit->setReadOnly(false);
        m_baseCombo->setCurrentIndex(0);
    }
}

// SdkPage implementation
SdkPage::SdkPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle(QStringLiteral("Select SDKs"));
    setSubTitle(QStringLiteral("Choose the software development kits to include in this workshop."));

    auto* mainLayout = new QVBoxLayout(this);

    // Search SDKs Section
    mainLayout->addWidget(new QLabel(QStringLiteral("Search SDKs from Store:"), this));

    auto* searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search store (e.g. go, rust, copilot, uv)..."));
    m_searchEdit->installEventFilter(this);
    searchLayout->addWidget(m_searchEdit);

    m_searchBtn = new QPushButton(QStringLiteral("Search"), this);
    connect(m_searchBtn, &QPushButton::clicked, this, &SdkPage::performSearch);
    searchLayout->addWidget(m_searchBtn);
    mainLayout->addLayout(searchLayout);

    m_searchResultsList = new QListWidget(this);
    mainLayout->addWidget(m_searchResultsList);

    // Track user selections in search results persistently
    connect(m_searchResultsList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        QString sdkName = m_searchSdkMap.value(item->text());
        if (sdkName.isEmpty())
            return;

        if (item->checkState() == Qt::Checked) {
            if (!m_selectedStoreSdks.contains(sdkName)) {
                m_selectedStoreSdks.append(sdkName);
            }
        } else {
            m_selectedStoreSdks.removeAll(sdkName);

            // If this item was manually appended because it was previously checked but did not
            // match the latest search query, remove it from the list widget immediately once unchecked.
            if (!m_lastSearchResults.contains(sdkName)) {
                m_searchResultsList->blockSignals(true);
                delete item;
                m_searchResultsList->blockSignals(false);
            }
        }
    });
}

void SdkPage::initializePage()
{
    auto* wizard = qobject_cast<WorkshopWizard*>(this->wizard());
    if (wizard) {
        // Pre-populate selections from existing configuration
        m_selectedStoreSdks = wizard->existingSdks();

        m_searchResultsList->blockSignals(true);
        m_searchResultsList->clear();
        m_searchSdkMap.clear();

        for (const QString& sdk : m_selectedStoreSdks) {
            QString summary = m_sdkSummaries.value(sdk, QStringLiteral("Selected SDK"));
            QString labelText = QStringLiteral("%1 (%2)").arg(sdk).arg(summary);

            auto* item = new QListWidgetItem(labelText, m_searchResultsList);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setCheckState(Qt::Checked);

            m_searchSdkMap.insert(labelText, sdk);
        }
        m_searchResultsList->blockSignals(false);
    }
}

bool SdkPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_searchEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            performSearch();
            return true; // Consume event so QWizard doesn't receive it and click "Next"
        }
    }
    return QWizardPage::eventFilter(watched, event);
}

void SdkPage::performSearch()
{
    QString queryText = m_searchEdit->text().trimmed();
    if (queryText.isEmpty())
        return;

    m_searchBtn->setEnabled(false);

    m_searchResultsList->blockSignals(true);
    m_searchResultsList->clear();
    m_searchResultsList->addItem(QStringLiteral("Searching..."));
    m_searchResultsList->blockSignals(false);

    auto* thread = QThread::create([this, queryText]() {
        QJsonDocument doc = WorkshopApi::query(QStringLiteral("/v1/find?q=%1").arg(queryText));

        QJsonArray results;
        if (!doc.isEmpty()) {
            results = doc.object().value(QStringLiteral("result")).toArray();
        }

        QMetaObject::invokeMethod(
            this,
            [this, results]() {
                m_searchBtn->setEnabled(true);

                m_searchResultsList->blockSignals(true);
                m_searchResultsList->clear();
                m_searchSdkMap.clear();

                QStringList returnedNames;

                // Populate matched results
                for (const QJsonValue& val : results) {
                    QJsonObject sdk = val.toObject();
                    QString name = sdk.value(QStringLiteral("name")).toString();
                    QString summary = sdk.value(QStringLiteral("summary")).toString();

                    returnedNames << name;
                    m_sdkSummaries.insert(name, summary);

                    QString labelText = QStringLiteral("%1 (%2)").arg(name).arg(summary);
                    auto* item = new QListWidgetItem(labelText, m_searchResultsList);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);

                    m_searchSdkMap.insert(labelText, name);

                    if (m_selectedStoreSdks.contains(name)) {
                        item->setCheckState(Qt::Checked);
                    } else {
                        item->setCheckState(Qt::Unchecked);
                    }
                }

                m_lastSearchResults = returnedNames;

                // Append previously selected items that didn't match the new search results
                for (const QString& name : m_selectedStoreSdks) {
                    if (!returnedNames.contains(name)) {
                        QString summary = m_sdkSummaries.value(name, QStringLiteral("Selected SDK"));
                        QString labelText = QStringLiteral("%1 (%2)").arg(name).arg(summary);

                        auto* item = new QListWidgetItem(labelText, m_searchResultsList);
                        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled
                                       | Qt::ItemIsSelectable);
                        item->setCheckState(Qt::Checked);

                        m_searchSdkMap.insert(labelText, name);
                    }
                }

                if (m_searchResultsList->count() == 0) {
                    m_searchResultsList->addItem(QStringLiteral("No matching SDKs found."));
                }

                m_searchResultsList->blockSignals(false);
            },
            Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

// ReviewPage implementation
ReviewPage::ReviewPage(const QString& projectPath, WorkshopWizard* wizard, QWidget* parent)
    : QWizardPage(parent)
    , m_projectPath(projectPath)
    , m_wizard(wizard)
{
    setTitle(QStringLiteral("Review Configuration"));
    setSubTitle(QStringLiteral("Verify the generated YAML configuration."));

    auto* layout = new QVBoxLayout(this);
    m_previewEdit = new QTextEdit(this);
    m_previewEdit->setReadOnly(true);
    layout->addWidget(m_previewEdit);
}

void ReviewPage::initializePage()
{
    QString name = m_wizard->workshopName();
    QString base = m_wizard->baseImage();
    QStringList sdks = m_wizard->selectedSdks();
    m_previewEdit->setText(serializeWorkshopConfiguration(name, base, sdks));
}

bool ReviewPage::validatePage()
{
    QString name = m_wizard->workshopName();
    if (name.isEmpty())
        name = QStringLiteral("dev");

    // Ensure .workshop directory exists
    QDir dir(m_projectPath);
    if (!dir.exists(QStringLiteral(".workshop"))) {
        dir.mkdir(QStringLiteral(".workshop"));
    }

    QString yamlFilePath = dir.filePath(QStringLiteral(".workshop/%1.yaml").arg(name));

    // Backup existing configuration if editing, so we can restore it on validation failure
    QByteArray existingConfigBackup;
    QFile existingFile(yamlFilePath);
    if (existingFile.exists() && existingFile.open(QIODevice::ReadOnly)) {
        existingConfigBackup = existingFile.readAll();
        existingFile.close();
    }

    // Write the new YAML file
    QFile file(yamlFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("File Error"),
                              QStringLiteral("Failed to open file for writing: %1").arg(yamlFilePath));
        return false;
    }

    QTextStream out(&file);
    out << m_previewEdit->toPlainText();
    file.close();

    // Register project path with workshopd API
    QJsonObject req;
    req.insert(QStringLiteral("path"), m_projectPath);
    QJsonDocument reqDoc(req);

    QJsonDocument projDoc = WorkshopApi::query(QStringLiteral("/v1/projects"), reqDoc.toJson(QJsonDocument::Compact),
                                               QStringLiteral("POST"));

    QString projectId;
    if (!projDoc.isEmpty()) {
        projectId = projDoc.object().value(QStringLiteral("result")).toObject().value(QStringLiteral("id")).toString();
    }

    // Validate the newly written file against the daemon
    if (!projectId.isEmpty()) {
        QJsonDocument workshopsDoc = WorkshopApi::query(QStringLiteral("/v1/projects/%1/workshops").arg(projectId));
        if (!workshopsDoc.isEmpty()) {
            QJsonObject respObj = workshopsDoc.object();
            if (respObj.value(QStringLiteral("type")).toString() == QLatin1String("error")) {
                QString errMsg =
                    respObj.value(QStringLiteral("result")).toObject().value(QStringLiteral("message")).toString();

                QMessageBox::critical(this, QStringLiteral("Validation Error"),
                                      QStringLiteral("The workshop configuration is invalid:\n\n%1").arg(errMsg));

                // Rollback file changes on validation failure
                if (!existingConfigBackup.isEmpty()) {
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(existingConfigBackup);
                        file.close();
                    }
                } else {
                    file.remove();
                }

                return false;
            }
        }
    }

    return true;
}
