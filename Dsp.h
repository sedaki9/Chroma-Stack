// ChromaStack DSP: 4 modül x 5 stereo efekt. Tüm efektler yerinde (in-place) çalışır
// ve "wet" sinyali üretir; karışım (mix) Module sınıfında yapılır.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace cs
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 2.0f * kPi;

inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float expMap (float x, float lo, float hi) { return lo * std::pow (hi / lo, x); }
inline float onePoleCoef (float fc, double sr)
{
    fc = std::min (fc, 0.45f * (float) sr);
    return 1.0f - std::exp (-kTwoPi * fc / (float) sr);
}

struct Rng
{
    uint32_t s = 22222u;
    explicit Rng (uint32_t seed = 22222u) : s (seed ? seed : 1u) {}
    uint32_t nextU() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (nextU() >> 8) * (1.0f / 16777216.0f); }
    float bi() { return uni() * 2.0f - 1.0f; }
};

// Yavaş, yumuşatılmış rastgele sapma (-1..1). "Drift" özelliğinin temeli.
struct Wander
{
    void prepare (double sr, float hz, uint32_t seed = 1234u)
    {
        coef = onePoleCoef (hz * 2.0f, sr);
        interval = std::max (1, (int) (sr / hz));
        count = 0;
        rng = Rng (seed);
        value = target = 0.f;
    }
    float next()
    {
        if (--count <= 0) { target = rng.bi(); count = interval; }
        value += coef * (target - value);
        return value;
    }
    Rng rng;
    float value = 0.f, target = 0.f, coef = 0.001f;
    int count = 0, interval = 1;
};

struct Delay
{
    void prepare (double sr, float maxSeconds)
    {
        size = (int) (sr * maxSeconds) + 4;
        buf.assign ((size_t) size, 0.f);
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.f); }
    void push (float x)
    {
        buf[(size_t) w] = x;
        if (++w >= size) w = 0;
    }
    // d: kaç örnek geriye (>=1). Okuma push'tan önce yapılırsa d=1 bir önceki örnektir.
    float read (float d) const
    {
        d = std::clamp (d, 1.0f, (float) (size - 2));
        float rp = (float) w - d;
        if (rp < 0.f) rp += (float) size;
        int i = (int) rp;
        float f = rp - (float) i;
        if (i >= size) i -= size;
        int j = i + 1;
        if (j >= size) j = 0;
        return buf[(size_t) i] + (buf[(size_t) j] - buf[(size_t) i]) * f;
    }
    std::vector<float> buf;
    int size = 4, w = 0;
};

struct Fx
{
    virtual ~Fx() = default;
    virtual void prepare (double sr, int /*maxBlock*/) { fs = sr; }
    virtual void reset() {}
    // a, b: 0..1 ; drift: 0..1
    virtual void process (float* l, float* r, int n, float a, float b, float drift) = 0;
    double fs = 44100.0;
};

//==================================================================== CHARACTER
struct Overdrive : Fx
{
    float lp[2] {};
    void reset() override { lp[0] = lp[1] = 0.f; }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        const float drive = 1.f + a * a * 40.f;
        const float comp = 1.f / std::sqrt (drive);
        const float k = onePoleCoef (expMap (b, 800.f, 12000.f), fs);
        float* ch[2] = { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float y = std::tanh (ch[c][i] * drive) * comp;
                lp[c] += k * (y - lp[c]);
                ch[c][i] = lp[c];
            }
    }
};

struct Fuzz : Fx
{
    float lp[2] {};
    void reset() override { lp[0] = lp[1] = 0.f; }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        const float g = 2.f + a * 80.f;
        const float k = onePoleCoef (expMap (b, 600.f, 9000.f), fs);
        float* ch[2] = { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i] * g;
                float y = x > 0.f ? std::tanh (x) : 0.7f * std::tanh (1.6f * x);
                lp[c] += k * (y * 0.35f - lp[c]);
                ch[c][i] = lp[c];
            }
    }
};

struct Compressor : Fx
{
    float env = 0.f;
    void reset() override { env = 0.f; }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        const float thrDb = -6.f - a * 40.f;
        const float makeup = -thrDb * 0.3f;
        const float att = std::exp (-1.f / (0.005f * (float) fs));
        const float rel = std::exp (-1.f / (lerp (0.05f, 0.8f, b) * (float) fs));
        for (int i = 0; i < n; ++i)
        {
            float in = std::max (std::abs (l[i]), std::abs (r[i]));
            env = in > env ? att * env + (1.f - att) * in : rel * env + (1.f - rel) * in;
            float envDb = 20.f * std::log10 (std::max (env, 1e-6f));
            float over = envDb - thrDb;
            float gDb = (over > 0.f ? -over * 0.75f : 0.f) + makeup;
            float g = std::pow (10.f, gDb * 0.05f);
            l[i] *= g;
            r[i] *= g;
        }
    }
};

struct TapeSat : Fx
{
    float lp[2] {};
    void reset() override { lp[0] = lp[1] = 0.f; }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        const float drive = 1.f + a * 6.f;
        const float comp = 1.f / std::sqrt (drive);
        const float k = onePoleCoef (expMap (b, 2000.f, 16000.f), fs);
        float* ch[2] = { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float y = std::tanh (ch[c][i] * drive) * comp;
                lp[c] += k * (y - lp[c]);
                ch[c][i] = lp[c];
            }
    }
};

// Zarf tetiklemeli ses artışı (volume swell)
struct Swell : Fx
{
    float env = 0.f, gain = 1.f;
    bool active = false;
    void reset() override { env = 0.f; gain = 1.f; active = false; }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        const float rise = lerp (0.03f, 2.0f, a);
        const float riseCoef = 1.f - std::exp (-3.f / (rise * (float) fs));
        const float thr = lerp (0.2f, 0.01f, b);
        const float atk = 1.f - std::exp (-1.f / (0.002f * (float) fs));
        const float rel = 1.f - std::exp (-1.f / (0.2f * (float) fs));
        for (int i = 0; i < n; ++i)
        {
            float in = std::max (std::abs (l[i]), std::abs (r[i]));
            env += (in > env ? atk : rel) * (in - env);
            if (! active && env > thr) { active = true; gain = 0.f; }
            else if (active && env < thr * 0.3f) active = false;
            if (active) gain += (1.f - gain) * riseCoef;
            l[i] *= gain;
            r[i] *= gain;
        }
    }
};

//==================================================================== MOVEMENT
struct Tremolo : Fx
{
    float ph = 0.f;
    void reset() override { ph = 0.f; }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        const float inc = expMap (a, 0.5f, 12.f) / (float) fs;
        for (int i = 0; i < n; ++i)
        {
            ph += inc;
            if (ph >= 1.f) ph -= 1.f;
            float pr = ph + 0.25f;
            if (pr >= 1.f) pr -= 1.f;
            l[i] *= 1.f - b * 0.5f * (1.f + std::sin (kTwoPi * ph));
            r[i] *= 1.f - b * 0.5f * (1.f + std::sin (kTwoPi * pr));
        }
    }
};

struct Vibrato : Fx
{
    Delay dl[2];
    Wander w;
    float ph = 0.f;
    void prepare (double sr, int mb) override
    {
        Fx::prepare (sr, mb);
        for (auto& d : dl) d.prepare (sr, 0.05f);
        w.prepare (sr, 0.6f, 11u);
    }
    void reset() override { for (auto& d : dl) d.clear(); }
    void process (float* l, float* r, int n, float a, float b, float drift) override
    {
        const float inc = expMap (a, 0.3f, 7.f) / (float) fs;
        const float msToSamp = 0.001f * (float) fs;
        for (int i = 0; i < n; ++i)
        {
            ph += inc;
            if (ph >= 1.f) ph -= 1.f;
            float pr = ph + 0.25f;
            if (pr >= 1.f) pr -= 1.f;
            float wob = w.next() * drift * 0.8f;
            dl[0].push (l[i]);
            dl[1].push (r[i]);
            l[i] = dl[0].read ((4.f + 3.f * b * std::sin (kTwoPi * ph) + wob) * msToSamp);
            r[i] = dl[1].read ((4.f + 3.f * b * std::sin (kTwoPi * pr) + wob) * msToSamp);
        }
    }
};

struct Phaser : Fx
{
    float z[2][4] {};
    float fbS[2] {};
    float ph = 0.f;
    void reset() override
    {
        for (auto& c : z) for (auto& s : c) s = 0.f;
        fbS[0] = fbS[1] = 0.f;
    }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        const float inc = expMap (a, 0.08f, 5.f) / (float) fs;
        const float fb = b * 0.85f;
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            ph += inc;
            if (ph >= 1.f) ph -= 1.f;
            for (int c = 0; c < 2; ++c)
            {
                float p = ph + 0.12f * (float) c;
                if (p >= 1.f) p -= 1.f;
                float lfo = 0.5f + 0.5f * std::sin (kTwoPi * p);
                float fc = expMap (lfo, 300.f, 3500.f);
                float t = std::tan (kPi * fc / (float) fs);
                float g = (t - 1.f) / (t + 1.f);
                float in = ch[c][i];
                float x = in + fb * fbS[c];
                for (int s = 0; s < 4; ++s)
                {
                    float y = g * x + z[c][s];
                    z[c][s] = x - g * y;
                    x = y;
                }
                fbS[c] = std::tanh (x);
                ch[c][i] = 0.5f * (in + x);
            }
        }
    }
};

struct Doubler : Fx
{
    Delay dl[2];
    Wander w;
    void prepare (double sr, int mb) override
    {
        Fx::prepare (sr, mb);
        for (auto& d : dl) d.prepare (sr, 0.1f);
        w.prepare (sr, 0.9f, 77u);
    }
    void reset() override { for (auto& d : dl) d.clear(); }
    void process (float* l, float* r, int n, float a, float b, float drift) override
    {
        const float msToSamp = 0.001f * (float) fs;
        for (int i = 0; i < n; ++i)
        {
            float wob = w.next() * (0.3f + drift) * 0.8f;
            float dL = (12.f + a * 28.f + wob) * msToSamp;
            float dR = (12.f + a * 28.f + b * 10.f - wob) * msToSamp;
            float inL = l[i], inR = r[i];
            dl[0].push (inL);
            dl[1].push (inR);
            l[i] = 0.6f * inL + 0.6f * dl[0].read (dL);
            r[i] = 0.6f * inR + 0.6f * dl[1].read (dR);
        }
    }
};

struct Chorus : Fx
{
    Delay dl[2];
    Wander w;
    float ph = 0.f;
    void prepare (double sr, int mb) override
    {
        Fx::prepare (sr, mb);
        for (auto& d : dl) d.prepare (sr, 0.06f);
        w.prepare (sr, 0.5f, 99u);
    }
    void reset() override { for (auto& d : dl) d.clear(); }
    void process (float* l, float* r, int n, float a, float b, float drift) override
    {
        const float inc = expMap (b, 0.15f, 3.f) / (float) fs;
        const float depth = a * 5.f;
        const float msToSamp = 0.001f * (float) fs;
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            ph += inc;
            if (ph >= 1.f) ph -= 1.f;
            float wob = w.next() * drift * 0.8f;
            for (int c = 0; c < 2; ++c)
            {
                float p1 = ph + 0.25f * (float) c;
                float p2 = p1 + 0.5f;
                p1 -= std::floor (p1);
                p2 -= std::floor (p2);
                float in = ch[c][i];
                dl[c].push (in);
                float t1 = dl[c].read ((14.f + depth * std::sin (kTwoPi * p1) + wob) * msToSamp);
                float t2 = dl[c].read ((14.f + depth * std::sin (kTwoPi * p2) - wob) * msToSamp);
                ch[c][i] = 0.5f * in + 0.35f * (t1 + t2);
            }
        }
    }
};

//==================================================================== DIFFUSION
// Echo ve Tape Delay (tape=true: wow/flutter, doygunluk, karanlık geri besleme)
struct EchoFx : Fx
{
    explicit EchoFx (bool tapeMode) : tape (tapeMode) {}
    bool tape;
    Delay dl[2];
    float lp[2] {};
    float tSm = 0.f;
    Wander w;
    void prepare (double sr, int mb) override
    {
        Fx::prepare (sr, mb);
        for (auto& d : dl) d.prepare (sr, 2.2f);
        w.prepare (sr, 0.7f, 5u);
        tSm = 0.3f * (float) sr;
    }
    void reset() override
    {
        for (auto& d : dl) d.clear();
        lp[0] = lp[1] = 0.f;
    }
    void process (float* l, float* r, int n, float a, float b, float drift) override
    {
        const float target = expMap (a, 0.06f, 1.0f) * (float) fs;
        const float fb = b * 0.92f;
        const float k = onePoleCoef (tape ? 2500.f : 7000.f, fs);
        const float glide = 1.f - std::exp (-1.f / (0.1f * (float) fs));
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            tSm += glide * (target - tSm);
            float d = tSm;
            if (tape) d *= 1.f + 0.004f * w.next() * (0.3f + drift);
            for (int c = 0; c < 2; ++c)
            {
                float y = dl[c].read (d);
                lp[c] += k * (y - lp[c]);
                float s = tape ? std::tanh (lp[c] * 1.2f) / 1.2f : lp[c];
                dl[c].push (ch[c][i] + fb * s);
                ch[c][i] = s;
            }
        }
    }
};

// Reverb ve Room (küçük oda). juce::Reverb kullanır.
struct ReverbFx : Fx
{
    ReverbFx (float roomLo, float roomHi) : lo (roomLo), hi (roomHi) {}
    float lo, hi;
    juce::Reverb rv;
    void prepare (double sr, int mb) override
    {
        Fx::prepare (sr, mb);
        rv.setSampleRate (sr);
    }
    void reset() override { rv.reset(); }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        juce::Reverb::Parameters p;
        p.roomSize = lerp (lo, hi, a);
        p.damping = 1.f - b;
        p.wetLevel = 0.33f;
        p.dryLevel = 0.f;
        p.width = 1.f;
        p.freezeMode = 0.f;
        rv.setParameters (p);
        rv.processStereo (l, r, n);
    }
};

// Allpass zinciri ile bulanıklaştırma
struct Smear : Fx
{
    static constexpr int kStages = 4;
    Delay dl[2][kStages];
    void prepare (double sr, int mb) override
    {
        Fx::prepare (sr, mb);
        for (auto& c : dl) for (auto& d : c) d.prepare (sr, 0.12f);
    }
    void reset() override { for (auto& c : dl) for (auto& d : c) d.clear(); }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        static const float lensMs[kStages] = { 5.3f, 8.1f, 13.7f, 19.3f };
        const float g = 0.3f + a * 0.45f;
        const float scale = 0.5f + b * 2.0f;
        float* ch[2] = { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                for (int s = 0; s < kStages; ++s)
                {
                    float len = lensMs[s] * (c == 1 ? 1.07f : 1.f) * scale * 0.001f * (float) fs;
                    float d = dl[c][s].read (len);
                    float v = x + g * d;
                    dl[c][s].push (v);
                    x = d - g * v;
                }
                ch[c][i] = x;
            }
    }
};

//==================================================================== TEXTURE
struct BitCrush : Fx
{
    float cnt[2] {}, held[2] {};
    void reset() override { cnt[0] = cnt[1] = held[0] = held[1] = 0.f; }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        const float bits = 16.f - a * 13.5f;
        const float levels = std::pow (2.f, bits - 1.f);
        const float hold = 1.f + b * b * 40.f;
        float* ch[2] = { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                if (cnt[c] <= 0.f)
                {
                    held[c] = std::round (ch[c][i] * levels) / levels;
                    cnt[c] += hold;
                }
                cnt[c] -= 1.f;
                ch[c][i] = held[c];
            }
    }
};

// Bant benzeri karartma + yalpalama (wow)
struct LoFi : Fx
{
    Delay dl[2];
    float lp[2] {};
    Wander w;
    void prepare (double sr, int mb) override
    {
        Fx::prepare (sr, mb);
        for (auto& d : dl) d.prepare (sr, 0.05f);
        w.prepare (sr, 0.4f, 31u);
    }
    void reset() override
    {
        for (auto& d : dl) d.clear();
        lp[0] = lp[1] = 0.f;
    }
    void process (float* l, float* r, int n, float a, float b, float drift) override
    {
        const float k = onePoleCoef (expMap (1.f - a, 1200.f, 14000.f), fs);
        const float depthMs = b * 4.f + drift * 1.f;
        const float msToSamp = 0.001f * (float) fs;
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            float d = (6.f + depthMs * w.next()) * msToSamp;
            for (int c = 0; c < 2; ++c)
            {
                dl[c].push (ch[c][i]);
                float y = dl[c].read (d);
                lp[c] += k * (y - lp[c]);
                ch[c][i] = std::tanh (lp[c] * 1.3f) / 1.3f;
            }
        }
    }
};

// Plak çıtırtısı + hışırtı
struct Crackle : Fx
{
    Rng rng { 4242u };
    float tick[2] {}, prevN[2] {};
    void reset() override { tick[0] = tick[1] = prevN[0] = prevN[1] = 0.f; }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        const float density = a * a * 0.0006f;
        float* ch[2] = { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                if (rng.uni() < density) tick[c] = rng.bi() * 0.8f;
                float t = tick[c];
                tick[c] *= -0.85f;
                float nz = rng.bi();
                float hiss = (nz - prevN[c]) * b * 0.03f;
                prevN[c] = nz;
                ch[c][i] += t + hiss;
            }
    }
};

// Rezonanslı alçak geçiren filtre (Cytomic TPT SVF)
struct FilterFx : Fx
{
    float ic1[2] {}, ic2[2] {};
    Wander w;
    void prepare (double sr, int mb) override
    {
        Fx::prepare (sr, mb);
        w.prepare (sr, 0.5f, 3u);
    }
    void reset() override { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.f; }
    void process (float* l, float* r, int n, float a, float b, float drift) override
    {
        const float base = expMap (a, 80.f, 14000.f);
        const float k = 2.f - 2.f * (b * 0.97f);
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            float fc = base * (1.f + drift * 0.3f * w.next());
            fc = std::clamp (fc, 20.f, 0.45f * (float) fs);
            float g = std::tan (kPi * fc / (float) fs);
            float a1 = 1.f / (1.f + g * (g + k));
            float a2 = g * a1;
            float a3 = g * a2;
            for (int c = 0; c < 2; ++c)
            {
                float v3 = ch[c][i] - ic2[c];
                float v1 = a1 * ic1[c] + a2 * v3;
                float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                ic1[c] = 2.f * v1 - ic1[c];
                ic2[c] = 2.f * v2 - ic2[c];
                ch[c][i] = v2;
            }
        }
    }
};

// Telsiz paraziti / sinyal kopmaları
struct Interference : Fx
{
    Wander w;
    Rng rng { 777u };
    float prevN[2] {};
    void prepare (double sr, int mb) override
    {
        Fx::prepare (sr, mb);
        w.prepare (sr, 3.f, 8u);
    }
    void reset() override { prevN[0] = prevN[1] = 0.f; }
    void process (float* l, float* r, int n, float a, float b, float) override
    {
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            float v = w.next();
            float burst = std::clamp ((v - (0.9f - a * 1.4f)) * 3.f, 0.f, 1.f);
            for (int c = 0; c < 2; ++c)
            {
                float nz = rng.bi();
                float hp = nz - prevN[c];
                prevN[c] = nz;
                ch[c][i] = ch[c][i] * (1.f - 0.85f * burst) + hp * 0.25f * burst * (0.2f + b);
            }
        }
    }
};

//==================================================================== MODULE
struct Module
{
    std::array<std::unique_ptr<Fx>, 5> fx;
    bool additive = false; // true: dry + wet*mix (zaman tabanlı) , false: crossfade
    int active = 0;
    float prevMix = 0.f;
    bool idle = true;
    std::vector<float> wl, wr;

    void prepare (double sr, int maxBlock)
    {
        wl.assign ((size_t) maxBlock, 0.f);
        wr.assign ((size_t) maxBlock, 0.f);
        for (auto& f : fx) f->prepare (sr, maxBlock);
        prevMix = 0.f;
        idle = true;
    }

    void process (float* l, float* r, int n, int idx, bool enabled, float a, float b, float mix, float drift)
    {
        if (n <= 0 || n > (int) wl.size()) return;
        idx = std::clamp (idx, 0, 4);
        const float target = enabled ? mix : 0.f;
        if (! enabled && prevMix < 1e-5f) { idle = true; prevMix = 0.f; return; }
        if (idx != active || idle) { active = idx; fx[(size_t) idx]->reset(); idle = false; }

        std::copy (l, l + n, wl.begin());
        std::copy (r, r + n, wr.begin());
        fx[(size_t) idx]->process (wl.data(), wr.data(), n, a, b, drift);

        float m = prevMix;
        const float step = (target - prevMix) / (float) n;
        for (int i = 0; i < n; ++i)
        {
            m += step;
            if (additive)
            {
                l[i] += wl[(size_t) i] * m;
                r[i] += wr[(size_t) i] * m;
            }
            else
            {
                l[i] = l[i] * (1.f - m) + wl[(size_t) i] * m;
                r[i] = r[i] * (1.f - m) + wr[(size_t) i] * m;
            }
        }
        prevMix = target;
    }
};

inline void buildModule (Module& m, int idx)
{
    switch (idx)
    {
        case 0:
            m.fx[0] = std::make_unique<Overdrive>();
            m.fx[1] = std::make_unique<Fuzz>();
            m.fx[2] = std::make_unique<Compressor>();
            m.fx[3] = std::make_unique<TapeSat>();
            m.fx[4] = std::make_unique<Swell>();
            break;
        case 1:
            m.fx[0] = std::make_unique<Tremolo>();
            m.fx[1] = std::make_unique<Vibrato>();
            m.fx[2] = std::make_unique<Phaser>();
            m.fx[3] = std::make_unique<Doubler>();
            m.fx[4] = std::make_unique<Chorus>();
            break;
        case 2:
            m.fx[0] = std::make_unique<EchoFx> (false);
            m.fx[1] = std::make_unique<ReverbFx> (0.45f, 0.98f);
            m.fx[2] = std::make_unique<EchoFx> (true);
            m.fx[3] = std::make_unique<ReverbFx> (0.1f, 0.4f);
            m.fx[4] = std::make_unique<Smear>();
            m.additive = true;
            break;
        default:
            m.fx[0] = std::make_unique<BitCrush>();
            m.fx[1] = std::make_unique<LoFi>();
            m.fx[2] = std::make_unique<Crackle>();
            m.fx[3] = std::make_unique<FilterFx>();
            m.fx[4] = std::make_unique<Interference>();
            break;
    }
}
} // namespace cs
