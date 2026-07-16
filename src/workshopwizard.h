#ifndef WORKSHOPWIZARD_H
#define WORKSHOPWIZARD_H

#include <QWizard>
#include <QWizardPage>
#include <QStringList>
#include <QMap>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QListWidget;
class QTextEdit;
class QPushButton;

class WorkshopWizard : public QWizard
{
    Q_OBJECT
public:
    explicit WorkshopWizard(const QString& projectPath, const QString& existingName = QString(), QWidget* parent = nullptr);

    QString workshopName() const;
    QString baseImage() const;
    QStringList selectedSdks() const;

    QString existingName() const { return m_existingName; }
    QString existingBase() const { return m_existingBase; }
    QStringList existingSdks() const { return m_existingSdks; }

private:
    void parseExistingYaml();

    QString m_projectPath;
    QString m_existingName;
    QString m_existingBase;
    QStringList m_existingSdks;
};

class GeneralPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit GeneralPage(QWidget* parent = nullptr);
    void initializePage() override;

    QLineEdit* m_nameEdit;
    QComboBox* m_baseCombo;
};

class SdkPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit SdkPage(QWidget* parent = nullptr);
    void initializePage() override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    QStringList m_selectedStoreSdks;
    QMap<QString, QString> m_searchSdkMap;      // display text -> name
    QMap<QString, QString> m_sdkSummaries;       // name -> summary
    QStringList m_lastSearchResults;             // names matching the last search query

private Q_SLOTS:
    void performSearch();

private:
    QLineEdit* m_searchEdit;
    QPushButton* m_searchBtn;
    QListWidget* m_searchResultsList;
};

class ReviewPage : public QWizardPage
{
    Q_OBJECT
public:
    ReviewPage(const QString& projectPath, WorkshopWizard* wizard, QWidget* parent = nullptr);
    void initializePage() override;
    bool validatePage() override;

private:
    QString m_projectPath;
    WorkshopWizard* m_wizard;
    QTextEdit* m_previewEdit;
};

#endif // WORKSHOPWIZARD_H
