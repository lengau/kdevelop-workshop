#ifndef WORKSHOPTOOLVIEW_H
#define WORKSHOPTOOLVIEW_H

#include <QWidget>
#include <QStringList>
#include <QMap>

class kdevelop_workshop;
class QTextEdit;
class QComboBox;
class QVBoxLayout;
class QLabel;
class QPushButton;
class QTimer;

struct WorkshopWidgets
{
    QLabel* statusLabel;
    QPushButton* actionBtn;
};

class WorkshopToolView : public QWidget
{
    Q_OBJECT
public:
    explicit WorkshopToolView(kdevelop_workshop* plugin, QWidget* parent = nullptr);

private Q_SLOTS:
    void refresh();
    void onProjectChanged(int index);
    void runCommand(const QString& cmd, const QStringList& args);
    void performAction(const QString& workshopName, const QString& action);
    void animateTransitions();
    void showContextMenu(const QString& workshopName, const QPoint& pos);
    void removeWorkshop(const QString& workshopName);

private:
    void clearLayout();
    void clearTransitionState(const QString& workshopName);
    void clearAllTransitionState();

    kdevelop_workshop* m_plugin;
    QTextEdit* m_output;
    QComboBox* m_projectCombo;
    QWidget* m_workshopsContainer;
    QVBoxLayout* m_workshopsLayout;

    QTimer* m_animationTimer;
    QTimer* m_pollTimer;
    bool m_refreshing;
    QMap<QString, QString>
        m_transitioningWorkshops; // name -> action/status ("Starting", "Stopping", "Removing", "Pending", etc.)
    QMap<QString, WorkshopWidgets> m_workshopWidgets; // name -> pointers
    QString m_projectId;
    QString m_lastProjectPath;
};

#endif // WORKSHOPTOOLVIEW_H
