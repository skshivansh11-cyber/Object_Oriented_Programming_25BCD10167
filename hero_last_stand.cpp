#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// HERO: LAST STAND — DEFINITIVE EDITION
// A complete code-drawn dark-fantasy boss battle using SFML 3.
// No sprite sheets or external art assets are required.
//
// Build from MSYS2 UCRT64:
// g++ main.cpp -o LastStand.exe $(pkg-config --cflags --libs sfml-graphics)

namespace {
constexpr unsigned WINDOW_W = 1280;
constexpr unsigned WINDOW_H = 720;
constexpr float GROUND_Y = 565.f;
constexpr float PI = 3.14159265358979323846f;

float randf(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (hi - lo);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * std::clamp(t, 0.f, 1.f);
}

sf::Color color(int r, int g, int b, int a = 255) {
    return sf::Color(
        static_cast<std::uint8_t>(std::clamp(r, 0, 255)),
        static_cast<std::uint8_t>(std::clamp(g, 0, 255)),
        static_cast<std::uint8_t>(std::clamp(b, 0, 255)),
        static_cast<std::uint8_t>(std::clamp(a, 0, 255))
    );
}

void rectangle(sf::RenderWindow& win, sf::Vector2f pos, sf::Vector2f size, sf::Color c) {
    sf::RectangleShape s(size);
    s.setPosition(pos);
    s.setFillColor(c);
    win.draw(s);
}

void circle(sf::RenderWindow& win, sf::Vector2f pos, float radius, sf::Color c) {
    sf::CircleShape s(radius);
    s.setPosition(pos);
    s.setFillColor(c);
    win.draw(s);
}

void circleCentered(sf::RenderWindow& win, sf::Vector2f center, float radius, sf::Color c) {
    sf::CircleShape s(radius);
    s.setOrigin({radius, radius});
    s.setPosition(center);
    s.setFillColor(c);
    win.draw(s);
}

void ringCentered(sf::RenderWindow& win, sf::Vector2f center, float radius, float thickness, sf::Color c) {
    sf::CircleShape s(radius);
    s.setOrigin({radius, radius});
    s.setPosition(center);
    s.setFillColor(sf::Color::Transparent);
    s.setOutlineThickness(thickness);
    s.setOutlineColor(c);
    win.draw(s);
}

void polygon(sf::RenderWindow& win, const std::vector<sf::Vector2f>& points, sf::Color c) {
    if (points.size() < 3) return;
    sf::ConvexShape shape(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) shape.setPoint(i, points[i]);
    shape.setFillColor(c);
    win.draw(shape);
}

void line(sf::RenderWindow& win, sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color c) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    sf::RectangleShape s({len, thickness});
    s.setPosition(a);
    s.setOrigin({0.f, thickness / 2.f});
    s.setRotation(sf::degrees(std::atan2(d.y, d.x) * 180.f / PI));
    s.setFillColor(c);
    win.draw(s);
}
} // namespace

// ============================================================================
// VFX: PARTICLES, SHOCKWAVES, FLOATING TEXT & HAZARDS
// ============================================================================

class Particle {
public:
    sf::Vector2f p, v;
    float life, maxLife, radius, gravity;
    sf::Color tint;

    Particle(sf::Vector2f position, sf::Vector2f velocity, float lifetime,
             float r, sf::Color c, float grav = 38.f)
        : p(position), v(velocity), life(lifetime), maxLife(lifetime),
          radius(r), gravity(grav), tint(c) {}

    void update(float dt) {
        life -= dt;
        p += v * dt;
        v.y += gravity * dt;
        v *= std::max(0.f, 1.f - dt * 0.45f);
        if (p.y > GROUND_Y && v.y > 0.f && gravity > 100.f) {
            p.y = GROUND_Y;
            v.y *= -0.35f;
            v.x *= 0.7f;
        }
    }

    void draw(sf::RenderWindow& win) const {
        float alpha = std::clamp(life / maxLife, 0.f, 1.f);
        sf::Color c = tint;
        c.a = static_cast<std::uint8_t>(static_cast<float>(tint.a) * alpha);
        circleCentered(win, p, radius * (0.3f + 0.7f * alpha), c);
    }

    bool dead() const { return life <= 0.f; }
};

struct Shockwave {
    sf::Vector2f center;
    float radius = 10.f;
    float maxRadius = 220.f;
    float life = 0.35f;
    float maxLife = 0.35f;
    float thickness = 5.f;
    sf::Color tint;

    void update(float dt) {
        life -= dt;
        float t = 1.f - std::clamp(life / maxLife, 0.f, 1.f);
        radius = lerp(12.f, maxRadius, 1.f - (1.f - t) * (1.f - t));
    }

    void draw(sf::RenderWindow& win) const {
        float alpha = std::clamp(life / maxLife, 0.f, 1.f);
        sf::Color c = tint;
        c.a = static_cast<std::uint8_t>(static_cast<float>(tint.a) * alpha);
        ringCentered(win, center, radius, thickness * alpha, c);
    }

    bool dead() const { return life <= 0.f; }
};

struct FloatingText {
    sf::Vector2f pos;
    sf::Vector2f vel;
    std::string text;
    sf::Color tint;
    float life = 0.9f;
    float maxLife = 0.9f;
    unsigned size = 18;

    void update(float dt) {
        life -= dt;
        pos += vel * dt;
        vel.y += 25.f * dt;
    }

    bool dead() const { return life <= 0.f; }
};

// Ground shockwave spike spawned by the Boss's slam
struct GroundSpike {
    float x;
    float warmup = 0.22f;
    float active = 0.38f;
    float maxActive = 0.38f;
    float height = 115.f;
    float width = 34.f;
    bool didHit = false;
    sf::Color tint;

    void update(float dt) {
        if (warmup > 0.f) warmup -= dt;
        else active -= dt;
    }

    bool isDangerous() const { return warmup <= 0.f && active > 0.08f; }
    bool dead() const { return warmup <= 0.f && active <= 0.f; }

    void draw(sf::RenderWindow& win) const {
        if (warmup > 0.f) {
            float pulse = 0.5f + 0.5f * std::sin(warmup * 45.f);
            rectangle(win, {x - width * 0.6f, GROUND_Y - 4.f}, {width * 1.2f, 6.f},
                      color(255, 90, 45, static_cast<int>(190 * pulse)));
            circleCentered(win, {x, GROUND_Y}, 14.f, color(255, 120, 50, 90));
        } else {
            float ratio = std::clamp(active / maxActive, 0.f, 1.f);
            float pop = std::sin(ratio * PI);
            float h = height * std::min(1.f, pop * 1.35f);
            polygon(win, {
                {x - width * 0.5f, GROUND_Y},
                {x - width * 0.15f, GROUND_Y - h},
                {x + width * 0.5f, GROUND_Y}
            }, color(42, 32, 46));
            polygon(win, {
                {x - width * 0.3f, GROUND_Y},
                {x, GROUND_Y - h * 0.95f},
                {x + width * 0.25f, GROUND_Y}
            }, tint);
        }
    }
};

// Meteor / Void Orb spawned in boss barrage
struct Meteor {
    sf::Vector2f pos;
    sf::Vector2f vel;
    float targetX;
    float radius = 14.f;
    bool exploded = false;
    sf::Color tint;

    void update(float dt, std::vector<Particle>& particles, std::vector<Shockwave>& waves) {
        pos += vel * dt;
        if (randf(0.f, 1.f) < 0.75f) {
            particles.emplace_back(pos + sf::Vector2f(randf(-6, 6), randf(-6, 6)),
                -vel * 0.15f + sf::Vector2f(randf(-30, 30), randf(-30, 30)),
                randf(0.2f, 0.45f), randf(3.f, 6.f), tint, -20.f);
        }
        if (pos.y >= GROUND_Y - 6.f) {
            exploded = true;
            waves.push_back({{pos.x, GROUND_Y - 4.f}, 8.f, 75.f, 0.28f, 0.28f, 4.f, tint});
            for (int i = 0; i < 18; ++i) {
                float a = randf(-PI, 0.f);
                float sp = randf(70.f, 240.f);
                particles.emplace_back(sf::Vector2f(pos.x, GROUND_Y - 4.f),
                    sf::Vector2f(std::cos(a) * sp, std::sin(a) * sp),
                    randf(0.25f, 0.6f), randf(2.5f, 5.f), tint, 320.f);
            }
        }
    }

    void draw(sf::RenderWindow& win) const {
        // Target warning on ground
        float distRatio = std::clamp(1.f - (GROUND_Y - pos.y) / 550.f, 0.15f, 1.f);
        rectangle(win, {targetX - 26.f * distRatio, GROUND_Y - 3.f},
                  {52.f * distRatio, 4.f}, color(255, 65, 45, static_cast<int>(180 * distRatio)));
        // Glowing core
        circleCentered(win, pos, radius * 1.8f, color(tint.r, tint.g, tint.b, 65));
        circleCentered(win, pos, radius, tint);
        circleCentered(win, pos, radius * 0.5f, color(255, 240, 215));
    }
};

// ============================================================================
// ATMOSPHERE & TOWN ENVIRONMENT
// ============================================================================

class Atmosphere {
    std::vector<sf::Vector2f> stars;
    std::vector<Particle> motes;
    float time = 0.f;
    float lightningFlash = 0.f;

public:
    Atmosphere() {
        for (int i = 0; i < 105; ++i) stars.push_back({randf(0, WINDOW_W), randf(15, 340)});
        for (int i = 0; i < 95; ++i) {
            motes.emplace_back(sf::Vector2f(randf(0, WINDOW_W), randf(200, GROUND_Y)),
                               sf::Vector2f(randf(-14, 14), randf(-24, -6)),
                               randf(3, 8), randf(1, 2.7f),
                               color(255, static_cast<int>(randf(100, 190)), 55, 170), -5.f);
        }
    }

    void triggerLightning(float intensity = 0.85f) {
        lightningFlash = std::max(lightningFlash, intensity);
    }

    void update(float dt, bool phase2) {
        time += dt;
        lightningFlash = std::max(0.f, lightningFlash - dt * 2.8f);
        if (phase2 && randf(0.f, 1.f) < dt * 0.18f) {
            lightningFlash = randf(0.35f, 0.75f);
        }
        for (auto& p : motes) {
            p.update(dt);
            if (p.dead() || p.p.y < 150 || p.p.x < -10 || p.p.x > WINDOW_W + 10) {
                sf::Color c = phase2
                    ? color(255, static_cast<int>(randf(50, 130)), static_cast<int>(randf(60, 120)), 185)
                    : color(255, static_cast<int>(randf(95, 180)), 45, 170);
                p = Particle({randf(0, WINDOW_W), randf(GROUND_Y - 70, GROUND_Y + 15)},
                             {randf(-18, 18) + (phase2 ? -15.f : 0.f), randf(-45, -10)},
                             randf(4, 9), randf(1.2f, 2.8f), c, -8.f);
            }
        }
    }

    void drawSky(sf::RenderWindow& win, bool phase2) const {
        sf::Color skyColor = phase2 ? color(18, 10, 26) : color(8, 12, 27);
        rectangle(win, {0, 0}, {float(WINDOW_W), float(WINDOW_H)}, skyColor);

        // Layered twilight/blood-moon glow
        sf::Color glow1 = phase2 ? color(75, 20, 48, 75) : color(25, 29, 58, 70);
        sf::Color glow2 = phase2 ? color(120, 32, 55, 55) : color(45, 42, 78, 50);
        circle(win, {850, 120}, 240, glow1);
        circle(win, {850, 120}, 170, glow2);

        for (std::size_t i = 0; i < stars.size(); ++i) {
            float pulse = 0.65f + 0.35f * std::sin(time * 1.7f + static_cast<float>(i));
            circle(win, stars[i], (i % 7 == 0) ? 1.8f : 1.1f,
                   color(190, 205, 235, static_cast<int>(180 * pulse)));
        }

        // Moon with dynamic Blood Moon shift in Phase 2
        sf::Color moonHalo = phase2 ? color(220, 60, 75, 30) : color(160, 174, 210, 18);
        sf::Color moonBody = phase2 ? color(235, 135, 130) : color(205, 213, 230);
        circle(win, {1010, 95}, 66, moonHalo);
        circle(win, {1010, 95}, 48, moonBody);
        circle(win, {1028, 82}, 43, skyColor);

        // Distant mountain silhouettes
        polygon(win, {{0,420},{110,300},{215,400},{340,275},{490,415},{610,325},
                      {740,430},{900,300},{1060,405},{1170,310},{1280,405},{1280,565},{0,565}},
                phase2 ? color(28, 18, 38) : color(20, 25, 43));
        polygon(win, {{0,475},{180,385},{310,460},{480,355},{650,470},{820,385},
                      {1000,475},{1140,380},{1280,460},{1280,565},{0,565}},
                phase2 ? color(36, 24, 44) : color(28, 31, 48));

        if (lightningFlash > 0.f) {
            rectangle(win, {0, 0}, {float(WINDOW_W), float(WINDOW_H)},
                      color(230, 215, 255, static_cast<int>(lightningFlash * 110.f)));
        }
    }

    void drawMotes(sf::RenderWindow& win) const {
        for (const auto& p : motes) p.draw(win);
    }
};

class Town {
    struct Rubble {
        std::vector<sf::Vector2f> pts;
        sf::Color c;
    };
    std::vector<Rubble> rubbles;
    float lampTime = 0.f;

public:
    Town() {
        // Pre-compute rubble so it never flickers on redraw
        for (int i = 0; i < 36; ++i) {
            float x = float((i * 83 + 29) % WINDOW_W);
            float y = 572.f + float((i * 47) % 135);
            float w1 = 8.f + float(i % 10);
            float w2 = 14.f + float((i * 3) % 12);
            sf::Color col = (i % 3 == 0) ? color(40, 39, 46) : color(48, 46, 53);
            rubbles.push_back({{{x, y}, {x + w1, y - 3.f}, {x + w2, y + 5.f}, {x + 3.f, y + 8.f}}, col});
        }
    }

    void update(float dt) { lampTime += dt; }

    void draw(sf::RenderWindow& win) const {
        // Distant ruined cathedral spire in center background
        polygon(win, {{585, 565}, {600, 360}, {640, 285}, {680, 360}, {695, 565}}, color(23, 25, 39));
        rectangle(win, {632, 345}, {16, 48}, color(14, 16, 26));

        // Backline rooftops and towers
        drawHouse(win, 45, 385, 150, 180, color(30, 30, 44), lampTime);
        drawHouse(win, 205, 420, 125, 145, color(34, 31, 44), lampTime + 1.2f);
        drawHouse(win, 930, 390, 155, 175, color(31, 29, 44), lampTime + 2.5f);
        drawHouse(win, 1090, 420, 150, 145, color(36, 31, 43), lampTime + 3.7f);

        // Broken walls
        rectangle(win, {0, 500}, {155, 65}, color(43, 43, 53));
        rectangle(win, {0, 490}, {155, 12}, color(63, 58, 62));
        rectangle(win, {1100, 500}, {180, 65}, color(42, 40, 50));
        rectangle(win, {1100, 490}, {180, 12}, color(61, 56, 61));

        for (int i = 0; i < 7; ++i) {
            line(win, {float(i * 22), 516}, {float(i * 22 + 12), 516}, 2, color(78, 68, 66, 100));
            line(win, {1110.f + i * 24.f, 540}, {1122.f + i * 24.f, 540}, 2, color(78, 68, 66, 100));
        }

        // Iron street lanterns
        drawLantern(win, 365.f, GROUND_Y, lampTime);
        drawLantern(win, 905.f, GROUND_Y, lampTime + 1.8f);

        // Ground and road
        rectangle(win, {0, GROUND_Y}, {float(WINDOW_W), WINDOW_H - GROUND_Y}, color(20, 22, 31));
        polygon(win, {{400, GROUND_Y}, {880, GROUND_Y}, {1100, 720}, {190, 720}}, color(29, 29, 37));
        line(win, {625, 585}, {580, 710}, 3, color(95, 74, 62, 80));
        line(win, {660, 585}, {700, 710}, 3, color(95, 74, 62, 80));

        // Stable scattered rubble
        for (const auto& r : rubbles) {
            polygon(win, r.pts, r.c);
        }
    }

private:
    static void drawLantern(sf::RenderWindow& win, float x, float groundY, float t) {
        float flicker = 0.88f + 0.12f * std::sin(t * 9.f + x);
        line(win, {x, groundY}, {x, groundY - 95.f}, 4.f, color(32, 34, 46));
        line(win, {x, groundY - 93.f}, {x + 18.f, groundY - 93.f}, 3.f, color(32, 34, 46));
        rectangle(win, {x + 13.f, groundY - 88.f}, {10.f, 14.f}, color(245, 165, 75, static_cast<int>(210 * flicker)));
        circleCentered(win, {x + 18.f, groundY - 81.f}, 36.f * flicker, color(245, 140, 50, static_cast<int>(32 * flicker)));
    }

    static void drawHouse(sf::RenderWindow& win, float x, float y, float w, float h, sf::Color wall, float t) {
        float flicker = 0.9f + 0.1f * std::sin(t * 6.5f);
        rectangle(win, {x, y}, {w, h}, wall);
        polygon(win, {{x - 12, y + 10}, {x + w * 0.48f, y - h * 0.38f}, {x + w + 12, y + 10}},
                color(48, 34, 43));
        rectangle(win, {x + 12, y + 35}, {w - 24, 7}, color(69, 56, 60, 170));
        for (int i = 0; i < 2; ++i) {
            float wx = x + 22 + i * (w * 0.48f);
            rectangle(win, {wx, y + 58}, {22, 34}, color(231, 126, 53, static_cast<int>(185 * flicker)));
            rectangle(win, {wx + 9, y + 58}, {3, 34}, color(77, 49, 42));
            rectangle(win, {wx, y + 73}, {22, 3}, color(77, 49, 42));
        }
        rectangle(win, {x + w * 0.43f, y + h - 52}, {w * 0.18f, 52}, color(18, 19, 29));
        line(win, {x + w * 0.75f, y + 25}, {x + w * 0.68f, y + 55}, 2, color(8, 10, 18, 180));
        line(win, {x + w * 0.68f, y + 55}, {x + w * 0.78f, y + 82}, 2, color(8, 10, 18, 180));
    }
};

// ============================================================================
// PLAYER: JUMP, 3-HIT COMBO, SHADOW DASH, AEGIS PARRY & FLASK HEAL
// ============================================================================

struct Afterimage {
    sf::Vector2f pos;
    int facing;
    float life;
    float maxLife;
};

class Player {
    sf::Vector2f position{380.f, GROUND_Y - 125.f};
    sf::Vector2f velocity{0.f, 0.f};
    int facing = 1; // 1 = right, -1 = left
    bool grounded = true;

    // Health, Stamina & Flasks
    float health = 100.f;
    float maxHealth = 100.f;
    float delayedHealth = 100.f;
    float stamina = 100.f;
    float maxStamina = 100.f;
    int flasks = 3;

    // Combat & Timers
    int comboStep = 0;         // 0=R1, 1=R2, 2=Finisher, 3=Aerial Slam
    float attackTimer = 0.f;
    float attackDuration = 0.36f;
    float comboWindow = 0.f;
    bool attackRegistered = false;

    float dashTimer = 0.f;
    float dashCooldown = 0.f;

    float parryTimer = 0.f;
    float parryCooldown = 0.f;

    float invulnerable = 0.f;
    float hitFlash = 0.f;
    float bob = 0.f;
    float capeWave = 0.f;

    std::vector<Afterimage> afterimages;

public:
    void reset() {
        position = {380.f, GROUND_Y - 125.f};
        velocity = {0.f, 0.f};
        facing = 1;
        grounded = true;
        health = maxHealth;
        delayedHealth = maxHealth;
        stamina = maxStamina;
        flasks = 3;
        comboStep = 0;
        attackTimer = 0.f;
        comboWindow = 0.f;
        dashTimer = 0.f;
        parryTimer = 0.f;
        invulnerable = 0.f;
        hitFlash = 0.f;
        afterimages.clear();
    }

    void update(float dt, bool left, bool right, bool jumpPressed, bool jumpHeld,
                bool attackPressed, bool dashPressed, bool parryPressed, bool healPressed,
                std::vector<Particle>& particles, std::vector<Shockwave>& waves,
                std::vector<FloatingText>& texts) {

        delayedHealth = lerp(delayedHealth, health, dt * 4.5f);
        hitFlash = std::max(0.f, hitFlash - dt);
        invulnerable = std::max(0.f, invulnerable - dt);
        dashCooldown = std::max(0.f, dashCooldown - dt);
        parryCooldown = std::max(0.f, parryCooldown - dt);
        comboWindow = std::max(0.f, comboWindow - dt);
        capeWave += dt * 8.f;

        for (auto& img : afterimages) img.life -= dt;
        afterimages.erase(std::remove_if(afterimages.begin(), afterimages.end(),
            [](const Afterimage& a) { return a.life <= 0.f; }), afterimages.end());

        if (health <= 0.f) return;

        // Stamina regeneration
        if (attackTimer <= 0.f && dashTimer <= 0.f && parryTimer <= 0.f) {
            stamina = std::min(maxStamina, stamina + 28.f * dt);
        }

        // Ember Flask Heal
        if (healPressed && flasks > 0 && health < maxHealth && dashTimer <= 0.f) {
            --flasks;
            float healAmt = 45.f;
            health = std::min(maxHealth, health + healAmt);
            texts.push_back({center() + sf::Vector2f(0.f, -55.f), {0.f, -55.f},
                             "+45 HP", color(110, 245, 160), 1.0f, 1.0f, 18});
            waves.push_back({center(), 10.f, 85.f, 0.35f, 0.35f, 4.f, color(255, 200, 80, 210)});
            for (int i = 0; i < 24; ++i) {
                float a = randf(0.f, 2.f * PI);
                particles.emplace_back(center() + sf::Vector2f(randf(-20, 20), randf(-30, 30)),
                    sf::Vector2f(std::cos(a) * randf(20, 80), randf(-140, -40)),
                    randf(0.4f, 0.8f), randf(2.5f, 4.5f), color(255, 215, 90), -30.f);
            }
        }

        // Shadow Dash
        if (dashPressed && dashCooldown <= 0.f && dashTimer <= 0.f && stamina >= 20.f) {
            stamina -= 20.f;
            dashTimer = 0.20f;
            dashCooldown = 0.45f;
            invulnerable = std::max(invulnerable, 0.24f);
            attackTimer = 0.f;
            parryTimer = 0.f;
            if (left) facing = -1;
            else if (right) facing = 1;
            velocity.x = facing * 670.f;
            velocity.y = 0.f;
            for (int i = 0; i < 16; ++i) {
                particles.emplace_back(center() + sf::Vector2f(randf(-15, 15), randf(-35, 35)),
                    sf::Vector2f(-facing * randf(60, 180), randf(-40, 40)),
                    randf(0.2f, 0.45f), randf(2.5f, 4.5f), color(95, 185, 255, 200), 0.f);
            }
        }

        // Runic Parry
        if (parryPressed && parryCooldown <= 0.f && dashTimer <= 0.f && attackTimer <= 0.f && stamina >= 12.f) {
            stamina -= 12.f;
            parryTimer = 0.28f;
            parryCooldown = 0.65f;
            velocity.x *= 0.25f;
        }

        if (parryTimer > 0.f) {
            parryTimer -= dt;
        }

        // Sword Combo & Aerial Attack
        if (attackPressed && attackTimer <= 0.f && dashTimer <= 0.f && parryTimer <= 0.f && stamina >= 14.f) {
            stamina -= 14.f;
            attackRegistered = false;
            if (left) facing = -1;
            else if (right) facing = 1;

            if (!grounded) {
                comboStep = 3; // Plunging Aerial Slash
                attackDuration = 0.38f;
                attackTimer = attackDuration;
                velocity.y = 420.f;
                velocity.x = facing * 140.f;
            } else {
                if (comboWindow > 0.f) comboStep = (comboStep + 1) % 3;
                else comboStep = 0;

                attackDuration = (comboStep == 2) ? 0.46f : 0.32f;
                attackTimer = attackDuration;
                comboWindow = 0.75f;
                velocity.x = facing * ((comboStep == 2) ? 260.f : 115.f);
            }

            sf::Color slashCol = (comboStep == 2) ? color(255, 115, 75) : color(110, 215, 255);
            for (int i = 0; i < 16; ++i) {
                float a = randf(-1.1f, 1.1f);
                particles.emplace_back(center() + sf::Vector2f(facing * 28.f, -8.f),
                    sf::Vector2f(facing * 220.f * std::cos(a), 190.f * std::sin(a)),
                    randf(0.22f, 0.55f), randf(2.f, 4.5f), slashCol, 20.f);
            }
        }

        // Horizontal Movement
        if (dashTimer > 0.f) {
            dashTimer -= dt;
            if (static_cast<int>(dashTimer * 60.f) % 2 == 0) {
                afterimages.push_back({position, facing, 0.22f, 0.22f});
            }
        } else if (attackTimer > 0.f) {
            attackTimer = std::max(0.f, attackTimer - dt);
            velocity.x *= std::pow(0.08f, dt);
        } else if (parryTimer <= 0.f) {
            const float moveSpeed = 285.f;
            float moveDir = 0.f;
            if (left) { moveDir -= 1.f; facing = -1; }
            if (right) { moveDir += 1.f; facing = 1; }
            velocity.x = lerp(velocity.x, moveDir * moveSpeed, dt * 14.f);
            if (std::abs(moveDir) > 0.1f && grounded) {
                bob += dt * 11.f;
            } else {
                bob += dt * 4.f;
            }
        }

        // Jump & Gravity
        if (jumpPressed && grounded && dashTimer <= 0.f && parryTimer <= 0.f) {
            grounded = false;
            velocity.y = -590.f;
            for (int i = 0; i < 10; ++i) {
                particles.emplace_back(sf::Vector2f(position.x + 28.f, GROUND_Y - 2.f),
                    sf::Vector2f(randf(-110, 110), randf(-90, -20)),
                    randf(0.25f, 0.5f), randf(2.f, 4.f), color(150, 155, 175, 180), 120.f);
            }
        }

        if (!grounded && dashTimer <= 0.f) {
            float grav = (velocity.y < 0.f && !jumpHeld) ? 2450.f : 1380.f;
            velocity.y += grav * dt;
        }

        position += velocity * dt;
        position.x = std::clamp(position.x, 55.f, 1165.f);

        // Ground Landing
        if (position.y >= GROUND_Y - 125.f) {
            if (!grounded) {
                // Landing dust
                for (int i = 0; i < 10; ++i) {
                    particles.emplace_back(sf::Vector2f(position.x + 28.f, GROUND_Y - 2.f),
                        sf::Vector2f(randf(-120, 120), randf(-70, -15)),
                        randf(0.2f, 0.45f), randf(2.f, 3.8f), color(140, 145, 165, 170), 140.f);
                }
                if (comboStep == 3 && attackTimer > 0.f) {
                    waves.push_back({{center().x, GROUND_Y - 4.f}, 12.f, 130.f, 0.3f, 0.3f, 4.f, color(115, 220, 255)});
                }
            }
            position.y = GROUND_Y - 125.f;
            velocity.y = 0.f;
            grounded = true;
        }
    }

    sf::Vector2f center() const { return {position.x + 28.f, position.y + 68.f}; }
    int getFacing() const { return facing; }
    bool isAttacking() const { return attackTimer > 0.f; }
    bool isParrying() const { return parryTimer > 0.f; }
    bool isInvulnerable() const { return invulnerable > 0.f; }
    bool canDealHit() const {
        float progress = 1.f - (attackTimer / std::max(0.01f, attackDuration));
        return isAttacking() && !attackRegistered && progress >= 0.2f && progress <= 0.85f;
    }
    void markHitRegistered() { attackRegistered = true; }
    int getComboStep() const { return comboStep; }

    float getAttackDamage() const {
        if (comboStep == 2) return 28.f;
        if (comboStep == 3) return 24.f;
        if (comboStep == 1) return 17.f;
        return 14.f;
    }

    float getAttackReach() const {
        return (comboStep == 2) ? 185.f : 155.f;
    }

    void onPerfectParry(std::vector<Particle>& particles, std::vector<Shockwave>& waves) {
        parryTimer = 0.f;
        parryCooldown = 0.1f;
        invulnerable = 0.55f;
        stamina = std::min(maxStamina, stamina + 40.f);
        waves.push_back({center() + sf::Vector2f(facing * 24.f, -8.f),
                         15.f, 195.f, 0.38f, 0.38f, 6.f, color(120, 235, 255)});
        for (int i = 0; i < 28; ++i) {
            float a = randf(0.f, 2.f * PI);
            float sp = randf(120.f, 360.f);
            particles.emplace_back(center() + sf::Vector2f(facing * 24.f, -8.f),
                sf::Vector2f(std::cos(a) * sp, std::sin(a) * sp),
                randf(0.25f, 0.6f), randf(2.5f, 5.f), color(145, 240, 255), 80.f);
        }
    }

    bool takeDamage(float amount, float sourceX, std::vector<Particle>& particles) {
        if (invulnerable > 0.f || health <= 0.f) return false;
        health = std::max(0.f, health - amount);
        invulnerable = 0.85f;
        hitFlash = 0.22f;
        attackTimer = 0.f;
        float pushDir = (center().x < sourceX) ? -1.f : 1.f;
        velocity.x = pushDir * 310.f;
        velocity.y = -210.f;
        grounded = false;
        for (int i = 0; i < 18; ++i) {
            particles.emplace_back(center(),
                sf::Vector2f(randf(-180, 180), randf(-200, 40)),
                randf(0.25f, 0.6f), randf(2.5f, 4.5f), color(235, 55, 65), 320.f);
        }
        return true;
    }

    float getHealth() const { return health; }
    float getDelayedHealth() const { return delayedHealth; }
    float getStamina() const { return stamina; }
    int getFlasks() const { return flasks; }

    void draw(sf::RenderWindow& win) const {
        // Draw Dash Afterimages
        for (const auto& img : afterimages) {
            float a = std::clamp(img.life / img.maxLife, 0.f, 1.f);
            sf::Color ghostCol = color(75, 165, 235, static_cast<int>(115 * a));
            float cx = img.pos.x + 28.f;
            float gy = img.pos.y;
            polygon(win, {
                {cx - img.facing * 20.f, gy + 35.f},
                {cx + img.facing * 22.f, gy + 35.f},
                {cx + img.facing * 32.f, gy + 111.f},
                {cx - img.facing * 34.f, gy + 111.f}
            }, ghostCol);
        }

        if (invulnerable > 0.f && hitFlash <= 0.f && static_cast<int>(invulnerable * 28.f) % 2 == 1) {
            // Brief invulnerability blink
        }

        float y = position.y + (grounded ? std::sin(bob) * 2.5f : 0.f);
        float cx = position.x + 28.f;
        float f = static_cast<float>(facing);
        auto X = [&](float offsetFromCenter) { return cx + f * offsetFromCenter; };

        // Ground shadow scales with height
        float heightFactor = std::clamp(1.f - (GROUND_Y - 125.f - position.y) / 260.f, 0.35f, 1.f);
        circleCentered(win, {cx, GROUND_Y - 4.f}, 28.f * heightFactor, color(0, 0, 0, static_cast<int>(105 * heightFactor)));

        // Flowing Cloak with wind/movement flutter
        float capeLag = std::clamp(-velocity.x * 0.045f * f, -18.f, 22.f) + std::sin(capeWave) * 4.5f;
        sf::Color cloakOuter = hitFlash > 0.f ? color(255, 210, 210) : color(34, 42, 64);
        sf::Color cloakInner = hitFlash > 0.f ? color(255, 230, 230) : color(49, 65, 91);

        polygon(win, {
            {X(-20), y + 35},
            {X(22), y + 35},
            {X(35 - capeLag * 0.5f), y + 111},
            {X(-34 - capeLag), y + 112}
        }, cloakOuter);
        polygon(win, {
            {X(-15), y + 43},
            {X(17), y + 45},
            {X(27 - capeLag * 0.4f), y + 108},
            {X(-26 - capeLag * 0.8f), y + 108}
        }, cloakInner);

        // Animated legs and boots
        float stride = ( std::abs(velocity.x) > 25.f && grounded ) ? std::sin(bob * 1.4f) * 6.f : 0.f;
        line(win, {X(-10), y + 93}, {X(-10 - stride), y + 118}, 13.f, color(24, 26, 38));
        line(win, {X(12), y + 93}, {X(12 + stride), y + 118}, 13.f, color(24, 26, 38));
        line(win, {X(-18 - stride), y + 118}, {X(2 - stride), y + 118}, 8.f, color(12, 14, 22));
        line(win, {X(4 + stride), y + 118}, {X(24 + stride), y + 118}, 8.f, color(12, 14, 22));

        // Head, hair & glowing eyes
        circleCentered(win, {X(4), y + 24}, 16.f, color(185, 143, 115));
        polygon(win, {
            {X(-15), y + 22}, {X(-10), y + 4}, {X(14), y + 3},
            {X(22), y + 18}, {X(12), y + 13}, {X(-6), y + 15}
        }, color(25, 25, 35));
        circleCentered(win, {X(11), y + 24}, 2.3f, color(115, 225, 255));

        // Chest armor & core rune
        polygon(win, {
            {X(-21), y + 34}, {X(19), y + 34},
            {X(14), y + 66}, {X(-15), y + 66}
        }, hitFlash > 0.f ? color(255, 240, 240) : color(83, 92, 112));
        line(win, {X(-1), y + 37}, {X(-1), y + 63}, 3.f, color(21, 25, 38));
        circleCentered(win, {X(0), y + 51}, 4.5f, color(94, 207, 239));

        // Parry Runic Shield
        if (parryTimer > 0.f) {
            float pAlpha = std::clamp(parryTimer / 0.28f, 0.f, 1.f);
            sf::Vector2f shieldPos{X(32), y + 55};
            ringCentered(win, shieldPos, 38.f, 4.f, color(110, 235, 255, static_cast<int>(230 * pAlpha)));
            circleCentered(win, shieldPos, 36.f, color(80, 200, 255, static_cast<int>(55 * pAlpha)));
            line(win, {X(22), y + 24}, {X(38), y + 86}, 5.f, color(185, 245, 255));
        }
        // Sword & Combo Slash Arcs
        else if (attackTimer > 0.f) {
            float progress = 1.f - (attackTimer / std::max(0.01f, attackDuration));
            float startDeg = -75.f, endDeg = 55.f;
            sf::Color arcColor = color(115, 220, 255, 210);
            float bladeLen = 84.f;

            if (comboStep == 1) {
                startDeg = 65.f; endDeg = -85.f; // Rising slash
                arcColor = color(135, 240, 255, 220);
            } else if (comboStep == 2) {
                startDeg = -110.f; endDeg = 75.f; // Heavy crimson finisher
                arcColor = color(255, 115, 65, 235);
                bladeLen = 108.f;
            } else if (comboStep == 3) {
                startDeg = -95.f; endDeg = 85.f; // Aerial plunge
                arcColor = color(140, 230, 255, 230);
                bladeLen = 96.f;
            }

            float curDeg = lerp(startDeg, endDeg, progress);
            float rad = curDeg * PI / 180.f;
            sf::Vector2f shoulder{X(16), y + 52};
            sf::Vector2f tip{shoulder.x + f * std::cos(rad) * bladeLen,
                             shoulder.y + std::sin(rad) * bladeLen};

            // Draw glowing motion trail fan
            const int segs = 7;
            for (int i = 0; i < segs; ++i) {
                float t0 = std::max(0.f, progress - float(i + 1) * 0.06f);
                float t1 = std::max(0.f, progress - float(i) * 0.06f);
                float a0 = lerp(startDeg, endDeg, t0) * PI / 180.f;
                float a1 = lerp(startDeg, endDeg, t1) * PI / 180.f;
                sf::Vector2f p0{shoulder.x + f * std::cos(a0) * bladeLen, shoulder.y + std::sin(a0) * bladeLen};
                sf::Vector2f p1{shoulder.x + f * std::cos(a1) * bladeLen, shoulder.y + std::sin(a1) * bladeLen};
                sf::Color trailCol = arcColor;
                trailCol.a = static_cast<std::uint8_t>((segs - i) * 22);
                polygon(win, {shoulder, p0, p1}, trailCol);
            }

            // Blade & Hilt
            line(win, shoulder, tip, 7.f, color(95, 115, 140));
            line(win, shoulder, tip, 3.f, (comboStep == 2) ? color(255, 220, 175) : color(205, 240, 255));
            line(win, shoulder - sf::Vector2f(f * 6.f, 0.f), shoulder + sf::Vector2f(f * 8.f, 0.f), 8.f, color(133, 87, 53));
        } else {
            // Idle / Run sword at side
            line(win, {X(18), y + 58}, {X(40), y + 100}, 6.f, color(100, 113, 132));
            line(win, {X(20), y + 56}, {X(42), y + 98}, 2.f, color(210, 232, 248));
            line(win, {X(14), y + 55}, {X(25), y + 50}, 7.f, color(135, 88, 53));
        }
    }
};

// ============================================================================
// MONSTER: THE ASHEN COLOSSUS (4 ATTACK PATTERNS, PHASE 2 & STAGGER)
// ============================================================================

class Monster {
public:
    enum class State { Idle, Walk, Windup, Strike, Recover, Staggered, Dead };
    enum class Move { None, GroundSlam, AbyssalCleave, MeteorBarrage };

private:
    sf::Vector2f position{880.f, GROUND_Y - 220.f};
    int facing = -1; // -1 = facing left toward player, 1 = facing right
    float time = 0.f;

    float health = 500.f;
    float maxHealth = 500.f;
    float delayedHealth = 500.f;
    bool phase2 = false;
    bool phase2Triggered = false;

    State state = State::Idle;
    Move currentMove = Move::None;
    float stateTimer = 1.1f;
    float stateDuration = 1.1f;
    bool strikeDealt = false;
    float hitFlash = 0.f;
    float deathDissolve = 0.f;

public:
    void reset() {
        position = {880.f, GROUND_Y - 220.f};
        facing = -1;
        health = maxHealth;
        delayedHealth = maxHealth;
        phase2 = false;
        phase2Triggered = false;
        state = State::Idle;
        currentMove = Move::None;
        stateTimer = 1.0f;
        stateDuration = 1.0f;
        strikeDealt = false;
        hitFlash = 0.f;
        deathDissolve = 0.f;
    }

    void update(float dt, Player& player, Atmosphere& atmosphere,
                std::vector<Particle>& particles,
                std::vector<Shockwave>& waves,
                std::vector<GroundSpike>& spikes,
                std::vector<Meteor>& meteors,
                std::vector<FloatingText>& texts,
                float& shake, float& hitstop) {

        time += dt;
        delayedHealth = lerp(delayedHealth, health, dt * 4.0f);
        hitFlash = std::max(0.f, hitFlash - dt);

        if (health <= 0.f) {
            if (state != State::Dead) {
                state = State::Dead;
                shake = 0.65f;
                waves.push_back({center(), 20.f, 380.f, 0.65f, 0.65f, 8.f, color(255, 95, 50)});
                for (int i = 0; i < 65; ++i) {
                    float a = randf(0.f, 2.f * PI);
                    float sp = randf(60.f, 340.f);
                    particles.emplace_back(center() + sf::Vector2f(randf(-50, 50), randf(-80, 60)),
                        sf::Vector2f(std::cos(a) * sp, std::sin(a) * sp),
                        randf(0.5f, 1.4f), randf(3.f, 7.f), color(255, 110, 50), 120.f);
                }
            }
            deathDissolve = std::min(1.f, deathDissolve + dt * 0.45f);
            if (randf(0.f, 1.f) < 0.5f && deathDissolve < 0.95f) {
                particles.emplace_back(center() + sf::Vector2f(randf(-70, 70), randf(-90, 50)),
                    sf::Vector2f(randf(-40, 40), randf(-120, -30)),
                    randf(0.4f, 0.9f), randf(2.5f, 5.f), color(255, 90, 45), -20.f);
            }
            return;
        }

        // Check Phase 2 Awakening
        if (!phase2Triggered && health <= maxHealth * 0.5f) {
            phase2 = true;
            phase2Triggered = true;
            state = State::Recover;
            stateTimer = 1.0f;
            shake = 0.55f;
            atmosphere.triggerLightning(1.0f);
            waves.push_back({center(), 20.f, 360.f, 0.55f, 0.55f, 8.f, color(255, 55, 95)});
            texts.push_back({center() + sf::Vector2f(0.f, -150.f), {0.f, -35.f},
                             "PHASE II: ASHEN OVERDRIVE!", color(255, 85, 110), 1.8f, 1.8f, 22});
            for (int i = 0; i < 45; ++i) {
                float a = randf(0.f, 2.f * PI);
                particles.emplace_back(center(),
                    sf::Vector2f(std::cos(a) * randf(90, 320), std::sin(a) * randf(90, 320)),
                    randf(0.4f, 0.9f), randf(3.f, 6.f), color(255, 65, 95), 40.f);
            }
            return;
        }

        // Ambient Phase 2 ember aura
        if (phase2 && randf(0.f, 1.f) < 0.45f) {
            particles.emplace_back(center() + sf::Vector2f(randf(-65, 65), randf(-90, 60)),
                sf::Vector2f(randf(-30, 30), randf(-110, -40)),
                randf(0.3f, 0.7f), randf(2.f, 4.5f), color(255, 60, 95, 200), -30.f);
        }

        float dx = player.center().x - position.x;
        float absDx = std::abs(dx);
        stateTimer -= dt;

        switch (state) {
            case State::Idle: {
                if (dx != 0.f) facing = (dx > 0.f) ? 1 : -1;
                if (stateTimer <= 0.f) {
                    state = State::Walk;
                    stateTimer = randf(0.9f, 1.6f);
                }
                break;
            }
            case State::Walk: {
                if (dx != 0.f) facing = (dx > 0.f) ? 1 : -1;
                float walkSpeed = phase2 ? 105.f : 72.f;
                if (absDx > 165.f) {
                    position.x += facing * walkSpeed * dt;
                }
                position.x = std::clamp(position.x, 180.f, 1100.f);

                if (stateTimer <= 0.f || absDx <= 185.f) {
                    // Pick an attack move
                    float roll = randf(0.f, 1.f);
                    if (absDx < 220.f && roll < 0.52f) {
                        currentMove = Move::AbyssalCleave;
                        stateDuration = phase2 ? 0.58f : 0.75f;
                    } else if (roll < 0.80f) {
                        currentMove = Move::GroundSlam;
                        stateDuration = phase2 ? 0.68f : 0.88f;
                    } else {
                        currentMove = Move::MeteorBarrage;
                        stateDuration = phase2 ? 0.65f : 0.85f;
                    }
                    state = State::Windup;
                    stateTimer = stateDuration;
                    strikeDealt = false;
                }
                break;
            }
            case State::Windup: {
                // Warning particles during telegraph
                sf::Color warnCol = (currentMove == Move::AbyssalCleave)
                    ? color(255, 60, 60) : color(255, 145, 50);
                if (randf(0.f, 1.f) < 0.65f) {
                    particles.emplace_back(center() + sf::Vector2f(facing * 45.f + randf(-30, 30), randf(-80, 20)),
                        sf::Vector2f(randf(-40, 40), randf(-90, -20)),
                        randf(0.2f, 0.45f), randf(2.5f, 5.f), warnCol, -40.f);
                }
                if (stateTimer <= 0.f) {
                    state = State::Strike;
                    stateDuration = 0.32f;
                    stateTimer = stateDuration;
                }
                break;
            }
            case State::Strike: {
                if (!strikeDealt) {
                    strikeDealt = true;
                    if (currentMove == Move::AbyssalCleave) {
                        // Lunge forward and cleave
                        position.x = std::clamp(position.x + facing * (phase2 ? 135.f : 95.f), 160.f, 1120.f);
                        shake = 0.22f;
                        waves.push_back({center() + sf::Vector2f(facing * 75.f, 20.f),
                                         15.f, 150.f, 0.28f, 0.28f, 5.f, color(255, 75, 55)});

                        float hitDist = (player.center().x - position.x) * facing;
                        float vertDist = std::abs(player.center().y - (GROUND_Y - 55.f));
                        if (hitDist > -45.f && hitDist < 205.f && vertDist < 95.f) {
                            if (player.isParrying()) {
                                // PERFECT PARRY!
                                player.onPerfectParry(particles, waves);
                                health = std::max(0.f, health - 35.f);
                                hitFlash = 0.35f;
                                state = State::Staggered;
                                stateDuration = 1.9f;
                                stateTimer = stateDuration;
                                shake = 0.38f;
                                hitstop = 0.11f;
                                texts.push_back({player.center() + sf::Vector2f(0.f, -65.f), {0.f, -55.f},
                                                 "PERFECT PARRY! -35", color(120, 245, 255), 1.2f, 1.2f, 20});
                                break;
                            } else if (player.takeDamage(phase2 ? 26.f : 20.f, position.x, particles)) {
                                shake = 0.28f;
                                hitstop = 0.06f;
                                texts.push_back({player.center() + sf::Vector2f(0.f, -45.f), {0.f, -50.f},
                                                 phase2 ? "-26" : "-20", color(255, 75, 75), 0.9f, 0.9f, 18});
                            }
                        }
                    } else if (currentMove == Move::GroundSlam) {
                        shake = 0.38f;
                        waves.push_back({{position.x, GROUND_Y - 5.f}, 18.f, 240.f, 0.42f, 0.42f, 6.f, color(255, 110, 45)});
                        int count = phase2 ? 8 : 6;
                        sf::Color spCol = phase2 ? color(255, 65, 95) : color(255, 110, 45);
                        for (int i = 1; i <= count; ++i) {
                            float sx1 = position.x + facing * (70.f + i * 78.f);
                            if (sx1 > 40.f && sx1 < WINDOW_W - 40.f) {
                                spikes.push_back({sx1, i * 0.085f, 0.38f, 0.38f, 118.f, 36.f, false, spCol});
                            }
                            if (phase2) {
                                float sx2 = position.x - facing * (70.f + i * 85.f);
                                if (sx2 > 40.f && sx2 < WINDOW_W - 40.f) {
                                    spikes.push_back({sx2, i * 0.095f, 0.38f, 0.38f, 118.f, 36.f, false, spCol});
                                }
                            }
                        }
                        atmosphere.triggerLightning(0.55f);
                    } else if (currentMove == Move::MeteorBarrage) {
                        shake = 0.18f;
                        int count = phase2 ? 6 : 4;
                        for (int i = 0; i < count; ++i) {
                            float tx = player.center().x + randf(-180.f, 180.f);
                            tx = std::clamp(tx, 80.f, float(WINDOW_W) - 80.f);
                            float sx = tx + randf(-120.f, 120.f);
                            sf::Color mCol = phase2 ? color(255, 55, 110) : color(255, 145, 45);
                            meteors.push_back({{sx, -60.f - i * 55.f},
                                               {(tx - sx) * 0.6f, 340.f + randf(0.f, 80.f)},
                                               tx, 14.f, false, mCol});
                        }
                        atmosphere.triggerLightning(0.4f);
                    }
                }
                if (stateTimer <= 0.f) {
                    state = State::Recover;
                    stateDuration = phase2 ? 0.65f : 0.95f;
                    stateTimer = stateDuration;
                }
                break;
            }
            case State::Recover: {
                if (stateTimer <= 0.f) {
                    state = State::Walk;
                    stateTimer = randf(0.55f, 1.2f);
                }
                break;
            }
            case State::Staggered: {
                // Boss is stunned — players can deal extra damage
                if (randf(0.f, 1.f) < 0.35f) {
                    particles.emplace_back(center() + sf::Vector2f(randf(-60, 60), randf(-80, 40)),
                        sf::Vector2f(randf(-50, 50), randf(-90, -20)),
                        randf(0.3f, 0.6f), randf(2.5f, 4.5f), color(255, 230, 120), -35.f);
                }
                if (stateTimer <= 0.f) {
                    state = State::Walk;
                    stateTimer = randf(0.3f, 0.7f);
                }
                break;
            }
            case State::Dead:
                break;
        }
    }

    void takeDamage(float amount, const Player& player,
                    std::vector<Particle>& particles, std::vector<Shockwave>& waves,
                    std::vector<FloatingText>& texts, float& shake, float& hitstop) {
        health = std::max(0.f, health - amount);
        hitFlash = 0.18f;
        shake = std::max(shake, 0.12f);
        hitstop = std::max(hitstop, 0.04f);

        std::ostringstream oss;
        oss << "-" << static_cast<int>(amount);
        sf::Color dmgCol = (player.getComboStep() == 2) ? color(255, 135, 75) : color(255, 235, 115);
        texts.push_back({center() + sf::Vector2f(randf(-40, 40), randf(-120, -80)),
                         {0.f, -55.f}, oss.str(), dmgCol, 0.9f, 0.9f, 20});

        for (int i = 0; i < 12; ++i) {
            float a = randf(0.f, 2.f * PI);
            float sp = randf(60.f, 210.f);
            particles.emplace_back(center() + sf::Vector2f(randf(-30, 30), randf(-40, 40)),
                sf::Vector2f(std::cos(a) * sp, std::sin(a) * sp),
                randf(0.2f, 0.5f), randf(2.f, 4.5f), color(255, 135, 55), 120.f);
        }
    }

    sf::Vector2f center() const { return {position.x + 55.f, position.y + 110.f}; }
    float getHealth() const { return health; }
    float getMaxHealth() const { return maxHealth; }
    float getDelayedHealth() const { return delayedHealth; }
    bool isPhase2() const { return phase2; }
    bool isDead() const { return state == State::Dead && deathDissolve >= 0.95f; }
    bool isStaggered() const { return state == State::Staggered; }
    State getState() const { return state; }

    void draw(sf::RenderWindow& win) const {
        if (state == State::Dead && deathDissolve >= 0.95f) return;
        int alpha = (state == State::Dead)
            ? static_cast<int>(255 * (1.f - deathDissolve))
            : 255;
        float f = static_cast<float>(facing);
        float cx = position.x + 55.f;
        float y = position.y;
        auto X = [&](float offsetFromCenter) { return cx + f * offsetFromCenter; };

        bool flash = hitFlash > 0.f;

        // Ground shadow
        circleCentered(win, {cx, GROUND_Y - 4.f}, 55.f, color(0, 0, 0, std::min(alpha, 90)));

        // Legs — massive armored pillars
        rectangle(win, {X(-42) - 18.f, y + 165}, {36.f, 62.f},
                  flash ? color(255, 230, 230, alpha) : color(48, 38, 55, alpha));
        rectangle(win, {X(18) - 10.f, y + 165}, {36.f, 62.f},
                  flash ? color(255, 230, 230, alpha) : color(48, 38, 55, alpha));
        // Boots
        rectangle(win, {X(-42) - 22.f, y + 218}, {42.f, 12.f},
                  flash ? color(255, 240, 240, alpha) : color(32, 28, 40, alpha));
        rectangle(win, {X(18) - 14.f, y + 218}, {42.f, 12.f},
                  flash ? color(255, 240, 240, alpha) : color(32, 28, 40, alpha));

        // Torso — massive slab
        polygon(win, {
            {X(-52), y + 50}, {X(52), y + 50},
            {X(48), y + 170}, {X(-48), y + 170}
        }, flash ? color(255, 235, 235, alpha) : color(62, 48, 72, alpha));

        // Chest plate with dark inlay
        polygon(win, {
            {X(-38), y + 60}, {X(38), y + 60},
            {X(33), y + 130}, {X(-33), y + 130}
        }, flash ? color(255, 245, 245, alpha) : color(82, 68, 95, alpha));

        // Glowing core rune
        sf::Color runeCol = phase2 ? color(255, 55, 85, alpha) : color(255, 145, 55, alpha);
        circleCentered(win, {cx, y + 95}, 12.f, color(runeCol.r, runeCol.g, runeCol.b, std::min(alpha, 90)));
        circleCentered(win, {cx, y + 95}, 7.f, runeCol);

        // Pauldrons (shoulder armor)
        polygon(win, {
            {X(-52), y + 48}, {X(-68), y + 55}, {X(-62), y + 80}, {X(-48), y + 72}
        }, flash ? color(255, 240, 240, alpha) : color(55, 48, 68, alpha));
        polygon(win, {
            {X(52), y + 48}, {X(68), y + 55}, {X(62), y + 80}, {X(48), y + 72}
        }, flash ? color(255, 240, 240, alpha) : color(55, 48, 68, alpha));

        // Arms
        line(win, {X(-58), y + 68}, {X(-68), y + 140}, 18.f,
             flash ? color(255, 230, 230, alpha) : color(52, 42, 62, alpha));
        line(win, {X(58), y + 68}, {X(68), y + 140}, 18.f,
             flash ? color(255, 230, 230, alpha) : color(52, 42, 62, alpha));

        // Weapon arm — giant mace/blade
        if (state == State::Strike || state == State::Windup) {
            float progress = 1.f - (stateTimer / std::max(0.01f, stateDuration));
            sf::Vector2f weaponBase{X(65), y + 110};
            float weapAngle;
            if (state == State::Windup) {
                weapAngle = lerp(-20.f, -135.f, progress) * PI / 180.f;
            } else {
                weapAngle = lerp(-135.f, 45.f, std::min(1.f, progress * 1.8f)) * PI / 180.f;
            }
            float weapLen = 120.f;
            sf::Vector2f weapTip{weaponBase.x + f * std::cos(weapAngle) * weapLen,
                                weaponBase.y + std::sin(weapAngle) * weapLen};
            line(win, weaponBase, weapTip, 12.f, color(65, 55, 78, alpha));
            line(win, weaponBase, weapTip, 5.f, color(180, 165, 195, alpha));
            // Mace head glow
            circleCentered(win, weapTip, 18.f, color(runeCol.r, runeCol.g, runeCol.b, std::min(alpha, 120)));
            circleCentered(win, weapTip, 11.f, color(runeCol.r, runeCol.g, runeCol.b, alpha));
        } else {
            // Weapon at rest
            sf::Vector2f weapBase{X(65), y + 120};
            sf::Vector2f weapEnd{X(75), y + 195};
            line(win, weapBase, weapEnd, 10.f, color(65, 55, 78, alpha));
            line(win, weapBase, weapEnd, 4.f, color(175, 160, 190, alpha));
            circleCentered(win, weapEnd, 14.f, color(runeCol.r, runeCol.g, runeCol.b, std::min(alpha, 80)));
        }

        // Head — horned helm
        circleCentered(win, {cx, y + 38}, 28.f,
                       flash ? color(255, 235, 235, alpha) : color(52, 42, 62, alpha));
        // Visor slit with glowing eyes
        rectangle(win, {X(-18), y + 32}, {36.f, 8.f}, color(15, 12, 22, alpha));
        circleCentered(win, {X(-10), y + 36}, 3.5f, color(runeCol.r, runeCol.g, runeCol.b, alpha));
        circleCentered(win, {X(10), y + 36}, 3.5f, color(runeCol.r, runeCol.g, runeCol.b, alpha));

        // Horns
        polygon(win, {
            {X(-22), y + 28}, {X(-38), y - 8}, {X(-28), y + 12}, {X(-18), y + 22}
        }, flash ? color(255, 240, 240, alpha) : color(42, 35, 52, alpha));
        polygon(win, {
            {X(22), y + 28}, {X(38), y - 8}, {X(28), y + 12}, {X(18), y + 22}
        }, flash ? color(255, 240, 240, alpha) : color(42, 35, 52, alpha));

        // Stagger indicator (spinning stars)
        if (state == State::Staggered) {
            for (int i = 0; i < 5; ++i) {
                float a = time * 3.5f + i * (2.f * PI / 5.f);
                sf::Vector2f starPos{cx + std::cos(a) * 42.f, y + 8.f + std::sin(a) * 12.f};
                circleCentered(win, starPos, 3.5f, color(255, 230, 120, 210));
            }
        }
    }
};

// ============================================================================
// MAIN — GAME LOOP, INPUT, COLLISION, HUD
// ============================================================================

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode({WINDOW_W, WINDOW_H}), "HERO: LAST STAND");
    window.setFramerateLimit(60);

    sf::Font font;
    bool fontLoaded = font.openFromFile("C:/Windows/Fonts/arial.ttf");

    Atmosphere atmosphere;
    Town town;
    Player player;
    Monster monster;

    std::vector<Particle> particles;
    std::vector<Shockwave> waves;
    std::vector<FloatingText> floatingTexts;
    std::vector<GroundSpike> spikes;
    std::vector<Meteor> meteors;

    float screenShake = 0.f;
    float hitstopTimer = 0.f;

    enum class GameState { Playing, Victory, Defeat };
    GameState gameState = GameState::Playing;
    float endTimer = 0.f;

    sf::Clock clock;

    while (window.isOpen()) {
        // — Event Handling —
        bool jumpPressed = false, attackPressed = false;
        bool dashPressed = false, parryPressed = false, healPressed = false;

        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            if (const auto* keyEvt = event->getIf<sf::Event::KeyPressed>()) {
                if (keyEvt->code == sf::Keyboard::Key::Space) jumpPressed = true;
                if (keyEvt->code == sf::Keyboard::Key::Z ||
                    keyEvt->code == sf::Keyboard::Key::J) attackPressed = true;
                if (keyEvt->code == sf::Keyboard::Key::LShift ||
                    keyEvt->code == sf::Keyboard::Key::K) dashPressed = true;
                if (keyEvt->code == sf::Keyboard::Key::X ||
                    keyEvt->code == sf::Keyboard::Key::L) parryPressed = true;
                if (keyEvt->code == sf::Keyboard::Key::C ||
                    keyEvt->code == sf::Keyboard::Key::H) healPressed = true;
                if (keyEvt->code == sf::Keyboard::Key::R &&
                    gameState != GameState::Playing) {
                    // Reset
                    player.reset();
                    monster.reset();
                    particles.clear();
                    waves.clear();
                    floatingTexts.clear();
                    spikes.clear();
                    meteors.clear();
                    screenShake = 0.f;
                    hitstopTimer = 0.f;
                    gameState = GameState::Playing;
                    endTimer = 0.f;
                    atmosphere = Atmosphere();
                    town = Town();
                }
                if (keyEvt->code == sf::Keyboard::Key::Escape) {
                    window.close();
                }
            }
        }

        bool leftHeld  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
                         sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
        bool rightHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
                         sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
        bool jumpHeld  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);

        float rawDt = clock.restart().asSeconds();
        rawDt = std::min(rawDt, 1.f / 15.f); // cap dt

        // Hitstop freeze
        if (hitstopTimer > 0.f) {
            hitstopTimer -= rawDt;
            continue; // freeze frame
        }
        float dt = rawDt;

        // — Update —
        if (gameState == GameState::Playing) {
            player.update(dt, leftHeld, rightHeld, jumpPressed, jumpHeld,
                          attackPressed, dashPressed, parryPressed, healPressed,
                          particles, waves, floatingTexts);
            monster.update(dt, player, atmosphere, particles, waves, spikes, meteors,
                           floatingTexts, screenShake, hitstopTimer);
            atmosphere.update(dt, monster.isPhase2());
            town.update(dt);

            // Player hits Monster
            if (player.canDealHit()) {
                float dist = (monster.center().x - player.center().x) * player.getFacing();
                float vertDist = std::abs(player.center().y - monster.center().y);
                if (dist > -30.f && dist < player.getAttackReach() && vertDist < 120.f) {
                    float dmg = player.getAttackDamage();
                    if (monster.isStaggered()) dmg *= 1.6f;
                    player.markHitRegistered();
                    monster.takeDamage(dmg, player, particles, waves, floatingTexts,
                                       screenShake, hitstopTimer);
                }
            }

            // Ground Spikes hit Player
            for (auto& spike : spikes) {
                spike.update(dt);
                if (spike.isDangerous() && !spike.didHit) {
                    float dist = std::abs(player.center().x - spike.x);
                    if (dist < 38.f) {
                        spike.didHit = true;
                        if (player.isParrying()) {
                            player.onPerfectParry(particles, waves);
                            floatingTexts.push_back({player.center() + sf::Vector2f(0.f, -65.f),
                                {0.f, -55.f}, "PARRIED!", color(120, 245, 255), 1.0f, 1.0f, 18});
                        } else {
                            float dmg = monster.isPhase2() ? 22.f : 16.f;
                            if (player.takeDamage(dmg, spike.x, particles)) {
                                screenShake = std::max(screenShake, 0.18f);
                                std::ostringstream oss;
                                oss << "-" << static_cast<int>(dmg);
                                floatingTexts.push_back({player.center() + sf::Vector2f(0.f, -45.f),
                                    {0.f, -50.f}, oss.str(), color(255, 75, 75), 0.9f, 0.9f, 18});
                            }
                        }
                    }
                }
            }

            // Meteors update & hit check
            for (auto& m : meteors) {
                m.update(dt, particles, waves);
                if (!m.exploded) {
                    float dist = std::sqrt(
                        (player.center().x - m.pos.x) * (player.center().x - m.pos.x) +
                        (player.center().y - m.pos.y) * (player.center().y - m.pos.y));
                    if (dist < m.radius + 28.f) {
                        m.exploded = true;
                        if (player.isParrying()) {
                            player.onPerfectParry(particles, waves);
                        } else {
                            float dmg = monster.isPhase2() ? 24.f : 18.f;
                            player.takeDamage(dmg, m.pos.x, particles);
                            screenShake = std::max(screenShake, 0.22f);
                        }
                    }
                }
            }

            // Check win/loss
            if (monster.isDead()) {
                gameState = GameState::Victory;
                endTimer = 0.f;
            }
            if (player.getHealth() <= 0.f) {
                gameState = GameState::Defeat;
                endTimer = 0.f;
            }
        } else {
            endTimer += dt;
            atmosphere.update(dt, monster.isPhase2());
        }

        // Update VFX
        for (auto& p : particles) p.update(dt);
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return p.dead(); }), particles.end());

        for (auto& w : waves) w.update(dt);
        waves.erase(std::remove_if(waves.begin(), waves.end(),
            [](const Shockwave& w) { return w.dead(); }), waves.end());

        for (auto& t : floatingTexts) t.update(dt);
        floatingTexts.erase(std::remove_if(floatingTexts.begin(), floatingTexts.end(),
            [](const FloatingText& t) { return t.dead(); }), floatingTexts.end());

        spikes.erase(std::remove_if(spikes.begin(), spikes.end(),
            [](const GroundSpike& s) { return s.dead(); }), spikes.end());

        meteors.erase(std::remove_if(meteors.begin(), meteors.end(),
            [](const Meteor& m) { return m.exploded; }), meteors.end());

        screenShake = std::max(0.f, screenShake - dt * 2.8f);

        // — Render —
        sf::Vector2f shakeOff{0.f, 0.f};
        if (screenShake > 0.01f) {
            shakeOff = {randf(-screenShake * 14.f, screenShake * 14.f),
                        randf(-screenShake * 10.f, screenShake * 10.f)};
        }

        sf::View view = window.getDefaultView();
        view.setCenter(view.getCenter() + shakeOff);
        window.setView(view);

        atmosphere.drawSky(window, monster.isPhase2());
        town.draw(window);

        // Draw ground spikes (behind characters)
        for (const auto& spike : spikes) spike.draw(window);

        // Draw meteors
        for (const auto& m : meteors) m.draw(window);

        // Draw characters
        monster.draw(window);
        player.draw(window);

        // Draw particles & shockwaves
        for (const auto& p : particles) p.draw(window);
        for (const auto& w : waves) w.draw(window);
        atmosphere.drawMotes(window);

        // Reset view for HUD
        window.setView(window.getDefaultView());

        // ---- HUD ----
        if (fontLoaded) {
            // Player Health Bar
            float barW = 260.f, barH = 18.f;
            float hpX = 32.f, hpY = 24.f;
            rectangle(window, {hpX - 2, hpY - 2}, {barW + 4, barH + 4}, color(12, 14, 22));
            rectangle(window, {hpX, hpY}, {barW, barH}, color(32, 28, 38));
            float delayRatio = std::clamp(player.getDelayedHealth() / 100.f, 0.f, 1.f);
            rectangle(window, {hpX, hpY}, {barW * delayRatio, barH}, color(180, 55, 55, 140));
            float hpRatio = std::clamp(player.getHealth() / 100.f, 0.f, 1.f);
            rectangle(window, {hpX, hpY}, {barW * hpRatio, barH}, color(195, 55, 55));

            sf::Text hpLabel(font, "HP", 13);
            hpLabel.setPosition({hpX + 4, hpY - 1});
            hpLabel.setFillColor(color(230, 220, 215));
            window.draw(hpLabel);

            // Stamina Bar
            float stY = hpY + barH + 6.f;
            float stW = 195.f, stH = 10.f;
            rectangle(window, {hpX - 1, stY - 1}, {stW + 2, stH + 2}, color(12, 14, 22));
            rectangle(window, {hpX, stY}, {stW, stH}, color(28, 32, 38));
            float stRatio = std::clamp(player.getStamina() / 100.f, 0.f, 1.f);
            rectangle(window, {hpX, stY}, {stW * stRatio, stH}, color(55, 165, 110));

            // Flask indicators
            float flaskY = stY + stH + 8.f;
            for (int i = 0; i < 3; ++i) {
                sf::Color flaskCol = (i < player.getFlasks())
                    ? color(255, 185, 55) : color(55, 50, 60);
                circleCentered(window, {hpX + 10.f + i * 22.f, flaskY + 6.f}, 7.f, flaskCol);
            }

            // Boss Health Bar
            float bBarW = 420.f, bBarH = 22.f;
            float bX = (WINDOW_W - bBarW) / 2.f, bY = WINDOW_H - 52.f;
            rectangle(window, {bX - 2, bY - 2}, {bBarW + 4, bBarH + 4}, color(12, 14, 22));
            rectangle(window, {bX, bY}, {bBarW, bBarH}, color(32, 28, 38));
            float bDelayRatio = std::clamp(monster.getDelayedHealth() / monster.getMaxHealth(), 0.f, 1.f);
            sf::Color bossBarDelayCol = monster.isPhase2() ? color(180, 45, 75, 140) : color(180, 100, 45, 140);
            rectangle(window, {bX, bY}, {bBarW * bDelayRatio, bBarH}, bossBarDelayCol);
            float bHpRatio = std::clamp(monster.getHealth() / monster.getMaxHealth(), 0.f, 1.f);
            sf::Color bossBarCol = monster.isPhase2() ? color(215, 50, 85) : color(215, 120, 45);
            rectangle(window, {bX, bY}, {bBarW * bHpRatio, bBarH}, bossBarCol);

            sf::Text bossName(font, "THE ASHEN COLOSSUS", 14);
            bossName.setPosition({bX + bBarW / 2.f - bossName.getLocalBounds().size.x / 2.f, bY - 22.f});
            bossName.setFillColor(color(215, 200, 195));
            window.draw(bossName);

            // Floating Texts
            for (const auto& ft : floatingTexts) {
                float alpha = std::clamp(ft.life / ft.maxLife, 0.f, 1.f);
                sf::Text txt(font, ft.text, ft.size);
                txt.setPosition(ft.pos);
                sf::Color c = ft.tint;
                c.a = static_cast<std::uint8_t>(static_cast<float>(c.a) * alpha);
                txt.setFillColor(c);
                window.draw(txt);
            }

            // Controls hint (fades after a few seconds)
            float hintAlpha = std::clamp(3.f - endTimer, 0.f, 1.f);
            if (gameState == GameState::Playing && hintAlpha > 0.01f) {
                sf::Text hint(font, "A/D:Move  Space:Jump  Z:Attack  Shift:Dash  X:Parry  C:Heal", 12);
                hint.setPosition({WINDOW_W / 2.f - hint.getLocalBounds().size.x / 2.f, 8.f});
                hint.setFillColor(color(180, 175, 190, static_cast<int>(160 * hintAlpha)));
                window.draw(hint);
            }

            // End screens
            if (gameState == GameState::Victory) {
                float a = std::min(1.f, endTimer * 1.2f);
                rectangle(window, {0, 0}, {float(WINDOW_W), float(WINDOW_H)},
                          color(0, 0, 0, static_cast<int>(120 * a)));
                sf::Text victoryText(font, "VICTORY ACHIEVED", 42);
                victoryText.setPosition({WINDOW_W / 2.f - victoryText.getLocalBounds().size.x / 2.f,
                                         WINDOW_H / 2.f - 50.f});
                victoryText.setFillColor(color(255, 215, 95, static_cast<int>(255 * a)));
                window.draw(victoryText);

                sf::Text restartText(font, "Press R to play again", 18);
                restartText.setPosition({WINDOW_W / 2.f - restartText.getLocalBounds().size.x / 2.f,
                                          WINDOW_H / 2.f + 20.f});
                restartText.setFillColor(color(195, 190, 205, static_cast<int>(200 * a)));
                window.draw(restartText);
            }

            if (gameState == GameState::Defeat) {
                float a = std::min(1.f, endTimer * 0.9f);
                rectangle(window, {0, 0}, {float(WINDOW_W), float(WINDOW_H)},
                          color(0, 0, 0, static_cast<int>(160 * a)));
                sf::Text defeatText(font, "YOU DIED", 48);
                defeatText.setPosition({WINDOW_W / 2.f - defeatText.getLocalBounds().size.x / 2.f,
                                         WINDOW_H / 2.f - 50.f});
                defeatText.setFillColor(color(215, 45, 45, static_cast<int>(255 * a)));
                window.draw(defeatText);

                sf::Text restartText(font, "Press R to try again", 18);
                restartText.setPosition({WINDOW_W / 2.f - restartText.getLocalBounds().size.x / 2.f,
                                          WINDOW_H / 2.f + 20.f});
                restartText.setFillColor(color(185, 175, 195, static_cast<int>(200 * a)));
                window.draw(restartText);
            }
        }

        window.display();
    }
    return 0;
}