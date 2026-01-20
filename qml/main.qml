import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    id: window
    visible: true
    width: 1100
    height: 750
    title: qsTr("PixelBridge")

    property bool isSidebarVisible: true

    readonly property color colorPrimary: "#0078D4"
    readonly property color colorBg: "#0B0B0B"
    readonly property color colorSurface: "#161616"
    readonly property color colorText: "#E0E0E0"
    readonly property color colorSecondary: "#888888"

    palette.window: colorBg
    palette.windowText: colorText
    palette.base: colorSurface
    palette.text: colorText
    palette.button: colorSurface
    palette.buttonText: colorText
    palette.placeholderText: colorSecondary

    function switchToDisplay() { sideNav.currentIndex = 0 }
    function switchToSettings() { sideNav.currentIndex = 1 }

    background: Rectangle {
        color: window.colorBg
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: isSidebarVisible ? 220 : 0
            visible: isSidebarVisible || Layout.preferredWidth > 0
            color: window.colorSurface
            clip: true

            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: 250; easing.type: Easing.InOutQuad }
            }

            ColumnLayout {
                width: 220
                height: parent.height
                spacing: 0

                Rectangle {
                    Layout.preferredHeight: 100
                    Layout.fillWidth: true
                    color: "transparent"
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "PIXEL"
                            color: window.colorPrimary
                            font.pixelSize: 26
                            font.bold: true
                            font.letterSpacing: 2
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "BRIDGE"
                            color: window.colorText
                            font.pixelSize: 14
                            font.weight: Font.Light
                            font.letterSpacing: 4
                        }
                    }
                }

                ListView {
                    id: sideNav
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    model: ListModel {
                        ListElement { name: qsTr("监控画面"); icon: "🖥️" }
                        ListElement { name: qsTr("配置中心"); icon: "⚙️" }
                    }
                    delegate: ItemDelegate {
                        width: parent.width
                        height: 60
                        padding: 20
                        highlighted: sideNav.currentIndex === index
                        
                        background: Rectangle {
                            color: highlighted ? Qt.rgba(0, 120/255, 212/255, 0.15) : "transparent"
                            Rectangle {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                height: parent.height * 0.5
                                width: 3
                                color: window.colorPrimary
                                visible: highlighted
                            }
                        }

                        contentItem: RowLayout {
                            spacing: 15
                            Text {
                                text: model.icon
                                font.pixelSize: 20
                                color: highlighted ? window.colorText : window.colorSecondary
                            }
                            Text {
                                text: model.name
                                color: highlighted ? window.colorText : window.colorSecondary
                                font.pixelSize: 15
                                font.bold: highlighted
                            }
                        }
                        onClicked: sideNav.currentIndex = index
                    }
                }

                Text {
                    Layout.margins: 20
                    Layout.alignment: Qt.AlignHCenter
                    text: "© 2026 PixelBridge"
                    color: "#333"
                    font.pixelSize: 10
                }
            }
        }

        // Main Content
        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 0

            // Top Bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: window.colorBg

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    spacing: 15

                    Button {
                        id: toggleBtn
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        flat: true
                        text: isSidebarVisible ? "" : "" // Using Fluent icons if available, or just symbols
                        font.pixelSize: 20
                        
                        contentItem: Text {
                            text: isSidebarVisible ? "◀" : "▶"
                            font.pixelSize: 16
                            color: window.colorText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: toggleBtn.down ? "#333" : (toggleBtn.hovered ? "#222" : "transparent")
                            radius: 4
                        }

                        onClicked: isSidebarVisible = !isSidebarVisible
                    }

                    Text {
                        text: sideNav.currentIndex === 0 ? qsTr("监控画面") : qsTr("配置中心")
                        color: window.colorText
                        font.pixelSize: 18
                        font.bold: true
                    }
                }
            }

            StackLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                currentIndex: sideNav.currentIndex

                DisplayPage {
                    id: displayPage
                    Layout.margins: 20
                }

                SettingsPage {
                    id: settingsPage
                }
            }
        }
    }
}
