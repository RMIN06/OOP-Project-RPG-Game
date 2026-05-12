/*
=============================================================================
  VroomsteinRPG.cpp
  OOP Semester 2 Project — BSCS 2E — NUST Faisalabad
  Single-file SFML game: All graphics procedurally drawn (no assets needed)
=============================================================================

  HOW TO COMPILE & RUN (VS Code terminal):
  -----------------------------------------
  Windows (MinGW-w64):
    g++ -o VroomsteinRPG VroomsteinRPG.cpp -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system -std=c++17
    .\VroomsteinRPG.exe

  Linux:
    g++ -o VroomsteinRPG VroomsteinRPG.cpp -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system -std=c++17
    ./VroomsteinRPG

  Make sure SFML 2.x is installed and headers/libs are on your path.

=============================================================================
  CONTROLS:
    Arrow Keys / WASD  — Move hero
    S                  — Melee attack
    E                  — Ranged attack (fires projectile)
    F                  — Special attack (AoE burst)
    1 / 2 / 3         — Select hero on menu
    ESC                — Pause / Resume
    Enter              — Confirm on screens
=============================================================================
  OOP STRUCTURE CHECKLIST:
    [x] Abstract base class: Avatar  (pure virtual move/attack/render)
    [x] Abstract base class: Move    (pure virtual trigger/getName)
    [x] Inheritance: Hero (x3) -> Avatar
    [x] Inheritance: Villain -> Avatar
    [x] Inheritance: Minion (x3) -> Avatar
    [x] Inheritance: MeleeMove, RangedMove, SpecialMove -> Move
    [x] Polymorphism: std::vector<Avatar*>, virtual dispatch
    [x] Encapsulation: private members, public getters/setters
    [x] Collision detection: AABB
    [x] Game loop: fixed timestep
    [x] Score system, HUD, cooldowns, flash messages
    [x] Minion formations: V-shape, Wave, Line
    [x] Boss AI: FSM with random attack patterns + minion summons
=============================================================================
*/

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  CONSTANTS
// ─────────────────────────────────────────────────────────────────────────────
const unsigned SCREEN_W   = 1280;
const unsigned SCREEN_H   = 720;
const float    FIXED_DT   = 1.f / 60.f;
const float    PI         = 3.14159265f;

// ─────────────────────────────────────────────────────────────────────────────
//  COLOUR PALETTE
// ─────────────────────────────────────────────────────────────────────────────
namespace Pal {
    const sf::Color BG          (15,  12,  30);
    const sf::Color BG2         (25,  20,  50);
    const sf::Color HeroA       (80,  180, 255);
    const sf::Color HeroB       (80,  255, 160);
    const sf::Color HeroC       (255, 200,  80);
    const sf::Color Villain     (220,  60,  60);
    const sf::Color MinionV     (200,  80, 220);
    const sf::Color MinionW     (255, 140,  40);
    const sf::Color MinionL     (180, 220,  60);
    const sf::Color Projectile  (255, 255, 100);
    const sf::Color EnemyProj   (255,  80,  80);
    const sf::Color HPGreen     (50,  220, 100);
    const sf::Color HPRed       (220,  50,  50);
    const sf::Color HPOrange    (255, 160,  30);
    const sf::Color Gold        (255, 220,  60);
    const sf::Color White       (255, 255, 255);
    const sf::Color DimWhite    (200, 200, 200);
    const sf::Color Teal        (0,   200, 200);
    const sf::Color Shield      (80,  140, 255);
    const sf::Color AoEFlash    (255, 100, 255);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UTILITY
// ─────────────────────────────────────────────────────────────────────────────
inline float clampf(float v, float lo, float hi){ return v<lo?lo:v>hi?hi:v; }
inline float lerpf (float a, float b, float t)  { return a + (b-a)*t; }
inline float randf (float lo, float hi)          { return lo+(hi-lo)*(rand()/(float)RAND_MAX); }
inline float length(sf::Vector2f v){ return std::sqrt(v.x*v.x+v.y*v.y); }
inline sf::Vector2f normalize(sf::Vector2f v){
    float l=length(v); return l>0.f?sf::Vector2f(v.x/l,v.y/l):sf::Vector2f(0,0);
}
inline bool aabbHit(sf::FloatRect a, sf::FloatRect b){ 
    return (a.position.x < b.position.x + b.size.x && a.position.x + a.size.x > b.position.x && 
            a.position.y < b.position.y + b.size.y && a.position.y + a.size.y > b.position.y);
}

sf::Color lerpColor(sf::Color a, sf::Color b, float t){
    return sf::Color(
        (unsigned char)lerpf(a.r,b.r,t),
        (unsigned char)lerpf(a.g,b.g,t),
        (unsigned char)lerpf(a.b,b.b,t),
        255
    );
}

sf::Color hpColor(float ratio){
    if(ratio>0.5f) return lerpColor(Pal::HPOrange, Pal::HPGreen, (ratio-0.5f)*2.f);
    return lerpColor(Pal::HPRed, Pal::HPOrange, ratio*2.f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  FLASH MESSAGE SYSTEM
// ─────────────────────────────────────────────────────────────────────────────
struct FlashMsg {
    std::string text;
    sf::Color   color;
    float       ttl;
    float       maxTTL;
    sf::Vector2f pos;
};
std::deque<FlashMsg> gFlash;

void pushFlash(const std::string& t, sf::Color c={255,255,255}, sf::Vector2f p={640,320}){
    gFlash.push_back({t, c, 1.8f, 1.8f, p});
    if(gFlash.size()>6) gFlash.pop_front();
}

// ─────────────────────────────────────────────────────────────────────────────
//  SCORE SYSTEM
// ─────────────────────────────────────────────────────────────────────────────
struct ScoreSystem {
    int   total       = 0;
    int   streak      = 0;
    float streakMult  = 1.f;

    void addHit(float damage, float moveMult){
        streak++;
        streakMult = 1.f + (streak/10)*0.5f;
        int pts = (int)(damage * moveMult * streakMult);
        total  += pts;
    }
    void resetStreak(){ streak=0; streakMult=1.f; }
    std::string str() const {
        std::ostringstream ss; ss<<"SCORE: "<<total; return ss.str();
    }
} gScore;

// ─────────────────────────────────────────────────────────────────────────────
//  PROJECTILE
// ─────────────────────────────────────────────────────────────────────────────
struct Projectile {
    sf::Vector2f pos;
    sf::Vector2f vel;
    float        damage;
    float        radius;
    bool         active;
    bool         friendly;   // true = hero fired, false = enemy fired
    bool         piercing;
    sf::Color    color;

    sf::FloatRect bounds() const {
        return sf::FloatRect(sf::Vector2f(pos.x-radius, pos.y-radius), sf::Vector2f(radius*2, radius*2));
    }
};
std::vector<Projectile> gProjectiles;

void spawnProjectile(sf::Vector2f pos, sf::Vector2f vel, float dmg,
                     bool friendly, bool piercing=false,
                     float radius=8.f, sf::Color col=Pal::Projectile){
    gProjectiles.push_back({pos,vel,dmg,radius,true,friendly,piercing,col});
}

// ─────────────────────────────────────────────────────────────────────────────
//  POWER-UP
// ─────────────────────────────────────────────────────────────────────────────
enum class PowerKind { Shield, AttackBoost, SpeedBoost };
struct PowerUp {
    sf::Vector2f pos;
    PowerKind    kind;
    bool         active;
    float        pulseT;
};
std::vector<PowerUp> gPowerUps;

sf::Texture gHeroTextures[3];
sf::Texture gMinionTexture;
sf::Texture gVroomsteinTexture;

void spawnPowerUp(){
    PowerKind k = (PowerKind)(rand()%3);
    sf::Vector2f p(randf(80,SCREEN_W-80), randf(150,SCREEN_H-80));
    gPowerUps.push_back({p,k,true,0.f});
}

// ─────────────────────────────────────────────────────────────────────────────
//  ABSTRACT BASE: Move
// ─────────────────────────────────────────────────────────────────────────────
class Move {
protected:
    std::string name;
    float       damage;
    float       cooldown;
    float       range;
    int         animState;
    sf::Clock   clock;

public:
    Move(const std::string& n, float dmg, float cd, float rng)
        : name(n), damage(dmg), cooldown(cd), range(rng), animState(0) {}
    virtual ~Move(){}

    virtual void        trigger(sf::Vector2f from, sf::Vector2f dir) = 0;
    virtual std::string getName()  const = 0;
    virtual float       getMult()  const = 0;  // score multiplier

    bool  isReady()  const { return clock.getElapsedTime().asSeconds() >= cooldown; }
    float getCooldownRatio() const {
        return clampf(clock.getElapsedTime().asSeconds()/cooldown, 0.f, 1.f);
    }
    float    getDamage()   const { return damage; }
    float    getCooldown() const { return cooldown; }
    void     resetClock()        { clock.restart(); }
    std::string getDisplayName() const { return name; }
};

// ─── MeleeMove ───────────────────────────────────────────────────────────────
class MeleeMove : public Move {
    float lifesteal;
public:
    float heroHealRef; // set externally to hero's currentHP pointer proxy
    std::function<void(float)> onHeal;

    MeleeMove(const std::string& n, float dmg, float cd, float ls)
        : Move(n,dmg,cd,80.f), lifesteal(ls) {}

    void trigger(sf::Vector2f from, sf::Vector2f dir) override {
        if(!isReady()) return;
        // Melee: spawn a very fast short-lived projectile (acts as hitbox)
        sf::Vector2f vel = normalize(dir)*600.f;
        spawnProjectile(from, vel, damage, true, false, 18.f, sf::Color(255,220,80));
        resetClock();
        if(onHeal) onHeal(damage * lifesteal);
    }
    std::string getName() const override { return "MELEE"; }
    float getMult() const override { return 1.0f; }
    float getLifesteal() const { return lifesteal; }
};

// ─── RangedMove ──────────────────────────────────────────────────────────────
class RangedMove : public Move {
    bool piercing;
public:
    RangedMove(const std::string& n, float dmg, float cd, bool pierce=false)
        : Move(n,dmg,cd,400.f), piercing(pierce) {}

    void trigger(sf::Vector2f from, sf::Vector2f dir) override {
        if(!isReady()) return;
        sf::Vector2f vel = normalize(dir)*520.f;
        spawnProjectile(from, vel, damage, true, piercing, 10.f, Pal::Projectile);
        resetClock();
    }
    std::string getName() const override { return "RANGED"; }
    float getMult() const override { return 1.2f; }
};

// ─── SpecialMove ─────────────────────────────────────────────────────────────
class SpecialMove : public Move {
    float aoeRadius;
    bool  isAoE;
public:
    // AoE burst: spawns 8 projectiles in a ring
    SpecialMove(const std::string& n, float dmg, float cd, float rad)
        : Move(n,dmg,cd,rad), aoeRadius(rad), isAoE(true) {}

    void trigger(sf::Vector2f from, sf::Vector2f /*dir*/) override {
        if(!isReady()) return;
        // Fire 8 projectiles in all directions
        for(int i=0;i<8;i++){
            float angle = (PI*2.f/8.f)*i;
            sf::Vector2f vel(std::cos(angle)*380.f, std::sin(angle)*380.f);
            spawnProjectile(from, vel, damage, true, false, 14.f, Pal::AoEFlash);
        }
        pushFlash("SPECIAL BURST!", Pal::AoEFlash, from);
        resetClock();
    }
    std::string getName() const override { return "SPECIAL"; }
    float getMult() const override { return 2.0f; }
    float getAoERadius() const { return aoeRadius; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ABSTRACT BASE: Avatar
// ─────────────────────────────────────────────────────────────────────────────
class Avatar {
protected:
    std::string  name;
    float        baseSpeed;
    float        maxHP;
    float        currentHP;
    sf::Vector2f pos;
    float        bodyRadius;
    bool         dead;
    float        hitFlash;   // seconds to flash white on hit
    const sf::Texture* spriteTexture;

public:
    Avatar(const std::string& n, float spd, float hp, sf::Vector2f startPos, float r, const sf::Texture* tex = nullptr)
        : name(n), baseSpeed(spd), maxHP(hp), currentHP(hp),
          pos(startPos), bodyRadius(r), dead(false), hitFlash(0.f), spriteTexture(tex) {}
    const sf::Texture* getTexture() const { return spriteTexture; }
    virtual ~Avatar(){}

    // Pure virtuals
    virtual void move(float dt, sf::Vector2f input) = 0;
    virtual void attack(sf::Vector2f dir)            = 0;
    virtual void render(sf::RenderWindow& win)       = 0;

    // Common
    virtual void takeDamage(float dmg){
        if(dead) return;
        currentHP -= dmg;
        hitFlash   = 0.12f;
        if(currentHP <= 0.f){ currentHP=0.f; dead=true; }
    }
    void healHP(float amt){
        currentHP = clampf(currentHP+amt, 0.f, maxHP);
    }
    bool  isDead()       const { return dead; }
    float getHP()        const { return currentHP; }
    float getMaxHP()     const { return maxHP; }
    float getHPRatio()   const { return currentHP/maxHP; }
    sf::Vector2f getPos()const { return pos; }
    float getRadius()    const { return bodyRadius; }
    std::string getName()const { return name; }

    sf::FloatRect bounds() const {
        return sf::FloatRect(sf::Vector2f(pos.x-bodyRadius, pos.y-bodyRadius), sf::Vector2f(bodyRadius*2, bodyRadius*2));
    }

    // Bounds clamp — clamp to screen
    void clampToScreen(){
        pos.x = clampf(pos.x, bodyRadius, SCREEN_W-bodyRadius);
        pos.y = clampf(pos.y, 120.f+bodyRadius, SCREEN_H-bodyRadius);
    }

    // Draw HP bar
    void drawHPBar(sf::RenderWindow& win, float barW=80.f, float yOff=-999.f) const {
        if(yOff <= -998.f) yOff = -bodyRadius - 14.f;
        float ratio  = getHPRatio();
        float bx     = pos.x - barW/2.f;
        float by     = pos.y + yOff;

        sf::RectangleShape bg({barW, 8.f});
        bg.setPosition(sf::Vector2f(bx, by));
        bg.setFillColor(sf::Color(40,40,40));
        win.draw(bg);

        sf::RectangleShape fill({barW*ratio, 8.f});
        fill.setPosition(sf::Vector2f(bx, by));
        fill.setFillColor(hpColor(ratio));
        win.draw(fill);
    }

    void tickFlash(float dt){ if(hitFlash>0.f) hitFlash -= dt; }
    sf::Color flashTint() const { return hitFlash>0.f ? sf::Color::White : sf::Color(0,0,0,0); }
};

// ─────────────────────────────────────────────────────────────────────────────
//  HERO BASE (still abstract — subclasses implement render)
// ─────────────────────────────────────────────────────────────────────────────
class Hero : public Avatar {
protected:
    MeleeMove*  meleeMove;
    RangedMove* rangedMove;
    SpecialMove* specialMove;
    float       speedMult;
    bool        shielded;
    float       attackBoost;
    float       speedBoost;
    float       powerTimer;
    sf::Color   bodyColor;
    float       animT;

public:
    Hero(const std::string& n, float spd, float hp,
         sf::Vector2f startPos, sf::Color col, const sf::Texture* tex=nullptr)
        : Avatar(n, spd, hp, startPos, 22.f, tex),
          meleeMove(nullptr), rangedMove(nullptr), specialMove(nullptr),
          speedMult(1.f), shielded(false), attackBoost(1.f), speedBoost(1.f),
          powerTimer(0.f), bodyColor(col), animT(0.f) {}

    ~Hero(){
        delete meleeMove; delete rangedMove; delete specialMove;
    }

    void move(float dt, sf::Vector2f input) override {
        float spd = baseSpeed * speedMult * speedBoost;
        float len = length(input);
        if(len > 0.f) input = normalize(input);
        pos += input * spd * dt;
        clampToScreen();
        animT += dt * 4.f;
    }

    void attack(sf::Vector2f dir) override { (void)dir; /* handled per-key */ }

    void doMelee(sf::Vector2f dir){
        if(meleeMove && meleeMove->isReady())
            meleeMove->trigger(pos, dir);
    }
    void doRanged(sf::Vector2f dir){
        if(rangedMove && rangedMove->isReady())
            rangedMove->trigger(pos, dir);
    }
    void doSpecial(){
        if(specialMove && specialMove->isReady())
            specialMove->trigger(pos, {1,0});
    }

    void applyPowerUp(PowerKind k){
        switch(k){
        case PowerKind::Shield:
            shielded=true; powerTimer=8.f;
            pushFlash("SHIELD ACTIVATED!", Pal::Shield, pos);
            break;
        case PowerKind::AttackBoost:
            attackBoost=1.5f; powerTimer=10.f;
            pushFlash("ATTACK BOOST x1.5!", Pal::Gold, pos);
            break;
        case PowerKind::SpeedBoost:
            speedBoost=1.5f; powerTimer=8.f;
            pushFlash("SPEED BOOST!", Pal::HeroB, pos);
            break;
        }
    }

    void takeDamage(float dmg) override {
        if(shielded){ shielded=false; pushFlash("SHIELD BLOCKED!", Pal::Shield, pos); return; }
        gScore.resetStreak();
        Avatar::takeDamage(dmg);
    }

    void tickPowers(float dt){
        if(powerTimer > 0.f){
            powerTimer -= dt;
            if(powerTimer <= 0.f){
                shielded=false; attackBoost=1.f; speedBoost=1.f; powerTimer=0.f;
            }
        }
    }

    float getAttackBoost() const { return attackBoost; }
    bool  isShielded()     const { return shielded; }

    Move* getMeleeMove()   const { return meleeMove; }
    Move* getRangedMove()  const { return rangedMove; }
    Move* getSpecialMove() const { return specialMove; }

    void render(sf::RenderWindow& win) override {
        const sf::Texture* tex = getTexture();
        if(tex){
            sf::Sprite sprite(*tex);
            sprite.setOrigin(sf::Vector2f(tex->getSize().x/2.f, tex->getSize().y/2.f));
            float scale = (bodyRadius*2.f) / std::max(tex->getSize().x, tex->getSize().y);
            sprite.setScale(sf::Vector2f(scale, scale));
            sprite.setPosition(pos);
            if(hitFlash > 0.f) sprite.setColor(sf::Color::White);
            win.draw(sprite);
        } else {
            // Body
            sf::CircleShape body(bodyRadius);
            body.setOrigin(sf::Vector2f(bodyRadius, bodyRadius));
            body.setPosition(pos);
            body.setFillColor(bodyColor);
            if(hitFlash > 0.f) body.setFillColor(sf::Color::White);
            win.draw(body);

            // Visor/eyes
            sf::CircleShape eye(5.f);
            eye.setFillColor(sf::Color(20,20,30));
            eye.setOrigin(sf::Vector2f(5.f,5.f));
            eye.setPosition(sf::Vector2f(pos.x-7.f, pos.y-5.f));
            win.draw(eye);
            eye.setPosition(sf::Vector2f(pos.x+3.f, pos.y-5.f));
            win.draw(eye);

            // Legs animation
            float legOff = std::sin(animT)*8.f;
            sf::RectangleShape leg1({6.f,14.f});
            leg1.setOrigin(sf::Vector2f(3.f,0.f));
            leg1.setFillColor(sf::Color(bodyColor.r/2,bodyColor.g/2,bodyColor.b/2));
            leg1.setPosition(sf::Vector2f(pos.x-8.f, pos.y+bodyRadius-4.f));
            leg1.setRotation(sf::degrees(legOff*2.f));
            win.draw(leg1);
            sf::RectangleShape leg2({6.f,14.f});
            leg2.setOrigin(sf::Vector2f(3.f,0.f));
            leg2.setFillColor(sf::Color(bodyColor.r/2,bodyColor.g/2,bodyColor.b/2));
            leg2.setPosition(sf::Vector2f(pos.x+2.f, pos.y+bodyRadius-4.f));
            leg2.setRotation(sf::degrees(-legOff*2.f));
            win.draw(leg2);
        }

        // Shield ring
        if(shielded){
            sf::CircleShape ring(bodyRadius+6.f);
            ring.setOrigin(sf::Vector2f(bodyRadius+6.f,bodyRadius+6.f));
            ring.setPosition(pos);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(Pal::Shield);
            ring.setOutlineThickness(3.f);
            win.draw(ring);
        }

        drawHPBar(win, 80.f);
    }
};

// ─── HeroA: The Paladin (blue, high HP, lifesteal melee) ─────────────────────
class HeroA : public Hero {
public:
    HeroA() : Hero("PALADIN", 200.f, 300.f, {200, 400}, Pal::HeroA, &gHeroTextures[0])
     {
        meleeMove   = new MeleeMove("Shield Bash", 35.f, 0.4f, 0.15f);
        rangedMove  = new RangedMove("Holy Bolt",  22.f, 0.6f, false);
        specialMove = new SpecialMove("Radiant Nova",50.f,3.5f,180.f);
        meleeMove->onHeal = [this](float amt){ healHP(amt); };
    }
};

// ─── HeroB: The Ranger (green, fast, piercing arrows) ────────────────────────
class HeroB : public Hero {
public:
    HeroB() : Hero("RANGER", 270.f, 220.f, {200, 400}, Pal::HeroB, &gHeroTextures[1]) {
        meleeMove   = new MeleeMove("Blade Swipe", 28.f, 0.35f, 0.08f);
        rangedMove  = new RangedMove("Pierce Arrow",30.f, 0.5f, true);
        specialMove = new SpecialMove("Storm Burst", 45.f, 4.0f, 160.f);
        meleeMove->onHeal = [this](float amt){ healHP(amt); };
    }
};

// ─── HeroC: The Mage (yellow, slow, massive special) ─────────────────────────
class HeroC : public Hero {
public:
    HeroC() : Hero("MAGE", 170.f, 250.f, {200, 400}, Pal::HeroC, &gHeroTextures[2]) {
        meleeMove   = new MeleeMove("Staff Strike", 40.f, 0.55f, 0.05f);
        rangedMove  = new RangedMove("Arcane Bolt", 38.f, 0.7f, false);
        specialMove = new SpecialMove("Arcane Bomb", 70.f, 5.0f, 220.f);
        meleeMove->onHeal = [this](float amt){ healHP(amt); };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  MINION BASE
// ─────────────────────────────────────────────────────────────────────────────
class Minion : public Avatar {
protected:
    float       contactDamage;
    float       contactTimer;
    sf::Color   bodyColor;
    sf::Vector2f formationOffset;
    float        animT;
    float        speed;

public:
    Minion(const std::string& n, float spd, float hp,
           sf::Vector2f p, sf::Color c, float cd, sf::Vector2f fOff,
           const sf::Texture* tex=nullptr)
        : Avatar(n, spd, hp, p, 16.f, tex),
          contactDamage(cd), contactTimer(0.f), bodyColor(c),
          formationOffset(fOff), animT(0.f), speed(spd) {}

    void attack(sf::Vector2f /*dir*/) override {}

    bool tryContactDamage(Avatar* target, float dt){
        contactTimer -= dt;
        if(contactTimer <= 0.f && aabbHit(bounds(), target->bounds())){
            target->takeDamage(contactDamage);
            contactTimer = 1.f;
            return true;
        }
        return false;
    }

    sf::Vector2f getFormationOffset() const { return formationOffset; }

    void render(sf::RenderWindow& win) override {
        const sf::Texture* tex = getTexture();
        if(tex){
            sf::Sprite sprite(*tex);
            sprite.setOrigin(sf::Vector2f(tex->getSize().x/2.f, tex->getSize().y/2.f));
            float scale = (bodyRadius*2.f) / std::max(tex->getSize().x, tex->getSize().y);
            sprite.setScale(sf::Vector2f(scale, scale));
            sprite.setPosition(pos);
            if(hitFlash > 0.f) sprite.setColor(sf::Color::White);
            win.draw(sprite);
        } else {
            // Diamond body
            sf::ConvexShape diamond;
            diamond.setPointCount(4);
            float r = bodyRadius;
            diamond.setPoint(0, sf::Vector2f(pos.x,       pos.y - r*1.2f));
            diamond.setPoint(1, sf::Vector2f(pos.x + r,   pos.y));
            diamond.setPoint(2, sf::Vector2f(pos.x,       pos.y + r*1.2f));
            diamond.setPoint(3, sf::Vector2f(pos.x - r,   pos.y));
            diamond.setFillColor(hitFlash>0.f ? sf::Color::White : bodyColor);
            win.draw(diamond);

            // Eye
            sf::CircleShape eye(4.f);
            eye.setFillColor(sf::Color(10,10,10));
            eye.setOrigin(sf::Vector2f(4.f,4.f));
            eye.setPosition(sf::Vector2f(pos.x, pos.y-2.f));
            win.draw(eye);
            sf::CircleShape pupil(2.f);
            pupil.setFillColor(sf::Color(255,50,50));
            pupil.setOrigin(sf::Vector2f(2.f,2.f));
            pupil.setPosition(sf::Vector2f(pos.x+1.f, pos.y-2.f));
            win.draw(pupil);
        }

        drawHPBar(win, 50.f);
    }
};

// ─── MinionV: V-Formation ─────────────────────────────────────────────────────
class MinionV : public Minion {
    sf::Vector2f target;
    float        waveT;
public:
    MinionV(sf::Vector2f startPos, sf::Vector2f fOffset, const sf::Texture* tex)
        : Minion("Vbot", 120.f, 50.f, startPos, Pal::MinionV, 8.f, fOffset, tex),
          target(640,360), waveT(randf(0,PI*2)) {}

    void move(float dt, sf::Vector2f heroPos) override {
        waveT += dt*2.f;
        // Move toward hero with slight sine weave
        sf::Vector2f desired = heroPos + formationOffset;
        sf::Vector2f dir     = normalize(desired - pos);
        pos += dir * speed * dt;
        // Weave perpendicular
        sf::Vector2f perp(-dir.y, dir.x);
        pos += perp * std::sin(waveT)*30.f*dt;
        animT += dt;
        clampToScreen();
    }
};

// ─── MinionW: Wave sinusoidal path ────────────────────────────────────────────
class MinionW : public Minion {
    float waveT;
    float baseY;
    float dirX;
public:
    MinionW(sf::Vector2f startPos, sf::Vector2f fOffset, const sf::Texture* tex)
        : Minion("Wavebot", 140.f, 40.f, startPos, Pal::MinionW, 10.f, fOffset, tex),
          waveT(randf(0,PI*2)), baseY(startPos.y), dirX(startPos.x>SCREEN_W/2?-1.f:1.f) {}

    void move(float dt, sf::Vector2f heroPos) override {
        waveT += dt*3.f;
        pos.x += dirX * speed * dt;
        pos.y  = heroPos.y + formationOffset.y + std::sin(waveT)*80.f;
        // When too far, wrap or chase
        if(pos.x < 50.f || pos.x > SCREEN_W-50.f) dirX *= -1.f;
        clampToScreen();
        animT += dt;
    }
};

// ─── MinionL: Line sweep ──────────────────────────────────────────────────────
class MinionL : public Minion {
    float sweepT;
    float startX;
public:
    MinionL(sf::Vector2f startPos, sf::Vector2f fOffset, const sf::Texture* tex)
        : Minion("Linebot", 100.f, 60.f, startPos, Pal::MinionL, 12.f, fOffset, tex),
          sweepT(randf(0,PI*2)), startX(startPos.x) {}

    void move(float dt, sf::Vector2f heroPos) override {
        sweepT += dt*1.5f;
        // Parabolic arc toward hero
        sf::Vector2f desired(heroPos.x + formationOffset.x,
                             heroPos.y + formationOffset.y + std::cos(sweepT)*60.f);
        sf::Vector2f dir = normalize(desired - pos);
        pos += dir * speed * dt;
        clampToScreen();
        animT += dt;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  VILLAIN — Dr. Vroomstein
// ─────────────────────────────────────────────────────────────────────────────
enum class VillainState { IDLE, CHARGE, SHOOT, SUMMON, COOLDOWN };

class Villain : public Avatar {
    VillainState state;
    float        stateTimer;
    float        moveTimer;
    sf::Vector2f targetPos;
    float        animT;
    int          summonThreshold; // summon when crossing 75/50/25% HP
    int          summonsSent;
    std::function<void()> onSummon;
    float        shootTimer;
    float        chargeSpeed;
    sf::Vector2f chargeDir;
    bool         charging;
    float        eyeT;

public:
    Villain(const sf::Texture* tex=nullptr)
        : Avatar("Dr. Vroomstein", 150.f, 1200.f, {SCREEN_W/2.f, 200.f}, 38.f, tex),
          state(VillainState::IDLE), stateTimer(2.f), moveTimer(0.f),
          targetPos(SCREEN_W/2.f, 200.f), animT(0.f), summonThreshold(3),
          summonsSent(0), shootTimer(0.f), chargeSpeed(0.f),
          chargeDir(0,0), charging(false), eyeT(0.f) {}

    void setSummonCallback(std::function<void()> cb){ onSummon = cb; }

    void attack(sf::Vector2f dir) override {
        // Shoot at hero
        sf::Vector2f vel = normalize(dir)*320.f;
        spawnProjectile(pos, vel, 18.f, false, false, 12.f, Pal::EnemyProj);
        // Extra spread
        float angle = std::atan2(dir.y,dir.x);
        float spread = 0.35f;
        sf::Vector2f v2(std::cos(angle+spread)*300.f, std::sin(angle+spread)*300.f);
        sf::Vector2f v3(std::cos(angle-spread)*300.f, std::sin(angle-spread)*300.f);
        spawnProjectile(pos, v2, 14.f, false, false, 10.f, Pal::EnemyProj);
        spawnProjectile(pos, v3, 14.f, false, false, 10.f, Pal::EnemyProj);
    }

    void move(float dt, sf::Vector2f heroPos) override {
        animT += dt;
        eyeT  += dt*2.f;
        tickFlash(dt);

        // Check HP thresholds for summons
        float ratio = getHPRatio();
        int shouldHaveSummoned = 0;
        if(ratio < 0.75f) shouldHaveSummoned = 1;
        if(ratio < 0.50f) shouldHaveSummoned = 2;
        if(ratio < 0.25f) shouldHaveSummoned = 3;
        if(summonsSent < shouldHaveSummoned){
            summonsSent++;
            if(onSummon){ onSummon(); pushFlash("MINIONS SUMMONED!", Pal::MinionV); }
        }

        stateTimer -= dt;

        switch(state){
        case VillainState::IDLE: {
            // Float around upper zone
            moveTimer -= dt;
            if(moveTimer <= 0.f){
                targetPos = {randf(150.f, SCREEN_W-150.f), randf(120.f, SCREEN_H/2.f)};
                moveTimer = randf(1.5f, 3.f);
            }
            sf::Vector2f dir = normalize(targetPos - pos);
            pos += dir * baseSpeed * 0.5f * dt;

            if(stateTimer <= 0.f){
                int choice = rand()%3;
                if(choice==0){ state=VillainState::SHOOT; stateTimer=2.0f; shootTimer=0.f; }
                else if(choice==1){ state=VillainState::CHARGE; stateTimer=1.2f;
                    chargeDir = normalize(heroPos - pos); chargeSpeed=0.f; charging=false; }
                else { state=VillainState::COOLDOWN; stateTimer=1.5f; }
            }
            break;
        }
        case VillainState::SHOOT: {
            shootTimer -= dt;
            if(shootTimer <= 0.f){
                attack(normalize(heroPos - pos));
                shootTimer = 0.45f;
            }
            if(stateTimer <= 0.f){ state=VillainState::COOLDOWN; stateTimer=1.2f; }
            break;
        }
        case VillainState::CHARGE: {
            if(!charging){
                // Wind up
                if(stateTimer <= 0.3f){
                    charging=true; chargeSpeed=500.f;
                }
            } else {
                pos += chargeDir * chargeSpeed * dt;
                chargeSpeed -= chargeSpeed * 4.f * dt; // decelerate
            }
            pos.x = clampf(pos.x, bodyRadius, SCREEN_W-bodyRadius);
            pos.y = clampf(pos.y, 120.f+bodyRadius, SCREEN_H-bodyRadius);
            if(stateTimer <= 0.f){ state=VillainState::COOLDOWN; stateTimer=1.5f; charging=false; }
            break;
        }
        case VillainState::SUMMON:
        case VillainState::COOLDOWN: {
            if(stateTimer <= 0.f){ state=VillainState::IDLE; stateTimer=randf(1.5f,3.f); }
            break;
        }
        }
    }

    void render(sf::RenderWindow& win) override {
        const sf::Texture* tex = getTexture();
        float r = bodyRadius;
        if(tex){
            sf::Sprite sprite(*tex);
            sprite.setOrigin(sf::Vector2f(tex->getSize().x/2.f, tex->getSize().y/2.f));
            float scale = (r*2.f) / std::max(tex->getSize().x, tex->getSize().y);
            sprite.setScale(sf::Vector2f(scale, scale));
            sprite.setPosition(pos);
            if(hitFlash>0.f) sprite.setColor(sf::Color::White);
            win.draw(sprite);
        } else {
            // Body — hexagon shape for Dr. Vroomstein
            sf::ConvexShape hex;
            hex.setPointCount(6);
            for(int i=0;i<6;i++){
                float a = PI/3.f*i - PI/6.f;
                hex.setPoint(i,{pos.x+r*std::cos(a), pos.y+r*std::sin(a)});
            }
            hex.setFillColor(hitFlash>0.f ? sf::Color::White : Pal::Villain);
            win.draw(hex);

            // Gear ring animation
            sf::CircleShape gearRing(r+10.f);
            gearRing.setOrigin(sf::Vector2f(r+10.f, r+10.f));
            gearRing.setPosition(pos);
            gearRing.setFillColor(sf::Color::Transparent);
            gearRing.setOutlineColor(sf::Color(180,40,40,180));
            gearRing.setOutlineThickness(4.f);
            win.draw(gearRing);
        }

        // Rotating gear teeth (rectangles around body)
        for(int i=0;i<8;i++){
            float a = animT*1.2f + (PI*2.f/8.f)*i;
            float tx = pos.x + (r+14.f)*std::cos(a);
            float ty = pos.y + (r+14.f)*std::sin(a);
            sf::RectangleShape tooth({12.f,6.f});
            tooth.setOrigin(sf::Vector2f(6.f,3.f));
            tooth.setPosition(sf::Vector2f(tx,ty));
            tooth.setRotation(sf::degrees(a*180.f/PI));
            tooth.setFillColor(sf::Color(160,30,30));
            win.draw(tooth);
        }

        // Eyes — three glowing eyes
        sf::Color eyeCol(255, (unsigned char)(150.f+100.f*std::sin(eyeT)), 0);
        for(int i=-1;i<=1;i++){
            sf::CircleShape eye(6.f);
            eye.setFillColor(eyeCol);
            eye.setOrigin(sf::Vector2f(6.f,6.f));
            eye.setPosition(sf::Vector2f(pos.x + i*14.f, pos.y - 8.f));
            win.draw(eye);
        }

        // Label
        // (drawn in HUD section)
        drawHPBar(win, 160.f, -bodyRadius-18.f);

        // Charge telegraph
        if(state == VillainState::CHARGE && !charging){
            sf::CircleShape pulse(bodyRadius*1.6f);
            pulse.setOrigin(sf::Vector2f(bodyRadius*1.6f, bodyRadius*1.6f));
            pulse.setPosition(pos);
            pulse.setFillColor(sf::Color::Transparent);
            pulse.setOutlineColor(sf::Color(255,50,50,200));
            pulse.setOutlineThickness(4.f);
            win.draw(pulse);
        }
    }

    VillainState getState() const { return state; }
    bool isCharging()       const { return charging; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  GAME STATE ENUM
// ─────────────────────────────────────────────────────────────────────────────
enum class GameState { MENU, HERO_SELECT, PLAYING, PAUSED, WIN, GAME_OVER };

// ─────────────────────────────────────────────────────────────────────────────
//  HUD
// ─────────────────────────────────────────────────────────────────────────────
class HUD {
    sf::Font& font;
public:
    HUD(sf::Font& f) : font(f) {}

    void drawText(sf::RenderWindow& win, const std::string& t,
                  float x, float y, unsigned sz, sf::Color col,
                  bool center=false) const {
        sf::Text tx(font, t, sz);
        tx.setFillColor(col);
        if(center){
            sf::FloatRect r = tx.getLocalBounds();
            tx.setOrigin(sf::Vector2f(r.position.x+r.size.x/2.f, r.position.y+r.size.y/2.f));
        }
        tx.setPosition(sf::Vector2f(x,y));
        win.draw(tx);
    }

    void drawBar(sf::RenderWindow& win, float x, float y,
                 float w, float h, float ratio, sf::Color fill, sf::Color bg) const {
        sf::RectangleShape bgr({w,h});
        bgr.setPosition(sf::Vector2f(x,y));
        bgr.setFillColor(bg);
        win.draw(bgr);
        sf::RectangleShape fr({w*ratio,h});
        fr.setPosition(sf::Vector2f(x,y));
        fr.setFillColor(fill);
        win.draw(fr);
    }

    void drawCooldownIndicator(sf::RenderWindow& win, Move* m, float x, float y,
                               const std::string& key, sf::Color col) const {
        if(!m) return;
        float ratio = m->getCooldownRatio();

        // Circle backdrop
        sf::CircleShape circle(20.f);
        circle.setOrigin(sf::Vector2f(20.f,20.f));
        circle.setPosition(sf::Vector2f(x,y));
        circle.setFillColor(sf::Color(30,30,50));
        circle.setOutlineColor(ratio>=1.f ? col : sf::Color(80,80,100));
        circle.setOutlineThickness(2.f);
        win.draw(circle);

        // Fill arc (approximated with a slightly smaller filled circle + alpha)
        sf::CircleShape fill(18.f*ratio);
        fill.setOrigin(sf::Vector2f(18.f*ratio, 18.f*ratio));
        fill.setPosition(sf::Vector2f(x,y));
        fill.setFillColor(sf::Color(col.r,col.g,col.b,80+(unsigned char)(80*ratio)));
        win.draw(fill);

        drawText(win, key,     x, y-12.f, 14, Pal::White, true);
        drawText(win, m->getDisplayName().substr(0,3), x, y+2.f, 11, col, true);
    }

    void draw(sf::RenderWindow& win, Hero* hero, int score,
              int stage, int minionsLeft, GameState gs) const {
        (void)score; (void)gs;
        if(!hero) return;

        // Top bar background
        sf::RectangleShape topBar({(float)SCREEN_W, 90.f});
        topBar.setFillColor(sf::Color(10,8,25,220));
        win.draw(topBar);

        // Hero HP bar
        drawText(win,"HP",10,8,14,Pal::DimWhite);
        drawBar(win,40,8,300,18,hero->getHPRatio(),
                hpColor(hero->getHPRatio()),sf::Color(40,40,60));
        std::ostringstream hpss;
        hpss<<(int)hero->getHP()<<"/"<<(int)hero->getMaxHP();
        drawText(win,hpss.str(),345,8,14,Pal::White);

        // Shield icon
        if(hero->isShielded()){
            drawText(win,"[SHIELD]",460,8,14,Pal::Shield);
        }

        // Score
        drawText(win,gScore.str(),(float)SCREEN_W-220.f,8,16,Pal::Gold);

        // Stage info
        std::ostringstream wss;
        wss<<"STAGE "<<stage<<"  |  MINIONS: "<<minionsLeft;
        drawText(win,wss.str(),(float)SCREEN_W/2.f,8,14,Pal::DimWhite,true);

        // Move cooldown indicators
        float cdX = 60.f;
        float cdY = 52.f;
        drawCooldownIndicator(win, hero->getMeleeMove(),  cdX,       cdY, "S", sf::Color(255,200,60));
        drawCooldownIndicator(win, hero->getRangedMove(), cdX+55.f,  cdY, "E", Pal::Projectile);
        drawCooldownIndicator(win, hero->getSpecialMove(),cdX+110.f, cdY, "F", Pal::AoEFlash);

        // Hero name
        drawText(win,hero->getName(),cdX+165.f,38.f,16,Pal::White);

        // Streak
        if(gScore.streak > 2){
            std::ostringstream ss;
            ss<<"STREAK x"<<gScore.streak<<"  x"<<(int)(gScore.streakMult*10)/10.0f;
            drawText(win,ss.str(),(float)SCREEN_W/2.f,35.f,13,Pal::AoEFlash,true);
        }

        // Stage label / boss text
        if(stage == 3){
            drawText(win,"Dr. Vroomstein",(float)SCREEN_W/2.f,(float)SCREEN_H-22.f,13,Pal::Villain,true);
        } else {
            drawText(win,"MINION SWARM",(float)SCREEN_W/2.f,(float)SCREEN_H-22.f,13,Pal::DimWhite,true);
        }
    }

    void drawFlashMessages(sf::RenderWindow& win) const {
        float y = SCREEN_H/2.f - 60.f;
        for(auto& msg : gFlash){
            float alpha = clampf(msg.ttl/msg.maxTTL, 0.f, 1.f);
            sf::Color c = msg.color;
            c.a = (unsigned char)(255*alpha);
            float scale = 1.f + 0.3f*(1.f - msg.ttl/msg.maxTTL);
            sf::Text tx(font, msg.text, (unsigned)(18*scale));
            tx.setFillColor(c);
            sf::FloatRect r = tx.getLocalBounds();
            tx.setOrigin(sf::Vector2f(r.position.x+r.size.x/2.f, r.position.y+r.size.y/2.f));
            tx.setPosition(sf::Vector2f(msg.pos.x, y));
            win.draw(tx);
            y += 28.f;
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  BACKGROUND RENDERER
// ─────────────────────────────────────────────────────────────────────────────
void drawBackground(sf::RenderWindow& win, float timeT){
    // Dark gradient fill
    sf::RectangleShape bg({(float)SCREEN_W,(float)SCREEN_H});
    bg.setFillColor(Pal::BG);
    win.draw(bg);

    // Scrolling grid lines
    sf::Color gridCol(40,30,70,120);
    float scroll = std::fmod(timeT*30.f, 60.f);
    for(float x=-60.f+scroll;x<SCREEN_W+60.f;x+=60.f){
        sf::Vertex line[] = {
            sf::Vertex{{x,90.f}, gridCol}, sf::Vertex{{x,(float)SCREEN_H}, gridCol}
        };
        win.draw(line,2,sf::PrimitiveType::Lines);
    }
    for(float y=90.f;y<SCREEN_H;y+=60.f){
        sf::Vertex line[] = {
            sf::Vertex{{0,y}, gridCol}, sf::Vertex{{(float)SCREEN_W,y}, gridCol}
        };
        win.draw(line,2,sf::PrimitiveType::Lines);
    }

    // Floating particles
    srand(42); // deterministic star positions
    for(int i=0;i<30;i++){
        float px = (float)(rand()%SCREEN_W);
        float py = 120.f + (float)(rand()%(SCREEN_H-120));
        float r  = 1.f + (rand()%3);
        float brightness = 60 + rand()%100;
        sf::CircleShape star(r);
        star.setFillColor(sf::Color((unsigned char)brightness,(unsigned char)brightness,(unsigned char)(brightness+40),120));
        star.setPosition(sf::Vector2f(px,py));
        win.draw(star);
    }
    srand((unsigned)time(nullptr)); // restore random seed
}

// ─────────────────────────────────────────────────────────────────────────────
//  POWER-UP RENDERER
// ─────────────────────────────────────────────────────────────────────────────
void drawPowerUps(sf::RenderWindow& win, std::vector<PowerUp>& pups, float dt){
    for(auto& p : pups){
        if(!p.active) continue;
        p.pulseT += dt*3.f;
        float pulse = std::sin(p.pulseT)*4.f;
        sf::Color col;
        std::string label;
        switch(p.kind){
        case PowerKind::Shield:      col=Pal::Shield;      label="SH"; break;
        case PowerKind::AttackBoost: col=Pal::Gold;        label="ATK"; break;
        case PowerKind::SpeedBoost:  col=Pal::HeroB;       label="SPD"; break;
        }
        sf::CircleShape c(14.f+pulse);
        c.setOrigin(sf::Vector2f(14.f+pulse,14.f+pulse));
        c.setPosition(p.pos);
        c.setFillColor(sf::Color(col.r,col.g,col.b,180));
        c.setOutlineColor(col);
        c.setOutlineThickness(2.f);
        win.draw(c);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  MAIN GAME LOOP MANAGER
// ─────────────────────────────────────────────────────────────────────────────
class GameLoop {
    sf::RenderWindow& win;
    sf::Font&         font;
    HUD               hud;

    GameState   state;
    int         selectedHero;
    int         stage;
    float       waveTimer;
    float       powerUpTimer;
    float       gameTime;
    sf::Music   bgMusic;
    std::string currentMusicPath;

    Hero*              hero;
    Villain*           villain;
    std::vector<Minion*> minions;

    sf::Vector2f  aimDir;   // last known aim direction (toward mouse or auto)

    void clearMinions(){
        for(auto* m : minions) delete m;
        minions.clear();
    }

    void playMusic(const std::string& path){
        if(currentMusicPath == path && bgMusic.getStatus() == sf::SoundSource::Status::Playing) return;
        if(bgMusic.openFromFile(path)){
            bgMusic.setLooping(true);
            bgMusic.play();
            currentMusicPath = path;
        }
    }

    void spawnWave(int stageNum){
        clearMinions();
        int count = 4 + stageNum*2;
        if(stageNum == 1) count = 5;
        else if(stageNum == 2) count = 8;
        else if(stageNum == 3) count = 5;

        for(int i=0;i<count;i++){
            // Spawn from random screen edge
            sf::Vector2f spawnPos;
            int edge = rand()%4;
            if(edge==0) spawnPos = {randf(0,SCREEN_W), 90.f};
            else if(edge==1) spawnPos = {(float)SCREEN_W-20.f, randf(120,SCREEN_H)};
            else if(edge==2) spawnPos = {randf(0,SCREEN_W), (float)SCREEN_H-20.f};
            else  spawnPos = {20.f, randf(120,SCREEN_H)};

            sf::Vector2f fOff = {randf(-80,80), randf(-60,60)};
            int type = i % 3;
            if(type==0)      minions.push_back(new MinionV(spawnPos, fOff, &gMinionTexture));
            else if(type==1) minions.push_back(new MinionW(spawnPos, fOff, &gMinionTexture));
            else             minions.push_back(new MinionL(spawnPos, fOff, &gMinionTexture));
        }
        std::ostringstream ss;
        ss<<"STAGE "<<stageNum<<" — "<<count<<" MINIONS!";
        pushFlash(ss.str(), Pal::Gold);
    }

    void setupStage(int stageNum){
        if(stageNum < 3){
            if(villain){ delete villain; villain = nullptr; }
        } else {
            setupVillain();
        }
        stage = stageNum;
        waveTimer = 5.f;
        spawnWave(stageNum);
        std::ostringstream ss;
        ss<<"STAGE "<<stageNum;
        pushFlash(ss.str(), Pal::Gold);
        if(stageNum == 3) pushFlash("DR. VROOMSTEIN ENTERS!", Pal::Villain);
        if(stageNum == 1) playMusic("Juhani Junkala [Retro Game Music Pack] Level 1.wav");
        else if(stageNum == 2) playMusic("Juhani Junkala [Retro Game Music Pack] Level 2.wav");
        else if(stageNum == 3) playMusic("Juhani Junkala [Retro Game Music Pack] Level 3.wav");
    }

    void setupVillain(){
        if(villain) delete villain;
        villain = new Villain(&gVroomsteinTexture);
        villain->setSummonCallback([this]{ spawnWave(stage); });
    }

    void createHero(int sel){
        if(hero) delete hero;
        switch(sel){
        case 0: hero = new HeroA(); break;
        case 1: hero = new HeroB(); break;
        default: hero = new HeroC(); break;
        }
        gScore.total=0; gScore.streak=0; gScore.streakMult=1.f;
        gProjectiles.clear();
        gFlash.clear();
        gPowerUps.clear();
        stage = 1;
        waveTimer = 3.f;
        powerUpTimer = 20.f;
        gameTime = 0.f;
    }

    // ── Menu screens ─────────────────────────────────────────────────────────
    void drawMenu(float t){
        drawBackground(win,t);

        // Title
        sf::Text title(font, "VROOMSTEIN RPG", 64);
        title.setFillColor(Pal::Villain);
        sf::FloatRect tr = title.getLocalBounds();
        title.setOrigin(sf::Vector2f(tr.position.x + tr.size.x/2.f, tr.position.y + tr.size.y/2.f));
        title.setPosition(sf::Vector2f(SCREEN_W/2.f, 200.f + std::sin(t*2.f)*8.f));
        win.draw(title);

        sf::Text sub(font, "DEFEAT DR. VROOMSTEIN AND HIS POLYMORPHIC MINIONS", 18);
        sub.setFillColor(Pal::DimWhite);
        sf::FloatRect sr = sub.getLocalBounds();
        sub.setOrigin(sf::Vector2f(sr.position.x + sr.size.x/2.f, sr.position.y + sr.size.y/2.f));
        sub.setPosition(sf::Vector2f(SCREEN_W/2.f, 295.f));
        win.draw(sub);

        // Controls hint
        hud.drawText(win,"PRESS  ENTER  TO  START",
                     SCREEN_W/2.f, 420.f, 22, Pal::Teal, true);
        hud.drawText(win,"CONTROLS:  WASD/ARROWS = MOVE   S = MELEE   E = RANGED   F = SPECIAL",
                     SCREEN_W/2.f, 500.f, 15, Pal::DimWhite, true);
        hud.drawText(win,"OOP PROJECT  —  BSCS 2E",
                     SCREEN_W/2.f, (float)SCREEN_H-30.f, 13, sf::Color(80,80,100), true);
    }

    void drawHeroSelect(float t){
        drawBackground(win,t);
        hud.drawText(win,"CHOOSE YOUR HERO",
                     SCREEN_W/2.f, 60.f, 36, Pal::Gold, true);
        hud.drawText(win,"Press 1, 2, or 3  —  then ENTER",
                     SCREEN_W/2.f, 110.f, 18, Pal::DimWhite, true);

        struct HeroInfo{ std::string name, desc1, desc2, desc3; sf::Color col; int key; };
        HeroInfo heroes[3] = {
            {"PALADIN","HP: 300","Speed: 200","Lifesteal Melee",Pal::HeroA,1},
            {"RANGER", "HP: 220","Speed: 270","Piercing Arrows",Pal::HeroB,2},
            {"MAGE",   "HP: 250","Speed: 170","Massive AoE Burst",Pal::HeroC,3},
        };

        for(int i=0;i<3;i++){
            float cx = 220.f + i*300.f;
            float cy = 340.f;
            bool  sel = selectedHero==i;

            sf::RectangleShape card({220.f, 280.f});
            card.setOrigin(sf::Vector2f(110.f,140.f));
            card.setPosition(sf::Vector2f(cx,cy));
            card.setFillColor(sf::Color(20,16,40,sel?220:140));
            card.setOutlineColor(sel ? heroes[i].col : sf::Color(60,60,80));
            card.setOutlineThickness(sel ? 3.f : 1.5f);
            win.draw(card);

            // Mini hero preview
            sf::CircleShape preview(sel ? 38.f : 32.f);
            preview.setOrigin(sf::Vector2f(preview.getRadius(),preview.getRadius()));
            preview.setPosition(sf::Vector2f(cx, cy-60.f));
            preview.setFillColor(heroes[i].col);
            win.draw(preview);

            hud.drawText(win, std::to_string(heroes[i].key)+". "+heroes[i].name,
                         cx, cy+10.f, 18, sel?heroes[i].col:Pal::DimWhite, true);
            hud.drawText(win, heroes[i].desc1, cx, cy+48.f, 14, Pal::DimWhite, true);
            hud.drawText(win, heroes[i].desc2, cx, cy+72.f, 14, Pal::DimWhite, true);
            hud.drawText(win, heroes[i].desc3, cx, cy+96.f, 14, heroes[i].col, true);

            if(sel)
                hud.drawText(win,"◄ SELECTED ►",cx,cy+130.f,14,heroes[i].col,true);
        }

        hud.drawText(win,"S = Shield Bash/Blade Swipe/Staff Strike   E = Holy Bolt/Pierce Arrow/Arcane Bolt   F = Special",
                     SCREEN_W/2.f, SCREEN_H-40.f, 13, sf::Color(100,100,120), true);
    }

    void drawOverlay(const std::string& title, const std::string& sub,
                     sf::Color titleCol, float t){
        (void)t;
        sf::RectangleShape dim({(float)SCREEN_W,(float)SCREEN_H});
        dim.setFillColor(sf::Color(0,0,0,180));
        win.draw(dim);

        hud.drawText(win, title, SCREEN_W/2.f, 240.f, 72, titleCol, true);
        hud.drawText(win, sub,   SCREEN_W/2.f, 340.f, 22, Pal::DimWhite, true);
        std::ostringstream ss;
        ss<<"FINAL SCORE: "<<gScore.total;
        hud.drawText(win, ss.str(), SCREEN_W/2.f, 400.f, 28, Pal::Gold, true);
        hud.drawText(win,"PRESS ENTER TO CONTINUE",
                     SCREEN_W/2.f, 490.f, 18, Pal::Teal, true);
    }

    void drawPaused(){
        sf::RectangleShape dim({(float)SCREEN_W,(float)SCREEN_H});
        dim.setFillColor(sf::Color(0,0,0,140));
        win.draw(dim);
        hud.drawText(win,"PAUSED",SCREEN_W/2.f,300.f,64,Pal::White,true);
        hud.drawText(win,"ESC to resume",SCREEN_W/2.f,390.f,22,Pal::DimWhite,true);
    }

    // ── Gameplay update ───────────────────────────────────────────────────────
    void update(float dt, sf::Vector2f mousePos){
        if(!hero) return;
        gameTime += dt;

        hero->tickPowers(dt);
        hero->tickFlash(dt);

        // Flash message timer
        for(auto& f : gFlash) f.ttl -= dt;
        while(!gFlash.empty() && gFlash.front().ttl <= 0.f)
            gFlash.pop_front();

        // Input
        sf::Vector2f moveInput(0,0);
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)  || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) moveInput.x -= 1.f;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) moveInput.x += 1.f;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)    || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) moveInput.y -= 1.f;
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)  || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) moveInput.y += 1.f;

        hero->move(dt, moveInput);

        // Aim: toward mouse if mouse moved recently, else toward nearest enemy
        sf::Vector2f heroPos = hero->getPos();
        sf::Vector2f toMouse = mousePos - heroPos;
        if(length(toMouse) > 10.f){
            aimDir = normalize(toMouse);
        } else if(villain && !villain->isDead()){
            aimDir = normalize(villain->getPos() - heroPos);
        } else {
            float bestDist = 1e9f;
            sf::Vector2f nearest(1.f,0.f);
            for(auto* m : minions){
                if(m->isDead()) continue;
                float d = length(m->getPos() - heroPos);
                if(d < bestDist){ bestDist = d; nearest = m->getPos(); }
            }
            aimDir = bestDist < 1e8f ? normalize(nearest - heroPos) : sf::Vector2f(1.f,0.f);
        }

        // Villain update
        if(villain) villain->move(dt, hero->getPos());

        // Minion update
        for(auto* m : minions){
            if(m->isDead()) continue;
            m->move(dt, hero->getPos());
            m->tickFlash(dt);
            // Contact damage on hero
            m->tryContactDamage(hero, dt);
        }

        // Villain contact damage (charge)
        if(villain && villain->isCharging()){
            if(aabbHit(villain->bounds(), hero->bounds())){
                hero->takeDamage(25.f);
            }
        }

        // Projectile update
        float atkBoost = hero->getAttackBoost();
        for(auto& p : gProjectiles){
            if(!p.active) continue;
            p.pos += p.vel * dt;
            // Out of screen
            if(p.pos.x<0||p.pos.x>SCREEN_W||p.pos.y<0||p.pos.y>SCREEN_H){
                p.active=false; continue;
            }
            if(p.friendly){
                // Hero projectile — check villain
                if(villain && !villain->isDead() && aabbHit(p.bounds(), villain->bounds())){
                    float dmg = p.damage * atkBoost;
                    villain->takeDamage(dmg);
                    gScore.addHit(dmg, 1.2f);
                    pushFlash(std::to_string((int)dmg)+" DMG", Pal::Gold, villain->getPos());
                    if(!p.piercing) p.active=false;
                    if(villain->isDead()){ state=GameState::WIN; return; }
                }
                // Hero projectile — check minions
                for(auto* m : minions){
                    if(m->isDead()) continue;
                    if(aabbHit(p.bounds(), m->bounds())){
                        float dmg = p.damage * atkBoost;
                        m->takeDamage(dmg);
                        gScore.addHit(dmg, 1.0f);
                        if(m->isDead()) pushFlash("MINION DOWN!", Pal::MinionV, m->getPos());
                        if(!p.piercing) { p.active=false; break; }
                    }
                }
            } else {
                // Enemy projectile — check hero
                if(aabbHit(p.bounds(), hero->bounds())){
                    hero->takeDamage(p.damage);
                    p.active=false;
                    if(hero->isDead()){ state=GameState::GAME_OVER; return; }
                }
            }
        }
        // Remove dead projectiles
        gProjectiles.erase(
            std::remove_if(gProjectiles.begin(),gProjectiles.end(),[](const Projectile&p){return !p.active;}),
            gProjectiles.end()
        );

        // Power-up collection
        for(auto& pu : gPowerUps){
            if(!pu.active) continue;
            sf::FloatRect pBounds(sf::Vector2f(pu.pos.x-16.f, pu.pos.y-16.f), sf::Vector2f(32.f, 32.f));
            if(aabbHit(pBounds, hero->bounds())){
                hero->applyPowerUp(pu.kind);
                pu.active=false;
            }
        }

        // Power-up spawn timer
        powerUpTimer -= dt;
        if(powerUpTimer <= 0.f){ spawnPowerUp(); powerUpTimer=randf(15.f,25.f); }

        // Stage progression
        waveTimer -= dt;
        bool allMinionsDead = true;
        for(auto* m : minions) if(!m->isDead()){ allMinionsDead=false; break; }
        if(allMinionsDead && waveTimer<=0.f){
            if(stage < 3){
                setupStage(stage+1);
            } else if(villain && !villain->isDead()){
                waveTimer = 7.f;
                spawnWave(3);
            }
        }

        // Hero death check
        if(hero->isDead()){ state=GameState::GAME_OVER; }
    }

    void drawGame(float t){
        drawBackground(win, t);

        // Draw power-ups
        drawPowerUps(win, gPowerUps, FIXED_DT);

        // Draw projectiles
        for(auto& p : gProjectiles){
            if(!p.active) continue;
            sf::CircleShape c(p.radius);
            c.setOrigin(sf::Vector2f(p.radius,p.radius));
            c.setPosition(p.pos);
            c.setFillColor(p.color);
            // Glow ring
            sf::CircleShape glow(p.radius+4.f);
            glow.setOrigin(sf::Vector2f(p.radius+4.f,p.radius+4.f));
            glow.setPosition(p.pos);
            glow.setFillColor(sf::Color(p.color.r,p.color.g,p.color.b,60));
            win.draw(glow);
            win.draw(c);
        }

        // Draw minions
        for(auto* m : minions){
            if(!m->isDead()) m->render(win);
        }

        // Draw villain
        if(villain && !villain->isDead()) villain->render(win);

        // Draw hero
        hero->render(win);

        // HUD
        int alive=0;
        for(auto* m : minions) if(!m->isDead()) alive++;
        hud.draw(win, hero, gScore.total, stage, alive, state);

        // Flash messages
        hud.drawFlashMessages(win);

        // Aim line (subtle)
        sf::Vector2f heroPos = hero->getPos();
        sf::Vertex aimLine[2] = {
            {{heroPos.x, heroPos.y}, sf::Color(255,255,255,30)},
            {{heroPos.x + aimDir.x*60.f, heroPos.y + aimDir.y*60.f}, sf::Color(255,255,255,0)}
        };
        win.draw(aimLine,2,sf::PrimitiveType::Lines);
    }

public:
    GameLoop(sf::RenderWindow& w, sf::Font& f)
        : win(w), font(f), hud(f),
          state(GameState::MENU), selectedHero(0),
          stage(1), waveTimer(3.f), powerUpTimer(20.f), gameTime(0.f),
          hero(nullptr), villain(nullptr), aimDir(1,0) {
        playMusic("Juhani Junkala [Retro Game Music Pack] Title Screen.wav");
    }

    ~GameLoop(){
        delete hero; delete villain; clearMinions();
    }

    void handleEvent(const sf::Event& ev){
        if(!ev.is<sf::Event::KeyPressed>()) return;
        const auto* key = ev.getIf<sf::Event::KeyPressed>();
        if(!key) return;

        switch(state){
        case GameState::MENU:
            if(key->code == sf::Keyboard::Key::Enter) state=GameState::HERO_SELECT;
            break;
        case GameState::HERO_SELECT:
            if(key->code == sf::Keyboard::Key::Num1) selectedHero=0;
            if(key->code == sf::Keyboard::Key::Num2) selectedHero=1;
            if(key->code == sf::Keyboard::Key::Num3) selectedHero=2;
            if(key->code == sf::Keyboard::Key::Enter){
                createHero(selectedHero);
                setupStage(stage);
                state=GameState::PLAYING;
                pushFlash("FIGHT!", Pal::Villain);
            }
            if(key->code == sf::Keyboard::Key::Escape) state=GameState::MENU;
            break;
        case GameState::PLAYING:
            if(key->code == sf::Keyboard::Key::Escape) state=GameState::PAUSED;
            if(key->code == sf::Keyboard::Key::S && hero) hero->doMelee(aimDir);
            if(key->code == sf::Keyboard::Key::E && hero) hero->doRanged(aimDir);
            if(key->code == sf::Keyboard::Key::F && hero) hero->doSpecial();
            break;
        case GameState::PAUSED:
            if(key->code == sf::Keyboard::Key::Escape) state=GameState::PLAYING;
            if(key->code == sf::Keyboard::Key::Enter)  state=GameState::PLAYING;
            break;
        case GameState::WIN:
        case GameState::GAME_OVER:
            if(key->code == sf::Keyboard::Key::Enter){
                state=GameState::MENU;
                delete hero; hero=nullptr;
                delete villain; villain=nullptr;
                clearMinions();
                gProjectiles.clear();
                gFlash.clear();
                gPowerUps.clear();
                playMusic("Juhani Junkala [Retro Game Music Pack] Title Screen.wav");
            }
            break;
        }
    }

    void run(){
        sf::Clock clock;
        float accumulator = 0.f;
        float menuTime    = 0.f;

        while(win.isOpen()){
            while(auto ev = win.pollEvent()){
                if(ev->is<sf::Event::Closed>()) win.close();
                handleEvent(*ev);
            }

            float dt = clock.restart().asSeconds();
            dt = clampf(dt, 0.f, 0.05f);
            menuTime += dt;

            win.clear(Pal::BG);

            switch(state){
            case GameState::MENU:
                drawMenu(menuTime);
                break;
            case GameState::HERO_SELECT:
                drawHeroSelect(menuTime);
                break;
            case GameState::PLAYING: {
                accumulator += dt;
                sf::Vector2i mp = sf::Mouse::getPosition(win);
                sf::Vector2f mousePos((float)mp.x,(float)mp.y);
                while(accumulator >= FIXED_DT){
                    update(FIXED_DT, mousePos);
                    accumulator -= FIXED_DT;
                }
                drawGame(gameTime);
                break;
            }
            case GameState::PAUSED:
                drawGame(gameTime);
                drawPaused();
                break;
            case GameState::WIN:
                drawGame(gameTime);
                drawOverlay("YOU WIN!", "Dr. Vroomstein has been defeated!", Pal::Gold, gameTime);
                break;
            case GameState::GAME_OVER:
                drawGame(gameTime);
                drawOverlay("GAME OVER", "The world falls to Dr. Vroomstein...", Pal::Villain, gameTime);
                break;
            }

            win.display();
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ENTRY POINT
// ─────────────────────────────────────────────────────────────────────────────
int main(){
    srand((unsigned)time(nullptr));

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u{SCREEN_W, SCREEN_H}),
        "VroomsteinRPG — OOP Semester Project — BSCS 2E",
        sf::Style::Close | sf::Style::Titlebar
    );
    window.setFramerateLimit(60);

    // Load font — try common system fonts, fallback to default
    sf::Font font;
    bool fontLoaded = false;
    const char* fontPaths[] = {
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/cour.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/System/Library/Fonts/Menlo.ttc",
    };
    for(auto* path : fontPaths){
        if(font.openFromFile(path)){ fontLoaded=true; break; }
    }
    if(!fontLoaded){
        std::cerr<<"[WARN] Could not load any system font. Text may not render.\n";
        std::cerr<<"       Place a .ttf file named 'font.ttf' next to the exe and recompile,\n";
        std::cerr<<"       or install DejaVu fonts (Linux: sudo apt install fonts-dejavu)\n";
        // Try local fallback
        (void)font.openFromFile("font.ttf");
    }

    if(!gHeroTextures[0].loadFromFile("hero1.png")) std::cerr<<"[WARN] hero1.png not found\n";
    if(!gHeroTextures[1].loadFromFile("hero2.png")) std::cerr<<"[WARN] hero2.png not found\n";
    if(!gHeroTextures[2].loadFromFile("hero3.png")) std::cerr<<"[WARN] hero3.png not found\n";
    if(!gMinionTexture.loadFromFile("minions.png")) std::cerr<<"[WARN] minions.png not found\n";
    if(!gVroomsteinTexture.loadFromFile("vroomstein.png")) std::cerr<<"[WARN] vroomstein.png not found\n";

    GameLoop game(window, font);
    game.run();

    return 0;
}

/*
=============================================================================
  COMPILATION QUICK REFERENCE
=============================================================================

  WINDOWS (in VS Code terminal, MinGW-w64):
  ------------------------------------------
  Compile:
    g++ -o VroomsteinRPG VroomsteinRPG.cpp ^
        -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system ^
        -std=c++17 -O2

  If SFML is in a custom path (e.g. C:\SFML-2.6.2\):
    g++ -o VroomsteinRPG VroomsteinRPG.cpp ^
        -I"C:\SFML-2.6.2\include" ^
        -L"C:\SFML-2.6.2\lib" ^
        -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system ^
        -std=c++17 -O2

  Then copy SFML DLLs next to the .exe:
    C:\SFML-2.6.2\bin\*.dll  ->  same folder as VroomsteinRPG.exe

  Run:
    .\VroomsteinRPG.exe

  LINUX:
  -------
    sudo apt install libsfml-dev    (if not installed)
    g++ -o VroomsteinRPG VroomsteinRPG.cpp \
        -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system \
        -std=c++17 -O2
    ./VroomsteinRPG

=============================================================================
  OOP REQUIREMENTS MET:
    Abstract base Avatar  -> pure virtual move(), attack(), render()
    Abstract base Move    -> pure virtual trigger(), getName(), getMult()
    Hero    (3 concrete)  -> HeroA (Paladin), HeroB (Ranger), HeroC (Mage)
    Villain               -> Dr. Vroomstein with FSM AI
    Minion  (3 concrete)  -> MinionV (V-form), MinionW (Wave), MinionL (Line)
    MeleeMove             -> lifesteal attribute
    RangedMove            -> piercing attribute
    SpecialMove           -> AoE 8-directional burst
    Polymorphism          -> std::vector<Minion*>, Avatar*, virtual dispatch
    Encapsulation         -> private/protected fields, public API
    Score system          -> damage * multiplier * streak
    Power-ups             -> Shield, AttackBoost, SpeedBoost
    AABB collision        -> aabbHit() between all entity pairs
    Fixed timestep loop   -> FIXED_DT = 1/60
    HUD                   -> HP bars, score, cooldown indicators, flash msgs
=============================================================================
*/
