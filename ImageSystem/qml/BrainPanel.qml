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
    property bool showResult: false
    property int preShowResultIndex: -1
    property bool isDeepprepOutput: false  // 标记是否为DeepPrep输出结构
    
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
        console.log("Detected output type - DeepPrep:", isDeepprepOutput, "Path:", path)
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
        segmentationBtn.clicked()
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
    
    Column{
        spacing: 10
        width: parent.width
        Rectangle {
            height: 60
            width: parent.width
            color: "transparent"
            Row{
                height: parent.height
                spacing: 20
                anchors.horizontalCenter: parent.horizontalCenter
                CustomButton{
                    text: qsTr("数据预处理")
                    height: parent.height
                    width: 150
                    backgroundColor: currentIndex === 1 ? "#619e9f" : "#000000"
                    textColor: "#ffffff"
                    onClicked: {
                        currentIndex = 1
                        showResult = true
                    }
                }
                CustomButton{
                    text: qsTr("脑区分割")
                    height: parent.height
                    width: 150
                    backgroundColor: currentIndex === 2 ? "#619e9f" : "#000000"
                    textColor: "#ffffff"
                    onClicked: {
                        currentIndex = 2
                        showResult = false
                    }
                }
                CustomButton{
                    text: qsTr("脑网络")
                    height: parent.height
                    width: 150
                    backgroundColor: currentIndex === 3 ? "#619e9f" : "#000000"
                    textColor: "#ffffff"
                    onClicked: {
                        currentIndex = 3
                        showResult = false
                    }
                }
                CustomButton{
                    text: qsTr("AI分析")
                    height: parent.height
                    width: 150
                    backgroundColor: currentIndex === 4 ? "#619e9f" : "#000000"
                    textColor: "#ffffff"
                    onClicked: {
                        currentIndex = 4
                        showResult = false
                    }
                }
                CustomButton{
                    text: qsTr("生成报告")
                    height: parent.height
                    width: 150
                    backgroundColor: currentIndex === 5 ? "#619e9f" : "#000000"
                    textColor: "#ffffff"
                    onClicked: {
                        currentIndex = 5
                        showResult = false
                    }
                }
            }
        }
        Rectangle {
            height: rootPanel.height - 60 - 10
            width: parent.width
            color: "transparent"
            Item {
                id: brainSegmentationContainer
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                width: parent.width - 380
                visible: (currentIndex === 1 && !showResult) || currentIndex === 2 || currentIndex === 4 || currentIndex === 5
            }
            WebEngineView {
                id: preResult
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                width: parent.width - 380
                backgroundColor: "#000000"
                visible: currentIndex === 1 && showResult
            }
            Rectangle{
                id: networkMain
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width - 380
                visible: currentIndex === 3
                color: "transparent"
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.topMargin: 5
                    anchors.leftMargin: 5
                    width: (parent.width - 15) / 2
                    height: (parent.height - 15) / 2
                    color: "#000000"
                    border.color: "#404040"
                    border.width: 1
                    Image{
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        source: $MainViewController.currentRegionplotsUrl
                    }
                }
                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 5
                    anchors.rightMargin: 5
                    width: (parent.width - 15) / 2
                    height: (parent.height - 15) / 2
                    color: "#000000"
                    border.color: "#404040"
                    border.width: 1
                    Image{
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        source: $MainViewController.currentAlffUrl
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.bottomMargin: 5
                    anchors.leftMargin: 5
                    width: (parent.width - 15) / 2
                    height: (parent.height - 15) / 2
                    color: "#000000"
                    border.color: "#404040"
                    border.width: 1
                    Image{
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        source: $MainViewController.currentCovarianceUrl
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.bottomMargin: 5
                    anchors.rightMargin: 5
                    width: (parent.width - 15) / 2
                    height: (parent.height - 15) / 2
                    color: "#000000"
                    border.color: "#404040"
                    border.width: 1
                    WebEngineView{
                        anchors.fill: parent
                        visible: $MainViewController.currentViewConnectomeUrl !== ""
                        url: $MainViewController.currentViewConnectomeUrl
                    }
                }
            }
            Rectangle{
                id: preAnalysis
                width: 380
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                color: "transparent"
                visible: currentIndex === 1
                Column{
                    width: parent.width
                    spacing: 20
                    padding: 5
                    TabSwitcher{
                        id: tabSwitcher
                        tabTitles: ["传统处理", "深度学习", "数据详情"]
                    }
                    Column{
                        id: fmriprepCol
                        width: parent.width - 15
                        spacing: 20
                        visible: tabSwitcher.currentIndex === 0
                        // 四个标签的最大宽度，保持对齐
                        property int maxLabelWidth: Math.max(
                                                        Math.max(label1.implicitWidth, label2.implicitWidth),
                                                        Math.max(label3.implicitWidth, label4.implicitWidth))
                        Row{
                            height: 30
                            spacing: 5
                            Label {
                                id:label1
                                text: qsTr("输入Dicom文件夹：")
                                color: "#ffffff"
                                anchors.verticalCenter: parent.verticalCenter
                                width: fmriprepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: dicomDir
                                width: 150
                            }
                            CustomButton{
                                width: fmriprepCol.width - label1.width - 150 - 10
                                height: 30
                                text: qsTr("导入")
                                onClicked: {
                                    preFileDialog.open()
                                }
                            }
                        }
                        Row{
                            height: 30
                            spacing: 5
                            Label {
                                id:label2
                                text: qsTr("输出Bids文件夹：")
                                color: "#ffffff"
                                anchors.verticalCenter: parent.verticalCenter
                                width: fmriprepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: bidsDir
                                width: fmriprepCol.width - label2.width - 5
                            }
                        }
                        Row{
                            height: 30
                            spacing: 5
                            Label {
                                id:label3
                                text: qsTr("输出Output文件夹：")
                                color: "#ffffff"
                                anchors.verticalCenter: parent.verticalCenter
                                width: fmriprepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: outputDir
                                width: fmriprepCol.width - label3.width - 5
                            }
                        }
                        Row{
                            height: 30
                            spacing: 5
                            Label {
                                id:label4
                                text: qsTr("license文件地址：")
                                color: "#ffffff"
                                anchors.verticalCenter: parent.verticalCenter
                                width: fmriprepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: licenseFile
                                width: 150
                            }
                            CustomButton{
                                width: fmriprepCol.width - label4.width - 150 - 10
                                height: 30
                                text: qsTr("导入")
                                onClicked: {
                                    licenseFileDialog.open()
                                }
                            }
                        }
                        Row{
                            height: 30
                            spacing: 5
                            CheckBox {
                                id: freesurferCheckBox
                                checked: false
                                width: 16
                                height: 16
                                anchors.verticalCenter: parent.verticalCenter
                                indicator: Rectangle {
                                    implicitWidth: 16
                                    implicitHeight: 16
                                    radius: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    border.color: freesurferCheckBox.checked ? "#006BFF" : "#40000000"
                                    border.width: 1
                                    color: freesurferCheckBox.checked ? "#006BFF" : "#ffffff"

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
                                color: "#ffffff"
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
                            height: 30
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
                            height: preAnalysis.height - 140 - 180 - tabSwitcher.height - 10
                            color: "#0f0f0f"
                            radius: 4
                            border.color: "#303030"
                            border.width: 1

                            ScrollView {
                                anchors.fill: parent
                                clip: true
                                TextArea {
                                    id: logArea
                                    readOnly: true
                                    wrapMode: TextEdit.Wrap
                                    font.pixelSize: 12
                                    font.family: "Alibaba PuHuiTi 3.0"
                                    color: "#ffffff"
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
                        width: parent.width - 15
                        spacing: 20
                        visible: tabSwitcher.currentIndex === 1
                        // 四个标签的最大宽度，保持对齐
                        property int maxLabelWidth: Math.max(
                                                        Math.max(label_dp1.implicitWidth, label_dp2.implicitWidth),
                                                        Math.max(label_dp3.implicitWidth, label_dp4.implicitWidth))
                        Row{
                            height: 30
                            spacing: 5
                            Label {
                                id:label_dp1
                                text: qsTr("输入文件夹：")
                                color: "#ffffff"
                                anchors.verticalCenter: parent.verticalCenter
                                width: deepprepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: inputDirDeep
                                width: 150
                            }
                            CustomButton{
                                width: deepprepCol.width - label_dp1.width - 150 - 10
                                height: 30
                                text: qsTr("导入")
                                onClicked: {
                                    inputDirDialog.open()
                                }
                            }
                        }
                        Row{
                            height: 30
                            spacing: 5
                            Label {
                                id:label_dp2
                                text: qsTr("输出Bids文件夹：")
                                color: "#ffffff"
                                anchors.verticalCenter: parent.verticalCenter
                                width: deepprepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: bidsDirDeep
                                width: deepprepCol.width - label_dp2.width - 5
                            }
                        }
                        Row{
                            height: 30
                            spacing: 5
                            Label {
                                id:label_dp3
                                text: qsTr("输出Output文件夹：")
                                color: "#ffffff"
                                anchors.verticalCenter: parent.verticalCenter
                                width: deepprepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: outputDirDeep
                                width: deepprepCol.width - label_dp3.width - 5
                            }
                        }
                        Row{
                            height: 30
                            spacing: 5
                            Label {
                                id:label_dp4
                                text: qsTr("license文件地址：")
                                color: "#ffffff"
                                anchors.verticalCenter: parent.verticalCenter
                                width: deepprepCol.maxLabelWidth
                            }
                            SingleLineTextInput{
                                id: licenseFileDeep
                                width: 150
                            }
                            CustomButton{
                                width: deepprepCol.width - label_dp4.width - 150 - 10
                                height: 30
                                text: qsTr("导入")
                                onClicked: {
                                    licenseFileDialogDeep.open()
                                }
                            }
                        }
                        CustomButton{
                            width: parent.width
                            height: 30
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
                            height: preAnalysis.height - 140 - 140 - tabSwitcher.height - 10
                            color: "#0f0f0f"
                            radius: 4
                            border.color: "#303030"
                            border.width: 1

                            ScrollView {
                                anchors.fill: parent
                                clip: true
                                TextArea {
                                    id: logAreaDeep
                                    readOnly: true
                                    wrapMode: TextEdit.Wrap
                                    font.pixelSize: 12
                                    font.family: "Alibaba PuHuiTi 3.0"
                                    color: "#ffffff"
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
                        width: parent.width - 15
                        spacing: 20
                        visible: tabSwitcher.currentIndex === 2
                        Row{
                            height: 30
                            spacing: 5
                            Label {
                                id: label5
                                text: qsTr("预处理后文件夹：")
                                color: "#ffffff"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            SingleLineTextInput{
                                id: outputDetailDir
                                width: 150
                            }
                            CustomButton{
                                width: fmriprepCol.width - label5.width - 150 - 10
                                height: 30
                                text: qsTr("导入")
                                onClicked: {
                                    outputDetailDialog.open()
                                }
                            }
                        }
                        Label {
                            text: qsTr("Structural Phase")
                            font.pixelSize: 16
                            color: "#ffffff"
                        }
                        CustomButton{
                            id: segmentationBtn
                            backgroundColor: preShowResultIndex === 0 ? "green" : "transparent"
                            textColor: "#ffffff"
                            borderColor: "#ffffff"
                            borderWidth: 1
                            fontSize: 14
                            text: qsTr("Segmentation")
                            onClicked: {
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 0
                                showResult = true
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_desc-volparc_T1w.svg" : "sub-01_dseg.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                        CustomButton{
                            backgroundColor: preShowResultIndex === 1 ? "green" : "transparent"
                            textColor: "#ffffff"
                            borderColor: "#ffffff"
                            borderWidth: 1
                            fontSize: 14
                            text: qsTr("Registration")
                            onClicked: {
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 1
                                showResult = true
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_task-rest_desc-coreg_bold.svg" : "sub-01_space-MNI152NLin2009cAsym_T1w.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                        Label {
                            text: qsTr("Standard Space")
                            font.pixelSize: 16
                            color: "#ffffff"
                        }
                        CustomButton{
                            backgroundColor: preShowResultIndex === 2 ? "green" : "transparent"
                            textColor: "#ffffff"
                            borderColor: "#ffffff"
                            borderWidth: 1
                            fontSize: 14
                            text: qsTr("MN152NLin2009cAsym")
                            onClicked: {
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 2
                                showResult = true
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_desc-T1toMNI152_combine.svg" : "sub-01_space-MNI152NLin2009cAsym_T1w.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                        Label {
                            text: qsTr("Functional Phase")
                            font.pixelSize: 16
                            color: "#ffffff"
                        }
                        CustomButton{
                            backgroundColor: preShowResultIndex === 3 ? "green" : "transparent"
                            textColor: "#ffffff"
                            borderColor: "#ffffff"
                            borderWidth: 1
                            fontSize: 14
                            text: qsTr("T1 to Fun")
                            onClicked: {
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 3
                                showResult = true
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = "sub-01_task-rest_desc-coreg_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                        CustomButton{
                            backgroundColor: preShowResultIndex === 4 ? "green" : "transparent"
                            textColor: "#ffffff"
                            borderColor: "#ffffff"
                            borderWidth: 1
                            fontSize: 14
                            text: qsTr("BOLD summary")
                            onClicked: {
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 4
                                showResult = true
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_task-rest_desc-carpet_bold.svg" : "sub-01_task-rest_desc-carpetplot_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                        Label {
                            text: isDeepprepOutput ? qsTr("DeepPrep Outputs") : qsTr("QC quality control")
                            font.pixelSize: 16
                            color: "#ffffff"
                        }
                        CustomButton{
                            backgroundColor: preShowResultIndex === 5 ? "green" : "transparent"
                            textColor: "#ffffff"
                            borderColor: "#ffffff"
                            borderWidth: 1
                            fontSize: 14
                            text: isDeepprepOutput ? qsTr("Cortical surface") : qsTr("CompCor ROIs")
                            onClicked: {
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 5
                                showResult = true
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_desc-surfparc_T1w.svg" : "sub-01_task-rest_desc-rois_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                        CustomButton{
                            backgroundColor: preShowResultIndex === 6 ? "green" : "transparent"
                            textColor: "#ffffff"
                            borderColor: "#ffffff"
                            borderWidth: 1
                            fontSize: 14
                            text: isDeepprepOutput ? qsTr("tSNR") : qsTr("Variance")
                            onClicked: {
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 6
                                showResult = true
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_task-rest_bold_desc-tsnr_bold.svg" : "sub-01_task-rest_desc-compcorvar_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                        CustomButton{
                            backgroundColor: preShowResultIndex === 7 ? "green" : "transparent"
                            textColor: "#ffffff"
                            borderColor: "#ffffff"
                            borderWidth: 1
                            fontSize: 14
                            text: isDeepprepOutput ? qsTr("Surface reconstruction") : qsTr("nuisance regressors Correlations")
                            onClicked: {
                                if(outputDetailDir.text === ""){
                                    return
                                }
                                preShowResultIndex = 7
                                showResult = true
                                var basePath = isDeepprepOutput ? "/QC/sub-01/figures/" : "/sub-01/figures/"
                                var fileName = isDeepprepOutput ? "sub-01_desc-volsurf_T1w.svg" : "sub-01_task-rest_desc-confoundcorr_bold.svg"
                                preResult.url = outputDetailDir.text + basePath + fileName
                            }
                        }
                    }
                }
            }
            Rectangle{
                id: segmentationAnalysis
                width: 380
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                color: "transparent"
                visible: currentIndex === 2
                Column{
                    width: parent.width
                    spacing: 10
                Label{
                    text: qsTr("使用预处理路径：") + outputDetailDir.text
                    color: "#ffffff"
                    font.pixelSize: 14
                    wrapMode: Text.WrapAnywhere
                }
                    ///脑区表格
                    Rectangle {
                        width: parent.width - 5
                        height: segmentationAnalysis.height - 50 - 5
                        color: "#1a1a1a"
                        border.color: "#404040"
                        border.width: 1

                        Column {
                            id: segTableColumn
                            anchors.fill: parent
                            anchors.margins: 5
                            spacing: 0

                            // 定义列宽常量
                            readonly property int colVisibleWidth: 40
                            readonly property int colColorWidth: 40
                            readonly property int colChineseWidth: 100
                            readonly property int colHemisphereWidth: 50
                            readonly property int colVolumeWidth: 70
                            readonly property int colPercentWidth: 70
                            readonly property int colAsymmetryWidth: 60
                            readonly property int totalContentWidth: colVisibleWidth + colColorWidth + colChineseWidth + colHemisphereWidth + colVolumeWidth + colPercentWidth + colAsymmetryWidth

                            // 横向滚动的表头容器
                            Item {
                                width: parent.width - 8  // 留出纵向滚动条的空间
                                height: 35
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
                                        height: 35
                                        color: "#2a2a2a"

                                        Row {
                                            width: parent.width
                                            height: parent.height

                                    // 显示列
                                    Rectangle {
                                        width: segTableColumn.colVisibleWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "显示"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 颜色列
                                    Rectangle {
                                        width: segTableColumn.colColorWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "颜色"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 中文名称
                                    Rectangle {
                                        width: segTableColumn.colChineseWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "中文名称"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 位置
                                    Rectangle {
                                        width: segTableColumn.colHemisphereWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "位置"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 容积
                                    Rectangle {
                                        width: segTableColumn.colVolumeWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "容积(cm³)"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 全脑占比
                                    Rectangle {
                                        width: segTableColumn.colPercentWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "全脑占比"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                            // 不对称指数
                                            Rectangle {
                                                width: segTableColumn.colAsymmetryWidth
                                                height: parent.height
                                                color: "transparent"
                                                border.color: "#404040"
                                                border.width: 1
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: "不对称"
                                                    color: "#ffffff"
                                                    font.pixelSize: 12
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
                                height: parent.height - 35

                                Flickable {
                                    id: contentFlickable
                                    anchors.fill: parent
                                    anchors.rightMargin: 8  // 为纵向滚动条留空间
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
                                            color: "#1a1a1a"
                                        }
                                        contentItem: Rectangle {
                                            implicitHeight: 8
                                            radius: 4
                                            color: "#404040"
                                        }
                                    }

                                    ListView {
                                        id: segmentationTableView
                                        width: segTableColumn.totalContentWidth
                                        height: parent.height
                                        interactive: false  // 禁用ListView自身的交互，使用Flickable的交互
                                        clip: true
                                        model: $BrainSegmentationTableModel

                                        delegate: Rectangle {
                                            width: segTableColumn.totalContentWidth
                                            height: 30
                                            color: index % 2 === 0 ? "#1a1a1a" : "#252525"

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
                                                color: "#cccccc"
                                                font.pixelSize: 11
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
                                                color: "#cccccc"
                                                font.pixelSize: 11
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
                                                color: "#cccccc"
                                                font.pixelSize: 11
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
                                                color: "#cccccc"
                                                font.pixelSize: 11
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
                                                        color: "#cccccc"
                                                        font.pixelSize: 11
                                                    }
                                                }
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
                                        color: "#1a1a1a"
                                    }
                                    contentItem: Rectangle {
                                        implicitWidth: 8
                                        radius: 4
                                        color: "#404040"
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Rectangle{
                id: networkAnalysis
                width: 380
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                color: "transparent"
                visible: currentIndex === 3
                Column {
                    padding: 10
                    width: parent.width
                    spacing: 10
                    Label{
                        text: qsTr("使用预处理路径：") + outputDetailDir.text
                        color: "#ffffff"
                        font.pixelSize: 14
                        wrapMode: Text.WrapAnywhere
                    }
                    Rectangle{
                        width: parent.width - 20
                        height: infoList.height
                        color: "#000000"
                        Column{
                            id:infoList
                            width: parent.width
                            spacing: 5
                            padding: 5
                            Label{
                                text: qsTr("全局效率：" + $MainViewController.globalEfficiency)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("平均局部效率：" + $MainViewController.averageLocalEfficiency)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("平均聚类系数：" + $MainViewController.averageClusteringCoefficient)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("富俱乐部系数")
                                color: "#ffffff"
                                font.weight: Font.Bold
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("富俱乐部连接：" + $MainViewController.richClubConnections)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("桥接连接：" + $MainViewController.bridgeConnections)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("局部连接：" + $MainViewController.localConnections)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                        }
                    }

                    // 脑区数据表格
                    Rectangle {
                        width: parent.width - 20
                        height: networkAnalysis.height - infoList.height - 60 - 15
                        color: "#1a1a1a"
                        border.color: "#404040"
                        border.width: 1

                        Column {
                            id: tableColumn
                            anchors.fill: parent
                            anchors.margins: 5
                            spacing: 0

                            // 定义列宽常量
                            readonly property int colIndexWidth: 40
                            readonly property int colChineseWidth: 80
                            readonly property int colEnglishWidth: 80
                            readonly property int colDegreeWidth: 40
                            readonly property int colClusteringWidth: 45

                            // 表格标题
                            Rectangle {
                                id: tableHeader
                                width: parent.width
                                height: 35
                                color: "#2a2a2a"

                                Row {
                                    anchors.fill: parent

                                    // 序号列
                                    Rectangle {
                                        width: tableColumn.colIndexWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "#"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 中文名称
                                    Rectangle {
                                        width: tableColumn.colChineseWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "中文名称"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // Mricro命名
                                    Rectangle {
                                        width: tableColumn.colEnglishWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "Mricro命名"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 度
                                    Rectangle {
                                        width: tableColumn.colDegreeWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "度"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 聚类
                                    Rectangle {
                                        width: tableColumn.colClusteringWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "聚类"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 局部效率
                                    Rectangle {
                                        width: tableColumn.width - tableColumn.colIndexWidth - tableColumn.colChineseWidth - tableColumn.colEnglishWidth - tableColumn.colDegreeWidth - tableColumn.colClusteringWidth - 10
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "局部效率"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }
                                }
                            }

                            // 表格内容
                            ListView {
                                id: regionTableView
                                width: parent.width
                                height: parent.height - 35
                                clip: true
                                model: $BrainRegionTableModel
                                currentIndex: 0

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AlwaysOn
                                    background: Rectangle {
                                        color: "#1a1a1a"
                                    }
                                    contentItem: Rectangle {
                                        implicitWidth: 8
                                        radius: 4
                                        color: "#404040"
                                    }
                                }

                                delegate: Rectangle {
                                    width: regionTableView.width - 10
                                    height: 30
                                    color: regionTableView.currentIndex === index ? "#0078d4" : (index % 2 === 0 ? "#1a1a1a" : "#252525")

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
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
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
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
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
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
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
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
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
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
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
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
                                            }
                                        }
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
            Rectangle{
                id: aiAnalysis
                width: 380
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                color: "transparent"
                visible: currentIndex === 4
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
            Rectangle{
                id: reportAnalysis
                width: 380
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                color: "transparent"
                visible: currentIndex === 5
                Column {
                    padding: 10
                    width: parent.width
                    spacing: 20
                    
                    Label{
                        text: qsTr("生成分析报告")
                        color: "#ffffff"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    
                    Label{
                        text: qsTr("将脑网络和脑区分割的数据生成为PDF报告")
                        color: "#cccccc"
                        font.pixelSize: 14
                        width: parent.width - 20
                        wrapMode: Text.WordWrap
                    }
                    
                    Rectangle {
                        width: parent.width - 20
                        height: 1
                        color: "#404040"
                    }
                    
                    Label{
                        text: qsTr("选择报告保存路径：")
                        color: "#ffffff"
                        font.pixelSize: 14
                    }
                    
                    SingleLineTextInput{
                        id: reportSavePath
                        width: parent.width - 20
                        placeholderText: qsTr("点击下方按钮选择保存路径...")
                    }
                    
                    CustomButton{
                        width: parent.width - 20
                        height: 40
                        text: qsTr("选择保存路径")
                        backgroundColor: "#004578"
                        onClicked: {
                            pdfSaveDialog.open()
                        }
                    }
                    
                    Rectangle {
                        width: parent.width - 20
                        height: 1
                        color: "#404040"
                    }
                    
                    Label{
                        text: qsTr("报告将包含以下内容：")
                        color: "#ffffff"
                        font.pixelSize: 14
                    }
                    
                    Column {
                        width: parent.width - 40
                        spacing: 10
                        x: 20
                        
                        Label{
                            text: qsTr("• 脑网络统计信息")
                            color: "#cccccc"
                            font.pixelSize: 12
                        }
                        Label{
                            text: qsTr("• 脑网络区域详细数据表格")
                            color: "#cccccc"
                            font.pixelSize: 12
                        }
                        Label{
                            text: qsTr("• 脑区分割详细数据表格")
                            color: "#cccccc"
                            font.pixelSize: 12
                        }
                    }
                    
                    CustomButton{
                        width: parent.width - 20
                        height: 50
                        text: qsTr("生成 PDF 报告")
                        backgroundColor: "#00875A"
                        fontSize: 16
                        onClicked: {
                            if(reportSavePath.text === ""){
                                if (messageManager) {
                                    messageManager.warning(qsTr("请先选择报告保存路径！"), 2000)
                                }
                                return
                            }
                            
                            $MainViewController.generatePdfReport(reportSavePath.text)
                            
                            if (messageManager) {
                                messageManager.success(qsTr("PDF 报告生成成功！"), 2000)
                            }
                        }
                    }
                    
                    Label{
                        text: qsTr("提示：生成报告前请确保已完成脑网络分析和脑区分割")
                        color: "#888888"
                        font.pixelSize: 11
                        width: parent.width - 20
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
