import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons as Commons

Item {
    id: root

    property var shell: null
    property var manifest: null
    readonly property string pluginId: "omasnap.window-snap"
    readonly property string pluginDir: String((root.manifest && root.manifest.__sourceDir)
        || (Quickshell.env("HOME") + "/.config/omarchy/plugins/" + root.pluginId))
    property var settings: ({})
    property bool ready: false
    readonly property color accent: Commons.Color.accent

    function configure() {
        if (!root.ready) return;
        var value = {};
        for (var key in root.settings) value[key] = root.settings[key];
        value.color = String(root.accent);
        controller.write(JSON.stringify(value) + "\n");
    }

    onSettingsChanged: configure()
    onAccentChanged: configure()

    FileView {
        id: configFile
        path: Quickshell.env("HOME") + "/.config/omarchy/shell.json"
        watchChanges: true
        printErrors: false
        onFileChanged: Qt.callLater(configFile.reload)
        onLoaded: {
            try {
                var entries = JSON.parse(text()).plugins || [];
                var value = {};
                for (var i = 0; i < entries.length; ++i)
                    if (entries[i].id === root.pluginId) value = entries[i];
                root.settings = value;
            } catch (error) {
                console.warn("Omasnap: could not read plugin settings: " + error);
            }
        }
    }

    Process {
        id: controller
        command: ["python3", root.pluginDir + "/native-controller", "run"]
        stdinEnabled: true
        stdout: SplitParser {
            onRead: function(line) {
                if (line === "ready") { root.ready = true; root.configure(); }
            }
        }
        stderr: SplitParser {
            onRead: function(line) { console.warn(line); }
        }
        onExited: function(exitCode) {
            root.ready = false;
            if (exitCode !== 0) console.warn("Omasnap: controller exited with code " + exitCode);
        }
    }

    Component.onCompleted: Qt.callLater(function() { controller.running = true; })
    Component.onDestruction: {
        if (controller.running) controller.write("quit\n");
    }
}
