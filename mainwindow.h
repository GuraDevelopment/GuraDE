#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    //void on_tabWidget_currentChanged(int index);

    void boot();

    void PictureWidget();

    void getApps();

    void raiseLauncher();

    void on_pushButton_12_clicked();

    void on_pushButton_13_clicked();

    void on_pushButton_14_clicked();

    void on_pushButton_15_clicked();

    void on_pushButton_16_clicked();

    void on_pushButton_17_clicked();

    void on_pushButton_18_clicked();
    void on_firefox_clicked();

    void on_PCManFM_clicked();

    void on_MPV_clicked();

private:
    Ui::MainWindow *ui;
    QTimer *clockTimer;
    QTimer *PictureTimer;
    QTimer *AppRefresh;
    QTimer *cornerWatcher;
    QTimer *taskPopulater;
    void populateTaskList();
    QList<QPair<WId, QString>> getOpenWindows();
    void raiseWindow(WId winId);
};
#endif // MAINWINDOW_H
