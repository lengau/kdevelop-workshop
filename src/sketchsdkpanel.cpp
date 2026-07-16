#include "sketchsdkpanel.h"
#include "parsesketchsdk.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryDir>
#include <QAccessible>
#include <QAccessibleEvent>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <KLocalizedString>

namespace
{
QString safeErrorOutput(const QString &stdErr, const QString &stdOut)
{
    if (!stdErr.trimmed().isEmpty()) {
        return stdErr.trimmed();
    }

    if (!stdOut.trimmed().isEmpty()) {
        return stdOut.trimmed();
    }

    return {};
}

bool isValidName(const QString &name)
{
    static QRegularExpression re(QStringLiteral("^[a-z][a-z0-9-]*$"));
    return re.match(name).hasMatch();
}
} // namespace

SketchSdkPanel::SketchSdkPanel(const QString &workshopName, const QString &projectPath,
                               const QString &workshopStatus, QWidget *parent)
    : QDialog(parent)
    , m_workshopName(workshopName)
    , m_projectPath(projectPath)
    , m_workshopStatus(workshopStatus)
{
    setupUi();
}

SketchSdkPanel::SketchSdkPanel(const QString &workshopName, const QString &projectPath,
                               const QString &workshopStatus, const SketchSdkData &prefillData, QWidget *parent)
    : QDialog(parent)
    , m_workshopName(workshopName)
    , m_projectPath(projectPath)
    , m_workshopStatus(workshopStatus)
{
    setupUi();
    populateFromPrefillData(prefillData);
}

SketchSdkPanel::~SketchSdkPanel()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void SketchSdkPanel::reject()
{
    if (m_process) {
        QMessageBox::information(this,
                                 i18n("Please wait for the current workshop command to finish."),
                                 i18n("Operation in Progress"));
        return;
    }

    QDialog::reject();
}

void SketchSdkPanel::setupUi()
{
    setWindowTitle(i18n("Sketch SDK"));
    setModal(true);
    resize(900, 700);

    auto *layout = new QVBoxLayout(this);

    auto *titleLabel =
        new QLabel(i18n("Configure the sketch SDK for workshop \"%1\".", m_workshopName), this);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    m_editorWidget = new QWidget(this);
    auto *editorLayout = new QVBoxLayout(m_editorWidget);
    editorLayout->setContentsMargins(0, 0, 0, 0);

    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    auto *hooksGroup = new QGroupBox(i18n("Hooks"), m_editorWidget);
    auto *hooksLayout = new QFormLayout(hooksGroup);

    m_setupBaseEdit = new QPlainTextEdit(hooksGroup);
    m_setupBaseEdit->setFont(fixedFont);
    m_setupBaseEdit->setPlaceholderText(i18n("apt-get update\napt-get install build-essential"));
    m_setupBaseEdit->setMinimumHeight(110);
    hooksLayout->addRow(i18n("Setup Base:"), m_setupBaseEdit);

    m_setupProjectEdit = new QPlainTextEdit(hooksGroup);
    m_setupProjectEdit->setFont(fixedFont);
    m_setupProjectEdit->setPlaceholderText(i18n("uv sync"));
    m_setupProjectEdit->setMinimumHeight(110);
    hooksLayout->addRow(i18n("Setup Project:"), m_setupProjectEdit);

    editorLayout->addWidget(hooksGroup);

    auto *plugsGroup = new QGroupBox(i18n("Plugs"), m_editorWidget);
    auto *plugsLayout = new QVBoxLayout(plugsGroup);

    m_plugsTable = new QTableWidget(0, 3, plugsGroup);
    m_plugsTable->setHorizontalHeaderLabels(
        {i18n("Name"), i18n("Interface"), i18n("Workshop Target")});
    m_plugsTable->horizontalHeader()->setStretchLastSection(true);
    m_plugsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_plugsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_plugsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_plugsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    plugsLayout->addWidget(m_plugsTable);

    auto *plugButtonLayout = new QHBoxLayout();
    plugButtonLayout->addStretch();
    m_addPlugButton = new QPushButton(i18n("Add"), plugsGroup);
    m_removePlugButton = new QPushButton(i18n("Remove"), plugsGroup);
    plugButtonLayout->addWidget(m_addPlugButton);
    plugButtonLayout->addWidget(m_removePlugButton);
    plugsLayout->addLayout(plugButtonLayout);

    editorLayout->addWidget(plugsGroup);

    auto *slotsGroup = new QGroupBox(i18n("Slots"), m_editorWidget);
    auto *slotsLayout = new QVBoxLayout(slotsGroup);

    m_slotsTable = new QTableWidget(0, 3, slotsGroup);
    m_slotsTable->setHorizontalHeaderLabels({i18n("Name"), i18n("Interface"), i18n("Endpoint")});
    m_slotsTable->horizontalHeader()->setStretchLastSection(true);
    m_slotsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_slotsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_slotsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_slotsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    slotsLayout->addWidget(m_slotsTable);

    auto *slotButtonLayout = new QHBoxLayout();
    slotButtonLayout->addStretch();
    m_addSlotButton = new QPushButton(i18n("Add"), slotsGroup);
    m_removeSlotButton = new QPushButton(i18n("Remove"), slotsGroup);
    slotButtonLayout->addWidget(m_addSlotButton);
    slotButtonLayout->addWidget(m_removeSlotButton);
    slotsLayout->addLayout(slotButtonLayout);

    editorLayout->addWidget(slotsGroup);
    layout->addWidget(m_editorWidget);

    m_statusLabel = new QLabel(this);
    m_statusLabel->hide();
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->hide();
    layout->addWidget(m_progressBar);

    m_buttonBox = new QDialogButtonBox(this);
    m_applyButton = m_buttonBox->addButton(i18n("Apply"), QDialogButtonBox::ApplyRole);
    m_stashButton = m_buttonBox->addButton(i18n("Stash"), QDialogButtonBox::ActionRole);
    m_restoreButton = m_buttonBox->addButton(i18n("Restore"), QDialogButtonBox::ActionRole);
    m_ejectButton = m_buttonBox->addButton(i18n("Eject"), QDialogButtonBox::ActionRole);
    m_removeButton = m_buttonBox->addButton(i18n("Remove"), QDialogButtonBox::DestructiveRole);
    m_closeButton = m_buttonBox->addButton(QDialogButtonBox::Close);
    layout->addWidget(m_buttonBox);

    connect(m_addPlugButton, &QPushButton::clicked, this, &SketchSdkPanel::addPlug);
    connect(m_removePlugButton, &QPushButton::clicked, this, &SketchSdkPanel::removeSelectedPlug);
    connect(m_addSlotButton, &QPushButton::clicked, this, &SketchSdkPanel::addSlot);
    connect(m_removeSlotButton, &QPushButton::clicked, this, &SketchSdkPanel::removeSelectedSlot);
    connect(m_applyButton, &QPushButton::clicked, this, &SketchSdkPanel::applySketchSdk);
    connect(m_stashButton, &QPushButton::clicked, this, &SketchSdkPanel::stashSketchSdk);
    connect(m_restoreButton, &QPushButton::clicked, this, &SketchSdkPanel::restoreSketchSdk);
    connect(m_ejectButton, &QPushButton::clicked, this, &SketchSdkPanel::ejectSketchSdk);
    connect(m_removeButton, &QPushButton::clicked, this, &SketchSdkPanel::removeSketchSdk);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_plugsTable, &QTableWidget::itemSelectionChanged, this,
            &SketchSdkPanel::updateActionState);
    connect(m_slotsTable, &QTableWidget::itemSelectionChanged, this,
            &SketchSdkPanel::updateActionState);

    updateActionState();

    bool isReady = (m_workshopStatus.toLower() == QLatin1String("ready") || m_workshopStatus.toLower() == QLatin1String("running"));
    if (!isReady) {
        bool needsLaunch = (m_workshopStatus.isEmpty() || m_workshopStatus.toLower() == QLatin1String("off"));
        QString action = needsLaunch ? QStringLiteral("launch") : QStringLiteral("start");

        PendingCommand command;
        if (needsLaunch) {
            command.runningMessage = i18n("Launching workshop \"%1\" to enable sketching…", m_workshopName);
            command.successMessage = i18n("Workshop launched successfully.");
        } else {
            command.runningMessage = i18n("Starting workshop \"%1\" to enable sketching…", m_workshopName);
            command.successMessage = i18n("Workshop started successfully.");
        }
        command.emitApplied = true;

        connect(this, &SketchSdkPanel::applied, this, [this]() {
            m_workshopStatus = QStringLiteral("Ready");
            updateActionState();
            // Disconnect ourselves so we don't handle subsequent applied signals
            disconnect(this, &SketchSdkPanel::applied, this, nullptr);
        });

        runWorkshopCommand({action, m_workshopName}, command);
    }
}

void SketchSdkPanel::populateFromPrefillData(const SketchSdkData &data)
{
    m_setupBaseEdit->setPlainText(data.setupBase);
    m_setupProjectEdit->setPlainText(data.setupProject);

    for (auto it = data.plugs.constBegin(); it != data.plugs.constEnd(); ++it) {
        addPlugRow(it.key(), it.value().interfaceName, it.value().workshopTarget);
    }

    for (auto it = data.slots.constBegin(); it != data.slots.constEnd(); ++it) {
        addSlotRow(it.key(), it.value().interfaceName, it.value().endpoint);
    }

    updateActionState();
}

void SketchSdkPanel::addPlug()
{
    addPlugRow();
    updateActionState();
}

void SketchSdkPanel::removeSelectedPlug()
{
    const int row = m_plugsTable->currentRow();
    if (row < 0) {
        return;
    }

    m_plugsTable->removeRow(row);
    updateActionState();
}

void SketchSdkPanel::addSlot()
{
    addSlotRow();
    updateActionState();
}

void SketchSdkPanel::removeSelectedSlot()
{
    const int row = m_slotsTable->currentRow();
    if (row < 0) {
        return;
    }

    m_slotsTable->removeRow(row);
    updateActionState();
}

void SketchSdkPanel::applySketchSdk()
{
    QString errorMessage;
    if (!validateForm(&errorMessage)) {
        finishWithError(errorMessage);
        return;
    }

    auto tempDir = std::make_unique<QTemporaryDir>(QDir::tempPath() +
                                                   QStringLiteral("/shopkeeper-sketch-sdk-XXXXXX"));
    if (!tempDir->isValid()) {
        finishWithError(i18n("Could not create temporary files for the sketch SDK command."));
        return;
    }

    const QString yamlPath = QDir(tempDir->path()).filePath(QStringLiteral("sketch-sdk.yaml"));
    QFile yamlFile(yamlPath);
    if (!yamlFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        finishWithError(i18n("Could not write the temporary sketch SDK file."));
        return;
    }
    yamlFile.write(generateYaml().toUtf8());
    yamlFile.close();

    const QString scriptPath =
        QDir(tempDir->path()).filePath(QStringLiteral("sketch-sdk-editor.sh"));
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        finishWithError(i18n("Could not create the temporary editor script."));
        return;
    }

    const QByteArray script = QStringLiteral("#!/bin/bash\nset -e\ncp %1 \"$1\"\n")
                                  .arg(shellSingleQuoted(yamlPath))
                                  .toUtf8();
    scriptFile.write(script);
    scriptFile.close();
    scriptFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                               QFileDevice::ExeOwner);

    m_tempDir = std::move(tempDir);

    PendingCommand command;
    command.runningMessage = i18n("Applying sketch SDK…");
    command.successMessage = i18n("Sketch SDK applied successfully.");
    command.emitApplied = true;

    runWorkshopCommand(
        {QStringLiteral("sketch-sdk"), m_workshopName, QStringLiteral("-p"), m_projectPath},
        command, scriptPath);
}

void SketchSdkPanel::stashSketchSdk()
{
    PendingCommand command;
    command.runningMessage = i18n("Stashing sketch SDK…");
    command.successMessage = i18n("Sketch SDK stashed successfully.");

    runWorkshopCommand({QStringLiteral("sketch-sdk"), m_workshopName, QStringLiteral("--stash"),
                        QStringLiteral("-p"), m_projectPath},
                       command);
}

void SketchSdkPanel::restoreSketchSdk()
{
    PendingCommand command;
    command.runningMessage = i18n("Restoring sketch SDK…");
    command.successMessage = i18n("Sketch SDK restored successfully.");

    runWorkshopCommand({QStringLiteral("sketch-sdk"), m_workshopName, QStringLiteral("--restore"),
                        QStringLiteral("-p"), m_projectPath},
                       command);
}

void SketchSdkPanel::ejectSketchSdk()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, i18n("Eject Sketch SDK"), i18n("SDK name:"),
                                               QLineEdit::Normal, QStringLiteral("sketch"), &ok)
                             .trimmed();
    if (!ok) {
        return;
    }

    if (!isValidName(name)) {
        finishWithError(i18n("SDK name must match [a-z][a-z0-9-]*."));
        return;
    }

    PendingCommand command;
    command.runningMessage = i18n("Ejecting sketch SDK…");
    command.successMessage = i18n("Sketch SDK ejected successfully.");

    runWorkshopCommand({QStringLiteral("sketch-sdk"), m_workshopName, QStringLiteral("--eject"),
                        QStringLiteral("--name"), name, QStringLiteral("-p"), m_projectPath},
                       command);
}

void SketchSdkPanel::removeSketchSdk()
{
    auto result = QMessageBox::question(
        this,
        i18n("Remove Sketch SDK"),
        i18n("Are you sure you want to remove the sketch SDK from workshop \"%1\"?", m_workshopName),
        QMessageBox::Yes | QMessageBox::No
    );
    if (result != QMessageBox::Yes) {
        return;
    }

    PendingCommand command;
    command.runningMessage = i18n("Removing sketch SDK…");
    command.successMessage = i18n("Sketch SDK removed successfully.");
    command.closeOnSuccess = true;

    runWorkshopCommand({QStringLiteral("sketch-sdk"), m_workshopName, QStringLiteral("--remove"),
                        QStringLiteral("-p"), m_projectPath},
                       command);
}

void SketchSdkPanel::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_process) {
        return;
    }

    m_standardOutput += QString::fromUtf8(m_process->readAllStandardOutput());
    m_standardError += QString::fromUtf8(m_process->readAllStandardError());

    const PendingCommand command = m_pendingCommand;

    m_process->deleteLater();
    m_process = nullptr;
    cleanupTemporaryFiles();
    setBusy(false);

    if (exitStatus != QProcess::NormalExit) {
        finishWithError(i18n("The workshop command crashed."));
        return;
    }

    if (exitCode != 0) {
        const QString output = safeErrorOutput(m_standardError, m_standardOutput);
        if (!output.isEmpty()) {
            finishWithError(output);
        } else {
            finishWithError(i18n("The workshop command exited with code %1.", exitCode));
        }
        return;
    }

    m_standardOutput.clear();
    m_standardError.clear();
    m_statusLabel->setText(command.successMessage);
    m_statusLabel->show();

    if (command.emitApplied) {
        Q_EMIT applied();
    }

    if (command.closeOnSuccess) {
        accept();
    }
}

void SketchSdkPanel::onProcessError(QProcess::ProcessError error)
{
    if (!m_process || error != QProcess::FailedToStart) {
        return;
    }

    m_process->deleteLater();
    m_process = nullptr;
    cleanupTemporaryFiles();
    setBusy(false);
    finishWithError(i18n("Could not start %1.", m_workshopBinaryPath));
}

void SketchSdkPanel::updateActionState()
{
    const bool busy = m_process != nullptr;
    const bool isReady = (m_workshopStatus.toLower() == QLatin1String("ready") || m_workshopStatus.toLower() == QLatin1String("running"));

    m_addPlugButton->setEnabled(!busy && isReady);
    m_removePlugButton->setEnabled(!busy && isReady && m_plugsTable->currentRow() >= 0);
    m_addSlotButton->setEnabled(!busy && isReady);
    m_removeSlotButton->setEnabled(!busy && isReady && m_slotsTable->currentRow() >= 0);
    m_applyButton->setEnabled(!busy && isReady);
    m_stashButton->setEnabled(!busy && isReady);
    m_restoreButton->setEnabled(!busy && isReady);
    m_ejectButton->setEnabled(!busy && isReady);
    m_removeButton->setEnabled(!busy && isReady);
    m_closeButton->setEnabled(!busy);

    if (!isReady && !busy) {
        m_statusLabel->setText(i18n("<h3>Workshop \"%1\" is currently stopped/offline</h3>"
                                    "<p>You must start the workshop from the sidebar or bottom panel "
                                    "before you can configure, apply, or eject its sketch SDK.</p>", m_workshopName));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-weight: bold;"));
        m_statusLabel->show();
    }
}

void SketchSdkPanel::addPlugRow(const QString &name, const QString &interfaceName,
                                const QString &workshopTarget)
{
    const int row = m_plugsTable->rowCount();
    m_plugsTable->insertRow(row);
    m_plugsTable->setItem(row, 0, new QTableWidgetItem(name));

    auto *interfaceItem = new QTableWidgetItem(
        interfaceName.trimmed().isEmpty() ? QStringLiteral("mount") : interfaceName.trimmed());
    interfaceItem->setFlags(interfaceItem->flags() & ~Qt::ItemIsEditable);
    m_plugsTable->setItem(row, 1, interfaceItem);
    m_plugsTable->setItem(row, 2, new QTableWidgetItem(workshopTarget));
    m_plugsTable->setCurrentCell(row, 0);
}

void SketchSdkPanel::addSlotRow(const QString &name, const QString &interfaceName, int endpoint)
{
    const int row = m_slotsTable->rowCount();
    m_slotsTable->insertRow(row);
    m_slotsTable->setItem(row, 0, new QTableWidgetItem(name));

    auto *interfaceItem = new QTableWidgetItem(
        interfaceName.trimmed().isEmpty() ? QStringLiteral("tunnel") : interfaceName.trimmed());
    interfaceItem->setFlags(interfaceItem->flags() & ~Qt::ItemIsEditable);
    m_slotsTable->setItem(row, 1, interfaceItem);
    m_slotsTable->setItem(row, 2, new QTableWidgetItem(QString::number(endpoint)));
    m_slotsTable->setCurrentCell(row, 0);
}

void SketchSdkPanel::runWorkshopCommand(const QStringList &arguments, const PendingCommand &command,
                                        const QString &editorScriptPath)
{
    if (m_process) {
        finishWithError(i18n("A workshop command is already running."));
        return;
    }

    m_pendingCommand = command;
    m_standardOutput.clear();
    m_standardError.clear();

    m_process = new QProcess(this);
    m_process->setProgram(m_workshopBinaryPath);
    m_process->setArguments(arguments);
    m_process->setWorkingDirectory(m_projectPath);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (!editorScriptPath.isEmpty()) {
        environment.insert(QStringLiteral("EDITOR"), editorScriptPath);
    }
    m_process->setProcessEnvironment(environment);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        if (!m_process) {
            return;
        }
        m_standardOutput += QString::fromUtf8(m_process->readAllStandardOutput());
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        if (!m_process) {
            return;
        }
        m_standardError += QString::fromUtf8(m_process->readAllStandardError());
    });
    connect(m_process, &QProcess::finished, this, &SketchSdkPanel::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &SketchSdkPanel::onProcessError);

    setBusy(true, command.runningMessage);
    m_process->start();
}

void SketchSdkPanel::setBusy(bool busy, const QString &message)
{
    m_editorWidget->setEnabled(!busy);
    if (busy) {
        m_statusLabel->setText(message);
        m_statusLabel->show();
        m_progressBar->show();
    } else {
        m_progressBar->hide();
    }

    updateActionState();
}

void SketchSdkPanel::finishWithError(const QString &error)
{
    m_standardOutput.clear();
    m_standardError.clear();
    m_statusLabel->setText(error);
    m_statusLabel->show();
    QMessageBox::critical(this, i18n("Sketch SDK Error"), error);
    Q_EMIT errorOccurred(error);
}

void SketchSdkPanel::cleanupTemporaryFiles()
{
    m_tempDir.reset();
}

bool SketchSdkPanel::validateForm(QString *errorMessage) const
{
    QSet<QString> plugNames;
    QSet<QString> slotNames;

    for (int row = 0; row < m_plugsTable->rowCount(); ++row) {
        const QString name = tableItemText(m_plugsTable, row, 0).trimmed();
        const QString interfaceName = tableItemText(m_plugsTable, row, 1).trimmed();
        const QString workshopTarget = tableItemText(m_plugsTable, row, 2).trimmed();

        if (!isValidName(name)) {
            *errorMessage = i18n("Plug name in row %1 must match [a-z][a-z0-9-]*.", row + 1);
            return false;
        }
        if (plugNames.contains(name)) {
            *errorMessage = i18n("Plug name \"%1\" is duplicated.", name);
            return false;
        }
        if (workshopTarget.isEmpty()) {
            *errorMessage = i18n("Plug target in row %1 cannot be empty.", row + 1);
            return false;
        }
        if (interfaceName.isEmpty()) {
            *errorMessage = i18n("Plug interface in row %1 cannot be empty.", row + 1);
            return false;
        }

        plugNames.insert(name);
    }

    for (int row = 0; row < m_slotsTable->rowCount(); ++row) {
        const QString name = tableItemText(m_slotsTable, row, 0).trimmed();
        const QString interfaceName = tableItemText(m_slotsTable, row, 1).trimmed();
        const QString endpointText = tableItemText(m_slotsTable, row, 2).trimmed();
        bool ok = false;
        const int endpoint = endpointText.toInt(&ok);

        if (!isValidName(name)) {
            *errorMessage = i18n("Slot name in row %1 must match [a-z][a-z0-9-]*.", row + 1);
            return false;
        }
        if (slotNames.contains(name)) {
            *errorMessage = i18n("Slot name \"%1\" is duplicated.", name);
            return false;
        }
        if (!ok || endpoint < 1 || endpoint > 65535) {
            *errorMessage =
                i18n("Slot endpoint in row %1 must be a port number between 1 and 65535.", row + 1);
            return false;
        }
        if (interfaceName.isEmpty()) {
            *errorMessage = i18n("Slot interface in row %1 cannot be empty.", row + 1);
            return false;
        }

        slotNames.insert(name);
    }

    return true;
}

QString SketchSdkPanel::generateYaml() const
{
    SketchSdkData data;
    data.setupBase = m_setupBaseEdit->toPlainText();
    data.setupProject = m_setupProjectEdit->toPlainText();

    for (int row = 0; row < m_plugsTable->rowCount(); ++row) {
        const QString name = tableItemText(m_plugsTable, row, 0).trimmed();
        SketchSdkData::Plug plug;
        plug.interfaceName = tableItemText(m_plugsTable, row, 1).trimmed();
        plug.workshopTarget = tableItemText(m_plugsTable, row, 2).trimmed();
        data.plugs[name] = plug;
    }

    for (int row = 0; row < m_slotsTable->rowCount(); ++row) {
        const QString name = tableItemText(m_slotsTable, row, 0).trimmed();
        SketchSdkData::Slot slot;
        slot.interfaceName = tableItemText(m_slotsTable, row, 1).trimmed();
        bool ok;
        slot.endpoint = tableItemText(m_slotsTable, row, 2).trimmed().toInt(&ok);
        Q_ASSERT(ok);
        data.slots[name] = slot;
    }

    return serializeSketchSdk(data);
}

QString SketchSdkPanel::shellSingleQuoted(const QString &value) const
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

QString SketchSdkPanel::tableItemText(QTableWidget *table, int row, int column) const
{
    if (auto *item = table->item(row, column)) {
        return item->text();
    }

    return {};
}
