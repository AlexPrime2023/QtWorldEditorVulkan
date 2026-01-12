#include "mainwindow.h"

#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *centralWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(centralWidget);

    m_vulkanWindow = new VulkanWindow();
    QWidget *vulkanWrapper = QWidget::createWindowContainer(m_vulkanWindow);

    layout->addWidget(vulkanWrapper, 1);

    setCentralWidget(centralWidget);
}

MainWindow::~MainWindow() {}
