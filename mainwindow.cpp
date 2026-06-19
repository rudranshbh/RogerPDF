#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog> //File picker dialog
#include <QFileInfo>   //File metadata
#include <mupdf/fitz.h>
#include <QImage>
#include <QPixmap>

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

    connect(ui->btnNext, &QPushButton::clicked, this, &MainWindow::nextPage);
    connect(ui->btnPrev, &QPushButton::clicked, this, &MainWindow::prevPage);
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

        currentPage = 0;
        loadPdf(currentPdfPath, currentPage);
    }
}

void MainWindow::nextPage() {
    if (currentPage + 1 < totalPages) {
        currentPage++;
        loadPdf(currentPdfPath, currentPage);
    }
}

void MainWindow::prevPage() {
    if (currentPage > 0) {
        currentPage--;
        loadPdf(currentPdfPath, currentPage);
    }
}

void MainWindow::loadPdf(const QString &path, int pageNumber)
{
    fz_context *ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);

    if (!ctx)
        return;

    fz_register_document_handlers(ctx);

    fz_try(ctx)
    {
        fz_document *doc = fz_open_document(ctx, path.toUtf8().constData());
        totalPages = fz_count_pages(ctx, doc);

        ui->pageInfoLabel->setText(QString("Page %1 of %2").arg(pageNumber + 1).arg(totalPages));

        fz_page *page = fz_load_page(ctx, doc, pageNumber);

        fz_matrix matrix = fz_scale(1.5f, 1.5f);

        fz_rect bounds = fz_bound_page(ctx, page);

        bounds = fz_transform_rect(bounds, matrix);

        fz_irect bbox = fz_round_rect(bounds);

        fz_pixmap *pix = fz_new_pixmap_with_bbox(
            ctx,
            fz_device_rgb(ctx),
            bbox,
            nullptr,
            0
            );

        fz_clear_pixmap_with_value(ctx, pix, 255);

        fz_device *dev = fz_new_draw_device(ctx, matrix, pix);

        fz_run_page(ctx, page, dev, fz_identity, nullptr);

        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);

        QImage image(
            fz_pixmap_samples(ctx, pix),
            fz_pixmap_width(ctx, pix),
            fz_pixmap_height(ctx, pix),
            fz_pixmap_stride(ctx, pix),
            QImage::Format_RGB888
            );

        QPixmap qpix = QPixmap::fromImage(image.copy());

        ui->pdfPageLabel->setPixmap(qpix);

        fz_drop_document(ctx, doc);
        fz_drop_pixmap(ctx, pix);
        fz_drop_page(ctx, page);
    }
    fz_catch(ctx)
    {
        ui->pdfPageLabel->setText(
            QString("Error: %1").arg(fz_caught_message(ctx))
            );
    }

    fz_drop_context(ctx);
}