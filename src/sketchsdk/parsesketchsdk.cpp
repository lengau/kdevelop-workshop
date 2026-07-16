#include "parsesketchsdk.h"

#include "debug.h"

#include <yaml-cpp/yaml.h>

namespace {
std::string toYamlString(const QString& value)
{
    return value.toUtf8().toStdString();
}

QString fromYamlString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QString scalarValue(const YAML::Node& node)
{
    if (!node || !node.IsScalar()) {
        return {};
    }

    return fromYamlString(node.as<std::string>());
}

QString trimmedScalarValue(const YAML::Node& node)
{
    return scalarValue(node).trimmed();
}

YAML::Node selectSketchNode(const YAML::Node& root, bool requireSketchName)
{
    if (!root) {
        return {};
    }

    if (root.IsMap()) {
        const QString name = trimmedScalarValue(root["name"]);
        if (!requireSketchName || name == QStringLiteral("sketch")) {
            return root;
        }
        return {};
    }

    if (!root.IsSequence()) {
        return {};
    }

    for (const auto& item : root) {
        if (!item || !item.IsMap()) {
            continue;
        }

        const QString name = trimmedScalarValue(item["name"]);
        if (name == QStringLiteral("sketch") || (!requireSketchName && name.isEmpty())) {
            return item;
        }
    }

    if (!requireSketchName) {
        for (const auto& item : root) {
            if (item && item.IsMap()) {
                return item;
            }
        }
    }

    return {};
}

SketchSdkData parseSketchNode(const YAML::Node& node)
{
    SketchSdkData data;
    if (!node || !node.IsMap()) {
        return data;
    }

    const YAML::Node hooks = node["hooks"];
    if (hooks && hooks.IsMap()) {
        data.setupBase = trimmedScalarValue(hooks["setup-base"]);
        data.setupProject = trimmedScalarValue(hooks["setup-project"]);
    }

    const YAML::Node plugs = node["plugs"];
    if (plugs && plugs.IsMap()) {
        for (auto it = plugs.begin(); it != plugs.end(); ++it) {
            const QString name = trimmedScalarValue(it->first);
            const YAML::Node plugNode = it->second;
            if (name.isEmpty() || !plugNode || !plugNode.IsMap()) {
                continue;
            }

            SketchSdkData::Plug plug;
            const YAML::Node interfaceNode = plugNode["interface"];
            if (interfaceNode && interfaceNode.IsScalar()) {
                plug.interfaceName = trimmedScalarValue(interfaceNode);
            }

            const YAML::Node targetNode = plugNode["workshop-target"];
            if (targetNode && targetNode.IsScalar()) {
                plug.workshopTarget = trimmedScalarValue(targetNode);
            }
            data.plugs.insert(name, plug);
        }
    }

    const YAML::Node slots = node["slots"];
    if (slots && slots.IsMap()) {
        for (auto it = slots.begin(); it != slots.end(); ++it) {
            const QString name = trimmedScalarValue(it->first);
            const YAML::Node slotNode = it->second;
            if (name.isEmpty() || !slotNode || !slotNode.IsMap()) {
                continue;
            }

            SketchSdkData::Slot slot;
            const YAML::Node interfaceNode = slotNode["interface"];
            if (interfaceNode && interfaceNode.IsScalar()) {
                slot.interfaceName = trimmedScalarValue(interfaceNode);
            }

            const YAML::Node endpointNode = slotNode["endpoint"];
            if (endpointNode && endpointNode.IsScalar()) {
                const QString endpointString = trimmedScalarValue(endpointNode);
                bool ok = false;
                const int endpoint = endpointString.toInt(&ok);
                if (ok) {
                    slot.endpoint = endpoint;
                }
            }

            data.slots.insert(name, slot);
        }
    }

    data.setupBase = data.setupBase.trimmed();
    data.setupProject = data.setupProject.trimmed();

    return data;
}

QString serializeSketchNode(const SketchSdkData& data)
{
    YAML::Node root;
    root["name"] = toYamlString(QStringLiteral("sketch"));

    YAML::Node hooks;
    hooks["setup-base"] = toYamlString(data.setupBase);
    hooks["setup-project"] = toYamlString(data.setupProject);
    root["hooks"] = hooks;

    YAML::Node plugs(YAML::NodeType::Map);
    for (auto it = data.plugs.cbegin(); it != data.plugs.cend(); ++it) {
        YAML::Node plug;
        plug["interface"] = toYamlString(it.value().interfaceName);
        plug["workshop-target"] = toYamlString(it.value().workshopTarget);
        plugs[toYamlString(it.key())] = plug;
    }
    root["plugs"] = plugs;

    YAML::Node slots(YAML::NodeType::Map);
    for (auto it = data.slots.cbegin(); it != data.slots.cend(); ++it) {
        YAML::Node slot;
        slot["interface"] = toYamlString(it.value().interfaceName);
        slot["endpoint"] = it.value().endpoint;
        slots[toYamlString(it.key())] = slot;
    }
    root["slots"] = slots;

    YAML::Emitter emitter;
    emitter << root;
    return QString::fromUtf8(emitter.c_str(), static_cast<int>(emitter.size()));
}

SketchSdkData parseYaml(const QStringList& lines, bool requireSketchName, const char* context)
{
    const QString yamlText = lines.join(QLatin1Char('\n'));
    if (yamlText.trimmed().isEmpty()) {
        return {};
    }

    try {
        const YAML::Node root = YAML::Load(yamlText.toUtf8().toStdString());
        return parseSketchNode(selectSketchNode(root, requireSketchName));
    } catch (const YAML::Exception& e) {
        qCWarning(PLUGIN_KDEVELOP_WORKSHOP) << context << "failed to parse YAML:" << e.what();
        return {};
    }
}
}

SketchSdkData parseSketchSdk(const QStringList& lines)
{
    return parseYaml(lines, true, "parseSketchSdk");
}

QString serializeSketchSdk(const SketchSdkData& data)
{
    return serializeSketchNode(data);
}

SketchSdkData parseSketchSdkMap(const QStringList& lines)
{
    return parseYaml(lines, false, "parseSketchSdkMap");
}
