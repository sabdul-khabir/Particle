#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cassert>

#define PASSERT(cond, msg) assert((cond) && msg)

#define GL_ASSERT() \
    { \
        unsigned int err; \
        while ((err = (unsigned int)glGetError()) != 0) { \
            std::cerr << "OpenGL error: " << err << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        } \
    }

inline void SRand() { srand((unsigned int)time(nullptr)); }
inline float frand() { return (float)rand() / (float)RAND_MAX; }
inline float frand(float low, float high) { return low + frand() * (high - low); }
inline int irand(int max) { return (max <= 0) ? 0 : rand() % max; }
inline int irand(int low, int high) { return low + irand(high - low); }

struct uc3Pixel {
    unsigned char data[3];
    uc3Pixel() { data[0] = data[1] = data[2] = 0; }
    uc3Pixel(unsigned char r, unsigned char g, unsigned char b) { data[0] = r; data[1] = g; data[2] = b; }
    unsigned char r() const { return data[0]; }
    unsigned char g() const { return data[1]; }
    unsigned char b() const { return data[2]; }
};

struct f3Pixel {
    float data[3];
    f3Pixel(const uc3Pixel& p) { data[0] = p.r() / 255.f; data[1] = p.g() / 255.f; data[2] = p.b() / 255.f; }
    float r() const { return data[0]; }
    float g() const { return data[1]; }
    float b() const { return data[2]; }
};

struct uc3Image {
    int width, height;
    std::vector<uc3Pixel> pix;
    uc3Image(int w = 0, int h = 0) : width(w), height(h), pix(w * h) {}
    int w() const { return width; }
    int h() const { return height; }
    size_t size() const { return pix.size(); }
    uc3Pixel& operator()(int x, int y) { return pix[y * width + x]; }
    const uc3Pixel& operator()(int x, int y) const { return pix[y * width + x]; }
};

inline void sample2(uc3Pixel& res, const uc3Image& img, float x, float y) {
    int x0 = (int)x; int y0 = (int)y;
    float fx = x - x0; float fy = y - y0;
    int x1 = std::min(x0 + 1, img.w() - 1);
    int y1 = std::min(y0 + 1, img.h() - 1);
    auto s00 = img(x0, y0); auto s10 = img(x1, y0);
    auto s01 = img(x0, y1); auto s11 = img(x1, y1);
    res = uc3Pixel(
        (unsigned char)((s00.r()*(1-fx) + s10.r()*fx)*(1-fy) + (s01.r()*(1-fx) + s11.r()*fx)*fy),
        (unsigned char)((s00.g()*(1-fx) + s10.g()*fx)*(1-fy) + (s01.g()*(1-fx) + s11.g()*fx)*fy),
        (unsigned char)((s00.b()*(1-fx) + s10.b()*fx)*(1-fy) + (s01.b()*(1-fx) + s11.b()*fx)*fy)
    );
}

class Timer {
    std::chrono::high_resolution_clock::time_point start_time;
public:
    Timer() { Reset(); }
    double Reset() {
        auto now = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        start_time = now;
        return elapsed;
    }
    double Read() const {
        return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start_time).count();
    }
};

class StatTimer {
    Timer clock;
    std::vector<float> history;
    int head = 0;
public:
    StatTimer(int n) : history(n, 0.0f) {}
    float Event() {
        float dt = (float)clock.Reset();
        history[head] = dt;
        head = (head + 1) % history.size();
        return dt;
    }
    float GetMean() const {
        float sum = 0;
        for (float f : history) sum += f;
        return sum / history.size();
    }
};

inline void RemoveArgs(int& argc, char** argv, int pos, int n) {
    for (int i = pos; i < argc - n; i++) argv[i] = argv[i + n];
    argc -= n;
}