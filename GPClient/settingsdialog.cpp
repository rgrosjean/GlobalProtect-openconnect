#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "clientos.h"

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    ui->clientosInput->addItems(clientOsOptions());
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::setClientos(QString clientos)
{
    ui->clientosInput->setCurrentIndex(clientOsIndex(clientos));
}

QString SettingsDialog::clientos()
{
    return ui->clientosInput->currentText();
}

void SettingsDialog::setOsVersion(QString osVersion) {
    ui->osVersionInput->setText(osVersion);
}

QString SettingsDialog::osVersion() {
    return ui->osVersionInput->text();
}
