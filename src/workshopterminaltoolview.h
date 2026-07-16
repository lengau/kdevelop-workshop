#ifndef WORKSHOPTERMINALTOOLVIEW_H
#define WORKSHOPTERMINALTOOLVIEW_H

#include <QWidget>

class kdevelop_workshop;
class QComboBox;
class QPushButton;
class QVBoxLayout;

namespace KParts {
    class ReadOnlyPart;
}

class WorkshopTerminalToolView : public QWidget
{
    Q_OBJECT
public:
    explicit WorkshopTerminalToolView(kdevelop_workshop* plugin, QWidget* parent = nullptr);
    ~WorkshopTerminalToolView() override;

private Q_SLOTS:
    void refresh();
    void onProjectChanged(int index);
    void connectToWorkshop(const QString& workshopName);
    void updateWorkshopState();
    void startWorkshop();

private:
    void clearTerminal();
    void showStatusMessage(const QString& html);

    kdevelop_workshop* m_plugin;
    QComboBox* m_projectCombo;
    QComboBox* m_workshopCombo;
    QPushButton* m_startBtn;
    QWidget* m_terminalContainer;
    QVBoxLayout* m_terminalLayout;
    class QLabel* m_statusMessageLabel = nullptr;
    KParts::ReadOnlyPart* m_part = nullptr;
    QString m_connectedWorkshop;
    bool m_refreshing;
};

#endif // WORKSHOPTERMINALTOOLVIEW_H
