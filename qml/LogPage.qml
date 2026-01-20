import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("系统日志")
                color: window.colorText
                font.pixelSize: 20
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("清除日志")
                onClicked: logModel.clear()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1a1a1a"
            border.color: "#333"
            radius: 4

            ListView {
                id: logListView
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                model: logModel
                delegate: RowLayout {
                    width: logListView.width
                    spacing: 10
                    Text {
                        text: model.timestamp
                        color: "#888"
                        font.family: "Consolas"
                    }
                    Text {
                        text: "[" + model.level + "]"
                        color: {
                            if (model.level === "ERROR") return "#ff4444"
                            if (model.level === "WARN") return "#ffbb33"
                            return "#00C851"
                        }
                        font.bold: true
                        font.family: "Consolas"
                    }
                    Text {
                        text: model.message
                        color: window.colorText
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        font.family: "Consolas"
                    }
                }
                ScrollBar.vertical: ScrollBar { }
                
                onCountChanged: {
                    if (count > 0) logListView.positionViewAtEnd()
                }
            }
        }
    }
}
