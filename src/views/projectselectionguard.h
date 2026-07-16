#ifndef PROJECTSELECTIONGUARD_H
#define PROJECTSELECTIONGUARD_H

#include <QString>

namespace ProjectSelectionGuard {
bool selectionMatches(const QString& expectedProjectPath, const QString& currentProjectPath);
}

#endif // PROJECTSELECTIONGUARD_H
