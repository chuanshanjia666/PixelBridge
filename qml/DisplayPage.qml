import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: displayRoot
    color: "#000000"
    radius: 8
    clip: true

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    Rectangle {
        anchors.fill: parent
        color: "#111"
        visible: !videoOutput.visible
        
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 15
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "📺"
                font.pixelSize: 64
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "准备就绪 - 等待传输启动"
                color: window.colorSecondary
                font.pixelSize: 18
                font.bold: true
            }
        }
    }

    // Connect bridge video sink to this output
    Component.onCompleted: {
        bridge.videoSink = videoOutput.videoSink
    }

    // Re-bind whenever this page becomes visible or active
    Connections {
        target: displayRoot
        function onVisibleChanged() {
            if (displayRoot.visible) {
                bridge.videoSink = videoOutput.videoSink
            }
        }
    }
}
