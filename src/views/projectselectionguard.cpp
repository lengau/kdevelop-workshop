#include "projectselectionguard.h"

namespace ProjectSelectionGuard {

bool selectionMatches(const QString& expectedProjectPath, const QString& currentProjectPath)
{
    return expectedProjectPath == currentProjectPath;
}

}
