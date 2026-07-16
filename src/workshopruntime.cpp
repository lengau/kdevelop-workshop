#include "workshopruntime.h"

#include <util/path.h>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <KProcess>

WorkshopRuntime::WorkshopRuntime(const QString& workshopName, const QString& projectPath, QObject* parent)
    : m_workshopName(workshopName)
    , m_projectPath(projectPath)
{
    setParent(parent);
}

WorkshopRuntime::~WorkshopRuntime() = default;

QString WorkshopRuntime::name() const
{
    return QStringLiteral("Workshop: ") + m_workshopName;
}

void WorkshopRuntime::startProcess(QProcess* process) const
{
    QString program = process->program();
    QStringList args = process->arguments();

    QStringList newArgs;
    newArgs << QStringLiteral("exec") << QStringLiteral("-w") << QStringLiteral("/project") 
            << m_workshopName << QStringLiteral("--") << program << args;
    
    process->setProgram(QStringLiteral("workshop"));
    process->setArguments(newArgs);
}

void WorkshopRuntime::startProcess(KProcess* process) const
{
    QStringList argv = process->program();
    QStringList newArgv;
    newArgv << QStringLiteral("workshop") << QStringLiteral("exec") << QStringLiteral("-w") << QStringLiteral("/project") 
            << m_workshopName << QStringLiteral("--") << argv;
    
    process->clearProgram();
    process->setProgram(newArgv);
}

KDevelop::Path WorkshopRuntime::pathInRuntime(const KDevelop::Path& localPath) const
{
    return localPath;
}

KDevelop::Path WorkshopRuntime::pathInHost(const KDevelop::Path& runtimePath) const
{
    return runtimePath;
}

QString WorkshopRuntime::findExecutable(const QString& executableName) const
{
    QString path = QStandardPaths::findExecutable(executableName);
    if (path.isEmpty()) {
        return executableName;
    }
    return path;
}

QByteArray WorkshopRuntime::getenv(const QByteArray& varname) const
{
    return qgetenv(varname.constData());
}

KDevelop::Path WorkshopRuntime::buildPath() const
{
    return KDevelop::Path();
}

void WorkshopRuntime::setEnabled(bool enabled)
{
    Q_UNUSED(enabled);
}
