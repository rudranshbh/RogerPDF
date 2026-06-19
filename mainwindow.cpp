#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog> //File picker dialog
#include <QFileInfo>   //File metadata
#include <mupdf/fitz.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if(ctx)
    {
        fz_drop_context(ctx);
    }

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

    currentPdfPath=QFileDialog::getOpenFileName(this,"Open PDF","","PDF Files (*.pdf)");

    if(!currentPdfPath.isEmpty()){


        QFileInfo fileinfo(currentPdfPath);         //used to extract file metadata
        QString filename=fileinfo.fileName(); //extract only filename from info of filepath
        ui->pdfViewLabel->setText(filename);
        loadPdf(currentPdfPath);}
}

void MainWindow::loadPdf(const QString &path)
{
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);

    if (!ctx)
        return;

    fz_register_document_handlers(ctx);

    fz_try(ctx)
    {fz_document *doc = fz_open_document(ctx, path.toUtf8().constData());

        int pageCount = fz_count_pages(ctx, doc);

        fz_page *page = fz_load_page(ctx, doc, 0);

        if(page)
        {
            ui->pdfViewLabel->setText(
                QString("Page 1 Loaded | Total Pages: %1")
                    .arg(pageCount)
                );

            fz_drop_page(ctx, page);
        }

        fz_drop_document(ctx, doc);
    }
    fz_catch(ctx)
    {
        ui->pdfViewLabel->setText(
            QString("Error: %1").arg(fz_caught_message(ctx))
            );
    }

    fz_drop_context(ctx);
}