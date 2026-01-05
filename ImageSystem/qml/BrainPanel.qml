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
    property bool preprocessDone: false
    property bool segmentationDone: false
    property bool networkDone: false
    property int preprocessProgress: 0
    property int segmentationProgress: 0
    property int networkProgress: 0
    property bool networkIndeterminate: false
    property string selectedOutputPath: ""
    
    // 接收从外部传入的 FourViewPanel 实例
    property var fourViewPanel: null
    
    // 四视图容器（当在脑分割面板时使用）
    property alias fourViewContainer: brainSegmentationContainer
    property var messageManager: null
    property int preShowResultIndex: -1
    property bool isDeepprepOutput: false  // 标记是否为DeepPrep输出结构
    
    // 右侧面板展开状态
    property bool rightPanelExpanded: true
    
    // PDF 生成状态管理
    property int pdfGenerationState: 0  // 0: 默认, 1: 生成中, 2: 完成
    
    // 监听 currentIndex 变化，重置 PDF 状态
    onCurrentIndexChanged: {
        pdfGenerationState = 0
    }
    
    function resetBatchProgress() {
        batchProcessing = true
        preprocessDone = false
        segmentationDone = false
        networkDone = false
        preprocessProgress = 0
        segmentationProgress = 0
        networkProgress = 0
        networkIndeterminate = true
    }

    function completePreprocessStep() {
        preprocessDone = true
        preprocessProgress = 100
        tryFinishBatch()
    }

    function tryFinishBatch() {
        if (preprocessDone && segmentationDone && networkDone) {
            batchProcessing = false
        }
    }

    function detectOutputType(path) {
        // 使用C++方法检测输出类型
        isDeepprepOutput = $MainViewController.isDeepprepOutput(path)
    }

    function startUnifiedImports(url, normalizedPath) {
        selectedOutputPath = normalizedPath
        outputDetailDir.text = normalizedPath
        detectOutputType(normalizedPath)
        resetBatchProgress()
        completePreprocessStep()
        // 同步触发脑区分割与脑网络分析
        $DicomDataModel.loadSegBrainDirectory(url)
        $MainViewController.importBrainData(url)
        // 默认展示分割结果预览
        segBtnMouseArea.clicked(Qt.LeftButton)
    }

    FileDialog {
        id: preFileDialog
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

            dicomDir.text = path

            var lastSlash = path.lastIndexOf("/")
            var baseDir = lastSlash >= 0 ? path.substring(0, lastSlash + 1) : path
            bidsDir.text = baseDir + "Bids"
            outputDir.text = baseDir + "Output"
            outputDetailDir.text = baseDir + "Output"
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
            detectOutputType(path)
            startUnifiedImports(url, path)
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
        id: inputDirDialog
        title: qsTr("选择输入文件夹")
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

            inputDirDeep.text = path

            var lastSlash = path.lastIndexOf("/")
            var baseDir = lastSlash >= 0 ? path.substring(0, lastSlash + 1) : path
            bidsDirDeep.text = baseDir + "Bids"
            outputDirDeep.text = baseDir + "Output_deepprep"
        }
    }

    FileDialog {
        id: licenseFileDialogDeep
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

            licenseFileDeep.text = path
        }
    }

    FileDialog {
        id: niiGzDialog
        title: "选择 NII.GZ 文件"
        selectMultiple: false
        // 确保是文件选择模式
        selectFolder: false
        nameFilters: ["NIfTI Files (*.nii.gz)", "All Files (*)"]
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
            brainAgePath.text = path;
        }
    }

    FileDialog {
        id: dcmFolderDialog
        title: "选择 DCM 文件夹"
        // 确保是文件夹选择模式
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
            brainAgePath.text = path;
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
            height: 260
            color: "#2a2a2a"
            border.color: "#0078d4"
            border.width: 2
            radius: 10
            
            Column {
                anchors.fill: parent
                anchors.margins: 20
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
                        width: parent.width
                        Label { text: qsTr("预处理结果"); color: "#ffffff"; font.pixelSize: 14; width: 110 }
                        ProgressBar {
                            id: preprocessBar
                            from: 0; to: 100
                            indeterminate: !preprocessDone && preprocessProgress === 0
                            value: preprocessProgress
                            width: parent.width - 130
                        }
                    }

                    Row {
                        spacing: 10
                        width: parent.width
                        Label { text: qsTr("脑区分割"); color: "#ffffff"; font.pixelSize: 14; width: 110 }
                        ProgressBar {
                            id: segBar
                            from: 0; to: 100
                            indeterminate: !segmentationDone && segmentationProgress === 0
                            value: segmentationProgress
                            width: parent.width - 130
                        }
                    }

                    Row {
                        spacing: 10
                        width: parent.width
                        Label { text: qsTr("脑网络分析"); color: "#ffffff"; font.pixelSize: 14; width: 110 }
                        ProgressBar {
                            id: netBar
                            from: 0; to: 100
                            indeterminate: networkIndeterminate
                            value: networkProgress
                            width: parent.width - 130
                        }
                    }
                }
                Label {
                    width: parent.width
                    text: qsTr("请等待三个步骤全部完成后继续操作")
                    color: "#cccccc"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
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
                visible: currentIndex === 2
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
            Rectangle{
                id: aiAnalysisContainer
                width: parent.width
                height: midPanel.height - 16 - 60
                visible: currentIndex === 4
                color: "transparent"
                Rectangle{
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: aiRightOperatePanel.left
                    color: "#030D1F"
                    radius: 12
                }
                Rectangle{
                    id: aiRightOperatePanel
                    width: 420
                    color: "transparent"
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                }
            }
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
        width: (rightPanelExpanded && (currentIndex !== 4 && currentIndex !== 5)) ? ((currentIndex === 2 || currentIndex === 3) ? 500 : 400) : 0
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
            
            property int currentTabIndex: 0
            
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
                        // 传统处理 Tab
                        Item{
                            id: tab1
                            height: parent.height
                            width: 88
                            
                            property bool isHovered: false
                            scale: isHovered && preAnalysis.currentTabIndex !== 0 ? 1.05 : 1.0
                            
                            Behavior on scale {
                                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                            }

                            Label{
                                text: qsTr("传统处理")
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
                                
                                onEntered: tab1.isHovered = true
                                onExited: tab1.isHovered = false
                                
                                onClicked: {
                                    textBackground.animateToTab(0)
                                    tabIndicator.animateToTab(0)
                                }
                            }
                        }

                        // 深度学习 Tab
                        Item{
                            id: tab2
                            height: parent.height
                            width: 88
                            
                            property bool isHovered: false
                            scale: isHovered && preAnalysis.currentTabIndex !== 1 ? 1.05 : 1.0
                            
                            Behavior on scale {
                                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                            }

                            Label{
                                text: qsTr("深度学习")
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
                                
                                onEntered: tab2.isHovered = true
                                onExited: tab2.isHovered = false
                                
                                onClicked: {
                                    textBackground.animateToTab(1)
                                    tabIndicator.animateToTab(1)
                                }
                            }
                        }

                        // 数据详情 Tab
                        Item{
                            id: tab3
                            height: parent.height
                            width: 88
                            
                            property bool isHovered: false
                            scale: isHovered && preAnalysis.currentTabIndex !== 2 ? 1.05 : 1.0
                            
                            Behavior on scale {
                                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                            }

                            Label{
                                text: qsTr("数据详情")
                                font.pixelSize: 16
                                anchors.centerIn: parent
                                color: preAnalysis.currentTabIndex === 2 ? "#E5FFFFFF" : "#80FFFFFF"
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
                                    textBackground.animateToTab(2)
                                    tabIndicator.animateToTab(2)
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
                        property int currentTab: 0
                        property int targetTab: 0
                        
                        Component.onCompleted: {
                            x = getTabX(0)
                        }
                        
                        function getTabX(tabIndex) {
                            var rowX = (tabContainer.width - 264) / 2
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

                        property int currentTab: 0
                        property int targetTab: 0

                        Component.onCompleted: {
                            x = getTabX(0)
                        }

                        function getTabX(tabIndex) {
                            var rowX = (tabContainer.width - 264) / 2
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
                }
                Column{
                    id: fmriprepCol
                    width: parent.width
                    spacing: 12
                    visible: preAnalysis.currentTabIndex === 0
                    // 四个标签的最大宽度，保持对齐
                    property int maxLabelWidth: Math.max(
                                                    Math.max(label1.implicitWidth, label2.implicitWidth),
                                                    Math.max(label3.implicitWidth, label4.implicitWidth))
                    Row{
                        height: 38
                        spacing: 10
                        Label {
                            id:label1
                            text: qsTr("输入Dicom文件夹：")
                            font.pixelSize: 16
                            color: "#80FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                            width: fmriprepCol.maxLabelWidth
                        }
                        SingleLineTextInput{
                            id: dicomDir
                            width: fmriprepCol.width - label1.width - 60 - 20
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
                    Row{
                        height: 38
                        spacing: 10
                        Label {
                            id:label2
                            text: qsTr("输出Bids文件夹：")
                            font.pixelSize: 16
                            color: "#80FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                            width: fmriprepCol.maxLabelWidth
                        }
                        SingleLineTextInput{
                            id: bidsDir
                            width: fmriprepCol.width - label2.width - 10
                            height: 38
                            inputRadius: 4
                            backgroundColor: "#14FFFFFF"
                        }
                    }
                    Row{
                        height: 38
                        spacing: 10
                        Label {
                            id:label3
                            text: qsTr("输出Output文件夹：")
                            font.pixelSize: 16
                            color: "#80FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                            width: fmriprepCol.maxLabelWidth
                        }
                        SingleLineTextInput{
                            id: outputDir
                            width: fmriprepCol.width - label3.width - 10
                            height: 38
                            inputRadius: 4
                            backgroundColor: "#14FFFFFF"
                        }
                    }
                    Row{
                        height: 38
                        spacing: 10
                        Label {
                            id:label4
                            text: qsTr("license文件地址：")
                            font.pixelSize: 16
                            color: "#80FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                            width: fmriprepCol.maxLabelWidth
                        }
                        SingleLineTextInput{
                            id: licenseFile
                            width: fmriprepCol.width - label4.width - 60 - 20
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
                    Row{
                        height: 30
                        spacing: 8
                        CheckBox {
                            id: freesurferCheckBox
                            checked: false
                            width: 14
                            height: 14
                            anchors.verticalCenter: parent.verticalCenter
                            indicator: Rectangle {
                                implicitWidth: 14
                                implicitHeight: 14
                                radius: 2
                                anchors.verticalCenter: parent.verticalCenter
                                border.color: freesurferCheckBox.checked ? "#3C7EFF" : "#40000000"
                                border.width: 1
                                color: freesurferCheckBox.checked ? "#3C7EFF" : "#ffffff"

                                Image{
                                    source: "qrc:/image/vector.png"
                                    anchors.centerIn: parent
                                    visible: freesurferCheckBox.checked
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: freesurferCheckBox.checked = !freesurferCheckBox.checked
                                }
                            }
                        }
                        Label{
                            text: qsTr("使用freesurfer")
                            color: "#E5FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: freesurferCheckBox.checked = !freesurferCheckBox.checked
                            }
                        }
                    }
                    CustomButton{
                        width: parent.width
                        height: 48
                        text: qsTr("分析")
                        onClicked: {
                            function warn(msg) {
                                if (messageManager) {
                                    messageManager.warning(msg, 2000)
                                } else {
                                    console.log(msg)
                                }
                            }

                            var d = dicomDir.text.trim()
                            var b = bidsDir.text.trim()
                            var o = outputDir.text.trim()
                            var l = licenseFile.text.trim()

                            if (d === "") {
                                warn(qsTr("请输入 Dicom 文件夹路径"))
                                return
                            }
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

                            $MainViewController.startfmriprepAnalysis(d, b, o, l, freesurferCheckBox.checked)
                        }
                    }
                    // log 日志展示
                    Rectangle {
                        width: parent.width
                        height: preAnalysis.height - tabContainer.height - 16 - 38 * 4 - 30 - 48 - 6 * 12
                        color: "#E016171B"
                        radius: 8

                        ScrollView {
                            anchors.fill: parent
                            clip: true
                            TextArea {
                                id: logArea
                                readOnly: true
                                wrapMode: TextEdit.Wrap
                                font.pixelSize: 16
                                font.family: "Alibaba PuHuiTi 3.0"
                                color: "#E5FFFFFF"
                                text: $MainViewController.fmriprepLog
                                background: null

                                onTextChanged: {
                                    // 自动滚动到底部
                                    logArea.cursorPosition = logArea.length
                                    if (logArea.flickableItem) {
                                        var flick = logArea.flickableItem
                                        flick.contentY = Math.max(0, flick.contentHeight - flick.height)
                                    }
                                }
                            }
                        }
                    }
                }
                Column{
                    id: deepprepCol
                    width: parent.width
                    spacing: 12
                    visible: preAnalysis.currentTabIndex === 1
                    // 四个标签的最大宽度，保持对齐
                    property int maxLabelWidth: Math.max(
                                                    Math.max(label_dp1.implicitWidth, label_dp2.implicitWidth),
                                                    Math.max(label_dp3.implicitWidth, label_dp4.implicitWidth))
                    Row{
                        height: 38
                        spacing: 10
                        Label {
                            id:label_dp1
                            text: qsTr("输入Dicom文件夹：")
                            font.pixelSize: 16
                            color: "#80FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                            width: deepprepCol.maxLabelWidth
                        }
                        SingleLineTextInput{
                            id: inputDirDeep
                            width: fmriprepCol.width - label_dp1.width - 60 - 20
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
                                inputDirDialog.open()
                            }
                        }
                    }
                    Row{
                        height: 38
                        spacing: 10
                        Label {
                            id:label_dp2
                            text: qsTr("输出Bids文件夹：")
                            font.pixelSize: 16
                            color: "#80FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                            width: deepprepCol.maxLabelWidth
                        }
                        SingleLineTextInput{
                            id: bidsDirDeep
                            width: fmriprepCol.width - label_dp2.width - 10
                            height: 38
                            inputRadius: 4
                            backgroundColor: "#14FFFFFF"
                        }
                    }
                    Row{
                        height: 38
                        spacing: 10
                        Label {
                            id:label_dp3
                            text: qsTr("输出Output文件夹：")
                            font.pixelSize: 16
                            color: "#80FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                            width: deepprepCol.maxLabelWidth
                        }
                        SingleLineTextInput{
                            id: outputDirDeep
                            width: fmriprepCol.width - label_dp3.width - 10
                            height: 38
                            inputRadius: 4
                            backgroundColor: "#14FFFFFF"
                        }
                    }
                    Row{
                        height: 38
                        spacing: 10
                        Label {
                            id:label_dp4
                            text: qsTr("license文件地址：")
                            font.pixelSize: 16
                            color: "#80FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                            width: deepprepCol.maxLabelWidth
                        }
                        SingleLineTextInput{
                            id: licenseFileDeep
                            width: fmriprepCol.width - label_dp4.width - 60 - 20
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
                                licenseFileDialogDeep.open()
                            }
                        }
                    }
                    CustomButton{
                        width: parent.width
                        height: 48
                        text: qsTr("分析")
                        onClicked: {
                            function warn(msg) {
                                if (messageManager) {
                                    messageManager.warning(msg, 2000)
                                } else {
                                    console.log(msg)
                                }
                            }

                            var i = inputDirDeep.text.trim()
                            var b = bidsDirDeep.text.trim()
                            var o = outputDirDeep.text.trim()
                            var l = licenseFileDeep.text.trim()

                            if (i === "") {
                                warn(qsTr("请输入 Input 文件夹路径"))
                                return
                            }
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

                            $MainViewController.startDeepprepAnalysis(i, b, o, l)
                        }
                    }
                    // log 日志展示
                    Rectangle {
                        width: parent.width
                        height: preAnalysis.height - tabContainer.height - 16 - 38 * 4 - 48 - 5 * 12
                        color: "#E016171B"
                        radius: 8

                        ScrollView {
                            anchors.fill: parent
                            clip: true
                            TextArea {
                                id: logAreaDeep
                                readOnly: true
                                wrapMode: TextEdit.Wrap
                                font.pixelSize: 16
                                font.family: "Alibaba PuHuiTi 3.0"
                                color: "#E5FFFFFF"
                                text: $MainViewController.deepprepLog
                                background: null

                                onTextChanged: {
                                    // 自动滚动到底部
                                    logAreaDeep.cursorPosition = logAreaDeep.length
                                    if (logAreaDeep.flickableItem) {
                                        var flick = logAreaDeep.flickableItem
                                        flick.contentY = Math.max(0, flick.contentHeight - flick.height)
                                    }
                                }
                            }
                        }
                    }
                }
                Column{
                    id: preDetailCol
                    width: parent.width
                    spacing: 12
                    visible: preAnalysis.currentTabIndex === 2
                    Row{
                        height: 38
                        spacing: 10
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
                    Rectangle{
                        width: parent.width
                        color: "transparent"
                        height: 8
                    }
                    Label {
                        text: qsTr("Structural Phase")
                        font.pixelSize: 18
                        color: "#ffffff"
                    }
                    Item{
                        id: segmentationBtn
                        width: btnText0.width + 40
                        height: 36
                        
                        property bool isHovered: false
                        
                        Image {
                            id: btnImg0
                            anchors.fill: parent
                            source: preShowResultIndex === 0 ? "qrc:/image/preBtnBackgroundSelected.png" : "qrc:/image/preBtnBackground.png"
                            fillMode: Image.Stretch
                            opacity: segmentationBtn.isHovered ? 0.8 : 1.0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 200 }
                            }
                        }
                        
                        Text {
                            id: btnText0
                            anchors.centerIn: parent
                            text: qsTr("Segmentation")
                            color: "#B2FFFFFF"
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
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 0
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_desc-volparc_T1w.svg" : "sub-01_dseg.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Item{
                        id: regBtn
                        width: btnText1.width + 40
                        height: 36
                        
                        property bool isHovered: false
                        
                        Image {
                            id: btnImg1
                            anchors.fill: parent
                            source: preShowResultIndex === 1 ? "qrc:/image/preBtnBackgroundSelected.png" : "qrc:/image/preBtnBackground.png"
                            fillMode: Image.Stretch
                            opacity: regBtn.isHovered ? 0.8 : 1.0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 200 }
                            }
                        }
                        
                        Text {
                            id: btnText1
                            anchors.centerIn: parent
                            text: qsTr("Registration")
                            color: "#B2FFFFFF"
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
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 1
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_task-rest_desc-coreg_bold.svg" : "sub-01_space-MNI152NLin2009cAsym_T1w.svg"
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
                        text: qsTr("Standard Space")
                        font.pixelSize: 18
                        color: "#ffffff"
                    }
                    Item{
                        id: mniBtn
                        width: btnText2.width + 40
                        height: 36
                        
                        property bool isHovered: false
                        
                        Image {
                            id: btnImg2
                            anchors.fill: parent
                            source: preShowResultIndex === 2 ? "qrc:/image/preBtnBackgroundSelected.png" : "qrc:/image/preBtnBackground.png"
                            fillMode: Image.Stretch
                            opacity: mniBtn.isHovered ? 0.8 : 1.0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 200 }
                            }
                        }
                        
                        Text {
                            id: btnText2
                            anchors.centerIn: parent
                            text: qsTr("MN152NLin2009cAsym")
                            color: "#B2FFFFFF"
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
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 2
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_desc-T1toMNI152_combine.svg" : "sub-01_space-MNI152NLin2009cAsym_T1w.svg"
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
                        text: qsTr("Functional Phase")
                        font.pixelSize: 18
                        color: "#ffffff"
                    }
                    Item{
                        id: t1FunBtn
                        width: btnText3.width + 40
                        height: 36
                        
                        property bool isHovered: false
                        
                        Image {
                            id: btnImg3
                            anchors.fill: parent
                            source: preShowResultIndex === 3 ? "qrc:/image/preBtnBackgroundSelected.png" : "qrc:/image/preBtnBackground.png"
                            fillMode: Image.Stretch
                            opacity: t1FunBtn.isHovered ? 0.8 : 1.0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 200 }
                            }
                        }
                        
                        Text {
                            id: btnText3
                            anchors.centerIn: parent
                            text: qsTr("T1 to Fun")
                            color: "#B2FFFFFF"
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
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 3
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = "sub-01_task-rest_desc-coreg_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Item{
                        id: boldBtn
                        width: btnText4.width + 40
                        height: 36
                        
                        property bool isHovered: false
                        
                        Image {
                            id: btnImg4
                            anchors.fill: parent
                            source: preShowResultIndex === 4 ? "qrc:/image/preBtnBackgroundSelected.png" : "qrc:/image/preBtnBackground.png"
                            fillMode: Image.Stretch
                            opacity: boldBtn.isHovered ? 0.8 : 1.0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 200 }
                            }
                        }
                        
                        Text {
                            id: btnText4
                            anchors.centerIn: parent
                            text: qsTr("BOLD summary")
                            color: "#B2FFFFFF"
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
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 4
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_task-rest_desc-carpet_bold.svg" : "sub-01_task-rest_desc-carpetplot_bold.svg"
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
                        text: isDeepprepOutput ? qsTr("DeepPrep Outputs") : qsTr("QC quality control")
                        font.pixelSize: 18
                        color: "#ffffff"
                    }
                    Item{
                        id: corticalBtn
                        width: btnText5.width + 40
                        height: 36
                        
                        property bool isHovered: false
                        
                        Image {
                            id: btnImg5
                            anchors.fill: parent
                            source: preShowResultIndex === 5 ? "qrc:/image/preBtnBackgroundSelected.png" : "qrc:/image/preBtnBackground.png"
                            fillMode: Image.Stretch
                            opacity: corticalBtn.isHovered ? 0.8 : 1.0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 200 }
                            }
                        }
                        
                        Text {
                            id: btnText5
                            anchors.centerIn: parent
                            text: isDeepprepOutput ? qsTr("Cortical surface") : qsTr("CompCor ROIs")
                            color: "#B2FFFFFF"
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
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 5
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_desc-surfparc_T1w.svg" : "sub-01_task-rest_desc-rois_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Item{
                        id: tsnrBtn
                        width: btnText6.width + 40
                        height: 36
                        
                        property bool isHovered: false
                        
                        Image {
                            id: btnImg6
                            anchors.fill: parent
                            source: preShowResultIndex === 6 ? "qrc:/image/preBtnBackgroundSelected.png" : "qrc:/image/preBtnBackground.png"
                            fillMode: Image.Stretch
                            opacity: tsnrBtn.isHovered ? 0.8 : 1.0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 200 }
                            }
                        }
                        
                        Text {
                            id: btnText6
                            anchors.centerIn: parent
                            text: isDeepprepOutput ? qsTr("tSNR") : qsTr("Variance")
                            color: "#B2FFFFFF"
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
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 6
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_task-rest_bold_desc-tsnr_bold.svg" : "sub-01_task-rest_desc-compcorvar_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                    Item{
                        id: surfaceBtn
                        width: btnText7.width + 40
                        height: 36
                        
                        property bool isHovered: false
                        
                        Image {
                            id: btnImg7
                            anchors.fill: parent
                            source: preShowResultIndex === 7 ? "qrc:/image/preBtnBackgroundSelected.png" : "qrc:/image/preBtnBackground.png"
                            fillMode: Image.Stretch
                            opacity: surfaceBtn.isHovered ? 0.8 : 1.0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: 200 }
                            }
                        }
                        
                        Text {
                            id: btnText7
                            anchors.centerIn: parent
                            text: isDeepprepOutput ? qsTr("Surface reconstruction") : qsTr("nuisance regressors Correlations")
                            color: "#B2FFFFFF"
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
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 7
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_desc-volsurf_T1w.svg" : "sub-01_task-rest_desc-confoundcorr_bold.svg"
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

                Rectangle {
                    width: parent.width
                    height: segmentationAnalysis.height - segLabel.height - 24
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
            id: aiAnalysis
            anchors.fill: parent
            color: "transparent"
            visible: currentIndex === 4
            clip: true
            Column {
                padding: 10
                width: parent.width
                spacing: 10
                Label{
                    text: qsTr("导入数据（nii.gz或dicom文件夹）：")
                    color: "#ffffff"
                    font.pixelSize: 16
                }
                Label{
                    id:brainAgePath
                    text: qsTr("")
                    color: "#ffffff"
                    font.pixelSize: 16
                    visible: text !== ""
                    width: parent.width - 20
                    elide: Text.ElideMiddle
                }
                Row {
                    spacing: 5
                    height: 40
                    CustomButton {
                        width: (aiAnalysis.width - 25) / 2
                        height: 40
                        text: "导入nii.gz"
                        backgroundColor: "#004578"
                        onClicked: {
                            niiGzDialog.open()
                        }
                    }
                    CustomButton {
                        width: (aiAnalysis.width - 25) / 2
                        height: 40
                        text: "导入dcm文件夹"
                        backgroundColor: "#004578"
                        onClicked: {
                            dcmFolderDialog.open()
                        }
                    }
                }
                Row{
                    height: 30
                    spacing: 5
                    CheckBox {
                        id: preprocessCheckBox
                        checked: true
                        width: 16
                        height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        indicator: Rectangle {
                            implicitWidth: 16
                            implicitHeight: 16
                            radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            border.color: preprocessCheckBox.checked ? "#006BFF" : "#40000000"
                            border.width: 1
                            color: preprocessCheckBox.checked ? "#006BFF" : "#ffffff"

                            Image{
                                source: "qrc:/image/vector.png"
                                anchors.centerIn: parent
                                visible: preprocessCheckBox.checked
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: preprocessCheckBox.checked = !preprocessCheckBox.checked
                            }
                        }
                    }
                    Label{
                        text: qsTr("去颅骨+与标准空间对齐")
                        color: "#ffffff"
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: preprocessCheckBox.checked = !preprocessCheckBox.checked
                        }
                    }
                }
                Label{
                    text: qsTr("注：不进行预处理的数据可能会导致模型预测结果误差过大！")
                    color: "#ffffff"
                    font.pixelSize: 16
                    visible: !preprocessCheckBox.checked
                    width: parent.width - 20
                    wrapMode: Text.WordWrap
                }
                CustomButton {
                    width: parent.width - 20
                    height: 40
                    text: "开始分析"
                    backgroundColor: "#004578"
                    onClicked: {
                        if(brainAgePath.text !== ""){
                            $MainViewController.startAnalysisBrainAge(brainAgePath.text, preprocessCheckBox.checked)
                        }else{
                            messageManager.error("请先选择文件！")
                        }
                    }
                }
                Row{
                    width: parent.width - 20
                    height: 32
                    spacing: 8
                    visible: $MainViewController.brainAgeProcessing
                    AnimatedImage {
                        id: loadingIcon
                        width: 16
                        height: 16
                        source: "qrc:/image/loading.gif"
                        playing: $MainViewController.brainAgeProcessing
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label{
                        text: qsTr("正在分析中...")
                        color: "#ffffff"
                        font.pixelSize: 16
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                Label{
                    width: parent.width - 20
                    text: $MainViewController.predictedBrainAge > 0 ? qsTr("预测年龄：") + $MainViewController.predictedBrainAge.toFixed(2) : ""
                    color: "#ffffff"
                    font.pixelSize: 18
                    visible: $MainViewController.predictedBrainAge > 0
                    wrapMode: Text.WrapAnywhere
                }
            }
        }
    }
}
