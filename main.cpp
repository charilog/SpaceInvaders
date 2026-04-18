#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QKeyEvent>
#include <QTimer>
#include <QFont>
#include <QBrush>
#include <QPen>
#include <QColor>
#include <QRandomGenerator>
#include <QGraphicsTextItem>
#include <QMessageBox>
#include <vector>
#include <algorithm>
#include <cmath>

// ─── Constants ────────────────────────────────────────────────────────────────
static const int   W            = 900;
static const int   H            = 650;
static const int   INV_ROWS     = 4;
static const int   INV_COLS     = 11;
static const int   INV_SX       = 65;
static const int   INV_SY       = 55;
static const int   PLAYER_SPEED = 6;
static const int   BULLET_SPEED = 10;
static const int   BOMB_SPEED   = 4;
static const int   ALIEN_TICK   = 35;
static const int   BOMB_TICK    = 45;

// ─── Invader painter ─────────────────────────────────────────────────────────
class InvaderItem : public QGraphicsItem {
public:
    int type;   // 0=top(30pts) 1=mid(20pts) 2=bot(10pts)
    int anim;
    bool alive;

    InvaderItem(int t) : type(t), anim(0), alive(true) {
        setZValue(1);
    }

    QRectF boundingRect() const override { return QRectF(0,0,40,28); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        if (!alive) return;
        p->setRenderHint(QPainter::Antialiasing);

        QColor col;
        if (type == 0) col = QColor(0,255,180);
        else if (type == 1) col = QColor(80,200,255);
        else col = QColor(180,255,80);

        p->setPen(Qt::NoPen);
        p->setBrush(col);

        // Body
        p->drawRect(10,8,20,14);
        // Head
        p->drawRect(14,2,12,8);
        // Eyes
        p->setBrush(Qt::black);
        p->drawRect(16,4,4,4);
        p->drawRect(24,4,4,4);
        // Legs (animated)
        p->setBrush(col);
        if (anim == 0) {
            p->drawRect(8, 22, 6, 6);
            p->drawRect(26,22, 6, 6);
            // Arms up
            p->drawRect(4,  10, 6, 5);
            p->drawRect(30, 10, 6, 5);
        } else {
            p->drawRect(10,22, 6, 6);
            p->drawRect(24,22, 6, 6);
            // Arms down
            p->drawRect(4,  14, 6, 5);
            p->drawRect(30, 14, 6, 5);
        }
        // Antennae
        if (type == 0) {
            p->drawRect(14, 0, 3, 3);
            p->drawRect(23, 0, 3, 3);
        }
    }
};

// ─── Player ───────────────────────────────────────────────────────────────────
class PlayerItem : public QGraphicsItem {
public:
    QRectF boundingRect() const override { return QRectF(0,0,50,30); }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(Qt::NoPen);
        // Body
        p->setBrush(QColor(0,220,0));
        p->drawRect(5,12,40,18);
        // Cannon
        p->setBrush(QColor(0,255,0));
        p->drawRect(21,2,8,12);
        // Legs
        p->drawRect(3, 26, 10, 4);
        p->drawRect(37,26, 10, 4);
    }
};

// ─── Game window ──────────────────────────────────────────────────────────────
class GameWindow : public QGraphicsView {
    Q_OBJECT

    enum State { SPLASH, PLAYING, PAUSED, DEAD, GAMEOVER, WIN };

    QGraphicsScene* scene;
    State state = SPLASH;

    // Player
    PlayerItem* player;
    int playerX;
    bool keyLeft=false, keyRight=false, keySpace=false;

    // Bullet
    QGraphicsRectItem* bullet = nullptr;
    int bulletX, bulletY;
    bool bulletActive = false;

    // Invaders
    std::vector<InvaderItem*> invaders;
    int alienDir = 1;
    int alienTick = ALIEN_TICK;
    int alienAnimFrame = 0;

    // Bombs
    struct Bomb { QGraphicsEllipseItem* item; int x,y; bool active; };
    std::vector<Bomb> bombs;
    int bombTick = BOMB_TICK;

    // Shields
    struct ShieldBlock { QGraphicsRectItem* item; int hp; };
    std::vector<ShieldBlock> shields;

    // HUD
    QGraphicsTextItem* scoreText;
    QGraphicsTextItem* livesText;
    QGraphicsTextItem* levelText;
    QGraphicsTextItem* overlayText;

    // State vars
    int score=0, lives=3, level=1;
    int frame=0;
    int deadTimer=0;

    QTimer* timer;

public:
    GameWindow(QWidget* parent=nullptr) : QGraphicsView(parent) {
        setFixedSize(W, H);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        scene = new QGraphicsScene(0,0,W,H,this);
        scene->setBackgroundBrush(Qt::black);
        setScene(scene);

        // Stars background
        for (int i=0;i<120;i++) {
            int sx = QRandomGenerator::global()->bounded(W);
            int sy = QRandomGenerator::global()->bounded(H);
            int r  = QRandomGenerator::global()->bounded(2)+1;
            auto* star = scene->addEllipse(sx,sy,r,r,Qt::NoPen,
                                           QColor(255,255,255,QRandomGenerator::global()->bounded(180)+60));
            star->setZValue(0);
        }

        setupHUD();
        showSplash();

        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &GameWindow::tick);
        timer->start(16); // ~60fps
    }

private:
    // ── HUD ──────────────────────────────────────────────────────────────────
    void setupHUD() {
        QFont f("Courier New", 14, QFont::Bold);

        scoreText = scene->addText("SCORE: 000000", f);
        scoreText->setDefaultTextColor(QColor(255,255,0));
        scoreText->setPos(10, 5);
        scoreText->setZValue(10);

        levelText = scene->addText("LVL: 1", f);
        levelText->setDefaultTextColor(QColor(0,200,255));
        levelText->setPos(W/2 - 40, 5);
        levelText->setZValue(10);

        livesText = scene->addText("♥♥♥", f);
        livesText->setDefaultTextColor(QColor(255,80,80));
        livesText->setPos(W-120, 5);
        livesText->setZValue(10);

        // Divider
        scene->addLine(0,40,W,40, QPen(QColor(60,60,60),1));
        scene->addLine(0,H-50,W,H-50, QPen(QColor(60,60,60),1));

        overlayText = scene->addText("", QFont("Courier New",22,QFont::Bold));
        overlayText->setDefaultTextColor(Qt::white);
        overlayText->setPos(W/2-200, H/2-60);
        overlayText->setZValue(20);
    }

    void updateHUD() {
        scoreText->setPlainText(QString("SCORE: %1").arg(score, 6, 10, QChar('0')));
        levelText->setPlainText(QString("LVL: %1").arg(level));
        QString hearts;
        for (int i=0;i<lives;i++) hearts += "♥ ";
        livesText->setPlainText(hearts);
    }

    // ── Splash ───────────────────────────────────────────────────────────────
    void showSplash() {
        state = SPLASH;
        overlayText->setHtml(
            "<div style='text-align:center;color:#00ffcc;'>"
            "<b>SPACE INVADERS</b><br/>"
            "<span style='font-size:14px;color:#aaaaaa;'>"
            "← → : Move &nbsp; SPACE : Fire &nbsp; P : Pause<br/><br/>"
            "<span style='color:yellow;'>Press SPACE to start!</span>"
            "</span></div>"
        );
        overlayText->setTextWidth(400);
    }

    // ── Init level ───────────────────────────────────────────────────────────
    void initLevel(bool fullReset=true) {
        if (fullReset) { score=0; lives=3; level=1; }

        // Clear old items
        for (auto* inv : invaders) scene->removeItem(inv);
        invaders.clear();
        if (player) scene->removeItem(player);
        if (bullet) { scene->removeItem(bullet); bullet=nullptr; }
        for (auto& b : bombs) if (b.item) scene->removeItem(b.item);
        bombs.clear();
        if (fullReset) {
            for (auto& s : shields) scene->removeItem(s.item);
            shields.clear();
        }

        // Player
        player = new PlayerItem();
        playerX = W/2 - 25;
        player->setPos(playerX, H-80);
        scene->addItem(player);

        // Invaders
        int startX = 60, startY = 60;
        for (int r=0; r<INV_ROWS; r++) {
            int type = (r==0)?0:(r<=1?1:2);
            for (int c=0; c<INV_COLS; c++) {
                auto* inv = new InvaderItem(type);
                inv->setPos(startX + c*INV_SX, startY + r*INV_SY);
                scene->addItem(inv);
                invaders.push_back(inv);
            }
        }

        // Shields (only on full reset)
        if (fullReset) {
            const int shieldXs[] = {100,260,420,580,740};
            for (int sx : shieldXs) {
                for (int r=0;r<3;r++) for (int c=0;c<5;c++) {
                    auto* rect = scene->addRect(sx+c*14, H-110+r*14, 12,12,
                                                Qt::NoPen, QColor(0,200,0));
                    rect->setZValue(2);
                    shields.push_back({rect, 3});
                }
            }
        }

        alienDir=1; alienTick=ALIEN_TICK; alienAnimFrame=0;
        bombTick=BOMB_TICK; bulletActive=false; frame=0;
        deadTimer=0;
        overlayText->setPlainText("");
        updateHUD();
        state = PLAYING;
    }

    // ── Tick ─────────────────────────────────────────────────────────────────
    void tick() {
        if (state==SPLASH || state==GAMEOVER || state==WIN) return;
        if (state==PAUSED) return;
        if (state==DEAD) { updateDead(); return; }

        frame++;

        // Input
        if (keyLeft  && playerX > 5)       { playerX -= PLAYER_SPEED; player->setX(playerX); }
        if (keyRight && playerX < W-55)     { playerX += PLAYER_SPEED; player->setX(playerX); }
        if (keySpace && !bulletActive)      fireBullet();

        updateBullet();
        updateAliens();
        updateBombs();
        dropBombs();
        updateHUD();

        // Win check
        bool anyAlive = std::any_of(invaders.begin(),invaders.end(),[](InvaderItem* i){return i->alive;});
        if (!anyAlive) {
            level++;
            for (auto& s:shields) { scene->removeItem(s.item); }
            shields.clear();
            initLevel(false);
        }
    }

    // ── Fire ─────────────────────────────────────────────────────────────────
    void fireBullet() {
        bulletActive=true;
        bulletX = playerX+22;
        bulletY = (int)player->y()-10;
        if (!bullet) {
            bullet = scene->addRect(0,0,4,18, Qt::NoPen, QColor(255,255,0));
            bullet->setZValue(3);
        }
        bullet->setPos(bulletX, bulletY);
        bullet->setVisible(true);
    }

    void updateBullet() {
        if (!bulletActive) return;
        bulletY -= BULLET_SPEED;
        bullet->setY(bulletY);

        if (bulletY < 42) { bulletActive=false; bullet->setVisible(false); return; }

        // Hit invader?
        for (auto* inv : invaders) {
            if (!inv->alive) continue;
            QRectF br = inv->mapToScene(inv->boundingRect()).boundingRect();
            QRectF bl(bulletX, bulletY, 4, 18);
            if (br.intersects(bl)) {
                inv->alive = false;
                inv->setVisible(false);
                score += (inv->type==0?30:inv->type==1?20:10);
                bulletActive=false; bullet->setVisible(false);
                flashExplosion(br.center().x(), br.center().y());
                return;
            }
        }
        // Hit shield?
        hitShieldWith(QRectF(bulletX,bulletY,4,18), [&](){
            bulletActive=false; bullet->setVisible(false);
        });
    }

    // ── Aliens ───────────────────────────────────────────────────────────────
    void updateAliens() {
        // Animate
        if (frame%20==0) {
            alienAnimFrame ^= 1;
            for (auto* inv : invaders) { inv->anim=alienAnimFrame; inv->update(); }
        }

        alienTick--;
        if (alienTick>0) return;
        int alive = (int)std::count_if(invaders.begin(),invaders.end(),[](InvaderItem* i){return i->alive;});
        alienTick = std::max(6, ALIEN_TICK - (INV_ROWS*INV_COLS - alive));

        // Edge check
        bool hit=false;
        for (auto* inv : invaders) {
            if (!inv->alive) continue;
            if ((alienDir==1 && inv->x()+40>=W-5)||(alienDir==-1 && inv->x()<=5)) { hit=true; break; }
        }
        if (hit) {
            alienDir=-alienDir;
            for (auto* inv:invaders) if(inv->alive) inv->setY(inv->y()+18);
        } else {
            for (auto* inv:invaders) if(inv->alive) inv->setX(inv->x()+alienDir*12);
        }

        // Reached bottom?
        for (auto* inv:invaders) {
            if (inv->alive && inv->y()>H-80) { triggerGameOver(); return; }
        }
    }

    // ── Bombs ────────────────────────────────────────────────────────────────
    void dropBombs() {
        bombTick--;
        if (bombTick>0) return;
        bombTick = std::max(15, BOMB_TICK - level*3);

        // Collect bottom invaders per column
        std::vector<InvaderItem*> shooters;
        for (int c=0;c<INV_COLS;c++) {
            InvaderItem* bot=nullptr;
            for (auto* inv:invaders) {
                if (!inv->alive) continue;
                int col = qRound((inv->x()-60)/INV_SX);
                if (col==c && (!bot||inv->y()>bot->y())) bot=inv;
            }
            if (bot) shooters.push_back(bot);
        }
        if (shooters.empty()) return;
        InvaderItem* s = shooters[QRandomGenerator::global()->bounded((int)shooters.size())];
        auto* ellipse = scene->addEllipse(0,0,8,16, Qt::NoPen, QColor(255,60,60));
        ellipse->setZValue(3);
        ellipse->setPos(s->x()+16, s->y()+28);
        bombs.push_back({ellipse,(int)s->x()+16,(int)s->y()+28,true});
    }

    void updateBombs() {
        for (auto& b:bombs) {
            if (!b.active) continue;
            b.y += BOMB_SPEED;
            b.item->setY(b.y);

            if (b.y > H-45) { scene->removeItem(b.item); b.active=false; continue; }

            // Hit shield?
            bool killed=false;
            hitShieldWith(QRectF(b.x,b.y,8,16),[&](){ scene->removeItem(b.item); b.active=false; killed=true; });
            if (killed) continue;

            // Hit player?
            QRectF pr(playerX, player->y(), 50,30);
            QRectF br(b.x, b.y, 8,16);
            if (pr.intersects(br)) {
                scene->removeItem(b.item); b.active=false;
                triggerPlayerDeath();
                return;
            }
        }
        bombs.erase(std::remove_if(bombs.begin(),bombs.end(),[](const Bomb& b){return !b.active;}),bombs.end());
    }

    // ── Shield collision helper ───────────────────────────────────────────────
    void hitShieldWith(QRectF rect, std::function<void()> onHit) {
        for (auto& s:shields) {
            if (s.hp<=0) continue;
            QRectF sr = s.item->mapToScene(s.item->rect()).boundingRect();
            if (sr.intersects(rect)) {
                s.hp--;
                if (s.hp==0) s.item->setVisible(false);
                else {
                    int g = 60 + s.hp*45;
                    s.item->setBrush(QColor(0,g,0));
                }
                onHit();
                return;
            }
        }
    }

    // ── Deaths ───────────────────────────────────────────────────────────────
    void triggerPlayerDeath() {
        if (state!=PLAYING) return;
        state=DEAD; deadTimer=80;
        player->setVisible(false);
        flashExplosion(playerX+25, player->y()+15, 50);
    }

    void updateDead() {
        deadTimer--;
        if (deadTimer>0) return;
        lives--;
        if (lives<=0) { triggerGameOver(); return; }
        player->setVisible(true);
        for (auto& b:bombs) { scene->removeItem(b.item); }
        bombs.clear();
        state=PLAYING;
        updateHUD();
    }

    void triggerGameOver() {
        state=GAMEOVER;
        overlayText->setHtml(
            QString("<div style='text-align:center;color:#ff4444;'>"
                    "<b>GAME OVER</b><br/>"
                    "<span style='font-size:14px;color:white;'>Score: %1<br/><br/>"
                    "<span style='color:yellow;'>SPACE = play again &nbsp; Q = quit</span>"
                    "</span></div>").arg(score));
        overlayText->setTextWidth(400);
    }

    // ── Explosion flash ───────────────────────────────────────────────────────
    void flashExplosion(qreal cx, qreal cy, int r=20) {
        auto* expl = scene->addEllipse(cx-r,cy-r,r*2,r*2,Qt::NoPen,QColor(255,160,0,200));
        expl->setZValue(5);
        QTimer::singleShot(120,[=](){ scene->removeItem(expl); });
    }

    // ── Key events ────────────────────────────────────────────────────────────
    void keyPressEvent(QKeyEvent* e) override {
        switch(e->key()) {
        case Qt::Key_Left:  case Qt::Key_A: keyLeft=true;  break;
        case Qt::Key_Right: case Qt::Key_D: keyRight=true; break;
        case Qt::Key_Space:
            keySpace=true;
            if (state==SPLASH || state==GAMEOVER || state==WIN) { initLevel(state!=WIN); }
            break;
        case Qt::Key_P:
            if (state==PLAYING) { state=PAUSED; overlayText->setPlainText("  ⏸  PAUSED\n\nPress P to continue"); }
            else if (state==PAUSED) { state=PLAYING; overlayText->setPlainText(""); }
            break;
        case Qt::Key_Q: case Qt::Key_Escape: QApplication::quit(); break;
        }
    }
    void keyReleaseEvent(QKeyEvent* e) override {
        switch(e->key()) {
        case Qt::Key_Left:  case Qt::Key_A: keyLeft=false;  break;
        case Qt::Key_Right: case Qt::Key_D: keyRight=false; break;
        case Qt::Key_Space: keySpace=false; break;
        }
    }
};

#include "main.moc"

int main(int argc, char* argv[]) {
    QApplication app(argc,argv);
    app.setApplicationName("Space Invaders");

    GameWindow w;
    w.setWindowTitle("Space Invaders | charilog | v1.0");
    w.show();
    return app.exec();
}
