#include "PageDriverManager.h"
#include "drivertableview.h"
#include "DetectedStatusWidget.h"
#include "MacroDefinition.h"
#include "driveritem.h"
#include "DriverScanWidget.h"
#include "PageDriverTableView.h"
#include "DBusDriverInterface.h"
#include "DriverScanner.h"
#include "DeviceManager.h"
#include "DevicePrint.h"
#include "DeviceNetwork.h"
#include "commontools.h"

#include <DScrollArea>
#include <DApplicationHelper>
#include <DApplication>

#include <QBoxLayout>
#include <QMetaType>

#include <unistd.h>



PageDriverManager::PageDriverManager(DWidget *parent)
    : DWidget(parent)
    , mp_StackWidget(new DStackedWidget(this))
    , mp_ViewNotInstall(new PageDriverTableView(this))
    , mp_ViewCanUpdate(new PageDriverTableView(this))
    , mp_AllDriverIsNew(new PageDriverTableView(this))
    , mp_HeadWidget(new DetectedStatusWidget(this))
    , mp_ScanWidget(new DriverScanWidget(this))
    , mp_InstallLabel(new DLabel(this))
    , mp_UpdateLabel(new DLabel(this))
    , mp_LabelIsNew(new DLabel(this))
    , mp_CurDriverInfo(nullptr)
    , mp_scanner(new DriverScanner(this))
{
    // 初始化界面
    initWidget();
    qRegisterMetaType<QList<DriverInfo *>>("QList<DriverInfo*>");
    qRegisterMetaType<ScanResult>("ScanResult");

    // 连接dbus处理信号
    connect(DBusDriverInterface::getInstance(), &DBusDriverInterface::downloadProgressChanged, this, &PageDriverManager::slotDownloadProgressChanged);
    connect(DBusDriverInterface::getInstance(), &DBusDriverInterface::downloadFinished, this, &PageDriverManager::slotDownloadFinished);
    connect(DBusDriverInterface::getInstance(), &DBusDriverInterface::installProgressChanged, this, &PageDriverManager::slotInstallProgressChanged);
    connect(DBusDriverInterface::getInstance(), &DBusDriverInterface::installProgressFinished, this, &PageDriverManager::slotInstallProgressFinished);
    connect(mp_ViewNotInstall, &PageDriverTableView::operatorClicked, this, &PageDriverManager::slotDriverOperationClicked);
    connect(mp_ViewCanUpdate, &PageDriverTableView::operatorClicked, this, &PageDriverManager::slotDriverOperationClicked);
    connect(mp_ViewNotInstall, &PageDriverTableView::itemChecked, this, &PageDriverManager::slotItemCheckedClicked);
    connect(mp_ViewCanUpdate, &PageDriverTableView::itemChecked, this, &PageDriverManager::slotItemCheckedClicked);
    connect(mp_HeadWidget, &DetectedStatusWidget::installAll, this, &PageDriverManager::slotInstallAllDrivers);
    connect(mp_HeadWidget, &DetectedStatusWidget::undoInstall, this, &PageDriverManager::slotUndoInstall);
    connect(mp_HeadWidget, &DetectedStatusWidget::redetected, this, &PageDriverManager::scanDriverInfo);
    connect(mp_ScanWidget, &DriverScanWidget::redetected, this, &PageDriverManager::scanDriverInfo);

    // 将扫描动作放到线程里面
    connect(mp_scanner, &DriverScanner::scanInfo, this, &PageDriverManager::slotScanInfo);
    connect(mp_scanner, &DriverScanner::scanFinished, this, &PageDriverManager::slotScanFinished);
}

PageDriverManager::~PageDriverManager()
{
    DELETE_PTR(mp_ViewNotInstall);
    DELETE_PTR(mp_ViewCanUpdate);
    DELETE_PTR(mp_AllDriverIsNew);
    DELETE_PTR(mp_HeadWidget);

    DELETE_PTR(mp_ScanWidget);
    DELETE_PTR(mp_InstallLabel);
    DELETE_PTR(mp_UpdateLabel);
    DELETE_PTR(mp_LabelIsNew);


    DELETE_PTR(mp_StackWidget);
}

void PageDriverManager::addDriverInfo(DriverInfo *info)
{
    m_ListDriverInfo.append(info);
}

bool PageDriverManager::isFirstScan()
{
    return m_ListNewIndex.isEmpty() && m_ListUpdateIndex.isEmpty() && m_ListInstallIndex.isEmpty();
}

bool PageDriverManager::isInstalling()
{
    return (mp_ViewCanUpdate->hasItemDisabled() || mp_ViewNotInstall->hasItemDisabled()) && m_ListDriverIndex.size() > 0;
}

void PageDriverManager::scanDriverInfo()
{
    // 如果在安装过程中则不扫描
    bool install = mp_scanner->isRunning() || ((mp_ViewCanUpdate->hasItemDisabled() || mp_ViewNotInstall->hasItemDisabled()) && m_ListDriverIndex.size() > 0);
    if (install) {
        return;
    }

    // 重新扫描之前清空所有数据
    clearAllData();

    // 获取需要扫描的设备的信息
    scanDevices();
//    testScanDevices();

    // 扫描驱动信息线程
    mp_scanner->setDriverList(m_ListDriverInfo);
    mp_scanner->start();

    // 切换到到扫描的界面
    mp_StackWidget->setCurrentIndex(0);
    mp_ScanWidget->setScanningUI("", 0);
}

void PageDriverManager::slotDriverOperationClicked(int index)
{
    // 如果已经在安装过程中，则直接添加到list
    // 如果不在安装过程中则需要安装
    addToDriverIndex(index);
    bool install = (mp_ViewCanUpdate->hasItemDisabled() || mp_ViewNotInstall->hasItemDisabled()) && m_ListDriverIndex.size() > 0;
    if (install) {
        installNextDriver();
    }
}

void PageDriverManager::slotItemCheckedClicked(int index, bool checked)
{
    if (!checked) {
        // 取消选中则从list中删除
        removeFromDriverIndex(index);
    } else {
        // 获取是否在安装过程中
        bool install = (mp_ViewCanUpdate->hasItemDisabled() || mp_ViewNotInstall->hasItemDisabled()) && m_ListDriverIndex.size() > 0;

        // 如果不在安装过程中则直接添加到list
        // 如果在安装过程中：1. 添加到list  2. 将item置灰
        addToDriverIndex(index);
        if (install) {
            mp_ViewCanUpdate->setCheckedCBDisnable();
            mp_ViewCanUpdate->setCheckedCBDisnable();
        }
    }
}

void PageDriverManager::slotDownloadProgressChanged(QStringList msg)
{
    // 将下载过程时时更新到表格上方的状态里面 qInfo() << "Download ********** " << msg[0] << " , " << msg[1] << " , " << msg[2];
    mp_HeadWidget->setDownloadUI(mp_CurDriverInfo->type(), msg[2], msg[1], mp_CurDriverInfo->size(), msg[0].toInt());

    // 设置表格下载中的状态
}

void PageDriverManager::slotDownloadFinished()
{
    mp_CurDriverInfo->m_Status = ST_INSTALL;
    mp_ViewCanUpdate->setItemStatus(m_ListDriverIndex[0], mp_CurDriverInfo->status());
    mp_ViewNotInstall->setItemStatus(m_ListDriverIndex[0], mp_CurDriverInfo->status());
}

void PageDriverManager::slotInstallProgressChanged(int progress)
{
    // 设置表头状态
    mp_HeadWidget->setInstallUI(mp_CurDriverInfo->type(), mp_CurDriverInfo->name(), progress);

    // 设置表格安装中的状态
}

void PageDriverManager::slotInstallProgressFinished(bool bsuccess, int err)
{
    static int successNum = 0;
    static int failedNum = 0;
    if (bsuccess) {
        successNum += 1;
    } else {
        failedNum += 1;
    }

    int index = m_ListDriverIndex[0];

    // 安装结束一个，就从m_ListDriverIndex里面pop掉一个
    m_ListDriverIndex.pop_front();

    // 安装结束后，对应的表格需要设置相应的状态
    mp_CurDriverInfo->m_Status = bsuccess ? ST_SUCESS : ST_FAILED;
    mp_ViewCanUpdate->setItemStatus(index, mp_CurDriverInfo->status());
    mp_ViewNotInstall->setItemStatus(index, mp_CurDriverInfo->status());
    mp_ViewCanUpdate->setErrorMsg(index, CommonTools::getErrorString(err));
    mp_ViewNotInstall->setErrorMsg(index, CommonTools::getErrorString(err));


    // 当前驱动安装结束，如果没有其它驱动需要安装，则显示安装结果
    // 如果有其它驱动，则显示安装内容
    if (m_ListDriverIndex.size() > 0) {
        sleep(1);
        installNextDriver();
    } else {
        // 设置头部显示效果
        mp_HeadWidget->setInstallSuccessUI(QString::number(successNum), QString::number(failedNum));
        successNum = 0;
        failedNum = 0;
    }
}

void PageDriverManager::slotInstallAllDrivers()
{
    // 安装过程中，所有已经选中的勾选框置灰
    mp_ViewNotInstall->setCheckedCBDisnable();
    mp_ViewCanUpdate->setCheckedCBDisnable();

    // 开始安装驱动
    installNextDriver();
}

void PageDriverManager::slotScanInfo(const QString &info, int progress)
{
    mp_StackWidget->setCurrentIndex(0);
    mp_ScanWidget->refreshProgress(info, progress);
}

void PageDriverManager::slotScanFinished(ScanResult sr)
{
    if (SR_Failed == sr) {
        mp_ScanWidget->setScanFailedUI();
    } else if (SR_SUCESS == sr) {
        mp_ScanWidget->setProgressFinish();
        // 扫描成功，则将数据显示到表格中
        foreach (DriverInfo *info, m_ListDriverInfo) {
            addDriverInfoToTableView(info, m_ListDriverInfo.indexOf(info));
            if (ST_NOT_INSTALL == info->status()) {
                m_ListInstallIndex.append(m_ListDriverInfo.size() - 1);
            } else if (ST_CAN_UPDATE == info->status()) {
                m_ListUpdateIndex.append(m_ListDriverInfo.size() - 1);
            } else if (ST_DRIVER_IS_NEW == info->status()) {
                m_ListNewIndex.append(m_ListDriverInfo.size() - 1);
            }
        }

        // 决定显示哪些列表，可安装，可更新，无需安装
        showTables();

        // 获取已经勾选的驱动index
        m_ListDriverIndex.clear();
        mp_ViewNotInstall->getCheckedDriverIndex(m_ListDriverIndex);
        mp_ViewCanUpdate->getCheckedDriverIndex(m_ListDriverIndex);

    } else if (SR_NETWORD_ERR == sr) {
        mp_ScanWidget->setNetworkErr();
        mp_scanner->quit();
    }
}

void PageDriverManager::slotUndoInstall()
{
    DBusDriverInterface::getInstance()->undoInstallDriver();
}

void PageDriverManager::initWidget()
{
    initTable();

    QHBoxLayout *mainLayout = new QHBoxLayout();
    mainLayout->addWidget(mp_StackWidget);
    this->setLayout(mainLayout);

    DFrame *mainFrame = new DFrame(this);
    mainFrame->setLineWidth(0);
    mainFrame->setFrameRounded(true);
    initMainFrame(mainFrame);
    mp_StackWidget->addWidget(mp_ScanWidget);
    mp_StackWidget->addWidget(mainFrame);
}

void PageDriverManager::initTable()
{
    // 设置列宽
    mp_ViewNotInstall->initHeaderView(QStringList() << ""
                                      << QObject::tr("Device Name")
                                      << QObject::tr("Version Available")
                                      << QObject::tr("Size")
                                      << QObject::tr("Status")
                                      << QObject::tr("Action"), true);
    mp_ViewNotInstall->setColumnWidth(0, 41);
    mp_ViewNotInstall->setColumnWidth(1, 324);
    mp_ViewNotInstall->setColumnWidth(2, 186);
    mp_ViewNotInstall->setColumnWidth(3, 120);
    mp_ViewNotInstall->setColumnWidth(4, 150);
    mp_ViewNotInstall->setColumnWidth(5, 120);

    mp_ViewCanUpdate->initHeaderView(QStringList() << ""
                                     << QObject::tr("Device Name")
                                     << QObject::tr("New Version")
                                     << QObject::tr("Size")
                                     << QObject::tr("Status")
                                     << QObject::tr("Action"), true);
    mp_ViewCanUpdate->setHeaderCbStatus(false);
    mp_ViewCanUpdate->setColumnWidth(0, 41);
    mp_ViewCanUpdate->setColumnWidth(1, 324);
    mp_ViewCanUpdate->setColumnWidth(2, 186);
    mp_ViewCanUpdate->setColumnWidth(3, 120);
    mp_ViewCanUpdate->setColumnWidth(4, 150);
    mp_ViewCanUpdate->setColumnWidth(5, 120);

    mp_AllDriverIsNew->initHeaderView(QStringList() << QObject::tr("Device Name") << QObject::tr("Current Version"));
    mp_AllDriverIsNew->setColumnWidth(0, 508);
    mp_AllDriverIsNew->setColumnWidth(1, 432);
}

void PageDriverManager::initMainFrame(DFrame *mainFrame)
{
    mainFrame->setContentsMargins(0, 0, 0, 0);
    QVBoxLayout *vLaout = new QVBoxLayout();
    vLaout->setContentsMargins(0, 20, 0, 0);
    vLaout->setSpacing(0);

    // 上方的表头
    QHBoxLayout *headerLayout = new QHBoxLayout();
    initHeadWidget(headerLayout);
    vLaout->addLayout(headerLayout);

    // 下方的可滑动区域
    DScrollArea *area = new DScrollArea(this);
    initScrollArea(area);
    vLaout->addWidget(area);

    mainFrame->setLayout(vLaout);
}

void PageDriverManager::initHeadWidget(QHBoxLayout *hLayout)
{
    hLayout->setSpacing(0);
    hLayout->addSpacing(18);
    hLayout->addWidget(mp_HeadWidget);
    hLayout->addSpacing(18);
}

void PageDriverManager::initScrollArea(DScrollArea *area)
{
    area->setFrameShape(QFrame::NoFrame);
    area->setWidgetResizable(true);
    DWidget *frame = new DWidget(this);
    frame->setContentsMargins(10, 10, 10, 10);
    QVBoxLayout *frameLayout = new QVBoxLayout();

    frameLayout->addWidget(mp_InstallLabel);
    mp_InstallLabel->setFixedHeight(25);
    mp_InstallLabel->setContentsMargins(0, 0, 1, 0);
    frameLayout->addWidget(mp_ViewNotInstall);

    frameLayout->addWidget(mp_UpdateLabel);
    mp_UpdateLabel->setFixedHeight(mp_UpdateLabel->height() + 10);
    mp_UpdateLabel->setContentsMargins(0, 10, 0, 0);
    frameLayout->addWidget(mp_ViewCanUpdate);

    frameLayout->addWidget(mp_LabelIsNew);
    frameLayout->addWidget(mp_AllDriverIsNew);

    frameLayout->addStretch();

    frame->setLayout(frameLayout);
    area->setWidget(frame);
}

void PageDriverManager::addDriverInfoToTableView(DriverInfo *info, int index)
{
    PageDriverTableView *view = nullptr;
    if (ST_NOT_INSTALL == info->status()) {
        view = mp_ViewNotInstall;
        view->appendRowItems(6);
    } else if (ST_CAN_UPDATE == info->status()) {
        view = mp_ViewCanUpdate;
        view->appendRowItems(6);
    } else if (ST_DRIVER_IS_NEW == info->status()) {
        view = mp_AllDriverIsNew;
        view->appendRowItems(2);
    } else {
        return;
    }

    int row = view->model()->rowCount() - 1;

    if (view != mp_AllDriverIsNew) {

        // 设置CheckBtn
        DriverCheckItem *cbItem = new DriverCheckItem(this);
        connect(cbItem, &DriverCheckItem::sigChecked, view, [index, view](bool checked) {
            view->setHeaderCbStatus(checked);
            qInfo() << "index : " << index;
        });
        cbItem->setChecked(ST_NOT_INSTALL == info->status());
        view->setWidget(row, 0, cbItem);

        // 设置设备信息
        DriverNameItem *nameItem = new DriverNameItem(this, info->type());
        nameItem->setName(info->name());
        nameItem->setIndex(index);
        view->setWidget(row, 1, nameItem);

        // 设置版本
        DriverLabelItem *versionItem = new DriverLabelItem(this, info->version());
        view->setWidget(row, 2, versionItem);

        // 设置大小
        DriverLabelItem *sizeItem = new DriverLabelItem(this, info->size());
        view->setWidget(row, 3, sizeItem);

        // 设置状态
        DriverStatusItem *statusItem = new DriverStatusItem(this, info->status());
        view->setWidget(row, 4, statusItem);

        // 添加操作按钮
        DriverOperationItem *operateItem = new DriverOperationItem(this, ST_NOT_INSTALL == info->status() ? true : false);
        view->setWidget(row, 5, operateItem);
    } else {
        // 设置设备信息
        DriverNameItem *nameItem = new DriverNameItem(this, info->type());
        nameItem->setName(info->name());
        nameItem->setIndex(index);
        view->setWidget(row, 0, nameItem);

        // 设置版本
        DriverLabelItem *versionItem = new DriverLabelItem(this, info->version());
        view->setWidget(row, 1, versionItem);
    }
}

void PageDriverManager::addCurDriverInfo(DriverInfo *info)
{
    mp_AllDriverIsNew->appendRowItems(2);

    DriverNameItem *nameItem = new DriverNameItem(this, info->type());
    int row = mp_AllDriverIsNew->model()->rowCount() - 1;

    nameItem->setName(info->name());
    mp_AllDriverIsNew->setWidget(row, 0, nameItem);

    DriverLabelItem *versionItem = new DriverLabelItem(this, info->version());
    mp_AllDriverIsNew->setWidget(row, 1, versionItem);
}

void PageDriverManager::installNextDriver()
{
    if (m_ListDriverIndex.size() > 0) {
        DriverInfo *info = m_ListDriverInfo[m_ListDriverIndex[0]];
        if (info) {
            mp_CurDriverInfo = info;
            mp_CurDriverInfo->m_Status = ST_DOWNLOADING;
            mp_ViewCanUpdate->setItemStatus(m_ListDriverIndex[0], mp_CurDriverInfo->status());
            mp_ViewNotInstall->setItemStatus(m_ListDriverIndex[0], mp_CurDriverInfo->status());
            mp_HeadWidget->setDownloadUI(mp_CurDriverInfo->type(), "0MB/s", "0MB", mp_CurDriverInfo->size(), 0);
            DBusDriverInterface::getInstance()->installDriver(info->packages(), info->debVersion());
        }
    }
}

void PageDriverManager::scanDevices()
{
    // 显卡
    scanDevicesInfo(QObject::tr("Display Adapter"), DR_Gpu);

    // 声卡
    scanDevicesInfo(QObject::tr("Sound Adapter"), DR_Sound);

    // 蓝牙
    scanDevicesInfo(QObject::tr("Bluetooth"), DR_Bluetooth);

    // 网卡
    scanDevicesInfo(QObject::tr("Network Adapter"), DR_Network);

    // 图像设备
    scanDevicesInfo(QObject::tr("Camera"), DR_Camera);

    // 打印机
    scanDevicesInfo(QObject::tr("Printer"), DR_Printer);
}

void PageDriverManager::testScanDevices()
{
    DriverInfo *info = new DriverInfo();
    info->m_Name = "GeFore RTX30 Series (Notebooks) 001";
    info->m_Size = "124.36";
    info->m_Type = DR_Gpu;
    info->m_Status = ST_NOT_INSTALL;
    info->m_Checked = true;
    info->m_DriverName = "lenovo-image-g-series";
    info->m_Packages = "deepin-log-viewer";
    info->m_Version = "1.0-17";
    addDriverInfo(info);

    info = new DriverInfo();
    info->m_Name = "GeFore RTX30 Series (Notebooks) 002";
    info->m_Size = "124.36";
    info->m_Type = DR_Camera;
    info->m_Status = ST_NOT_INSTALL;
    info->m_Checked = true;
    info->m_DriverName = "deepin-log-viewer";
    info->m_Packages = "deepin-log-viewer";
    info->m_Version = "5.8.32-1";
    addDriverInfo(info);

    info = new DriverInfo();
    info->m_Name = "GeFore RTX30 Series (Notebooks) 003";
    info->m_Size = "124.36";
    info->m_Type = DR_Camera;
    info->m_Status = ST_NOT_INSTALL;
    info->m_Checked = true;
    info->m_DriverName = "deepin-reader";
    info->m_Packages = "deepin-log-viewer";
    info->m_Version = "5.10.11-1";
    addDriverInfo(info);

    info = new DriverInfo();
    info->m_Name = "GeFore RTX30 Series (Notebooks) 004";
    info->m_Size = "124.36";
    info->m_Type = DR_Camera;
    info->m_Status = ST_NOT_INSTALL;
    info->m_Checked = true;
    info->m_DriverName = "deepin-diskmanager";
    info->m_Packages = "deepin-log-viewer";
    info->m_Version = "1.2.21-1";
    addDriverInfo(info);

    info = new DriverInfo();
    info->m_Name = "GeFore RTX30 Series (Notebooks) 005";
    info->m_Size = "124.36";
    info->m_Type = DR_Camera;
    info->m_Status = ST_CAN_UPDATE;
    info->m_Checked = true;
    info->m_DriverName = "uos-remote-assistance";
    info->m_Packages = "deepin-log-viewer";
    info->m_Version = "1.0.7-1";
    addDriverInfo(info);

    info = new DriverInfo();
    info->m_Name = "GeFore RTX30 Series (update)";
    info->m_Size = "124.36";
    info->m_Type = DR_Camera;
    info->m_Status = ST_CAN_UPDATE;
    info->m_Checked = false;
    info->m_DriverName = "deepin-calculator";
    info->m_Packages = "deepin-log-viewer";
    info->m_Version = "5.7.12-1";
    addDriverInfo(info);

    info = new DriverInfo();
    info->m_Name = "GeFore RTX30 Series (update)";
    info->m_Size = "124.36";
    info->m_Type = DR_Camera;
    info->m_Status = ST_CAN_UPDATE;
    info->m_Checked = false;
    info->m_DriverName = "deepin-defender";
    info->m_Packages = "deepin-log-viewer";
    info->m_Version = "3.0.21-1";
    addDriverInfo(info);

    info = new DriverInfo();
    info->m_Name = "GeFore RTX30 Series (update)";
    info->m_Size = "124.36";
    info->m_Type = DR_Camera;
    info->m_Status = ST_CAN_UPDATE;
    info->m_Checked = false;
    info->m_DriverName = "deepin-system-monitor";
    info->m_Packages = "deepin-log-viewer";
    info->m_Version = "5.8.17-1";
    addDriverInfo(info);

    info = new DriverInfo();
    info->m_Name = "GeFore RTX30 Series (update)";
    info->m_Size = "124.36";
    info->m_Type = DR_Camera;
    info->m_Status = ST_DRIVER_IS_NEW;
    info->m_Checked = false;
    info->m_DriverName = "dde-introduction";
    info->m_Packages = "deepin-log-viewer";
    info->m_Version = "5.6.18-1";
    addDriverInfo(info);

}

void PageDriverManager::scanDevicesInfo(const QString &deviceType, DriverType driverType)
{
    // 显卡
    QList<DeviceBaseInfo *> lst;
    bool ret = DeviceManager::instance()->getDeviceList(deviceType, lst);
    if (ret) {
        foreach (DeviceBaseInfo *device, lst) {
            DriverInfo *info = new DriverInfo();
            info->m_Name = device->name();        // 设备名称
            info->m_Type = driverType;
            info->m_Status = ST_DRIVER_IS_NEW;    // 默认已安装驱动为最新
            info->m_VendorId = device->getVendorOrModelId(device->sysPath(), true);    // vendor id
            info->m_ModelId = device->getVendorOrModelId(device->sysPath(), false);    // model id
            info->m_DriverName = device->driver();                 // 驱动名称
            info->m_Version = device->getDriverVersion();          // 驱动版本

            DevicePrint *print = dynamic_cast<DevicePrint *>(device);
            if (print != nullptr) {
                info->m_VendorName = device->vendor();           // vendor name
                info->m_ModelName = print->makeAndeModel();      // model name
            }

            DeviceNetwork *network = dynamic_cast<DeviceNetwork *>(device);
            if (network != nullptr) {
                if (network->isWireless()) {
                    info->m_Type = DR_WiFi;
                }

            }

            addDriverInfo(info);
        }
    }
}

void PageDriverManager::showTables()
{
    int installLength = m_ListInstallIndex.size();
    int updateLength = m_ListUpdateIndex.size();

    // Label显示
    mp_InstallLabel->setText(QObject::tr("Missing drivers (%1)").arg(m_ListInstallIndex.size()));
    mp_UpdateLabel->setText(QObject::tr("Outdated drivers (%1)").arg(m_ListUpdateIndex.size()));
    mp_LabelIsNew->setText(QObject::tr("Up-to-date drivers (%1)").arg(m_ListNewIndex.size()));

    // 设置哪几个Label需要显示出来
    mp_InstallLabel->setVisible(installLength != 0);
    mp_UpdateLabel->setVisible(updateLength != 0);
    mp_LabelIsNew->setVisible(installLength == 0 && updateLength == 0);

    // 设置哪几个表格需要显示出来
    mp_ViewNotInstall->setVisible(installLength != 0);
    mp_ViewCanUpdate->setVisible(updateLength != 0);
    mp_AllDriverIsNew->setVisible(installLength == 0 && updateLength == 0);


    // 显示表头显示的内容
    if (installLength == 0 && updateLength == 0) {
        mp_HeadWidget->setNoUpdateDriverUI("PythonManGH");
    } else {
        mp_HeadWidget->setDetectFinishUI(QString::number(installLength + updateLength), "PythonManGH", installLength != 0);
    }

    // 显示驱动列表界面
    mp_StackWidget->setCurrentIndex(1);
}


void PageDriverManager::clearAllData()
{
    m_ListNewIndex.clear();
    m_ListDriverIndex.clear();
    m_ListUpdateIndex.clear();
    m_ListInstallIndex.clear();

    foreach (DriverInfo *info, m_ListDriverInfo) {
        delete info;
    }
    m_ListDriverInfo.clear();

    mp_ViewCanUpdate->clear();
    mp_ViewNotInstall->clear();
    mp_AllDriverIsNew->clear();

    initTable();
}

void PageDriverManager::addToDriverIndex(int index)
{
    // 如果第一次添加到index，一键安装按钮置灰
    if (m_ListDriverIndex.size() <= 0) {
        mp_HeadWidget->setInstallBtnEnable(true);
    }

    bool add = true;
    for (int i = 0; i < m_ListDriverIndex.size(); i++) {
        if (index == m_ListDriverIndex[i]) {
            add = false;
            break;
        }
    }

    if (add) {
        m_ListDriverIndex.append(index);
    }
}

void PageDriverManager::removeFromDriverIndex(int index)
{
    for (int i = 0; i < m_ListDriverIndex.size(); i++) {
        if (index == m_ListDriverIndex[i]) {
            m_ListDriverIndex.removeAt(i);
            break;
        }
    }

    // 移除时 如果一个都没有 则一键安装按钮置灰
    if (m_ListDriverIndex.size() <= 0) {
        mp_HeadWidget->setInstallBtnEnable(false);
    }
}
