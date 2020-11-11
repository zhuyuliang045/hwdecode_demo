#include "mainwindow.h"
#include <QHBoxLayout>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QWidget(parent)
{
    m_pHBoxLayout = new QHBoxLayout(this);

    this->setLayout(m_pHBoxLayout);

    m_pNormalView = new QGraphicsView(this);
    m_pHBoxLayout->addWidget(m_pNormalView);
    m_pNormalScene = new QGraphicsScene(this);
    m_pNormalView->setScene(m_pNormalScene);
    m_pNormalItem = new QGraphicsPixmapItem();
    m_pNormalScene->addItem(m_pNormalItem);

    m_pThread = new QThread();
    m_pDecodeThread = new DecodeThread();
    m_pDecodeThread->moveToThread(m_pThread);
    connect(m_pDecodeThread,SIGNAL(rewardPixmap(QPixmap*)),this,SLOT(freshPixmap(QPixmap*)),Qt::QueuedConnection);
    connect(m_pThread,SIGNAL(started()),m_pDecodeThread,SLOT(decode()));
    m_pThread->start();
}

MainWindow::~MainWindow()
{
}

void MainWindow::freshPixmap(QPixmap* pixmap)
{
    m_pNormalItem->setPixmap(*pixmap);
}
