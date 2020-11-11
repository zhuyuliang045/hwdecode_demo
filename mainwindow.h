#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QThread>
#include "decodethread.h"

class QHBoxLayout;


class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void startDecode();

public slots:
    void freshPixmap(QPixmap* pixmap);

private:
    QGraphicsView        *m_pNormalView;
    QGraphicsScene       *m_pNormalScene;
    QGraphicsPixmapItem *m_pNormalItem;
    QHBoxLayout* m_pHBoxLayout;
    QThread* m_pThread = nullptr;
    DecodeThread* m_pDecodeThread = nullptr;
};

#endif // MAINWINDOW_H
