#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog> //File picker dialog
#include <QFileInfo>   //File metadata

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(
        ui->actionOpen_PDF,   //Menu Action
        &QAction::triggered,  //Signal emitted when clicked
        this,                 //Receiver Object
        &MainWindow::openPdf  //Slot to execute
        );
}

MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::openPdf(){
    QString filepath=QFileDialog::getOpenFileName(this,"Open PDF","","PDF Files (*.pdf)");

    if(!filepath.isEmpty()){
        QFileInfo fileinfo(filepath);         //used to extract file metadata
        QString filename=fileinfo.fileName(); //extract only filename from info of filepath
        ui->pdfViewLabel->setText(filename);}
}