import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import QtQuick.Dialogs
import QtCore

ColumnLayout {
    spacing: 15
    Layout.margins: 20

    GroupBox {
        title: "播放源配置"
        Layout.fillWidth: true
        
        GridLayout {
            columns: 3
            anchors.fill: parent
            columnSpacing: 10
            rowSpacing: 10

            Label { text: "URL / 文件:" }
            TextField {
                id: playUrl
                placeholderText: "输入 RTSP 地址或选择本地文件"
                Layout.fillWidth: true
                text: ""
            }
            Button {
                text: "浏览..."
                onClicked: fileDialog.open()
            }

            Label { text: "硬件加速:" }
            ComboBox {
                id: playHw
                model: bridge.hwTypes
                Layout.columnSpan: 2
                Layout.fillWidth: true
            }

            Label { text: "延迟模式:" }
            ComboBox {
                id: playLatency
                model: ["极低 (Ultra-Low)", "平衡 (Low)", "稳定 (Standard)"]
                currentIndex: 1
                Layout.columnSpan: 2
                Layout.fillWidth: true
            }
        }
    }

    Button {
        text: "开始播放"
        highlighted: true
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 200
        onClicked: {
            let hw = playHw.currentText === "None" ? "" : playHw.currentText
            bridge.startPlay(playUrl.text, hw, playLatency.currentIndex)
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: "#1a1a1a"
        radius: 4
        clip: true

        VideoOutput {
            id: playOutput
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
        }
        
        Text {
            visible: !playOutput.visible || playUrl.text === ""
            anchors.centerIn: parent
            text: "无信号"
            color: "#444"
            font.pixelSize: 24
        }
        
        Component.onCompleted: {
            // This is a bit tricky with shared bridge. We rely on Page visibility or active status.
            if (parent.visible) bridge.videoSink = playOutput.videoSink
        }
        
        Connections {
            target: parent
            function onVisibleChanged() {
                if (parent.visible) bridge.videoSink = playOutput.videoSink
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "选择视频文件"
        onAccepted: {
            playUrl.text = bridge.urlToPath(selectedFile);
        }
    }
}
