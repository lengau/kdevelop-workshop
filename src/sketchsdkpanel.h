#ifndef SKETCHSDKPANEL_H
#define SKETCHSDKPANEL_H

#include "sketchsdkdata.h"

#include <QDialog>
#include <QProcess>
#include <memory>

class QDialogButtonBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTemporaryDir;

class SketchSdkPanel : public QDialog
{
    Q_OBJECT
public:
    explicit SketchSdkPanel(const QString &workshopName, const QString &projectPath, const QString &workshopStatus, QWidget *parent = nullptr);
    SketchSdkPanel(const QString &workshopName, const QString &projectPath, const QString &workshopStatus, const SketchSdkData &prefillData, QWidget *parent = nullptr);
    ~SketchSdkPanel() override;

Q_SIGNALS:
    void applied();
    void errorOccurred(const QString &error);

protected:
    void reject() override;

private Q_SLOTS:
    void addPlug();
    void removeSelectedPlug();
    void addSlot();
    void removeSelectedSlot();

    void applySketchSdk();
    void stashSketchSdk();
    void restoreSketchSdk();
    void ejectSketchSdk();
    void removeSketchSdk();

    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void updateActionState();

private:
    struct PendingCommand {
        QString runningMessage;
        QString successMessage;
        bool emitApplied = false;
        bool closeOnSuccess = false;
    };

    void setupUi();
    void populateFromPrefillData(const SketchSdkData &data);
    void addPlugRow(const QString &name = QString(),
                    const QString &interfaceName = QStringLiteral("mount"),
                    const QString &workshopTarget = QString());
    void addSlotRow(const QString &name = QString(),
                    const QString &interfaceName = QStringLiteral("tunnel"), int endpoint = 8080);
    void runWorkshopCommand(const QStringList &arguments, const PendingCommand &command,
                            const QString &editorScriptPath = QString());
    void setBusy(bool busy, const QString &message = QString());
    void finishWithError(const QString &error);
    void cleanupTemporaryFiles();
    bool validateForm(QString *errorMessage) const;
    QString generateYaml() const;
    QString shellSingleQuoted(const QString &value) const;
    QString tableItemText(QTableWidget *table, int row, int column) const;

    QString m_workshopName;
    QString m_projectPath;
    QString m_workshopStatus;

    QWidget *m_editorWidget = nullptr;
    QPlainTextEdit *m_setupBaseEdit = nullptr;
    QPlainTextEdit *m_setupProjectEdit = nullptr;
    QTableWidget *m_plugsTable = nullptr;
    QTableWidget *m_slotsTable = nullptr;
    QPushButton *m_addPlugButton = nullptr;
    QPushButton *m_removePlugButton = nullptr;
    QPushButton *m_addSlotButton = nullptr;
    QPushButton *m_removeSlotButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_stashButton = nullptr;
    QPushButton *m_restoreButton = nullptr;
    QPushButton *m_ejectButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    QProcess *m_process = nullptr;
    PendingCommand m_pendingCommand;
    QString m_workshopBinaryPath = QStringLiteral("workshop");
    QString m_standardOutput;
    QString m_standardError;
    std::unique_ptr<QTemporaryDir> m_tempDir;
};

#endif // SKETCHSDKPANEL_H
