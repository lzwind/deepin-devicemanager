/*
* Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
*
* Author:     Jun.Liu <liujuna@uniontech.com>
*
* Maintainer: XiaoMei.Ji <jixiaomei@uniontech.com>
* Maintainer: Jun.Liu <liujuna@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef PAGEINSTALLDRIVER_H
#define PAGEINSTALLDRIVER_H

#include <DStackedWidget>
#include <DDialog>
#include <DLabel>


class DriverWaitingWidget;
class GetDriverNameWidget;
class GetDriverPathWidget;

DWIDGET_USE_NAMESPACE
class PageDriverControl : public DDialog
{
    Q_OBJECT
public:
    /**
     * @brief PageInstallDriver
     * @param operation 动作：卸载还是更新
     * @param name 驱动名称
     * @param install 标识卸载还是更新
     * @param parent
     */
    PageDriverControl(QString operation, QString deviceName, QString driverName, bool install = true, QWidget *parent = nullptr);

    /**
     * @brief hasWidgetIsVisible 有驱动界面显示
     * @return
     */
    static bool isRunning();
signals:
    /**
     * @brief refreshInfo:刷新信息信号
     */
    void refreshInfo();

private:
    /**
     * @brief init 初始化安装驱动界面
     */
    void initInstallWidget();

    /**
     * @brief initUninstallWidget 初始化卸载驱动界面
     */
    void initUninstallWidget();

private slots:
    /**
     * @brief slotBtnCancel 取消按钮
     */
    void slotBtnCancel();

    /**
     * @brief slotBtnNext 下一步按钮操作
     */
    void slotBtnNext();

    /**
     * @brief slotProcessChange 处理更新进度的曹函数
     * @param value 当前进度 0 ～ 100
     * @param detail 当前处理的详细信息
     */
    void slotProcessChange(qint32 value, QString detail);

    /**
     * @brief slotProcessEnd 更新处理结束的信号
     * @param sucess 更新是否成功
     */
    void slotProcessEnd(bool sucess);

    /**
     * @brief slotClose
     */
    void slotClose();

    /**
     * @brief slotBackPathPage
     */
    void slotBackPathPage();

private:
    /**
     * @brief removeBtn 移除按钮
     */
    void removeBtn();

    /**
     * @brief installDriverLogical
     */
    void installDriverLogical();

    /**
     * @brief uninstallDriverLogical
     */
    void uninstallDriverLogical();

private:
    DStackedWidget             *mp_stackWidget;
    GetDriverPathWidget        *mp_PathDialog;
    GetDriverNameWidget        *mp_NameDialog;
    DriverWaitingWidget        *mp_WaitDialog;
    DLabel                     *mp_tipLabel;
    bool                       m_Install;
    QString                    m_DriverName;
};
#endif // PAGEINSTALLDRIVER_H
