#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDateTime>
#include <QTimer>
#include <QDirIterator>
#include <QFile>
#include <QDebug>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QIcon>
#include <QSettings>
#include <QFileInfo>
#include <QMap>
#include <QPair>
#include <QRandomGenerator>
#include <QToolButton>
#include <QProcess>
#include <QScreen>
#include <cmath>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
static bool showed;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    boot();
    showFullScreen();
    taskPopulater = new QTimer(this);
    connect(taskPopulater, &QTimer::timeout, this, &MainWindow::populateTaskList);
    taskPopulater->start(100);

    clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &MainWindow::boot);
    clockTimer->start(500);

    PictureTimer = new QTimer(this);
    connect(PictureTimer, &QTimer::timeout, this, &MainWindow::PictureWidget);
    PictureTimer->start(6000);
    QTimer *cornerWatcher = new QTimer(this);
    connect(cornerWatcher, &QTimer::timeout, this, [=]() {
        QPoint cursorPos = QCursor::pos();  // global screen coordinates
        QRect screen = QGuiApplication::primaryScreen()->geometry();
        int margin = 4; // trigger distance from bottom-left
        bool inTriggerArea = cursorPos.x() <= screen.left() + margin &&
                             cursorPos.y() >= screen.bottom() - margin;

        static bool wasInCorner = false;

        if (inTriggerArea) {
            // Only toggle the window if the cursor just entered the corner.
            if (!wasInCorner) {
                if (this->isVisible()) {
                    this->hide();
                } else {
                    raiseLauncher();
                }
            }
            wasInCorner = true;
        } else {
            // The cursor has left the corner, so reset the flag.
            wasInCorner = false;
        }
    });
    cornerWatcher->start(150);


    //here starts the reading of the config files

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::boot()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    ui->tabWidget->setTabText(2, currentDateTime.toString("hh:mm"));
    ui->label_3->setText(currentDateTime.toString("hh:mm:ss"));
    QString user = qgetenv("USER");
    ui->label->setText("Welcome "+user+"!");
    getApps();
}

void MainWindow::PictureWidget()
{
    QString picturesPath = QDir::homePath() + "/Pictures";
    QStringList imageFilters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif"};
    QStringList imagePaths;

    QDir picturesDir(picturesPath);

    // Non-recursive: only top-level files in ~/Pictures
    QFileInfoList files = picturesDir.entryInfoList(imageFilters, QDir::Files | QDir::Readable);
    for (const QFileInfo &file : files) {
        imagePaths << file.absoluteFilePath();
    }

    if (!imagePaths.isEmpty()) {
        int index = QRandomGenerator::global()->bounded(imagePaths.size());
        QString selectedImagePath = imagePaths.at(index);
        QPixmap pixmap(selectedImagePath);

        if (!pixmap.isNull()) {
            QSize labelSize = ui->label_2->size();
            QPixmap scaledPixmap = pixmap.scaled(
                labelSize,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
                );

            ui->label_2->setAlignment(Qt::AlignCenter);
            ui->label_2->setPixmap(scaledPixmap);
        } else {
            ui->label_2->setText("Failed to load image.");
        }
    } else {
        ui->label_2->setText("No images found in ~/Pictures.");
    }
}

void MainWindow::getApps()
{
    QStringList searchPaths = {
        QDir::homePath() + "/.local/share/applications",
        "/usr/share/applications",
        "/usr/local/share/applications"
    };

    QMap<QString, QList<QPair<QString, QPair<QString, QIcon>>>> categoryMap;

    const QMap<QString, QString> categoryAliases = {
        {"Utility", "Accessories"}, {"Development", "Development"}, {"Education", "Education"},
        {"Game", "Games"}, {"Graphics", "Graphics"}, {"Network", "Internet"},
        {"Office", "Office"}, {"Settings", "Settings"}, {"System", "System Tools"},
        {"AudioVideo", "Multimedia"}, {"Other", "Other"}
    };

    for (const QString &path : searchPaths) {
        QDirIterator it(path, QStringList() << "*.desktop", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            QFileInfo fi(filePath);

            QSettings desktopFile(filePath, QSettings::IniFormat);


            QString name = desktopFile.value("Desktop Entry/Name").toString();
            QString iconName = desktopFile.value("Desktop Entry/Icon").toString();
            QStringList categories = desktopFile.value("Desktop Entry/Categories").toString().split(";", Qt::SkipEmptyParts);
            QString exec = desktopFile.value("Desktop Entry/Exec").toString().split(" ").value(0);
            bool noDisplay = desktopFile.value("Desktop Entry/NoDisplay", false).toBool();
            bool hidden = desktopFile.value("Desktop Entry/Hidden", false).toBool();

            if (name.isEmpty() || exec.isEmpty() || noDisplay || hidden)
                continue;

            // Filter out internal components (KDE/GNOME helpers etc.)
            if (name.contains("rules", Qt::CaseInsensitive) || name.contains("support", Qt::CaseInsensitive))
                continue;

            QString baseCategory = "Other";
            for (const QString &cat : categories) {
                if (categoryAliases.contains(cat)) {
                    baseCategory = categoryAliases[cat];
                    break;
                }
            }

            QIcon icon = QIcon::fromTheme(iconName);
            categoryMap[baseCategory].append({name, {exec, icon}});
        }
    }

    QWidget *container = ui->programs->widget();
    if (!container) {
        container = new QWidget;
        ui->programs->setWidget(container);
    }

    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    container->setLayout(mainLayout);

    for (const QString &category : categoryMap.keys()) {
        QLabel *categoryLabel = new QLabel("<b>" + category + "</b>");
        categoryLabel->setStyleSheet("color: white; font-size: 30px; margin: 8px;");
        mainLayout->addWidget(categoryLabel);

        QGridLayout *grid = new QGridLayout;
        int iconWidth = 150;
        int iconHeight = 150;
        int spacing = 4;
        grid->setSpacing(spacing);

        int screenWidth = QGuiApplication::primaryScreen()->geometry().width();

        // Calculate the effective width of an icon including its spacing.
        int itemWidth = iconWidth + spacing;

        // Calculate the number of columns using integer division,
        // which automatically truncates and is more direct than std::floor.
        int columns = screenWidth / itemWidth;

        // Add a check to ensure there's at least one column to prevent a crash
        if (columns < 1) {
            columns = 1;
        }

        qDebug() << "Screen width:" << screenWidth;
        qDebug() << "Calculated columns:" << columns;



        QList<QWidget*> appButtons;
        int totalApps = categoryMap[category].size();

        for (const auto &app : categoryMap[category]) {
            QString appName = app.first;
            QString execCommand = app.second.first;
            QIcon appIcon = app.second.second;

            QToolButton *btn = new QToolButton;
            btn->setIcon(appIcon);
            btn->setText(appName);
            btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            btn->setIconSize(QSize(48, 48));
            btn->setFixedSize(iconWidth, iconHeight);
            btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

            QString shortName = QFontMetrics(btn->font()).elidedText(appName, Qt::ElideRight, 120);
            btn->setText(shortName);
            btn->setToolTip(appName);

            connect(btn, &QToolButton::clicked, this, [execCommand, this]() {
                QProcess::startDetached(execCommand);
                showed = false;
                this->hide();
            });

            btn->setStyleSheet(R"(
        QToolButton {
            color: white;
            background-color: rgba(0, 100, 150, 0.2);
            border: 2px solid rgba(0, 255, 255, 0.4);
            border-radius: 12px;
            padding: 6px;
        }
        QToolButton:hover {
            background-color: rgba(0, 150, 200, 0.3);
            border-color: cyan;
        }
        QToolButton:pressed {
            background-color: rgba(0, 200, 255, 0.4);
            border-color: white;
        }
    )");

            appButtons.append(btn);
        }

        // Pad with blank widgets if necessary
        int totalNeeded = qCeil((double)appButtons.size() / columns) * columns;
        for (int i = appButtons.size(); i < totalNeeded; ++i) {
            QWidget *blank = new QWidget;
            blank->setFixedSize(iconWidth, iconHeight);
            blank->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            appButtons.append(blank);
        }

        // Now add widgets to grid layout
        for (int i = 0; i < appButtons.size(); ++i) {
            int row = i / columns;
            int col = i % columns;
            grid->addWidget(appButtons[i], row, col);
        }



        QWidget *gridContainer = new QWidget;
        gridContainer->setLayout(grid);
        mainLayout->addWidget(gridContainer);
    }

    mainLayout->addStretch(1);
}


void MainWindow::raiseLauncher() {
    this->show();
    this->raise();
    this->activateWindow();
}

QList<QPair<WId, QString>> MainWindow::getOpenWindows() {
    QList<QPair<WId, QString>> windows;

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return windows;

    Atom netClientList = XInternAtom(dpy, "_NET_CLIENT_LIST", True);
    Atom netWmName = XInternAtom(dpy, "_NET_WM_NAME", True);

    Window root = DefaultRootWindow(dpy);
    Atom actualType;
    int format;
    unsigned long nItems, bytesAfter;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(dpy, root, netClientList, 0, ~0L, False, XA_WINDOW,
                           &actualType, &format, &nItems, &bytesAfter, &data) == Success && data) {
        Window* array = reinterpret_cast<Window*>(data);

        for (unsigned long i = 0; i < nItems; ++i) {
            Window w = array[i];
            XTextProperty nameProp;

            if (XGetTextProperty(dpy, w, &nameProp, netWmName)) {
                QString name = QString::fromUtf8(reinterpret_cast<char*>(nameProp.value));
                if (!name.isEmpty()) {
                    windows.append({(WId)w, name});
                }
                XFree(nameProp.value);
            }
        }

        XFree(data);
    }

    XCloseDisplay(dpy);
    return windows;
}

void MainWindow::raiseWindow(WId winId) {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return;

    XRaiseWindow(dpy, static_cast<Window>(winId));
    XSetInputFocus(dpy, static_cast<Window>(winId), RevertToParent, CurrentTime);

    XCloseDisplay(dpy);
}

void MainWindow::populateTaskList() {
    // Assume ui->tasks is your QScrollArea
    QWidget *container = ui->Tasks->widget();
    if (!container) {
        container = new QWidget;
        ui->Tasks->setWidget(container);
    }

    // Clear layout if needed
    QLayout *oldLayout = container->layout();
    if (oldLayout)
        delete oldLayout;

    QVBoxLayout *layout = new QVBoxLayout(container);
    container->setLayout(layout);

    for (const auto &pair : getOpenWindows()) {
        WId winId = pair.first;
        QString title = pair.second;

        QPushButton *btn = new QPushButton(title);
        btn->setStyleSheet("text-align: left; padding: 4px; font-size: 16px;");
        layout->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [=]() {
            raiseWindow(winId);
        });
    }

    layout->addStretch(1);  // Push items up
}


void MainWindow::on_pushButton_12_clicked()
{
    getApps();
}


void MainWindow::on_pushButton_13_clicked()
{
    QProcess::startDetached("sh", {"-c", "systemctl poweroff"});
}


void MainWindow::on_pushButton_14_clicked()
{
    QProcess::startDetached("sh", {"-c", "systemctl reboot"});
}


void MainWindow::on_pushButton_15_clicked()
{
    QProcess::startDetached("sh", {"-c", "pkill -KILL -u $USER"});
}


void MainWindow::on_pushButton_16_clicked()
{
    QProcess::startDetached("sh", {"-c", "systemctl suspend"});
}


void MainWindow::on_pushButton_17_clicked()
{
    QProcess::startDetached("sh", {"-c", "systemctl hibernate"});
}


void MainWindow::on_pushButton_18_clicked()
{
    this->resize(this->width(), 100);
}




void MainWindow::on_firefox_clicked()
{
    QProcess::startDetached("firefox-esr");
    this->hide();
}


void MainWindow::on_PCManFM_clicked()
{
    QProcess::startDetached("pcmanfm");
    this->hide();
}


void MainWindow::on_MPV_clicked()
{
    QProcess::startDetached("vlc");
    this->hide();
}

