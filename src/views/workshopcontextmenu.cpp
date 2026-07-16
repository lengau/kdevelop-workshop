#include "workshopcontextmenu.h"

#include <QAction>
#include <QIcon>
#include <QMenu>

WorkshopContextMenuActions populateWorkshopContextMenu(QMenu* menu, const std::function<void()>& onEdit,
                                                       const std::function<void()>& onSketchSdk,
                                                       const std::function<void()>& onRemove)
{
    auto* editAction =
        menu->addAction(QIcon::fromTheme(QStringLiteral("document-edit")), QStringLiteral("Edit Workshop"));
    QObject::connect(editAction, &QAction::triggered, menu, [onEdit]() {
        onEdit();
    });

    auto* sketchSdkAction =
        menu->addAction(QIcon::fromTheme(QStringLiteral("document-edit-sign")), QStringLiteral("Sketch SDK..."));
    QObject::connect(sketchSdkAction, &QAction::triggered, menu, [onSketchSdk]() {
        onSketchSdk();
    });

    menu->addSeparator();

    auto* removeAction =
        menu->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), QStringLiteral("Remove Workshop"));
    QObject::connect(removeAction, &QAction::triggered, menu, [onRemove]() {
        onRemove();
    });

    return {editAction, sketchSdkAction, removeAction};
}
