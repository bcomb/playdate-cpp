#include "Physics.h"
#include "Globals.h"
#include "SimpleMath.h"
#include "SvgLoader.h"

#include <pd_api.h>
#include <assert.h>
#include <float.h>
#include <vector>



inline void drawSegment(float x0, float y0, float x1, float y1, float w = 1)
{
    _G.pd->graphics->drawLine(x0, y0, x1, y1, w, kColorBlack);
}

void drawSegment(const vec2& p0, const vec2& p1, float w = 1)
{
    _G.pd->graphics->drawLine(p0.x, p0.y, p1.x, p1.y, w, kColorBlack);
}

inline void drawCirle(float xc, float yc, float r, float w = 1)
{
    _G.pd->graphics->drawEllipse(xc - r, yc - r, r * 2, r * 2, w, 0, 0, kColorBlack);
}

inline void drawCross(float x, float y, float size=4, float w = 1)
{
    _G.pd->graphics->drawLine(x - size, y - size, x + size, y + size, w, kColorBlack);
    _G.pd->graphics->drawLine(x - size, y + size, x + size, y - size, w, kColorBlack);
}

inline void drawPolyline(vec2 p[], int count, float w, bool closed = false)
{
    for (int i = 0; i < count - 1; ++i)
    {
        _G.pd->graphics->drawLine(p[i].x, p[i].y, p[i + 1].x, p[i + 1].y, w, kColorBlack);
    }

    if(closed && count > 2)
    {
        _G.pd->graphics->drawLine(p[count - 1].x, p[count - 1].y, p[0].x, p[0].y, w, kColorBlack);
    }
}

inline void drawArrow(float x0, float y0, float x1, float y1, float w = 1)
{
    _G.pd->graphics->drawLine(x0, y0, x1, y1, w, kColorBlack);
    float angle = atan2f(y1 - y0, x1 - x0);
    float arrowSize = 6.0f;
    float angle1 = angle + 3.14159f * 3.0f / 4.0f;
    float angle2 = angle - 3.14159f * 3.0f / 4.0f;
    _G.pd->graphics->drawLine(x1, y1,
        x1 + cosf(angle1) * arrowSize,
        y1 + sinf(angle1) * arrowSize,
        w, kColorBlack);
    _G.pd->graphics->drawLine(x1, y1,
        x1 + cosf(angle2) * arrowSize,
        y1 + sinf(angle2) * arrowSize,
        w, kColorBlack);
}

inline void drawArrow(const vec2& p0, const vec2& p1, float w = 1)
{
    drawArrow(p0.x, p0.y, p1.x, p1.y, w);
}

inline void drawNormal(float px, float py, float nx, float ny, float length = 10.0f, float w = 1)
{
    drawArrow(px, py, px + nx * length, py + ny * length, w);
}

struct Segment
{
    vec2 p0;
    vec2 p1;
};

struct Circle
{
    vec2 center;
    float radius;
};

// Intersection cercle-segment
// Renvoie: 0, 1 ou 2 selon le nombre d'intersections qui tombent sur le segment [p0,p1].
// - center, radius: cercle
// - p0, p1: extrémités du segment
// - outI0, outI1: points d'intersection (valables si le nombre retourné >= 1 / 2)
// - eps: tolérance numérique
static int intersectCircleSegment(const vec2& center,
                                  float radius,
                                  const vec2& p0,
                                  const vec2& p1,
                                  vec2& outI0,
                                  vec2& outI1)
{
    // Segment dégénéré: traiter comme un point
    vec2 d = vec2(p1.x - p0.x, p1.y - p0.y);
    float a = d.x * d.x + d.y * d.y;
    if (a <= EPSILON) {
        // p0 == p1: vérifier si le point est sur le cercle
        vec2 v = vec2(p0.x - center.x, p0.y - center.y);
        float dist = sqrtf(v.x * v.x + v.y * v.y);
        if (fabsf(dist - radius) <= 1e-5f) {
            outI0 = p0;
            return 1;
        }
        return 0;
    }

    // Résoudre |p(t) - center|^2 = r^2 pour p(t) = p0 + t*d, t in [0,1]
    vec2 f = vec2(p0.x - center.x, p0.y - center.y);
    float b = 2.0f * (f.x * d.x + f.y * d.y);
    float c = (f.x * f.x + f.y * f.y) - radius * radius;

    float disc = b * b - 4.0f * a * c;
    if (disc < -EPSILON) {
        return 0; // pas d'intersection
    }

    // Racine numériquement ~0 => tangence
    if (fabsf(disc) <= EPSILON) {
        float t = -b / (2.0f * a);
        if (t >= -EPSILON && t <= 1.0f + EPSILON) {
            // Clamper pour éviter de sortir très légèrement de [0,1]
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            outI0 = vec2(p0.x + d.x * t, p0.y + d.y * t);
            return 1;
        }
        return 0;
    }

    // Deux intersections potentielles
    float sqrtDisc = sqrtf(disc);
    float inv2a = 0.5f / a;
    float t0 = (-b - sqrtDisc) * inv2a;
    float t1 = (-b + sqrtDisc) * inv2a;

    // Ordonner t0 <= t1
    if (t0 > t1) { float tmp = t0; t0 = t1; t1 = tmp; }

    int count = 0;
    if (t0 >= -EPSILON && t0 <= 1.0f + EPSILON) {
        float tc = t0;
        if (tc < 0.0f) tc = 0.0f;
        if (tc > 1.0f) tc = 1.0f;
        outI0 = vec2(p0.x + d.x * tc, p0.y + d.y * tc);
        count = 1;
    }
    if (t1 >= -EPSILON && t1 <= 1.0f + EPSILON) {
        float tc = t1;
        if (tc < 0.0f) tc = 0.0f;
        if (tc > 1.0f) tc = 1.0f;
        vec2 p = vec2(p0.x + d.x * tc, p0.y + d.y * tc);

        if (count == 0) {
            outI0 = p;
            count = 1;
        } else {
            // Éviter de compter deux fois la même intersection (tangent double)
            float dx = p.x - outI0.x;
            float dy = p.y - outI0.y;
            if ((dx * dx + dy * dy) > (EPSILON * EPSILON)) {
                outI1 = p;
                count = 2;
            }
        }
    }

    return count;
}



// CCD: cercle en mouvement de c0 -> c1 (sur t in [0,1]) contre segment statique [s0,s1].
// Retourne true si un impact se produit, avec:
//  - outT: temps d'impact (0..1)
//  - outPoint: point de contact sur le segment (ou l'extrémité pour un cap)
//  - outNormal: normale au contact (orientée du segment/end vers le centre du cercle)
static bool sweepCircleAgainstSegment(const vec2& c0,
    const vec2& c1,
    float radius,
    const vec2& s0,
    const vec2& s1,
    float& outT,
    vec2& outPoint,
    vec2& outNormal)
{
    vec2 v = vec2(c1.x - c0.x, c1.y - c0.y); // déplacement du centre

    bool hit = false;
    float bestT = 1e30f;
    vec2 bestPoint = vec2(0.0f, 0.0f);
    vec2 bestNormal = vec2(0.0f, 0.0f);

    // 1) Impact sur la partie intérieure du segment (corps du segment)
    vec2 s = vec2(s1.x - s0.x, s1.y - s0.y);
    float L = length(s);
    if (L > EPSILON) {
        vec2 u = vec2(s.x / L, s.y / L);           // direction du segment
        vec2 n = vec2(-u.y, u.x);                   // normale perpendiculaire

        // f(t) = dot((c0 + v t) - s0, n) = ± radius
        float f0 = dot(vec2(c0.x - s0.x, c0.y - s0.y), n);
        float fn = dot(v, n);

        if (fabsf(fn) > EPSILON) {
            // Deux candidats sur la droite infinie: f(t) = +r et f(t) = -r
            float rhs[2] = { +radius, -radius };
            for (int k = 0; k < 2; ++k) {
                float t = (rhs[k] - f0) / fn;
                if (t >= -EPSILON && t <= 1.0f + EPSILON) {
                    if (t < 0.0f) t = 0.0f;
                    if (t > 1.0f) t = 1.0f;
                    // Vérifier que la projection tombe sur le segment (lambda in [0,L])
                    vec2 ct = vec2(c0.x + v.x * t, c0.y + v.y * t);
                    float lambda = dot(vec2(ct.x - s0.x, ct.y - s0.y), u);
                    if (lambda >= -EPSILON && lambda <= L + EPSILON) {
                        if (lambda < 0.0f) lambda = 0.0f;
                        if (lambda > L) lambda = L;
                        vec2 q = vec2(s0.x + u.x * lambda, s0.y + u.y * lambda); // point sur segment

                        // normale orientée du segment vers le centre au moment du contact
                        vec2 nSign = (rhs[k] > 0.0f) ? n : vec2(-n.x, -n.y);

                        if (t < bestT) {
                            bestT = t; bestPoint = q; bestNormal = nSign; hit = true;
                        }
                    }
                }
            }
        }
        else {
            // Déplacement parallèle à la normale => distance perpendiculaire constante.
            // Si déjà en contact (|f0| == r) et projection dans le segment, on peut prendre t=0.
            if (fabsf(f0) <= radius + EPSILON) {
                float lambda0 = dot(vec2(c0.x - s0.x, c0.y - s0.y), u);
                if (lambda0 >= -EPSILON && lambda0 <= L + EPSILON) {
                    float t = 0.0f;
                    vec2 q = vec2(s0.x + u.x * clamp(lambda0, 0.0f, L), s0.y + u.y * clamp(lambda0, 0.0f, L));
                    vec2 nSign = (f0 >= 0.0f) ? n : vec2(-n.x, -n.y);
                    if (t < bestT) { bestT = t; bestPoint = q; bestNormal = nSign; hit = true; }
                }
            }
        }
    }

    // 2) Impact sur le cap proche de s0 (cercle centre s0, rayon r)
    {
        vec2 m = vec2(c0.x - s0.x, c0.y - s0.y);
        float A = dot(v, v);
        float B = 2.0f * dot(m, v);
        float C = dot(m, m) - radius * radius;
        if (A <= EPSILON) {
            // Presque immobile: traiter comme une intersection statique
            if (C <= EPSILON) {
                // déjà en contact; t=0
                float t = 0.0f;
                if (t < bestT) {
                    bestT = t; bestPoint = s0; bestNormal = normalizeSafe(vec2(c0.x - s0.x, c0.y - s0.y)); hit = true;
                }
            }
        }
        else {
            float D = B * B - 4.0f * A * C;
            if (D >= -EPSILON) {
                if (D < 0.0f) D = 0.0f; // tangence numérique
                float sqrtD = sqrtf(D);
                float inv2A = 0.5f / A;
                float t0 = (-B - sqrtD) * inv2A;
                float t1 = (-B + sqrtD) * inv2A;
                if (t0 >= -EPSILON && t0 <= 1.0f + EPSILON) {
                    if (t0 < 0.0f) t0 = 0.0f; if (t0 > 1.0f) t0 = 1.0f;
                    if (t0 < bestT) {
                        vec2 ct = vec2(c0.x + v.x * t0, c0.y + v.y * t0);
                        bestT = t0; bestPoint = s0; bestNormal = normalizeSafe(vec2(ct.x - s0.x, ct.y - s0.y)); hit = true;
                    }
                }
                if (t1 >= -EPSILON && t1 <= 1.0f + EPSILON) {
                    if (t1 < 0.0f) t1 = 0.0f; if (t1 > 1.0f) t1 = 1.0f;
                    if (t1 < bestT) {
                        vec2 ct = vec2(c0.x + v.x * t1, c0.y + v.y * t1);
                        bestT = t1; bestPoint = s0; bestNormal = normalizeSafe(vec2(ct.x - s0.x, ct.y - s0.y)); hit = true;
                    }
                }
            }
        }
    }

    // 3) Impact sur le cap proche de s1 (cercle centre s1, rayon r)
    {
        vec2 m = vec2(c0.x - s1.x, c0.y - s1.y);
        float A = dot(v, v);
        float B = 2.0f * dot(m, v);
        float C = dot(m, m) - radius * radius;
        if (A <= EPSILON) {
            if (C <= EPSILON) {
                float t = 0.0f;
                if (t < bestT) {
                    bestT = t; bestPoint = s1; bestNormal = normalizeSafe(vec2(c0.x - s1.x, c0.y - s1.y)); hit = true;
                }
            }
        }
        else {
            float D = B * B - 4.0f * A * C;
            if (D >= -EPSILON) {
                if (D < 0.0f) D = 0.0f;
                float sqrtD = sqrtf(D);
                float inv2A = 0.5f / A;
                float t0 = (-B - sqrtD) * inv2A;
                float t1 = (-B + sqrtD) * inv2A;
                if (t0 >= -EPSILON && t0 <= 1.0f + EPSILON) {
                    if (t0 < 0.0f) t0 = 0.0f; if (t0 > 1.0f) t0 = 1.0f;
                    if (t0 < bestT) {
                        vec2 ct = vec2(c0.x + v.x * t0, c0.y + v.y * t0);
                        bestT = t0; bestPoint = s1; bestNormal = normalizeSafe(vec2(ct.x - s1.x, ct.y - s1.y)); hit = true;
                    }
                }
                if (t1 >= -EPSILON && t1 <= 1.0f + EPSILON) {
                    if (t1 < 0.0f) t1 = 0.0f; if (t1 > 1.0f) t1 = 1.0f;
                    if (t1 < bestT) {
                        vec2 ct = vec2(c0.x + v.x * t1, c0.y + v.y * t1);
                        bestT = t1; bestPoint = s1; bestNormal = normalizeSafe(vec2(ct.x - s1.x, ct.y - s1.y)); hit = true;
                    }
                }
            }
        }
    }

    if (hit) {
        outT = bestT;
        outPoint = bestPoint;
        outNormal = bestNormal;
        return true;
    }
    return false;
}

#define ARRAY_SIZE(_arr) (sizeof(_arr)/sizeof(_arr[0]))
struct Ship
{
    void update(float dt)
    {
        vec2 dir = { cosf(angle), sinf(angle) };
        vec2 acc = dir * thrust;

        vel = vel + acc * dt;

        // Medium resistance approximation
        // --- Simple single-constant drag (quadratic) ---
        // F_drag = -k * |v| * v  -> a_drag = -k * |v| * v  (single constant k)
        vec2 dragAcc = { 0.0f, 0.0f };
        
        float dragCoefficient = 0.02f; // tune this to taste (0 = no drag)        
        float speed = length(vel);
        dragAcc = vel * -dragCoefficient * speed;
        

        vel = vel + (acc + dragAcc) * dt;
        pos = pos + vel * dt;
    }

    void draw(vec2 drawPos)
    {
        PlaydateAPI* pd = _G.pd;

        vec2 p[] = {
            {-8, -8},
            { 8, 0 },
            { -8,8 },
            { -4,0 } };
        int line_width = 1;

        for (int i = 0; i < ARRAY_SIZE(p); ++i)
        {
            p[i] = rotate(p[i], vec2(0.0f, 0.0f), angle);
            p[i] += drawPos;
        }       

        for (int i = 0; i < ARRAY_SIZE(p); ++i)
        {
            int j = (i + 1) % ARRAY_SIZE(p);
            pd->graphics->drawLine(roundf(p[i].x), roundf(p[i].y), roundf(p[j].x), roundf(p[j].y), line_width, kColorBlack);
        }

        // Bound volume
        //drawCirle(roundf(drawPos.x), roundf(drawPos.y), 6, 1);

        // Test fill
        //int coords[ARRAY_SIZE(p)*2];
        //for(int i=0; i< ARRAY_SIZE(p); i++)
        //{
        //    coords[i*2] = (int)p[i].x;
        //    coords[i*2+1] = (int)p[i].y;
        //}
        //pd->graphics->fillPolygon(ARRAY_SIZE(p), coords, kColorBlack, kPolygonFillNonZero);
        

    }

    float angle = 0.0f;
    float thrust = 0.0f;

    vec2 pos = { 0.0f, 0.0f };
    vec2 vel = { 0.0f, 0.0f };
};

void testPicoSvg(float Time)
{
    PlaydateAPI* pd = _G.pd;

    static bool sIsInit = false;
    static std::vector<std::vector<vec2>> polygons;
    if (!sIsInit)
    {
        sIsInit = true;
        polygons = svgParsePath("level0.svg");
    }
    

    float speed = 10.0f;
    int offset = cosf(Time * speed) * 100.0f;
    //pd->graphics->setDrawOffset(offset, 0);
    pd->graphics->setLineCapStyle(kLineCapStyleRound);
    for (size_t p = 0; p < polygons.size(); ++p)
    {
        for (size_t i = 0; i < polygons[p].size() - 1; ++i)
        {
            const vec2& p0 = polygons[p][i] * 2.0;
            const vec2& p1 = polygons[p][i + 1] * 2.0;
            pd->graphics->drawLine(p0.x, p0.y, p1.x, p1.y, 3, kColorBlack);
        }
    }
}


Ship ship;

// Normalise un angle entre 0 et 360
float normalizeAngle(float angle) {
    angle = fmodf(angle, 360.0f);
    if (angle < 0)
        angle += 360.0f;
    return angle;
}


void debugOneCCD(float t)
{

    float radius = 6;
    vec2 c0{348.891815,164.384766};
    vec2 c1{ 348.554535,164.830841 };
    vec2 s0{ 347,116 };
    vec2 s1{355,175};

    drawCirle(c0.x, c0.y, 20.0f, 1);
    drawCirle(c1.x, c1.y, 20.0f, 1);
    //drawSegment(c0.x, c0.y, c1.x, c1.y, 1);
    drawArrow(c0.x, c0.y, c1.x, c1.y, 1);

    drawSegment(s0.x, s0.y, s1.x, s1.y, 2);

    float outT;
    vec2 contact;
    vec2 normal;
    bool intersect = sweepCircleAgainstSegment(c0, c1, radius, s0, s1, outT, contact, normal);
    if (intersect)
    {
        vec2 newCenter = c0 + (c1 - c0) * outT;
        drawCirle(newCenter.x, newCenter.y, radius);
        drawCross(contact.x, contact.y);

        drawNormal(contact.x, contact.y, normal.x, normal.y, 15.0f, 1);
    }
}

void testCircleCCD(float t)
{
    static float previousTime = t;
    static bool sFirst = true;


    if (sFirst)
    {
        sFirst = false;
    }
    float dt = t - previousTime;
    PlaydateAPI* pd = _G.pd;


    float radius = 20.0f;
    vec2 c0 = { 50.0f, 50.0f };
    vec2 c1 = { 350.0f, 200.0f };

    vec2 s0 = { 200.0f, 50.0f };
    vec2 s1 = { 200.0f, 150.0f };
    vec2 s2 = { 50.0f, 150.0f };

    vec2 sc = (s0 + s1) * 0.5f;
    s0 = rotate(s0, sc, t*0.5f);
    s1 = rotate(s1, sc, t*0.5f);

    drawCirle(c0.x, c0.y, 20.0f, 1);
    drawCirle(c1.x, c1.y, 20.0f, 1);
    //drawSegment(c0.x, c0.y, c1.x, c1.y, 1);
    drawArrow(c0.x, c0.y, c1.x, c1.y, 1);

    drawSegment(s0.x, s0.y, s1.x, s1.y, 2);
    drawSegment(s0.x, s0.y, s2.x, s2.y, 2);

    {
        float outT;
        vec2 contact;
        vec2 normal;
        bool intersect = sweepCircleAgainstSegment(c0, c1, radius, s0, s1, outT, contact, normal);
        if (intersect)
        {
            vec2 newCenter = c0 + (c1 - c0) * outT;
            drawCirle(newCenter.x, newCenter.y, radius);
            drawCross(contact.x, contact.y);

            drawNormal(contact.x, contact.y, normal.x, normal.y, 15.0f, 1);
        }
    }

    {
        float outT;
        vec2 contact;
        vec2 normal;
        bool intersect = sweepCircleAgainstSegment(c0, c1, radius, s0, s2, outT, contact, normal);
        if (intersect)
        {
            vec2 newCenter = c0 + (c1 - c0) * outT;
            drawCirle(newCenter.x, newCenter.y, radius);
            drawCross(contact.x, contact.y);

            drawNormal(contact.x, contact.y, normal.x, normal.y, 15.0f, 1);
        }
    }
}

// Sphere-segment contact info
struct Contact
{
    vec2 p; // contact position
    vec2 n; // normal
    vec2 newPos; // new sphere position after contact

    // debug
    vec2 s0, s1;
};


static std::vector<Contact> sContacts;

void testCircleCCD2(float t)
{
    static float previousTime = t;
    static bool sFirst = true;
    static bool sIsInit = false;
    static std::vector<std::vector<vec2>> polygons;
    static vec2* polyline = nullptr;
    static int polylineCount = 0;
    if (!sIsInit)
    {
        sIsInit = true;
        polygons = svgParsePath("debug_curve.svg");
        assert(polygons.size() && polygons[0].size() >= 2);
        polylineCount = (int)polygons[0].size();
        polyline = polygons[0].data();
    }

    float dt = t - previousTime;
    PlaydateAPI* pd = _G.pd;


    float radius = 16.0f;
    vec2 c0 = { 200.0f, 120.0f };
    vec2 c1 = { 400.0f, 120.0f };
    c1 = rotate(c1, c0, t * 0.5f);


    // Draw debug circle movement
    drawCirle(c0.x, c0.y, radius, 1);
    drawCirle(c1.x, c1.y, radius, 1);
    drawArrow(c0.x, c0.y, c1.x, c1.y, 1);


    // Draw polyline
    drawPolyline(polyline, polylineCount, 3, false);


    // Brute-force test against all segments
    sContacts.clear();
    for(int i=0; i< polylineCount-1; ++i)
    {
        vec2 s0 = polyline[i];
        vec2 s1 = polyline[i + 1];
        float outT;
        vec2 contact;
        vec2 normal;
        bool intersect = sweepCircleAgainstSegment(c0, c1, radius, s0, s1, outT, contact, normal);
        if (intersect)
        {
            
            vec2 newCenter = c0 + (c1 - c0) * outT;

            sContacts.push_back({ {contact.x, contact.y}, {normal.x, normal.y}, newCenter } );
            /*drawCirle(newCenter.x, newCenter.y, radius);
            drawCross(contact.x, contact.y);
            drawNormal(contact.x, contact.y, normal.x, normal.y, 15.0f, 1);*/
        }
    }

    // search the nearest new position
    if (sContacts.size())
    {
        float bestDist2 = FLT_MAX;
        Contact* bestContact = nullptr;
        for (size_t i = 0; i < sContacts.size(); ++i)
        {
            vec2 d = vec2(sContacts[i].newPos.x - c0.x, sContacts[i].newPos.y - c0.y);
            float dist2 = dot(d, d);
            if (dist2 < bestDist2)
            {
                bestDist2 = dist2;
                bestContact = &sContacts[i];
            }
        }
        // Draw best contact
        if (bestContact)
        {
            drawCirle(bestContact->newPos.x, bestContact->newPos.y, radius);
            drawCross(bestContact->p.x, bestContact->p.y);
            drawNormal(bestContact->p.x, bestContact->p.y, bestContact->n.x, bestContact->n.y, 15.0f, 1);
        }
    }

    /*
    float outT;
    vec2 contact;
    vec2 normal;
    bool intersect = sweepCircleAgainstSegment(c0, c1, radius, s0, s1, outT, contact, normal);
    if (intersect)
    {
        vec2 newCenter = c0 + (c1 - c0) * outT;
        drawCirle(newCenter.x, newCenter.y, radius);
        drawCross(contact.x, contact.y);

        drawNormal(contact.x, contact.y, normal.x, normal.y, 15.0f, 1);
    }
    */
 
}

void test(float t)
{

    //debugOneCCD(t);
    //testCircleCCD(t);
    //testCircleCCD2(t);
    //return;

    static float previousTime = t;
    static bool sFirst = true;
    static std::vector<std::vector<vec2>> polygons;        
    if (sFirst)
    {
        sFirst = false;

        const float scaleWorld = 4.0;

        ship.pos = { 164, 33 } ;
        ship.pos = ship.pos * scaleWorld; // scale up
        
        ship.thrust = 40.0f;

        polygons = svgParsePath("level0.svg");

        for (int i = 0; i < polygons.size(); ++i)
        {
            for (int j = 0; j < polygons[i].size(); ++j)
            {
                polygons[i][j] = polygons[i][j] * scaleWorld; // scale up
            }
        }
    }

    float dt = t - previousTime;
    PlaydateAPI* pd = _G.pd;        


    Ship previousShip = ship;

    PDButtons current, pushed, released;
    pd->system->getButtonState(&current, &pushed, &released);
    if (pushed & kButtonUp)
    {
        ship.thrust *= 2.0f;
        if (ship.thrust == 0.0f)
        {
            ship.thrust = 1.0f;
        }
    }

    if (pushed & kButtonDown)
    {
        ship.thrust *= 0.5f;
        if(ship.thrust <= 1.0f)
        {
            ship.thrust = 0.0f;
        }
    }

    ship.thrust = clamp(ship.thrust, 0.0f, 10000.0f);

    pd->graphics->setDrawOffset(0, 0);
   
    // Angle are between 0 and 360, and i wan't to reach by the shorstest arc the target angle
    const float maxAngleSpeed = 360.0f; // degrees per second
    float targetAngle = pd->system->getCrankAngle();
    float currentAngle = normalizeAngle(degrees(ship.angle));
    float angleDiff = targetAngle - currentAngle;
    if (angleDiff > 180.0f) angleDiff -= 360.0f;
    else if (angleDiff < -180.0f) angleDiff += 360.0f;
    float angleInput = angleDiff;
    ship.angle += radians(clamp(angleInput, -maxAngleSpeed * dt, maxAngleSpeed * dt));

    char tmp[32] = { '\0' };
    sprintf(tmp, "%.1f thrust=%.1f", targetAngle, ship.thrust);
    pd->graphics->drawText(tmp, strlen(tmp), kASCIIEncoding, 0, 0);


    pd->graphics->setDrawOffset(roundf(-ship.pos.x + 200), roundf(-ship.pos.y + 120));
    
    //ship.angle += radians(clamp(angleInput, -maxAngleSpeed * dt, maxAngleSpeed * dt));
    //ship.angle = radians(pd->system->getCrankAngle());    
    ship.update(dt);


    const float shipRadius = 6.0f;

    // brute-force CCD against level geometry
    sContacts.clear();
    vec2 c0 = previousShip.pos;
    vec2 c1 = ship.pos;
    for (int j = 0; j < polygons.size(); ++j)
    {
        for (int i = 0; i < polygons[j].size() - 1; ++i)
        {
            vec2 s0 = polygons[j][i];
            vec2 s1 = polygons[j][i+1];
            float outT;
            vec2 contact;
            vec2 normal;
            bool intersect = sweepCircleAgainstSegment(c0, c1, shipRadius, s0, s1, outT, contact, normal);
            if (intersect)
            {                

                vec2 newCenter = c0 + (c1 - c0) * outT;
                Contact ctt;
                ctt.newPos = newCenter;
                ctt.n = normal;
                ctt.p = contact;
                ctt.s0 = s0;
                ctt.s1 = s1;
                sContacts.push_back(ctt);
                //sContacts.push_back({ {contact.x, contact.y}, {normal.x, normal.y}, newCenter });


                /*
                drawArrow(c0, c0 + normalize(c1 - c0) * 64.0, 1);
                drawCirle(newCenter.x, newCenter.y, shipRadius);
                drawCross(contact.x, contact.y);
                drawNormal(contact.x, contact.y, normal.x, normal.y, 15.0f, 1);
                */
            }
        }
    }

    // search the nearest new position
    if (sContacts.size())
    {
        float bestDist2 = FLT_MAX;
        Contact* bestContact = nullptr;
        for (size_t i = 0; i < sContacts.size(); ++i)
        {
            vec2 d = vec2(sContacts[i].newPos.x - c0.x, sContacts[i].newPos.y - c0.y);
            float dist2 = dot(d, d);
            if (dist2 < bestDist2)
            {
                bestDist2 = dist2;
                bestContact = &sContacts[i];
            }
        }
        // Draw best contact
        if (bestContact)
        {
            /*
            drawCirle(bestContact->newPos.x, bestContact->newPos.y, shipRadius);
            drawCross(bestContact->p.x, bestContact->p.y);
            drawNormal(bestContact->p.x, bestContact->p.y, bestContact->n.x, bestContact->n.y, 15.0f, 1);
            */

            ship.pos = bestContact->newPos + bestContact->n * 0.5f; // Push it a bit farther to avoid constant contact
            //ship.pos = bestContact->newPos;
            const float restitution = 0.8f;
            //ship.vel = reflect(ship.vel, bestContact->n) * restitution; // simple bounce with energy loss
            //ship.vel -= bestContact->n * dot(ship.vel, bestContact->n); // glisse
        }
    }

  
    for (int i = 0; i < polygons.size(); ++i)
    {
        drawPolyline(polygons[i].data(), (int)polygons[i].size(), 3, false);
    }

    pd->graphics->drawRect(0, 0, 400, 240, kColorBlack);
    ship.draw(ship.pos);



    previousTime = t;
}

