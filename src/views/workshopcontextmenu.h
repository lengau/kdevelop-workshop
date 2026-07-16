#ifndef WORKSHOPCONTEXTMENU_H
#define WORKSHOPCONTEXTMENU_H

#include <functional>

class QAction;
class QMenu;

struct WorkshopContextMenuActions
{
    QAction* editAction;
    QAction* sketchSdkAction;
    QAction* removeAction;
};

WorkshopContextMenuActions populateWorkshopContextMenu(QMenu* menu, const std::function<void()>& onEdit,
                                                       const std::function<void()>& onSketchSdk,
                                                       const std::function<void()>& onRemove);

#endif // WORKSHOPCONTEXTMENU_H
