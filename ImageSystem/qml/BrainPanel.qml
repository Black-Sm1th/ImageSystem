import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.3
import QtWebEngine 1.10
import "./components"

Rectangle {
    id: rootPanel
    color: "transparent"
    property int currentIndex: 1
    // 三步联合处理状态
    property bool batchProcessing: false
    property bool segmentationDone: false
    property bool networkDone: false
    property int segmentationProgress: 0
    property int networkProgress: 0
    property bool networkIndeterminate: false
    property string selectedOutputPath: ""
    
    // 接收从外部传入的 FourViewPanel 实例
    property var fourViewPanel: null

    // 接收从外部传入的 DataStorePanel 实例
    property var dataStorePanel: null
    
    // 四视图容器（当在脑分割面板时使用）
    property alias fourViewContainer: brainSegmentationContainer
    property var messageManager: null
    property int preShowResultIndex: -1
    property bool isDeepprepOutput: false  // 标记是否为DeepPrep输出结构
    
    // 右侧面板展开状态
    property bool rightPanelExpanded: true
    
    // PDF 生成状态管理
    property int pdfGenerationState: 0  // 0: 默认, 1: 生成中, 2: 完成
    
    // 当前选中的被试ID
    property string currentSubjectId: ""
    property var subjectsList: []
    property var subjectsMetadata: []  // 存储完整的 metadata subjects 数据
    
    // 监听 currentIndex 变化，重置 PDF 状态
    onCurrentIndexChanged: {
        pdfGenerationState = 0
        if(currentIndex === 2){
            $DicomDataModel.setSegDisplayMode($DicomDataModel.showOriginal ? 0 : 2)
        }else{
            $DicomDataModel.setSegDisplayMode(1)
        }
    }
    
    function resetBatchProgress() {
        batchProcessing = true
        segmentationDone = false
        networkDone = false
        segmentationProgress = 0
        networkProgress = 0
        networkIndeterminate = true
    }

    function tryFinishBatch() {
        if (segmentationDone && networkDone) {
            batchProcessing = false
        }
    }

    function detectOutputType(path) {
        // 使用C++方法检测输出类型
        isDeepprepOutput = $MainViewController.isDeepprepOutput(path)
    }

    function normalizeDateKey(value) {
        if (!value)
            return ""

        return String(value).replace(/[^0-9]/g, "").slice(0, 8)
    }

    function resolvePreferredSubjectIndex(metadataSubjects, preferredCase) {
        if (!metadataSubjects || metadataSubjects.length === 0)
            return 0

        if (!preferredCase)
            return 0

        var preferredPatientId = ""
        var preferredPatientName = ""
        var preferredExamDate = ""
        var preferredSeriesUid = ""

        if (typeof preferredCase === "string") {
            preferredPatientId = preferredCase
        } else {
            preferredPatientId = preferredCase.patientId || ""
            preferredPatientName = preferredCase.patientName || ""
            preferredExamDate = normalizeDateKey(preferredCase.examDate || "")
            preferredSeriesUid = preferredCase.seriesUid || ""
        }

        for (var i = 0; i < metadataSubjects.length; ++i) {
            var bySeries = metadataSubjects[i]
            if (preferredSeriesUid !== ""
                    && (bySeries.primarySeriesUid === preferredSeriesUid
                        || bySeries.t1SeriesUid === preferredSeriesUid
                        || bySeries.boldSeriesUid === preferredSeriesUid)) {
                return i
            }
        }

        for (var j = 0; j < metadataSubjects.length; ++j) {
            var byPatientAndDate = metadataSubjects[j]
            if (preferredPatientId !== ""
                    && byPatientAndDate.patientId === preferredPatientId
                    && preferredExamDate !== ""
                    && normalizeDateKey(byPatientAndDate.studyDate || "") === preferredExamDate) {
                return j
            }
        }

        for (var k = 0; k < metadataSubjects.length; ++k) {
            if (preferredPatientId !== "" && metadataSubjects[k].patientId === preferredPatientId)
                return k
        }

        for (var m = 0; m < metadataSubjects.length; ++m) {
            var byNameAndDate = metadataSubjects[m]
            if (preferredPatientName !== ""
                    && byNameAndDate.patientName === preferredPatientName
                    && preferredExamDate !== ""
                    && normalizeDateKey(byNameAndDate.studyDate || "") === preferredExamDate) {
                return m
            }
        }

        return 0
    }

    function refreshDefaultPrepPaths() {
        var defaultPaths = $MainViewController.defaultProcessingPaths()
        bidsDir.text = defaultPaths.bidsPath || ""
        outputDir.text = defaultPaths.outputPath || ""
        outputDetailDir.text = outputDir.text
    }

    function startUnifiedImports(url, normalizedPath, subjectId, currentPatientId) {
        selectedOutputPath = normalizedPath
        outputDetailDir.text = normalizedPath
        currentSubjectId = subjectId
        detectOutputType(normalizedPath)
        resetBatchProgress()
        // 同步触发脑区分割与脑网络分析
        $DicomDataModel.loadSegBrainDirectory(url, subjectId)
        $MainViewController.importBrainData(url, subjectId, currentPatientId)
        // 默认展示分割结果预览
        segBtnMouseArea.clicked(Qt.LeftButton)
    }

    function selectSubjectAtIndex(selectedIndex) {
        if (selectedIndex < 0 || subjectsMetadata.length <= selectedIndex || outputDetailDir.text === "")
            return

        currentSubjectId = subjectsMetadata[selectedIndex].subjectId
        var path = outputDetailDir.text
        var subjectUrl = "file:///" + path
        startUnifiedImports(subjectUrl, path, currentSubjectId, subjectsMetadata[selectedIndex].patientId)
    }

    function loadOutputDirectory(path, preferredCase) {
        if (!path || path === "")
            return false

        var normalizedPath = path.replace(/\\/g, "/")
        outputDetailDir.text = normalizedPath
        selectedOutputPath = normalizedPath

        var metadata = $MainViewController.readMetadataFile(normalizedPath)
        if (!(metadata && metadata.subjects && metadata.subjects.length > 0)) {
            subjectsMetadata = []
            subjectsList = []
            patientComboBox.model = []
            currentSubjectId = ""
            if (messageManager)
                messageManager.warning(qsTr("未在该文件夹中找到被试数据"), 2000)
            return false
        }

        subjectsMetadata = metadata.subjects
        var displayList = []
        var selectedIndex = resolvePreferredSubjectIndex(metadata.subjects, preferredCase)
        for (var i = 0; i < metadata.subjects.length; i++) {
            var sub = metadata.subjects[i]
            displayList.push(sub.patientName + "(" + sub.patientId + ")")
        }

        subjectsList = displayList
        patientComboBox.model = displayList
        patientComboBox.selectedText = displayList[selectedIndex]
        patientComboBox.selectedIndices = [selectedIndex]

        $MainViewController.loadBrainAgePredictions(normalizedPath)
        selectSubjectAtIndex(selectedIndex)
        return true
    }

    Component.onCompleted: {
        refreshDefaultPrepPaths()
        licenseFile.text = $MainViewController.defaultLicenseFilePath()
    }

    FileDialog {
        id: preFileDialog
        title: qsTr("选择要预处理的文件夹")
        selectFolder: true
        onAccepted: {
            if (fileUrls.length === 0)
                return

            var url = fileUrls[0].toString()
            var path = url
            if (path.startsWith("file:///")) {
                path = path.substring("file:///".length)
            }
            // 统一为正斜杠，方便字符串处理
            path = path.replace(/\\/g, "/")

            dicomDir.text = path

            refreshDefaultPrepPaths()
        }
    }
    
    FileDialog {
        id: licenseFileDialog
        title: qsTr("选择license")
        onAccepted: {
            if (fileUrls.length === 0)
                return

            var url = fileUrls[0].toString()
            var path = url
            if (path.startsWith("file:///")) {
                path = path.substring("file:///".length)
            }
            // 统一为正斜杠，方便字符串处理
            path = path.replace(/\\/g, "/")

            licenseFile.text = path
        }
    }

    FileDialog {
        id: outputDetailDialog
        title: qsTr("选择要上传的文件")
        selectFolder: true
        onAccepted: {
            if (fileUrls.length === 0)
                return

            var url = fileUrls[0].toString()
            var path = url
            if (path.startsWith("file:///")) {
                path = path.substring("file:///".length)
            }
            // 统一为正斜杠，方便字符串处理
            path = path.replace(/\\/g, "/")
            
            // 更新路径显示
            outputDetailDir.text = path
            selectedOutputPath = path
            
            // 读取 metadata.json 文件
            var metadata = $MainViewController.readMetadataFile(path)
            if (metadata && metadata.subjects && metadata.subjects.length > 0) {
                subjectsMetadata = metadata.subjects
                // 生成显示列表：patientName(patientId)
                var displayList = []
                for (var i = 0; i < metadata.subjects.length; i++) {
                    var sub = metadata.subjects[i]
                    displayList.push(sub.patientName + "(" + sub.patientId + ")")
                }
                subjectsList = displayList
                patientComboBox.model = displayList
                if (displayList.length > 0) {
                    patientComboBox.selectedText = displayList[0]
                    patientComboBox.selectedIndices = [0]
                    // 使用 subjectId 触发选择变更
                    currentSubjectId = subjectsMetadata[0].subjectId
                    var subjectUrl = "file:///" + path
                    $MainViewController.loadBrainAgePredictions(path)
                    startUnifiedImports(subjectUrl, path, currentSubjectId, subjectsMetadata[0].patientId)
                }
            } else {
                subjectsMetadata = []
                subjectsList = []
                patientComboBox.model = []
                messageManager.warning(qsTr("未在该文件夹中找到被试数据"), 2000)
            }
        }
    }

    FileDialog {
        id: pdfSaveDialog
        title: qsTr("选择报告保存路径")
        selectExisting: false
        selectFolder: false
        nameFilters: ["PDF Files (*.pdf)"]
        defaultSuffix: "pdf"
        onAccepted: {
            if (fileUrls.length === 0)
                return

            var url = fileUrls[0].toString()
            var path = url
            if (path.startsWith("file:///")) {
                path = path.substring("file:///".length)
            }
            // 统一为正斜杠，方便字符串处理
            path = path.replace(/\\/g, "/")
            
            // 如果路径没有 .pdf 后缀，添加它
            if (!path.toLowerCase().endsWith(".pdf")) {
                path += ".pdf"
            }
            
            reportSavePath.text = path;
        }
    }
    
    // 联合处理进度对话框
    Rectangle {
        id: batchProgressDialog
        anchors.fill: parent
        color: "#80000000"
        visible: batchProcessing
        z: 1000
        
        Rectangle {
            anchors.centerIn: parent
            width: 460
            height: processCol.height
            color: "#2a2a2a"
            border.color: "#0078d4"
            border.width: 2
            radius: 10
            
            Column {
                id: processCol
                width: parent.width
                padding: 20
                spacing: 18
                
                Label {
                    width: parent.width
                    text: qsTr("数据处理中，请稍候...")
                    color: "#ffffff"
                    font.pixelSize: 18
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Column {
                    width: parent.width
                    spacing: 10

                    Row {
                        spacing: 10
                        width: parent.width - 40
                        Label { text: qsTr("脑区分割"); color: "#ffffff"; font.pixelSize: 16; width: 110 }
                        ProgressBar {
                            id: segBar
                            from: 0; to: 100
                            indeterminate: !segmentationDone && segmentationProgress === 0
                            value: segmentationProgress
                            width: parent.width - 130
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Row {
                        spacing: 10
                        width: parent.width - 40
                        Label { text: qsTr("脑网络分析"); color: "#ffffff"; font.pixelSize: 16; width: 110 }
                        ProgressBar {
                            id: netBar
                            from: 0; to: 100
                            indeterminate: networkIndeterminate
                            value: networkProgress
                            width: parent.width - 130
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }
    
    // 连接信号
    Connections {
        target: $MainViewController
        
        function onBrainAnalysisStarted() {
            networkDone = false
            networkProgress = 0
            networkIndeterminate = true
            batchProcessing = true
        }
        
        function onBrainAnalysisFinished(success) {
            networkIndeterminate = false
            networkProgress = success ? 100 : networkProgress
            networkDone = true
            if (!success && messageManager) {
                messageManager.error(qsTr("脑网络分析失败，请检查数据"))
            }
            tryFinishBatch()
        }

        function onNetworkTableIndexChanged(index){
            regionTableView.currentIndex = index
        }
    }

    Connections {
        target: $DicomDataModel

        function onSegLoadingStarted() {
            segmentationDone = false
            segmentationProgress = 0
            batchProcessing = true
        }

        function onSegLoadingProgress(percent, message) {
            segmentationProgress = percent
        }

        function onSegLoadingFinished(success, message) {
            segmentationProgress = success ? 100 : segmentationProgress
            segmentationDone = true
            if (!success && messageManager) {
                messageManager.error(qsTr("脑区分割加载失败"))
            }
            tryFinishBatch()
        }
    }
    
    Rectangle {
        id: midPanel
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: rightPanel.left
        anchors.rightMargin: 16
        color: "transparent"
        Column{
            spacing: 16
            width: parent.width
            Rectangle {
                height: 60
                width: parent.width
                color: "#1AFFFFFF"
                radius: 8
                Row{
                    height: parent.height
                    spacing: 20
                    anchors.horizontalCenter: parent.horizontalCenter
                    
                    // 数据预处理按钮
                    Item{
                        id: btn1
                        height: 36
                        width: 119
                        anchors.verticalCenter: parent.verticalCenter
                        
                        property bool isHovered: false
                        scale: isHovered && currentIndex !== 1 ? 1.05 : 1.0
                        
                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }
                        
                        Image {
                            id: btn1Bg
                            anchors.fill: parent
                            source: currentIndex === 1 ? "qrc:/image/brainTitleBtn.png" : ""
                            fillMode: Image.Stretch
                            opacity: 0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
                            }
                            
                            Component.onCompleted: {
                                if (currentIndex === 1) opacity = 1
                            }
                            
                            Connections {
                                target: btn1
                                function onVisibleChanged() {
                                    if (currentIndex === 1) btn1Bg.opacity = 1
                                    else btn1Bg.opacity = 0
                                }
                            }
                        }
                        
                        // 滑动指示器
                        Rectangle {
                            width: parent.width
                            height: parent.height
                            color: "transparent"
                            opacity: currentIndex === 1 ? 1 : 0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
                            }
                        }
                        
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("数据预处理")
                            color: currentIndex === 1 ? "#ffffff" : "#80FFFFFF"
                            font.pixelSize: 16
                            font.family: "Alibaba PuHuiTi 3.0"
                            
                            Behavior on color {
                                ColorAnimation { duration: 200 }
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: btn1.isHovered = true
                            onExited: btn1.isHovered = false
                            
                            onClicked: {
                                currentIndex = 1
                                btn1Bg.opacity = 1
                            }
                        }
                        
                        onVisibleChanged: {
                            if (currentIndex === 1) btn1Bg.opacity = 1
                            else btn1Bg.opacity = 0
                        }
                    }
                    
                    // 脑区分割按钮
                    Item{
                        id: btn2
                        height: 36
                        width: 119
                        anchors.verticalCenter: parent.verticalCenter
                        
                        property bool isHovered: false
                        scale: isHovered && currentIndex !== 2 ? 1.05 : 1.0
                        
                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }
                        
                        Image {
                            id: btn2Bg
                            anchors.fill: parent
                            source: currentIndex === 2 ? "qrc:/image/brainTitleBtn.png" : ""
                            fillMode: Image.Stretch
                            opacity: 0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
                            }
                            
                            Component.onCompleted: {
                                if (currentIndex === 2) opacity = 1
                            }
                        }
                        
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("脑区分割")
                            color: currentIndex === 2 ? "#ffffff" : "#80FFFFFF"
                            font.pixelSize: 16
                            font.family: "Alibaba PuHuiTi 3.0"
                            
                            Behavior on color {
                                ColorAnimation { duration: 200 }
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: btn2.isHovered = true
                            onExited: btn2.isHovered = false
                            
                            onClicked: {
                                currentIndex = 2
                                btn2Bg.opacity = 1
                            }
                        }
                        
                        onVisibleChanged: {
                            if (currentIndex === 2) btn2Bg.opacity = 1
                            else btn2Bg.opacity = 0
                        }
                    }
                    
                    // 脑网络按钮
                    Item{
                        id: btn3
                        height: 36
                        width: 119
                        anchors.verticalCenter: parent.verticalCenter
                        
                        property bool isHovered: false
                        scale: isHovered && currentIndex !== 3 ? 1.05 : 1.0
                        
                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }
                        
                        Image {
                            id: btn3Bg
                            anchors.fill: parent
                            source: currentIndex === 3 ? "qrc:/image/brainTitleBtn.png" : ""
                            fillMode: Image.Stretch
                            opacity: 0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
                            }
                            
                            Component.onCompleted: {
                                if (currentIndex === 3) opacity = 1
                            }
                        }
                        
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("脑网络")
                            color: currentIndex === 3 ? "#ffffff" : "#80FFFFFF"
                            font.pixelSize: 16
                            font.family: "Alibaba PuHuiTi 3.0"
                            
                            Behavior on color {
                                ColorAnimation { duration: 200 }
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: btn3.isHovered = true
                            onExited: btn3.isHovered = false
                            
                            onClicked: {
                                currentIndex = 3
                                btn3Bg.opacity = 1
                            }
                        }
                        
                        onVisibleChanged: {
                            if (currentIndex === 3) btn3Bg.opacity = 1
                            else btn3Bg.opacity = 0
                        }
                    }
                    
                    // AI分析按钮
                    Item{
                        id: btn4
                        height: 36
                        width: 119
                        anchors.verticalCenter: parent.verticalCenter
                        
                        property bool isHovered: false
                        scale: isHovered && currentIndex !== 4 ? 1.05 : 1.0
                        
                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }
                        
                        Image {
                            id: btn4Bg
                            anchors.fill: parent
                            source: currentIndex === 4 ? "qrc:/image/brainTitleBtn.png" : ""
                            fillMode: Image.Stretch
                            opacity: 0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
                            }
                            
                            Component.onCompleted: {
                                if (currentIndex === 4) opacity = 1
                            }
                        }
                        
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("AI分析")
                            color: currentIndex === 4 ? "#ffffff" : "#80FFFFFF"
                            font.pixelSize: 16
                            font.family: "Alibaba PuHuiTi 3.0"
                            
                            Behavior on color {
                                ColorAnimation { duration: 200 }
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: btn4.isHovered = true
                            onExited: btn4.isHovered = false
                            
                            onClicked: {
                                currentIndex = 4
                                btn4Bg.opacity = 1
                            }
                        }
                        
                        onVisibleChanged: {
                            if (currentIndex === 4) btn4Bg.opacity = 1
                            else btn4Bg.opacity = 0
                        }
                    }
                    
                    // 生成报告按钮
                    Item{
                        id: btn5
                        height: 36
                        width: 119
                        anchors.verticalCenter: parent.verticalCenter
                        
                        property bool isHovered: false
                        scale: isHovered && currentIndex !== 5 ? 1.05 : 1.0
                        
                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }
                        
                        Image {
                            id: btn5Bg
                            anchors.fill: parent
                            source: currentIndex === 5 ? "qrc:/image/brainTitleBtn.png" : ""
                            fillMode: Image.Stretch
                            opacity: 0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
                            }
                            
                            Component.onCompleted: {
                                if (currentIndex === 5) opacity = 1
                            }
                        }
                        
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("生成报告")
                            color: currentIndex === 5 ? "#ffffff" : "#80FFFFFF"
                            font.pixelSize: 16
                            font.family: "Alibaba PuHuiTi 3.0"
                            
                            Behavior on color {
                                ColorAnimation { duration: 200 }
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: btn5.isHovered = true
                            onExited: btn5.isHovered = false
                            
                            onClicked: {
                                currentIndex = 5
                                btn5Bg.opacity = 1
                            }
                        }
                        
                        onVisibleChanged: {
                            if (currentIndex === 5) btn5Bg.opacity = 1
                            else btn5Bg.opacity = 0
                        }
                    }
                }
            }
            Rectangle{
                width: parent.width
                height: midPanel.height - 16 - 60
                visible: currentIndex === 1
                color: "transparent"
                border.width: 1
                border.color: "#484849"
                WebEngineView {
                    id: preResult
                    anchors.fill: parent
                    anchors.margins: 1
                    backgroundColor: "#000000"
                }
            }
            Item {
                id: brainSegmentationContainer
                width: parent.width
                height: midPanel.height - 16 - 60
                visible: currentIndex === 2 || currentIndex === 4
            }
            Rectangle{
                id: networkMain
                width: parent.width
                height: midPanel.height - 16 - 60
                visible: currentIndex === 3
                color: "transparent"
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    width: (parent.width - 10) / 2
                    height: (parent.height - 10) / 2
                    color: "#000000"
                    border.color: "#484849"
                    border.width: 1
                    Image{
                        anchors.fill: parent
                        anchors.margins: 1
                        fillMode: Image.PreserveAspectFit
                        source: $MainViewController.currentRegionplotsUrl
                    }
                }
                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    width: (parent.width - 10) / 2
                    height: (parent.height - 10) / 2
                    color: "#000000"
                    border.color: "#484849"
                    border.width: 1
                    Image{
                        anchors.fill: parent
                        anchors.margins: 1
                        fillMode: Image.PreserveAspectFit
                        source: $MainViewController.currentAlffUrl
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: (parent.width - 10) / 2
                    height: (parent.height - 10) / 2
                    color: "#000000"
                    border.color: "#484849"
                    border.width: 1
                    Image{
                        anchors.fill: parent
                        anchors.margins: 1
                        fillMode: Image.PreserveAspectFit
                        source: $MainViewController.currentCovarianceUrl
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    width: (parent.width - 10) / 2
                    height: (parent.height - 10) / 2
                    color: "#000000"
                    border.color: "#484849"
                    border.width: 1
                    WebEngineView{
                        anchors.fill: parent
                        anchors.margins: 1
                        visible: $MainViewController.currentViewConnectomeUrl !== ""
                        url: $MainViewController.currentViewConnectomeUrl
                    }
                }
            }
            // Rectangle{
            //     id: aiAnalysisContainer
            //     width: parent.width
            //     height: midPanel.height - 16 - 60
            //     visible: currentIndex === 4
            //     color: "transparent"
            //     Rectangle{
            //         anchors.left: parent.left
            //         anchors.top: parent.top
            //         anchors.bottom: parent.bottom
            //         anchors.right: aiRightOperatePanel.left
            //         color: "#030D1F"
            //         radius: 12
            //         Column{
            //             width: Math.max(uploadPicture.width, aiButtonGroup.width)
            //             spacing: 48
            //             anchors.centerIn: parent
            //             Image{
            //                 id: uploadPicture
            //                 width: 306
            //                 height: 306
            //                 source: "qrc:/image/uploadGif/000.png"
            //                 anchors.horizontalCenter: parent.horizontalCenter
            //             }
            //             Label{
            //                 id:brainAgePath
            //                 text: qsTr("")
            //                 color: "#ffffff"
            //                 font.pixelSize: 16
            //                 visible: text !== ""
            //                 width: parent.width - 20
            //                 elide: Text.ElideMiddle
            //                 onTextChanged: {
            //                     // 当重新导入文件时，重置分析结果
            //                     if(text !== "") {
            //                         $MainViewController.predictedBrainAge = 0.0
            //                     }
            //                 }
            //             }
            //             Row{
            //                 id: aiButtonGroup
            //                 height: 48
            //                 spacing: 10
            //                 anchors.horizontalCenter: parent.horizontalCenter
            //                 CustomButton{
            //                     fontSize: 18
            //                     text: qsTr("导入nii.gz")
            //                     radius: 4
            //                     iconSource: "qrc:/image/picture.png"
            //                     width: 188
            //                     height: 48
            //                     onClicked: {
            //                         niiGzDialog.open()
            //                     }
            //                 }
            //                 CustomButton{
            //                     fontSize: 16
            //                     text: qsTr("导入dcm文件夹")
            //                     radius: 4
            //                     backgroundColor: "#293C7EFF"
            //                     borderColor: "#3C7EFF"
            //                     borderWidth: 1
            //                     iconSource: "qrc:/image/fold.png"
            //                     width: 188
            //                     height: 48
            //                     onClicked: {
            //                         dcmFolderDialog.open()
            //                     }
            //                 }
            //             }
            //         }
            //     }
            // }
            Rectangle{
                id: generateReportContainer
                width: parent.width
                height: midPanel.height - 16 - 60
                visible: currentIndex === 5
                color: "transparent"
                Rectangle{
                    height: parent.height * 0.75
                    width: parent.width * 0.45
                    anchors.centerIn: parent
                    color: "transparent"
                    Label {
                        text: qsTr("生成分析报告")
                        color: "#FFFFFF"
                        font.pixelSize: 36
                        font.weight: Font.Medium
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                    }
                    Column{
                        width: parent.width * 0.75
                        spacing: 52
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: parent.height * 0.13
                        Column{
                            width: parent.width
                            spacing: 32
                            Label {
                                text: qsTr("报告将包含以下内容:")
                                color: "#FFFFFF"
                                font.pixelSize: 18
                            }
                            Grid {
                                width: parent.width
                                columns: 2
                                columnSpacing: parent.width * 0.2
                                rowSpacing: 20
                                Row{
                                    height: 24
                                    spacing: 8
                                    Image{
                                        source: "qrc:/image/reportIcon.png"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label{
                                        text: qsTr("脑测量综合评估结果")
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 18
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                Row{
                                    height: 24
                                    spacing: 8
                                    Image{
                                        source: "qrc:/image/reportIcon.png"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label{
                                        text: qsTr("异常建议措施及建议措施")
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 18
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                Row{
                                    height: 24
                                    spacing: 8
                                    Image{
                                        source: "qrc:/image/reportIcon.png"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label{
                                        text: qsTr("脑测量数据纵览")
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 18
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                Row{
                                    height: 24
                                    spacing: 8
                                    Image{
                                        source: "qrc:/image/reportIcon.png"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label{
                                        text: qsTr("脑测量详细数据表格")
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 18
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                Row{
                                    height: 24
                                    spacing: 8
                                    Image{
                                        source: "qrc:/image/reportIcon.png"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label{
                                        text: qsTr("脑网络分析纵览")
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 18
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                Row{
                                    height: 24
                                    spacing: 8
                                    Image{
                                        source: "qrc:/image/reportIcon.png"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label{
                                        text: qsTr("脑网络区域详细数据表格")
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 18
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                Row{
                                    height: 24
                                    spacing: 8
                                    Image{
                                        source: "qrc:/image/reportIcon.png"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label{
                                        text: qsTr("AI脑龄预测分析")
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 18
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }
                        CustomButton{
                            id: generatePdfButton
                            width: parent.width
                            height: 48
                            buttonRadius: 4
                            
                            // 根据状态动态设置属性
                            iconSource: {
                                if (pdfGenerationState === 0) {
                                    return "qrc:/image/generateReport.png"
                                } else if (pdfGenerationState === 1) {
                                    return ""  // 生成中不显示图标
                                } else {
                                    return "qrc:/image/successReport.png"
                                }
                            }
                            
                            text: {
                                if (pdfGenerationState === 0) {
                                    return qsTr("生成pdf分析报告")
                                } else if (pdfGenerationState === 1) {
                                    return qsTr("正在生成中...")
                                } else {
                                    return qsTr("报告生成完成")
                                }
                            }
                            
                            useGradient: pdfGenerationState === 1
                            gradientStartColor: "#3C7EFF"
                            gradientEndColor: "#572499"
                            animateGradient: pdfGenerationState === 1
                            
                            enabled: pdfGenerationState !== 1  // 生成中禁用按钮
                            
                            onClicked: {
                                if(reportSavePath.text === ""){
                                    if (messageManager) {
                                        messageManager.warning(qsTr("请先选择报告保存路径！"), 2000)
                                    }
                                    return
                                }

                                // 设置为生成中状态
                                pdfGenerationState = 1
                                
                                // 使用 Timer 模拟异步执行
                                pdfGenerationTimer.start()
                            }
                            
                            // 异步生成 PDF 的定时器
                            Timer {
                                id: pdfGenerationTimer
                                interval: 100
                                repeat: false
                                onTriggered: {
                                    // 调用 C++ 函数生成 PDF
                                    $MainViewController.generatePdfReport(reportSavePath.text)
                                    pdfGenerationState = 2
                                    if (messageManager) {
                                        messageManager.success(qsTr("PDF 报告生成成功！"), 2000)
                                    }
                                }
                            }
                        }
                    }
                    Row{
                        height: 48
                        width: parent.width
                        spacing: 20
                        anchors.bottom: parent.bottom
                        Label {
                            id: savePathLabel
                            text: qsTr("选择报告保存路径:")
                            color: "#80FFFFFF"
                            font.pixelSize: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        SingleLineTextInput{
                            id: reportSavePath
                            width: parent.width - savePathLabel.width - 40 - 135
                            height: 48
                            inputRadius: 4
                            backgroundColor: "#14FFFFFF"
                            
                            // 监听文本变化，重置 PDF 生成状态
                            onTextChanged: {
                                pdfGenerationState = 0
                            }
                        }
                        CustomButton{
                            width: 135
                            height: 48
                            buttonRadius: 4
                            text: qsTr("选择保存路径")
                            anchors.verticalCenter: parent.verticalCenter
                            onClicked: {
                                pdfSaveDialog.open()
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle{
        id: rightPanel
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        width: (rightPanelExpanded && currentIndex !== 5) ? ((currentIndex === 2 || currentIndex === 3) ? 500 : 400) : 0
        visible: width > 0
        Behavior on width {
            NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
        }
        color: "transparent"
        
        Image {
            anchors.fill: parent
            source: "qrc:/image/rightBackground.png"
            fillMode: Image.Stretch
        }
        
        Rectangle{
            id: preAnalysis
            anchors.fill: parent
            anchors.margins: 16
            color: "transparent"
            visible: currentIndex === 1
            clip: true

            property int currentTabIndex: 1

            Column{
                width: parent.width
                spacing: 16

                Rectangle {
                    id: tabContainer
                    width: parent.width
                    height: 48
                    color: "#16171B"
                    radius: 8

                    Row{
                        id: tabRow
                        height: parent.height
                        anchors.horizontalCenter: parent.horizontalCenter
                        z: 2
                        visible: false
                        // // 传统处理 Tab
                        // Item{
                        //     id: tab1
                        //     height: parent.height
                        //     width: 88
                            
                        //     property bool isHovered: false
                        //     scale: isHovered && preAnalysis.currentTabIndex !== 0 ? 1.05 : 1.0
                            
                        //     Behavior on scale {
                        //         NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        //     }

                        //     Label{
                        //         text: qsTr("传统处理")
                        //         font.pixelSize: 16
                        //         anchors.centerIn: parent
                        //         color: preAnalysis.currentTabIndex === 0 ? "#E5FFFFFF" : "#80FFFFFF"
                        //         font.family: "Alibaba PuHuiTi 3.0"

                        //         Behavior on color {
                        //             ColorAnimation { duration: 200 }
                        //         }
                        //     }

                        //     MouseArea {
                        //         anchors.fill: parent
                        //         cursorShape: Qt.PointingHandCursor
                        //         hoverEnabled: true
                                
                        //         onEntered: tab1.isHovered = true
                        //         onExited: tab1.isHovered = false
                                
                        //         onClicked: {
                        //             textBackground.animateToTab(0)
                        //             tabIndicator.animateToTab(0)
                        //         }
                        //     }
                        // }

                        // // 深度学习 Tab
                        // Item{
                        //     id: tab2
                        //     height: parent.height
                        //     width: 88
                            
                        //     property bool isHovered: false
                        //     scale: isHovered && preAnalysis.currentTabIndex !== 1 ? 1.05 : 1.0
                            
                        //     Behavior on scale {
                        //         NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        //     }

                        //     Label{
                        //         text: qsTr("深度学习")
                        //         font.pixelSize: 16
                        //         anchors.centerIn: parent
                        //         color: preAnalysis.currentTabIndex === 1 ? "#E5FFFFFF" : "#80FFFFFF"
                        //         font.family: "Alibaba PuHuiTi 3.0"

                        //         Behavior on color {
                        //             ColorAnimation { duration: 200 }
                        //         }
                        //     }

                        //     MouseArea {
                        //         anchors.fill: parent
                        //         cursorShape: Qt.PointingHandCursor
                        //         hoverEnabled: true
                                
                        //         onEntered: tab2.isHovered = true
                        //         onExited: tab2.isHovered = false
                                
                        //         onClicked: {
                        //             textBackground.animateToTab(1)
                        //             tabIndicator.animateToTab(1)
                        //         }
                        //     }
                        // }

                        // 预处理 Tab
                        Item{
                            id: tab4
                            height: parent.height
                            width: 88

                            property bool isHovered: false
                            scale: isHovered && preAnalysis.currentTabIndex !== 0 ? 1.05 : 1.0

                            Behavior on scale {
                                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                            }

                            Label{
                                text: qsTr("预处理")
                                font.pixelSize: 16
                                anchors.centerIn: parent
                                color: preAnalysis.currentTabIndex === 0 ? "#E5FFFFFF" : "#80FFFFFF"
                                font.family: "Alibaba PuHuiTi 3.0"

                                Behavior on color {
                                    ColorAnimation { duration: 200 }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true

                                onEntered: tab4.isHovered = true
                                onExited: tab4.isHovered = false

                                onClicked: {
                                    textBackground.animateToTab(0)
                                    tabIndicator.animateToTab(0)
                                }
                            }
                        }

                        // 数据详情 Tab
                        Item{
                            id: tab3
                            height: parent.height
                            width: 88
                            
                            property bool isHovered: false
                            scale: isHovered && preAnalysis.currentTabIndex !== 1 ? 1.05 : 1.0
                            
                            Behavior on scale {
                                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                            }

                            Label{
                                text: qsTr("数据详情")
                                font.pixelSize: 16
                                anchors.centerIn: parent
                                color: preAnalysis.currentTabIndex === 1 ? "#E5FFFFFF" : "#80FFFFFF"
                                font.family: "Alibaba PuHuiTi 3.0"

                                Behavior on color {
                                    ColorAnimation { duration: 200 }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                
                                onEntered: tab3.isHovered = true
                                onExited: tab3.isHovered = false
                                
                                onClicked: {
                                    textBackground.animateToTab(1)
                                    tabIndicator.animateToTab(1)
                                }
                            }
                        }
                    }

                    // 文字背景图片
                    Image {
                        id: textBackground
                        y: (parent.height - height) / 2 - 2
                        source: "qrc:/image/textBackgroundAfter.png"
                        z: 1
                        visible: false
                        property int currentTab: 0
                        property int targetTab: 0
                        
                        Component.onCompleted: {
                            x = getTabX(0)
                        }
                        
                        function getTabX(tabIndex) {
                            var rowX = (tabContainer.width - 176) / 2
                            return rowX + tabIndex * 88
                        }
                        
                        function animateToTab(tabIndex) {
                            if (currentTab === tabIndex) return
                            targetTab = tabIndex
                            backgroundSlideAnimation.start()
                        }
                        
                        SequentialAnimation {
                            id: backgroundSlideAnimation
                            
                            // 第一步：变换为 Before 图片并缩短到10px
                            ParallelAnimation {
                                PropertyAction {
                                    target: textBackground
                                    property: "source"
                                    value: "qrc:/image/textBackgroundBefore.png"
                                }
                                NumberAnimation {
                                    target: textBackground
                                    property: "width"
                                    to: 10
                                    duration: 150
                                    easing.type: Easing.InQuad
                                }
                                NumberAnimation {
                                    target: textBackground
                                    property: "x"
                                    to: textBackground.getTabX(textBackground.currentTab) + 39
                                    duration: 150
                                    easing.type: Easing.InQuad
                                }
                            }
                            
                            // 第二步：平移到目标tab的中间
                            NumberAnimation {
                                target: textBackground
                                property: "x"
                                to: textBackground.getTabX(textBackground.targetTab) + 39
                                duration: 200
                                easing.type: Easing.InOutQuad
                            }
                            
                            // 第三步：变换为 After 图片并伸展到88px
                            ParallelAnimation {
                                PropertyAction {
                                    target: textBackground
                                    property: "source"
                                    value: "qrc:/image/textBackgroundAfter.png"
                                }
                                NumberAnimation {
                                    target: textBackground
                                    property: "width"
                                    to: 88
                                    duration: 150
                                    easing.type: Easing.OutQuad
                                }
                                NumberAnimation {
                                    target: textBackground
                                    property: "x"
                                    to: textBackground.getTabX(textBackground.targetTab)
                                    duration: 150
                                    easing.type: Easing.OutQuad
                                }
                            }
                            
                            // 动画结束后更新状态
                            ScriptAction {
                                script: {
                                    textBackground.currentTab = textBackground.targetTab
                                }
                            }
                        }
                    }

                    // 蓝色横线指示器
                    Rectangle {
                        id: tabIndicator
                        anchors.bottom: parent.bottom
                        height: 2
                        width: 88
                        color: "#0078d4"
                        radius: 1
                        visible: false

                        property int currentTab: 0
                        property int targetTab: 0

                        Component.onCompleted: {
                            x = getTabX(0)
                        }

                        function getTabX(tabIndex) {
                            var rowX = (tabContainer.width - 176) / 2
                            return rowX + tabIndex * 88
                        }

                        function animateToTab(tabIndex) {
                            if (currentTab === tabIndex) return
                            targetTab = tabIndex
                            preAnalysis.currentTabIndex = tabIndex
                            slideAnimation.start()
                        }

                        SequentialAnimation {
                            id: slideAnimation

                            // 第一步：从两边向中间缩短到10px
                            ParallelAnimation {
                                NumberAnimation {
                                    target: tabIndicator
                                    property: "width"
                                    to: 10
                                    duration: 150
                                    easing.type: Easing.InQuad
                                }
                                NumberAnimation {
                                    target: tabIndicator
                                    property: "x"
                                    to: tabIndicator.getTabX(tabIndicator.currentTab) + 39
                                    duration: 150
                                    easing.type: Easing.InQuad
                                }
                            }
                            NumberAnimation {
                                target: tabIndicator
                                property: "x"
                                to: tabIndicator.getTabX(tabIndicator.targetTab) + 39
                                duration: 200
                                easing.type: Easing.InOutQuad
                            }
                            ParallelAnimation {
                                NumberAnimation {
                                    target: tabIndicator
                                    property: "width"
                                    to: 88
                                    duration: 150
                                    easing.type: Easing.OutQuad
                                }
                                NumberAnimation {
                                    target: tabIndicator
                                    property: "x"
                                    to: tabIndicator.getTabX(tabIndicator.targetTab)
                                    duration: 150
                                    easing.type: Easing.OutQuad
                                }
                            }

                            ScriptAction {
                                script: {
                                    tabIndicator.currentTab = tabIndicator.targetTab
                                }
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        text: qsTr("数据详情")
                        font.pixelSize: 16
                        color: "#E5FFFFFF"
                        font.family: "Alibaba PuHuiTi 3.0"
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: 88
                        height: 2
                        color: "#0078d4"
                        radius: 1
                    }
                }

                ScrollView{
                    width: parent.width
                    height: preAnalysis.height - 16 - 48
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                    visible: false
                    Column{
                        id: prepCol
                        width: parent.width
                        spacing: 12
                        // 四个标签的最大宽度，保持对齐
                        property int maxLabelWidth: Math.max(
                                                        Math.max(label1.implicitWidth, label2.implicitWidth),
                                                        Math.max(label3.implicitWidth, label4.implicitWidth))
                        Row{
                            height: 38
                            spacing: 10
                            visible: !$MainViewController.isPreAnalysisRunning
                            Label {
                                id:label1
                                text: qsTr("搜索文件夹：")
                                font.pixelSize: 16
                                color: "#80FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                                width: prepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: dicomDir
                                width: prepCol.width - label1.width - 60 - 20
                                height: 38
                                inputRadius: 4
                                backgroundColor: "#14FFFFFF"
                            }
                            CustomButton{
                                width: 60
                                height: 36
                                buttonRadius: 4
                                fontSize: 14
                                text: qsTr("导入")
                                anchors.verticalCenter: parent.verticalCenter
                                onClicked: {
                                    preFileDialog.open()
                                }
                            }
                        }

                        CustomButton{
                            width: parent.width
                            height: 48
                            visible: !$MainViewController.isPreAnalysisRunning
                            text: $MainViewController.isScanning ? qsTr("搜索中...") : qsTr("搜索")
                            enabled: !$MainViewController.isScanning
                            onClicked: {
                                $MainViewController.scanFolder(dicomDir.text)
                            }
                        }

                        // 扫描进度条区域
                        Column {
                            width: parent.width
                            spacing: 8
                            visible: !$MainViewController.isPreAnalysisRunning && ($MainViewController.isScanning || $MainViewController.scanProgress > 0)

                            // 进度条背景
                            Rectangle {
                                width: parent.width
                                height: 8
                                radius: 4
                                color: "#14FFFFFF"

                                // 进度条填充
                                Rectangle {
                                    width: parent.width * $MainViewController.scanProgress
                                    height: parent.height
                                    radius: 4
                                    color: "#3C7EFF"

                                    Behavior on width {
                                        NumberAnimation {
                                            duration: 200
                                            easing.type: Easing.OutQuad
                                        }
                                    }
                                }
                            }

                            // 进度文字信息
                            Row {
                                width: parent.width
                                spacing: 16

                                Label {
                                    text: qsTr("进度：") + Math.round($MainViewController.scanProgress * 100) + "%"
                                    font.pixelSize: 12
                                    color: "#80FFFFFF"
                                }

                                Label {
                                    text: qsTr("已扫描：") + $MainViewController.scanScannedFolders + "/" + $MainViewController.scanTotalFolders
                                    font.pixelSize: 12
                                    color: "#80FFFFFF"
                                }
                            }

                            // 扫描统计信息
                            Row {
                                width: parent.width
                                spacing: 16

                                Label {
                                    text: qsTr("T1W：") + $MainViewController.scanFoundT1Count
                                    font.pixelSize: 12
                                    color: "#4CAF50"
                                }

                                Label {
                                    text: qsTr("BOLD：") + $MainViewController.scanFoundBoldCount
                                    font.pixelSize: 12
                                    color: "#FF9800"
                                }

                                Label {
                                    text: qsTr("配对成功：") + $MainViewController.scanPairedCount
                                    font.pixelSize: 12
                                    color: "#3C7EFF"
                                }
                            }

                            // 当前扫描的文件夹
                            Label {
                                width: parent.width
                                text: $MainViewController.isScanning ? qsTr("正在扫描：") + $MainViewController.scanCurrentFolder : qsTr("扫描完成")
                                font.pixelSize: 11
                                color: "#60FFFFFF"
                                elide: Text.ElideMiddle
                            }
                        }

                        // MRI 配对结果表格
                        Column {
                            width: parent.width
                            spacing: 8
                            visible: !$MainViewController.isPreAnalysisRunning && !$MainViewController.isScanning && $MriPairResultModel.resultCount > 0

                            // 表格标题和操作栏
                            Row {
                                width: parent.width
                                spacing: 12

                                Label {
                                    text: qsTr("扫描结果（") + $MriPairResultModel.resultCount + qsTr("条）")
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: "#FFFFFF"
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Item { width: 1; height: 1; }

                                // 全选按钮
                                CustomButton {
                                    width: 60
                                    height: 28
                                    buttonRadius: 4
                                    fontSize: 12
                                    text: qsTr("全选")
                                    onClicked: {
                                        $MriPairResultModel.selectAll()
                                    }
                                }

                                // 取消全选按钮
                                CustomButton {
                                    width: 70
                                    height: 28
                                    buttonRadius: 4
                                    fontSize: 12
                                    backgroundColor: "#14FFFFFF"
                                    borderColor: "#3C7EFF"
                                    borderWidth: 1
                                    text: qsTr("取消全选")
                                    onClicked: {
                                        $MriPairResultModel.deselectAll()
                                    }
                                }

                                Label {
                                    text: qsTr("已选：") + $MriPairResultModel.checkedCount
                                    font.pixelSize: 12
                                    color: "#3C7EFF"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            // 表格容器（支持横向滚动）
                            Flickable {
                                id: mriTableFlickable
                                width: parent.width
                                height: Math.min($MriPairResultModel.resultCount * 44 + 36, 306) + 12  // 36为表头高度，12为滚动条空间
                                contentWidth: mriTableContentWidth
                                contentHeight: height
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds

                                // 表格内容总宽度
                                property int mriTableContentWidth: 40 + 120 + 140 + 60 + 100 + 100  // 选择+患者ID+姓名+性别+出生日期+检查日期

                                ScrollBar.horizontal: ScrollBar {
                                    active: true
                                    policy: ScrollBar.AsNeeded
                                }

                                Column {
                                    width: mriTableFlickable.mriTableContentWidth
                                    spacing: 0

                                    // 表格头部
                                    Rectangle {
                                        width: parent.width
                                        height: 36
                                        color: "#1AFFFFFF"
                                        radius: 4

                                        Row {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            spacing: 0

                                            // 勾选列
                                            Item {
                                                width: 40
                                                height: parent.height
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: qsTr("选择")
                                                    font.pixelSize: 12
                                                    color: "#80FFFFFF"
                                                }
                                            }

                                            // 患者ID列
                                            Item {
                                                width: 120
                                                height: parent.height
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: qsTr("患者ID")
                                                    font.pixelSize: 12
                                                    color: "#80FFFFFF"
                                                }
                                            }

                                            // 姓名列
                                            Item {
                                                width: 140
                                                height: parent.height
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: qsTr("姓名")
                                                    font.pixelSize: 12
                                                    color: "#80FFFFFF"
                                                }
                                            }

                                            // 性别列
                                            Item {
                                                width: 60
                                                height: parent.height
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: qsTr("性别")
                                                    font.pixelSize: 12
                                                    color: "#80FFFFFF"
                                                }
                                            }

                                            // 出生日期列
                                            Item {
                                                width: 100
                                                height: parent.height
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: qsTr("出生日期")
                                                    font.pixelSize: 12
                                                    color: "#80FFFFFF"
                                                }
                                            }

                                            // 检查日期列
                                            Item {
                                                width: 100
                                                height: parent.height
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: qsTr("检查日期")
                                                    font.pixelSize: 12
                                                    color: "#80FFFFFF"
                                                }
                                            }
                                        }
                                    }

                                    // 表格内容区域
                                    ListView {
                                        id: mriResultListView
                                        width: parent.width
                                        height: Math.min($MriPairResultModel.resultCount * 44, 270)
                                        clip: true
                                        model: $MriPairResultModel
                                        interactive: true

                                        ScrollBar.vertical: ScrollBar {
                                            active: true
                                            policy: ScrollBar.AsNeeded
                                        }

                                        delegate: Rectangle {
                                            width: mriResultListView.width
                                            height: 44
                                            color: index % 2 === 0 ? "#0AFFFFFF" : "transparent"
                                            radius: 2

                                            // 鼠标悬停效果
                                            Rectangle {
                                                anchors.fill: parent
                                                color: "#1AFFFFFF"
                                                radius: 2
                                                visible: mriRowMouseArea.containsMouse
                                            }

                                            MouseArea {
                                                id: mriRowMouseArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    $MriPairResultModel.toggleChecked(index)
                                                }
                                            }

                                            Row {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                spacing: 0

                                                // 勾选框
                                                Item {
                                                    width: 40
                                                    height: parent.height

                                                    Rectangle {
                                                        anchors.centerIn: parent
                                                        width: 20
                                                        height: 20
                                                        radius: 4
                                                        color: model.isChecked ? "#3C7EFF" : "transparent"
                                                        border.color: model.isChecked ? "#3C7EFF" : "#60FFFFFF"
                                                        border.width: 1

                                                        // 勾选符号
                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "✓"
                                                            color: "#FFFFFF"
                                                            font.pixelSize: 14
                                                            font.bold: true
                                                            visible: model.isChecked
                                                        }

                                                        MouseArea {
                                                            anchors.fill: parent
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                $MriPairResultModel.toggleChecked(index)
                                                            }
                                                        }
                                                    }
                                                }

                                                // 患者ID
                                                Item {
                                                    width: 120
                                                    height: parent.height
                                                    Label {
                                                        anchors.centerIn: parent
                                                        width: parent.width - 8
                                                        text: model.patientId || "-"
                                                        font.pixelSize: 12
                                                        color: "#FFFFFF"
                                                        elide: Text.ElideRight
                                                        horizontalAlignment: Text.AlignHCenter
                                                    }
                                                }

                                                // 姓名
                                                Item {
                                                    width: 140
                                                    height: parent.height
                                                    Label {
                                                        anchors.centerIn: parent
                                                        width: parent.width - 8
                                                        text: model.patientName || "-"
                                                        font.pixelSize: 12
                                                        color: "#FFFFFF"
                                                        elide: Text.ElideRight
                                                        horizontalAlignment: Text.AlignHCenter
                                                    }
                                                }

                                                // 性别
                                                Item {
                                                    width: 60
                                                    height: parent.height
                                                    Label {
                                                        anchors.centerIn: parent
                                                        text: {
                                                            var sex = model.patientSex || ""
                                                            if (sex === "M") return qsTr("男")
                                                            if (sex === "F") return qsTr("女")
                                                            return sex || "-"
                                                        }
                                                        font.pixelSize: 12
                                                        color: "#FFFFFF"
                                                    }
                                                }

                                                // 出生日期
                                                Item {
                                                    width: 100
                                                    height: parent.height
                                                    Label {
                                                        anchors.centerIn: parent
                                                        text: {
                                                            var date = model.patientBirthDate || ""
                                                            if (date.length === 8) {
                                                                return date.substring(0, 4) + "-" + date.substring(4, 6) + "-" + date.substring(6, 8)
                                                            }
                                                            return date || "-"
                                                        }
                                                        font.pixelSize: 12
                                                        color: "#FFFFFF"
                                                    }
                                                }

                                                // 检查日期
                                                Item {
                                                    width: 100
                                                    height: parent.height
                                                    Label {
                                                        anchors.centerIn: parent
                                                        text: {
                                                            var date = model.studyDate || ""
                                                            if (date.length === 8) {
                                                                return date.substring(0, 4) + "-" + date.substring(4, 6) + "-" + date.substring(6, 8)
                                                            }
                                                            return date || "-"
                                                        }
                                                        font.pixelSize: 12
                                                        color: "#FFFFFF"
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Row{
                            height: 38
                            spacing: 10
                            visible: !$MainViewController.isScanning && $MriPairResultModel.checkedCount && dataStorePanel && dataStorePanel.uploadProcessingMode === 1
                            Label {
                                id:label2
                                text: qsTr("输出Bids文件夹：")
                                font.pixelSize: 16
                                color: "#80FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                                width: prepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: bidsDir
                                width: prepCol.width - label2.width - 10
                                height: 38
                                text: ""
                                inputRadius: 4
                                backgroundColor: "#14FFFFFF"
                            }
                        }
                        Row{
                            height: 38
                            spacing: 10
                            visible: !$MainViewController.isScanning && $MriPairResultModel.checkedCount && dataStorePanel && dataStorePanel.uploadProcessingMode === 1
                            Label {
                                id:label3
                                text: qsTr("输出Output文件夹：")
                                font.pixelSize: 16
                                color: "#80FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                                width: prepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: outputDir
                                width: prepCol.width - label3.width - 10
                                height: 38
                                inputRadius: 4
                                backgroundColor: "#14FFFFFF"
                            }
                        }
                        Row{
                            height: 38
                            spacing: 10
                            visible: !$MainViewController.isScanning && $MriPairResultModel.checkedCount && dataStorePanel && dataStorePanel.uploadProcessingMode === 1
                            Label {
                                id:label4
                                text: qsTr("license文件地址：")
                                font.pixelSize: 16
                                color: "#80FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                                width: prepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: licenseFile
                                width: prepCol.width - label4.width - 60 - 20
                                height: 38
                                inputRadius: 4
                                backgroundColor: "#14FFFFFF"
                            }
                            CustomButton{
                                width: 60
                                height: 36
                                buttonRadius: 4
                                fontSize: 14
                                text: qsTr("导入")
                                anchors.verticalCenter: parent.verticalCenter
                                onClicked: {
                                    licenseFileDialog.open()
                                }
                            }
                        }
                        Row {
                            height: 38
                            spacing: 10
                            visible: !$MainViewController.isScanning && $MriPairResultModel.checkedCount && $DataStorePanel.uploadProcessingMode === 1
                            Label {
                                text: qsTr("预处理方式：")
                                font.pixelSize: 16
                                color: "#80FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                                width: prepCol.maxLabelWidth
                            }
                            CustomComboBox {
                                id: methodComboBox
                                width: prepCol.width - prepCol.maxLabelWidth - 10
                                model: ["传统处理", "深度学习"]
                                
                                // 预估处理时间的函数
                                function updateEstimatedTime() {
                                    if (selectedIndices.length > 0 && $MriPairResultModel.checkedCount > 0) {
                                        var method = selectedIndices[0]  // 0: 传统处理(fmriprep), 1: 深度学习(deepprep)
                                        var subjectCount = $MriPairResultModel.checkedCount
                                        var estimatedTimeStr = $MainViewController.estimateProcessingTime(method, subjectCount)
                                        estimatedTimeText.text = qsTr("预估处理时间：") + estimatedTimeStr
                                        estimatedTimeText.visible = true
                                    } else {
                                        estimatedTimeText.visible = false
                                    }
                                }
                                
                                onSelectionChanged: {
                                    // 选择预处理方式时，预估处理时间
                                    updateEstimatedTime()
                                }
                                
                                // 监听勾选数量变化
                                Connections {
                                    target: $MriPairResultModel
                                    function onCheckedCountChanged() {
                                        // 当已选择预处理方式时，勾选数量变化也更新预估时间
                                        methodComboBox.updateEstimatedTime()
                                    }
                                }
                            }
                        }
                        Label {
                            id: estimatedTimeText
                            visible: false
                            font.pixelSize: 14
                            color: "#80FFFFFF"
                            horizontalAlignment: Text.AlignLeft
                        }
                        CustomButton{
                            visible: !$MainViewController.isScanning && ($MriPairResultModel.checkedCount || $MainViewController.isPreAnalysisRunning)
                            width: parent.width
                            height: 48
                            text: $MainViewController.isPreAnalysisRunning ? qsTr("取消预处理") : qsTr("分析")
                            backgroundColor: $MainViewController.isPreAnalysisRunning ? "#E74C3C" : "#3C7EFF"
                            onClicked: {
                                if ($MainViewController.isPreAnalysisRunning) {
                                    $MainViewController.stopFmriprepProcess()
                                    $MainViewController.stopDeepprepProcess()
                                    return
                                }

                                function warn(msg) {
                                    if (messageManager) {
                                        messageManager.warning(msg, 2000)
                                    } else {
                                        console.log(msg)
                                    }
                                }

                                if (dataStorePanel && dataStorePanel.uploadProcessingMode === 0) {
                                    // 仅脑龄预测：只需去脸→脑零，不需要 bids/output/license
                                    $MainViewController.startBrainAgeOnly()
                                } else {
                                    // 完整流程：去脸→脑龄 + deepprep/fmriprep
                                    var b = bidsDir.text.trim()
                                    var o = outputDir.text.trim()
                                    var l = licenseFile.text.trim()
                                    if (b === "") {
                                        warn(qsTr("请输入 Bids 文件夹路径"))
                                        return
                                    }
                                    if (o === "") {
                                        warn(qsTr("请输入 Output 文件夹路径"))
                                        return
                                    }
                                    if (l === "") {
                                        warn(qsTr("请输入 license 文件路径"))
                                        return
                                    }
                                    if (methodComboBox.selectedIndices.length === 0) {
                                        warn(qsTr("请选择预处理方式"))
                                        return
                                    }
                                    $MainViewController.startPreAnalysis(methodComboBox.selectedIndices[0], b, o, l)
                                }
                            }
                        }

                        // 预分析日志展示区域
                        Rectangle {
                            width: parent.width
                            height: preAnalysis.height - tabContainer.height - 16 - 38 * 4 - 48 - 5 * 12
                            color: "#E016171B"
                            radius: 8
                            visible: $MainViewController.isPreAnalysisRunning

                            // 日志更新Timer，延迟更新避免阻塞动画
                            Timer {
                                id: preAnalysisLogUpdateTimer
                                interval: 150
                                repeat: false
                                onTriggered: {
                                    preAnalysisLogArea.text = $MainViewController.preAnalysisLog
                                }
                            }

                            // 滚动延迟Timer
                            Timer {
                                id: preAnalysisScrollTimer
                                interval: 50
                                repeat: false
                                onTriggered: {
                                    preAnalysisLogArea.cursorPosition = preAnalysisLogArea.length
                                    if (preAnalysisLogFlickable.contentHeight > preAnalysisLogFlickable.height) {
                                        preAnalysisLogFlickable.contentY = preAnalysisLogFlickable.contentHeight - preAnalysisLogFlickable.height
                                    }
                                }
                            }

                            Connections {
                                target: $MainViewController
                                function onPreAnalysisLogUpdated() {
                                    preAnalysisLogUpdateTimer.restart()
                                }
                            }

                            Column {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                Row {
                                    width: parent.width
                                    spacing: 8

                                    Label {
                                        text: qsTr("预处理日志")
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: "#FFFFFF"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Item {width: 1; height: 1 }

                                    // 运行状态指示器
                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: $MainViewController.isPreAnalysisRunning ? "#4CAF50" : "#80FFFFFF"
                                        anchors.verticalCenter: parent.verticalCenter

                                        SequentialAnimation on opacity {
                                            running: $MainViewController.isPreAnalysisRunning
                                            loops: Animation.Infinite
                                            NumberAnimation { to: 0.3; duration: 500 }
                                            NumberAnimation { to: 1.0; duration: 500 }
                                        }
                                    }

                                    Label {
                                        text: $MainViewController.isPreAnalysisRunning ? qsTr("运行中...") : qsTr("已完成")
                                        font.pixelSize: 12
                                        color: $MainViewController.isPreAnalysisRunning ? "#4CAF50" : "#80FFFFFF"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                Flickable {
                                    id: preAnalysisLogFlickable
                                    width: parent.width
                                    height: parent.height - 30
                                    contentWidth: width
                                    contentHeight: preAnalysisLogArea.contentHeight
                                    clip: true
                                    boundsBehavior: Flickable.StopAtBounds

                                    ScrollBar.vertical: ScrollBar {
                                        active: true
                                        policy: ScrollBar.AsNeeded
                                    }

                                    TextArea {
                                        id: preAnalysisLogArea
                                        width: parent.width
                                        readOnly: true
                                        wrapMode: TextArea.Wrap
                                        font.pixelSize: 14
                                        color: "#E5FFFFFF"
                                        background: null
                                        selectByMouse: true

                                        Component.onCompleted: {
                                            text = $MainViewController.preAnalysisLog
                                        }

                                        onTextChanged: {
                                            preAnalysisScrollTimer.restart()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: preAnalysis.height - 16 - 48
                    radius: 8
                    color: "#141A24"
                    border.color: "#2A3348"
                    border.width: 1
                    visible: false

                    Column {
                        anchors.centerIn: parent
                        spacing: 12

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("预处理任务入口已迁移到数据上传窗口")
                            color: "#E5FFFFFF"
                            font.pixelSize: 18
                            font.family: "Alibaba PuHuiTi 3.0"
                            font.bold: true
                        }

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("请在主界面打开数据上传，完成硬件扫描、选择算法并提交任务。")
                            color: "#80FFFFFF"
                            font.pixelSize: 14
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                    }
                }
                Column{
                    id: preDetailCol
                    width: parent.width
                    spacing: 12
                    visible: true
                    Row{
                        height: 38
                        spacing: 10
                        visible: false
                        Label {
                            id: label5
                            text: qsTr("预处理后文件夹：")
                            color: "#80FFFFFF"
                            font.pixelSize: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        SingleLineTextInput{
                            id: outputDetailDir
                            width: preDetailCol.width - label5.width - 60 - 20
                            height: 38
                            inputRadius: 4
                            backgroundColor: "#14FFFFFF"
                        }
                        CustomButton{
                            width: 60
                            height: 36
                            buttonRadius: 4
                            fontSize: 14
                            text: qsTr("导入")
                            anchors.verticalCenter: parent.verticalCenter
                            onClicked: {
                                outputDetailDialog.open()
                            }
                        }
                    }
                    Row{
                        height: 38
                        spacing: 10
                        visible: false
                        Label {
                            id: label6
                            text: qsTr("数据列表：")
                            color: "#80FFFFFF"
                            font.pixelSize: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        CustomComboBox {
                            id: patientComboBox
                            width: preDetailCol.width - label6.width - 10
                            model: []
                            onSelectionChanged: {
                                // 通过索引从 subjectsMetadata 获取对应的 subjectId
                                if (selectedIndices.length > 0)
                                    selectSubjectAtIndex(selectedIndices[0])
                            }
                        }
                    }
                    Rectangle{
                        width: parent.width
                        color: "transparent"
                        height: 8
                    }
                    Label {
                        text: qsTr("结构相")
                        font.pixelSize: 18
                        color: "#ffffff"
                    }
                    Rectangle{
                        id: segmentationBtn
                        width: btnText0.width + 40
                        height: 36
                        radius: 24
                        border.width: 1
                        border.color: preShowResultIndex === 0 ? "#3C7EFF" : "#4DFFFFFF"
                        property bool isHovered: false
                        property bool isSelected: preShowResultIndex === 0
                        
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { 
                                position: 1.0
                                color: segmentationBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.04)
                            }
                            GradientStop { 
                                position: 0.49
                                color: segmentationBtn.isSelected ? Qt.rgba(1/255, 34/255, 109/255, 0.8) : Qt.rgba(1, 1, 1, 0.15)
                            }
                            GradientStop { 
                                position: 0.0
                                color: segmentationBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.4)
                            }
                        }
                        
                        opacity: segmentationBtn.isHovered ? 0.8 : 1.0
                        
                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                        
                        Text {
                            id: btnText0
                            anchors.centerIn: parent
                            text: qsTr("分割")
                            color: segmentationBtn.isSelected ? "#FFFFFF" : "#B2FFFFFF"
                            font.pixelSize: 14
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                        
                        MouseArea {
                            id: segBtnMouseArea
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: segmentationBtn.isHovered = true
                            onExited: segmentationBtn.isHovered = false
                            
                            onClicked: {
                                if(outputDetailDir.text === "" || currentSubjectId === ""){
                                    return
                                }
                                preShowResultIndex = 0
                                var basePath = isDeepprepOutput ? "/QC/" + currentSubjectId + "/figures/" : "/" + currentSubjectId + "/figures/"
                                var fileName = isDeepprepOutput ? currentSubjectId + "_desc-volparc_T1w.svg" : currentSubjectId + "_dseg.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Rectangle{
                        id: regBtn
                        width: btnText1.width + 40
                        height: 36
                        radius: 24
                        border.width: 1
                        border.color: preShowResultIndex === 1 ? "#3C7EFF" : "#4DFFFFFF"
                        property bool isHovered: false
                        property bool isSelected: preShowResultIndex === 1
                        
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { 
                                position: 1.0
                                color: regBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.04)
                            }
                            GradientStop { 
                                position: 0.49
                                color: regBtn.isSelected ? Qt.rgba(1/255, 34/255, 109/255, 0.8) : Qt.rgba(1, 1, 1, 0.15)
                            }
                            GradientStop { 
                                position: 0.0
                                color: regBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.4)
                            }
                        }
                        
                        opacity: regBtn.isHovered ? 0.8 : 1.0
                        
                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                        
                        Text {
                            id: btnText1
                            anchors.centerIn: parent
                            text: qsTr("配准")
                            color: regBtn.isSelected ? "#FFFFFF" : "#B2FFFFFF"
                            font.pixelSize: 14
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: regBtn.isHovered = true
                            onExited: regBtn.isHovered = false
                            
                            onClicked: {
                                if(outputDetailDir.text === "" || currentSubjectId === ""){
                                    return
                                }
                                preShowResultIndex = 1
                                var basePath = isDeepprepOutput ? "/QC/" + currentSubjectId + "/figures/" : "/" + currentSubjectId + "/figures/"
                                var fileName = isDeepprepOutput ? currentSubjectId + "_task-rest_desc-coreg_bold.svg" : currentSubjectId + "_space-MNI152NLin2009cAsym_T1w.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Rectangle{
                        width: parent.width
                        color: "transparent"
                        height: 8
                    }
                    Label {
                        text: qsTr("标准空间")
                        font.pixelSize: 18
                        color: "#ffffff"
                    }
                    Rectangle{
                        id: mniBtn
                        width: btnText2.width + 40
                        height: 36
                        radius: 24
                        border.width: 1
                        border.color: preShowResultIndex === 2 ? "#3C7EFF" : "#4DFFFFFF"
                        property bool isHovered: false
                        property bool isSelected: preShowResultIndex === 2
                        
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { 
                                position: 1.0
                                color: mniBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.04)
                            }
                            GradientStop { 
                                position: 0.49
                                color: mniBtn.isSelected ? Qt.rgba(1/255, 34/255, 109/255, 0.8) : Qt.rgba(1, 1, 1, 0.15)
                            }
                            GradientStop { 
                                position: 0.0
                                color: mniBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.4)
                            }
                        }
                        
                        opacity: mniBtn.isHovered ? 0.8 : 1.0
                        
                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                        
                        Text {
                            id: btnText2
                            anchors.centerIn: parent
                            text: qsTr("MN152NLin2009cAsym")
                            color: mniBtn.isSelected ? "#FFFFFF" : "#B2FFFFFF"
                            font.pixelSize: 14
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: mniBtn.isHovered = true
                            onExited: mniBtn.isHovered = false
                            
                            onClicked: {
                                if(outputDetailDir.text === "" || currentSubjectId === ""){
                                    return
                                }
                                preShowResultIndex = 2
                                var basePath = isDeepprepOutput ? "/QC/" + currentSubjectId + "/figures/" : "/" + currentSubjectId + "/figures/"
                                var fileName = isDeepprepOutput ? currentSubjectId + "_desc-T1toMNI152_combine.svg" : currentSubjectId + "_space-MNI152NLin2009cAsym_T1w.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Rectangle{
                        width: parent.width
                        color: "transparent"
                        height: 8
                    }
                    Label {
                        text: qsTr("功能相")
                        font.pixelSize: 18
                        color: "#ffffff"
                    }
                    Rectangle{
                        id: t1FunBtn
                        width: btnText3.width + 40
                        height: 36
                        radius: 24
                        border.width: 1
                        border.color: preShowResultIndex === 3 ? "#3C7EFF" : "#4DFFFFFF"
                        property bool isHovered: false
                        property bool isSelected: preShowResultIndex === 3
                        
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { 
                                position: 1.0
                                color: t1FunBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.04)
                            }
                            GradientStop { 
                                position: 0.49
                                color: t1FunBtn.isSelected ? Qt.rgba(1/255, 34/255, 109/255, 0.8) : Qt.rgba(1, 1, 1, 0.15)
                            }
                            GradientStop { 
                                position: 0.0
                                color: t1FunBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.4)
                            }
                        }
                        
                        opacity: t1FunBtn.isHovered ? 0.8 : 1.0
                        
                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                        
                        Text {
                            id: btnText3
                            anchors.centerIn: parent
                            text: qsTr("T1 to Fun")
                            color: t1FunBtn.isSelected ? "#FFFFFF" : "#B2FFFFFF"
                            font.pixelSize: 14
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: t1FunBtn.isHovered = true
                            onExited: t1FunBtn.isHovered = false
                            
                            onClicked: {
                                if(outputDetailDir.text === "" || currentSubjectId === ""){
                                    return
                                }
                                preShowResultIndex = 3
                                var basePath = isDeepprepOutput ? "/QC/" + currentSubjectId + "/figures/" : "/" + currentSubjectId + "/figures/"
                                var fileName = currentSubjectId + "_task-rest_desc-coreg_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Rectangle{
                        id: boldBtn
                        width: btnText4.width + 40
                        height: 36
                        radius: 24
                        border.width: 1
                        border.color: preShowResultIndex === 4 ? "#3C7EFF" : "#4DFFFFFF"
                        property bool isHovered: false
                        property bool isSelected: preShowResultIndex === 4
                        
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { 
                                position: 1.0
                                color: boldBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.04)
                            }
                            GradientStop { 
                                position: 0.49
                                color: boldBtn.isSelected ? Qt.rgba(1/255, 34/255, 109/255, 0.8) : Qt.rgba(1, 1, 1, 0.15)
                            }
                            GradientStop { 
                                position: 0.0
                                color: boldBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.4)
                            }
                        }
                        
                        opacity: boldBtn.isHovered ? 0.8 : 1.0
                        
                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                        
                        Text {
                            id: btnText4
                            anchors.centerIn: parent
                            text: qsTr("BOLD总结")
                            color: boldBtn.isSelected ? "#FFFFFF" : "#B2FFFFFF"
                            font.pixelSize: 14
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: boldBtn.isHovered = true
                            onExited: boldBtn.isHovered = false
                            
                            onClicked: {
                                if(outputDetailDir.text === "" || currentSubjectId === ""){
                                    return
                                }
                                preShowResultIndex = 4
                                var basePath = isDeepprepOutput ? "/QC/" + currentSubjectId + "/figures/" : "/" + currentSubjectId + "/figures/"
                                var fileName = isDeepprepOutput ? currentSubjectId + "_task-rest_desc-carpet_bold.svg" : currentSubjectId + "_task-rest_desc-carpetplot_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Rectangle{
                        width: parent.width
                        color: "transparent"
                        height: 8
                    }
                    Label {
                        text: isDeepprepOutput ? qsTr("DeepPrep输出") : qsTr("质量控制")
                        font.pixelSize: 18
                        color: "#ffffff"
                    }
                    Rectangle{
                        id: corticalBtn
                        width: btnText5.width + 40
                        height: 36
                        radius: 24
                        border.width: 1
                        border.color: preShowResultIndex === 5 ? "#3C7EFF" : "#4DFFFFFF"
                        property bool isHovered: false
                        property bool isSelected: preShowResultIndex === 5
                        
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { 
                                position: 1.0
                                color: corticalBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.04)
                            }
                            GradientStop { 
                                position: 0.49
                                color: corticalBtn.isSelected ? Qt.rgba(1/255, 34/255, 109/255, 0.8) : Qt.rgba(1, 1, 1, 0.15)
                            }
                            GradientStop { 
                                position: 0.0
                                color: corticalBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.4)
                            }
                        }
                        
                        opacity: corticalBtn.isHovered ? 0.8 : 1.0
                        
                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                        
                        Text {
                            id: btnText5
                            anchors.centerIn: parent
                            text: isDeepprepOutput ? qsTr("皮层表面") : qsTr("CompCor脑区")
                            color: corticalBtn.isSelected ? "#FFFFFF" : "#B2FFFFFF"
                            font.pixelSize: 14
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: corticalBtn.isHovered = true
                            onExited: corticalBtn.isHovered = false
                            
                            onClicked: {
                                if(outputDetailDir.text === "" || currentSubjectId === ""){
                                    return
                                }
                                preShowResultIndex = 5
                                var basePath = isDeepprepOutput ? "/QC/" + currentSubjectId + "/figures/" : "/" + currentSubjectId + "/figures/"
                                var fileName = isDeepprepOutput ? currentSubjectId + "_desc-surfparc_T1w.svg" : currentSubjectId + "_task-rest_desc-rois_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Rectangle{
                        id: tsnrBtn
                        width: btnText6.width + 40
                        height: 36
                        radius: 24
                        border.width: 1
                        border.color: preShowResultIndex === 6 ? "#3C7EFF" : "#4DFFFFFF"
                        property bool isHovered: false
                        property bool isSelected: preShowResultIndex === 6
                        
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { 
                                position: 1.0
                                color: tsnrBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.04)
                            }
                            GradientStop { 
                                position: 0.49
                                color: tsnrBtn.isSelected ? Qt.rgba(1/255, 34/255, 109/255, 0.8) : Qt.rgba(1, 1, 1, 0.15)
                            }
                            GradientStop { 
                                position: 0.0
                                color: tsnrBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.4)
                            }
                        }
                        
                        opacity: tsnrBtn.isHovered ? 0.8 : 1.0
                        
                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                        
                        Text {
                            id: btnText6
                            anchors.centerIn: parent
                            text: isDeepprepOutput ? qsTr("时间信噪比") : qsTr("方差")
                            color: tsnrBtn.isSelected ? "#FFFFFF" : "#B2FFFFFF"
                            font.pixelSize: 14
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: tsnrBtn.isHovered = true
                            onExited: tsnrBtn.isHovered = false
                            
                            onClicked: {
                                if(outputDetailDir.text === "" || currentSubjectId === ""){
                                    return
                                }
                                preShowResultIndex = 6
                                var basePath = isDeepprepOutput ? "/QC/" + currentSubjectId + "/figures/" : "/" + currentSubjectId + "/figures/"
                                var fileName = isDeepprepOutput ? currentSubjectId + "_task-rest_bold_desc-tsnr_bold.svg" : currentSubjectId + "_task-rest_desc-compcorvar_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Rectangle{
                        id: surfaceBtn
                        width: btnText7.width + 40
                        height: 36
                        radius: 24
                        border.width: 1
                        border.color: preShowResultIndex === 7 ? "#3C7EFF" : "#4DFFFFFF"
                        property bool isHovered: false
                        property bool isSelected: preShowResultIndex === 7
                        
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { 
                                position: 1.0
                                color: surfaceBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.04)
                            }
                            GradientStop { 
                                position: 0.49
                                color: surfaceBtn.isSelected ? Qt.rgba(1/255, 34/255, 109/255, 0.8) : Qt.rgba(1, 1, 1, 0.15)
                            }
                            GradientStop { 
                                position: 0.0
                                color: surfaceBtn.isSelected ? "#223D7C" : Qt.rgba(1, 1, 1, 0.4)
                            }
                        }
                        
                        opacity: surfaceBtn.isHovered ? 0.8 : 1.0
                        
                        Behavior on opacity {
                            NumberAnimation { duration: 200 }
                        }
                        
                        Text {
                            id: btnText7
                            anchors.centerIn: parent
                            text: isDeepprepOutput ? qsTr("表面重建") : qsTr("干扰回归变量相关性")
                            color: surfaceBtn.isSelected ? "#FFFFFF" : "#B2FFFFFF"
                            font.pixelSize: 14
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            
                            onEntered: surfaceBtn.isHovered = true
                            onExited: surfaceBtn.isHovered = false
                            
                            onClicked: {
                                if(outputDetailDir.text === "" || currentSubjectId === ""){
                                    return
                                }
                                preShowResultIndex = 7
                                var basePath = isDeepprepOutput ? "/QC/" + currentSubjectId + "/figures/" : "/" + currentSubjectId + "/figures/"
                                var fileName = isDeepprepOutput ? currentSubjectId + "_desc-volsurf_T1w.svg" : currentSubjectId + "_task-rest_desc-confoundcorr_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                }
            }
        }
        Rectangle{
            id: segmentationAnalysis
            anchors.fill: parent
            anchors.rightMargin: 16
            anchors.leftMargin: 16
            anchors.topMargin: 22
            anchors.bottomMargin: 22
            color: "transparent"
            visible: currentIndex === 2
            clip: true
            Column{
                width: parent.width
                spacing: 24
                Label{
                    id: segLabel
                    text: qsTr("使用预处理路径：") + outputDetailDir.text
                    color: "#ffffff"
                    font.weight: Font.Medium
                    font.pixelSize: 16
                    wrapMode: Text.WrapAnywhere
                }
                Row{
                    height: 20
                    Label{
                        id: originLabel
                        text: qsTr("原始图像：")
                        color: "#ffffff"
                        font.weight: Font.Medium
                        font.pixelSize: 16
                        wrapMode: Text.WrapAnywhere
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Image{
                        source: $DicomDataModel.showOriginal ? "qrc:/image/eye.png" : "qrc:/image/eyeSlash.png"
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                $DicomDataModel.showOriginal = !$DicomDataModel.showOriginal
                                $DicomDataModel.setSegDisplayMode($DicomDataModel.showOriginal ? 0 : 2)
                            }
                        }
                    }
                }
                Rectangle {
                    width: parent.width
                    height: segmentationAnalysis.height - segLabel.height - 48 - 20
                    color: "#14FFFFFF"

                    Column {
                        id: segTableColumn
                        anchors.fill: parent
                        // 定义列宽常量
                        readonly property int colVisibleWidth: 50
                        readonly property int colColorWidth: 50
                        readonly property int colChineseWidth: 110
                        readonly property int colHemisphereWidth: 60
                        readonly property int colVolumeWidth: 110
                        readonly property int colPercentWidth: 95
                        readonly property int colAsymmetryWidth: 85
                        readonly property int totalContentWidth: colVisibleWidth + colColorWidth + colChineseWidth + colHemisphereWidth + colVolumeWidth + colPercentWidth + colAsymmetryWidth

                        // 横向滚动的表头容器
                        Item {
                            width: parent.width - 12 // 留出纵向滚动条的空间
                            height: 38
                            clip: true

                            Flickable {
                                id: headerFlickable
                                anchors.fill: parent
                                contentWidth: segTableColumn.totalContentWidth
                                contentHeight: height
                                interactive: false  // 表头不直接交互，跟随内容滚动
                                clip: true

                                Rectangle {
                                    id: segTableHeader
                                    width: segTableColumn.totalContentWidth
                                    height: 38
                                    color: "#14FFFFFF"

                                    Row {
                                        width: parent.width
                                        height: parent.height

                                        // 显示列
                                        Rectangle {
                                            width: segTableColumn.colVisibleWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: "显示"
                                                color: "#E5FFFFFF"
                                                font.pixelSize: 16
                                                font.bold: true
                                            }
                                        }

                                        // 颜色列
                                        Rectangle {
                                            width: segTableColumn.colColorWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: "颜色"
                                                color: "#E5FFFFFF"
                                                font.pixelSize: 16
                                                font.bold: true
                                            }
                                        }

                                        // 中文名称
                                        Rectangle {
                                            width: segTableColumn.colChineseWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: "中文名称"
                                                color: "#E5FFFFFF"
                                                font.pixelSize: 16
                                                font.bold: true
                                            }
                                        }

                                        // 位置
                                        Rectangle {
                                            width: segTableColumn.colHemisphereWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: "位置"
                                                color: "#E5FFFFFF"
                                                font.pixelSize: 16
                                                font.bold: true
                                            }
                                        }

                                        // 容积
                                        Rectangle {
                                            width: segTableColumn.colVolumeWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: "容积(cm³)"
                                                color: "#E5FFFFFF"
                                                font.pixelSize: 16
                                                font.bold: true
                                            }
                                        }

                                        // 全脑占比
                                        Rectangle {
                                            width: segTableColumn.colPercentWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: "全脑占比"
                                                color: "#E5FFFFFF"
                                                font.pixelSize: 16
                                                font.bold: true
                                            }
                                        }

                                        // 不对称指数
                                        Rectangle {
                                            width: segTableColumn.colAsymmetryWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: "不对称"
                                                color: "#E5FFFFFF"
                                                font.pixelSize: 16
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 表格内容（支持横向滚动）
                        Item {
                            width: parent.width
                            height: parent.height - 38

                            Flickable {
                                id: contentFlickable
                                anchors.fill: parent
                                anchors.rightMargin: 12  // 为纵向滚动条留空间
                                contentWidth: segTableColumn.totalContentWidth
                                contentHeight: segmentationTableView.contentHeight
                                clip: true

                                // 同步表头的横向滚动
                                onContentXChanged: {
                                    headerFlickable.contentX = contentX
                                }

                                // 横向滚动条
                                ScrollBar.horizontal: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                    background: Rectangle {
                                        color: "#99232324"
                                    }
                                    contentItem: Rectangle {
                                        implicitHeight: 8
                                        radius: 4
                                        color: "#484849"
                                    }
                                }

                                ListView {
                                    id: segmentationTableView
                                    width: segTableColumn.totalContentWidth
                                    height: parent.height - 12
                                    anchors.bottomMargin: 12
                                    interactive: false  // 禁用ListView自身的交互，使用Flickable的交互
                                    clip: true
                                    model: $BrainSegmentationTableModel

                                    delegate: Rectangle {
                                        width: segTableColumn.totalContentWidth
                                        height: 39
                                        color: "#99232324"

                                        Row {
                                            width: parent.width
                                            height: parent.height

                                            // 可见性眼睛图标
                                            Rectangle {
                                                width: segTableColumn.colVisibleWidth
                                                height: parent.height
                                                color: "transparent"

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        $DicomDataModel.setRegionVisible(index, !model.visible)
                                                    }
                                                    Image{
                                                        anchors.centerIn: parent
                                                        source: model.visible ? "qrc:/image/eye.png" : "qrc:/image/eyeSlash.png"
                                                        width: 16
                                                        height: 16
                                                    }
                                                }
                                            }

                                            // 颜色小方块
                                            Rectangle {
                                                width: segTableColumn.colColorWidth
                                                height: parent.height
                                                color: "transparent"

                                                Rectangle {
                                                    anchors.centerIn: parent
                                                    width: 24
                                                    height: 24
                                                    color: model.regionColor
                                                    border.color: "#ffffff"
                                                    border.width: 1
                                                    radius: 2
                                                }
                                            }

                                            // 中文名称
                                            Rectangle {
                                                width: segTableColumn.colChineseWidth
                                                height: parent.height
                                                color: "transparent"
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: model.chineseName
                                                    color: "#B2FFFFFF"
                                                    font.pixelSize: 16
                                                    elide: Text.ElideRight
                                                    width: parent.width - 4
                                                    horizontalAlignment: Text.AlignHCenter
                                                }
                                            }

                                            // 位置
                                            Rectangle {
                                                width: segTableColumn.colHemisphereWidth
                                                height: parent.height
                                                color: "transparent"
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: model.hemisphere
                                                    color: "#B2FFFFFF"
                                                    font.pixelSize: 16
                                                }
                                            }

                                            // 容积
                                            Rectangle {
                                                width: segTableColumn.colVolumeWidth
                                                height: parent.height
                                                color: "transparent"
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: model.volume
                                                    color: "#B2FFFFFF"
                                                    font.pixelSize: 16
                                                }
                                            }

                                            // 全脑占比
                                            Rectangle {
                                                width: segTableColumn.colPercentWidth
                                                height: parent.height
                                                color: "transparent"
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: model.volumePercent
                                                    color: "#B2FFFFFF"
                                                    font.pixelSize: 16
                                                }
                                            }

                                            // 不对称指数
                                            Rectangle {
                                                width: segTableColumn.colAsymmetryWidth
                                                height: parent.height
                                                color: "transparent"
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: model.asymmetryIndex
                                                    color: "#B2FFFFFF"
                                                    font.pixelSize: 16
                                                }
                                            }
                                        }

                                        // 分割线
                                        Rectangle {
                                            width: parent.width
                                            height: 1
                                            color: "#484849"
                                            anchors.bottom: parent.bottom
                                        }
                                    }
                                }
                            }

                            // 固定的纵向滚动条
                            ScrollBar {
                                id: verticalScrollBar
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: contentFlickable.bottom
                                orientation: Qt.Vertical
                                policy: ScrollBar.AlwaysOn
                                size: contentFlickable.height / segmentationTableView.contentHeight
                                position: contentFlickable.contentY / segmentationTableView.contentHeight

                                onPositionChanged: {
                                    if (pressed) {
                                        contentFlickable.contentY = position * segmentationTableView.contentHeight
                                    }
                                }

                                background: Rectangle {
                                    color: "#99232324"
                                }
                                contentItem: Rectangle {
                                    implicitWidth: 8
                                    radius: 4
                                    color: "#484849"
                                }
                            }
                        }
                    }
                }
            }
        }
        Rectangle{
            id: networkAnalysis
            anchors.fill: parent
            anchors.rightMargin: 16
            anchors.leftMargin: 16
            anchors.topMargin: 22
            anchors.bottomMargin: 22
            color: "transparent"
            visible: currentIndex === 3
            clip: true
            Column {
                width: parent.width
                spacing: 24
                Label{
                    id: networkLabel
                    text: qsTr("使用预处理路径：") + outputDetailDir.text
                    color: "#ffffff"
                    font.weight: Font.Medium
                    font.pixelSize: 16
                    wrapMode: Text.WrapAnywhere
                }
                Column {
                    height: networkAnalysis.height - 24 - networkLabel.height
                    width: parent.width
                    spacing: 16
                    Rectangle{
                        width: parent.width
                        height: infoList.height
                        color: "#E016171B"
                        radius: 8
                        Column{
                            id:infoList
                            width: parent.width
                            spacing: 12
                            leftPadding: 12
                            rightPadding: 12
                            topPadding: 16
                            bottomPadding: 24
                            Row{
                                height: 38
                                spacing: 20
                                Label{
                                    text: qsTr("全局效率：")
                                    color: "#80FFFFFF"
                                    font.pixelSize: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 140
                                }
                                Rectangle{
                                    width: infoList.width - 24 - 140 - 20
                                    height: parent.height
                                    radius: 4
                                    color: "#14FFFFFF"
                                    Label{
                                        text: qsTr($MainViewController.globalEfficiency.toString())
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 16
                                        leftPadding: 12
                                        rightPadding: 12
                                    }
                                }
                            }
                            Row{
                                height: 38
                                spacing: 20
                                Label{
                                    text: qsTr("平均局部效率：")
                                    color: "#80FFFFFF"
                                    font.pixelSize: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 140
                                }
                                Rectangle{
                                    width: infoList.width - 24 - 140 - 20
                                    height: parent.height
                                    radius: 4
                                    color: "#14FFFFFF"
                                    Label{
                                        text: qsTr($MainViewController.averageLocalEfficiency.toString())
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 16
                                        leftPadding: 12
                                        rightPadding: 12
                                    }
                                }
                            }
                            Row{
                                height: 38
                                spacing: 20
                                Label{
                                    text: qsTr("平均聚类系数：")
                                    color: "#80FFFFFF"
                                    font.pixelSize: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 140
                                }
                                Rectangle{
                                    width: infoList.width - 24 - 140 - 20
                                    height: parent.height
                                    radius: 4
                                    color: "#14FFFFFF"
                                    Label{
                                        text: qsTr($MainViewController.averageClusteringCoefficient.toString())
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 16
                                        leftPadding: 12
                                        rightPadding: 12
                                    }
                                }
                            }
                            Row{
                                height: 32
                                spacing: 6
                                Image{
                                    source: "qrc:/image/richClub.png"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Label{
                                    text: qsTr("富俱乐部系数")
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: "#E5FFFFFF"
                                    font.pixelSize: 18
                                    font.weight: Font.Medium
                                }
                            }
                            Row{
                                height: 38
                                spacing: 20
                                Label{
                                    text: qsTr("富俱乐部连接：")
                                    color: "#80FFFFFF"
                                    font.pixelSize: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 140
                                }
                                Rectangle{
                                    width: infoList.width - 24 - 140 - 20
                                    height: parent.height
                                    radius: 4
                                    color: "#14FFFFFF"
                                    Label{
                                        text: qsTr($MainViewController.richClubConnections.toString())
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 16
                                        leftPadding: 12
                                        rightPadding: 12
                                    }
                                }
                            }
                            Row{
                                height: 38
                                spacing: 20
                                Label{
                                    text: qsTr("桥接连接：")
                                    color: "#80FFFFFF"
                                    font.pixelSize: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 140
                                }
                                Rectangle{
                                    width: infoList.width - 24 - 140 - 20
                                    height: parent.height
                                    radius: 4
                                    color: "#14FFFFFF"
                                    Label{
                                        text: qsTr($MainViewController.bridgeConnections.toString())
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 16
                                        leftPadding: 12
                                        rightPadding: 12
                                    }
                                }
                            }
                            Row{
                                height: 38
                                spacing: 20
                                Label{
                                    text: qsTr("局部连接：")
                                    color: "#80FFFFFF"
                                    font.pixelSize: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 140
                                }
                                Rectangle{
                                    width: infoList.width - 24 - 140 - 20
                                    height: parent.height
                                    radius: 4
                                    color: "#14FFFFFF"
                                    Label{
                                        text: qsTr($MainViewController.localConnections.toString())
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#E5FFFFFF"
                                        font.pixelSize: 16
                                        leftPadding: 12
                                        rightPadding: 12
                                    }
                                }
                            }
                        }
                    }

                    // 脑区数据表格
                    Rectangle {
                        width: parent.width
                        height: parent.height - 16 - infoList.height
                        color: "#99232324"

                        Column {
                            id: tableColumn
                            anchors.fill: parent

                            // 定义列宽常量
                            readonly property int colIndexWidth: 40
                            readonly property int colChineseWidth: 120
                            readonly property int colEnglishWidth: 130
                            readonly property int colDegreeWidth: 40
                            readonly property int colClusteringWidth: 50

                            // 表格标题
                            Rectangle {
                                id: tableHeader
                                width: parent.width
                                height: 38
                                color: "#14FFFFFF"

                                Row {
                                    anchors.fill: parent

                                    // 序号列
                                    Rectangle {
                                        width: tableColumn.colIndexWidth
                                        height: parent.height
                                        color: "transparent"
                                        Label {
                                            anchors.centerIn: parent
                                            text: "#"
                                            color: "#E5FFFFFF"
                                            font.pixelSize: 16
                                            font.bold: true
                                        }
                                    }

                                    // 中文名称
                                    Rectangle {
                                        width: tableColumn.colChineseWidth
                                        height: parent.height
                                        color: "transparent"
                                        Label {
                                            anchors.centerIn: parent
                                            text: "中文名称"
                                            color: "#E5FFFFFF"
                                            font.pixelSize: 16
                                            font.bold: true
                                        }
                                    }

                                    // Mricro命名
                                    Rectangle {
                                        width: tableColumn.colEnglishWidth
                                        height: parent.height
                                        color: "transparent"
                                        Label {
                                            anchors.centerIn: parent
                                            text: "Mricro命名"
                                            color: "#E5FFFFFF"
                                            font.pixelSize: 16
                                            font.bold: true
                                        }
                                    }

                                    // 度
                                    Rectangle {
                                        width: tableColumn.colDegreeWidth
                                        height: parent.height
                                        color: "transparent"
                                        Label {
                                            anchors.centerIn: parent
                                            text: "度"
                                            color: "#E5FFFFFF"
                                            font.pixelSize: 16
                                            font.bold: true
                                        }
                                    }

                                    // 聚类
                                    Rectangle {
                                        width: tableColumn.colClusteringWidth
                                        height: parent.height
                                        color: "transparent"
                                        Label {
                                            anchors.centerIn: parent
                                            text: "聚类"
                                            color: "#E5FFFFFF"
                                            font.pixelSize: 16
                                            font.bold: true
                                        }
                                    }

                                    // 局部效率
                                    Rectangle {
                                        width: tableColumn.width - tableColumn.colIndexWidth - tableColumn.colChineseWidth - tableColumn.colEnglishWidth - tableColumn.colDegreeWidth - tableColumn.colClusteringWidth - 10
                                        height: parent.height
                                        color: "transparent"
                                        Label {
                                            anchors.centerIn: parent
                                            text: "局部效率"
                                            color: "#E5FFFFFF"
                                            font.pixelSize: 16
                                            font.bold: true
                                        }
                                    }
                                }
                            }

                            // 表格内容
                            ListView {
                                id: regionTableView
                                width: parent.width
                                height: parent.height - 38
                                clip: true
                                model: $BrainRegionTableModel
                                currentIndex: 0

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                    background: Rectangle {
                                        color: "#99232324"
                                    }
                                    contentItem: Rectangle {
                                        implicitWidth: 8
                                        radius: 4
                                        color: "#484849"
                                    }
                                }

                                delegate: Rectangle {
                                    width: regionTableView.width - 10
                                    height: 39
                                    color: regionTableView.currentIndex === index ? "#B23C7EFF" : "#99232324"

                                    Row {
                                        anchors.fill: parent

                                        // 序号
                                        Rectangle {
                                            width: tableColumn.colIndexWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: (index + 1)
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#B2FFFFFF"
                                                font.pixelSize: 16
                                            }
                                        }

                                        // 中文名称
                                        Rectangle {
                                            width: tableColumn.colChineseWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: model.chineseName
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#B2FFFFFF"
                                                font.pixelSize: 16
                                                elide: Text.ElideRight
                                                width: parent.width - 4
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }

                                        // Mricro命名
                                        Rectangle {
                                            width: tableColumn.colEnglishWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: model.englishName
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#B2FFFFFF"
                                                font.pixelSize: 16
                                                elide: Text.ElideRight
                                                width: parent.width - 4
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }

                                        // 度
                                        Rectangle {
                                            width: tableColumn.colDegreeWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: model.degree
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#B2FFFFFF"
                                                font.pixelSize: 16
                                            }
                                        }

                                        // 聚类
                                        Rectangle {
                                            width: tableColumn.colClusteringWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: model.clustering
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#B2FFFFFF"
                                                font.pixelSize: 16
                                            }
                                        }

                                        // 局部效率
                                        Rectangle {
                                            width: tableColumn.width - tableColumn.colIndexWidth - tableColumn.colChineseWidth - tableColumn.colEnglishWidth - tableColumn.colDegreeWidth - tableColumn.colClusteringWidth - 10
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: model.localEfficiency
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#B2FFFFFF"
                                                font.pixelSize: 16
                                            }
                                        }
                                    }
                                    // 分割线
                                    Rectangle {
                                        width: parent.width
                                        height: 1
                                        color: "#484849"
                                        anchors.bottom: parent.bottom
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            regionTableView.currentIndex = index
                                            $MainViewController.selectBrainRegion(index)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        Rectangle{
            id: aiRightOperatePanel
            anchors.fill: parent
            anchors.rightMargin: 16
            anchors.leftMargin: 16
            anchors.topMargin: 22
            anchors.bottomMargin: 22
            color: "transparent"
            visible: currentIndex === 4
            clip: true
            Column{
                width: parent.width
                spacing: 32
                anchors.centerIn: parent
                Image{
                    id: analyzePicture
                    width: 320
                    height: 320
                    source: "qrc:/image/brainAgeAnalyze/000.png"
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text{
                        anchors.centerIn: parent
                        text: qsTr("请先上传数据")
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        color: "#ffffff"
                        visible: !$MainViewController.brainAgeProcessing && $MainViewController.predictedBrainAge <= 0
                    }

                    // 分析结果显示
                    Column {
                        anchors.centerIn: parent
                        visible: $MainViewController.predictedBrainAge > 0
                        spacing: 2
                        Label {
                            text: qsTr("预测年龄")
                            color: "#ffffff"
                            font.pixelSize: 18
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Label {
                            text: $MainViewController.predictedBrainAge.toFixed() + " 岁"
                            color: "#FFFFFF"
                            font.pixelSize: 40
                            font.weight: Font.Bold
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }
        }
    }
}
