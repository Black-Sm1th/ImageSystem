import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.3
import "./components"

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0B1120"

    property var messageManager: null
    property bool showUploadDialog: false
    property int uploadTabIndex: 0
    property string currentStatusFilter: "全部"
    property var statusOptions: ["全部", "排队中", "分析中", "分析完成"]
    property var tableHeaders: ["序号", "姓名", "病历号", "年龄", "性别", "检查日期", "预测脑龄", "处理模式", "状态", "操作"]
    property var columnWidths: [0.07, 0.09, 0.11, 0.06, 0.06, 0.12, 0.09, 0.10, 0.09, 0.21]
    property bool allChecked: false
    property var allTasks: []
    property string scanSourcePath: ""
    property string uploadBidsPath: ""
    property string uploadOutputPath: ""
    property string uploadLicensePath: ""
    property int uploadMethodIndex: 1
    property int contentMaxWidth: 1760
    property int contentHorizontalMargin: 24
    property int uploadProcessingMode: 0  // 0: 仅脑龄预测 1: 仅预处理 2: 全流程
    property bool scanNoticePending: false
    readonly property bool uploadModeSwitchLocked: $MainViewController.isPreAnalysisRunning || $MainViewController.brainAgeProcessing

    signal viewAnalysisRequested(var caseInfo)

    readonly property bool scanAllChecked: $MriPairResultModel.resultCount > 0
                                         && $MriPairResultModel.checkedCount === $MriPairResultModel.resultCount

    QtObject {
        id: palette
        property color textWhite90: "#E6FFFFFF"
        property color textWhite70: "#B3FFFFFF"
        property color textWhite50: "#80FFFFFF"
        property color strokeBlack: "#000000"
        property color lineColor: "#2A3348"
        property color panelColor: "#121A29"
        property color panelBorder: "#283347"
        property color headerColor: "#242936"
        property color rowColor: "#171D2B"
        property color rowHoverColor: "#1D2536"
        property color rowCheckedColor: "#2A3B67"
        property color inputColor: "#202739"
        property color inputFocusColor: "#242D42"
        property color inputBorderColor: "#2A3348"
        property color inputFocusBorderColor: "#3C7EFF"
    }

    ListModel {
        id: taskModel
    }

    FileDialog {
        id: scanFolderDialog
        title: qsTr("选择要扫描的文件夹")
        selectFolder: true
        onAccepted: {
            if (fileUrls.length === 0)
                return

            var path = fileUrls[0].toString()
            if (path.startsWith("file:///"))
                path = path.substring("file:///".length)
            path = path.replace(/\\/g, "/")
            root.setScanSourcePath(path)
        }
    }

    function notifyWarning(text) {
        if (messageManager)
            messageManager.warning(text, 2000)
        else
            console.log(text)
    }

    function notifyInfo(text) {
        if (messageManager)
            messageManager.info(text, 2000)
        else
            console.log(text)
    }

    function notifySuccess(text) {
        if (messageManager)
            messageManager.success(text, 2200)
        else
            console.log(text)
    }

    function formatDateText(value) {
        if (!value)
            return ""

        var text = String(value)
        if (text.length === 8 && text.indexOf("-") === -1)
            return text.slice(0, 4) + "-" + text.slice(4, 6) + "-" + text.slice(6, 8)
        return text
    }

    function normalizeStatusText(statusValue) {
        if (statusValue === "queued")
            return "排队中"
        if (statusValue === "processing")
            return "分析中"
        return "分析完成"
    }

    function statusColor(statusText) {
        if (statusText === "排队中")
            return "#6EA8FF"
        if (statusText === "分析中")
            return "#F3B64E"
        return "#32D26B"
    }

    function formatSexText(value) {
        if (!value)
            return "-"

        var text = String(value).trim()
        var upper = text.toUpperCase()
        if (upper === "M" || upper === "MALE" || text === "男")
            return "男"
        if (upper === "F" || upper === "FEMALE" || text === "女")
            return "女"
        if (text === "男女")
            return "-"
        return text
    }

    function formatMethodText(preprocessMethod) {
        if (preprocessMethod === "DeepPrep")
            return "深度学习"
        if (preprocessMethod === "fMRIPrep")
            return "传统算法"
        return ""
    }

    function formatCheckTypeText(checkType, preprocessMethod) {
        var methodText = formatMethodText(preprocessMethod)

        if (checkType === "BrainAgeOnly")
            return "仅脑龄预测"
        if (checkType === "PrepOnly")
            return methodText ? ("仅预处理/" + methodText) : "仅预处理"
        if (checkType === "FullPipeline")
            return methodText ? ("全流程/" + methodText) : "全流程"
        if (checkType === "fMRIPrep")
            return "仅预处理/传统算法"
        if (checkType === "DeepPrep")
            return "仅预处理/深度学习"
        return "-"
    }

    function setScanSourcePath(path) {
        scanSourcePath = path
        if (scanPathField)
            scanPathField.text = path

        refreshUploadPaths()
        uploadLicensePath = $MainViewController.defaultLicenseFilePath()
    }

    function refreshUploadPaths() {
        var defaultPaths = $MainViewController.defaultProcessingPaths()
        uploadBidsPath = defaultPaths.bidsPath || ""
        uploadOutputPath = defaultPaths.outputPath || ""
    }

    function resetScanResultsBeforeModeSwitch() {
        $MainViewController.resetScanState()
        scanNoticePending = false
    }

    function openUploadDialog() {
        showUploadDialog = true
        uploadTabIndex = 0
        uploadMethodIndex = 1
        uploadProcessingMode = 0  // 默认仅脑龄预测
        refreshUploadPaths()
        if (uploadLicensePath === "")
            uploadLicensePath = $MainViewController.defaultLicenseFilePath()
    }

    function closeUploadDialog() {
        showUploadDialog = false
    }

    function clearPanelInputs() {
        scanSourcePath = ""
        uploadBidsPath = ""
        uploadOutputPath = ""
        uploadLicensePath = ""

        if (scanPathField)
            scanPathField.text = ""
        if (searchField)
            searchField.text = ""

        currentStatusFilter = "全部"
        if (statusComboBox) {
            statusComboBox.selectedIndices = [0]
            statusComboBox.selectedText = root.statusOptions[0]
        }
    }

    function loadTaskList() {
        var rows = $AppDataModel.records()
        var tasks = []

        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i]
            var statusText = normalizeStatusText(row.status)
            var numericAge = Number(row.age)
            var brainAge = Number(row.predictedBrainAge)
            tasks.push({
                checked: false,
                serialNumber: (i + 1 < 10 ? "0" : "") + (i + 1),
                patientName: row.name || "",
                recordNumber: row.patientId || "",
                ageText: !isNaN(numericAge) && numericAge > 0 ? String(Math.round(numericAge)) : "-",
                genderText: formatSexText(row.sex),
                inspectTime: formatDateText(row.examDate),
                predictedBrainAgeText: !isNaN(brainAge) && brainAge >= 0 ? String(Math.round(brainAge)) : "-",
                checkTypeText: formatCheckTypeText(row.checkType || "", row.preprocessMethod || ""),
                statusText: statusText,
                statusColorValue: statusColor(statusText),
                detailEnabled: statusText === "分析完成" && hasPreprocessingCapability(row.checkType || ""),
                dbId: row.dbId !== undefined ? row.dbId : -1,
                source: row.source,
                outputPath: row.outputPath || "",
                bidsPath: row.bidsPath || "",
                checkType: row.checkType || "",
                seriesUid: row.seriesUid || ""
            })
        }

        allTasks = tasks
        applyFilters()
    }

    function canDeleteTask(task) {
        if (task.statusText === "分析完成" && task.dbId !== undefined && task.dbId >= 0)
            return true
        if (task.statusText === "排队中")
            return true
        return false
    }

    function hasPreprocessingCapability(checkType) {
        return checkType === "PrepOnly"
            || checkType === "FullPipeline"
            || checkType === "fMRIPrep"
            || checkType === "DeepPrep"
    }

    function detailDisabledReason(task) {
        if (task.statusText !== "分析完成")
            return "未分析完成，无法查看"
        if (!hasPreprocessingCapability(task.checkType))
            return "仅进行了脑龄预测，暂无可查看的分析详情"
        return "当前任务暂无可查看的分析详情"
    }

    function canViewTaskDetail(task) {
        return task.statusText === "分析完成" && hasPreprocessingCapability(task.checkType)
    }

    function appendTask(task) {
        taskModel.append({
            checked: task.checked,
            serialNumber: task.serialNumber,
            patientName: task.patientName,
            recordNumber: task.recordNumber,
            ageText: task.ageText,
            genderText: task.genderText,
            inspectTime: task.inspectTime,
            predictedBrainAgeText: task.predictedBrainAgeText,
            checkTypeText: task.checkTypeText,
            statusText: task.statusText,
            statusColorValue: task.statusColorValue,
            detailEnabled: task.detailEnabled,
            detailTooltip: detailDisabledReason(task),
            dbId: task.dbId,
            source: task.source,
            outputPath: task.outputPath,
            bidsPath: task.bidsPath,
            checkType: task.checkType,
            seriesUid: task.seriesUid
        })
    }

    function syncTaskProperty(serialNumber, propertyName, value) {
        for (var i = 0; i < allTasks.length; ++i) {
            if (allTasks[i].serialNumber === serialNumber) {
                allTasks[i][propertyName] = value
                return
            }
        }
    }

    function refreshAllChecked() {
        if (taskModel.count === 0) {
            allChecked = false
            return
        }

        for (var i = 0; i < taskModel.count; ++i) {
            if (!taskModel.get(i).checked) {
                allChecked = false
                return
            }
        }
        allChecked = true
    }

    function applyFilters() {
        var keyword = searchField ? searchField.text.trim().toLowerCase() : ""
        taskModel.clear()

        for (var i = 0; i < allTasks.length; ++i) {
            var task = allTasks[i]
            var matchesKeyword = keyword === ""
                || task.serialNumber.toLowerCase().indexOf(keyword) !== -1
                || task.patientName.toLowerCase().indexOf(keyword) !== -1
                || task.recordNumber.toLowerCase().indexOf(keyword) !== -1
            var matchesStatus = currentStatusFilter === "全部" || task.statusText === currentStatusFilter

            if (matchesKeyword && matchesStatus)
                appendTask(task)
        }

        refreshAllChecked()
    }

    function toggleAllRows() {
        var nextState = !allChecked
        for (var i = 0; i < taskModel.count; ++i) {
            taskModel.setProperty(i, "checked", nextState)
            syncTaskProperty(taskModel.get(i).serialNumber, "checked", nextState)
        }
        allChecked = nextState
    }

    function removeTask(task) {
        if (!canDeleteTask(task))
            return false

        if (task.statusText === "排队中") {
            $MainViewController.removeQueuedTask(
                task.patientName || "",
                task.inspectTime || "",
                task.seriesUid || ""
            )
            loadTaskList()
            return true
        }

        if (!$MainViewController.removeCompletedCase(task.dbId))
            return false
        loadTaskList()
        return true
    }

    function removeCheckedRows() {
        var removedAny = false
        var completedIds = []
        var queuedTasks = []

        for (var i = taskModel.count - 1; i >= 0; --i) {
            var task = taskModel.get(i)
            if (!task.checked || !canDeleteTask(task))
                continue

            if (task.statusText === "排队中") {
                queuedTasks.push({
                    name: task.patientName || "",
                    examDate: task.inspectTime || "",
                    seriesUid: task.seriesUid || ""
                })
            } else if (task.dbId !== undefined && task.dbId >= 0) {
                completedIds.push(task.dbId)
            }
        }

        for (var j = 0; j < completedIds.length; ++j) {
            if ($MainViewController.removeCompletedCase(completedIds[j]))
                removedAny = true
        }

        for (var k = 0; k < queuedTasks.length; ++k) {
            $MainViewController.removeQueuedTask(
                queuedTasks[k].name,
                queuedTasks[k].examDate,
                queuedTasks[k].seriesUid
            )
            removedAny = true
        }

        if (removedAny)
            loadTaskList()
    }

    function startHardwareScan() {
        var path = scanPathField.text.trim()
        if (path === "") {
            notifyWarning(qsTr("请选择要扫描的文件夹"))
            return
        }

        setScanSourcePath(path.replace(/\\/g, "/"))
        // 根据处理模式调用扫描：0 = 仅脑龄预测(mode=1), 1/2 = 含预处理(mode=0)
        var scanMode = (uploadProcessingMode === 0) ? 1 : 0
        scanNoticePending = true
        notifyInfo(qsTr("已开始扫描，请稍候…"))
        $MainViewController.scanFolder(scanSourcePath, scanMode)
    }

    function rescanForCurrentMode() {
        if (!showUploadDialog || uploadTabIndex !== 0)
            return
        if ($MainViewController.isScanning)
            return
        if (!scanPathField || scanPathField.text.trim() === "")
            return

        notifyInfo(qsTr("处理模式已切换，正在按新模式重新扫描…"))
        startHardwareScan()
    }

    function confirmUploadTask() {
        if ($MriPairResultModel.checkedCount <= 0) {
            notifyWarning(qsTr("请先勾选需要上传的数据"))
            return
        }
        if (uploadBidsPath === "" || uploadOutputPath === "") {
            notifyWarning(qsTr("请先完成扫描，生成 Bids 和 Output 路径"))
            return
        }

        // 仅脑龄预测模式不需要 license 文件
        if (uploadProcessingMode !== 0 && uploadLicensePath === "") {
            notifyWarning(qsTr("未找到默认 license 文件路径"))
            return
        }

        if (uploadProcessingMode === 0) {
            $MainViewController.startBrainAgeOnly()
        } else if (uploadProcessingMode === 1) {
            $MainViewController.startPreprocessingOnly(
                uploadMethodIndex,
                uploadBidsPath,
                uploadOutputPath,
                uploadLicensePath
            )
        } else {
            $MainViewController.startPreAnalysis(
                uploadMethodIndex,
                uploadBidsPath,
                uploadOutputPath,
                uploadLicensePath
            )
        }

        $MriPairResultModel.deselectAll()
        refreshUploadPaths()
        closeUploadDialog()
        loadTaskList()
    }

    Component.onCompleted: {
        clearPanelInputs()
        loadTaskList()
    }

    Connections {
        target: $AppDataModel
        function onModelReset() {
            root.loadTaskList()
        }
    }

    Connections {
        target: $MainViewController
        function onIsScanningChanged() {
            if ($MainViewController.isScanning)
                return

            if (!root.scanNoticePending)
                return

            root.scanNoticePending = false
            root.notifySuccess(qsTr("扫描完成，共识别 %1 条结果").arg($MriPairResultModel.resultCount))
        }
    }

    onUploadProcessingModeChanged: rescanForCurrentMode()

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#09101D" }
            GradientStop { position: 0.58; color: "#0D1730" }
            GradientStop { position: 1.0; color: "#090E18" }
        }
    }

    Rectangle {
        width: 760
        height: 760
        x: 940
        y: -80
        radius: 380
        color: "#1E4FA8"
        opacity: 0.16
    }

    Rectangle {
        id: frameCard
        anchors.fill: parent
        anchors.margins: 4
        radius: 8
        color: "#0D1323"

        Column {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                id: topBar
                width: parent.width
                height: 44
                color: "#171D29"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "数据管理"
                    color: "#FFFFFF"
                    font.family: "Alibaba PuHuiTi 3.0"
                    font.pixelSize: 16
                }

                DataStoreButton {
                    width: 26
                    height: 26
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "×"
                    fontSize: 16
                    radiusSize: 4
                    normalColor: "transparent"
                    hoverColor: "#32384A"
                    pressedColor: "#252B3B"
                    onClicked: {
                        if (root.Window.window)
                            root.Window.window.close()
                    }
                }
            }

            Item {
                width: parent.width
                height: parent.height - topBar.height

                Item {
                    width: Math.min(parent.width - root.contentHorizontalMargin * 2, root.contentMaxWidth)
                    height: parent.height - 36
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 20

                    Item {
                        id: headerBlock
                        anchors.left: parent.left
                        anchors.right: uploadDataButton.left
                        anchors.rightMargin: 20
                        anchors.top: parent.top
                        height: 64

                        Text {
                            x: 0
                            y: 4
                            text: "数据管理中心"
                            color: "#40000000"
                            font.family: "Alibaba PuHuiTi 3.0"
                            font.pixelSize: 30
                            font.weight: Font.Bold
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            text: "数据管理中心"
                            color: palette.textWhite90
                            style: Text.Outline
                            styleColor: palette.strokeBlack
                            font.family: "Alibaba PuHuiTi 3.0"
                            font.pixelSize: 30
                            font.weight: Font.Bold
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.topMargin: 38
                            text: "管理扫描结果、筛选任务，并进入上传与分析流程"
                            color: palette.textWhite50
                            style: Text.Outline
                            styleColor: palette.strokeBlack
                            font.family: "Alibaba PuHuiTi 3.0"
                            font.pixelSize: 16
                        }
                    }

                    DataStoreButton {
                        id: uploadDataButton
                        width: 100
                        height: 40
                        anchors.top: parent.top
                        anchors.right: parent.right
                        text: "数据上传"
                        normalColor: "#3C7EFF"
                        hoverColor: "#5D91FF"
                        pressedColor: "#2A63DA"
                        textColor: "#E6FFFFFF"
                        fontSize: 16
                        onClicked: root.openUploadDialog()
                    }

                    Item {
                        id: filterRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: headerBlock.bottom
                        anchors.topMargin: 24
                        height: 42

                        Rectangle {
                            id: searchBox
                            anchors.left: parent.left
                            width: Math.min(520, parent.width * 0.34)
                            height: parent.height
                            radius: 6
                            color: searchField.activeFocus ? palette.inputFocusColor : palette.inputColor
                            border.color: searchField.activeFocus ? palette.inputFocusBorderColor : palette.inputBorderColor
                            border.width: 1

                            Behavior on color { ColorAnimation { duration: 120 } }
                            Behavior on border.color { ColorAnimation { duration: 120 } }

                            TextField {
                                id: searchField
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                color: "#FFFFFF"
                                font.family: "Alibaba PuHuiTi 3.0"
                                font.pixelSize: 16
                                placeholderText: "搜索姓名或病历号"
                                placeholderTextColor: "#80FFFFFF"
                                background: Item {}
                                selectByMouse: true
                                verticalAlignment: Text.AlignVCenter
                                onAccepted: root.applyFilters()
                                onTextChanged: root.applyFilters()
                            }

                        }

                        Text {
                            anchors.left: searchBox.right
                            anchors.leftMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                            text: "状态筛选："
                            color: palette.textWhite50
                            font.family: "Alibaba PuHuiTi 3.0"
                            font.pixelSize: 14
                        }

                        CustomComboBox {
                            id: statusComboBox
                            anchors.left: searchBox.right
                            anchors.leftMargin: 92
                            anchors.verticalCenter: parent.verticalCenter
                            comboWidth: 138
                            comboHeight: parent.height
                            model: root.statusOptions
                            selectedIndices: [0]
                            selectedText: root.statusOptions[0]
                            placeholderText: "请选择"
                            textColor: palette.textWhite90
                            backgroundColor: palette.inputColor
                            dropdownBackgroundColor: "#202739"
                            itemHoverColor: "#2A3142"
                            itemSelectedColor: "#3C7EFF"
                            borderColor: palette.inputBorderColor
                            borderWidth: 1
                            radius: 6
                            fontSize: 14

                            onSelectionChanged: {
                                if (selectedItems.length > 0) {
                                    root.currentStatusFilter = selectedItems[0]
                                    root.applyFilters()
                                }
                            }
                        }

                        DataStoreButton {
                            width: 92
                            height: parent.height
                            anchors.left: statusComboBox.right
                            anchors.leftMargin: 12
                            text: "重置筛选"
                            normalColor: "#2A3142"
                            hoverColor: "#39425A"
                            pressedColor: "#212838"
                            textColor: palette.textWhite70
                            fontSize: 14
                            onClicked: {
                                searchField.text = ""
                                root.currentStatusFilter = root.statusOptions[0]
                                statusComboBox.selectedIndices = [0]
                                statusComboBox.selectedText = root.statusOptions[0]
                                root.applyFilters()
                            }
                        }
                    }

                    Item {
                        id: listHeaderRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: filterRow.bottom
                        anchors.topMargin: 22
                        height: 32

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: "扫描结果列表"
                            color: palette.textWhite90
                            font.family: "Alibaba PuHuiTi 3.0"
                            font.pixelSize: 14
                        }

                        DataStoreButton {
                            width: 68
                            height: 30
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: "删除"
                            normalColor: "#F56C6C"
                            hoverColor: "#FF8181"
                            pressedColor: "#D25555"
                            textColor: palette.textWhite90
                            fontSize: 14
                            onClicked: root.removeCheckedRows()
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: listHeaderRow.bottom
                        anchors.topMargin: 12
                        anchors.bottom: parent.bottom
                        radius: 10
                        color: "#101827"
                        border.color: "#2B3950"
                        border.width: 1
                        clip: true

                        Column {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                width: parent.width
                                height: 46
                                color: "#222938"

                                Row {
                                    anchors.fill: parent

                                    Rectangle {
                                        width: 46
                                        height: parent.height
                                        color: "transparent"

                                        Rectangle {
                                            width: 18
                                            height: 18
                                            radius: 4
                                            anchors.centerIn: parent
                                            color: root.allChecked ? "#3C7EFF" : "#313848"
                                            border.color: root.allChecked ? "#3C7EFF" : "#4D566A"
                                            border.width: 1

                                            Text {
                                                anchors.centerIn: parent
                                                text: root.allChecked ? "✓" : ""
                                                color: "#FFFFFF"
                                                font.family: "Alibaba PuHuiTi 3.0"
                                                font.pixelSize: 11
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.toggleAllRows()
                                            }
                                        }
                                    }

                                    Repeater {
                                        model: root.tableHeaders.length

                                        Item {
                                            width: (parent.width - 46) * root.columnWidths[index]
                                            height: parent.height

                                            Text {
                                                anchors.centerIn: parent
                                                text: root.tableHeaders[index]
                                                color: palette.textWhite90
                                                font.family: "Alibaba PuHuiTi 3.0"
                                                font.pixelSize: 14
                                            }
                                        }
                                    }
                                }
                            }

                            ListView {
                                id: taskListView
                                width: parent.width
                                height: parent.height - 46
                                model: taskModel
                                clip: true

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                    background: Rectangle { color: "#131A28" }
                                    contentItem: Rectangle {
                                        implicitWidth: 8
                                        radius: 4
                                        color: "#4A556B"
                                    }
                                }

                                delegate: DataStoreTaskRow {
                                    width: taskListView.width
                                    rowIndex: index
                                    checked: model.checked
                                    serialNumber: model.serialNumber
                                    patientName: model.patientName
                                    recordNumber: model.recordNumber
                                    ageText: model.ageText
                                    genderText: model.genderText
                                    inspectTime: model.inspectTime
                                    predictedBrainAgeText: model.predictedBrainAgeText
                                    checkTypeText: model.checkTypeText || "-"
                                    statusText: model.statusText
                                    statusColor: model.statusColorValue
                                    detailEnabled: model.detailEnabled
                                    detailTooltip: model.detailTooltip || ""
                                    deleteEnabled: root.canDeleteTask(model)
                                    columnWidths: root.columnWidths

                                    onCheckClicked: {
                                        var nextChecked = !model.checked
                                        taskModel.setProperty(index, "checked", nextChecked)
                                        root.syncTaskProperty(model.serialNumber, "checked", nextChecked)
                                        root.refreshAllChecked()
                                    }
                                    onDetailClicked: {
                                        if (!root.canViewTaskDetail(model)) {
                                            root.notifyWarning(qsTr("当前任务暂无可查看的分析详情"))
                                            return
                                        }
                                        root.viewAnalysisRequested({
                                            outputPath: model.outputPath,
                                            patientId: model.recordNumber,
                                            patientName: model.patientName,
                                            examDate: model.inspectTime,
                                            seriesUid: model.seriesUid
                                        })
                                    }
                                    onDeleteClicked: root.removeTask(model)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#9A090B10"
        visible: showUploadDialog
        z: 2000

        MouseArea {
            anchors.fill: parent
            onClicked: {}
        }

        Rectangle {
            width: Math.min(parent.width - 20, 1200)
            height: Math.min(parent.height - 20, 840)
            anchors.centerIn: parent
            radius: 14
            color: "#232527"
            border.color: "#2E3440"
            border.width: 1

            Column {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    width: parent.width
                    height: 44
                    radius: 14
                    color: "#232527"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: "#3A3F46"
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "数据上传"
                        color: "#FFFFFF"
                        font.family: "Alibaba PuHuiTi 3.0"
                        font.pixelSize: 16
                        font.bold: true
                    }

                    DataStoreButton {
                        width: 28
                        height: 28
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: "×"
                        fontSize: 16
                        radiusSize: 6
                        normalColor: "transparent"
                        hoverColor: "#343942"
                        pressedColor: "#2A2E36"
                        onClicked: root.closeUploadDialog()
                    }
                }

                Item {
                    width: parent.width
                    height: parent.height - 44

                    Column {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 18

                        Rectangle {
                            width: parent.width
                            height: 36
                            radius: 4
                            color: "#2D2F31"

                            Row {
                                anchors.fill: parent
                                spacing: 0

                                Rectangle {
                                    width: parent.width / 2
                                    height: parent.height
                                    radius: 4
                                    color: uploadTabIndex === 0 ? "#3C68C6" : "transparent"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "硬件扫描"
                                        color: uploadTabIndex === 0 ? "#FFFFFF" : "#B9BDC6"
                                        font.pixelSize: 16
                                        font.family: "Alibaba PuHuiTi 3.0"
                                        font.bold: uploadTabIndex === 0
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: uploadTabIndex = 0
                                    }
                                }

                                Rectangle {
                                    width: parent.width / 2
                                    height: parent.height
                                    radius: 4
                                    color: uploadTabIndex === 1 ? "#3C68C6" : "transparent"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "网络获取"
                                        color: uploadTabIndex === 1 ? "#FFFFFF" : "#B9BDC6"
                                        font.pixelSize: 16
                                        font.family: "Alibaba PuHuiTi 3.0"
                                        font.bold: uploadTabIndex === 1
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: uploadTabIndex = 1
                                    }
                                }
                            }
                        }

                        // 处理模式选择
                        Row {
                            width: parent.width
                            height: 36
                            spacing: 16

                            Text {
                                text: "处理模式："
                                color: "#B9BDC6"
                                font.pixelSize: 14
                                font.family: "Alibaba PuHuiTi 3.0"
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Repeater {
                                model: [qsTr("仅脑龄预测"), qsTr("仅预处理"), qsTr("全流程")]
                                Row {
                                    spacing: 6
                                    anchors.verticalCenter: parent.verticalCenter
                                    Rectangle {
                                        width: 18
                                        height: 18
                                        radius: 9
                                        color: "transparent"
                                        border.color: root.uploadProcessingMode === index ? "#3C7EFF" : "#7E8796"
                                        border.width: 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        Rectangle {
                                            width: 10
                                            height: 10
                                            radius: 5
                                            color: "#3C7EFF"
                                            anchors.centerIn: parent
                                            visible: root.uploadProcessingMode === index
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: {
                                                if (root.uploadProcessingMode === index)
                                                    return
                                                if ($MainViewController.isScanning) {
                                                    root.notifyWarning(qsTr("扫描中，暂不支持切换处理模式"))
                                                    return
                                                }
                                                root.resetScanResultsBeforeModeSwitch()
                                                root.uploadProcessingMode = index
                                            }
                                        }
                                    }
                                    Text {
                                        text: modelData
                                        color: root.uploadProcessingMode === index ? "#FFFFFF" : "#B9BDC6"
                                        font.pixelSize: 14
                                        font.family: "Alibaba PuHuiTi 3.0"
                                        anchors.verticalCenter: parent.verticalCenter
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: {
                                                if (root.uploadProcessingMode === index)
                                                    return
                                                if (root.uploadModeSwitchLocked) {
                                                    root.notifyWarning(qsTr("处理中，暂不允许切换处理模式"))
                                                    return
                                                }
                                                if ($MainViewController.isScanning) {
                                                    root.notifyWarning(qsTr("扫描尚未完成，暂不允许切换处理模式"))
                                                    return
                                                }
                                                root.resetScanResultsBeforeModeSwitch()
                                                root.uploadProcessingMode = index
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            id: uploadContentArea
                            width: parent.width
                            height: parent.height - 36 - 36 - 60 - 18 * 2

                            Column {
                                anchors.fill: parent
                                spacing: 14
                                visible: uploadTabIndex === 0

                                Row {
                                    width: parent.width
                                    height: 36
                                    spacing: 12

                                    Rectangle {
                                        width: parent.width - 120
                                        height: parent.height
                                        radius: 4
                                        color: scanPathField.activeFocus ? "#383C42" : "#343638"
                                        border.color: scanPathField.activeFocus ? "#3C7EFF" : "#3A3F46"
                                        border.width: 1

                                        TextField {
                                            id: scanPathField
                                            anchors.fill: parent
                                            anchors.leftMargin: 16
                                            anchors.rightMargin: 44
                                            color: "#E8EAF0"
                                            font.family: "Alibaba PuHuiTi 3.0"
                                            font.pixelSize: 14
                                            placeholderText: "请输入扫描目录"
                                            placeholderTextColor: "#7E8796"
                                            background: Item {}
                                            selectByMouse: true
                                            onAccepted: root.startHardwareScan()
                                        }

                                        Rectangle {
                                            width: 24
                                            height: 24
                                            radius: 12
                                            anchors.right: parent.right
                                            anchors.rightMargin: 10
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: "#3C7EFF"

                                            Text {
                                                anchors.centerIn: parent
                                                text: "⌕"
                                                color: "#FFFFFF"
                                                font.pixelSize: 13
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: scanFolderDialog.open()
                                            }
                                        }
                                    }

                                    DataStoreButton {
                                        width: 108
                                        height: parent.height
                                        text: $MainViewController.isScanning ? "扫描中..." : "开始扫描"
                                        normalColor: "#3C7EFF"
                                        hoverColor: "#5D91FF"
                                        pressedColor: "#2A63DA"
                                        fontSize: 14
                                        enabledButton: !$MainViewController.isScanning
                                        onClicked: root.startHardwareScan()
                                    }
                                }

                                Rectangle {
                                    width: parent.width
                                    height: 8
                                    radius: 4
                                    color: "#30343A"
                                    visible: $MainViewController.isScanning || $MainViewController.scanProgress > 0

                                    Rectangle {
                                        width: parent.width * $MainViewController.scanProgress
                                        height: parent.height
                                        radius: 4
                                        color: "#3C7EFF"

                                        Behavior on width {
                                            NumberAnimation { duration: 150 }
                                        }
                                    }
                                }

                                Row {
                                    width: parent.width
                                    spacing: 20
                                    visible: $MainViewController.isScanning || $MainViewController.scanProgress > 0

                                    Text {
                                        text: "进度：" + Math.round($MainViewController.scanProgress * 100) + "%"
                                        color: "#A8AFBC"
                                        font.pixelSize: 12
                                        font.family: "Alibaba PuHuiTi 3.0"
                                    }

                                    Text {
                                        text: "已扫描：" + $MainViewController.scanScannedFolders + "/" + $MainViewController.scanTotalFolders
                                        color: "#A8AFBC"
                                        font.pixelSize: 12
                                        font.family: "Alibaba PuHuiTi 3.0"
                                    }

                                    Text {
                                        text: "成功：" + $MainViewController.scanPairedCount
                                        color: "#A8AFBC"
                                        font.pixelSize: 12
                                        font.family: "Alibaba PuHuiTi 3.0"
                                    }
                                }

                                Item {
                                    width: parent.width
                                    height: root.uploadProcessingMode !== 0 ? 26 : 0
                                    visible: root.uploadProcessingMode !== 0
                                    clip: true

                                    Text {
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "算法选择"
                                        color: "#FFFFFF"
                                        font.pixelSize: 14
                                        font.family: "Alibaba PuHuiTi 3.0"
                                    }

                                    Row {
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 20

                                        Item {
                                            width: 78
                                            height: 20

                                            Row {
                                                anchors.fill: parent
                                                spacing: 8

                                                Rectangle {
                                                    width: 14
                                                    height: 14
                                                    radius: 7
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    color: "transparent"
                                                    border.color: uploadMethodIndex === 1 ? "#3C7EFF" : "#7E8796"
                                                    border.width: 2

                                                    Rectangle {
                                                        width: 6
                                                        height: 6
                                                        radius: 3
                                                        anchors.centerIn: parent
                                                        color: "#3C7EFF"
                                                        visible: uploadMethodIndex === 1
                                                    }
                                                }

                                                Text {
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: "深度学习"
                                                    color: "#E6FFFFFF"
                                                    font.pixelSize: 14
                                                    font.family: "Alibaba PuHuiTi 3.0"
                                                }
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: uploadMethodIndex = 1
                                            }
                                        }

                                        Item {
                                            width: 78
                                            height: 20

                                            Row {
                                                anchors.fill: parent
                                                spacing: 8

                                                Rectangle {
                                                    width: 14
                                                    height: 14
                                                    radius: 7
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    color: "transparent"
                                                    border.color: uploadMethodIndex === 0 ? "#3C7EFF" : "#7E8796"
                                                    border.width: 2

                                                    Rectangle {
                                                        width: 6
                                                        height: 6
                                                        radius: 3
                                                        anchors.centerIn: parent
                                                        color: "#3C7EFF"
                                                        visible: uploadMethodIndex === 0
                                                    }
                                                }

                                                Text {
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: "传统算法"
                                                    color: "#E6FFFFFF"
                                                    font.pixelSize: 14
                                                    font.family: "Alibaba PuHuiTi 3.0"
                                                }
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: uploadMethodIndex = 0
                                            }
                                        }
                                    }
                                }

                                Item {
                                    width: parent.width
                                    height: 26

                                    Text {
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "文件列表"
                                        color: "#FFFFFF"
                                        font.pixelSize: 14
                                        font.family: "Alibaba PuHuiTi 3.0"
                                    }
                                }

                                Rectangle {
                                    width: parent.width
                                    height: parent.height - (($MainViewController.isScanning || $MainViewController.scanProgress > 0) ? 148 : 98)
                                    radius: 4
                                    color: "#262729"
                                    border.color: "#3A3F46"
                                    border.width: 1
                                    clip: true

                                    Column {
                                        anchors.fill: parent
                                        spacing: 0

                                        Rectangle {
                                            width: parent.width
                                            height: 50
                                            color: "#3A3A3A"

                                            Row {
                                                anchors.fill: parent
                                                Item {
                                                    width: 52
                                                    height: parent.height

                                                    Rectangle {
                                                        width: 18
                                                        height: 18
                                                        radius: 4
                                                        anchors.centerIn: parent
                                                        color: root.scanAllChecked ? "#3C7EFF" : "#3A3D42"
                                                        border.color: root.scanAllChecked ? "#3C7EFF" : "#4B5057"
                                                        border.width: 1

                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: root.scanAllChecked ? "✓" : ""
                                                            color: "#FFFFFF"
                                                            font.pixelSize: 11
                                                            font.family: "Alibaba PuHuiTi 3.0"
                                                        }

                                                        MouseArea {
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                if (root.scanAllChecked)
                                                                    $MriPairResultModel.deselectAll()
                                                                else
                                                                    $MriPairResultModel.selectAll()
                                                            }
                                                        }
                                                    }
                                                }
                                                Item { width: (parent.width - 52) * 0.08; height: parent.height
                                                    Text { anchors.centerIn: parent; text: "序号"; color: "#FFFFFF"; font.pixelSize: 14; font.family: "Alibaba PuHuiTi 3.0" } }
                                                Item { width: (parent.width - 52) * 0.18; height: parent.height
                                                    Text { anchors.centerIn: parent; text: "姓名"; color: "#FFFFFF"; font.pixelSize: 14; font.family: "Alibaba PuHuiTi 3.0" } }
                                                Item { width: (parent.width - 52) * 0.18; height: parent.height
                                                    Text { anchors.centerIn: parent; text: "病历号"; color: "#FFFFFF"; font.pixelSize: 14; font.family: "Alibaba PuHuiTi 3.0" } }
                                                Item { width: (parent.width - 52) * 0.20; height: parent.height
                                                    Text { anchors.centerIn: parent; text: "检查日期"; color: "#FFFFFF"; font.pixelSize: 14; font.family: "Alibaba PuHuiTi 3.0" } }
                                                Item { width: (parent.width - 52) * 0.20; height: parent.height
                                                    Text { anchors.centerIn: parent; text: "检查类型"; color: "#FFFFFF"; font.pixelSize: 14; font.family: "Alibaba PuHuiTi 3.0" } }
                                                Item { width: (parent.width - 52) * 0.16; height: parent.height
                                                    Text { anchors.centerIn: parent; text: "层数"; color: "#FFFFFF"; font.pixelSize: 14; font.family: "Alibaba PuHuiTi 3.0" } }
                                            }
                                        }

                                        ListView {
                                            id: hardwareResultList
                                            width: parent.width
                                            height: parent.height - 50
                                            clip: true
                                            model: $MriPairResultModel

                                            ScrollBar.vertical: ScrollBar {
                                                policy: ScrollBar.AsNeeded
                                                background: Rectangle { color: "#1D2128" }
                                                contentItem: Rectangle {
                                                    implicitWidth: 8
                                                    radius: 4
                                                    color: "#4A556B"
                                                }
                                            }

                                            delegate: Rectangle {
                                                width: hardwareResultList.width
                                                height: 52
                                                color: model.isChecked ? "#314269" : rowHover.containsMouse ? "#2B2D31" : "#262729"

                                                Row {
                                                    anchors.fill: parent

                                                    Item {
                                                        width: 52
                                                        height: parent.height

                                                        Rectangle {
                                                            width: 18
                                                            height: 18
                                                            radius: 4
                                                            anchors.centerIn: parent
                                                            color: model.isChecked ? "#3C7EFF" : "#3A3D42"
                                                            border.color: model.isChecked ? "#3C7EFF" : "#4B5057"
                                                            border.width: 1

                                                            Text {
                                                                anchors.centerIn: parent
                                                                text: model.isChecked ? "✓" : ""
                                                                color: "#FFFFFF"
                                                                font.pixelSize: 11
                                                            }

                                                            MouseArea {
                                                                anchors.fill: parent
                                                                hoverEnabled: true
                                                                cursorShape: Qt.PointingHandCursor
                                                                onClicked: $MriPairResultModel.toggleChecked(index)
                                                            }
                                                        }
                                                    }

                                                    Item { width: (parent.width - 52) * 0.08; height: parent.height
                                                        Text { anchors.centerIn: parent; text: (index + 1 < 10 ? "0" : "") + (index + 1); color: "#E6E6E6"; font.pixelSize: 14; font.family: "Alibaba PuHuiTi 3.0" } }
                                                    Item { width: (parent.width - 52) * 0.18; height: parent.height
                                                        Text { anchors.centerIn: parent; text: model.patientName || "-"; color: "#E6E6E6"; font.pixelSize: 14; font.family: "Alibaba PuHuiTi 3.0" } }
                                                    Item { width: (parent.width - 52) * 0.18; height: parent.height
                                                        Text { anchors.centerIn: parent; text: model.patientId || "-"; color: "#E6E6E6"; font.pixelSize: 14; font.family: "Alibaba PuHuiTi 3.0" } }
                                                    Item { width: (parent.width - 52) * 0.20; height: parent.height
                                                        Text { anchors.centerIn: parent; text: root.formatDateText(model.studyDate); color: "#E6E6E6"; font.pixelSize: 14; font.family: "Alibaba PuHuiTi 3.0" } }
                                                    Item { width: (parent.width - 52) * 0.20; height: parent.height
                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: model.scanMode === 1 ? "T1W" : (model.isComplete ? "MRI 配对" : "待补全")
                                                            color: "#E6E6E6"
                                                            font.pixelSize: 14
                                                            font.family: "Alibaba PuHuiTi 3.0"
                                                        } }
                                                    Item { width: (parent.width - 52) * 0.16; height: parent.height
                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: model.scanMode === 1
                                                                  ? (model.t1ImageCount || 0)
                                                                  : ((model.t1ImageCount || 0) + (model.boldImageCount || 0))
                                                            color: "#E6E6E6"
                                                            font.pixelSize: 14
                                                            font.family: "Alibaba PuHuiTi 3.0"
                                                        } }
                                                }

                                                Rectangle {
                                                    anchors.left: parent.left
                                                    anchors.right: parent.right
                                                    anchors.bottom: parent.bottom
                                                    height: 1
                                                    color: "#3A3F46"
                                                }

                                                MouseArea {
                                                    id: rowHover
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    z: -1
                                                    onClicked: $MriPairResultModel.toggleChecked(index)
                                                }
                                            }
                                        }
                                    }
                                }

                            }

                            Rectangle {
                                anchors.fill: parent
                                visible: uploadTabIndex === 1
                                color: "transparent"

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 10

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "网络获取"
                                        color: "#FFFFFF"
                                        font.pixelSize: 18
                                        font.family: "Alibaba PuHuiTi 3.0"
                                        font.bold: true
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "该模块暂未接入，当前请使用硬件扫描完成数据上传。"
                                        color: "#A8AFBC"
                                        font.pixelSize: 14
                                        font.family: "Alibaba PuHuiTi 3.0"
                                    }
                                }
                            }
                        }

                        Item {
                            width: parent.width
                            height: 44

                            Row {
                                anchors.centerIn: parent
                                spacing: 10

                                DataStoreButton {
                                    width: 96
                                    height: 34
                                    text: "取消"
                                    normalColor: "#3A3F46"
                                    hoverColor: "#4A515C"
                                    pressedColor: "#2F343B"
                                    fontSize: 14
                                    onClicked: root.closeUploadDialog()
                                }

                                DataStoreButton {
                                    width: 96
                                    height: 34
                                    text: "确认"
                                    normalColor: "#3C7EFF"
                                    hoverColor: "#5D91FF"
                                    pressedColor: "#2A63DA"
                                    fontSize: 14
                                    onClicked: root.confirmUploadTask()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

