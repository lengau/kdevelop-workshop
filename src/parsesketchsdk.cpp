#include "parsesketchsdk.h"
#include <QRegularExpression>
#include <QDebug>

SketchSdkData parseSketchSdk(const QStringList &lines)
{
    SketchSdkData data;
    bool inSketchSdk = false;
    bool inHooks = false;
    bool inPlugs = false;
    bool inSlots = false;
    
    QString currentItemName;
    
    // For multiline strings (hooks)
    bool inSetupBaseLiteral = false;
    bool inSetupProjectLiteral = false;
    int baseIndent = 0;
    
    for (int i = 0; i < lines.count(); ++i) {
        QString line = lines.at(i);
        QString trimmed = line.trimmed();
        
        int indent = 0;
        while (indent < line.length() && line.at(indent).isSpace()) {
            indent++;
        }
        
        // Handle multiline literal blocks for setup-base / setup-project
        if (inSetupBaseLiteral || inSetupProjectLiteral) {
            const bool isEnd = !trimmed.isEmpty() && indent <= baseIndent;
            if (!isEnd) {
                QString contentLine = (line.length() > baseIndent + 2) ? line.mid(baseIndent + 2) : QString();
                if (inSetupBaseLiteral) {
                    data.setupBase.append(contentLine + QLatin1Char('\n'));
                } else {
                    data.setupProject.append(contentLine + QLatin1Char('\n'));
                }
                continue;
            } else {
                inSetupBaseLiteral = false;
                inSetupProjectLiteral = false;
            }
        }
        
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }
        
        if (trimmed.startsWith(QLatin1String("- name:"))) {
            QString val = trimmed.mid(7).trimmed();
            if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"'))) val = val.mid(1, val.length() - 2);
            if (val.startsWith(QLatin1Char('\'')) && val.endsWith(QLatin1Char('\''))) val = val.mid(1, val.length() - 2);
            if (val == QLatin1String("sketch")) {
                inSketchSdk = true;
            } else {
                inSketchSdk = false;
            }
            inHooks = false;
            inPlugs = false;
            inSlots = false;
            continue;
        }
        
        if (!inSketchSdk) continue;
        
        if (trimmed.startsWith(QLatin1String("hooks:"))) {
            inHooks = true;
            inPlugs = false;
            inSlots = false;
            continue;
        }
        
        if (trimmed.startsWith(QLatin1String("plugs:"))) {
            inHooks = false;
            inPlugs = true;
            inSlots = false;
            continue;
        }
        
        if (trimmed.startsWith(QLatin1String("slots:"))) {
            inHooks = false;
            inPlugs = false;
            inSlots = true;
            continue;
        }
        
        if (inHooks) {
            if (trimmed.startsWith(QLatin1String("setup-base:"))) {
                QString right = trimmed.mid(11).trimmed();
                if (right == QLatin1String("|")) {
                    inSetupBaseLiteral = true;
                    baseIndent = indent;
                    data.setupBase.clear();
                } else {
                    data.setupBase = right;
                }
            } else if (trimmed.startsWith(QLatin1String("setup-project:"))) {
                QString right = trimmed.mid(14).trimmed();
                if (right == QLatin1String("|")) {
                    inSetupProjectLiteral = true;
                    baseIndent = indent;
                    data.setupProject.clear();
                } else {
                    data.setupProject = right;
                }
            }
        } else if (inPlugs) {
            if (trimmed.endsWith(QLatin1Char(':'))) {
                currentItemName = trimmed.left(trimmed.length() - 1).trimmed();
                data.plugs[currentItemName] = SketchSdkData::Plug{};
            } else if (!currentItemName.isEmpty()) {
                if (trimmed.startsWith(QLatin1String("interface:"))) {
                    QString val = trimmed.mid(10).trimmed();
                    if (val.startsWith(QLatin1Char('"')) || val.startsWith(QLatin1Char('\''))) val = val.mid(1, val.length() - 2);
                    data.plugs[currentItemName].interfaceName = val;
                } else if (trimmed.startsWith(QLatin1String("workshop-target:"))) {
                    QString val = trimmed.mid(16).trimmed();
                    if (val.startsWith(QLatin1Char('"')) || val.startsWith(QLatin1Char('\''))) val = val.mid(1, val.length() - 2);
                    data.plugs[currentItemName].workshopTarget = val;
                }
            }
        } else if (inSlots) {
            if (trimmed.endsWith(QLatin1Char(':'))) {
                currentItemName = trimmed.left(trimmed.length() - 1).trimmed();
                data.slots[currentItemName] = SketchSdkData::Slot{};
            } else if (!currentItemName.isEmpty()) {
                if (trimmed.startsWith(QLatin1String("interface:"))) {
                    QString val = trimmed.mid(10).trimmed();
                    if (val.startsWith(QLatin1Char('"')) || val.startsWith(QLatin1Char('\''))) val = val.mid(1, val.length() - 2);
                    data.slots[currentItemName].interfaceName = val;
                } else if (trimmed.startsWith(QLatin1String("endpoint:"))) {
                    data.slots[currentItemName].endpoint = trimmed.mid(9).trimmed().toInt();
                }
            }
        }
    }
    
    data.setupBase = data.setupBase.trimmed();
    data.setupProject = data.setupProject.trimmed();
    
    return data;
}

QString serializeSketchSdk(const SketchSdkData &data)
{
    QString out;
    out.append(QStringLiteral("name: sketch\n"));
    out.append(QStringLiteral("hooks:\n"));
    
    out.append(QStringLiteral("  setup-base: |\n"));
    const QStringList baseLines = data.setupBase.split(QLatin1Char('\n'));
    for (const QString& line : baseLines) {
        out.append(QStringLiteral("    %1\n").arg(line));
    }
    
    out.append(QStringLiteral("  setup-project: |\n"));
    const QStringList projLines = data.setupProject.split(QLatin1Char('\n'));
    for (const QString& line : projLines) {
        out.append(QStringLiteral("    %1\n").arg(line));
    }
    
    out.append(QStringLiteral("plugs:\n"));
    if (data.plugs.isEmpty()) {
        out.append(QStringLiteral("  {}\n"));
    } else {
        for (auto it = data.plugs.cbegin(); it != data.plugs.cend(); ++it) {
            out.append(QStringLiteral("  %1:\n").arg(it.key()));
            out.append(QStringLiteral("    interface: '%1'\n").arg(it.value().interfaceName));
            out.append(QStringLiteral("    workshop-target: '%1'\n").arg(it.value().workshopTarget));
        }
    }
    
    out.append(QStringLiteral("slots:\n"));
    if (data.slots.isEmpty()) {
        out.append(QStringLiteral("  {}\n"));
    } else {
        for (auto it = data.slots.cbegin(); it != data.slots.cend(); ++it) {
            out.append(QStringLiteral("  %1:\n").arg(it.key()));
            out.append(QStringLiteral("    interface: '%1'\n").arg(it.value().interfaceName));
            out.append(QStringLiteral("    endpoint: %1\n").arg(it.value().endpoint));
        }
    }
    
    return out;
}

SketchSdkData parseSketchSdkMap(const QStringList &lines)
{
    SketchSdkData data;
    bool inHooks = false;
    bool inPlugs = false;
    bool inSlots = false;
    
    QString currentItemName;
    
    // For multiline strings (hooks)
    bool inSetupBaseLiteral = false;
    bool inSetupProjectLiteral = false;
    int baseIndent = 0;
    
    for (int i = 0; i < lines.count(); ++i) {
        QString line = lines.at(i);
        QString trimmed = line.trimmed();
        
        int indent = 0;
        while (indent < line.length() && line.at(indent).isSpace()) {
            indent++;
        }
        
        // Handle multiline literal blocks for setup-base / setup-project
        if (inSetupBaseLiteral || inSetupProjectLiteral) {
            const bool isEnd = !trimmed.isEmpty() && indent <= baseIndent;
            if (!isEnd) {
                QString contentLine = (line.length() > baseIndent + 2) ? line.mid(baseIndent + 2) : QString();
                if (inSetupBaseLiteral) {
                    data.setupBase.append(contentLine + QLatin1Char('\n'));
                } else {
                    data.setupProject.append(contentLine + QLatin1Char('\n'));
                }
                continue;
            } else {
                inSetupBaseLiteral = false;
                inSetupProjectLiteral = false;
            }
        }
        
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }
        
        if (trimmed.startsWith(QLatin1String("hooks:"))) {
            inHooks = true;
            inPlugs = false;
            inSlots = false;
            continue;
        }
        
        if (trimmed.startsWith(QLatin1String("plugs:"))) {
            inHooks = false;
            inPlugs = true;
            inSlots = false;
            continue;
        }
        
        if (trimmed.startsWith(QLatin1String("slots:"))) {
            inHooks = false;
            inPlugs = false;
            inSlots = true;
            continue;
        }
        
        if (inHooks) {
            if (trimmed.startsWith(QLatin1String("setup-base:"))) {
                QString right = trimmed.mid(11).trimmed();
                if (right == QLatin1String("|")) {
                    inSetupBaseLiteral = true;
                    baseIndent = indent;
                    data.setupBase.clear();
                } else {
                    data.setupBase = right;
                }
            } else if (trimmed.startsWith(QLatin1String("setup-project:"))) {
                QString right = trimmed.mid(14).trimmed();
                if (right == QLatin1String("|")) {
                    inSetupProjectLiteral = true;
                    baseIndent = indent;
                    data.setupProject.clear();
                } else {
                    data.setupProject = right;
                }
            }
        } else if (inPlugs) {
            if (trimmed.endsWith(QLatin1Char(':'))) {
                currentItemName = trimmed.left(trimmed.length() - 1).trimmed();
                data.plugs[currentItemName] = SketchSdkData::Plug{};
            } else if (!currentItemName.isEmpty()) {
                if (trimmed.startsWith(QLatin1String("interface:"))) {
                    QString val = trimmed.mid(10).trimmed();
                    if (val.startsWith(QLatin1Char('"')) || val.startsWith(QLatin1Char('\''))) val = val.mid(1, val.length() - 2);
                    data.plugs[currentItemName].interfaceName = val;
                } else if (trimmed.startsWith(QLatin1String("workshop-target:"))) {
                    QString val = trimmed.mid(16).trimmed();
                    if (val.startsWith(QLatin1Char('"')) || val.startsWith(QLatin1Char('\''))) val = val.mid(1, val.length() - 2);
                    data.plugs[currentItemName].workshopTarget = val;
                }
            }
        } else if (inSlots) {
            if (trimmed.endsWith(QLatin1Char(':'))) {
                currentItemName = trimmed.left(trimmed.length() - 1).trimmed();
                data.slots[currentItemName] = SketchSdkData::Slot{};
            } else if (!currentItemName.isEmpty()) {
                if (trimmed.startsWith(QLatin1String("interface:"))) {
                    QString val = trimmed.mid(10).trimmed();
                    if (val.startsWith(QLatin1Char('"')) || val.startsWith(QLatin1Char('\''))) val = val.mid(1, val.length() - 2);
                    data.slots[currentItemName].interfaceName = val;
                } else if (trimmed.startsWith(QLatin1String("endpoint:"))) {
                    data.slots[currentItemName].endpoint = trimmed.mid(9).trimmed().toInt();
                }
            }
        }
    }
    
    data.setupBase = data.setupBase.trimmed();
    data.setupProject = data.setupProject.trimmed();
    
    return data;
}
