import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore

ScrollView {
    id: settingsScroll
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: parent.width
        spacing: 25
        Layout.margins: 30

        // SECTION: Playback Settings
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: playColumn.implicitHeight + 60
            color: window.colorSurface
            radius: 12
            border.color: "#333"

            ColumnLayout {
                id: playColumn
                anchors.fill: parent
                anchors.margins: 30
                spacing: 25

                RowLayout {
                    spacing: 12
                    Text { text: "▶️"; font.pixelSize: 22 }
                    Text { 
                        text: "播放设置 (Playback)"
                        color: window.colorText
                        font.pixelSize: 20
                        font.bold: true
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#333" }

                GridLayout {
                    id: playGrid
                    columns: 2
                    Layout.fillWidth: true
                    columnSpacing: 25
                    rowSpacing: 20

                    Label { text: "URL / 文件:"; color: window.colorSecondary; font.pixelSize: 14 }
                    RowLayout {
                        spacing: 10
                        TextField {
                            id: playUrl
                            Layout.fillWidth: true
                            Layout.preferredHeight: 45
                            placeholderText: "输入 RTSP/RTMP 地址或选择文件"
                            font.pixelSize: 14
                            verticalAlignment: TextInput.AlignVCenter
                            leftPadding: 15
                            color: window.colorText
                            background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: playUrl.activeFocus ? window.colorPrimary : "#333" }
                        }
                        Button {
                            text: "浏览"
                            Layout.preferredHeight: 45
                            Layout.preferredWidth: 80
                            onClicked: playFileDialog.open()
                            background: Rectangle { color: parent.pressed ? "#444" : "#3a3a3a"; radius: 8 }
                            contentItem: Text { text: parent.text; color: window.colorText; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        }
                    }

                    Label { text: "硬件加速:"; color: window.colorSecondary; font.pixelSize: 14 }
                    ComboBox {
                        id: playHw
                        model: bridge.hwTypes
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        font.pixelSize: 14
                        leftPadding: 15
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                        palette.text: window.colorText
                        palette.buttonText: window.colorText
                        palette.windowText: window.colorText
                    }

                    Label { text: "延迟等级:"; color: window.colorSecondary; font.pixelSize: 14 }
                    ComboBox {
                        id: playLatency
                        model: ["极低 (Ultra-Low)", "平衡 (Low)", "稳定 (Standard)"]
                        currentIndex: 1
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        font.pixelSize: 14
                        leftPadding: 15
                        palette.text: window.colorText
                        palette.buttonText: window.colorText
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                    }
                }

                Button {
                    text: "启动播放"
                    Layout.alignment: Qt.AlignRight
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 48
                    onClicked: {
                        let hw = playHw.currentText === "None" ? "" : playHw.currentText
                        bridge.startPlay(playUrl.text, hw, playLatency.currentIndex)
                        window.switchToDisplay()
                    }
                    background: Rectangle { color: parent.pressed ? Qt.darker(window.colorPrimary, 1.1) : window.colorPrimary; radius: 8 }
                    contentItem: Text { text: parent.text; color: "white"; font.bold: true; font.pixelSize: 15; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }
        }

        // SECTION: Serve / Stream Settings
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: serveColumn.implicitHeight + 60
            color: window.colorSurface
            radius: 12
            border.color: "#333"

            ColumnLayout {
                id: serveColumn
                anchors.fill: parent
                anchors.margins: 30
                spacing: 25

                RowLayout {
                    spacing: 12
                    Text { text: "📡"; font.pixelSize: 22 }
                    Text { 
                        text: "推流与服务 (Serve / Push)"
                        color: window.colorText
                        font.pixelSize: 20
                        font.bold: true
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#333" }

                GridLayout {
                    id: serveGrid
                    columns: 4
                    Layout.fillWidth: true
                    columnSpacing: 25
                    rowSpacing: 20

                    Label { text: "输入源:"; color: window.colorSecondary; font.pixelSize: 14; Layout.alignment: Qt.AlignVCenter }
                    RowLayout {
                        Layout.columnSpan: 3
                        Layout.fillWidth: true
                        spacing: 10
                        TextField {
                            id: serveSource
                            text: "screen"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 45
                            placeholderText: "screen, video.mp4, 或 rtsp://..."
                            font.pixelSize: 14
                            verticalAlignment: TextInput.AlignVCenter
                            leftPadding: 15
                            color: window.colorText
                            background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: serveSource.activeFocus ? window.colorPrimary : "#333" }
                        }
                        Button {
                            text: "浏览"
                            Layout.preferredHeight: 45
                            Layout.preferredWidth: 80
                            onClicked: serveFileDialog.open()
                            background: Rectangle { color: parent.pressed ? "#444" : "#3a3a3a"; radius: 8 }
                            contentItem: Text { text: parent.text; color: window.colorText; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        }
                    }

                    Label { text: "传输协议:"; color: window.colorSecondary; font.pixelSize: 14 }
                    ComboBox {
                        id: protocolType
                        model: ["RTSP Server", "UDP Push", "RTP Push", "RTSP Push"]
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        font.pixelSize: 14
                        leftPadding: 15
                        palette.text: window.colorText
                        palette.buttonText: window.colorText
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                    }

                    Label { text: "硬件框架:"; color: window.colorSecondary; font.pixelSize: 14 }
                    ComboBox {
                        id: serveHw
                        model: bridge.hwTypes
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        font.pixelSize: 14
                        leftPadding: 15
                        palette.text: window.colorText
                        palette.buttonText: window.colorText
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                    }

                    Label { text: "视频编码:"; color: window.colorSecondary; font.pixelSize: 14 }
                    ComboBox {
                        id: codecSelect
                        model: ["H.264", "H.265", "MJPEG", "AV1"]
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        font.pixelSize: 14
                        leftPadding: 15
                        palette.text: window.colorText
                        palette.buttonText: window.colorText
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                    }

                    Label { text: "具体编码器:"; color: window.colorSecondary; font.pixelSize: 14 }
                    ComboBox {
                        id: serveEnc
                        model: bridge.getEncoders(codecSelect.currentText, serveHw.currentText)
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        font.pixelSize: 14
                        leftPadding: 15
                        palette.text: window.colorText
                        palette.buttonText: window.colorText
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                    }

                    Label { text: "帧率 (FPS):"; color: window.colorSecondary; font.pixelSize: 14 }
                    SpinBox {
                        id: serveFps; from: 1; to: 165; value: 30; editable: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        font.pixelSize: 14
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                        contentItem: TextInput {
                            text: serveFps.textFromValue(serveFps.value, serveFps.locale)
                            color: window.colorText
                            font.pixelSize: 14
                            horizontalAlignment: Qt.AlignHCenter
                            verticalAlignment: Qt.AlignVCenter
                        }
                    }

                    Label { text: "延迟等级:"; color: window.colorSecondary; font.pixelSize: 14 }
                    ComboBox {
                        id: serveLatency
                        model: ["极低 (Ultra-Low)", "平衡 (Low)", "稳定 (Standard)"]
                        currentIndex: 1
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        font.pixelSize: 14
                        leftPadding: 15
                        palette.text: window.colorText
                        palette.buttonText: window.colorText
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                    }

                    CheckBox {
                        id: echoEnable
                        text: "本地预览 (Echo)"
                        checked: true
                        Layout.columnSpan: 2
                        contentItem: Text { text: parent.text; color: window.colorText; font.pixelSize: 14; leftPadding: 35; verticalAlignment: Text.AlignVCenter }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#333" }

                // Protocol Specific Details
                GridLayout {
                    columns: 4
                    Layout.fillWidth: true
                    columnSpacing: 25
                    rowSpacing: 20
                    visible: protocolType.currentIndex !== -1

                    // RTSP Server
                    Label { 
                        text: "服务端口:"; color: window.colorSecondary; font.pixelSize: 14 
                        visible: protocolType.currentText === "RTSP Server"
                    }
                    SpinBox {
                        id: serverPort
                        from: 1024; to: 65535; value: 8554; editable: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        visible: protocolType.currentText === "RTSP Server"
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                        contentItem: TextInput {
                            text: serverPort.textFromValue(serverPort.value, serverPort.locale)
                            color: window.colorText
                            font.pixelSize: 14
                            horizontalAlignment: Qt.AlignHCenter
                            verticalAlignment: Qt.AlignVCenter
                        }
                    }
                    Label { 
                        text: "流名称:"; color: window.colorSecondary; font.pixelSize: 14 
                        visible: protocolType.currentText === "RTSP Server"
                    }
                    TextField {
                        id: serverStreamName
                        text: "live"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        visible: protocolType.currentText === "RTSP Server"
                        color: window.colorText
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                    }
                    Label { 
                        text: "绑定地址:"; color: window.colorSecondary; font.pixelSize: 14 
                        visible: protocolType.currentText === "RTSP Server"
                    }
                    TextField {
                        id: serverAddress
                        placeholderText: "0.0.0.0 (默认)"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        visible: protocolType.currentText === "RTSP Server"
                        color: window.colorText
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                    }

                    // Push Settings (UDP, RTP, RTSP Push)
                    Label { 
                        text: protocolType.currentText === "RTSP Push" ? "目标 URL:" : "目标地址:"
                        color: window.colorSecondary; font.pixelSize: 14
                        visible: protocolType.currentText !== "RTSP Server"
                    }
                    TextField {
                        id: targetAddress
                        text: protocolType.currentText === "RTSP Push" ? "rtsp://127.0.0.1:8554/live" : "127.0.0.1"
                        Layout.fillWidth: true
                        Layout.columnSpan: protocolType.currentText === "RTSP Push" ? 3 : 1
                        Layout.preferredHeight: 45
                        visible: protocolType.currentText !== "RTSP Server"
                        color: window.colorText
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                    }
                    Label { 
                        text: "目标端口:"; color: window.colorSecondary; font.pixelSize: 14
                        visible: protocolType.currentText === "UDP Push" || protocolType.currentText === "RTP Push"
                    }
                    SpinBox {
                        id: targetPort
                        from: 1; to: 65535; value: 1234; editable: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        visible: protocolType.currentText === "UDP Push" || protocolType.currentText === "RTP Push"
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                        contentItem: TextInput {
                            text: targetPort.textFromValue(targetPort.value, targetPort.locale)
                            color: window.colorText
                            font.pixelSize: 14
                            horizontalAlignment: Qt.AlignHCenter
                            verticalAlignment: Qt.AlignVCenter
                        }
                    }

                    // Local Bind (UDP/RTP Only)
                    Label { 
                        text: "本地地址:"; color: window.colorSecondary; font.pixelSize: 14
                        visible: protocolType.currentText === "UDP Push" || protocolType.currentText === "RTP Push"
                    }
                    TextField {
                        id: localAddress
                        placeholderText: "默认"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        visible: protocolType.currentText === "UDP Push" || protocolType.currentText === "RTP Push"
                        color: window.colorText
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                    }
                    Label { 
                        text: "本地端口:"; color: window.colorSecondary; font.pixelSize: 14
                        visible: protocolType.currentText === "UDP Push" || protocolType.currentText === "RTP Push"
                    }
                    SpinBox {
                        id: localPort
                        from: 0; to: 65535; value: 0; editable: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        visible: protocolType.currentText === "UDP Push" || protocolType.currentText === "RTP Push"
                        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#333" }
                        contentItem: TextInput {
                            text: localPort.textFromValue(localPort.value, localPort.locale)
                            color: window.colorText
                            font.pixelSize: 14
                            horizontalAlignment: Qt.AlignHCenter
                            verticalAlignment: Qt.AlignVCenter
                        }
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 15
                    Button {
                        text: "停止所有"
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 48
                        onClicked: bridge.stopAll()
                        background: Rectangle { color: parent.pressed ? "#b71c1c" : "#d32f2f"; radius: 8 }
                        contentItem: Text { text: parent.text; color: "white"; font.bold: true; font.pixelSize: 15; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                    Button {
                        text: "启动服务"
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 48
                        onClicked: {
                            let hw = serveHw.currentText === "None" ? "" : serveHw.currentText
                            let fps = serveFps.value
                            let lat = serveLatency.currentIndex
                            let echo = echoEnable.checked
                            if (protocolType.currentText === "RTSP Server") {
                                bridge.startServe(serveSource.text, serverPort.value, serverStreamName.text, serveEnc.currentText, hw, fps, lat, echo, serverAddress.text)
                            } else if (protocolType.currentText === "UDP Push") {
                                let url = "udp://" + targetAddress.text + ":" + targetPort.value
                                let opts = []
                                if (localAddress.text !== "") opts.push("localaddr=" + localAddress.text)
                                if (localPort.value !== 0) opts.push("localport=" + localPort.value)
                                if (opts.length > 0) url += "?" + opts.join("&")
                                bridge.startPush(serveSource.text, url, serveEnc.currentText, hw, fps, lat, echo)
                            } else if (protocolType.currentText === "RTP Push") {
                                let url = "rtp://" + targetAddress.text + ":" + targetPort.value
                                let opts = []
                                if (localAddress.text !== "") opts.push("localaddr=" + localAddress.text)
                                if (localPort.value !== 0) opts.push("localport=" + localPort.value)
                                if (opts.length > 0) url += "?" + opts.join("&")
                                bridge.startPush(serveSource.text, url, serveEnc.currentText, hw, fps, lat, echo)
                            } else if (protocolType.currentText === "RTSP Push") {
                                bridge.startPush(serveSource.text, targetAddress.text, serveEnc.currentText, hw, fps, lat, echo)
                            }
                            if (echo) window.switchToDisplay()
                        }
                        background: Rectangle { color: parent.pressed ? "#2E7D32" : "#388E3C"; radius: 8 }
                        contentItem: Text { text: parent.text; color: "white"; font.bold: true; font.pixelSize: 15; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }
        }
    }

    FileDialog {
        id: playFileDialog; title: "选择视频文件"
        onAccepted: playUrl.text = bridge.urlToPath(selectedFile)
    }
    FileDialog {
        id: serveFileDialog; title: "选择媒体文件"
        onAccepted: serveSource.text = bridge.urlToPath(selectedFile)
    }
}
