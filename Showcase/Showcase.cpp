// Showcase.cpp - A guided tour of the Particle System API
//
// Copyright 2026 by the Particle System API contributors
//
// Each scene demonstrates a handful of actions, domains, and emission features, and lists them on screen
// so you can see exactly what is being shown. Together the scenes cover every action, every domain, and
// every way of creating particles. Press C in the program to see the full coverage table, and H for help.
//
// The simulation code for each scene is in its Update() function. Read it alongside the running scene:
// the on-screen lists name exactly the API calls used there.
//
// Run "Showcase --selftest" to simulate every scene without a window and print particle statistics.
// "Showcase --selftest 7" also prints scene 7's statistics once per simulated second.
// "Showcase --shots <dir>" runs each scene for 4 seconds and saves a screenshot of it to <dir>/sceneNN.bmp.

#include "Particle/pAPI.h"
using namespace PAPI;

// OpenGL
#include "GL/glew.h"

// This needs to come after GLEW
#include "GL/freeglut.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <execution>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

ParticleContext_t P;

//////////////////////////////////////////////////////////////////
// The feature catalog: everything the showcase is meant to cover

enum Category_e { CAT_ACTION, CAT_DOMAIN, CAT_API, CAT_COUNT };
const char* CategoryNames[CAT_COUNT] = {"Actions", "Domains", "Emission & API"};

const std::vector<std::string> AllFeatures[CAT_COUNT] = {
    // Actions (inline and legacy)
    {"Avoid",          "Bounce",           "Callback",        "CommitKills",       "CopyVertexB",   "Damping",       "DensityColor",
     "Explosion",      "Follow",           "Gravitate",       "Gravity",           "Jet",           "KillOld",       "MatchRotVelocity",
     "MatchVelocity",  "Move",             "OrbitLine",       "OrbitPoint",        "PositionColor", "RandomAccel",   "RandomColor",
     "RandomDisplace", "RandomRotVelocity", "RandomVelocity", "ResolveCollisions", "Restore",       "RotDamping",    "Sink",
     "SinkVelocity",   "Sort",             "SpeedClamp",      "SPH",               "TargetColor",   "TargetRotVelocity",
     "TargetSize",     "TargetVelocity",   "VelocityColor",   "VelocitySize",      "Vortex",        "VoxelField",    "Waypoints"},
    // Domains
    {"PDBlob", "PDBox", "PDCone", "PDCylinder", "PDDisc", "PDLine", "PDPlane", "PDPoint", "PDRectangle", "PDSphere", "PDTriangle", "PDUnion"},
    // Emission, source state, and other API
    {"Source", "Vertex", "Vertex(data)", "S.Velocity", "S.Color", "S.Color(+alpha)", "S.Size", "S.Mass", "S.StartingAge", "S.UpVec", "S.RotVelocity",
     "S.VertexB", "S.VertexBTracks", "S.Data", "Action lists", "CopyGroup", "CurrentGroup", "SetMaxParticles", "BirthCallback", "DeathCallback",
     "TimeStep substeps", "Seed", "GetParticlePointer", "GetParticles", "ParticleLoop(par_unseq)", "ParticleLoop(seq)", "Legacy actions"},
};

//////////////////////////////////////////////////////////////////
// Small helpers

const pVec ZAxis(0.f, 0.f, 1.f);
inline pVec V(float x, float y, float z) { return pVec(x, y, z); }
inline pVec V(float a) { return pVec(a, a, a); }
inline float Clamp01(float t) { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }

// Two unit vectors perpendicular to n and to each other
void Basis(const pVec& nIn, pVec& u, pVec& v)
{
    pVec n = nIn;
    n.normalize();
    pVec a = fabsf(n.x()) < 0.9f ? V(1, 0, 0) : V(0, 1, 0);
    u = Cross(n, a);
    u.normalize();
    v = Cross(n, u);
}

//////////////////////////////////////////////////////////////////
// Camera

struct Camera {
    pVec target = V(0, 0, 2);
    float yaw = 0.f;    // Degrees around Z; 0 looks along +Y
    float pitch = 20.f; // Degrees above the horizon
    float dist = 25.f;

    pVec Eye() const
    {
        float y = yaw * float(M_PI) / 180.f, p = pitch * float(M_PI) / 180.f;
        return target + V(sinf(y) * cosf(p), -cosf(y) * cosf(p), sinf(p)) * dist;
    }
};
Camera Cam;

//////////////////////////////////////////////////////////////////
// Rendering helpers. These read particle data directly from API memory with GetParticlePointer().

struct PView {
    const float* ptr = nullptr;
    size_t n = 0, stride = 0, pos, posB, size, vel, velB, color, alpha, age, up, rvel, upB, mass, data;

    bool Get()
    {
        if (P.GetGroupCount() < 1) return false; // GetParticlePointer throws on an empty group
        n = P.GetParticlePointer(ptr, stride, pos, posB, size, vel, velB, color, alpha, age, up, rvel, upB, mass, data);
        return n > 0;
    }
    const float* At(size_t i, size_t ofs) const { return ptr + i * stride + ofs; }
    pVec Vec(size_t i, size_t ofs) const { return pVec(const_cast<float*>(At(i, ofs))); }
    float F(size_t i, size_t ofs) const { return *At(i, ofs); }
};

GLuint SpriteTex = 0;

void MakeSpriteTexture()
{
    const int N = 32;
    std::vector<unsigned char> img(N * N * 2);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float dx = (x + 0.5f) / N * 2.f - 1.f, dy = (y + 0.5f) / N * 2.f - 1.f;
            float a = expf(-3.f * (dx * dx + dy * dy)) * Clamp01((1.f - sqrtf(dx * dx + dy * dy)) * 4.f);
            img[(y * N + x) * 2 + 0] = 255;
            img[(y * N + x) * 2 + 1] = (unsigned char)(a * 255.f);
        }
    glGenTextures(1, &SpriteTex);
    glBindTexture(GL_TEXTURE_2D, SpriteTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, N, N, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, img.data());
}

// Points with per-particle color and alpha, using vertex arrays straight from API memory
void DrawPoints(float pointSize)
{
    PView v;
    if (!v.Get()) return;
    GLsizei s = GLsizei(v.stride * sizeof(float));
    glPointSize(pointSize);
    glEnable(GL_POINT_SMOOTH);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, s, v.ptr + v.pos);
    glColorPointer(4, GL_FLOAT, s, v.ptr + v.color); // Color is followed by alpha in Particle_t
    glDrawArrays(GL_POINTS, 0, (GLsizei)v.n);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

// Soft camera-facing sprites whose radius is the particle's size.x times scale
void DrawSprites(float scale, bool depthWrite = false)
{
    PView v;
    if (!v.Get()) return;
    GLfloat m[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    pVec r(m[0], m[4], m[8]), u(m[1], m[5], m[9]);

    glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, SpriteTex);
    glBegin(GL_QUADS);
    for (size_t i = 0; i < v.n; ++i) {
        pVec p = v.Vec(i, v.pos);
        float s = v.F(i, v.size) * scale;
        glColor4fv(v.At(i, v.color));
        pVec a = p - (r + u) * s, b = p + (r - u) * s, c = p + (r + u) * s, d = p - (r - u) * s;
        glTexCoord2f(0, 0), glVertex3f(a.x(), a.y(), a.z());
        glTexCoord2f(1, 0), glVertex3f(b.x(), b.y(), b.z());
        glTexCoord2f(1, 1), glVertex3f(c.x(), c.y(), c.z());
        glTexCoord2f(0, 1), glVertex3f(d.x(), d.y(), d.z());
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
}

// A line from each particle along its up vector, to show orientation and rotation
void DrawUpSticks(float len)
{
    PView v;
    if (!v.Get()) return;
    glLineWidth(2.f);
    glBegin(GL_LINES);
    for (size_t i = 0; i < v.n; ++i) {
        pVec p = v.Vec(i, v.pos), up = v.Vec(i, v.up);
        if (up.lenSqr() > 0.f) up.normalize(len);
        pVec e = p + up;
        glColor4fv(v.At(i, v.color));
        glVertex3f(p.x(), p.y(), p.z());
        glColor4f(1, 1, 1, 0.9f);
        glVertex3f(e.x(), e.y(), e.z());
    }
    glEnd();
    glLineWidth(1.f);
}

// A line from each particle's posB to its position, stretched by 'stretch' to make one frame of motion visible
void DrawStreaks(float stretch)
{
    PView v;
    if (!v.Get()) return;
    glLineWidth(2.f);
    glBegin(GL_LINES);
    for (size_t i = 0; i < v.n; ++i) {
        pVec p = v.Vec(i, v.pos), b = v.Vec(i, v.posB);
        pVec tail = p + (b - p) * stretch;
        const float* c = v.At(i, v.color);
        glColor4f(c[0], c[1], c[2], 0.f);
        glVertex3f(tail.x(), tail.y(), tail.z());
        glColor4f(c[0], c[1], c[2], c[3]);
        glVertex3f(p.x(), p.y(), p.z());
    }
    glEnd();
    glLineWidth(1.f);
}

// Lit spheres of radius size.x
void DrawBalls()
{
    PView v;
    if (!v.Get()) return;
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    GLfloat lp[4] = {0.4f, -0.6f, 1.f, 0.f};
    glLightfv(GL_LIGHT0, GL_POSITION, lp);
    for (size_t i = 0; i < v.n; ++i) {
        pVec p = v.Vec(i, v.pos);
        glColor4fv(v.At(i, v.color));
        glPushMatrix();
        glTranslatef(p.x(), p.y(), p.z());
        glutSolidSphere(v.F(i, v.size), 14, 10);
        glPopMatrix();
    }
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LIGHTING);
}

//////////////////////////////////////////////////////////////////
// Wireframe guides that show where each domain is

void GuideColor(float a = 0.55f) { glColor4f(1.f, 0.9f, 0.45f, a); }

void GCircle(const pVec& c, const pVec& n, float r, int segs = 48)
{
    pVec u, v;
    Basis(n, u, v);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segs; ++i) {
        float a = 2.f * float(M_PI) * i / segs;
        pVec p = c + u * (r * cosf(a)) + v * (r * sinf(a));
        glVertex3f(p.x(), p.y(), p.z());
    }
    glEnd();
}

void GLine(const pVec& a, const pVec& b)
{
    glBegin(GL_LINES);
    glVertex3f(a.x(), a.y(), a.z());
    glVertex3f(b.x(), b.y(), b.z());
    glEnd();
}

void GPoint(const pVec& p, float s = 0.2f)
{
    GLine(p - V(s, 0, 0), p + V(s, 0, 0));
    GLine(p - V(0, s, 0), p + V(0, s, 0));
    GLine(p - V(0, 0, s), p + V(0, 0, s));
}

void GSphere(const pVec& c, float r)
{
    GCircle(c, V(1, 0, 0), r);
    GCircle(c, V(0, 1, 0), r);
    GCircle(c, V(0, 0, 1), r);
}

void GDisc(const pVec& c, const pVec& n, float rOut, float rIn = 0.f)
{
    GCircle(c, n, rOut);
    if (rIn > 0.f) GCircle(c, n, rIn);
    pVec nn = n;
    nn.normalize(0.4f);
    GLine(c, c + nn); // Normal
}

void GRect(const pVec& p, const pVec& u, const pVec& v)
{
    glBegin(GL_LINE_LOOP);
    pVec q[4] = {p, p + u, p + u + v, p + v};
    for (auto& x : q) glVertex3f(x.x(), x.y(), x.z());
    glEnd();
}

void GTri(const pVec& a, const pVec& b, const pVec& c)
{
    glBegin(GL_LINE_LOOP);
    glVertex3f(a.x(), a.y(), a.z());
    glVertex3f(b.x(), b.y(), b.z());
    glVertex3f(c.x(), c.y(), c.z());
    glEnd();
}

void GBox(const pVec& lo, const pVec& hi)
{
    for (int i = 0; i < 4; ++i) {
        float x0 = (i & 1) ? hi.x() : lo.x(), y0 = (i & 2) ? hi.y() : lo.y();
        GLine(V(x0, y0, lo.z()), V(x0, y0, hi.z()));
    }
    GRect(lo, V(hi.x() - lo.x(), 0, 0), V(0, hi.y() - lo.y(), 0));
    GRect(V(lo.x(), lo.y(), hi.z()), V(hi.x() - lo.x(), 0, 0), V(0, hi.y() - lo.y(), 0));
}

void GPlane(const pVec& p, const pVec& n, float extent, int lines = 8)
{
    pVec u, v;
    Basis(n, u, v);
    for (int i = 0; i <= lines; ++i) {
        float t = -extent + 2.f * extent * i / lines;
        GLine(p + u * t - v * extent, p + u * t + v * extent);
        GLine(p + v * t - u * extent, p + v * t + u * extent);
    }
}

void GCylinder(const pVec& e0, const pVec& e1, float rOut, float rIn = 0.f)
{
    pVec ax = e1 - e0, u, v;
    Basis(ax, u, v);
    GCircle(e0, ax, rOut);
    GCircle(e1, ax, rOut);
    if (rIn > 0.f) {
        GCircle(e0, ax, rIn);
        GCircle(e1, ax, rIn);
    }
    for (int i = 0; i < 8; ++i) {
        float a = 2.f * float(M_PI) * i / 8;
        pVec o = u * (rOut * cosf(a)) + v * (rOut * sinf(a));
        GLine(e0 + o, e1 + o);
    }
}

void GCone(const pVec& apex, const pVec& base, float r)
{
    pVec ax = base - apex, u, v;
    Basis(ax, u, v);
    GCircle(base, ax, r);
    for (int i = 0; i < 8; ++i) {
        float a = 2.f * float(M_PI) * i / 8;
        GLine(apex, base + u * (r * cosf(a)) + v * (r * sinf(a)));
    }
}

void Label3D(const pVec& p, const std::string& s, float r = 1.f, float g = 1.f, float b = 1.f)
{
    glColor4f(r, g, b, 1.f);
    glRasterPos3f(p.x(), p.y(), p.z());
    glutBitmapString(GLUT_BITMAP_HELVETICA_12, (const unsigned char*)s.c_str());
}

void DrawGround(float extent, float z = 0.f)
{
    glColor4f(0.35f, 0.4f, 0.45f, 0.35f);
    GPlane(V(0, 0, z), ZAxis, extent, 16);
}

//////////////////////////////////////////////////////////////////
// Scene base class

const auto PAR = std::execution::par_unseq;
const auto SEQ = std::execution::seq;

struct Scene {
    std::string title, summary;
    std::vector<std::string> features[CAT_COUNT];
    Camera camera;
    std::vector<int> groups;
    std::vector<int> actionLists;
    float time = 0.f;

    virtual ~Scene() {}
    virtual void Start() = 0;              // Create groups and initial particles
    virtual void Update(float dt) = 0;     // Run the actions for one time step. dt has already been passed to P.TimeStep().
    virtual void Draw() = 0;               // Draw particles and domain guides
    virtual void Trigger() {}              // Space bar
    virtual std::string TriggerHelp() const { return ""; }
    virtual std::vector<std::string> Status() { return {}; }

    void Uses(Category_e c, std::initializer_list<const char*> names)
    {
        for (auto n : names) features[c].push_back(n);
    }

    // Make a particle group and make it current
    int NewGroup(size_t maxParticles)
    {
        int g = P.GenParticleGroups(1, maxParticles);
        groups.push_back(g);
        P.CurrentGroup(g);
        return g;
    }

    void Stop()
    {
        for (int g : groups) P.DeleteParticleGroups(g);
        for (int a : actionLists) P.DeleteActionLists(a);
        groups.clear();
        actionLists.clear();
    }

    size_t ParticleCount()
    {
        size_t n = 0;
        for (int g : groups) {
            P.CurrentGroup(g);
            n += P.GetGroupCount();
        }
        return n;
    }
};

//////////////////////////////////////////////////////////////////
// 1. Fountain: the basic workflow

struct FountainScene : Scene {
    PDLine Nozzle = PDLine(V(0, 0, 0.1f), V(0, 0, 0.4f));
    PDDisc Pool = PDDisc(V(0, 0, 0), ZAxis, 5.f);
    PDPlane KillPlane = PDPlane(V(0, 0, -4.f), ZAxis);
    int g = -1;

    FountainScene()
    {
        title = "Fountain - the basic workflow";
        summary = "Source() emits from a line with velocities from a cylinder; one inline ParticleLoop() applies all actions; CommitKills() removes the dead.";
        Uses(CAT_ACTION, {"Gravity", "Bounce", "Move", "Sink", "KillOld", "CommitKills"});
        Uses(CAT_DOMAIN, {"PDLine", "PDCylinder", "PDDisc", "PDPlane"});
        Uses(CAT_API, {"Source", "S.Velocity", "S.Color", "ParticleLoop(par_unseq)", "GetParticlePointer", "Seed"});
        camera.target = V(0, 0, 1.5f);
        camera.dist = 20.f;
    }

    void Start() override { g = NewGroup(30000); }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        pSourceState S;
        S.Velocity(PDCylinder(V(0, 0, 7.5f), V(0, 0, 8.2f), 0.7f, 0.5f)); // A thin-walled tube in *velocity* space: upward with a spread
        S.Color(PDLine(V(0.55f, 0.7f, 1.f), V(1, 1, 1)));                // Each particle gets a color on the line from blue to white
        P.Source(3000, Nozzle, S);                                        // 3000 particles per second

        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.Gravity(p, V(0, 0, -9.8f));
            P.Bounce(p, 0.1f, 0.4f, 0.1f, Pool); // Last velocity change before Move()
            P.Move(p);
            P.Sink(p, false, KillPlane); // Mark particles that fall below z = -4 (outside the plane's positive side)
            P.KillOld(p, 6.f);           // ...and any that come to rest in the pool
        });
        P.CommitKills();
    }

    void Draw() override
    {
        GuideColor();
        GLine(V(0, 0, 0.1f), V(0, 0, 0.4f));
        GDisc(V(0, 0, 0), ZAxis, 5.f);
        GPlane(V(0, 0, -4.f), ZAxis, 7.f);
        Label3D(V(0.3f, 0, 0.4f), "PDLine nozzle (Source position)");
        Label3D(V(5.2f, 0, 0), "PDDisc pool (Bounce)");
        Label3D(V(7.2f, 0, -4.f), "PDPlane z=-4 (Sink)");
        P.CurrentGroup(g);
        DrawPoints(3.f);
    }
};

//////////////////////////////////////////////////////////////////
// 2. Emission domains: Source() from every domain type

struct DomainGalleryScene : Scene {
    struct Emitter {
        std::shared_ptr<pDomain> dom;
        std::string name, hint;
        pVec center, color;
    };
    std::vector<Emitter> E;
    int g = -1;

    DomainGalleryScene()
    {
        title = "Emission domains - Source() from every domain";
        summary = "Each cluster is emitted from a different domain shape. Particles appear at random points in the domain, then fade with TargetColor() and die with KillOld().";
        Uses(CAT_ACTION, {"KillOld", "TargetColor", "Move", "CommitKills"});
        Uses(CAT_DOMAIN, {"PDPoint", "PDLine", "PDTriangle", "PDRectangle", "PDDisc", "PDPlane", "PDBox", "PDCylinder", "PDCone", "PDSphere", "PDBlob", "PDUnion"});
        Uses(CAT_API, {"Source", "S.Color"});
        camera.target = V(0, 0, 4.f);
        camera.dist = 24.f;
        camera.pitch = 8.f;
    }

    void Add(int idx, std::shared_ptr<pDomain> d, const char* name, const char* hint, pVec color)
    {
        E.push_back({d, name, hint, Cell(idx), color});
    }
    static pVec Cell(int i) { return V(-7.5f + 5.f * (i % 4), 0, 8.5f - 4.5f * (i / 4)); }

    void Start() override
    {
        g = NewGroup(60000);
        E.clear();
        int i = 0;
        pVec c;
        c = Cell(i);
        Add(i++, std::make_shared<PDPoint>(c), "PDPoint", "one point", V(1, 1, 1));
        c = Cell(i);
        Add(i++, std::make_shared<PDLine>(c - V(1.4f, 0, 0.8f), c + V(1.4f, 0, 0.8f)), "PDLine", "segment e0-e1", V(1, 0.5f, 0.5f));
        c = Cell(i);
        Add(i++, std::make_shared<PDTriangle>(c + V(-1.4f, 0, -1.1f), c + V(1.4f, 0, -1.1f), c + V(0, 0, 1.3f)), "PDTriangle", "filled triangle",
            V(1, 0.8f, 0.3f));
        c = Cell(i);
        Add(i++, std::make_shared<PDRectangle>(c + V(-1.4f, 0, -1.f), V(2.8f, 0, 0), V(0, 0.6f, 2.f)), "PDRectangle", "corner + two edges",
            V(0.6f, 1, 0.4f));
        c = Cell(i);
        Add(i++, std::make_shared<PDDisc>(c, V(0, -1, 0.4f), 1.4f, 0.8f), "PDDisc", "annulus: inner radius 0.8", V(0.3f, 0.9f, 1));
        c = Cell(i);
        Add(i++, std::make_shared<PDPlane>(c, V(0, -1, 0)), "PDPlane", "Generate() gives only its point", V(0.7f, 0.7f, 1));
        c = Cell(i);
        Add(i++, std::make_shared<PDBox>(c - V(1.1f, 0.6f, 1.1f), c + V(1.1f, 0.6f, 1.1f)), "PDBox", "solid box", V(1, 0.4f, 1));
        c = Cell(i);
        Add(i++, std::make_shared<PDCylinder>(c - V(0, 0, 1.2f), c + V(0, 0, 1.2f), 1.f, 0.8f), "PDCylinder", "tube: inner radius 0.8",
            V(1, 0.65f, 0.2f));
        c = Cell(i);
        Add(i++, std::make_shared<PDCone>(c + V(0, 0, 1.3f), c - V(0, 0, 1.2f), 1.2f), "PDCone", "apex e0, base e1", V(0.5f, 1, 0.8f));
        c = Cell(i);
        Add(i++, std::make_shared<PDSphere>(c, 1.3f, 1.15f), "PDSphere", "thin shell: radii 1.3/1.15", V(0.4f, 0.6f, 1));
        c = Cell(i);
        Add(i++, std::make_shared<PDBlob>(c, 0.45f), "PDBlob", "Gaussian, std dev 0.45", V(1, 1, 0.5f));
        c = Cell(i);
        Add(i++,
            std::make_shared<PDUnion>(PDSphere(c - V(0.9f, 0, 0.5f), 0.5f), PDLine(c + V(0.2f, 0, -1.f), c + V(1.2f, 0, 1.f)),
                                      PDDisc(c + V(-0.6f, 0, 0.9f), V(0, -1, 0), 0.45f)),
            "PDUnion", "sphere + line + disc", V(1, 0.5f, 0.8f));
    }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        for (auto& e : E) {
            pSourceState S;
            S.Color(e.color);
            P.Source(1200, *e.dom, S);
        }
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.TargetColor(p, V(1, 1, 1), 0.f, 0.9f); // Fade toward transparent white
            P.Move(p);                              // No velocity, but Move() also ages the particles
            P.KillOld(p, 1.5f);
        });
        P.CommitKills();
    }

    void Draw() override
    {
        P.CurrentGroup(g);
        glEnable(GL_BLEND);
        DrawPoints(2.f);
        for (auto& e : E) {
            Label3D(e.center + V(-1.4f, -0.5f, -1.75f), e.name, 1.f, 0.9f, 0.45f);
            Label3D(e.center + V(-1.4f, -0.5f, -2.15f), e.hint, 0.75f, 0.75f, 0.75f);
        }
    }
};

//////////////////////////////////////////////////////////////////
// 3. Bounce: every domain Bounce() supports

struct BounceScene : Scene {
    PDBox Box = PDBox(V(-11.2f, -1.2f, 0), V(-8.8f, 1.2f, 2.4f));
    PDDisc Disc = PDDisc(V(-5, 0, 1.5f), V(0.35f, 0, 1), 1.4f);
    PDRectangle Rect = PDRectangle(V(-1.2f, -1.2f, 2.4f), V(2.4f, 0, 0), V(0, 2.4f, -1.4f));
    PDSphere Ball = PDSphere(V(5, 0, 1.2f), 1.2f);
    PDTriangle Tri = PDTriangle(V(8.6f, -1.3f, 1.5f), V(11.4f, -1.3f, 2.3f), V(10, 1.4f, 1.f));
    PDPlane Floor = PDPlane(V(0, 0, -1.f), ZAxis);
    int g = -1;

    BounceScene()
    {
        title = "Bounce - every surface Bounce() supports";
        summary = "Particles rain onto a box (from inside and out), a tilted disc, a tilted rectangle, a sphere and a triangle, then onto an infinite PDPlane floor.";
        Uses(CAT_ACTION, {"Gravity", "Bounce", "Move", "KillOld"});
        Uses(CAT_DOMAIN, {"PDBox", "PDDisc", "PDRectangle", "PDSphere", "PDTriangle", "PDPlane", "PDBlob"});
        Uses(CAT_API, {"Source", "S.Velocity"});
        camera.target = V(0, 0, 1.5f);
        camera.dist = 26.f;
    }

    void Start() override { g = NewGroup(20000); }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        const float xs[5] = {-10, -5, 0, 5, 10};
        const pVec cols[5] = {V(1, 0.4f, 1), V(0.3f, 0.9f, 1), V(0.6f, 1, 0.4f), V(0.4f, 0.6f, 1), V(1, 0.8f, 0.3f)};
        for (int i = 0; i < 5; ++i) {
            pSourceState S;
            S.Velocity(PDBlob(V(0, 0, 0), 0.4f)); // Small random velocities from a Gaussian blob
            S.Color(cols[i]);
            P.Source(150, PDDisc(V(xs[i], 0, 6.f), ZAxis, 0.6f), S);
        }
        pSourceState Inside;
        Inside.Velocity(PDBlob(V(0, 0, 3.f), 1.5f));
        Inside.Color(V(1, 1, 1));
        P.Source(60, PDPoint(V(-10, 0, 1.2f)), Inside); // These start inside the box and bounce around in it

        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.Gravity(p, V(0, 0, -9.8f));
            P.Bounce(p, 0.1f, 0.75f, 0.1f, Box);
            P.Bounce(p, 0.1f, 0.6f, 0.1f, Disc);
            P.Bounce(p, 0.1f, 0.6f, 0.1f, Rect);
            P.Bounce(p, 0.1f, 0.6f, 0.1f, Ball);
            P.Bounce(p, 0.1f, 0.6f, 0.1f, Tri);
            P.Bounce(p, 0.5f, 0.2f, 0.1f, Floor);
            P.Move(p);
            P.KillOld(p, 6.f);
        });
        P.CommitKills();
    }

    void Draw() override
    {
        GuideColor();
        GBox(V(-11.2f, -1.2f, 0), V(-8.8f, 1.2f, 2.4f));
        GDisc(V(-5, 0, 1.5f), V(0.35f, 0, 1), 1.4f);
        GRect(V(-1.2f, -1.2f, 2.4f), V(2.4f, 0, 0), V(0, 2.4f, -1.4f));
        GSphere(V(5, 0, 1.2f), 1.2f);
        GTri(V(8.6f, -1.3f, 1.5f), V(11.4f, -1.3f, 2.3f), V(10, 1.4f, 1.f));
        GPlane(V(0, 0, -1.f), ZAxis, 14.f, 14);
        const char* names[5] = {"PDBox", "PDDisc", "PDRectangle", "PDSphere", "PDTriangle"};
        for (int i = 0; i < 5; ++i) Label3D(V(-11.f + 5.f * i, -1.5f, -0.6f), names[i], 1.f, 0.9f, 0.45f);
        Label3D(V(-13.5f, -1.5f, 3.f), "white: born inside the box", 0.8f, 0.8f, 0.8f);
        Label3D(V(14.2f, 0, -1.f), "PDPlane floor", 1.f, 0.9f, 0.45f);
        P.CurrentGroup(g);
        DrawPoints(3.f);
    }
};

//////////////////////////////////////////////////////////////////
// 4. Avoid: steering around every domain Avoid() supports

struct AvoidScene : Scene {
    PDSphere Ball = PDSphere(V(-6, 0, 1.5f), 1.2f);
    PDRectangle Rect = PDRectangle(V(-3.2f, 0, 0.4f), V(2.4f, 0, 0), V(0, 0, 2.2f));
    PDTriangle Tri = PDTriangle(V(0.8f, 0, 0.3f), V(3.2f, 0, 0.3f), V(2.f, 0, 2.8f));
    PDDisc Disc = PDDisc(V(6, 0, 1.5f), V(0, -1, 0), 1.2f);
    PDPlane Wall = PDPlane(V(0, 9, 0), V(0, -1, 0));
    std::shared_ptr<PDUnion> Emitters;
    int g = -1;

    AvoidScene()
    {
        title = "Avoid - steering around obstacles";
        summary = "Four streams flow toward a sphere, rectangle, triangle and disc and steer around them, then veer from a PDPlane wall. They are emitted from a PDUnion of four discs.";
        Uses(CAT_ACTION, {"Avoid", "SpeedClamp", "Move", "KillOld"});
        Uses(CAT_DOMAIN, {"PDSphere", "PDRectangle", "PDTriangle", "PDDisc", "PDPlane", "PDUnion"});
        Uses(CAT_API, {"Source", "S.Velocity"});
        camera.target = V(0, 0, 1.5f);
        camera.yaw = -35.f;
        camera.pitch = 35.f;
        camera.dist = 26.f;
    }

    void Start() override
    {
        g = NewGroup(20000);
        std::vector<std::shared_ptr<pDomain>> discs;
        for (float x : {-6.f, -2.f, 2.f, 6.f}) discs.push_back(std::make_shared<PDDisc>(V(x, -10, 1.5f), V(0, 1, 0), 0.5f));
        Emitters = std::make_shared<PDUnion>(discs);
    }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        pSourceState S;
        S.Velocity(V(0, 4.f, 0));
        S.Color(PDLine(V(0.3f, 0.8f, 1.f), V(0.8f, 1.f, 1.f)));
        P.Source(800, *Emitters, S);

        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.Avoid(p, 6.f, 0.1f, 1.5f, Ball);
            P.Avoid(p, 6.f, 0.1f, 1.5f, Rect);
            P.Avoid(p, 6.f, 0.1f, 1.5f, Tri);
            P.Avoid(p, 6.f, 0.1f, 1.5f, Disc);
            P.Avoid(p, 8.f, 0.5f, 3.f, Wall);
            P.SpeedClamp(p, 2.f, 6.f); // Keep them moving after they turn
            P.Move(p);
            P.KillOld(p, 8.f);
        });
        P.CommitKills();
    }

    void Draw() override
    {
        GuideColor();
        GSphere(V(-6, 0, 1.5f), 1.2f);
        GRect(V(-3.2f, 0, 0.4f), V(2.4f, 0, 0), V(0, 0, 2.2f));
        GTri(V(0.8f, 0, 0.3f), V(3.2f, 0, 0.3f), V(2.f, 0, 2.8f));
        GDisc(V(6, 0, 1.5f), V(0, -1, 0), 1.2f);
        GPlane(V(0, 9, 1.5f), V(0, -1, 0), 5.f);
        for (float x : {-6.f, -2.f, 2.f, 6.f}) GDisc(V(x, -10, 1.5f), V(0, 1, 0), 0.5f);
        const char* names[4] = {"PDSphere", "PDRectangle", "PDTriangle", "PDDisc"};
        for (int i = 0; i < 4; ++i) Label3D(V(-6.8f + 4.f * i, 0, -0.2f), names[i], 1.f, 0.9f, 0.45f);
        Label3D(V(-7.f, 9, 4.f), "PDPlane wall", 1.f, 0.9f, 0.45f);
        Label3D(V(-8.f, -10, 2.4f), "PDUnion of 4 PDDiscs (Source)", 1.f, 0.9f, 0.45f);
        P.CurrentGroup(g);
        DrawPoints(3.f);
    }
};

//////////////////////////////////////////////////////////////////
// 5. Orbits and jets, in three particle groups

struct OrbitScene : Scene {
    const pVec L = V(-9, 0, 3), R = V(9, 0, 3);
    PDCylinder JetZone = PDCylinder(V(0, 0, -0.5f), V(0, 0, 2.f), 1.2f);
    PDCone JetAccel = PDCone(V(0, 0, 0), V(0, 0, 60.f), 12.f); // Acceleration domain: mostly up, with a spread
    int gPoint = -1, gLine = -1, gJet = -1;

    OrbitScene()
    {
        title = "Orbits and jets - three particle groups";
        summary = "Left: OrbitPoint() pulls toward a point. Right: OrbitLine() pulls toward a vertical line. Center: particles wander with RandomAccel() until they drift into the Jet() zone and are blasted upward.";
        Uses(CAT_ACTION, {"OrbitPoint", "OrbitLine", "Jet", "RandomAccel", "Gravity", "Damping", "Move", "KillOld"});
        Uses(CAT_DOMAIN, {"PDSphere", "PDCylinder", "PDCone", "PDBlob", "PDDisc"});
        Uses(CAT_API, {"Source", "S.Velocity", "CurrentGroup"});
        camera.target = V(0, 0, 3.f);
        camera.dist = 30.f;
    }

    void Start() override
    {
        gPoint = NewGroup(6000);
        gLine = NewGroup(6000);
        gJet = NewGroup(10000);
    }

    void Update(float dt) override
    {
        // Group 1: OrbitPoint. Emitted on a spherical shell around the attractor.
        P.CurrentGroup(gPoint);
        pSourceState S1;
        S1.Velocity(PDBlob(V(0, 0, 0), 1.f));
        S1.Color(PDLine(V(1, 0.4f, 0.2f), V(1, 0.9f, 0.3f)));
        P.Source(400, PDSphere(L, 3.f, 2.6f), S1);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.OrbitPoint(p, L, 40.f, 2.f);
            P.Move(p);
            P.KillOld(p, 6.f);
        });
        P.CommitKills();

        // Group 2: OrbitLine. Emitted in a tube around the line.
        P.CurrentGroup(gLine);
        pSourceState S2;
        S2.Velocity(PDBlob(V(0, 0, 0), 1.f));
        S2.Color(PDLine(V(0.3f, 0.6f, 1), V(0.6f, 1, 1)));
        P.Source(400, PDCylinder(R - V(0, 0, 3), R + V(0, 0, 3), 3.f, 2.6f), S2);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.OrbitLine(p, R, ZAxis, 40.f, 2.f);
            P.Move(p);
            P.KillOld(p, 6.f);
        });
        P.CommitKills();

        // Group 3: wander on the ground; the jet launches whatever enters its zone
        P.CurrentGroup(gJet);
        pSourceState S3;
        S3.Color(PDLine(V(0.5f, 1, 0.5f), V(1, 1, 1)));
        P.Source(600, PDDisc(V(0, 0, 0.05f), ZAxis, 4.f), S3);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.RandomAccel(p, PDSphere(V(0, 0, 0), 6.f));
            P.Jet(p, JetZone, JetAccel);
            P.Gravity(p, V(0, 0, -9.8f));
            P.Damping(p, V(0.3f, 0.3f, 0.95f));
            P.Move(p);
            if (p.pos.z() < 0.f) { // Keep the wanderers on the ground (a plain C++ tweak inside the loop)
                p.pos = V(p.pos.x(), p.pos.y(), 0.f);
                p.vel = V(p.vel.x(), p.vel.y(), 0.f);
            }
            P.KillOld(p, 5.f);
        });
        P.CommitKills();
    }

    void Draw() override
    {
        GuideColor();
        GPoint(L, 0.4f);
        GLine(R - V(0, 0, 5), R + V(0, 0, 5));
        GCylinder(V(0, 0, -0.5f), V(0, 0, 2.f), 1.2f);
        GDisc(V(0, 0, 0.05f), ZAxis, 4.f);
        Label3D(L + V(-1.5f, 0, -4.5f), "OrbitPoint (group 1)", 1.f, 0.9f, 0.45f);
        Label3D(L + V(-1.5f, 0, -5.f), "emitted on a PDSphere shell", 0.75f, 0.75f, 0.75f);
        Label3D(R + V(-1.5f, 0, -4.5f), "OrbitLine (group 2)", 1.f, 0.9f, 0.45f);
        Label3D(R + V(-1.5f, 0, -5.f), "emitted in a PDCylinder tube", 0.75f, 0.75f, 0.75f);
        Label3D(V(-2.f, 0, -1.f), "Jet zone: PDCylinder, accel: PDCone", 1.f, 0.9f, 0.45f);
        glEnable(GL_BLEND);
        for (int g : groups) {
            P.CurrentGroup(g);
            DrawPoints(3.f);
        }
    }
};

//////////////////////////////////////////////////////////////////
// 6. Vortex

struct VortexScene : Scene {
    PDCylinder Bounds = PDCylinder(V(0, 0, -1), V(0, 0, 14), 9.f);
    int g = -1;

    VortexScene()
    {
        title = "Vortex - a tornado";
        summary = "Particles born on a ground ring (a PDDisc with an inner radius) are drawn in, lifted and spun by Vortex(). Sink() with a PDCylinder kills any that escape.";
        Uses(CAT_ACTION, {"Vortex", "SpeedClamp", "Sink", "Move", "KillOld"});
        Uses(CAT_DOMAIN, {"PDDisc", "PDCylinder"});
        Uses(CAT_API, {"Source"});
        camera.target = V(0, 0, 5.f);
        camera.pitch = 12.f;
        camera.dist = 28.f;
    }

    void Start() override { g = NewGroup(30000); }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        pSourceState S;
        S.Color(PDLine(V(0.6f, 0.55f, 0.5f), V(0.9f, 0.85f, 0.8f)));
        P.Source(3000, PDDisc(V(0, 0, 0), ZAxis, 8.f, 6.f), S);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            // Tip below ground, so the funnel has width at z = 0. Inside the funnel Vortex() *replaces* velocity with
            // (upSpeed, aroundSpeed) * dt, so those two act like speeds times 1/dt rather than accelerations.
            P.Vortex(p, V(0, 0, -2), V(0, 0, 14), 1.8f, 8.f, 15.f, 240.f, 600.f);
            P.SpeedClamp(p, 0.f, 14.f);
            P.Move(p);
            P.Sink(p, false, Bounds);
            P.KillOld(p, 8.f);
        });
        P.CommitKills();
    }

    void Draw() override
    {
        GuideColor(0.35f);
        GDisc(V(0, 0, 0), ZAxis, 8.f, 6.f);
        GCylinder(V(0, 0, -1), V(0, 0, 14), 9.f);
        GuideColor();
        GLine(V(0, 0, -2), V(0, 0, 12));
        Label3D(V(0.3f, 0, 12.3f), "vortex axis", 1.f, 0.9f, 0.45f);
        Label3D(V(6.f, 0, 0.3f), "PDDisc ring (Source)", 1.f, 0.9f, 0.45f);
        Label3D(V(9.2f, 0, 13.f), "PDCylinder (Sink outside)", 1.f, 0.9f, 0.45f);
        P.CurrentGroup(g);
        glEnable(GL_BLEND);
        DrawPoints(2.f);
    }
};

//////////////////////////////////////////////////////////////////
// 7. Explosion and Restore

struct ExplodeScene : Scene {
    const pVec C = V(0, 0, 4.f);
    int g = -1;
    std::string phase;

    ExplodeScene()
    {
        title = "Explosion & Restore";
        summary = "Vertex() creates a torus of particles. Each remembers its home in posB (S.VertexB) and its surface normal in up (S.UpVec). Restore() flies them home; Explosion() blasts them apart.";
        Uses(CAT_ACTION, {"Restore", "Explosion", "Damping", "RotDamping", "RandomRotVelocity", "Move"});
        Uses(CAT_DOMAIN, {"PDSphere"});
        Uses(CAT_API, {"Vertex", "S.VertexB", "S.VertexBTracks", "S.UpVec", "S.Color"});
        camera.target = C;
        camera.pitch = 25.f;
        camera.dist = 18.f;
    }

    void Start() override
    {
        g = NewGroup(1000);
        PDSphere Scatter(C, 12.f, 10.f); // Particles start scattered on a big shell
        const int NU = 48, NV = 16;
        const float RR = 3.f, r = 1.f;
        for (int i = 0; i < NU; ++i)
            for (int j = 0; j < NV; ++j) {
                float a = 2.f * float(M_PI) * i / NU, b = 2.f * float(M_PI) * j / NV;
                pVec n(cosf(a) * cosf(b), sinf(a) * cosf(b), sinf(b)); // Surface normal
                pVec home = C + V(cosf(a), sinf(a), 0) * RR + n * r;

                pSourceState S;
                S.VertexBTracks(false); // Don't set posB = pos. Use the VertexB domain instead...
                S.VertexB(home);        // ...so posB is the particle's home on the torus.
                S.UpVec(n);             // Restore() also returns the up vector to this value.
                S.Color(V(0.5f + 0.5f * cosf(a), 0.5f + 0.5f * sinf(a), 0.6f + 0.4f * sinf(b)));
                P.Vertex(Scatter.Generate(), S);
            }
    }

    // Jump to the start of the next explosion phase of the 7 second cycle that begins at t = 5
    void Trigger() override { time = time < 5.f ? 5.f : 5.f + 7.f * (floorf((time - 5.f) / 7.f) + 1.f); }
    std::string TriggerHelp() const override { return "Space: explode now"; }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        float t = time;
        float c = t < 5.f ? -1.f : fmodf(t - 5.f, 7.f);

        if (c < 0.f) {
            phase = t < 3.f ? "assembling: Restore(time_left)" : "holding: Restore(0)";
            float left = std::max(0.f, 3.f - t);
            P.ParticleLoop(PAR, [&](Particle_t& p) {
                P.Restore(p, left);
                P.Move(p);
            });
        } else if (c < 1.f) {
            phase = "exploding: Explosion() + RandomRotVelocity()";
            float radius = c * 14.f;
            P.ParticleLoop(PAR, [&](Particle_t& p) {
                P.Explosion(p, C, radius, 900.f, 1.f, 0.1f);
                if (c < 0.1f) P.RandomRotVelocity(p, PDSphere(V(0, 0, 0), 8.f));
                P.Move(p);
            });
        } else if (c < 2.f) {
            phase = "drifting: Damping() + RotDamping()";
            P.ParticleLoop(PAR, [&](Particle_t& p) {
                P.Damping(p, V(0.2f));
                P.RotDamping(p, V(0.2f));
                P.Move(p);
            });
        } else if (c < 5.f) {
            phase = "restoring: Restore(time_left)";
            float left = 5.f - c;
            P.ParticleLoop(PAR, [&](Particle_t& p) {
                P.Restore(p, left);
                P.Move(p);
            });
        } else {
            phase = "holding: Restore(0)";
            P.ParticleLoop(PAR, [&](Particle_t& p) {
                P.Restore(p, 0.f);
                P.Move(p);
            });
        }
    }

    std::vector<std::string> Status() override { return {"Phase: " + phase}; }

    void Draw() override
    {
        P.CurrentGroup(g);
        DrawUpSticks(0.35f);
        DrawPoints(4.f);
        glColor4f(0.35f, 0.4f, 0.45f, 0.35f);
        GPlane(V(0, 0, 0), ZAxis, 12.f, 12);
    }
};

//////////////////////////////////////////////////////////////////
// 8. Random motion, one group per action

struct RandomScene : Scene {
    int gAcc = -1, gVel = -1, gDisp = -1, gRot = -1;

    RandomScene()
    {
        title = "Random motion - four groups";
        summary = "From left: RandomAccel() jitters the velocity (SpeedClamp() caps it); RandomVelocity() replaces it; RandomDisplace() moves positions directly; RandomRotVelocity() spins the up-vector sticks.";
        Uses(CAT_ACTION, {"RandomAccel", "RandomVelocity", "RandomDisplace", "RandomRotVelocity", "SpeedClamp", "Move", "KillOld"});
        Uses(CAT_DOMAIN, {"PDSphere", "PDBlob", "PDBox", "PDPoint", "PDLine"});
        Uses(CAT_API, {"Source", "CurrentGroup", "S.Velocity"});
        camera.target = V(0, 0, 3.f);
        camera.dist = 26.f;
        camera.pitch = 12.f;
    }

    void Start() override
    {
        gAcc = NewGroup(4000);
        gVel = NewGroup(4000);
        gDisp = NewGroup(4000);
        gRot = NewGroup(400);
    }

    void Update(float dt) override
    {
        pSourceState S;
        S.Velocity(V(0, 0, 1.2f));

        P.CurrentGroup(gAcc);
        S.Color(V(1, 0.5f, 0.3f));
        P.Source(250, PDPoint(V(-9, 0, 0.5f)), S);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.RandomAccel(p, PDSphere(V(0, 0, 0), 25.f));
            P.SpeedClamp(p, 0.f, 3.f);
            P.Move(p);
            P.KillOld(p, 5.f);
        });
        P.CommitKills();

        P.CurrentGroup(gVel);
        S.Color(V(0.4f, 1, 0.5f));
        P.Source(250, PDPoint(V(-3, 0, 0.5f)), S);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.RandomVelocity(p, PDBlob(V(0, 0, 1.2f), 1.2f));
            P.Move(p);
            P.KillOld(p, 5.f);
        });
        P.CommitKills();

        P.CurrentGroup(gDisp);
        S.Color(V(0.4f, 0.7f, 1));
        P.Source(250, PDPoint(V(3, 0, 0.5f)), S);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.RandomDisplace(p, PDBox(V(-0.12f), V(0.12f)));
            P.Move(p);
            P.KillOld(p, 5.f);
        });
        P.CommitKills();

        P.CurrentGroup(gRot);
        S.Color(V(1, 0.9f, 0.4f));
        S.Velocity(V(0, 0, 0.8f));
        P.Source(40, PDLine(V(8, 0, 0.5f), V(10, 0, 0.5f)), S);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.RandomRotVelocity(p, PDSphere(V(0, 0, 0), 6.f));
            P.Move(p, true, true); // true, true also applies rotational velocity to the up vector
            P.KillOld(p, 8.f);
        });
        P.CommitKills();
    }

    void Draw() override
    {
        const char* names[4] = {"RandomAccel(PDSphere)", "RandomVelocity(PDBlob)", "RandomDisplace(PDBox)", "RandomRotVelocity(PDSphere)"};
        for (int i = 0; i < 4; ++i) Label3D(V(-10.5f + 6.f * i, 0, -0.5f), names[i], 1.f, 0.9f, 0.45f);
        DrawGround(13.f);
        glEnable(GL_BLEND);
        for (int g : {gAcc, gVel, gDisp}) {
            P.CurrentGroup(g);
            DrawPoints(3.f);
        }
        P.CurrentGroup(gRot);
        DrawUpSticks(0.5f);
        DrawPoints(4.f);
    }
};

//////////////////////////////////////////////////////////////////
// 9. Target actions

struct TargetScene : Scene {
    int gColor = -1, gSpin = -1;

    TargetScene()
    {
        title = "Target actions - easing attributes toward a goal";
        summary = "Left: random colors, sizes and velocities ease toward orange, size 0.25 and straight up (TargetColor/Size/Velocity). Right: each up-vector stick starts with a random rotational velocity; TargetRotVelocity() eases them all to one, so the sticks swing round to point the same way.";
        Uses(CAT_ACTION, {"TargetColor", "TargetSize", "TargetVelocity", "TargetRotVelocity", "Move", "KillOld"});
        Uses(CAT_DOMAIN, {"PDSphere", "PDBox", "PDLine", "PDRectangle"});
        Uses(CAT_API, {"Source", "S.Size", "S.StartingAge", "S.RotVelocity", "S.UpVec", "S.Color", "S.Velocity", "CurrentGroup"});
        camera.target = V(0, 0, 3.f);
        camera.dist = 22.f;
    }

    void Start() override
    {
        gColor = NewGroup(4000);
        gSpin = NewGroup(800);
    }

    void Update(float dt) override
    {
        P.CurrentGroup(gColor);
        pSourceState S;
        S.Color(PDBox(V(0, 0, 0), V(1, 1, 1)));             // Any color
        S.Size(PDLine(V(0.03f), V(0.3f)));                  // Random size between 0.03 and 0.3
        S.Velocity(PDSphere(V(0, 0, 0), 4.f));              // Random direction
        S.StartingAge(0.f, 0.6f);                           // Ages vary, so particles don't all die together
        P.Source(250, PDSphere(V(-5, 0, 1.f), 0.4f), S);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.TargetColor(p, V(1, 0.55f, 0.1f), 0.8f, 1.f); // Toward orange
            P.TargetSize(p, V(0.25f), V(1.f));
            P.TargetVelocity(p, V(0, 0, 2.f), 1.f); // Toward straight up
            P.Move(p);
            P.KillOld(p, 4.f);
        });
        P.CommitKills();

        P.CurrentGroup(gSpin);
        pSourceState R;
        R.Color(V(0.6f, 0.9f, 1.f));
        R.UpVec(V(1, 0, 0));
        R.RotVelocity(PDSphere(V(0, 0, 0), 3.f)); // Random rotational velocities
        R.StartingAge(0.f, 0.5f);
        P.Source(60, PDRectangle(V(3, -2, 0.5f), V(4, 0, 0), V(0, 4, 0)), R);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.TargetRotVelocity(p, V(0, 0, 4.f), 3.f); // All converge to the same rotational velocity, so the sticks swing to vertical
            P.Move(p, true, true);
            P.KillOld(p, 6.f);
        });
        P.CommitKills();
    }

    void Draw() override
    {
        Label3D(V(-7.f, 0, -0.6f), "TargetColor + TargetSize + TargetVelocity", 1.f, 0.9f, 0.45f);
        Label3D(V(3.f, 0, -0.6f), "TargetRotVelocity", 1.f, 0.9f, 0.45f);
        DrawGround(10.f);
        glEnable(GL_BLEND);
        P.CurrentGroup(gColor);
        DrawSprites(1.f);
        P.CurrentGroup(gSpin);
        DrawUpSticks(0.6f);
        DrawPoints(4.f);
    }
};

//////////////////////////////////////////////////////////////////
// 10. Flocking

struct FlockScene : Scene {
    PDPlane Floor = PDPlane(V(0, 0, 0), ZAxis);
    pVec goal;
    int g = -1;

    FlockScene()
    {
        title = "Flocking - inter-particle actions";
        summary = "Boids chase a moving goal (OrbitPoint), stay together (Gravitate +), keep apart (Gravitate -), align headings (MatchVelocity) and spins (MatchRotVelocity), and Avoid() the floor.";
        Uses(CAT_ACTION, {"OrbitPoint", "Gravitate", "MatchVelocity", "MatchRotVelocity", "SpeedClamp", "Avoid", "Move"});
        Uses(CAT_DOMAIN, {"PDBox", "PDSphere", "PDPlane"});
        Uses(CAT_API, {"Vertex", "S.Velocity", "S.RotVelocity", "S.UpVec"});
        camera.target = V(0, 0, 4.f);
        camera.dist = 26.f;
    }

    void Start() override
    {
        g = NewGroup(400);
        pSourceState S;
        S.Velocity(PDSphere(V(0, 0, 0), 2.f));
        S.RotVelocity(PDSphere(V(0, 0, 0), 6.f)); // Random spins, which MatchRotVelocity() will bring into step
        S.UpVec(V(0, 0, 1));
        S.Color(PDLine(V(0.9f, 0.9f, 1), V(0.6f, 0.8f, 1)));
        PDBox Start(V(-6, -6, 2), V(6, 6, 8));
        for (int i = 0; i < 300; ++i) P.Vertex(Start.Generate(), S);
    }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        goal = V(8.f * cosf(time * 0.5f), 8.f * sinf(time * 0.5f), 4.f + 2.f * sinf(time * 0.9f));
        const float radius = 1.5f;
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.OrbitPoint(p, goal, 250.f, 10.f);         // Follow the goal
            P.Gravitate(p, 0.4f, 0.05f);                // Flock centering
            P.MatchVelocity(p, 0.5f, 0.5f, radius);     // Align with neighbors
            P.Gravitate(p, -1.5f, 0.1f, radius * 0.6f); // Don't crowd
            P.MatchRotVelocity(p, 1.f, 0.5f, radius * 2.f);
            P.Avoid(p, 8.f, 0.2f, 2.f, Floor);
            P.SpeedClamp(p, 2.f, 6.f);
            P.Move(p, true, true);
        });
    }

    void Draw() override
    {
        DrawGround(14.f);
        GuideColor();
        GPoint(goal, 0.4f);
        Label3D(goal + V(0.4f, 0, 0.4f), "goal", 1.f, 0.9f, 0.45f);
        P.CurrentGroup(g);
        PView v;
        if (v.Get()) { // Draw each boid as a dart pointing along its velocity
            glLineWidth(2.f);
            glBegin(GL_LINES);
            for (size_t i = 0; i < v.n; ++i) {
                pVec p = v.Vec(i, v.pos), vel = v.Vec(i, v.vel);
                pVec tail = p - vel * 0.12f;
                glColor4f(0.4f, 0.5f, 0.7f, 1.f);
                glVertex3f(tail.x(), tail.y(), tail.z());
                glColor4fv(v.At(i, v.color));
                glVertex3f(p.x(), p.y(), p.z());
            }
            glEnd();
            glLineWidth(1.f);
        }
        DrawUpSticks(0.3f);
    }
};

//////////////////////////////////////////////////////////////////
// 11. Follow the leader

struct FollowScene : Scene {
    int g = -1;
    pVec head;
    static constexpr int N = 120;

    FollowScene()
    {
        title = "Follow - a snake";
        summary = "Follow() pulls each particle toward the next one in the group. Every frame the last particle (the head) is killed with KillOld(-0.5, true) and replaced by Vertex() at the new head position.";
        Uses(CAT_ACTION, {"Follow", "TargetVelocity", "SpeedClamp", "KillOld", "Move", "CommitKills"});
        Uses(CAT_DOMAIN, {"PDPoint"});
        Uses(CAT_API, {"Vertex", "S.StartingAge", "GetParticles", "ParticleLoop(seq)"});
        camera.target = V(0, 0, 3.f);
        camera.dist = 22.f;
        camera.pitch = 35.f;
    }

    pVec HeadAt(float t) { return V(6.f * sinf(t * 0.9f), 5.f * sinf(t * 1.3f), 3.f + 1.5f * sinf(t * 0.7f)); }

    void Start() override
    {
        g = NewGroup(N + 4);
        pSourceState S;
        S.Color(PDLine(V(0.3f, 0.9f, 0.3f), V(0.7f, 1.f, 0.3f)));
        for (int i = 0; i < N; ++i) P.Vertex(HeadAt(0.f) - V(0.f, 0.03f * (N - i), 0.f), S); // Body, in order from tail to head
        pSourceState H;
        H.StartingAge(-1.f); // The head is the only particle with a negative age
        H.Color(V(1, 0.3f, 0.2f));
        P.Vertex(HeadAt(0.f), H);
    }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        head = HeadAt(time);
        // Sequential: Follow() reads the next particle, so a parallel loop would give order-dependent results
        P.ParticleLoop(SEQ, [&](Particle_t& p) {
            P.Follow(p, 80.f, 1.f);
            // Strong drag so links don't overshoot. Damping() removes at most 100% of velocity per second,
            // so here TargetVelocity() toward zero acts as a drag of 8 per second.
            P.TargetVelocity(p, V(0.f), 8.f);
            P.SpeedClamp(p, 0.f, 9.f);
            P.Move(p);
            P.KillOld(p, -0.5f, true); // Kill particles younger than -0.5: only the head
        });
        P.CommitKills();

        pSourceState H;
        H.StartingAge(-1.f);
        H.Color(V(1, 0.3f, 0.2f));
        P.Vertex(head, H); // The new head goes at the end of the group, so Follow() leaves it alone
    }

    void Draw() override
    {
        DrawGround(10.f);
        P.CurrentGroup(g);
        // GetParticles() copies positions into our own array, which suits drawing a line strip in group order
        size_t n = P.GetGroupCount();
        if (n > 0) {
            std::vector<float> pos(n * 3), col(n * 4);
            P.GetParticles(0, n, pos.data(), true, col.data());
            glLineWidth(3.f);
            glBegin(GL_LINE_STRIP);
            for (size_t i = 0; i < n; ++i) {
                glColor4fv(&col[i * 4]);
                glVertex3fv(&pos[i * 3]);
            }
            glEnd();
            glLineWidth(1.f);
        }
        DrawPoints(6.f);
        Label3D(head + V(0.4f, 0, 0.4f), "head (Vertex each frame)", 1.f, 0.9f, 0.45f);
    }
};

//////////////////////////////////////////////////////////////////
// 12. Waypoints, with speed-to-color and speed-to-size mapping

struct WaypointScene : Scene {
    std::vector<pVec> track;
    bool loop = true;
    int g = -1;

    WaypointScene()
    {
        title = "Waypoints - following a path";
        summary = "Waypoints() steers each particle toward its current waypoint and advances on arrival. The index is kept in the particle's data, so S.Data must be 0. Speed maps to color and size.";
        Uses(CAT_ACTION, {"Waypoints", "Damping", "VelocityColor", "VelocitySize", "Move", "KillOld"});
        Uses(CAT_DOMAIN, {"PDSphere"});
        Uses(CAT_API, {"Source", "S.Data"});
        camera.target = V(0, 0, 2.f);
        camera.pitch = 40.f;
        camera.dist = 26.f;
    }

    void Start() override
    {
        g = NewGroup(4000);
        track.clear();
        for (int i = 0; i < 7; ++i) {
            float a = 2.f * float(M_PI) * i / 7;
            track.push_back(V(9.f * cosf(a), 6.f * sinf(a), 1.f + 3.f * (i % 2)));
        }
    }

    void Trigger() override { loop = !loop; }
    std::string TriggerHelp() const override { return "Space: toggle loop"; }
    std::vector<std::string> Status() override { return {loop ? "loop = true: back to waypoint 0 after the last" : "loop = false: particles stop at the last waypoint"}; }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        pSourceState S;
        S.Data(0); // Required: the data attribute holds the current waypoint index (0 is also the default)
        P.Source(40, PDSphere(track.back(), 0.4f), S);
        const pVec* wp = track.data();
        int count = (int)track.size();
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.Waypoints(p, wp, count, 14.f, 1.2f, loop);
            P.Damping(p, V(0.15f)); // Bleed off speed so particles don't circle the waypoints
            P.VelocityColor(p, 0.f, 8.f, V(0.2f, 0.4f, 1.f), V(1.f, 0.25f, 0.1f));
            P.VelocitySize(p, 0.f, 8.f, V(0.12f), V(0.45f));
            P.Move(p);
            P.KillOld(p, 25.f);
        });
        P.CommitKills();
    }

    void Draw() override
    {
        DrawGround(12.f);
        GuideColor(0.4f);
        glBegin(GL_LINE_LOOP);
        for (auto& w : track) glVertex3f(w.x(), w.y(), w.z());
        glEnd();
        GuideColor();
        for (size_t i = 0; i < track.size(); ++i) {
            GSphere(track[i], 0.3f);
            Label3D(track[i] + V(0.4f, 0, 0.4f), "waypoint " + std::to_string(i), 1.f, 0.9f, 0.45f);
        }
        Label3D(V(-11.f, -8.f, 0.f), "blue/small = slow   red/large = fast", 0.8f, 0.8f, 0.8f);
        P.CurrentGroup(g);
        glEnable(GL_BLEND);
        DrawSprites(1.f);
    }
};

//////////////////////////////////////////////////////////////////
// 13. Voxel field, with position-to-color mapping

struct VoxelScene : Scene {
    static constexpr int DX = 8, DY = 8, DZ = 5;
    const pVec Origin = V(-6, -6, 0), Size = V(12, 12, 8);
    std::vector<pVec> field;
    bool interpolate = true;
    int g = -1;

    VoxelScene()
    {
        title = "VoxelField - a gridded wind field";
        summary = "VoxelField() samples an 8x8x5 grid of acceleration vectors (yellow arrows): a swirl that rises. PositionColor() colors particles by where they are. Sink() kills those leaving the grid's PDBox.";
        Uses(CAT_ACTION, {"VoxelField", "PositionColor", "Damping", "Sink", "Move", "KillOld"});
        Uses(CAT_DOMAIN, {"PDBox"});
        Uses(CAT_API, {"Source"});
        camera.target = V(0, 0, 4.f);
        camera.pitch = 30.f;
        camera.dist = 26.f;
    }

    void Start() override
    {
        g = NewGroup(30000);
        field.resize(DX * DY * DZ);
        for (int z = 0; z < DZ; ++z)
            for (int y = 0; y < DY; ++y)
                for (int x = 0; x < DX; ++x) {
                    pVec c = Origin + CompMult(Size, V((x + 0.5f) / DX, (y + 0.5f) / DY, (z + 0.5f) / DZ)); // Cell center
                    pVec radial = V(c.x(), c.y(), 0);
                    pVec tangent = Cross(ZAxis, radial);
                    field[(z * DY + y) * DX + x] = tangent * 1.2f - radial * 0.3f + V(0, 0, 1.5f + 0.4f * z);
                }
    }

    void Trigger() override { interpolate = !interpolate; }
    std::string TriggerHelp() const override { return "Space: toggle interpolation"; }
    std::vector<std::string> Status() override
    {
        return {interpolate ? "interpolate = true: smooth trilinear sampling" : "interpolate = false: nearest cell (paths kink at cell edges)"};
    }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        pSourceState S;
        P.Source(2500, PDBox(V(-5, -5, 0.05f), V(5, 5, 0.3f)), S);
        PDBox Grid(Origin, Origin + Size);
        const pVec* f = field.data();
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.VoxelField(p, Origin, Size, DX, DY, DZ, f, 1.f, interpolate);
            P.Damping(p, V(0.6f));
            P.PositionColor(p, V(-6, -6, 0), V(6, 6, 8), V(0.1f, 0.3f, 1.f), V(1.f, 1.f, 0.2f));
            P.Move(p);
            P.Sink(p, false, Grid);
            P.KillOld(p, 8.f);
        });
        P.CommitKills();
    }

    void Draw() override
    {
        GuideColor(0.3f);
        GBox(Origin, Origin + Size);
        glColor4f(1.f, 0.9f, 0.3f, 0.6f);
        glBegin(GL_LINES);
        for (int z = 0; z < DZ; z += 2)
            for (int y = 0; y < DY; ++y)
                for (int x = 0; x < DX; ++x) {
                    pVec c = Origin + CompMult(Size, V((x + 0.5f) / DX, (y + 0.5f) / DY, (z + 0.5f) / DZ));
                    pVec e = c + field[(z * DY + y) * DX + x] * 0.3f;
                    glVertex3f(c.x(), c.y(), c.z());
                    glVertex3f(e.x(), e.y(), e.z());
                }
        glEnd();
        Label3D(Origin + V(0, 0, 8.4f), "PDBox grid bounds (Sink outside)", 1.f, 0.9f, 0.45f);
        P.CurrentGroup(g);
        glEnable(GL_BLEND);
        DrawPoints(2.f);
    }
};

//////////////////////////////////////////////////////////////////
// 14. Particle-particle collisions

struct CollideScene : Scene {
    const pVec Lo = V(-3, -3, 0), Hi = V(3, 3, 7);
    PDBox Box = PDBox(V(-3, -3, 0), V(3, 3, 7));
    int g = -1;

    CollideScene()
    {
        title = "ResolveCollisions - billiard balls in zero gravity";
        summary = "Legacy ResolveCollisions() bounces particles off each other as spheres of radius size.x, using their mass: bigger, redder balls are heavier. It computes all impulses from one snapshot, so momentum is conserved. Zero gravity: impulses alone cannot hold up a resting pile.";
        Uses(CAT_ACTION, {"ResolveCollisions", "Bounce", "Move", "RandomVelocity"});
        Uses(CAT_DOMAIN, {"PDBox", "PDSphere"});
        Uses(CAT_API, {"Vertex", "S.Size", "S.Mass", "S.Velocity", "Legacy actions"});
        camera.target = V(0, 0, 2.5f);
        camera.pitch = 25.f;
        camera.dist = 16.f;
    }

    void Start() override
    {
        g = NewGroup(200);
        int i = 0;
        for (float z = 0.5f; z < 6.5f; z += 0.9f)
            for (float y = -2.4f; y < 2.5f; y += 1.f)
                for (float x = -2.4f; x < 2.5f; x += 1.f) {
                    if (i++ >= 90) return;
                    float r = 0.15f + 0.25f * pRandf();
                    pSourceState S;
                    S.Size(V(r));                    // Radius
                    S.Mass(r * r * r * 60.f);        // Mass grows with volume
                    S.Velocity(PDSphere(V(0, 0, 0), 3.f));
                    S.Color(V(0.3f + 1.6f * r, 0.75f - 1.2f * r, 1.2f - 2.2f * r));
                    P.Vertex(V(x, y, z), S);
                }
    }

    void Trigger() override
    {
        P.CurrentGroup(g);
        P.RandomVelocity(PDSphere(V(0, 0, 0), 6.f)); // A one-off legacy (non-inline) action applied to the whole group
    }
    std::string TriggerHelp() const override { return "Space: kick everything (legacy RandomVelocity)"; }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        // The legacy ResolveCollisions() computes every impulse from the same starting velocities, then applies them,
        // so momentum is conserved. The inline version updates particles one at a time while others read them, which
        // can add energy when a light particle is processed before the heavy one it hits.
        P.ResolveCollisions(0.95f, 0.02f);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.Bounce(p, 0.f, 0.95f, 0.f, Box);
            P.Move(p);
        });
    }

    void Draw() override
    {
        GuideColor(0.5f);
        GBox(Lo, Hi);
        Label3D(Hi + V(0.2f, 0, 0), "PDBox (Bounce inside)", 1.f, 0.9f, 0.45f);
        P.CurrentGroup(g);
        DrawBalls();
    }
};

//////////////////////////////////////////////////////////////////
// 15. SPH fluid with the legacy API and an action list

struct SPHScene : Scene {
    const pVec Lo = V(0, 0, 0), Hi = V(5, 1.6f, 4);
    PDBox Tank = PDBox(V(0, 0, 0), V(5, 1.6f, 4));
    int g = -1, list = -1;
    static constexpr int Substeps = 4;

    SPHScene()
    {
        title = "SPH - a dam break, using an action list";
        summary = "SPH() and DensityColor() are legacy-only (they need every particle's density first). The fluid's actions are recorded once into an action list, which is called 4 times per frame with dt/4. DensityColor() then runs once per frame.";
        Uses(CAT_ACTION, {"SPH", "DensityColor", "Gravity", "Bounce", "Move"});
        Uses(CAT_DOMAIN, {"PDBox"});
        Uses(CAT_API, {"Vertex", "Action lists", "TimeStep substeps", "Legacy actions"});
        camera.target = V(2.5f, 0.8f, 1.5f);
        camera.pitch = 15.f;
        camera.dist = 11.f;
    }

    void Start() override
    {
        g = NewGroup(1000);
        Fill();

        // Record the per-step actions once. Domains and parameters are copied into the list.
        list = P.GenActionLists(1);
        actionLists.push_back(list);
        P.NewActionList(list);
        // A lattice with 0.2 spacing has density about 126 with this radius and mass 1. A slightly lower
        // target lets the column relax a little before gravity takes over.
        P.SPH(0.4f, 110.f, 20.f, 1.f);
        P.Gravity(V(0, 0, -9.8f));
        P.Bounce(0.02f, 0.2f, 0.f, Tank);
        P.Move(true, false);
        P.EndActionList();
    }

    void Fill()
    {
        P.CurrentGroup(g);
        P.SetMaxParticles(0); // Empty the group...
        P.SetMaxParticles(1000);
        pSourceState S;
        S.Color(V(0.3f, 0.5f, 1.f));
        S.Size(V(0.09f)); // Drawn as balls of this radius
        for (float z = 0.1f; z < 2.8f; z += 0.2f)
            for (float y = 0.1f; y < 1.5f; y += 0.2f)
                for (float x = 0.1f; x < 1.5f; x += 0.2f) P.Vertex(V(x, y, z), S);
    }

    void Trigger() override { Fill(); }
    std::string TriggerHelp() const override { return "Space: refill the dam"; }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        P.TimeStep(dt / Substeps); // Smaller steps keep the stiff fluid stable
        for (int i = 0; i < Substeps; ++i) P.CallActionList(list);
        P.TimeStep(dt);
        P.DensityColor(0.35f, 4.f, 18.f, V(0.65f, 0.9f, 1.f), V(0.05f, 0.2f, 0.75f)); // Legacy action, outside the list
    }

    void Draw() override
    {
        GuideColor(0.5f);
        GBox(Lo, Hi);
        Label3D(Hi + V(0.2f, 0, 0), "PDBox tank", 1.f, 0.9f, 0.45f);
        Label3D(Lo + V(0, 0, -0.4f), "color: DensityColor (dark = crowded)", 0.8f, 0.8f, 0.8f);
        P.CurrentGroup(g);
        DrawBalls();
    }
};

//////////////////////////////////////////////////////////////////
// 16. Life and death: callbacks and the killing actions

std::atomic<int> Births(0), Deaths[4];

void OnBirth(Particle_t& p, const pdata_t) { Births++; }
void OnDeath(Particle_t& p, const pdata_t) { Deaths[p.data < 4 ? p.data : 0]++; }

// A Callback() action: your own per-particle code, here fading alpha with age
void FadeWithAge(Particle_t& p, const pdata_t lifetime_ms, const float dt) { p.alpha = std::min(p.alpha, Clamp01(1.f - p.age / (lifetime_ms * 0.001f))); }

struct LifeScene : Scene {
    PDSphere Hole = PDSphere(V(1.5f, 0, 3.5f), 1.2f);
    PDSphere Slow = PDSphere(V(0, 0, 0), 0.8f); // In velocity space: speeds below 0.8
    PDPlane Floor = PDPlane(V(0, 0, -0.5f), ZAxis);
    int g = -1;

    LifeScene()
    {
        title = "Life and death - callbacks and killing";
        summary = "Emitter 1 (data 1) fires up a cone; SinkVelocity() pops particles that slow at the top. Emitter 2 (data 2) arcs into a sphere that Sink() eats. Birth/death callbacks count them.";
        Uses(CAT_ACTION, {"Sink", "SinkVelocity", "KillOld", "Callback", "RandomColor", "Gravity", "Move", "CommitKills"});
        Uses(CAT_DOMAIN, {"PDCone", "PDSphere", "PDLine", "PDPoint", "PDPlane", "PDBlob"});
        Uses(CAT_API, {"Source", "S.Data", "S.Color(+alpha)", "BirthCallback", "DeathCallback", "Vertex(data)"});
        camera.target = V(0, 0, 3.f);
        camera.dist = 20.f;
    }

    void Start() override
    {
        g = NewGroup(20000);
        P.BirthCallback(OnBirth);
        P.DeathCallback(OnDeath);
        Births = 0;
        for (auto& d : Deaths) d = 0;
    }

    void Trigger() override
    {
        P.CurrentGroup(g);
        pSourceState S;
        S.Color(V(1, 1, 1));
        S.Size(V(1));
        S.Velocity(V(0, 0, 6.f));
        P.Vertex(V(0, 0, 0.2f), S, 3); // Vertex() with its own user data value
    }
    std::string TriggerHelp() const override { return "Space: Vertex(pos, S, 3) - one particle with data 3"; }

    std::vector<std::string> Status() override
    {
        return {"BirthCallback: " + std::to_string(Births.load()) + " born",
                "DeathCallback by data: emitter 1: " + std::to_string(Deaths[1].load()) + "   emitter 2: " + std::to_string(Deaths[2].load()) +
                    "   Vertex(data 3): " + std::to_string(Deaths[3].load())};
    }

    void Update(float dt) override
    {
        P.CurrentGroup(g);
        pSourceState S1;
        S1.Data(1);
        S1.Velocity(PDCone(V(0, 0, 0), V(0, 0, 9.f), 2.5f));                            // A cone in velocity space
        S1.Color(PDLine(V(1, 0.4f, 0.1f), V(1, 0.9f, 0.2f)), PDLine(V(0.6f), V(1.f))); // Color and alpha domains
        P.Source(500, PDPoint(V(-3, 0, 0)), S1);

        pSourceState S2;
        S2.Data(2);
        S2.Velocity(PDBlob(V(-2.f, 0, 6.5f), 0.6f));
        S2.Color(PDLine(V(0.2f, 0.6f, 1), V(0.5f, 1, 1)), PDLine(V(0.6f), V(1.f)));
        P.Source(400, PDLine(V(5, -2, 0), V(5, 2, 0)), S2);

        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.Gravity(p, V(0, 0, -9.8f));
            P.RandomColor(p, PDSphere(V(0, 0, 0), 0.8f)); // Colors drift randomly
            P.Callback(p, FadeWithAge, 3000);             // Our own function, called per particle
            P.Move(p);
            if (p.data == 1 && p.vel.z() < 0.f) P.SinkVelocity(p, true, Slow); // Pop emitter-1 particles that slow down after their peak
            P.Sink(p, true, Hole);                                             // Kill inside the sphere
            P.Sink(p, false, Floor);                                           // Kill below the floor
            P.KillOld(p, 3.f);
        });
        P.CommitKills(); // DeathCallback is called here, for each killed particle
    }

    void Draw() override
    {
        GuideColor();
        GSphere(V(1.5f, 0, 3.5f), 1.2f);
        GCone(V(-3, 0, 0), V(-3, 0, 1.5f), 0.42f);
        GLine(V(5, -2, 0), V(5, 2, 0));
        GPlane(V(0, 0, -0.5f), ZAxis, 8.f);
        Label3D(V(2.8f, 0, 4.5f), "PDSphere: Sink(kill_inside)", 1.f, 0.9f, 0.45f);
        Label3D(V(-5.5f, 0, -0.3f), "emitter 1: PDPoint, velocity PDCone", 1.f, 0.9f, 0.45f);
        Label3D(V(4.f, 0, -0.3f), "emitter 2: PDLine", 1.f, 0.9f, 0.45f);
        P.CurrentGroup(g);
        glEnable(GL_BLEND);
        DrawPoints(3.f);
    }
};

//////////////////////////////////////////////////////////////////
// 17. Groups, CopyGroup, CopyVertexB, and Sort

struct GroupsScene : Scene {
    std::shared_ptr<PDUnion> Chimneys;
    PDPlane Floor = PDPlane(V(0, 0, 0), ZAxis);
    int gSmoke = -1, gSparks = -1;
    bool sorting = true;
    float nextCopy = 0.f;

    GroupsScene()
    {
        title = "Groups, CopyGroup and Sort";
        summary = "Smoke rises from a PDUnion of two chimneys. Every 0.2 s CopyGroup() copies a few smoke particles into a second group, where they fall as sparks with CopyVertexB() streaks. Sort() orders the translucent smoke back to front.";
        Uses(CAT_ACTION, {"Sort", "CopyVertexB", "TargetColor", "TargetSize", "RandomAccel", "Gravity", "Bounce", "Move", "KillOld"});
        Uses(CAT_DOMAIN, {"PDUnion", "PDDisc", "PDCylinder", "PDSphere", "PDPlane"});
        Uses(CAT_API, {"Source", "CopyGroup", "CurrentGroup", "SetMaxParticles", "S.Size"});
        camera.target = V(0, 0, 4.f);
        camera.dist = 22.f;
        camera.pitch = 10.f;
    }

    void Start() override
    {
        gSmoke = NewGroup(4000);
        gSparks = NewGroup(0);
        P.CurrentGroup(gSparks);
        P.SetMaxParticles(3000); // Groups can be resized at any time
        Chimneys = std::make_shared<PDUnion>(PDDisc(V(-3, 0, 2), ZAxis, 0.6f), PDDisc(V(3, 0, 2), ZAxis, 0.6f));
        nextCopy = 0.f;
    }

    void Trigger() override { sorting = !sorting; }
    std::string TriggerHelp() const override { return "Space: toggle Sort"; }
    std::vector<std::string> Status() override
    {
        return {sorting ? "Sort: on - smoke drawn back to front" : "Sort: off - see the hard edges where near puffs hide far ones"};
    }

    void Update(float dt) override
    {
        P.CurrentGroup(gSmoke);
        pSourceState S;
        S.Velocity(PDCylinder(V(0, 0, 1.2f), V(0, 0, 1.8f), 0.4f));
        S.Color(V(0.75f, 0.75f, 0.8f), 0.45f);
        S.Size(V(0.3f));
        P.Source(120, *Chimneys, S);
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.RandomAccel(p, PDSphere(V(0, 0, 0), 1.5f));
            P.TargetColor(p, V(0.35f, 0.35f, 0.4f), 0.f, 0.15f);
            P.TargetSize(p, V(1.8f), V(0.25f));
            P.Move(p);
            P.KillOld(p, 7.f);
        });
        P.CommitKills();
        if (sorting) P.Sort(Cam.Eye(), Cam.target - Cam.Eye()); // A legacy action: reorders the whole group

        P.CurrentGroup(gSparks);
        if ((nextCopy -= dt) <= 0.f) {
            nextCopy = 0.2f;
            P.CurrentGroup(gSmoke);
            size_t n = P.GetGroupCount();
            P.CurrentGroup(gSparks);
            if (n > 0) P.CopyGroup(gSmoke, size_t(pRandf() * n) % n, 12); // Copy 12 smoke particles into the current group
        }
        P.ParticleLoop(PAR, [&](Particle_t& p) {
            P.TargetColor(p, V(1.f, 0.6f, 0.15f), 1.f, 4.f);
            P.TargetSize(p, V(0.06f), V(4.f));
            P.Gravity(p, V(0, 0, -9.8f));
            P.Bounce(p, 0.2f, 0.5f, 0.1f, Floor);
            P.CopyVertexB(p, true, false); // Remember where it was, for the streak
            P.Move(p);
            P.KillOld(p, 10.f);
        });
        P.CommitKills();
    }

    void Draw() override
    {
        DrawGround(10.f);
        GuideColor();
        GDisc(V(-3, 0, 2), ZAxis, 0.6f);
        GDisc(V(3, 0, 2), ZAxis, 0.6f);
        Label3D(V(-6.5f, 0, 1.5f), "PDUnion of two PDDiscs", 1.f, 0.9f, 0.45f);
        glEnable(GL_BLEND);
        P.CurrentGroup(gSparks);
        DrawStreaks(4.f);
        P.CurrentGroup(gSmoke);
        DrawSprites(1.f, true); // Depth writes on, so drawing order matters
    }
};

//////////////////////////////////////////////////////////////////
// The scene list and coverage

std::vector<std::shared_ptr<Scene>> Scenes;
int CurScene = 0;

void MakeScenes()
{
    Scenes = {std::make_shared<FountainScene>(), std::make_shared<DomainGalleryScene>(), std::make_shared<BounceScene>(), std::make_shared<AvoidScene>(),
              std::make_shared<OrbitScene>(),    std::make_shared<VortexScene>(),        std::make_shared<ExplodeScene>(), std::make_shared<RandomScene>(),
              std::make_shared<TargetScene>(),   std::make_shared<FlockScene>(),         std::make_shared<FollowScene>(),  std::make_shared<WaypointScene>(),
              std::make_shared<VoxelScene>(),    std::make_shared<CollideScene>(),       std::make_shared<SPHScene>(),     std::make_shared<LifeScene>(),
              std::make_shared<GroupsScene>()};
}

// Which scenes show each feature, e.g. Coverage[CAT_ACTION]["Bounce"] = {1, 3, 14}
std::map<std::string, std::vector<int>> Coverage[CAT_COUNT];

// Returns false if any feature is not shown or a scene names an unknown feature
bool CheckCoverage(bool print)
{
    bool ok = true;
    for (int c = 0; c < CAT_COUNT; ++c) {
        Coverage[c].clear();
        for (auto& f : AllFeatures[c]) Coverage[c][f] = {};
    }
    for (size_t s = 0; s < Scenes.size(); ++s)
        for (int c = 0; c < CAT_COUNT; ++c)
            for (auto& f : Scenes[s]->features[c]) {
                auto it = Coverage[c].find(f);
                if (it == Coverage[c].end()) {
                    std::cerr << "Scene " << s + 1 << " lists unknown " << CategoryNames[c] << " feature \"" << f << "\"\n";
                    ok = false;
                } else
                    it->second.push_back(int(s + 1));
            }
    for (int c = 0; c < CAT_COUNT; ++c) {
        if (print) std::cout << "\n" << CategoryNames[c] << ":\n";
        for (auto& f : AllFeatures[c]) {
            auto& v = Coverage[c][f];
            if (v.empty()) {
                std::cerr << "NOT SHOWN: " << CategoryNames[c] << " " << f << "\n";
                ok = false;
            }
            if (print) {
                std::cout << "  " << f << std::string(f.size() < 26 ? 26 - f.size() : 1, ' ') << "scenes";
                for (int s : v) std::cout << " " << s;
                std::cout << "\n";
            }
        }
    }
    return ok;
}

void StartScene(int i)
{
    if (!Scenes.empty()) Scenes[CurScene]->Stop();
    CurScene = (i + (int)Scenes.size()) % (int)Scenes.size();
    Scene& s = *Scenes[CurScene];
    P.Seed(1000 + CurScene); // The same scene always starts the same way
    s.time = 0.f;
    s.Start();
    Cam = s.camera;
}

//////////////////////////////////////////////////////////////////
// Headless self test: run every scene without graphics and report particle statistics

struct Stats {
    size_t n = 0;
    float maxR = 0.f, maxV = 0.f;
    pVec centroid = pVec(0.f);
    bool finite = true;
};

Stats Measure(Scene& s)
{
    Stats st;
    for (int g : s.groups) {
        P.CurrentGroup(g);
        size_t c = P.GetGroupCount();
        if (!c) continue;
        std::vector<float> pos(c * 3), vel(c * 3);
        P.GetParticles(0, c, pos.data(), false, nullptr, vel.data());
        for (size_t k = 0; k < c; ++k) {
            pVec p(&pos[k * 3]), v(&vel[k * 3]);
            st.finite = st.finite && std::isfinite(p.x() + p.y() + p.z() + v.x() + v.y() + v.z());
            st.maxR = std::max(st.maxR, p.length());
            st.maxV = std::max(st.maxV, v.length());
            st.centroid += p;
        }
        st.n += c;
    }
    if (st.n) st.centroid /= float(st.n);
    return st;
}

// traceScene > 0 also prints that scene's statistics once per simulated second
int SelfTest(int traceScene)
{
    bool ok = CheckCoverage(false);
    const float dt = 1.f / 60.f;
    for (size_t i = 0; i < Scenes.size(); ++i) {
        if (traceScene > 0 && traceScene != int(i + 1)) continue; // Tracing one scene: skip the others
        StartScene(int(i));
        Scene& s = *Scenes[i];
        if (i == 6 || i == 13 || i == 15) s.Trigger(); // Exercise the triggers too
        P.TimeStep(dt);
        for (int f = 0; f < 600; ++f) { // 10 simulated seconds
            s.Update(dt);
            s.time += dt;
            if (traceScene == int(i + 1) && f % 60 == 59) {
                Stats t = Measure(s);
                printf("   t=%4.1f  n %6zu  centroid %6.2f %6.2f %6.2f  max |pos| %6.2f  max |vel| %6.2f\n", s.time, t.n, t.centroid.x(), t.centroid.y(),
                       t.centroid.z(), t.maxR, t.maxV);
            }
        }
        Stats st = Measure(s);
        bool sceneOk = st.n > 0 && st.finite && st.maxR < 60.f; // Every scene keeps its particles near the origin
        ok = ok && sceneOk;
        printf("%-2zu %-52s particles %6zu  max |pos| %6.2f  max |vel| %6.2f  %s\n", i + 1, s.title.c_str(), st.n, st.maxR, st.maxV, sceneOk ? "ok" : "FAIL");
        for (auto& st : s.Status()) printf("      %s\n", st.c_str());
    }
    Scenes[CurScene]->Stop();
    printf(ok ? "SELF TEST PASSED\n" : "SELF TEST FAILED\n");
    return ok ? 0 : 1;
}

//////////////////////////////////////////////////////////////////
// On-screen text

int WinW = 1280, WinH = 800;
bool ShowText = true, ShowCoverage = false, Paused = false, AutoTour = false;
float Fps = 0.f, TourTimer = 0.f;
const float TourSeconds = 20.f;

bool MeasureOnly = false; // When true, text calls lay out without drawing, so panels can be sized first

void Text(int x, int y, const std::string& s, void* font = GLUT_BITMAP_HELVETICA_12)
{
    if (MeasureOnly) return;
    glRasterPos2i(x, y);
    glutBitmapString(font, (const unsigned char*)s.c_str());
}

int TextW(const std::string& s, void* font = GLUT_BITMAP_HELVETICA_12) { return glutBitmapLength(font, (const unsigned char*)s.c_str()); }

// Draw words wrapped to maxW pixels; returns the y after the last line
int WrapText(int x, int y, int maxW, const std::string& s, int lineH = 15, void* font = GLUT_BITMAP_HELVETICA_12)
{
    std::string line, word;
    auto flush = [&]() {
        Text(x, y, line, font);
        y += lineH;
        line.clear();
    };
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == ' ') {
            std::string trial = line.empty() ? word : line + " " + word;
            if (!line.empty() && TextW(trial, font) > maxW) {
                flush();
                line = word;
            } else
                line = trial;
            word.clear();
        } else
            word += s[i];
    }
    if (!line.empty()) flush();
    return y;
}

// A label in a category color followed by the feature names, wrapped
int FeatureLine(int x, int y, int maxW, const char* label, const std::vector<std::string>& items, float r, float g, float b)
{
    glColor3f(r, g, b);
    Text(x, y, label, GLUT_BITMAP_HELVETICA_12);
    int lx = x + 100;
    std::string joined;
    for (size_t i = 0; i < items.size(); ++i) joined += (i ? ",  " : "") + items[i];
    glColor3f(1.f, 1.f, 1.f);
    return WrapText(lx, y, maxW - 100, joined);
}

void Panel(int x0, int y0, int x1, int y1, float a = 0.6f)
{
    glColor4f(0.02f, 0.03f, 0.06f, a);
    glBegin(GL_QUADS);
    glVertex2i(x0, y0), glVertex2i(x1, y0), glVertex2i(x1, y1), glVertex2i(x0, y1);
    glEnd();
}

const float CatColor[CAT_COUNT][3] = {{1.f, 0.75f, 0.3f}, {0.45f, 0.85f, 1.f}, {0.6f, 1.f, 0.55f}};

// Scene numbers with consecutive runs shortened, e.g. {1,2,3,4,6,8,9} -> "1-4, 6, 8, 9"
std::string SceneList(const std::vector<int>& v)
{
    std::string s;
    for (size_t i = 0; i < v.size();) {
        size_t j = i;
        while (j + 1 < v.size() && v[j + 1] == v[j] + 1) ++j;
        if (!s.empty()) s += ", ";
        s += std::to_string(v[i]);
        if (j >= i + 2)
            s += "-" + std::to_string(v[j]);
        else if (j == i + 1)
            s += ", " + std::to_string(v[j]);
        i = j + 1;
    }
    return s;
}

void DrawCoverage()
{
    Scene& s = *Scenes[CurScene];
    std::set<std::string> here[CAT_COUNT];
    for (int c = 0; c < CAT_COUNT; ++c) here[c].insert(s.features[c].begin(), s.features[c].end());

    const int colW = 290, rowH = 15, rows = 28;
    int ncols = 0;
    for (int c = 0; c < CAT_COUNT; ++c) ncols += ((int)AllFeatures[c].size() + rows - 1) / rows;
    int w = ncols * colW + 30, h = rows * rowH + 90;
    int x0 = std::max(10, (WinW - w) / 2), y0 = std::max(10, (WinH - h) / 2);
    Panel(x0, y0, x0 + w, y0 + h, 0.85f);
    glColor3f(1, 1, 1);
    Text(x0 + 15, y0 + 24, "Coverage: every feature and the scenes that show it", GLUT_BITMAP_HELVETICA_18);
    glColor3f(0.7f, 0.7f, 0.7f);
    Text(x0 + 15, y0 + 44, "Highlighted = shown in the current scene (" + std::to_string(CurScene + 1) + "). Press C to close.");

    int col = 0;
    for (int c = 0; c < CAT_COUNT; ++c) {
        glColor3fv(CatColor[c]);
        Text(x0 + 15 + col * colW, y0 + 68, CategoryNames[c], GLUT_BITMAP_HELVETICA_12);
        for (size_t i = 0; i < AllFeatures[c].size(); ++i) {
            const std::string& f = AllFeatures[c][i];
            int cx = x0 + 15 + (col + int(i) / rows) * colW, cy = y0 + 88 + (int(i) % rows) * rowH;
            std::string sc = SceneList(Coverage[c][f]);
            if (here[c].count(f))
                glColor3f(1.f, 1.f, 0.3f);
            else if (sc.empty())
                glColor3f(1.f, 0.3f, 0.3f);
            else
                glColor3f(0.85f, 0.85f, 0.85f);
            Text(cx, cy, f);
            glColor3f(0.55f, 0.55f, 0.6f);
            Text(cx + 135, cy, sc.empty() ? "none!" : sc);
        }
        col += ((int)AllFeatures[c].size() + rows - 1) / rows;
    }
}

void DrawOverlay()
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, WinW, WinH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    Scene& s = *Scenes[CurScene];
    if (ShowText) {
        const int x = 14, w = std::min(640, WinW - 28);
        auto body = [&]() -> int {
        int y = 28;
        glColor3f(1, 1, 1);
        Text(x, y, "Scene " + std::to_string(CurScene + 1) + "/" + std::to_string(Scenes.size()) + ":  " + s.title, GLUT_BITMAP_HELVETICA_18);
        y += 22;
        glColor3f(0.85f, 0.85f, 0.85f);
        y = WrapText(x, y, w, s.summary) + 6;
        y = FeatureLine(x, y, w, "Actions:", s.features[CAT_ACTION], CatColor[0][0], CatColor[0][1], CatColor[0][2]);
        y = FeatureLine(x, y, w, "Domains:", s.features[CAT_DOMAIN], CatColor[1][0], CatColor[1][1], CatColor[1][2]);
        y = FeatureLine(x, y, w, "Emission/API:", s.features[CAT_API], CatColor[2][0], CatColor[2][1], CatColor[2][2]);
        glColor3f(1.f, 1.f, 0.5f);
        for (auto& st : s.Status()) {
            Text(x, y, st);
            y += 15;
        }
        glColor3f(0.65f, 0.65f, 0.7f);
        char buf[128];
        snprintf(buf, sizeof(buf), "%zu particles   %.0f fps%s%s", s.ParticleCount(), Fps, Paused ? "   PAUSED" : "", AutoTour ? "   AUTO TOUR" : "");
        Text(x, y, buf);
        return y;
        };
        MeasureOnly = true;
        int bottom = body();
        MeasureOnly = false;
        Panel(4, 4, x + w + 10, bottom + 10, 0.55f);
        body();

        std::string keys = "Left/Right or N/P: scene   1-9: jump   R: restart   C: coverage   A: auto tour   F: pause   H: hide text   drag: orbit   wheel: zoom";
        if (!s.TriggerHelp().empty()) keys = s.TriggerHelp() + "   " + keys;
        Panel(0, WinH - 24, WinW, WinH, 0.55f);
        glColor3f(0.8f, 0.8f, 0.85f);
        Text(10, WinH - 8, keys);
    }
    if (ShowCoverage) DrawCoverage();

    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

//////////////////////////////////////////////////////////////////
// GLUT callbacks

int LastMs = 0;

// Screenshot mode (--shots <dir>): run each scene for a fixed simulated time, save a BMP, and move on
std::string ShotDir;
int ShotFrame = 0;
const int ShotFrames = 240; // 4 seconds at a fixed 1/60 s step

void SaveBMP(const std::string& path)
{
    int w = WinW, h = WinH, rowBytes = (w * 3 + 3) & ~3;
    std::vector<unsigned char> px(rowBytes * h);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, px.data());
    unsigned int fileSize = 54 + rowBytes * h;
    unsigned char hdr[54] = {'B', 'M'};
    auto put32 = [&](int at, unsigned int v) { memcpy(hdr + at, &v, 4); };
    put32(2, fileSize), put32(10, 54), put32(14, 40), put32(18, w), put32(22, h);
    hdr[26] = 1, hdr[28] = 24;
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "wb");
    if (!f) return;
    fwrite(hdr, 1, 54, f);
    fwrite(px.data(), 1, px.size(), f);
    fclose(f);
}

void Display()
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    float frame = (now - LastMs) * 0.001f;
    LastMs = now;
    Fps = Fps * 0.9f + (frame > 0.f ? 1.f / frame : 0.f) * 0.1f;
    float dt = std::min(frame, 1.f / 30.f); // Frame-rate independent, but don't take huge steps after a hitch
    if (!ShotDir.empty()) dt = 1.f / 60.f;  // Deterministic in screenshot mode

    Scene& s = *Scenes[CurScene];
    if (!Paused && dt > 0.f) {
        P.TimeStep(dt);
        s.Update(dt);
        s.time += dt;
        if (AutoTour && (TourTimer += dt) > TourSeconds) {
            TourTimer = 0.f;
            StartScene(CurScene + 1);
        }
    }

    glClearColor(0.07f, 0.08f, 0.11f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    pVec eye = Cam.Eye();
    gluLookAt(eye.x(), eye.y(), eye.z(), Cam.target.x(), Cam.target.y(), Cam.target.z(), 0, 0, 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Scenes[CurScene]->Draw();
    DrawOverlay();

    if (!ShotDir.empty() && ++ShotFrame >= ShotFrames) {
        if (ShowCoverage) { // The extra frame after the last scene shows the coverage table
            SaveBMP(ShotDir + "/coverage.bmp");
            exit(0);
        }
        char name[32];
        snprintf(name, sizeof(name), "/scene%02d.bmp", CurScene + 1);
        SaveBMP(ShotDir + name);
        ShotFrame = 0;
        if (CurScene + 1 >= (int)Scenes.size()) {
            ShowCoverage = true;
            ShotFrame = ShotFrames - 1;
        } else
            StartScene(CurScene + 1);
    }
    glutSwapBuffers();
}

void Reshape(int w, int h)
{
    WinW = w, WinH = std::max(h, 1);
    glViewport(0, 0, w, WinH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(40, w / double(WinH), 0.1, 300);
    glMatrixMode(GL_MODELVIEW);
}

void Keyboard(unsigned char key, int, int)
{
    switch (key) {
    case 27:
    case 'q': exit(0);
    case 'n': StartScene(CurScene + 1); break;
    case 'p': StartScene(CurScene - 1); break;
    case 'r': StartScene(CurScene); break;
    case 'c': ShowCoverage = !ShowCoverage; break;
    case 'h': ShowText = !ShowText; break;
    case 'f': Paused = !Paused; break;
    case 'a':
        AutoTour = !AutoTour;
        TourTimer = 0.f;
        break;
    case ' ': Scenes[CurScene]->Trigger(); break;
    default:
        if (key >= '1' && key <= '9') StartScene(key - '1');
    }
}

void Special(int key, int, int)
{
    if (key == GLUT_KEY_RIGHT) StartScene(CurScene + 1);
    if (key == GLUT_KEY_LEFT) StartScene(CurScene - 1);
}

int MouseBtn = -1, MouseX = 0, MouseY = 0;

void Mouse(int button, int state, int x, int y)
{
    MouseBtn = state == GLUT_DOWN ? button : -1;
    MouseX = x, MouseY = y;
    if (state == GLUT_DOWN && (button == 3 || button == 4)) Cam.dist *= button == 3 ? 0.9f : 1.1f; // Wheel on some GLUTs
}

void MouseWheel(int, int dir, int, int) { Cam.dist *= dir > 0 ? 0.9f : 1.1f; }

void Motion(int x, int y)
{
    int dx = x - MouseX, dy = y - MouseY;
    MouseX = x, MouseY = y;
    if (MouseBtn == GLUT_LEFT_BUTTON) {
        Cam.yaw -= dx * 0.4f;
        Cam.pitch = std::max(-85.f, std::min(85.f, Cam.pitch + dy * 0.4f));
    } else if (MouseBtn == GLUT_RIGHT_BUTTON)
        Cam.dist *= 1.f + dy * 0.01f;
}

int main(int argc, char** argv)
{
    MakeScenes();

    try {
        if (argc > 1 && !strcmp(argv[1], "--selftest")) return SelfTest(argc > 2 ? atoi(argv[2]) : 0);
        if (argc > 2 && !strcmp(argv[1], "--shots")) ShotDir = argv[2];

        if (!CheckCoverage(true)) std::cerr << "\nWarning: some features are not covered by any scene.\n";

        glutInit(&argc, argv);
        glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE | GLUT_MULTISAMPLE);
        glutInitWindowSize(WinW, WinH);
        glutCreateWindow("Particle System API Showcase");
        glewInit();

        glutDisplayFunc(Display);
        glutIdleFunc(Display);
        glutReshapeFunc(Reshape);
        glutKeyboardFunc(Keyboard);
        glutSpecialFunc(Special);
        glutMouseFunc(Mouse);
        glutMouseWheelFunc(MouseWheel);
        glutMotionFunc(Motion);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_LINE_SMOOTH);
        MakeSpriteTexture();

        StartScene(0);
        LastMs = glutGet(GLUT_ELAPSED_TIME);
        glutMainLoop();
    }
    catch (PError_t& Er) {
        std::cerr << "Particle API exception: " << Er.ErrMsg << std::endl;
        return 1;
    }
    return 0;
}
