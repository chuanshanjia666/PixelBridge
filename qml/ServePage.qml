import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

ColumnLayout {
    spacing: 15
    Layout.margins: 20

    GroupBox {
        title: "捕获源与全局设置"
        Layout.fillWidth: true
        
        GridLayout {
            columns: 2
            anchors.fill: parent
            
            Label { text: "输入源:" }
            TextField {
                id: serveSource
                text: "screen"
                Layout.fillWidth: true
                placeholderText: "screen, video.mp4, 或 rtsp://..."
            }
        }
    }

    RowLayout {
        spacing: 15
        Layout.fillWidth: true

        GroupBox {
            title: "RTSP 服务端配置"
            Layout.fillWidth: true
            
            GridLayout {
                columns: 2
                anchors.fill: parent
                
                Label { text: "端口:" }
                SpinBox {
                    id: servePort
                    from: 1024; to: 65535; value: 8554
                    editable: true
                    Layout.fillWidth: true
                }
                
                Label { text: "流名称:" }
                TextField {
                    id: serveName
                    text: "live"
                    Layout.fillWidth: true
                }
            }
        }

        GroupBox {
            title: "编码设置"
            Layout.fillWidth: true
            
            GridLayout {
                columns: 2
                anchors.fill: parent
                
                Label { text: "编码器:" }
                ComboBox {
                    id: serveEnc
                    model: ["libx264", "h264_nvenc", "h264_vaapi"]
                    Layout.fillWidth: true
                }
                
                Label { text: "硬件框架:" }
                ComboBox {
                    id: serveHw
                    model: bridge.hwTypes
                    Layout.fillWidth: true
                }
            }
        }
    }

    Button {
        text: "启动服务"
        highlighted: true
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 200
        onClicked: {
            let hw = serveHw.currentText === "None" ? "" : serveHw.currentText
            bridge.startServe(serveSource.text, servePort.value, serveName.text, serveEnc.currentText, hw)
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: "#1a1a1a"
        radius: 4
        clip: true

        VideoOutput {
            id: serveOutput
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
        }

        Text {
            visible: !serveOutput.visible
            anchors.centerIn: parent
            text: "预览未启动"
            color: "#444"
            font.pixelSize: 24
        }

        Connections {
            target: parent
            function onVisibleChanged() {
                if (parent.visible) bridge.videoSink = serveOutput.videoSink
            }
        }
    }
}
