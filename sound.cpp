#include <iostream>
#include <sstream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <AL/alut.h>

size_t NUM_SPEAKERS = 3;
ALuint *buffer;
ALuint *source;

struct Sample
{
    typedef union {
        struct {
            char hi; 
            char lo; 
        } b;
        short s;
    } __Bytes;
    static const int INTERVAL = (1 << (8*sizeof(__Bytes)));
    static const int INTERVAL_HALF = (INTERVAL >> 1);

    Sample(double freq_, double length = 1., size_t rate_ = 44000)
        : freq(freq_), bytes(rate_ * length), rate(rate_)
    {
        srand(freq * time(NULL));
        samples = new __Bytes[bytes];
        bytes *= sizeof(__Bytes);
    }
    ~Sample() { 
        delete[] samples; 
    }
    void fill()
    {
        for (size_t i = 0; i < bytes / sizeof(__Bytes); ++i, ++t)
            samples[i].s = next();
    }
    virtual int next() = 0;
    int bound(int v) 
    { return v > INTERVAL_HALF ? INTERVAL_HALF : (v < -INTERVAL_HALF ? -INTERVAL_HALF : v); }

    __Bytes* samples;
    double freq;
    size_t bytes;
    size_t rate;
    int t;
};

struct Generator
{
    virtual int pcm(int t, int T) = 0;
}

struct Triangle : public Sample
{
    Triangle(double f, double l, size_t r) : Sample(f,l,r), up(true) {}
    bool up;
    virtual int next()
    {
        double frac = t * freq / rate;
        int v = (up ? -1 : 1) * (frac * INTERVAL - INTERVAL_HALF);
        if (frac >= 1.0) {
            up = !up;
            t = 0;
            v = up ? INTERVAL_HALF : -INTERVAL_HALF; // value might be out of bounds. fix this
        }
        return v;
    }
};

struct Sinus : public Sample
{
    Sinus(double f, double l, size_t r) : Sample(f,l,r) {}
    virtual int next()
    {
        double frac = t * freq / rate * 2 * 3.14159265;
        return sin(frac) * INTERVAL_HALF;
    }
};

struct WhiteNoise : public Sample
{
    WhiteNoise(double f, double l, size_t r) : Sample(f,l,r) {}
    virtual int next()
    {
        return bound(rand() % INTERVAL - INTERVAL_HALF);
    }
};

struct BrownNoise : public Sample 
{
    BrownNoise(double f, double l, size_t r) : Sample(f,l,r), last(0) {}
    int last;
    virtual int next()
    {
        int v = bound(last + rand() % INTERVAL - INTERVAL_HALF);
        last = v;
        return v;
    }
};

int main()
{
    // init
    alutInit(0, NULL);
    alGetError();

    buffer = new ALuint[NUM_SPEAKERS];
    alGenBuffers(NUM_SPEAKERS, buffer);
    source = new ALuint[NUM_SPEAKERS];
    alGenSources(NUM_SPEAKERS, source);

    size_t rate = 64000;

    // MAGIC!
    enum { SIN = 0, TRIANGLE, WHITE_NOISE, BROWN_NOISE } type = SIN;
    const char* prompt[] = { "sin > ", "triangle > ", "white > ", "brown > " };
    const char* allowed = "0123456789stwb";
    double freq;
    std::string inp;
    do {
        std::cout << prompt[type];
        std::cin >> inp;
        std::stringstream ss(inp);
        ss >> freq;
        if (inp[0] == 's') type = SIN;
        else if (inp[0] == 't') type = TRIANGLE;
        else if (inp[0] == 'w') type = WHITE_NOISE;
        else if (inp[0] == 'b') type = BROWN_NOISE;
        else { 
            alSourceStop(source[0]);
            alSourceStop(source[1]);
            alSourcei(source[0], AL_BUFFER, 0);
            alSourcei(source[1], AL_BUFFER, 0);

            Sample *s;
            for (int i = 0; i < 2; ++i) {
                freq += i * freq;
                switch (type) {
                    case SIN:
                        s = new Sinus(freq, 12.5, rate); break;
                    case TRIANGLE:
                        s = new Triangle(freq, 12.5, rate); break;
                    case WHITE_NOISE:
                        s = new WhiteNoise(freq, 12.5, rate); break;
                    case BROWN_NOISE:
                    default:
                        s = new BrownNoise(freq, 12.5, rate); break;
                }
                s->fill();

                alBufferData(buffer[i], AL_FORMAT_MONO16, s->samples, s->bytes, s->rate);
                delete s;

                alSourcei(source[i], AL_BUFFER, buffer[i]);
            }
            alSourcePlay(source[0]);
            alSourcePlay(source[1]);
        }
    } while (freq > 0 && (inp.find_first_of(allowed) != std::string::npos));

    // deinit
    alSourceStopv(NUM_SPEAKERS, source);
    for (size_t i = 0; i < NUM_SPEAKERS; ++i)
        alSourcei(source[i], AL_BUFFER, NULL);
    alDeleteBuffers(NUM_SPEAKERS, buffer);
    alDeleteSources(NUM_SPEAKERS, source);
    alutExit();

    delete[] buffer;
    delete[] source;
}

