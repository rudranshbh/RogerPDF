#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QKeyEvent>
#include <QShortcut>
#include <mupdf/fitz.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void openPdf();
    void nextPage();
    void prevPage();
    void zoomIn();
    void zoomOut();

private:
    void renderPage();
    void closeCurrentDocument();
    void setupShortcuts();

private:
    Ui::MainWindow *ui;
    QString currentPdfPath;
    int currentPage = 0;
    int totalPages = 0;
    float currentZoom = 1.5f;

    fz_context *ctx = nullptr;
    fz_document *doc = nullptr;
};
#endif // MAINWINDOW_H