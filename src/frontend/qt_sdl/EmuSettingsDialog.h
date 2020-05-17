/*
    Copyright 2016-2020 Arisotura

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef EMUSETTINGSDIALOG_H
#define EMUSETTINGSDIALOG_H

#include <QDialog>

namespace Ui { class EmuSettingsDialog; }
class EmuSettingsDialog;

class EmuSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EmuSettingsDialog(QWidget* parent);
    ~EmuSettingsDialog();

    static EmuSettingsDialog* currentDlg;
    static void openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return;
        }

        currentDlg = new EmuSettingsDialog(parent);
        currentDlg->show();
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void on_EmuSettingsDialog_accepted();
    void on_EmuSettingsDialog_rejected();

    void on_btnBIOS9Browse_clicked();
    void on_btnBIOS7Browse_clicked();
    void on_btnFirmwareBrowse_clicked();

private:
    void verifyFirmware();

    Ui::EmuSettingsDialog* ui;
};

#endif // EMUSETTINGSDIALOG_H
