#ifndef WORKSHOPRUNTIME_H
#define WORKSHOPRUNTIME_H

#include <interfaces/iruntime.h>
#include <QObject>
#include <QString>

class WorkshopRuntime : public KDevelop::IRuntime
{
    Q_OBJECT
public:
    explicit WorkshopRuntime(const QString& workshopName, const QString& projectPath, QObject* parent = nullptr);
    ~WorkshopRuntime() override;

    QString name() const override;
    void startProcess(QProcess* process) const override;
    void startProcess(KProcess* process) const override;
    KDevelop::Path pathInRuntime(const KDevelop::Path& localPath) const override;
    KDevelop::Path pathInHost(const KDevelop::Path& runtimePath) const override;
    QString findExecutable(const QString& executableName) const override;
    QByteArray getenv(const QByteArray& varname) const override;
    KDevelop::Path buildPath() const override;

protected:
    void setEnabled(bool enabled) override;

private:
    QString m_workshopName;
    QString m_projectPath;
};

#endif // WORKSHOPRUNTIME_H
