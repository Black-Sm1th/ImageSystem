//#include "DicomNetwork.h"
//
//#include <QDir>
//#include <QDebug>
//#include <QDateTime>
//#include <atomic>
//#include <functional>
//
//// DCMTK headers
//#include "dcmtk/config/osconfig.h"
//#include "dcmtk/dcmnet/scu.h"
//#include "dcmtk/dcmnet/scp.h"
//#include "dcmtk/dcmnet/diutil.h"
//#include "dcmtk/dcmdata/dcdatset.h"
//#include "dcmtk/dcmdata/dcfilefo.h"
//#include "dcmtk/dcmdata/dcdeftag.h"
//#include "dcmtk/dcmdata/dcuid.h"
//#include "dcmtk/dcmdata/dcmetinf.h"
//
//// ============================================================================
//// DicomNetwork Implementation
//// ============================================================================
//
//DicomNetwork::DicomNetwork(QObject* parent)
//    : QObject(parent)
//{
//    m_storageDirectory = QDir::tempPath() + "/DicomReceived";
//}
//
//DicomNetwork::~DicomNetwork()
//{
//    stopStoreScp();
//}
//
//void DicomNetwork::setLocalAeTitle(const QString& aeTitle)
//{
//    if (m_localAeTitle != aeTitle) {
//        m_localAeTitle = aeTitle;
//        emit localAeTitleChanged();
//    }
//}
//
//void DicomNetwork::setRemoteAeTitle(const QString& aeTitle)
//{
//    if (m_remoteAeTitle != aeTitle) {
//        m_remoteAeTitle = aeTitle;
//        emit remoteAeTitleChanged();
//    }
//}
//
//void DicomNetwork::setRemoteHost(const QString& host)
//{
//    if (m_remoteHost != host) {
//        m_remoteHost = host;
//        emit remoteHostChanged();
//    }
//}
//
//void DicomNetwork::setRemotePort(int port)
//{
//    if (m_remotePort != port) {
//        m_remotePort = port;
//        emit remotePortChanged();
//    }
//}
//
//void DicomNetwork::setLocalPort(int port)
//{
//    if (m_localPort != port) {
//        m_localPort = port;
//        emit localPortChanged();
//    }
//}
//
//void DicomNetwork::setStorageDirectory(const QString& dir)
//{
//    if (m_storageDirectory != dir) {
//        m_storageDirectory = dir;
//        QDir().mkpath(dir);
//        emit storageDirectoryChanged();
//    }
//}
//
//bool DicomNetwork::initScu(DcmSCU& scu)
//{
//    scu.setAETitle(m_localAeTitle.toStdString().c_str());
//    scu.setPeerAETitle(m_remoteAeTitle.toStdString().c_str());
//    scu.setPeerHostName(m_remoteHost.toStdString().c_str());
//    scu.setPeerPort(static_cast<Uint16>(m_remotePort));
//
//    scu.setACSETimeout(30);
//    scu.setDIMSETimeout(60);
//    scu.setConnectionTimeout(30);
//
//    return true;
//}
//
//// ============================================================================
//// C-ECHO Implementation
//// ============================================================================
//
//bool DicomNetwork::cEcho()
//{
//    qDebug() << "[DicomNetwork] C-ECHO to" << m_remoteHost << ":" << m_remotePort;
//
//    DcmSCU scu;
//    initScu(scu);
//
//    OFList<OFString> transferSyntaxes;
//    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
//    scu.addPresentationContext(UID_VerificationSOPClass, transferSyntaxes);
//
//    OFCondition result = scu.initNetwork();
//    if (result.bad()) {
//        QString error = QString("Init network failed: %1").arg(result.text());
//        qWarning() << error;
//        emit echoCompleted(false, error);
//        emit errorOccurred(error);
//        return false;
//    }
//
//    result = scu.negotiateAssociation();
//    if (result.bad()) {
//        QString error = QString("Negotiate association failed: %1").arg(result.text());
//        qWarning() << error;
//        emit echoCompleted(false, error);
//        emit errorOccurred(error);
//        return false;
//    }
//
//    result = scu.sendECHORequest(0);
//    bool success = result.good();
//
//    scu.releaseAssociation();
//
//    QString message = success ? "C-ECHO success" : QString("C-ECHO failed: %1").arg(result.text());
//    qDebug() << message;
//    emit echoCompleted(success, message);
//
//    return success;
//}
//
//// ============================================================================
//// C-FIND Implementation
//// ============================================================================
//
//QList<QVariantMap> DicomNetwork::findPatients(const QString& patientId, const QString& patientName)
//{
//    qDebug() << "[DicomNetwork] C-FIND Patients:" << patientId << patientName;
//    QList<QVariantMap> results;
//
//    DcmSCU scu;
//    initScu(scu);
//
//    OFList<OFString> transferSyntaxes;
//    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
//    scu.addPresentationContext(UID_FINDPatientRootQueryRetrieveInformationModel, transferSyntaxes);
//
//    if (scu.initNetwork().bad() || scu.negotiateAssociation().bad()) {
//        emit errorOccurred("Cannot connect to PACS server");
//        return results;
//    }
//
//    DcmDataset query;
//    query.putAndInsertString(DCM_QueryRetrieveLevel, "PATIENT");
//    query.putAndInsertString(DCM_PatientID, patientId.toStdString().c_str());
//    query.putAndInsertString(DCM_PatientName, patientName.toStdString().c_str());
//    query.putAndInsertString(DCM_PatientBirthDate, "");
//    query.putAndInsertString(DCM_PatientSex, "");
//
//    OFList<QRResponse*> responses;
//    T_ASC_PresentationContextID pcid = scu.findPresentationContextID(
//        UID_FINDPatientRootQueryRetrieveInformationModel, "");
//    
//    OFCondition result = scu.sendFINDRequest(pcid, &query, &responses);
//    
//    if (result.good()) {
//        for (OFListIterator(QRResponse*) it = responses.begin(); it != responses.end(); ++it) {
//            if ((*it)->m_dataset != nullptr) {
//                QVariantMap item;
//                OFString value;
//                
//                if ((*it)->m_dataset->findAndGetOFString(DCM_PatientID, value).good())
//                    item["patientId"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_PatientName, value).good())
//                    item["patientName"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_PatientBirthDate, value).good())
//                    item["patientBirthDate"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_PatientSex, value).good())
//                    item["patientSex"] = QString::fromStdString(value.c_str());
//                
//                results.append(item);
//            }
//            delete *it;
//        }
//    }
//
//    scu.releaseAssociation();
//    emit findCompleted(results.size(), QString("Found %1 patients").arg(results.size()));
//    return results;
//}
//
//QList<QVariantMap> DicomNetwork::findStudies(const QString& patientId, 
//                                              const QString& studyDate, 
//                                              const QString& modality)
//{
//    qDebug() << "[DicomNetwork] C-FIND Studies:" << patientId << studyDate << modality;
//    QList<QVariantMap> results;
//
//    DcmSCU scu;
//    initScu(scu);
//
//    OFList<OFString> transferSyntaxes;
//    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
//    scu.addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, transferSyntaxes);
//
//    if (scu.initNetwork().bad() || scu.negotiateAssociation().bad()) {
//        emit errorOccurred("Cannot connect to PACS server");
//        return results;
//    }
//
//    DcmDataset query;
//    query.putAndInsertString(DCM_QueryRetrieveLevel, "STUDY");
//    query.putAndInsertString(DCM_PatientID, patientId.toStdString().c_str());
//    query.putAndInsertString(DCM_PatientName, "");
//    query.putAndInsertString(DCM_StudyDate, studyDate.toStdString().c_str());
//    query.putAndInsertString(DCM_ModalitiesInStudy, modality.toStdString().c_str());
//    query.putAndInsertString(DCM_StudyInstanceUID, "");
//    query.putAndInsertString(DCM_StudyDescription, "");
//    query.putAndInsertString(DCM_AccessionNumber, "");
//    query.putAndInsertString(DCM_NumberOfStudyRelatedSeries, "");
//    query.putAndInsertString(DCM_NumberOfStudyRelatedInstances, "");
//
//    OFList<QRResponse*> responses;
//    T_ASC_PresentationContextID pcid = scu.findPresentationContextID(
//        UID_FINDStudyRootQueryRetrieveInformationModel, "");
//    
//    OFCondition result = scu.sendFINDRequest(pcid, &query, &responses);
//    
//    if (result.good()) {
//        for (OFListIterator(QRResponse*) it = responses.begin(); it != responses.end(); ++it) {
//            if ((*it)->m_dataset != nullptr) {
//                QVariantMap item;
//                OFString value;
//                
//                if ((*it)->m_dataset->findAndGetOFString(DCM_PatientID, value).good())
//                    item["patientId"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_PatientName, value).good())
//                    item["patientName"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_StudyInstanceUID, value).good())
//                    item["studyInstanceUid"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_StudyDate, value).good())
//                    item["studyDate"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_StudyDescription, value).good())
//                    item["studyDescription"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_AccessionNumber, value).good())
//                    item["accessionNumber"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_ModalitiesInStudy, value).good())
//                    item["modality"] = QString::fromStdString(value.c_str());
//                
//                results.append(item);
//            }
//            delete *it;
//        }
//    }
//
//    scu.releaseAssociation();
//    emit findCompleted(results.size(), QString("Found %1 studies").arg(results.size()));
//    return results;
//}
//
//QList<QVariantMap> DicomNetwork::findSeries(const QString& studyInstanceUid)
//{
//    qDebug() << "[DicomNetwork] C-FIND Series:" << studyInstanceUid;
//    QList<QVariantMap> results;
//
//    DcmSCU scu;
//    initScu(scu);
//
//    OFList<OFString> transferSyntaxes;
//    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
//    scu.addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, transferSyntaxes);
//
//    if (scu.initNetwork().bad() || scu.negotiateAssociation().bad()) {
//        emit errorOccurred("Cannot connect to PACS server");
//        return results;
//    }
//
//    DcmDataset query;
//    query.putAndInsertString(DCM_QueryRetrieveLevel, "SERIES");
//    query.putAndInsertString(DCM_StudyInstanceUID, studyInstanceUid.toStdString().c_str());
//    query.putAndInsertString(DCM_SeriesInstanceUID, "");
//    query.putAndInsertString(DCM_SeriesNumber, "");
//    query.putAndInsertString(DCM_SeriesDescription, "");
//    query.putAndInsertString(DCM_Modality, "");
//    query.putAndInsertString(DCM_NumberOfSeriesRelatedInstances, "");
//
//    OFList<QRResponse*> responses;
//    T_ASC_PresentationContextID pcid = scu.findPresentationContextID(
//        UID_FINDStudyRootQueryRetrieveInformationModel, "");
//    
//    OFCondition result = scu.sendFINDRequest(pcid, &query, &responses);
//    
//    if (result.good()) {
//        for (OFListIterator(QRResponse*) it = responses.begin(); it != responses.end(); ++it) {
//            if ((*it)->m_dataset != nullptr) {
//                QVariantMap item;
//                OFString value;
//                
//                if ((*it)->m_dataset->findAndGetOFString(DCM_SeriesInstanceUID, value).good())
//                    item["seriesInstanceUid"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_SeriesNumber, value).good())
//                    item["seriesNumber"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_SeriesDescription, value).good())
//                    item["seriesDescription"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_Modality, value).good())
//                    item["modality"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_NumberOfSeriesRelatedInstances, value).good())
//                    item["numberOfImages"] = QString::fromStdString(value.c_str()).toInt();
//                
//                results.append(item);
//            }
//            delete *it;
//        }
//    }
//
//    scu.releaseAssociation();
//    emit findCompleted(results.size(), QString("Found %1 series").arg(results.size()));
//    return results;
//}
//
//QList<QVariantMap> DicomNetwork::findImages(const QString& seriesInstanceUid)
//{
//    qDebug() << "[DicomNetwork] C-FIND Images:" << seriesInstanceUid;
//    QList<QVariantMap> results;
//
//    DcmSCU scu;
//    initScu(scu);
//
//    OFList<OFString> transferSyntaxes;
//    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
//    scu.addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, transferSyntaxes);
//
//    if (scu.initNetwork().bad() || scu.negotiateAssociation().bad()) {
//        emit errorOccurred("Cannot connect to PACS server");
//        return results;
//    }
//
//    DcmDataset query;
//    query.putAndInsertString(DCM_QueryRetrieveLevel, "IMAGE");
//    query.putAndInsertString(DCM_SeriesInstanceUID, seriesInstanceUid.toStdString().c_str());
//    query.putAndInsertString(DCM_SOPInstanceUID, "");
//    query.putAndInsertString(DCM_InstanceNumber, "");
//    query.putAndInsertString(DCM_SOPClassUID, "");
//
//    OFList<QRResponse*> responses;
//    T_ASC_PresentationContextID pcid = scu.findPresentationContextID(
//        UID_FINDStudyRootQueryRetrieveInformationModel, "");
//    
//    OFCondition result = scu.sendFINDRequest(pcid, &query, &responses);
//    
//    if (result.good()) {
//        for (OFListIterator(QRResponse*) it = responses.begin(); it != responses.end(); ++it) {
//            if ((*it)->m_dataset != nullptr) {
//                QVariantMap item;
//                OFString value;
//                
//                if ((*it)->m_dataset->findAndGetOFString(DCM_SOPInstanceUID, value).good())
//                    item["sopInstanceUid"] = QString::fromStdString(value.c_str());
//                if ((*it)->m_dataset->findAndGetOFString(DCM_InstanceNumber, value).good())
//                    item["instanceNumber"] = QString::fromStdString(value.c_str()).toInt();
//                if ((*it)->m_dataset->findAndGetOFString(DCM_SOPClassUID, value).good())
//                    item["sopClassUid"] = QString::fromStdString(value.c_str());
//                
//                results.append(item);
//            }
//            delete *it;
//        }
//    }
//
//    scu.releaseAssociation();
//    emit findCompleted(results.size(), QString("Found %1 images").arg(results.size()));
//    return results;
//}
//
//// ============================================================================
//// C-GET Implementation
//// ============================================================================
//
//bool DicomNetwork::cGetStudy(const QString& studyInstanceUid)
//{
//    qDebug() << "[DicomNetwork] C-GET Study:" << studyInstanceUid;
//
//    DcmSCU scu;
//    initScu(scu);
//    scu.setStorageDir(m_storageDirectory.toStdString().c_str());
//
//    OFList<OFString> transferSyntaxes;
//    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
//    scu.addPresentationContext(UID_GETStudyRootQueryRetrieveInformationModel, transferSyntaxes);
//
//    scu.addPresentationContext(UID_CTImageStorage, transferSyntaxes);
//    scu.addPresentationContext(UID_MRImageStorage, transferSyntaxes);
//    scu.addPresentationContext(UID_SecondaryCaptureImageStorage, transferSyntaxes);
//    scu.addPresentationContext(UID_DigitalXRayImageStorageForPresentation, transferSyntaxes);
//    scu.addPresentationContext(UID_UltrasoundImageStorage, transferSyntaxes);
//
//    if (scu.initNetwork().bad() || scu.negotiateAssociation().bad()) {
//        emit errorOccurred("Cannot connect to PACS server");
//        return false;
//    }
//
//    DcmDataset query;
//    query.putAndInsertString(DCM_QueryRetrieveLevel, "STUDY");
//    query.putAndInsertString(DCM_StudyInstanceUID, studyInstanceUid.toStdString().c_str());
//
//    T_ASC_PresentationContextID pcid = scu.findPresentationContextID(
//        UID_GETStudyRootQueryRetrieveInformationModel, "");
//
//    OFList<RetrieveResponse*> responses;
//    OFCondition result = scu.sendCGETRequest(pcid, &query, &responses);
//
//    int imageCount = 0;
//    for (OFListIterator(RetrieveResponse*) it = responses.begin(); it != responses.end(); ++it) {
//        if ((*it)->m_numberOfCompletedSubops > 0) {
//            imageCount += (*it)->m_numberOfCompletedSubops;
//        }
//        delete *it;
//    }
//
//    scu.releaseAssociation();
//
//    bool success = result.good();
//    emit retrieveCompleted(success, imageCount, 
//        success ? QString("Retrieved %1 images").arg(imageCount) : QString("C-GET failed: %1").arg(result.text()));
//    
//    return success;
//}
//
//bool DicomNetwork::cGetSeries(const QString& seriesInstanceUid)
//{
//    qDebug() << "[DicomNetwork] C-GET Series:" << seriesInstanceUid;
//
//    DcmSCU scu;
//    initScu(scu);
//    scu.setStorageDir(m_storageDirectory.toStdString().c_str());
//
//    OFList<OFString> transferSyntaxes;
//    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
//    scu.addPresentationContext(UID_GETStudyRootQueryRetrieveInformationModel, transferSyntaxes);
//    scu.addPresentationContext(UID_CTImageStorage, transferSyntaxes);
//    scu.addPresentationContext(UID_MRImageStorage, transferSyntaxes);
//    scu.addPresentationContext(UID_SecondaryCaptureImageStorage, transferSyntaxes);
//
//    if (scu.initNetwork().bad() || scu.negotiateAssociation().bad()) {
//        emit errorOccurred("Cannot connect to PACS server");
//        return false;
//    }
//
//    DcmDataset query;
//    query.putAndInsertString(DCM_QueryRetrieveLevel, "SERIES");
//    query.putAndInsertString(DCM_SeriesInstanceUID, seriesInstanceUid.toStdString().c_str());
//
//    T_ASC_PresentationContextID pcid = scu.findPresentationContextID(
//        UID_GETStudyRootQueryRetrieveInformationModel, "");
//
//    OFList<RetrieveResponse*> responses;
//    OFCondition result = scu.sendCGETRequest(pcid, &query, &responses);
//
//    int imageCount = 0;
//    for (OFListIterator(RetrieveResponse*) it = responses.begin(); it != responses.end(); ++it) {
//        if ((*it)->m_numberOfCompletedSubops > 0) {
//            imageCount += (*it)->m_numberOfCompletedSubops;
//        }
//        delete *it;
//    }
//
//    scu.releaseAssociation();
//
//    bool success = result.good();
//    emit retrieveCompleted(success, imageCount, 
//        success ? QString("Retrieved %1 images").arg(imageCount) : QString("C-GET failed: %1").arg(result.text()));
//    
//    return success;
//}
//
//// ============================================================================
//// C-MOVE Implementation
//// ============================================================================
//
//bool DicomNetwork::cMoveStudy(const QString& studyInstanceUid, const QString& destinationAeTitle)
//{
//    QString destAe = destinationAeTitle.isEmpty() ? m_localAeTitle : destinationAeTitle;
//    qDebug() << "[DicomNetwork] C-MOVE Study:" << studyInstanceUid << "to" << destAe;
//
//    DcmSCU scu;
//    initScu(scu);
//
//    OFList<OFString> transferSyntaxes;
//    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
//    scu.addPresentationContext(UID_MOVEStudyRootQueryRetrieveInformationModel, transferSyntaxes);
//
//    if (scu.initNetwork().bad() || scu.negotiateAssociation().bad()) {
//        emit errorOccurred("Cannot connect to PACS server");
//        return false;
//    }
//
//    DcmDataset query;
//    query.putAndInsertString(DCM_QueryRetrieveLevel, "STUDY");
//    query.putAndInsertString(DCM_StudyInstanceUID, studyInstanceUid.toStdString().c_str());
//
//    T_ASC_PresentationContextID pcid = scu.findPresentationContextID(
//        UID_MOVEStudyRootQueryRetrieveInformationModel, "");
//
//    OFList<RetrieveResponse*> responses;
//    OFCondition result = scu.sendMOVERequest(pcid, destAe.toStdString().c_str(), &query, &responses);
//
//    int imageCount = 0;
//    for (OFListIterator(RetrieveResponse*) it = responses.begin(); it != responses.end(); ++it) {
//        if ((*it)->m_numberOfCompletedSubops > 0) {
//            imageCount += (*it)->m_numberOfCompletedSubops;
//        }
//        delete *it;
//    }
//
//    scu.releaseAssociation();
//
//    bool success = result.good();
//    emit retrieveCompleted(success, imageCount, 
//        success ? QString("C-MOVE sent, expecting %1 images").arg(imageCount) : QString("C-MOVE failed: %1").arg(result.text()));
//    
//    return success;
//}
//
//bool DicomNetwork::cMoveSeries(const QString& seriesInstanceUid, const QString& destinationAeTitle)
//{
//    QString destAe = destinationAeTitle.isEmpty() ? m_localAeTitle : destinationAeTitle;
//    qDebug() << "[DicomNetwork] C-MOVE Series:" << seriesInstanceUid << "to" << destAe;
//
//    DcmSCU scu;
//    initScu(scu);
//
//    OFList<OFString> transferSyntaxes;
//    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
//    scu.addPresentationContext(UID_MOVEStudyRootQueryRetrieveInformationModel, transferSyntaxes);
//
//    if (scu.initNetwork().bad() || scu.negotiateAssociation().bad()) {
//        emit errorOccurred("Cannot connect to PACS server");
//        return false;
//    }
//
//    DcmDataset query;
//    query.putAndInsertString(DCM_QueryRetrieveLevel, "SERIES");
//    query.putAndInsertString(DCM_SeriesInstanceUID, seriesInstanceUid.toStdString().c_str());
//
//    T_ASC_PresentationContextID pcid = scu.findPresentationContextID(
//        UID_MOVEStudyRootQueryRetrieveInformationModel, "");
//
//    OFList<RetrieveResponse*> responses;
//    OFCondition result = scu.sendMOVERequest(pcid, destAe.toStdString().c_str(), &query, &responses);
//
//    int imageCount = 0;
//    for (OFListIterator(RetrieveResponse*) it = responses.begin(); it != responses.end(); ++it) {
//        if ((*it)->m_numberOfCompletedSubops > 0) {
//            imageCount += (*it)->m_numberOfCompletedSubops;
//        }
//        delete *it;
//    }
//
//    scu.releaseAssociation();
//
//    bool success = result.good();
//    emit retrieveCompleted(success, imageCount, 
//        success ? QString("C-MOVE sent, expecting %1 images").arg(imageCount) : QString("C-MOVE failed: %1").arg(result.text()));
//    
//    return success;
//}
//
//// ============================================================================
//// C-STORE SCP Implementation
//// ============================================================================
//
//bool DicomNetwork::startStoreScp()
//{
//    if (m_scpThread && m_scpThread->isRunning()) {
//        qWarning() << "SCP already running";
//        return true;
//    }
//
//    QDir().mkpath(m_storageDirectory);
//
//    m_scpThread = std::make_unique<StorageScpThread>(m_localAeTitle, m_localPort, m_storageDirectory, this);
//    
//    connect(m_scpThread.get(), &StorageScpThread::imageReceived, this, &DicomNetwork::imageReceived);
//    connect(m_scpThread.get(), &StorageScpThread::errorOccurred, this, &DicomNetwork::errorOccurred);
//    connect(m_scpThread.get(), &QThread::finished, this, [this]() {
//        emit storeScpStatusChanged(false, m_localPort);
//    });
//
//    m_scpThread->start();
//    emit storeScpStatusChanged(true, m_localPort);
//    qDebug() << "[DicomNetwork] SCP started on port" << m_localPort;
//    return true;
//}
//
//void DicomNetwork::stopStoreScp()
//{
//    if (m_scpThread && m_scpThread->isRunning()) {
//        m_scpThread->stopServer();
//        m_scpThread->wait(5000);
//        m_scpThread.reset();
//        qDebug() << "[DicomNetwork] SCP stopped";
//    }
//}
//
//bool DicomNetwork::isScpRunning() const
//{
//    return m_scpThread && m_scpThread->isRunning();
//}
//
//// ============================================================================
//// Custom SCP class derived from DcmSCP to handle C-STORE requests
//// ============================================================================
//
//class MyStorageSCP : public DcmSCP
//{
//public:
//    MyStorageSCP(const QString& storageDir, std::function<void(const QString&)> onImageReceived)
//        : m_storageDir(storageDir)
//        , m_onImageReceived(onImageReceived)
//        , m_stopFlag(false)
//    {
//    }
//
//    void requestStop() 
//    { 
//        m_stopFlag = true; 
//        stopAfterCurrentAssociation();
//    }
//    
//    bool stopRequested() const { return m_stopFlag; }
//
//protected:
//    void notifyAssociationTermination() override
//    {
//        DcmSCP::notifyAssociationTermination();
//        if (m_stopFlag) {
//            stopAfterCurrentAssociation();
//        }
//    }
//
//    OFCondition handleIncomingCommand(T_DIMSE_Message* incomingMsg,
//                                       const DcmPresentationContextInfo& presInfo) override
//    {
//        if (incomingMsg == nullptr) {
//            return EC_IllegalParameter;
//        }
//
//        OFCondition result = EC_Normal;
//        
//        // Handle C-ECHO request - call base class handleECHORequest
//        if (incomingMsg->CommandField == DIMSE_C_ECHO_RQ) {
//            qDebug() << "[StorageSCP] Received C-ECHO request";
//            result = DcmSCP::handleIncomingCommand(incomingMsg, presInfo);
//        }
//        // Handle C-STORE request
//        else if (incomingMsg->CommandField == DIMSE_C_STORE_RQ) {
//            result = handleSTORERequest(incomingMsg->msg.CStoreRQ, presInfo.presentationContextID);
//        }
//        else {
//            result = DcmSCP::handleIncomingCommand(incomingMsg, presInfo);
//        }
//        
//        return result;
//    }
//
//private:
//    OFCondition handleSTORERequest(T_DIMSE_C_StoreRQ& req, T_ASC_PresentationContextID presID)
//    {
//        OFCondition result = EC_Normal;
//        DcmDataset* dataset = nullptr;
//        
//        result = receiveDIMSEDataset(&presID, &dataset);
//        
//        Uint16 rspStatus = STATUS_Success;
//        
//        if (result.good() && dataset != nullptr) {
//            OFString sopInstanceUid;
//            dataset->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUid);
//            
//            QString fileName = QString::fromStdString(sopInstanceUid.c_str()) + ".dcm";
//            QString filePath = m_storageDir + "/" + fileName;
//            
//            QDir().mkpath(m_storageDir);
//            
//            DcmFileFormat fileFormat(dataset);
//            OFCondition saveResult = fileFormat.saveFile(filePath.toStdString().c_str(), 
//                                                          EXS_LittleEndianExplicit);
//            
//            if (saveResult.good()) {
//                qDebug() << "[StorageSCP] Saved:" << filePath;
//                if (m_onImageReceived) {
//                    m_onImageReceived(filePath);
//                }
//            } else {
//                qWarning() << "[StorageSCP] Failed to save:" << saveResult.text();
//                rspStatus = STATUS_STORE_Error_CannotUnderstand;
//            }
//        } else {
//            rspStatus = STATUS_STORE_Error_CannotUnderstand;
//        }
//        
//        result = sendSTOREResponse(presID, req, rspStatus);
//        
//        delete dataset;
//        return result;
//    }
//
//    QString m_storageDir;
//    std::function<void(const QString&)> m_onImageReceived;
//    std::atomic<bool> m_stopFlag;
//};
//
//// ============================================================================
//// StorageScpThread Implementation
//// ============================================================================
//
//DicomNetwork::StorageScpThread::StorageScpThread(const QString& aeTitle, int port, 
//                                                  const QString& storageDir, QObject* parent)
//    : QThread(parent)
//    , m_aeTitle(aeTitle)
//    , m_port(port)
//    , m_storageDir(storageDir)
//{
//}
//
//DicomNetwork::StorageScpThread::~StorageScpThread()
//{
//    stopServer();
//    wait();
//}
//
//void DicomNetwork::StorageScpThread::stopServer()
//{
//    m_stopRequested = true;
//    if (m_scp) {
//        m_scp->requestStop();
//    }
//}
//
//void DicomNetwork::StorageScpThread::run()
//{
//    qDebug() << "[StorageSCP] Starting on port" << m_port << "AE:" << m_aeTitle;
//
//    m_scp = std::make_unique<MyStorageSCP>(m_storageDir, [this](const QString& filePath) {
//        emit imageReceived(filePath);
//    });
//    
//    m_scp->setAETitle(m_aeTitle.toStdString().c_str());
//    m_scp->setPort(static_cast<Uint16>(m_port));
//    
//    m_scp->setConnectionTimeout(1);
//    m_scp->setACSETimeout(30);
//    m_scp->setDIMSETimeout(60);
//    
//    OFList<OFString> transferSyntaxes;
//    transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
//    transferSyntaxes.push_back(UID_BigEndianExplicitTransferSyntax);
//    
//    m_scp->addPresentationContext(UID_VerificationSOPClass, transferSyntaxes);
//    m_scp->addPresentationContext(UID_CTImageStorage, transferSyntaxes);
//    m_scp->addPresentationContext(UID_MRImageStorage, transferSyntaxes);
//    m_scp->addPresentationContext(UID_SecondaryCaptureImageStorage, transferSyntaxes);
//    m_scp->addPresentationContext(UID_DigitalXRayImageStorageForPresentation, transferSyntaxes);
//    m_scp->addPresentationContext(UID_UltrasoundImageStorage, transferSyntaxes);
//    m_scp->addPresentationContext(UID_UltrasoundMultiframeImageStorage, transferSyntaxes);
//    m_scp->addPresentationContext(UID_NuclearMedicineImageStorage, transferSyntaxes);
//    m_scp->addPresentationContext(UID_PositronEmissionTomographyImageStorage, transferSyntaxes);
//    m_scp->addPresentationContext(UID_EnhancedCTImageStorage, transferSyntaxes);
//    m_scp->addPresentationContext(UID_EnhancedMRImageStorage, transferSyntaxes);
//
//    qDebug() << "[StorageSCP] Listening on port" << m_port;
//
//    OFCondition result = m_scp->listen();
//    
//    if (result.bad() && !m_stopRequested) {
//        emit errorOccurred(QString("SCP listen failed: %1").arg(result.text()));
//    }
//
//    m_scp.reset();
//    qDebug() << "[StorageSCP] Stopped";
//}
