import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    title: i18n("PlasmaHabit")
    width: 1040
    height: 780
    minimumWidth: 800
    minimumHeight: 600

    pageStack.initialPage: Kirigami.ScrollablePage {
        id: mainPage
        title: habitModel.showArchived ? i18n("Archived Habits") : i18n("Habit Tracker")

        actions: [
            Kirigami.Action {
                text: i18n("New Habit")
                icon.name: "list-add"
                visible: !habitModel.showArchived
                onTriggered: createHabitDialog.open()
            },
            Kirigami.Action {
                text: habitModel.showArchived ? i18n("Active Habits") : i18n("Show Archived")
                icon.name: habitModel.showArchived ? "view-list-details" : "archive-insert"
                checkable: true
                checked: habitModel.showArchived
                onTriggered: {
                    habitModel.showArchived = checked
                }
            }
        ]

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            width: parent.width

            // Error banner if any
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: habitModel.lastError.length > 0
                type: Kirigami.MessageType.Error
                text: habitModel.lastError
                showCloseButton: true
            }

            // Global Combined Heatmap Card
            Kirigami.Card {
                Layout.fillWidth: true
                visible: !habitModel.showArchived && habitModel.totalActiveCount > 0

                header: RowLayout {
                    Kirigami.Heading {
                        level: 3
                        text: i18n("Overall Activity Heatmap")
                    }
                    Item { Layout.fillWidth: true }
                    QQC2.Label {
                        text: i18n("%1 active habits", habitModel.totalActiveCount)
                        opacity: 0.7
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                    }
                }

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    // Heatmap grid (52 weeks x 7 days)
                    QQC2.ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 110
                        clip: true
                        QQC2.ScrollBar.vertical.policy: QQC2.ScrollBar.AlwaysOff

                        Item {
                            width: 52 * 14 + 40
                            height: 7 * 14 + 10

                            Row {
                                spacing: 3
                                Repeater {
                                    model: 52
                                    delegate: Column {
                                        required property int index
                                        readonly property int weekIdx: index
                                        spacing: 3

                                        Repeater {
                                            model: 7
                                            delegate: Rectangle {
                                                required property int index
                                                readonly property int dayIdx: index
                                                readonly property int cellIndex: weekIdx * 7 + dayIdx
                                                readonly property var cellData: (habitModel.globalHeatmap && cellIndex < habitModel.globalHeatmap.length) ? habitModel.globalHeatmap[cellIndex] : null

                                                width: 11
                                                height: 11
                                                radius: 2

                                                color: {
                                                    if (!cellData || cellData.isFuture) return Kirigami.Theme.disabledTextColor
                                                    var intensity = cellData.intensity || 0
                                                    if (intensity === 0) return Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
                                                    if (intensity === 1) return "#27ae60"
                                                    if (intensity === 2) return "#2ecc71"
                                                    if (intensity === 3) return "#1abc9c"
                                                    return "#16a085"
                                                }

                                                opacity: (cellData && cellData.isFuture) ? 0.2 : 1.0

                                                border.width: (cellData && cellData.isToday) ? 1 : 0
                                                border.color: Kirigami.Theme.highlightColor

                                                QQC2.ToolTip.visible: globalCellHover.hovered && cellData !== null
                                                QQC2.ToolTip.text: cellData ? (cellData.date + (cellData.isFuture ? " (" + i18n("Future") + ")" : ": " + cellData.completedCount + " " + i18n("completed"))) : ""

                                                HoverHandler {
                                                    id: globalCellHover
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Legend
                    RowLayout {
                        Layout.alignment: Qt.AlignRight
                        spacing: 4
                        QQC2.Label {
                            text: i18n("Less")
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.7
                        }
                        Rectangle { width: 10; height: 10; radius: 2; color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08) }
                        Rectangle { width: 10; height: 10; radius: 2; color: "#27ae60" }
                        Rectangle { width: 10; height: 10; radius: 2; color: "#2ecc71" }
                        Rectangle { width: 10; height: 10; radius: 2; color: "#1abc9c" }
                        Rectangle { width: 10; height: 10; radius: 2; color: "#16a085" }
                        QQC2.Label {
                            text: i18n("More")
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.7
                        }
                    }
                }
            }

            // Empty state placeholder
            Kirigami.PlaceholderMessage {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.gridUnit * 2
                visible: habitModel.count === 0
                icon.name: habitModel.showArchived ? "archive-insert" : "task-accepted"
                text: habitModel.showArchived ? i18n("No archived habits") : i18n("No habits created yet")
                explanation: habitModel.showArchived ? i18n("Archived habits will be displayed here.") : i18n("Start building consistent routines by creating your first habit.")

                helpfulAction: Kirigami.Action {
                    text: i18n("Create First Habit")
                    icon.name: "list-add"
                    visible: !habitModel.showArchived
                    onTriggered: createHabitDialog.open()
                }
            }

            // Habits Cards List
            Repeater {
                model: habitModel

                delegate: Kirigami.Card {
                    id: habitCard
                    Layout.fillWidth: true
                    Layout.bottomMargin: Kirigami.Units.smallSpacing

                    required property int index
                    required property int id
                    required property string name
                    required property string frequencyType
                    required property int targetCount
                    required property string colorHex
                    required property bool isArchived
                    required property bool completedToday
                    required property int currentStreak
                    required property int longestStreak
                    required property string weeklyProgress
                    required property var heatmapCells

                    header: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            width: 14
                            height: 14
                            radius: 7
                            color: habitCard.colorHex
                        }

                        Kirigami.Heading {
                            level: 3
                            text: habitCard.name
                        }

                        // Frequency badge
                        Rectangle {
                            radius: 4
                            color: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.15)
                            implicitWidth: freqLabel.implicitWidth + 12
                            implicitHeight: freqLabel.implicitHeight + 4

                            QQC2.Label {
                                id: freqLabel
                                anchors.centerIn: parent
                                text: habitCard.frequencyType === "daily" ? i18n("Daily") : i18n("%1× / week (%2)", habitCard.targetCount, habitCard.weeklyProgress)
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                font.bold: true
                                color: Kirigami.Theme.highlightColor
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Streak Badges
                        QQC2.Label {
                            text: habitCard.frequencyType === "daily"
                                ? i18n("🔥 %1d streak", habitCard.currentStreak)
                                : i18n("🔥 %1w streak", habitCard.currentStreak)
                            font.bold: true
                            color: habitCard.currentStreak > 0 ? "#e67e22" : Kirigami.Theme.disabledTextColor
                        }

                        QQC2.Label {
                            text: habitCard.frequencyType === "daily"
                                ? i18n("(Best: %1d)", habitCard.longestStreak)
                                : i18n("(Best: %1w)", habitCard.longestStreak)
                            opacity: 0.7
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                        }

                        // Quick Today toggle
                        QQC2.Button {
                            visible: !habitCard.isArchived
                            text: habitCard.completedToday ? i18n("Done Today ✓") : i18n("Mark Today")
                            icon.name: habitCard.completedToday ? "dialog-ok" : "checkbox"
                            highlighted: habitCard.completedToday
                            onClicked: {
                                var todayIso = new Date().toISOString().substring(0, 10)
                                habitModel.toggleCompletion(habitCard.id, todayIso)
                            }
                        }

                        // Archive/Unarchive button
                        QQC2.ToolButton {
                            icon.name: habitCard.isArchived ? "archive-extract" : "archive-insert"
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: habitCard.isArchived ? i18n("Restore Habit") : i18n("Archive Habit")
                            onClicked: {
                                habitModel.setHabitArchived(habitCard.id, !habitCard.isArchived)
                            }
                        }

                        // Delete button
                        QQC2.ToolButton {
                            visible: habitCard.isArchived
                            icon.name: "edit-delete"
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Delete Permanently")
                            onClicked: {
                                habitModel.removeHabit(habitCard.id)
                            }
                        }
                    }

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: i18n("Contribution Grid (Mon–Sun, click any past or current cell to toggle)")
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.7
                        }

                        // 52-week per-habit binary heatmap grid
                        QQC2.ScrollView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 110
                            clip: true
                            QQC2.ScrollBar.vertical.policy: QQC2.ScrollBar.AlwaysOff

                            Item {
                                width: 52 * 14 + 40
                                height: 7 * 14 + 10

                                Row {
                                    spacing: 3
                                    Repeater {
                                        model: 52
                                        delegate: Column {
                                            required property int index
                                            readonly property int weekIdx: index
                                            spacing: 3

                                            Repeater {
                                                model: 7
                                                delegate: Rectangle {
                                                    required property int index
                                                    readonly property int dayIdx: index
                                                    readonly property int cellIndex: weekIdx * 7 + dayIdx
                                                    readonly property var cellData: (habitCard.heatmapCells && cellIndex < habitCard.heatmapCells.length) ? habitCard.heatmapCells[cellIndex] : null

                                                    width: 11
                                                    height: 11
                                                    radius: 2

                                                    color: {
                                                        if (!cellData || cellData.isFuture) return Kirigami.Theme.disabledTextColor
                                                        if (cellData.isCompleted) return habitCard.colorHex
                                                        return Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
                                                    }

                                                    opacity: (cellData && cellData.isFuture) ? 0.2 : 1.0

                                                    border.width: (cellData && cellData.isToday) ? 1 : 0
                                                    border.color: Kirigami.Theme.highlightColor

                                                    QQC2.ToolTip.visible: cellHover.hovered && cellData !== null
                                                    QQC2.ToolTip.text: cellData ? (cellData.date + (cellData.isFuture ? " (" + i18n("Future") + ")" : (cellData.isCompleted ? ": " + i18n("Completed") : ": " + i18n("Missed")))) : ""

                                                    HoverHandler {
                                                        id: cellHover
                                                    }

                                                    MouseArea {
                                                        anchors.fill: parent
                                                        enabled: cellData !== null && !cellData.isFuture && !habitCard.isArchived
                                                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                                        onClicked: {
                                                            if (cellData) {
                                                                habitModel.toggleCompletion(habitCard.id, cellData.date)
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Create Habit Dialog
    Kirigami.Dialog {
        id: createHabitDialog
        title: i18n("New Habit")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        preferredWidth: Kirigami.Units.gridUnit * 22

        property string selectedColor: "#2ecc71"

        onAccepted: {
            var freq = freqCombo.currentIndex === 0 ? "daily" : "weekly"
            var target = freq === "daily" ? 1 : targetSpin.value
            habitModel.addHabit(nameField.text, freq, target, selectedColor)
            nameField.text = ""
            freqCombo.currentIndex = 0
            targetSpin.value = 3
        }

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            Kirigami.FormLayout {
                QQC2.TextField {
                    id: nameField
                    Kirigami.FormData.label: i18n("Name:")
                    placeholderText: i18n("e.g. Morning Jog, Read 20 mins")
                    Layout.fillWidth: true
                }

                QQC2.ComboBox {
                    id: freqCombo
                    Kirigami.FormData.label: i18n("Frequency:")
                    model: [i18n("Daily (1x/day)"), i18n("Flexible Weekly (N/week)")]
                }

                QQC2.SpinBox {
                    id: targetSpin
                    Kirigami.FormData.label: i18n("Target / Week:")
                    from: 1
                    to: 7
                    value: 3
                    visible: freqCombo.currentIndex === 1
                }

                RowLayout {
                    Kirigami.FormData.label: i18n("Accent Color:")
                    spacing: 8

                    Repeater {
                        model: ["#2ecc71", "#3498db", "#9b59b6", "#e67e22", "#e74c3c", "#1abc9c"]
                        delegate: Rectangle {
                            required property string modelData
                            width: 24
                            height: 24
                            radius: 12
                            color: modelData
                            border.width: createHabitDialog.selectedColor === modelData ? 2 : 0
                            border.color: Kirigami.Theme.highlightColor

                            MouseArea {
                                anchors.fill: parent
                                onClicked: createHabitDialog.selectedColor = parent.modelData
                            }
                        }
                    }
                }
            }
        }
    }
}
