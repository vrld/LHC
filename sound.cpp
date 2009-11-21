#include <iostream>
#include <sstream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <AL/alut.h>
#include <boost/thread.hpp>

double clamp(double v, double min = -1., double max = 1.)
{ return (v > max ? max : (v < min ? min : v)); }

struct Generator
{ virtual double get(double p) = 0; };

struct Signal
{ 
    Signal(Generator* g, double f, double p = 0.) : generator(g), freq(f), phase(p) {}
    Generator *generator;
    double freq;
    double phase;
    virtual double get(double t) { return generator->get(fmod(t * freq + phase, 1.)); }
};

struct Add : public Signal
{
    Add(Signal *aa, Signal *bb) : Signal(NULL, 0, 0), a(aa), b(bb) {}
    Signal* a; Signal* b;
    virtual double get(double t) { return a->get(t) + b->get(t); }
};

struct Mul : public Signal
{
    Mul(Signal *aa, Signal *bb) : Signal(NULL, 0, 0), a(aa), b(bb) {}
    Signal* a; Signal* b;
    virtual double get(double t) { return a->get(t) * b->get(t); }
};

struct Amp : public Signal
{
    Amp(Signal *s_, double a) : Signal(NULL, a, 0), s(s_) {}
    Signal* s;
    virtual double get(double t) { return freq * s->get(t); }
};

struct Sample
{
    typedef union {
        struct {
            char hi; 
            char lo; 
        } b;
        short s;
    } __Bytes;
    static const int INTERVAL = (1 << (8*sizeof(__Bytes)-1)) - 1;

    Sample(double length = 1., size_t rate_ = 44000)
        : bytes(rate_ * length), rate(rate_)
    {
        samples = new __Bytes[bytes];
        bytes *= sizeof(__Bytes);
    }
    ~Sample() { 
        delete[] samples; 
    }
    void fill(struct Signal &s)
    {
        int t = 0;
        for (size_t i = 0; i < bytes / sizeof(__Bytes); ++i, ++t) {
            double amp = clamp(s.get(static_cast<double>(t) / rate));
            samples[i].s = amp * INTERVAL;
        }
    }

    __Bytes* samples;
    size_t bytes;
    size_t rate;
};

struct Sinus : public Generator
{ virtual double get(double p) { return sin(p * 2 * 3.14159265); } };

struct Triangle : public Generator
{ virtual double get(double p) { return (p<=.5) ? (4.*p - 1.) : (3. - 4*p); } };

struct Saw : public Generator 
{ virtual double get(double p) { return 2.*p - 1.; } };

struct WhiteNoise : public Generator
{ virtual double get(double) { return (rand() % (1 << 15)) / double(1 << 14) - 1.; } }; // value in [-1,1]

struct BrownNoise : public Generator
{ 
    BrownNoise() : last(0) {}
    double last;
    WhiteNoise wn;
    virtual double get(double) { 
        double tmp = clamp(last + wn.get(0.));
        last = tmp;
        return tmp;
    }
};

size_t sem = 0;
boost::mutex sem_lock;
bool stop = false;

Sinus g_sin;
Triangle g_tri;
Saw g_saw;
WhiteNoise g_white;
BrownNoise g_brown;

void player(double freq, double length, Generator *gen)
{
    {boost::mutex::scoped_lock l(sem_lock);
        ++sem;
    }
    ALuint buffer, source;
    alGenBuffers(1, &buffer);
    alGenSources(1, &source);

    Sample s(length, 128000);
    Signal sig(gen, freq);
    Amp amper(&sig, .95);
    Signal sawer(&g_saw, freq);
    Amp ampSaw(&sawer, .05);
    Add adder(&amper, &ampSaw);
    Signal tri(&g_sin, 1.5);
    Mul modulate(&amper, &tri);

    s.fill(modulate);

    alBufferData(buffer, AL_FORMAT_MONO16, s.samples, s.bytes, s.rate);
    alSourcei(source, AL_BUFFER, buffer);
    alSourcePlay(source);

    ALint v;
    do {
        alGetSourcei(source, AL_SOURCE_STATE, &v); 
        boost::thread::yield();
    } while (v == AL_PLAYING && !stop);

    alSourcei(source, AL_BUFFER, NULL);
    alDeleteBuffers(1, &buffer);
    alDeleteSources(1, &source);

    {boost::mutex::scoped_lock l(sem_lock);
        --sem;
    }
}

int main()
{
    // init
    alutInit(0, NULL);
    alGetError();

    // MAGIC!
    const char* prompt_c[] = { "sin", "saw", "triangle", "white", "brown" };
    const char* prompt = prompt_c[0];
    double freq;
    std::string inp;
    Generator *gen = &g_sin;

    bool run = true;
    do {
        std::cout << "[satwb0-9q] (" << prompt << ") > ";
        std::cin >> inp;
        std::stringstream ss(inp);
        ss >> freq;
        switch (inp[0]) {
            case 's': case 'S':
                gen = &g_sin; prompt = prompt_c[0]; break;
            case 'a': case 'A':
                gen = &g_saw; prompt = prompt_c[1]; break;
            case 't': case 'T':
                gen = &g_tri; prompt = prompt_c[2]; break;
            case 'w': case 'W':
                gen = &g_white; prompt = prompt_c[3]; break;
            case 'b': case 'B':
                gen = &g_brown; prompt = prompt_c[4]; break;
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': 
                boost::thread(boost::bind(&player, freq, 5, gen));
                break;
            default: run = false;
        }
    } while (run);

    stop = true;
    while (sem > 0)
        /* loop */;

    // deinit
    alutExit();
}

