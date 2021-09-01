#include "DetectThread.h"
#include "MonitorUsb.h"

#include <QDebug>
#include <QProcess>

#define LEAST_NUM 10

DetectThread::DetectThread(QObject *parent)
    : QThread(parent)
    , mp_MonitorUsb(new MonitorUsb())
{
    // 连接槽函数
    connect(mp_MonitorUsb, SIGNAL(usbChanged()), this, SLOT(slotUsbChanged()), Qt::QueuedConnection);

    QMap<QString,QMap<QString,QString>> usbInfo;
    curHwinfoUsbInfo(usbInfo);
    updateMemUsbInfo(usbInfo);
}

void DetectThread::run()
{
    if (mp_MonitorUsb) {
        mp_MonitorUsb->monitor();
    }
}

void DetectThread::slotUsbChanged()
{
    // 当监听到新的usb时，内核需要加载usb信息，而上层应用需要在内核处理之后获取信息
    // 为了确保缓存信息之前，内核已经处理完毕，先判断内核是否处理完信息，且判断时间不能多于10s
    qint64 begin = QDateTime::currentMSecsSinceEpoch();
    qint64 end = begin;
    while ((end - begin) <= 10000) {
        if(isUsbDevicesChanged())
            break;
        sleep(1);
        end = QDateTime::currentMSecsSinceEpoch();
    }
    qInfo() << " 此次判断插拔是否完成的时间为 ************ " << QDateTime::currentMSecsSinceEpoch() - begin;
    emit usbChanged();
}

bool DetectThread::isUsbDevicesChanged()
{
    QMap<QString,QMap<QString,QString>> curUsbInfo;
    curHwinfoUsbInfo(curUsbInfo);

    // 拔出的时候，如果当前的usb设备个数小于m_MapUsbInfo的个数则返回true
    if(curUsbInfo.size() < m_MapUsbInfo.size()){
        updateMemUsbInfo(curUsbInfo);
        return true;
    }

    // 数量一样或curUsbInfo的大小大于m_MapUsbInfo的大小，则一个一个的比较
    // 如果curUsbInfo里面的在m_MapUsbInfo里面找不到则说明内核信息还没有处理完
    foreach(const QString& key,curUsbInfo.keys()){
        if(m_MapUsbInfo.find(key) != m_MapUsbInfo.end())
            continue;
        if(curUsbInfo[key]["Hardware Class"] == "disk"
                && curUsbInfo[key].find("Capacity") == curUsbInfo[key].end())
            continue;
        updateMemUsbInfo(curUsbInfo);
        return true;
    }
    return false;
}

void DetectThread::updateMemUsbInfo(const QMap<QString,QMap<QString,QString>>& usbInfo)
{
    m_MapUsbInfo.clear();
    m_MapUsbInfo = usbInfo;
}

void DetectThread::curHwinfoUsbInfo(QMap<QString,QMap<QString,QString>>& usbInfo)
{
    QProcess process;
    process.start("hwinfo --usb");
    process.waitForFinished(-1);
    QString info = process.readAllStandardOutput();

    QStringList items = info.split("\n\n");
    foreach(const QString& item,items){
        QMap<QString,QString> mapItem;
        if(!getMapInfo(item,mapItem))
            continue;
        usbInfo.insert(mapItem["SysFS BusID"],mapItem);
    }
}

bool DetectThread::getMapInfo(const QString& item,QMap<QString,QString>& mapInfo)
{
    QStringList lines = item.split("\n");
    // 行数太少则为无用信息
    if(lines.size() <= LEAST_NUM){
        return false;
    }

    foreach(const QString& line,lines){
        QStringList words = line.split(": ");
        if(words.size() != 2)
            continue;
        mapInfo.insert(words[0].trimmed(),words[1].trimmed());
    }

    // hub为usb接口，可以直接过滤
    if(mapInfo["Hardware Class"] == "hub"){
        return false;
    }

    // 没有总线信息的设备可以过滤
    if(mapInfo.find("SysFS BusID") == mapInfo.end()){
        return false;
    }

    return true;
}
