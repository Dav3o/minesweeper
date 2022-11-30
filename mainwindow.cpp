#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QObject>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    gameBoard = new GameBoard();
    settings = new Settings(gameBoard, this);
    connect(ui->actionChangeGameSize, SIGNAL(triggered()), settings, SLOT(show()));
}

MainWindow::~MainWindow()
{
    delete gameBoard;
    delete settings;
    delete ui;
}
