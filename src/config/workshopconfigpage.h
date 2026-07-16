#ifndef WORKSHOPCONFIGPAGE_H
#define WORKSHOPCONFIGPAGE_H

#include <interfaces/configpage.h>

class QLineEdit;

class WorkshopConfigPage : public KDevelop::ConfigPage
{
    Q_OBJECT
public:
    explicit WorkshopConfigPage(KDevelop::IPlugin* plugin, QWidget* parent = nullptr);

    QString name() const override;
    QString fullName() const override;
    QIcon icon() const override;
    ConfigPageType configPageType() const override;

private:
    QLineEdit* m_socketPathEdit;
};

#endif // WORKSHOPCONFIGPAGE_H
