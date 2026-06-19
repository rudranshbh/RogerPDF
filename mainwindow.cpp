#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog> //File picker dialog
#include <QFileInfo>   //File metadata
#include <mupdf/fitz.h>
#include <QImage>
#include <QPixmap>
#include <QScreen>
#include <QShortcut>
#include <QStyle>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->resize(800, 600);

    this->setWindowIcon(style()->standardIcon(QStyle::SP_FileIcon));
    ui->btnPrev->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    ui->btnNext->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    ui->actionOpen_PDF->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui->actionExit->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    ui->actionAbout->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));

    this->showMaximized();

    ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    fz_register_document_handlers(ctx);

    setupShortcuts();

    connect(ui->actionOpen_PDF, &QAction::triggered, this, &MainWindow::openPdf);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAbout);
    connect(ui->btnNext, &QPushButton::clicked, this, &MainWindow::nextPage);
    connect(ui->btnPrev, &QPushButton::clicked, this, &MainWindow::prevPage);
    connect(ui->btnZoomIn, &QPushButton::clicked, this, &MainWindow::zoomIn);
    connect(ui->btnZoomOut, &QPushButton::clicked, this, &MainWindow::zoomOut);
}

MainWindow::~MainWindow()
{
    closeCurrentDocument();
    if (ctx) fz_drop_context(ctx);
    delete ui;
}

void MainWindow::showAbout() {
    QString aboutText =
        "<h2>Roger PDF v1.0</h2>"
        "<p><b>Developed by:</b> Rudransh Bhardwaj</p>"
        "<p><b>Technical Stack:</b></p>"
        "<ul>"
        "<li><b>Language:</b> C++ (Standard 17)</li>"
        "<li><b>Framework:</b> Qt 6 (UI Logic & High-DPI Scaling)</li>"
        "<li><b>Engine:</b> MuPDF Fitz (Lightweight PDF Rendering)</li>"
        "</ul>"
        "<p><b>Core Features:</b> High-performance matrix re-rendering, "
        "Anti-aliased DPI awareness, and native shortcut integration.</p>";

    QMessageBox::about(this, "About Roger PDF", aboutText);
}

void MainWindow::setupShortcuts() {
    new QShortcut(QKeySequence(Qt::Key_Right), this, SLOT(nextPage()));
    new QShortcut(QKeySequence(Qt::Key_Left), this, SLOT(prevPage()));
    new QShortcut(QKeySequence(Qt::Key_PageDown), this, SLOT(nextPage()));
    new QShortcut(QKeySequence(Qt::Key_PageUp), this, SLOT(prevPage()));
    new QShortcut(QKeySequence("Ctrl+="), this, SLOT(zoomIn()));
    new QShortcut(QKeySequence("Ctrl+-"), this, SLOT(zoomOut()));
}

void MainWindow::closeCurrentDocument() {
    if (doc) {
        fz_drop_document(ctx, doc);
        doc = nullptr;
    }
}

void MainWindow::openPdf(){
    currentPdfPath = QFileDialog::getOpenFileName(this, "Open PDF", "", "PDF Files (*.pdf)");

    if(!currentPdfPath.isEmpty()){
        closeCurrentDocument();

        fz_try(ctx) {
            doc = fz_open_document(ctx, currentPdfPath.toUtf8().constData());
            totalPages = fz_count_pages(ctx, doc);
            currentPage = 0;
            renderPage();

            QFileInfo fileinfo(currentPdfPath);         //used to extract file metadata
            this->setWindowTitle(QString("Roger PDF - %1").arg(fileinfo.fileName()));
        } fz_catch(ctx) {
            ui->pdfPageLabel->setText("Failed to open PDF");
        }
    }
}

void MainWindow::nextPage() {
    if (doc && currentPage + 1 < totalPages) {
        currentPage++;
        renderPage();
    }
}

void MainWindow::prevPage() {
    if (doc && currentPage > 0) {
        currentPage--;
        renderPage();
    }
}

void MainWindow::zoomIn() {
    if (doc) {
        currentZoom += 0.2f;
        renderPage();
    }
}

void MainWindow::zoomOut() {
    if (doc && currentZoom > 0.4f) {
        currentZoom -= 0.2f;
        renderPage();
    }
}

void MainWindow::renderPage()
{
    if (!doc) return;

    fz_try(ctx)
    {
        ui->pageInfoLabel->setText(QString("%1 / %2").arg(currentPage + 1).arg(totalPages));

        fz_page *page = fz_load_page(ctx, doc, currentPage);
        float dpr = this->devicePixelRatioF();
        fz_matrix matrix = fz_scale(currentZoom * dpr, currentZoom * dpr);

        fz_rect bounds = fz_bound_page(ctx, page);
        bounds = fz_transform_rect(bounds, matrix);
        fz_irect bbox = fz_round_rect(bounds);

        fz_pixmap *pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, nullptr, 0);
        fz_clear_pixmap_with_value(ctx, pix, 255);

        fz_device *dev = fz_new_draw_device(ctx, matrix, pix);
        fz_run_page(ctx, page, dev, fz_identity, nullptr);

        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);

        QImage image(fz_pixmap_samples(ctx, pix), fz_pixmap_width(ctx, pix), fz_pixmap_height(ctx, pix), fz_pixmap_stride(ctx, pix), QImage::Format_RGB888);
        image.setDevicePixelRatio(dpr);

        ui->pdfPageLabel->setPixmap(QPixmap::fromImage(image.copy()));

        fz_drop_pixmap(ctx, pix);
        fz_drop_page(ctx, page);
    }
    fz_catch(ctx)
    {
        ui->pdfPageLabel->setText(QString("Error: %1").arg(fz_caught_message(ctx)));
    }
}