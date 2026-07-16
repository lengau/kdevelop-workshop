#include "workshopconfigpage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QIcon>
#include <KCoreConfigSkeleton>
#include <KSharedConfig>
#include <KConfigGroup>

WorkshopConfigPage::WorkshopConfigPage(KDevelop::IPlugin* plugin, QWidget* parent)
    : KDevelop::ConfigPage(plugin, new KCoreConfigSkeleton(QStringLiteral("kdevelop-workshoprc"), parent), parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* descLabel = new QLabel(QStringLiteral("Configure the Canonical Workshop integration."), this);
    layout->addWidget(descLabel);

    auto* formLayout = new QHBoxLayout();
    formLayout->addWidget(new QLabel(QStringLiteral("workshopd Socket Path:"), this));

    m_socketPathEdit = new QLineEdit(this);
    m_socketPathEdit->setPlaceholderText(QStringLiteral("/var/snap/workshop/common/workshop/workshop.socket"));
    formLayout->addWidget(m_socketPathEdit);

    layout->addLayout(formLayout);
    layout->addStretch();

    // Signal KDevelop that settings have changed when editing the text field
    connect(m_socketPathEdit, &QLineEdit::textChanged, this, &WorkshopConfigPage::changed);

    reset();
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

void WorkshopConfigPage::apply()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kdevelop-workshoprc"));
    KConfigGroup group = config->group(QStringLiteral("General"));
    group.writeEntry("SocketPath", m_socketPathEdit->text().trimmed());
    group.sync();
}

void WorkshopConfigPage::defaults()
{
    m_socketPathEdit->clear();
}

void WorkshopConfigPage::reset()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kdevelop-workshoprc"));
    KConfigGroup group = config->group(QStringLiteral("General"));
    m_socketPathEdit->setText(group.readEntry("SocketPath", QString()));
}
