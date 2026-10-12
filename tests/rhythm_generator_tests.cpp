#include "../src/core/RhythmGenerator.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
using namespace br;
static int count(const Patterns& p, int n) {
    int hits = 0; for (int i = 0; i < n; ++i) hits += p.gate[i].on ? 1 : 0; return hits;
}
int main() {
    Patterns p;
    const int originalPanLen = p.panLen;
    const float originalRate = p.rate[0].ratio;
    p.gate[20].on = 0;
    RhythmRequest r;
    r.steps = 16; r.pulses = 5; r.seed = 42;
    generateRhythm(r, p);
    assert(count(p, 16) == 5);
    assert(p.gate[20].on == 0);
    assert(p.panLen == originalPanLen && p.rate[0].ratio == originalRate);
    Patterns copy = p;
    generateRhythm(r, copy);
    for (int i = 0; i < 16; ++i) {
        assert(copy.gate[i].on == p.gate[i].on);
        assert(copy.gate[i].width == p.gate[i].width);
    }
    r.rotation = -3;
    Patterns rotated;
    generateRhythm(r, rotated);
    for (int i = 0; i < 16; ++i)
        assert(rotated.gate[i].on == p.gate[(i + 3) % 16].on);
    r.rotation = 0; r.pulses = 0;
    generateRhythm(r, p);
    assert(count(p, 16) == 0);
    r.pulses = 16; generateRhythm(r, p);
    assert(count(p, 16) == 16);
    r.steps = 999; r.pulses = 999; generateRhythm(r, p);
    assert(count(p, 64) == 64);
    r.steps = 16; r.style = RhythmStyle::Backbeat; r.variation = 0.7f;
    generateRhythm(r, p);
    assert(count(p, 16) >= 1);
    for (int i = 0; i < 16; ++i) {
        assert(p.gate[i].width == -1.f || (p.gate[i].width >= 0.15f && p.gate[i].width <= 1.f));
        assert(p.gate[i].dir == 0 && p.gate[i].fx == 0);
    }
    std::puts("Rhythm generator tests passed");
}
