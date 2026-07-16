#include "workshopconfigpage.h"
#include "workshopsettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QIcon>

WorkshopConfigPage::WorkshopConfigPage(KDevelop::IPlugin* plugin, QWidget* parent)
    : KDevelop::ConfigPage(plugin, WorkshopSettings::self(), parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* descLabel = new QLabel(QStringLiteral("Configure the Canonical Workshop integration."), this);
    layout->addWidget(descLabel);

    auto* formLayout = new QHBoxLayout();
    formLayout->addWidget(new QLabel(QStringLiteral("workshopd Socket Path:"), this));

    m_socketPathEdit = new QLineEdit(this);
    m_socketPathEdit->setObjectName(QStringLiteral("kcfg_SocketPath"));
    m_socketPathEdit->setPlaceholderText(WorkshopSettings::defaultSocketPathValue());
    formLayout->addWidget(m_socketPathEdit);

    layout->addLayout(formLayout);
    layout->addStretch();
}

QString WorkshopConfigPage::name() const
{
    return QStringLiteral("Workshop");
}

QString WorkshopConfigPage::fullName() const
{
    return QStringLiteral("Canonical Workshop Runtimes");
}

QIcon WorkshopConfigPage::icon() const
{
    return QIcon::fromTheme(QStringLiteral("system-run"));
}

KDevelop::ConfigPage::ConfigPageType WorkshopConfigPage::configPageType() const
{
    return RuntimeConfigPage;
}
