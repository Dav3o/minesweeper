#include "mon/header/gameboard.h"
#include "qtimer.h"

#include <algorithm>
#include <random>

#include <QGridLayout>
#include <QSpacerItem>
#include <QSet>

int GameBoard::DEFAULT_TILE_WIDTH = 20;

GameBoard::GameBoard(QWidget* parent, Settings *settings): QFrame(parent), settings(settings), explosionTimer(new QTimer(this))
{
    initialize();
}

GameBoard::~GameBoard()
{
    delete explosionTimer;
    delete gameLayout;
}

void GameBoard::initialize()
{
    if (!validMineCount(settings->numRows(), settings->numColumns(), settings->numMines()))
        throw std::invalid_argument("Invalid game board settings");

    this->m_numRows = settings->numRows();
    this->m_numColumns = settings->numColumns();
    this->m_numMines = settings->numMines();

    setupLayout();
    createTiles();
    addNeighbors();

    connect(this, &GameBoard::victory, [this]()
    {
        explosionTimer->setProperty("victory", true);
    });

    connect(this, &GameBoard::defeat, [this]()
    {
        explosionTimer->setProperty("victory", false);
    });

    connect(explosionTimer, &QTimer::timeout, [this]()
    {
        if (m_mines.isEmpty())
        {
            explosionTimer->stop();
            return;
        }

        Tile* mine = m_mines.values().front();
        m_mines.remove(mine);

        if (explosionTimer->property("victory").toBool())
            mine->setIcon(mine->mineIcon());
        else
        {
            if (!m_correctFlags.contains(mine))
                mine->setIcon(mine->explosionIcon());
        }
    });
}

bool GameBoard::validMineCount(unsigned int numRows, unsigned int numColums, unsigned int numMines)
{
    return numMines < numRows * numColums;
}

unsigned int GameBoard::numRows()
{
    return m_numRows;
}

unsigned int GameBoard::numColumns()
{
    return m_numColumns;
}

unsigned int GameBoard::numMines()
{
    return m_numMines;
}

void GameBoard::setupLayout()
{
    this->move(80, 90);
    this->setMinimumSize(600, 400);
    this->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    calculateTileSize();

    gameLayout = new QGridLayout();
    gameLayout->setSpacing(0);
    gameLayout->setContentsMargins(0, 0, 0, 0);
    gameLayout->setSizeConstraint(QLayout::SetFixedSize);

    unsigned int layoutWidth = m_numColumns * tileWidth;
    unsigned int layoutHeight = m_numRows * tileWidth;

    this->setFixedSize(layoutWidth, layoutHeight);
    this->setLayout(gameLayout);
}

void GameBoard::calculateTileSize()
{
    tileWidth = DEFAULT_TILE_WIDTH;
    unsigned int layoutWidth = m_numColumns * tileWidth;
    unsigned int layoutHeight = m_numRows * tileWidth;

    if (layoutWidth < this->width() || layoutHeight < this->height())
    {
            if (layoutHeight < layoutWidth)
            {
                tileWidth = this->width() / m_numColumns;
            }
            else
            {
                tileWidth = this->width() / m_numRows;
            }
    }
}

void GameBoard::createTiles()
{
    for (unsigned int row = 0; row < m_numRows; ++row)
    {
        m_tiles += QList<Tile*>{};
        for (unsigned int column = 0; column < m_numColumns; ++column)
        {
            m_tiles[row] += new Tile(row, column, tileWidth, this);
            static_cast<QGridLayout*>(this->layout())->addWidget(m_tiles[row][column], row, column);
            connect(m_tiles[row][column], &Tile::firstClick, this, &GameBoard::placeMines);
            connect(m_tiles[row][column], &Tile::flagged, [this, tile = m_tiles[row][column]](bool isMine)
            {
                if (isMine)
                    m_correctFlags.insert(tile);
                else
                    m_incorrectFlags.insert(tile);
                checkVictory();
            });
            connect(m_tiles[row][column], &Tile::unFlagged, [this, tile = m_tiles[row][column]](bool isMine)
            {
                if (isMine)
                    m_correctFlags.remove(tile);
                else
                    m_incorrectFlags.remove(tile);
                checkVictory();
            });
            connect(m_tiles[row][column], &Tile::revealed, [this, tile = m_tiles[row][column]]()
            {
                m_revealedTiles.insert(tile);
                checkVictory();
            });
            connect(m_tiles[row][column], &Tile::detonated, this, &GameBoard::defeatAnimation);
            connect(this, &GameBoard::defeat, m_tiles[row][column], &Tile::disable);
            connect(this, &GameBoard::victory, m_tiles[row][column], &Tile::disable);
        }
    }
    m_tiles[0][0]->setDown(true);
}

void GameBoard::quit()
{
    if (m_mines.empty())
    {
        return;
    }
    emit m_mines.values().at(0)->detonated();
}

void GameBoard::addNeighbors()
{
    for (unsigned int row = 0; row < m_numRows; ++row)
    {
        for (unsigned int column = 0; column < m_numColumns; ++column)
        {
            Tile* tile = m_tiles[row][column];
            unsigned int lastRow = m_numRows - 1;
            unsigned int lastColumn = m_numColumns - 1;

            if (row && column)                          tile->addNeighbor(m_tiles[row - 1][column - 1]);
            if (row)                                    tile->addNeighbor(m_tiles[row - 1][column]);
            if (row && column < lastColumn)             tile->addNeighbor(m_tiles[row - 1][column + 1]);
            if (column < lastColumn)                    tile->addNeighbor(m_tiles[row][column + 1]);
            if (row < lastRow && column < lastColumn)	tile->addNeighbor(m_tiles[row + 1][column + 1]);
            if (row < lastRow)                          tile->addNeighbor(m_tiles[row + 1][column]);
            if (row < lastRow && column)                tile->addNeighbor(m_tiles[row + 1][column - 1]);
            if (column)                                 tile->addNeighbor(m_tiles[row][column - 1]);
        }
    }
}

void GameBoard::checkVictory()
{
    if (!m_victory)
    {
        emit flagCountChanged(m_correctFlags.size() + m_incorrectFlags.size());
        if ((m_revealedTiles.size() == m_numColumns * m_numRows - m_numMines) && m_incorrectFlags.isEmpty())
        {
            emit victory();
            m_victory = true;
            QTimer::singleShot(0, explosionTimer, [this]()
            {
                explosionTimer->start(25);
            });
        }
    }
}

void GameBoard::defeatAnimation()
{
    Tile* sender = dynamic_cast<Tile*>(this->sender());

    QTimer::singleShot(350, this, [sender]()
    {
        sender->setIcon(sender->explosionIcon());
    });

    QTimer::singleShot(500, this, [this]()
    {
        for (auto wrong : qAsConst(m_incorrectFlags))
        {
            wrong->setStyleSheet("background-color: red;");
        }
        for (auto correct : qAsConst(m_correctFlags))
        {
            correct->setStyleSheet("background-color: blue;");
        }
        for (auto mine : qAsConst(m_mines))
        {
            disconnect(mine, &Tile::detonated, this, &GameBoard::defeatAnimation);
            if (!mine->isFlagged())
                emit mine->reveal();
        }
        emit defeat();
    });

    QTimer::singleShot(1000, explosionTimer, [this]()
    {
        explosionTimer->start(25);
    });
}

void GameBoard::placeMines(Tile* firstClicked)
{
    QList<Tile*> tiles;
    QSet<Tile*> doneUse;
    doneUse += firstClicked;
    for (Tile* neighbor: firstClicked->neighbors())
    {
        doneUse += neighbor;
    }

    for (unsigned int row = 0; row < m_numRows; ++row)
    {
        m_tiles += QList<Tile*>{};
        for (unsigned int column = 0; column < m_numColumns; ++column)
        {
            if (Tile *tile = m_tiles[row][column]; !doneUse.contains(tile))
                tiles += tile;
        }
    }

    std::random_device rd;
    std::mt19937 generator(rd());

    std::shuffle(tiles.begin(), tiles.end(), generator);

    for (unsigned int i = 0; i < m_numMines; ++i)
    {
        tiles[i]->placeMine(true);
        m_mines.insert(tiles[i]);
    }

    emit initialized();
}
