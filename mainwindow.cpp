#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QNetworkInterface>
#include "json/json.cpp"
#include <QScreen>
#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QTextStream>
#include <QElapsedTimer>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    nam = new QNetworkAccessManager(this);

    m_sSettingsFile = qApp->applicationDirPath() + "/settings.ini";

    QNetworkInterface mach = QNetworkInterface::interfaceFromName("eth0");
    mac_address = mach.hardwareAddress().replace(":","");

    PortSettings serialsettings = {BAUD9600, DATA_8, PAR_NONE, STOP_1, FLOW_OFF, 10};

    teensy = new QextSerialPort("/dev/ttyUSB0", serialsettings, QextSerialPort::EventDriven);
    scanner = new QextSerialPort("/dev/ttyUSB0", serialsettings, QextSerialPort::EventDriven);
    shortcut = new QShortcut(QKeySequence(tr("Esc", "Quit")), this);
    submit_shortcut = new QShortcut(QKeySequence(tr("F1", "Submit")), this);
    workdrop_shortcut = new QShortcut(QKeySequence(tr("F2", "Drop")), this);
    fullscreen_shortcut = new QShortcut(QKeySequence(tr("F12", "Fullscreen")), this);

    connect(ui->submitButton,SIGNAL(clicked()),this,SLOT(changeSerial()));
    connect(ui->workdroppedButton,SIGNAL(clicked()),this,SLOT(dropWork()));

    connect(shortcut, SIGNAL(activated()), this, SLOT(quitNow()));
    connect(submit_shortcut, SIGNAL(activated()), this, SLOT(changeSerial()));
    connect(workdrop_shortcut, SIGNAL(activated()), this, SLOT(dropWork()));
    connect(fullscreen_shortcut, SIGNAL(activated()), this, SLOT(change_bfullscreen()));
    connect(teensy, SIGNAL(readyRead()), SLOT(teensyRead()));
    connect(scanner, SIGNAL(readyRead()), SLOT(scannerRead()));

    QSettings stngs(m_sSettingsFile, QSettings::NativeFormat);
    QString tokentext = stngs.value("webservice_security_token").toString();
    tokentext.append(QDateTime::currentDateTime().date().toString("yyyyMMdd"));

    token.append(tokentext);
    token = token.toBase64();

    qDebug() << token;

    bfullscreen = true;

    fetch_settings();
    loadSettings();

    servo_open = false;

    changeImage(QString("%1/images/002.png").arg(qApp->applicationDirPath()));

}

void MainWindow::change_bfullscreen()
{
    if (bfullscreen == false)
    {
        bfullscreen = true;
    }
    else
    {
        bfullscreen = false;
    }

    switch_fullscreen();
}

void MainWindow::switch_fullscreen()
{
    if (bfullscreen == true)
    {
        qApp->setOverrideCursor( QCursor( Qt::BlankCursor ) );
        int width = 1024;
        int height = 600;
        QRect q (0, 0, width, height);

        //QMainWindow::setWindowState(Qt::WindowFullScreen);

        MainWindow::resize(width, height);
        ui->imageLabel->setGeometry(q);
    }
    else
    {
        qApp->setOverrideCursor( QCursor( Qt::ArrowCursor));
        QMainWindow::setWindowState(Qt::WindowNoState);
        MainWindow::resize(init_width, init_height);
        ui->imageLabel->setGeometry(init_imageLabel);
    }
}

void MainWindow::fetch_settings()
{
    QSettings settings(m_sSettingsFile, QSettings::NativeFormat);

    QString url = settings.value("cw_reporting_url").toString();

    QNetworkRequest request(QUrl(QString("%1setup/%2/%3/%4").arg(url, token, "Debian VM", mac_address)));

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = nam->get(request);

    QEventLoop loop;
    QObject::connect(reply, SIGNAL(readyRead()), &loop, SLOT(quit()));

    QTimer webservice_timeout;
    webservice_timeout.setSingleShot(true);
    webservice_timeout.start(10000);
    connect(&webservice_timeout, SIGNAL(timeout()), &loop, SLOT(quit()));
    loop.exec();
    QByteArray result;

    if(reply->error() == QNetworkReply::NoError)
    {
        result = reply->readAll();
        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        ui->textBrowser->append(QString("HTTP Fetch Settings Response: %1").arg(statusCode.toString()));

    }
    else
    {
        result.append(reply->errorString());
        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        ui->textBrowser->append(statusCode.toString());
    }

    JsonReader reader;

    reader.parse(result);

    QVariant vresult = reader.result();
    QVariantMap var = vresult.toMap();

    foreach(QString key, var.keys())
    {
        QVariant value = var.value(key);
        QString sText = value.toString();
        settings.setValue(key, sText);
    }
}

bool MainWindow::lsbu_submit(const QString &barcode)
{

    QNetworkRequest request(QUrl(QString("%1").arg(cw_submit_url)));

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QVariantMap dat;

    dat.insert("SimpleToken", token);
    dat.insert("CourseworkID", barcode);
    dat.insert("DropBoxID", QString("%1-%2").arg(box_id, mac_address));

    JsonWriter writer;
    writer.stringify(dat);
    QString json = writer.result();

    QByteArray qj;

    qj.append(json);

    qDebug() << "JSON STRING::" << qj;
    QNetworkReply *reply = nam->put( request, qj);
    reply->ignoreSslErrors();
    QEventLoop loop;
    QObject::connect(reply, SIGNAL(readyRead()), &loop, SLOT(quit()));

    QTimer webservice_timeout;
    webservice_timeout.setSingleShot(true);
    webservice_timeout.start(connection_timeout);
    connect(&webservice_timeout, SIGNAL(timeout()), &loop, SLOT(quit()));
    loop.exec();
    QByteArray result;

    if(reply->error() == QNetworkReply::NoError)
    {
        result = reply->readAll();
        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "Successful submission HTTP::" << statusCode.toString();
        ui->statusBar->showMessage(QString("Submit :: Success"));
        return true;
    }

    else
    {
        result.append(reply->errorString());
        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "Network error. Status code::" << statusCode.toString();
        qDebug() << "Network error. result::" << result;
        ui->statusBar->showMessage(QString("Submit :: Network error :: %1").arg(statusCode.toString()));
        return false;

    }

}


QString MainWindow::cw_request(const QString &barcode)
{

    changeImage(QString("%1/images/019.png").arg(qApp->applicationDirPath()));

    QNetworkRequest request(QUrl(QString(cw_request_url).arg(token, barcode)));

    ui->textBrowser->append(QString(cw_request_url).arg(token, barcode));

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = nam->get(request);

    QEventLoop loop;
    QObject::connect(reply, SIGNAL(readyRead()), &loop, SLOT(quit()));

    QTimer webservice_timeout;
    webservice_timeout.setSingleShot(true);
    webservice_timeout.start(5000);
    connect(&webservice_timeout, SIGNAL(timeout()), &loop, SLOT(quit()));
    loop.exec();

    QByteArray result;

    QString thisID = "";

    if(reply->error() == QNetworkReply::NoError)
    {
        result = reply->readAll();
        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // statusCode == 200 //
        ui->textBrowser->append(QString("HTTP Request Response: %1").arg(statusCode.toString()));

    }
    else
    {

        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        ui->textBrowser->append(statusCode.toString());

        thisID = "-1";
        ui->textBrowser->append(reply->errorString());
    }

    JsonReader reader;
    reader.parse(result);
    QVariant vresult = reader.result();
    QVariantMap variant = vresult.toMap();

    foreach(QString key, variant.keys())
    {
        QVariant value = variant.value(key);

        if (key=="CourseworkID")
        {
            thisID = value.toString();
        }
    }

    return thisID;

}


int MainWindow::cw_submit(const QString &cwid)
{
    QString url;

    url.append(QString("%1submit/%2/%3").arg(cw_reporting_url, token,cwid));

    QNetworkReply *reply = nam->get( QNetworkRequest( QUrl( url ) ) );

    QEventLoop loop;
    QObject::connect(reply, SIGNAL(readyRead()), &loop, SLOT(quit()));

    QTimer webservice_timeout;
    webservice_timeout.setSingleShot(true);
    webservice_timeout.start(connection_timeout);
    connect(&webservice_timeout, SIGNAL(timeout()), &loop, SLOT(quit()));
    loop.exec();
    QByteArray result;

    int affected = 0;

    if(reply->error() == QNetworkReply::NoError)
    {
        result = reply->readAll();
        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        ui->textBrowser->append(QString("HTTP Submit Response: %1").arg(statusCode.toString()));
    }
    else
    {
        result.append(reply->errorString());
        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        ui->textBrowser->append(statusCode.toString());

        affected = -2;
    }

    JsonReader reader;
    reader.parse(result);
    QVariant vresult = reader.result();
    QVariantMap variant = vresult.toMap();

    QByteArray qd;

    foreach(QString key, variant.keys())
    {
        QVariant value = variant.value(key);

        qd.append(key);
        qd.append(" :: ");
        qd.append(value.toString());
        qd.append("\n");

        if (key=="affected")
        {
            affected = value.toInt();
        }
    }

    ui->textBrowser->append(qd);

    return affected;

}


void MainWindow::pause(const int &time)
{
    QEventLoop countdown_loop;

    QTimer countdown_timeout;
    countdown_timeout.setSingleShot(true);
    countdown_timeout.start(time);
    connect(&countdown_timeout, SIGNAL(timeout()), &countdown_loop, SLOT(quit()));
    countdown_loop.exec();
}

void MainWindow::dropWork()
{
    ui->serialString->setText("1");
}

void MainWindow::changeSerial()
{
    ui->serialString->setText("30313233000");
}

void MainWindow::log_barcode(const QString &bcode, const QString &req_result)
{
    QDir mdir;
    mdir.mkpath(qApp->applicationDirPath() + "/logs");

    QString filename = QString("%1/logs/barcodes_%2").arg(qApp->applicationDirPath(), QDateTime::currentDateTime().date().toString("yyyy-MM-dd"));

    QFile file(filename);
    file.open(QIODevice::Append | QIODevice::Text);
    QTextStream out(&file);

    QString log_data = QString("%1 \t %2 \t %3 \r\n").arg(QDateTime::currentDateTime().currentDateTime().toString("yyyy-MM-dd hh:mm:ss"), bcode, req_result);

    out << log_data;

    file.close();

}

void MainWindow::barcode_tick()
{
    if (barcode_ticks > 0)
    {

        QString barcode = "";

        if (scanProcessing == true && scannerData != "")
        {
            barcode = scannerData;
            scannerData = "";

        }


        if(scanner->isOpen() == false)
        {
            barcode = ui->dataLine->text();
        }

        if (barcode != "")
        {
            barcode_timer->stop();
            barcode_ticks = barcode_max_ticks;
            QString request_result = cw_request(barcode);



            if (request_result == "")
            {
                changeImage(QString("%1/images/006.png").arg(qApp->applicationDirPath()));
                ui->textBrowser->append("Barcode not found");
                qDebug() << "Invalid barcode";
                log_barcode(barcode, "Invalid barcode");

                ui->statusBar->showMessage("Request :: Invalid barcode");
            }
            else if (request_result == "-1")
            {
                changeImage(QString("%1/images/006.png").arg(qApp->applicationDirPath()));
                ui->textBrowser->append("Network error");
                log_barcode(barcode, "Network error");
                ui->statusBar->showMessage("Request :: Network error");
            }
            else
            {
                ui->statusBar->showMessage("Request :: Success");

                ui->textBrowser->append(QString("CourseworkID found: %1").arg(request_result));

                //open servo
                status_string = "";
                ui->statusBar->showMessage("Open Servo");
                servo_open = true;
                teensy->write("b");
                //pause(5);


                QElapsedTimer work_drop_wait;

                ui->serialString->setText("");
                work_drop_wait.start();

                while(!work_drop_wait.hasExpired(6000))
                {
                    QEventLoop teensy_wait_loop;
                    QObject::connect(teensy, SIGNAL(readyRead()), &teensy_wait_loop, SLOT(quit()));
                    QTimer work_drop_timer;
                    work_drop_timer.setSingleShot(true);
                    work_drop_timer.start(50);
                    connect(&work_drop_timer, SIGNAL(timeout()), &teensy_wait_loop, SLOT(quit()));

                    if (teensy->isOpen())
                    {
                        teensy->write("i");

                    }
                    else
                    {
                        status_string = ui->serialString->text();
                    }

                    teensy_wait_loop.exec();

                    if (status_string == "1")
                    {
                        work_drop_wait.invalidate();
                    }

                }

                if(status_string == "1")
                {
                    ui->textBrowser->append("Work dropped");
                    bool submission_result;
                    submission_result = lsbu_submit(barcode);

                    if(submission_result == true)
                    {
                        changeImage(QString("%1/images/003.png").arg(qApp->applicationDirPath()));
                        ui->textBrowser->append("Submission successful\n");
                        scan_count++;
                        total_scans++;
                        log_barcode(barcode, "Success");
                    }
                    else
                    {
                        changeImage(QString("%1/images/006.png").arg(qApp->applicationDirPath()));
                        ui->textBrowser->append("Submission failed\n");
                        log_barcode(barcode, "Submission failed");
                    }
                }
                else
                {
                    ui->textBrowser->append("Work didn't drop");
                    changeImage(QString("%1/images/007.png").arg(qApp->applicationDirPath()));
                    log_barcode(barcode, "Barcode valid, work didn't drop");

                }


                /*
                //status_string = "1";
                //ui->textBrowser->append(QString("Work dropped :: %1").arg(status_string));
                if (status_string == "")
                {
                    ui->textBrowser->append("Work didn't drop");
                    changeImage(QString("%1/images/007.png").arg(qApp->applicationDirPath()));
                    log_barcode(barcode, "Barcode valid, work didn't drop");

                }
                else
                }
                else
                {
                    ui->textBrowser->append("Unexpected servo response: " + status_string);
                    log_barcode(barcode, "Unexpected response from server");
                }*/

                servo_open = false;
            }

            if (teensy->isOpen())
            {

            }
            else
            {
                ui->serialString->setText("00313233000");
            }

            scannerData = "";
            barcode = "";
            scanProcessing = false;

            pause(screen_cooldown);
            status_string = "";
            teensy->flush();
            scanner->flush();
            timer->start();


        }
        else
        {
            barcode_ticks--; //nothing found on this tick
        }

    }
    else
    {

        ui->textBrowser->append("Timed out waiting for barcode");
        changeImage(QString("%1/images/004.png").arg(qApp->applicationDirPath()));
        pause(screen_cooldown);

        status_string = "";
        scannerData = "";
        scanProcessing = false;
        scanner->flush();
        teensy->flush();

        if (teensy->isOpen())
        {

        }
        else
        {
            ui->serialString->setText("00313233000");
        }

        barcode_timer->stop();
        barcode_ticks = barcode_max_ticks;
        timer->start();

    }

}

void MainWindow::quitNow()
{
    qApp->exit();

}

void MainWindow::scannerRead()
{
    if (scanner->bytesAvailable())
    {
        QString data = scanner->readAll();

        if (scanProcessing == false)
        {

            if (data.endsWith("\n") && data.length() < 10)
            {
                temp_string = "";
                scannerData = "";

            }
            else if (data.endsWith("\n") && data.length() >= 10)
            {
                temp_string.append(data);

                scannerData = temp_string.mid(0,temp_string.length()-2);
                //ui->textBrowser->append(scannerData);
                scanProcessing = true;
                temp_string = "";

            }
            else
            {
                temp_string = data;
            }

        }

    }

}

void MainWindow::teensyRead()
{
    if (teensy->bytesAvailable())
    {
        QString res = QString::fromLatin1(teensy->readAll());
        res = res.mid(0,res.length()-2);

        if (res.length() != 0)
        {
            status_string = res;

        }
        else
        {
            status_string = "";

        }
        //ui->textBrowser->append(res);
    }
    else
    {
        status_string = "";
    }

}



int MainWindow::levenshtein_distance(const QString word1, const QString word2)
{
    int len1, len2;
    len1 = word1.size();
    len2 = word2.size();

    QVector<QVector<int> > d(len1+1, QVector<int>(len2+1));

    d[0][0] = 0;

    for (int i=1; i <= len1; ++i)
    {
        d[i][0] = i;
    }
    for (int j=1; j <= len2; ++j)
    {
        d[0][j] = j;
    }

    for (int i = 1; i <= len1; ++i)
    {
        for (int j = 1; j <= len2; ++j)
        {
            d[i][j] = qMin(qMin(d[i-1][j] + 1, d[i][j-1] + 1), d[i-1][j-1] + (word1.data()[i-1] == word2.data()[j-1] ? 0 : 1));
        }
    }

    return d[len1][len2];
}


void MainWindow::log_teensy(const QString &teensy_string)
{
    QDir mdir;
    mdir.mkpath(qApp->applicationDirPath() + "/logs");

    QString filename = QString("%1/logs/teensy_%2").arg(qApp->applicationDirPath(), QDateTime::currentDateTime().date().toString("yyyy-MM-dd"));

    QFile file(filename);
    file.open(QIODevice::Append | QIODevice::Text);
    QTextStream out(&file);

    QString log_data = QString("%1 \t %2 \r\n").arg(QDateTime::currentDateTime().currentDateTime().toString("yyyy-MM-dd hh:mm:ss"), teensy_string);

    out << log_data;

    file.close();
}

void MainWindow::diagnostics()
{
    // 3,0,32,37,36,0,00

    // ldr | servo moving | temp 1 | temp 2 | temp 3 | fan state | obs state


    if (teensy->isOpen())
    {
        ui->serialString->setText(status_string);
    }
    else
    {
        status_string = ui->serialString->text();
    }

    error_count = 0;
    int ldr = 0;

    if (status_string.length() == 11)
    {

        if (prevnumtest != numtest)
        {

            if ((numtest % 1500) == 0)
            {
                if (levenshtein_distance(status_string, previous_status_string) > 2)
                {
                    log_teensy(status_string);
                    previous_status_string = status_string;
                }
            }
            ldr = status_string.mid(0,1).toInt();
            int temp1 = status_string.mid(2,2).toInt();
            int temp2 = status_string.mid(4,2).toInt();
            int temp3 = status_string.mid(6,2).toInt();

            if (temp1 > 80 || temp2 > 80 || temp3 > 80)
            {
                error_count += 32;
            }

            int obstruction = status_string.mid(9,2).toInt();

            if (obstruction >= 1 && obstruction <= 3)
            {
                error_count += 1;
            }
            else if (obstruction> 3)
            {
                error_count += 2;
            }
            else
            {
                //no obstruction

            }

            prevnumtest = numtest;
        }

    }


    if (error_count == 0)
    {

        if(ldr == 0)
        {
            changeImage(QString("%1/images/001.png").arg(qApp->applicationDirPath()));
        }
        else if(ldr == 1)
        {
            //ui->textBrowser->append(QString("%1").arg(obstruction));

            changeImage(QString("%1/images/017.png").arg(qApp->applicationDirPath()));

            // scanner on, set spinup_time
            if (scanner->isOpen())
            {
                /*
                if (scanner_on == false)
                {
                    scanner->write("M");
                    scanner_on = true;
                    //pause(520);

                }*/
            }





        }
        else if(ldr == 3)
        {
            // scanner on, set spinup_time
            if (scanner->isOpen())
            {
                /*
                if (scanner_on == false)
                {
                    scanner->write("M");
                    scanner_on = true;
                    pause(520);

                }*/
            }


            changeImage(QString("%1/images/018.png").arg(qApp->applicationDirPath()));
            timer->stop();
            barcode_timer->start(barcode_interval);


        }
    }
    else if (error_count == 1)
    {
        changeImage(QString("%1/images/004.png").arg(qApp->applicationDirPath()));
    }
    else if (error_count == 2 || error_count == 3)
    {
        changeImage(QString("%1/images/010.png").arg(qApp->applicationDirPath()));
    }
    else if (error_count >= 32)
    {
        changeImage(QString("%1/images/008.png").arg(qApp->applicationDirPath()));
    }

    if ((numtest % 1500) == 0)
    {
        //ui->textBrowser->append(QString("%1").arg(status_string.length()));
        log_to_webservice(error_count);


    }


    if (teensy->isOpen())
    {
        teensy->write("a");
        teensy_statusbar = "";
    }
    else
    {
        teensy_statusbar = "no_mc ";
    }

    if (scanner->isOpen())
    {
        scanner_statusbar = "";
    }
    else
    {
        scanner_statusbar = "no_sc ";
    }

    ui->statusBar->showMessage(QString("%1%2").arg(teensy_statusbar, scanner_statusbar));

    numtest++;
}

void MainWindow::log_to_webservice(const int &error_state)
{

    QString ip;
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();

    for (int i = 0; i < ipAddressesList.size(); ++i)
    {
        if (ipAddressesList.at(i) != QHostAddress::LocalHost && ipAddressesList.at(i).toIPv4Address())
        {
            ip = ipAddressesList.at(i).toString();
            break;
        }
    }


    QString sc = QString("%1").arg(scan_count);
    QString es = QString("%1").arg(error_state);

    QUrl u = QString("%1cw/%2/%3/%4/%5/%6/%7").arg(cw_reporting_url, token, box_id, mac_address, ip, es, sc);

    QNetworkRequest request(u);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = nam->get(request);

    QEventLoop loop;
    QObject::connect(reply, SIGNAL(readyRead()), &loop, SLOT(quit()));
    QTimer webservice_timeout;
    webservice_timeout.setSingleShot(true);
    webservice_timeout.start(10000);
    connect(&webservice_timeout, SIGNAL(timeout()), &loop, SLOT(quit()));
    loop.exec();

    QByteArray result;

    if(reply->error() == QNetworkReply::NoError)
    {
        result = reply->readAll();
        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        ui->textBrowser->append(QString("HTTP Reporting Response: %1").arg(statusCode.toString()));

    }
    else
    {
        result.append(reply->errorString());
        QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        ui->textBrowser->append(QString("HTTP Reporting Response: %1").arg(statusCode.toString()));
    }
}

void MainWindow::changeImage(const QString &imagePath)
{
    QImage image;
    image.load(imagePath);
    if (!image.isNull())
    {
        ui->imageLabel->setPixmap(QPixmap::fromImage(image));
    }
}

void MainWindow::loadSettings()
{
    numtest = 0;
    testCount = 0;
    scanProcessing = false;
    temp_string = "";
    scannerData = "";
    status_string = "";
    teensy_statusbar = "";
    scanner_statusbar = "";

    timer = new QTimer(this);
    barcode_timer = new QTimer(this);

    connect(timer, SIGNAL(timeout()), this, SLOT(diagnostics()));
    connect(barcode_timer, SIGNAL(timeout()), this, SLOT(barcode_tick()));

    QSettings settings(m_sSettingsFile, QSettings::NativeFormat);

    timer->start(settings.value("diagnostic_tick", "200").toInt());
    barcode_interval = settings.value("barcode_tick", "100").toInt();
    barcode_timer->setInterval(barcode_interval);

    screen_cooldown = settings.value("countdown", "5000").toInt();
    connection_timeout = settings.value("webservice_timeout", "10000").toInt();
    box_id = settings.value("boxid", "Not defined").toString();

    cw_request_url = settings.value("lsbu_request_url", "").toString();
    cw_submit_url = settings.value("lsbu_submit_url", "").toString();
    cw_reporting_url = settings.value("reporting_webservice_address", "").toString();

    scan_count = settings.value("scan_count", "0").toLongLong();
    total_scans= settings.value("total_scans", "0").toLongLong();

    if (!teensy->isOpen())
    {
        teensy->setPortName(settings.value("serial_port_linux", "/dev/ttyACM0").toString());
        teensy->open(QIODevice::ReadWrite);
    }

    if (!scanner->isOpen())
    {
        scanner->setPortName(settings.value("barcode_port_linux", "/dev/ttyUSB0").toString());
        scanner->open(QIODevice::ReadWrite);
    }

    barcode_max_ticks = settings.value("barcode_max_ticks", "8").toInt();
    barcode_ticks = barcode_max_ticks;
    ui->dataLine->setText(settings.value("ui/dataLine", "452012125600010").toString());

    QRect sr = QApplication::desktop()->screenGeometry();

    init_imageLabel = ui->imageLabel->geometry();
    QRect fullscreen_imageLabel (0, 0, sr.width(), sr.height());

    fullscreen_width = sr.width();
    fullscreen_height = sr.height();

    init_width = MainWindow::width();
    init_height = MainWindow::height();

    bfullscreen = true;

    switch_fullscreen();

    this->statusBar()->show();

}

void MainWindow::saveSettings()
{
    QSettings settings(m_sSettingsFile, QSettings::NativeFormat);

    QString sText = (ui->dataLine) ? ui->dataLine->text() : "";


    settings.setValue("ui/dataLine", sText);
    settings.setValue("fullscreen_on_boot", bfullscreen);
    settings.setValue("scan_count", QString("%1").arg(scan_count));
    settings.setValue("total_scans", QString("%1").arg(total_scans));

}

MainWindow::~MainWindow()
{
    teensy->close();
    scanner->close();
    saveSettings();
    //write settings to web service
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type())
    {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

