import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    title: i18n("PlasmaHabit")
    width: 900
    height: 700

    pageStack.initialPage: Kirigami.ScrollablePage {
        title: i18n("Habit Tracker")

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            icon.name: "task-accepted"
            text: i18n("Welcome to PlasmaHabit")
            explanation: i18n("Track your daily and weekly habits with honest streaks.")
        }
    }
}
