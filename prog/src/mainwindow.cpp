/**
 * @file mainwindow.cpp
 * @brief Базовая реализация функционала работы с китайской погремушкой TIOP-01.
 *
 * Реализация основного функционала: подключение к устройству, получение изображения,
 * необходимый минимум интерфейсных штук
 *
 * @date 27-02-2025
 *
 * @version 1.0
 *
 * @note Нужен OpenCV
 * @author Евгений Большаков <bolshakov.evgeniy@gmail.com>
 */

#include "mainwindow.h"

#define TIOP_MATRIX_W   32
#define TIOP_MATRIX_H   32
#define IMG_BUF_SIZE TIOP_MATRIX_W*TIOP_MATRIX_H*sizeof(CV_16SC1)

/**
 * @brief Конструктор класса MainWindow.
 * Инициализирует объекты пользовательского интерфейса, таймер и соединяет сигналы с слотами.
 * @param parent Указатель на родительский виджет.
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
    timer(new QTimer(this))
{
    ui->setupUi(this);

    connect(timer, &QTimer::timeout, this, &MainWindow::captureFrame);
    // Заполняем комбобокс портами
    populateSerialPorts();
    // Заполняем комбобокс цветовыми картами
    populateColorMaps();

    // Подключаем сигналы
    connect(ui->portComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MainWindow::onPortChanged);
    connect(ui->colorMapComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &MainWindow::onColorMapChanged);


    connect(ui->horFlipCheckBox,
            &QCheckBox::stateChanged,
            this,
            &MainWindow::onHorFlipStateChanged);
    // Устанавливаем первое значение для порта и цветовой карты
    onPortChanged(0);
}

/**
 * @brief Деструктор класса MainWindow.
 */
MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief Заполняет комбобокс доступными последовательными портами.
 */
void MainWindow::populateSerialPorts()
{
    const auto availablePorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &portInfo : availablePorts) {
        ui->portComboBox->addItem(portInfo.portName());
    }
}

/**
 * @brief Заполняет комбобокс доступными цветовыми картами.
 */
void MainWindow::populateColorMaps()
{
    const QVector<QPair<QString, int>> colorMapList = {
        {"Jet", cv::COLORMAP_JET},
        {"Hot", cv::COLORMAP_HOT},
        {"Cool", cv::COLORMAP_COOL},
        {"Hsv", cv::COLORMAP_HSV}
    };

    for (const auto &pair : colorMapList) {
        ui->colorMapComboBox->addItem(pair.first, pair.second);
    }

    ui->colorMapComboBox->setCurrentIndex(1);
    currentColorMap = 1;
}

/**
 * @brief MainWindow::onHorFlipStateChanged
 * @details Изменяет состояние флипа по горизонтали
 * @param state Состояние чекбокса (крутим или нет).
 */
void MainWindow::onHorFlipStateChanged(int state)
{
    // Этот метод может быть пустым, так как состояние будет проверяться в captureFrame
}

/**
 * @brief Устанавливает имя последовательного порта и открывает его.
 * @param portName Имя последовательного порта.
 */
void MainWindow::setPortName(const QString &portName)
{
    serialPort.setPortName(portName);
    if (!serialPort.open(QIODevice::ReadWrite)) {
        qDebug() << "Failed to open port:" << portName;
        return;
    }

    /* Удивидельный китайский тепловизор работает по ком-порту:
     * 921600 baud
     * 8 data
     * 1 stop
     * 0 parity
     * 0 flow control
     */

    serialPort.setBaudRate(921600);

    // Установка эмиссивности

    /* Единственная известная команда управления - задание параметра "Эмиссивность"
     * Найдена вот тут - https://github.com/geefr/tinythermalcam
     * Only known command is to set emissivity constant - Send 4 bytes
     * 85
     * 1
     * emissivity constant (0-100)
     * checksum (85 + 1 + emissivity constant)
     */

    QByteArray msg;
    msg.append(static_cast<char>(85));
    msg.append(static_cast<char>(1));
    msg.append(static_cast<char>(0));
    msg.append(static_cast<char>(0));

    msg[2] = static_cast<char>(int(0.59 * 100));
    char checksum = msg[0] + msg[1] + msg[2];
    msg[3] = checksum;
    serialPort.write(msg);
}


/**
 * @brief Захватывает кадр с теплака и обрабатывает его.
 * @details Метод проверяет доступность данных в последовательном порту,
 * считывает их и преобразует в матрицу OpenCV.
 */
void MainWindow::captureFrame()
{
    // Поток приходит из уарта кадрами по 2048 байт - 32x32x2 байта (int16)
    if (serialPort.bytesAvailable() < static_cast<qint64>(IMG_BUF_SIZE)) {
        return;
    }

    QByteArray buffer = serialPort.read(IMG_BUF_SIZE);
    cv::Mat img(TIOP_MATRIX_W, TIOP_MATRIX_H, CV_16SC1, const_cast<char*>(buffer.data()));

    processFrame(img);
}

void MainWindow::processFrame(const cv::Mat &img)
{
    if (img.empty()) {
        qDebug() << "Картинка йок";
        return;
    }

    // Проверяем состояние чекбокса для переворота по горизонтали
    if (ui->horFlipCheckBox->isChecked()) {
        cv::flip(img, img, 1); // Переворот по горизонтали
    } else {
        cv::flip(img, img, -1); // Нет переворота по горизонтали
    }


    cv::flip(img, img, 0);

    // Convert to Celsius
    cv::Mat img_celsius;
    img.convertTo(img_celsius, CV_32FC1, 1.0 / 10.0);

    // Температура в цельсиях - (значение пикселя)/10
    double minTemp, maxTemp;
    cv::minMaxLoc(img_celsius, &minTemp, &maxTemp);
    qDebug() << "Min:" << minTemp << "Max:" << maxTemp;

    // Сглаживаем и применяем цветовую карту
    cv::Mat smoothed_img;
    cv::GaussianBlur(img_celsius, smoothed_img, cv::Size(3, 3), 0);

    cv::Mat img_vis;
    cv::normalize(smoothed_img, img_vis, 0, 255, cv::NORM_MINMAX);
    img_vis.convertTo(img_vis, CV_8UC1);
    cv::applyColorMap(img_vis, img_vis, currentColorMap);
    cv::bitwise_not(img_vis, img_vis);

    // Фильтрация шума с помощью медианного фильтра
    cv::Mat denoised_img;
    cv::medianBlur(img_vis, denoised_img, 3); // Using median filter

    // Улучшение контраста и яркости с помощью выравнивания гистограммы
    cv::Mat equalized_img;
    if (currentColorMap != -1) {
        cv::cvtColor(denoised_img, equalized_img, cv::COLOR_BGR2YCrCb);
        std::vector<cv::Mat> channels;
        cv::split(equalized_img, channels);
        cv::equalizeHist(channels[0], channels[0]);
        cv::merge(channels, equalized_img);
        cv::cvtColor(equalized_img, equalized_img, cv::COLOR_YCrCb2BGR);
    } else {
        cv::equalizeHist(denoised_img, equalized_img);
    }

    // Растягиваем обработанное изображение до 512х512
    cv::Mat resized_img;
    cv::resize(equalized_img, resized_img, cv::Size(512, 512), 0, 0, cv::INTER_LINEAR);

    QImage qImage(reinterpret_cast<const unsigned char*>(resized_img.data),
                  resized_img.cols, resized_img.rows,
                  static_cast<int>(resized_img.step), QImage::Format_RGB888);
    ui->imageLabel->setPixmap(QPixmap::fromImage(qImage));

    // Растягиваем оригинальное изображение до 512х512
    cv::Mat resized_original_img;
    cv::resize(img_celsius, resized_original_img, cv::Size(512, 512), 0, 0, cv::INTER_LINEAR);
    // Нормализуем данные в диапазон [0, 255]
    cv::normalize(resized_original_img, resized_original_img, 0, 255, cv::NORM_MINMAX);
    resized_original_img.convertTo(resized_original_img, CV_8UC1);

    QImage qOriginalImage(reinterpret_cast<const unsigned char*>(resized_original_img.data),
                          resized_original_img.cols, resized_original_img.rows,
                          static_cast<int>(resized_original_img.step), QImage::Format_Grayscale8);
    ui->originalImageLabel->setPixmap(QPixmap::fromImage(qOriginalImage));
}

/**
 * @brief MainWindow::onPortChanged
 * Обрабатывает изменение выбранного порта в комбобоксе.
 * Закрывает текущий открытый порт, устанавливает новый и начинает таймер для чтения данных.
 * @param index Индекс выбранного элемента в комбобоксе.
 */
void MainWindow::onPortChanged(int index)
{
    if (serialPort.isOpen()) {
        serialPort.close();
    }

    setPortName(ui->portComboBox->itemText(index));
    timer->start(100); // Adjust interval as needed
}

/**
 * @brief MainWindow::onColorMapChanged
 * Обрабатывает изменение выбранной цветовой карты в комбобоксе.
 * Обновляет текущую цветовую карту и перерисовывает изображение с новой картой.
 */
void MainWindow::onColorMapChanged()
{
    currentColorMap = ui->colorMapComboBox->currentData().toInt();
}


/**
 * @brief Обрабатывает нажатие клавиш.
 * В данном примере обрабатывается только клавиша Esc для закрытия приложения.
 * @param event Событие нажатия клавиши.
 */
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
    }
}
