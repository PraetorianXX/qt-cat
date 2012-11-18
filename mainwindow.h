#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QEventLoop>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QScriptValueIterator>
#include <QSettings>
#include <QScriptEngine>
#include <QShortcut>
#include <QTimer>
#include <qextserialport/qextserialport.h>




namespace Ui
{
class MainWindow;
}

QT_BEGIN_NAMESPACE
class QAction;
class QLabel;
class QScrollArea;
class QScrollBar;
QT_END_NAMESPACE


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void changeEvent(QEvent *e);

private slots:

    void dropWork();
    void changeSerial();
    void loadSettings();
    void saveSettings();
    void diagnostics();
    void teensyRead();
    void scannerRead();
    void quitNow();
    void changeImage(const QString &imagePath);
    void barcode_tick();
    void pause(const int &time);
    void fetch_settings();
    void log_barcode(const QString &bcode, const QString &req_result);
    void log_teensy(const QString &teensy_string);
    void log_to_webservice(const int &error_state);

    QString cw_request(const QString &barcode);
    int levenshtein_distance(const QString word1, const QString word2);
    int cw_submit(const QString &cwid);

    bool lsbu_submit(const QString &barcode);
    void switch_fullscreen();
    void change_bfullscreen();



private:

    Ui::MainWindow *ui;
    QNetworkAccessManager *nam;

    QString m_sSettingsFile;
    QString status_string;
    QString previous_status_string;
    QString temp_string;
    QString scannerData;
    QString mac_address;
    QString box_id;
    QString cw_reporting_url;
    QString cw_request_url;
    QString cw_submit_url;

    QString teensy_statusbar;
    QString scanner_statusbar;

    QEventLoop *eloop;
    QTimer *timer;
    QTimer *barcode_timer;
    QLabel *imageLabel;

    QextSerialPort *teensy;
    QextSerialPort *scanner;

    QShortcut *shortcut;
    QShortcut *submit_shortcut;
    QShortcut *workdrop_shortcut;
    QShortcut *fullscreen_shortcut;

    QRect init_imageLabel;
    QRect fullscreen_imageLabel;

    int init_width;
    int init_height;

    int fullscreen_width;
    int fullscreen_height;

    int error_count;
    int barcode_ticks;
    int barcode_max_ticks;
    int barcode_interval;
    int numtest;
    int prevnumtest;
    int screen_cooldown;
    int connection_timeout;

    double scaleFactor;

    long testCount;
    long scan_count;
    long total_scans;

    bool scanner_on;
    bool scanProcessing;
    bool bfullscreen;
    bool servo_open;

    QByteArray token;




    /*
    struct box_info
    {
        QString mac;
        QString name;
        ulong scans_current;
        ulong scans_max;
        ulong scans_total;
        ulong boot_count;
        bool permit_reboots;
        bool fullscreen;
        ulong screen_image_timer;
        ulong reporting_timer;
    } box_setup;

    struct serial_device
    {
        QString name;
        QString address;
        int baud;
        int timeout;

    } scanner, teensy;
    *//*
struct web_service
{
    QString address;
    QString token;
    int timeout;
} webservice;
*/

};

#endif // MAINWINDOW_H
