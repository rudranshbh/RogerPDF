#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

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

private:
    void loadPdf(const QString &path, int pageNumber);

private:
    Ui::MainWindow *ui;
    QString currentPdfPath;
    int currentPage = 0;
    int totalPages = 0;
};
#endif // MAINWINDOW_H