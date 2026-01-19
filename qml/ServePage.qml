import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import QtQuick.Dialogs

ColumnLayout {
    spacing: 15
    Layout.margins: 20

    GroupBox {
        title: "捕获源与全局设置"
        Layout.fillWidth: true
        
        GridLayout {
            columns: 3
            anchors.fill: parent
            columnSpacing: 10
            
            Label { text: "输入源:" }
            TextField {
                id: serveSource
                text: "screen"
                Layout.fillWidth: true
                placeholderText: "screen, video.mp4, 或 rtsp://..."
            }
            Button {
                text: "浏览文件..."
                onClicked: serveFileDialog.open()
            }
        }
    }

    RowLayout {
        spacing: 15
        Layout.fillWidth: true

        GroupBox {
            title: "传输协议"
            Layout.fillWidth: true
            
            GridLayout {
                columns: 2
                anchors.fill: parent
                
                Label { text: "协议类型:" }
                ComboBox {
                    id: protocolType
                    model: ["RTSP Server", "UDP Push", "RTP Push", "RTSP Push"]
                    Layout.fillWidth: true
                    onCurrentIndexChanged: {
                        // 根据协议类型显示/隐藏不同面板
                    }
                }
            }
        }

        GroupBox {
            title: "协议设置"
            Layout.fillWidth: true
            
            StackLayout {
                id: protocolSettings
                currentIndex: protocolType.currentIndex
                anchors.fill: parent

                // RTSP Server
                GridLayout {
                    columns: 2
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

                // UDP Push
                GridLayout {
                    columns: 2
                    Label { text: "地址:" }
                    TextField {
                        id: udpAddress
                        text: "127.0.0.1"
                        Layout.fillWidth: true
                    }
                    Label { text: "端口:" }
                    SpinBox {
                        id: udpPort
                        from: 1; to: 65535; value: 1234
                        editable: true
                        Layout.fillWidth: true
                    }
                }

                // RTP Push
                GridLayout {
                    columns: 2
                    Label { text: "地址:" }
                    TextField {
                        id: rtpAddress
                        text: "127.0.0.1"
                        Layout.fillWidth: true
                    }
                    Label { text: "端口:" }
                    SpinBox {
                        id: rtpPort
                        from: 1; to: 65535; value: 5004
                        editable: true
                        Layout.fillWidth: true
                    }
                }

                // RTSP Push
                GridLayout {
                    columns: 2
                    Label { text: "目标URL:" }
                    TextField {
                        id: rtspPushUrl
                        text: "rtsp://127.0.0.1:8554/live"
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }

    GroupBox {
        title: "硬件与编码设置"
        Layout.fillWidth: true
        
        GridLayout {
            columns: 4
            anchors.fill: parent
            
            Label { text: "编码方式:" }
            ComboBox {
                id: codecSelect
                model: ["H.264", "H.265", "MJPEG", "AV1"]
                Layout.fillWidth: true
                currentIndex: 0
            }

            Label { text: "硬件框架:" }
            ComboBox {
                id: serveHw
                model: bridge.hwTypes
                Layout.fillWidth: true
            }

            Label { text: "具体编码器:" }
            ComboBox {
                id: serveEnc
                model: bridge.getEncoders(codecSelect.currentText, serveHw.currentText)
                Layout.fillWidth: true
            }

            Label { text: "帧率 (FPS):" }
            SpinBox {
                id: serveFps
                from: 1; to: 165; value: 30
                editable: true
                Layout.fillWidth: true
            }

            Label { text: "延迟等级:" }
            ComboBox {
                id: latencyLevel
                model: ["极低 (Ultra-Low)", "平衡 (Low)", "稳定 (Standard)"]
                currentIndex: 1
                Layout.fillWidth: true
            }

            CheckBox {
                id: echoEnable
                text: "开启回显 (本地预览)"
                checked: true
                Layout.columnSpan: 2
            }
        }
    }

    Button {
        text: "开始推流 / 启动服务"
        highlighted: true
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 300
        onClicked: {
            let hw = serveHw.currentText === "None" ? "" : serveHw.currentText
            let fps = serveFps.value
            let lat = latencyLevel.currentIndex
            let echo = echoEnable.checked
            if (protocolType.currentText === "RTSP Server") {
                bridge.startServe(serveSource.text, servePort.value, serveName.text, serveEnc.currentText, hw, fps, lat, echo)
            } else if (protocolType.currentText === "UDP Push") {
                bridge.startPush(serveSource.text, "udp://" + udpAddress.text + ":" + udpPort.value, serveEnc.currentText, hw, fps, lat, echo)
            } else if (protocolType.currentText === "RTP Push") {
                bridge.startPush(serveSource.text, "rtp://" + rtpAddress.text + ":" + rtpPort.value, serveEnc.currentText, hw, fps, lat, echo)
            } else if (protocolType.currentText === "RTSP Push") {
                bridge.startPush(serveSource.text, rtspPushUrl.text, serveEnc.currentText, hw, fps, lat, echo)
            }
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

    FileDialog {
        id: serveFileDialog
        title: "选择媒体文件"
        onAccepted: {
            serveSource.text = bridge.urlToPath(selectedFile);
        }
    }
}
