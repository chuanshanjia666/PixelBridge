import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    id: window
    visible: true
    width: 800
    height: 600
    title: qsTr("PixelBridge Multi-Tool")

    header: TabBar {
        id: bar
        width: parent.width
        TabButton { text: qsTr("播放 (Receive)") }
        TabButton { text: qsTr("服务 (Serve/Push)") }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: bar.currentIndex

        PlaybackPage {
            id: playbackPage
        }

        ServePage {
            id: servePage
        }
    }
}
