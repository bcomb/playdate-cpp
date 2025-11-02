#include "Audio.h"
#include "Globals.h"
#include "SimpleMath.h"
#include "SvgLoader.h"

#include <pd_api.h>
#include <assert.h>
#include <float.h>
#include <vector>
#include <string>
#include <functional>


static PDSynth* s_sine = nullptr;
static PDSynth* s_noise = nullptr;
static PDSynth* s_saw = nullptr;
static PDSynth* s_engine = nullptr;
static PDSynth* s_boost  = nullptr;
static PDSynth* s_shield = nullptr;
static PDSynth* s_click  = nullptr;
static PDSynth* s_missile = nullptr;
static PDSynth* s_impact  = nullptr;
static PDSynth* s_warp    = nullptr;
static PDSynth* s_ui      = nullptr;

static PDSynthEnvelope* s_freqEnv = nullptr; // Modulateur de fréquence pour "laser" (enveloppe)
static PDSynthEnvelope* s_freqEnvUp   = nullptr;  // pitch up
static PDSynthEnvelope* s_freqEnvDown = nullptr;  // pitch down

static inline void setup_amp_adsr(PDSynth* s, float a, float d, float sus, float r)
{
    _G.pd->sound->synth->setAttackTime(s, a);
    _G.pd->sound->synth->setDecayTime(s, d);
    _G.pd->sound->synth->setSustainLevel(s, sus);
    _G.pd->sound->synth->setReleaseTime(s, r);
}

void AudioSfx_Init()
{
    auto snd = _G.pd->sound;

    // Bleep (sinus)
    s_sine = snd->synth->newSynth();
    snd->synth->setWaveform(s_sine, kWaveformSine);
    setup_amp_adsr(s_sine, 0.001f, 0.06f, 0.0f, 0.04f);
    snd->synth->setVolume(s_sine, 0.8f, 0.8f);

    // Explosion / bruit (noise)
    s_noise = snd->synth->newSynth();
    snd->synth->setWaveform(s_noise, kWaveformNoise);
    setup_amp_adsr(s_noise, 0.0f, 0.15f, 0.0f, 0.20f);
    snd->synth->setVolume(s_noise, 0.8f, 0.8f);

    // Laser (scie + sweep de fréquence via enveloppe)
    s_saw = snd->synth->newSynth();
    snd->synth->setWaveform(s_saw, kWaveformSawtooth);
    setup_amp_adsr(s_saw, 0.0f, 0.10f, 0.0f, 0.05f);
    snd->synth->setVolume(s_saw, 0.8f, 0.8f);

    // Enveloppe pour moduler la fréquence (descente rapide)
    s_freqEnv = snd->envelope->newEnvelope(0.0f, 0.09f, 0.0f, 0.0f); // A,D,S,R
    // Amplitude de mod en Hz (valeur négative -> pitch down)
    _G.pd->sound->signal->setValueScale((PDSynthSignal*)s_freqEnv, -1200.0f);
    snd->synth->setFrequencyModulator(s_saw, (PDSynthSignalValue*)s_freqEnv);

    // Enveloppes de pitch (réutilisables)
    s_freqEnvUp = snd->envelope->newEnvelope(0.0f, 0.08f, 0.0f, 0.0f);
    _G.pd->sound->signal->setValueScale((PDSynthSignal*)s_freqEnvUp, +1200.0f); // Hz up-sweep

    s_freqEnvDown = snd->envelope->newEnvelope(0.0f, 0.25f, 0.0f, 0.0f);
    _G.pd->sound->signal->setValueScale((PDSynthSignal*)s_freqEnvDown, -2400.0f); // Hz down-sweep

    // Moteur (burst court selon throttle)
    s_engine = snd->synth->newSynth();
    snd->synth->setWaveform(s_engine, kWaveformTriangle);
    setup_amp_adsr(s_engine, 0.0f, 0.12f, 0.0f, 0.08f);
    snd->synth->setVolume(s_engine, 0.7f, 0.7f);

    // Boost (scie + pitch up)
    s_boost = snd->synth->newSynth();
    snd->synth->setWaveform(s_boost, kWaveformSawtooth);
    setup_amp_adsr(s_boost, 0.0f, 0.10f, 0.0f, 0.08f);
    snd->synth->setVolume(s_boost, 0.8f, 0.8f);
    snd->synth->setFrequencyModulator(s_boost, (PDSynthSignalValue*)s_freqEnvUp);

    // Bouclier: on/off/hit (bruit)
    s_shield = snd->synth->newSynth();
    snd->synth->setWaveform(s_shield, kWaveformNoise);
    setup_amp_adsr(s_shield, 0.02f, 0.20f, 0.0f, 0.25f);
    snd->synth->setVolume(s_shield, 0.9f, 0.9f);

    // Click/impact UI (sin/square)
    s_click = snd->synth->newSynth();
    snd->synth->setWaveform(s_click, kWaveformSine);
    setup_amp_adsr(s_click, 0.0f, 0.03f, 0.0f, 0.02f);
    snd->synth->setVolume(s_click, 0.6f, 0.6f);

    s_impact = snd->synth->newSynth();
    snd->synth->setWaveform(s_impact, kWaveformSquare);
    setup_amp_adsr(s_impact, 0.0f, 0.05f, 0.0f, 0.04f);
    snd->synth->setVolume(s_impact, 0.8f, 0.8f);

    // Missile (sinus + léger up-sweep)
    s_missile = snd->synth->newSynth();
    snd->synth->setWaveform(s_missile, kWaveformSine);
    setup_amp_adsr(s_missile, 0.0f, 0.10f, 0.0f, 0.08f);
    snd->synth->setVolume(s_missile, 0.8f, 0.8f);
    snd->synth->setFrequencyModulator(s_missile, (PDSynthSignalValue*)s_freqEnvUp);

    // Warp (sinus + long down-sweep)
    s_warp = snd->synth->newSynth();
    snd->synth->setWaveform(s_warp, kWaveformSine);
    setup_amp_adsr(s_warp, 0.0f, 0.40f, 0.0f, 0.20f);
    snd->synth->setVolume(s_warp, 0.8f, 0.8f);
    snd->synth->setFrequencyModulator(s_warp, (PDSynthSignalValue*)s_freqEnvDown);

    // UI bips
    s_ui = snd->synth->newSynth();
    snd->synth->setWaveform(s_ui, kWaveformTriangle);
    setup_amp_adsr(s_ui, 0.0f, 0.05f, 0.0f, 0.04f);
    snd->synth->setVolume(s_ui, 0.7f, 0.7f);
}

void AudioSfx_Shutdown()
{
    auto snd = _G.pd->sound;
    if (s_freqEnv) { snd->envelope->freeEnvelope(s_freqEnv); s_freqEnv = nullptr; }
    if (s_saw) { snd->synth->freeSynth(s_saw); s_saw = nullptr; }
    if (s_noise) { snd->synth->freeSynth(s_noise); s_noise = nullptr; }
    if (s_sine) { snd->synth->freeSynth(s_sine); s_sine = nullptr; }

    if (s_freqEnvUp)   { snd->envelope->freeEnvelope(s_freqEnvUp);   s_freqEnvUp = nullptr; }
    if (s_freqEnvDown) { snd->envelope->freeEnvelope(s_freqEnvDown); s_freqEnvDown = nullptr; }

    if (s_ui)     { snd->synth->freeSynth(s_ui); s_ui = nullptr; }
    if (s_warp)   { snd->synth->freeSynth(s_warp); s_warp = nullptr; }
    if (s_missile){ snd->synth->freeSynth(s_missile); s_missile = nullptr; }
    if (s_impact) { snd->synth->freeSynth(s_impact); s_impact = nullptr; }
    if (s_click)  { snd->synth->freeSynth(s_click); s_click = nullptr; }
    if (s_shield) { snd->synth->freeSynth(s_shield); s_shield = nullptr; }
    if (s_boost)  { snd->synth->freeSynth(s_boost); s_boost = nullptr; }
    if (s_engine) { snd->synth->freeSynth(s_engine); s_engine = nullptr; }
}

void Sfx_Bleep(float midi = NOTE_C4 + 12, float vel = 1.0f)
{
    if (!s_sine) return;
    float f = pd_noteToFrequency(midi);
    _G.pd->sound->synth->playNote(s_sine, f, vel, 0.08f, 0); // len en secondes (<=0 pour sustain)
}

void Sfx_Explosion(float vel = 1.0f)
{
    if (!s_noise) return;
    // fréquence "porteuse" peu importante sur noise; len court
    _G.pd->sound->synth->playNote(s_noise, 200.0f, vel, 0.25f, 0);
}

void Sfx_Laser(float baseMidi = NOTE_C4 + 24, float vel = 1.0f)
{
    if (!s_saw) return;
    float f = pd_noteToFrequency(baseMidi);
    _G.pd->sound->synth->playNote(s_saw, f, vel, 0.12f, 0);
}

// Thruster burst selon "throttle" (0..1)
void Sfx_EngineBurst(float throttle)
{
    if (!s_engine) return;
    throttle = clamp(throttle, 0.0f, 1.0f);
    float baseHz = 90.0f + 220.0f * throttle;
    _G.pd->sound->synth->playNote(s_engine, baseHz, 0.9f, 0.16f, 0);
}

// Boost court (whoosh)
void Sfx_Boost()
{
    if (!s_boost) return;
    _G.pd->sound->synth->playNote(s_boost, 220.0f, 1.0f, 0.15f, 0);
}

// Bouclier: on/off et hit
void Sfx_ShieldOn()
{
    if (!s_shield) return;
    _G.pd->sound->synth->playNote(s_shield, 400.0f, 0.8f, 0.35f, 0);
}
void Sfx_ShieldOff()
{
    if (!s_shield) return;
    _G.pd->sound->synth->playNote(s_shield, 250.0f, 0.8f, 0.30f, 0);
}
void Sfx_ShieldHit()
{
    if (!s_shield) return;
    _G.pd->sound->synth->playNote(s_shield, 700.0f, 1.0f, 0.12f, 0);
}

// Lancer de missile
void Sfx_Missile()
{
    if (!s_missile) return;
    _G.pd->sound->synth->playNote(s_missile, 520.0f, 0.9f, 0.12f, 0);
}

// Impact/ricochet
void Sfx_Impact()
{
    if (!s_impact) return;
    _G.pd->sound->synth->playNote(s_impact, 320.0f, 0.9f, 0.10f, 0);
}

// Warp/hyper-espace
void Sfx_Warp()
{
    if (!s_warp) return;
    _G.pd->sound->synth->playNote(s_warp, 880.0f, 1.0f, 0.6f, 0);
}

// Pickups/powerups
void Sfx_Pickup()
{
    if (!s_sine) return;
    _G.pd->sound->synth->playNote(s_sine, pd_noteToFrequency(NOTE_C4 + 7), 0.9f, 0.08f, 0);
    _G.pd->sound->synth->playNote(s_sine, pd_noteToFrequency(NOTE_C4 + 12), 0.9f, 0.10f, 0);
}
void Sfx_Powerup()
{
    if (!s_saw) return;
    _G.pd->sound->synth->playNote(s_saw, pd_noteToFrequency(NOTE_C4), 0.8f, 0.10f, 0);
    _G.pd->sound->synth->playNote(s_saw, pd_noteToFrequency(NOTE_C4 + 4), 0.8f, 0.12f, 0);
    _G.pd->sound->synth->playNote(s_saw, pd_noteToFrequency(NOTE_C4 + 7), 0.8f, 0.14f, 0);
}

// UI
void Sfx_UIClick()    { if (s_click) _G.pd->sound->synth->playNote(s_click, 1200.0f, 0.6f, 0.04f, 0); }
void Sfx_UIMove()     { if (s_ui)    _G.pd->sound->synth->playNote(s_ui, pd_noteToFrequency(NOTE_C4+2), 0.7f, 0.06f, 0); }
void Sfx_UIBack()     { if (s_ui)    _G.pd->sound->synth->playNote(s_ui, pd_noteToFrequency(NOTE_C4-3), 0.7f, 0.06f, 0); }
void Sfx_UIConfirm()  { if (s_ui)    _G.pd->sound->synth->playNote(s_ui, pd_noteToFrequency(NOTE_C4+7), 0.8f, 0.08f, 0); }

// [REMPLACER l'initialisation du vector 'sounds' dans test() par ceci]
static std::vector< std::pair<std::string, std::function<void(void)> > > sounds = {
    {"Bleep",      [](){ Sfx_Bleep(); }},
    {"Explosion",  [](){ Sfx_Explosion(); }},
    {"Laser",      [](){ Sfx_Laser(); }},

    {"Engine0",     [](){ Sfx_EngineBurst(0); }},
    {"Engine25",     []() { Sfx_EngineBurst(0.25); }},
    {"Engine50",     []() { Sfx_EngineBurst(0.5); }},
    {"Engine75",     []() { Sfx_EngineBurst(0.75); }},
    {"Engine100",     []() { Sfx_EngineBurst(1); }},
    {"Boost",      [](){ Sfx_Boost(); }},
    {"ShieldOn",   [](){ Sfx_ShieldOn(); }},
    {"ShieldOff",  [](){ Sfx_ShieldOff(); }},
    {"ShieldHit",  [](){ Sfx_ShieldHit(); }},
    {"Missile",    [](){ Sfx_Missile(); }},
    {"Impact",     [](){ Sfx_Impact(); }},
    {"Warp",       [](){ Sfx_Warp(); }},
    {"Pickup",     [](){ Sfx_Pickup(); }},
    {"Powerup",    [](){ Sfx_Powerup(); }},

    {"UI Click",   [](){ Sfx_UIClick(); }},
    {"UI Move",    [](){ Sfx_UIMove(); }},
    {"UI Back",    [](){ Sfx_UIBack(); }},
    {"UI Confirm", [](){ Sfx_UIConfirm(); }},
};


void test(float t)
{
    PlaydateAPI* pd = _G.pd;
    static bool sInit = false;
    static int soundIndex = 0;
    if (!sInit)
    {
        sInit = true;
        AudioSfx_Init();
    }

    PDButtons current, pushed, released;
    pd->system->getButtonState(&current, &pushed, &released);
    if (pushed & kButtonUp)
    {
        soundIndex = ++soundIndex % sounds.size();
        sounds[soundIndex].second();
    }

    if (pushed & kButtonDown)
    {
        if(soundIndex == 0)
            soundIndex = (int)sounds.size() - 1;
        --soundIndex;

        sounds[soundIndex].second();
    }

    if(current & kButtonA)
    {
        sounds[soundIndex].second();
    }

    pd->graphics->clear(kColorWhite);
    
    pd->graphics->drawText(sounds[soundIndex].first.c_str(), strlen(sounds[soundIndex].first.c_str()), kASCIIEncoding, 10, 10);
}