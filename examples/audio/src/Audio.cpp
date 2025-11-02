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

// Jet engine layers
static PDSynth* s_jetRumble = nullptr; // low turbine rumble
static PDSynth* s_jetWhine  = nullptr; // turbine whine
static PDSynth* s_jetRoar   = nullptr; // broadband roar (noise)

static PDSynthEnvelope* s_freqEnv = nullptr; // Modulateur de fréquence pour "laser" (enveloppe)
static PDSynthEnvelope* s_freqEnvUp   = nullptr;  // pitch up
static PDSynthEnvelope* s_freqEnvDown = nullptr;  // pitch down

// Crackle
static PDSynth*   s_crackle    = nullptr;
static PDSynthLFO* s_crackleLFO = nullptr;

// Canal et effets dédiés au crackle (pour ne pas filtrer le reste)
static SoundChannel*   s_crackleCh  = nullptr;
static TwoPoleFilter*  s_crackleBP  = nullptr; // band-pass 2-4 kHz
static Overdrive*      s_crackleOD  = nullptr; // léger grain


// Add near other globals (below s_crackleLFO)
static PDSynthLFO* s_hissLFO = nullptr; // sine LFO for soft hiss

// --- Wind (vent) ---
static PDSynth*       s_wind      = nullptr;
static SoundChannel*  s_windCh    = nullptr;
static TwoPoleFilter* s_windBP    = nullptr;   // band-pass doux
static PDSynthLFO*    s_windAmpLFO  = nullptr; // LFO sin pour amplitude
static PDSynthLFO*    s_windGustLFO = nullptr; // LFO tri pour « rafales »


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

    // Jet engine layers
    s_jetRumble = snd->synth->newSynth();
    snd->synth->setWaveform(s_jetRumble, kWaveformTriangle);
    setup_amp_adsr(s_jetRumble, 0.0f, 0.12f, 0.0f, 0.08f);
    snd->synth->setVolume(s_jetRumble, 0.9f, 0.9f);

    s_jetWhine = snd->synth->newSynth();
    snd->synth->setWaveform(s_jetWhine, kWaveformSine);
    setup_amp_adsr(s_jetWhine, 0.0f, 0.10f, 0.0f, 0.08f);
    snd->synth->setVolume(s_jetWhine, 0.9f, 0.9f);

    s_jetRoar = snd->synth->newSynth();
    snd->synth->setWaveform(s_jetRoar, kWaveformNoise);
    setup_amp_adsr(s_jetRoar, 0.0f, 0.18f, 0.0f, 0.20f);
    snd->synth->setVolume(s_jetRoar, 0.9f, 0.9f);

    // Crackle: bruit blanc + LFO Sample&Hold sur l'amplitude
    s_crackle = snd->synth->newSynth();
    snd->synth->setWaveform(s_crackle, kWaveformNoise);
    // enveloppe courte pour des bursts nerveux
    setup_amp_adsr(s_crackle, 0.0f, 0.10f, 0.0f, 0.10f);
    snd->synth->setVolume(s_crackle, 0.9f, 0.9f);

    s_crackleLFO = snd->lfo->newLFO(kLFOTypeSampleAndHold);
    snd->lfo->setRate(s_crackleLFO, 25.0f);      // vitesse de crépitement
    snd->lfo->setCenter(s_crackleLFO, 0.35f);    // offset d’amplitude
    snd->lfo->setDepth(s_crackleLFO, 0.65f);     // profondeur (0..1)
    snd->synth->setAmplitudeModulator(s_crackle, (PDSynthSignalValue*)s_crackleLFO);

    // Canal et effets dédiés au crackle (pour ne pas filtrer le reste)
    s_crackleCh = snd->channel->newChannel();
    snd->channel->setVolume(s_crackleCh, 0.6f);            // niveau global du canal
    snd->addChannel(s_crackleCh);
    // Route le synth crackle vers ce canal
    snd->channel->addSource(s_crackleCh, (SoundSource*)s_crackle);

    // Band-pass 2–4 kHz
    s_crackleBP = snd->effect->twopolefilter->newFilter();
    snd->effect->twopolefilter->setType(s_crackleBP, kFilterTypeBandPass);
    snd->effect->twopolefilter->setFrequency(s_crackleBP, 3000.0f);
    snd->effect->twopolefilter->setResonance(s_crackleBP, 0.5f);
    // Overdrive léger
    s_crackleOD = snd->effect->overdrive->newOverdrive();
    snd->effect->overdrive->setGain(s_crackleOD, 0.6f);
    snd->effect->overdrive->setLimit(s_crackleOD, 0.9f);

    // Ajouter les effets au canal (ordre: filtre puis overdrive)
    snd->channel->addEffect(s_crackleCh, (SoundEffect*)s_crackleBP);
    snd->channel->addEffect(s_crackleCh, (SoundEffect*)s_crackleOD);

    // Mixer des effets (facultatif)
    snd->effect->setMix((SoundEffect*)s_crackleBP, 1.0f);
    snd->effect->setMix((SoundEffect*)s_crackleOD, 0.6f);


    // Add inside AudioSfx_Init(), after s_crackleLFO init
    // Soft hiss LFO (sine) — subtle amplitude modulation
    s_hissLFO = snd->lfo->newLFO(kLFOTypeSine);
    snd->lfo->setRate(s_hissLFO, 5.0f);
    snd->lfo->setCenter(s_hissLFO, 0.7f);
    snd->lfo->setDepth(s_hissLFO, 0.15f);

    // --- Wind (vent) ---
    s_wind = snd->synth->newSynth();
    snd->synth->setWaveform(s_wind, kWaveformNoise);
    // ADSR doux (attaques lisses)
    setup_amp_adsr(s_wind, 0.02f, 0.30f, 0.0f, 0.30f);
    snd->synth->setVolume(s_wind, 0.9f, 0.9f);

    // Canal dédié au vent
    s_windCh = snd->channel->newChannel();
    snd->channel->setVolume(s_windCh, 0.7f);
    snd->addChannel(s_windCh);
    snd->channel->addSource(s_windCh, (SoundSource*)s_wind);

    // Filtrage band‑pass doux (centre ~2.2 kHz, faible résonance)
    s_windBP = snd->effect->twopolefilter->newFilter();
    snd->effect->twopolefilter->setType(s_windBP, kFilterTypeBandPass);
    snd->effect->twopolefilter->setFrequency(s_windBP, 2200.0f);
    snd->effect->twopolefilter->setResonance(s_windBP, 0.2f);
    snd->channel->addEffect(s_windCh, (SoundEffect*)s_windBP);
    snd->effect->setMix((SoundEffect*)s_windBP, 1.0f);

    // LFO amplitude (sin) : léger « souffle »
    s_windAmpLFO = snd->lfo->newLFO(kLFOTypeSine);
    snd->lfo->setRate(s_windAmpLFO, 0.7f);   // 0.5–1 Hz typique
    snd->lfo->setCenter(s_windAmpLFO, 0.7f); // niveau moyen
    snd->lfo->setDepth(s_windAmpLFO, 0.18f); // variation douce
    snd->synth->setAmplitudeModulator(s_wind, (PDSynthSignalValue*)s_windAmpLFO);

    // LFO « rafales » (triangle) -> modulation de la fréquence du filtre
    s_windGustLFO = snd->lfo->newLFO(kLFOTypeTriangle);
    snd->lfo->setRate(s_windGustLFO, 0.15f); // période lente (6–8 s)
    snd->lfo->setCenter(s_windGustLFO, 0.0f);
    snd->lfo->setDepth(s_windGustLFO, 1.0f);
    // Échelle de la mod en Hz pour la fréquence du filtre (±1200 Hz)
    snd->signal->setValueScale((PDSynthSignal*)s_windGustLFO, 1200.0f);
    snd->effect->twopolefilter->setFrequencyModulator(s_windBP, (PDSynthSignalValue*)s_windGustLFO);
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

    if (s_jetRoar)   { snd->synth->freeSynth(s_jetRoar);   s_jetRoar = nullptr; }
    if (s_jetWhine)  { snd->synth->freeSynth(s_jetWhine);  s_jetWhine = nullptr; }
    if (s_jetRumble) { snd->synth->freeSynth(s_jetRumble); s_jetRumble = nullptr; }
    if (s_crackleLFO){ snd->lfo->freeLFO(s_crackleLFO); s_crackleLFO = nullptr; }
    if (s_crackle)   { snd->synth->freeSynth(s_crackle); s_crackle = nullptr; }

    // Canal et effets dédiés au crackle (pour ne pas filtrer le reste)
    if (s_crackleCh) {
        // retirer la source et effets avant free
        if (s_crackle)   snd->channel->removeSource(s_crackleCh, (SoundSource*)s_crackle);
        if (s_crackleOD) snd->channel->removeEffect(s_crackleCh, (SoundEffect*)s_crackleOD);
        if (s_crackleBP) snd->channel->removeEffect(s_crackleCh, (SoundEffect*)s_crackleBP);
        snd->removeChannel(s_crackleCh);
        snd->channel->freeChannel(s_crackleCh);
        s_crackleCh = nullptr;
    }
    if (s_crackleOD) { snd->effect->overdrive->freeOverdrive(s_crackleOD); s_crackleOD = nullptr; }
    if (s_crackleBP) { snd->effect->twopolefilter->freeFilter(s_crackleBP); s_crackleBP = nullptr; }
    if (s_hissLFO) { snd->lfo->freeLFO(s_hissLFO); s_hissLFO = nullptr; }

    // Vent
    if (s_windCh) {
        if (s_wind) snd->channel->removeSource(s_windCh, (SoundSource*)s_wind);
        if (s_windBP) snd->channel->removeEffect(s_windCh, (SoundEffect*)s_windBP);
        snd->removeChannel(s_windCh);
        snd->channel->freeChannel(s_windCh);
        s_windCh = nullptr;
    }
    if (s_windBP)      { snd->effect->twopolefilter->freeFilter(s_windBP); s_windBP = nullptr; }
    if (s_windAmpLFO)  { snd->lfo->freeLFO(s_windAmpLFO); s_windAmpLFO = nullptr; }
    if (s_windGustLFO) { snd->lfo->freeLFO(s_windGustLFO); s_windGustLFO = nullptr; }
    if (s_wind)        { snd->synth->freeSynth(s_wind); s_wind = nullptr; }
}

// One-shot "rafale" de vent modulée par strength ∈ [0..1]
void Sfx_WindGust(float strength)
{
    if (!s_wind || !s_windBP || !s_windAmpLFO || !s_windGustLFO) return;
    strength = clamp(strength, 0.0f, 1.0f);

    // Center and Q
    float centerHz = mapRange(strength, 0.0f, 1.0f, 1600.0f, 3200.0f);
    float res      = mix(0.15f, 0.30f, strength);
    _G.pd->sound->effect->twopolefilter->setFrequency(s_windBP, centerHz);
    _G.pd->sound->effect->twopolefilter->setResonance(s_windBP, res);

    // Amplitude LFO
    _G.pd->sound->lfo->setRate(s_windAmpLFO, mix(0.5f, 1.2f, strength));
    _G.pd->sound->lfo->setDepth(s_windAmpLFO, mix(0.12f, 0.25f, strength));
    _G.pd->sound->lfo->setCenter(s_windAmpLFO, mix(0.55f, 0.85f, strength));

    // Gust LFO -> clamp modulation span so frequency stays in [minHz, maxHz]
    _G.pd->sound->lfo->setRate(s_windGustLFO, mix(0.10f, 0.30f, strength));
    const float desiredSpan = mix(800.0f, 1600.0f, strength); // intended ±Hz
    const float minHz = 120.0f, maxHz = 8000.0f;
    float maxUp   = maxHz - centerHz;
    float maxDown = centerHz - minHz;
    float safeSpan = fminf(desiredSpan, fminf(maxUp, maxDown));
    if (safeSpan < 0.0f) safeSpan = 0.0f;
    _G.pd->sound->signal->setValueScale((PDSynthSignal*)s_windGustLFO, safeSpan);

    // ADSR
    _G.pd->sound->synth->setAttackTime(s_wind, mix(0.02f, 0.05f, strength));
    _G.pd->sound->synth->setDecayTime(s_wind,  mix(0.30f, 0.55f, strength));
    _G.pd->sound->synth->setSustainLevel(s_wind, 0.0f);
    _G.pd->sound->synth->setReleaseTime(s_wind, mix(0.25f, 0.45f, strength));

    float len = mix(0.50f, 1.10f, strength);
    float vel = mix(0.55f, 0.95f, strength);
    _G.pd->sound->synth->playNote(s_wind, 200.0f, vel, len, 0);
}

void WindLoop_Update(float strength)
{
    if (!s_wind || !s_windBP || !s_windAmpLFO || !s_windGustLFO) return;
    strength = clamp(strength, 0.0f, 1.0f);

    float centerHz = mapRange(strength, 0.0f, 1.0f, 1400.0f, 3400.0f);
    float res      = mix(0.15f, 0.30f, strength);
    _G.pd->sound->effect->twopolefilter->setFrequency(s_windBP, centerHz);
    _G.pd->sound->effect->twopolefilter->setResonance(s_windBP, res);

    _G.pd->sound->lfo->setRate(s_windAmpLFO,  mix(0.4f, 1.2f, strength));
    _G.pd->sound->lfo->setDepth(s_windAmpLFO, mix(0.10f, 0.28f, strength));
    _G.pd->sound->lfo->setCenter(s_windAmpLFO, mix(0.55f, 0.90f, strength));

    _G.pd->sound->lfo->setRate(s_windGustLFO, mix(0.08f, 0.35f, strength));
    const float desiredSpan = mix(600.0f, 1800.0f, strength);
    const float minHz = 120.0f, maxHz = 8000.0f;
    float maxUp   = maxHz - centerHz;
    float maxDown = centerHz - minHz;
    float safeSpan = fminf(desiredSpan, fminf(maxUp, maxDown));
    if (safeSpan < 0.0f) safeSpan = 0.0f;
    _G.pd->sound->signal->setValueScale((PDSynthSignal*)s_windGustLFO, safeSpan);

    _G.pd->sound->channel->setVolume(s_windCh, mix(0.5f, 0.9f, strength));
}

// Boucle de vent: démarrer/mettre à jour/arrêter (pour usage in-game)
void WindLoop_Start(float strength)
{
    if (!s_wind) return;
    // Note infinie (len = -1) contrôlée par LFO + filtre
    _G.pd->sound->synth->playNote(s_wind, 200.0f, 0.8f, -1.0f, 0);
    WindLoop_Update(strength);
}

void WindLoop_Stop()
{
    if (!s_wind) return;
    _G.pd->sound->synth->stop(s_wind);
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


// Map thrust [0..1] to a composite jet-engine sound
void Sfx_JetEngineThrust(float thrust)
{
    if (!s_jetRumble || !s_jetWhine || !s_jetRoar) return;

    thrust = clamp(thrust, 0.0f, 1.0f);

    // Frequencies
    float rumbleHz = mapRange(thrust, 0.0f, 1.0f, 70.0f, 140.0f);
    float whineHz = mapRange(thrust, 0.0f, 1.0f, 500.0f, 2000.0f);

    // Relative loudness
    float rumbleVel = mix(0.45f, 0.95f, thrust);
    float whineVel = mix(0.25f, 1.00f, thrust);
    float roarVel = mix(0.15f, 0.85f, thrust);

    // One-shot burst (preview style)
    float len = 0.18f;
    _G.pd->sound->synth->playNote(s_jetRumble, rumbleHz, rumbleVel, len, 0);
    _G.pd->sound->synth->playNote(s_jetWhine, whineHz, whineVel, len, 0);
    _G.pd->sound->synth->playNote(s_jetRoar, 200.0f, roarVel, len, 0); // freq ignored for noise
}

// Presets
inline void Sfx_JetIdle() { Sfx_JetEngineThrust(0.10f); }
inline void Sfx_JetTaxi() { Sfx_JetEngineThrust(0.30f); }
inline void Sfx_JetCruise() { Sfx_JetEngineThrust(0.60f); }
inline void Sfx_JetTakeoff() { Sfx_JetEngineThrust(0.90f); }
inline void Sfx_JetAfterburner() { Sfx_JetEngineThrust(1.00f); }


void Sfx_HissGraze(float strength)
{
    if (!s_crackle || !s_crackleCh || !s_crackleBP || !s_hissLFO) return;
    strength = clamp(strength, 0.0f, 1.0f);

    // Use a gentle band-pass (low resonance) for a pleasant hiss
    _G.pd->sound->effect->twopolefilter->setType(s_crackleBP, kFilterTypeBandPass);
    float freq = mapRange(strength, 0.0f, 1.0f, 2200.0f, 4200.0f);
    float res  = mix(0.15f, 0.35f, strength);
    _G.pd->sound->effect->twopolefilter->setFrequency(s_crackleBP, freq);
    _G.pd->sound->effect->twopolefilter->setResonance(s_crackleBP, res);
    _G.pd->sound->effect->setMix((SoundEffect*)s_crackleBP, 0.9f);

    // Disable overdrive for a soft result
    if (s_crackleOD) _G.pd->sound->effect->setMix((SoundEffect*)s_crackleOD, 0.0f);

    // Sine LFO for subtle flutter (no harsh gating)
    _G.pd->sound->lfo->setRate(s_hissLFO, mapRange(strength, 0.0f, 1.0f, 3.0f, 9.0f));
    _G.pd->sound->lfo->setDepth(s_hissLFO, mix(0.10f, 0.25f, strength));
    _G.pd->sound->lfo->setCenter(s_hissLFO, mix(0.55f, 0.85f, strength));
    _G.pd->sound->synth->setAmplitudeModulator(s_crackle, (PDSynthSignalValue*)s_hissLFO);

    // Smooth ADSR
    _G.pd->sound->synth->setAttackTime(s_crackle, 0.01f);
    _G.pd->sound->synth->setDecayTime(s_crackle, mix(0.10f, 0.18f, strength));
    _G.pd->sound->synth->setSustainLevel(s_crackle, 0.0f);
    _G.pd->sound->synth->setReleaseTime(s_crackle, mix(0.12f, 0.22f, strength));

    float len = mix(0.14f, 0.28f, strength);
    float vel = mix(0.45f, 0.85f, strength);
    _G.pd->sound->synth->playNote(s_crackle, 200.0f, vel, len, 0);
}

// Presets pour menu
inline void Sfx_HissSoft()  { Sfx_HissGraze(0.25f); }
inline void Sfx_HissMid()   { Sfx_HissGraze(0.55f); }
inline void Sfx_HissBoost() { Sfx_HissGraze(0.85f); }

//# Crépitement: intensity [0..1], duration en secondes
void Sfx_Crackle(float intensity = 0.7f, float duration = 0.25f)
{
    if (!s_crackle || !s_crackleLFO) return;
    intensity = clamp(intensity, 0.0f, 1.0f);

    // Adapter le caractère du crépitement
    float rate = mapRange(intensity, 0.0f, 1.0f, 18.0f, 60.0f);
    float depth = mix(0.45f, 0.95f, intensity);
    float center = mix(0.30f, 0.50f, intensity);

    _G.pd->sound->lfo->setRate(s_crackleLFO, rate);
    _G.pd->sound->lfo->setDepth(s_crackleLFO, depth);
    _G.pd->sound->lfo->setCenter(s_crackleLFO, center);

    // Note « porteuse » peu importante en noise; len contrôle la durée
    _G.pd->sound->synth->playNote(s_crackle, 200.0f, 0.9f, duration, 0);
}

// Presets rapides
inline void Sfx_CrackleLight() { Sfx_Crackle(0.35f, 0.18f); }
inline void Sfx_CrackleMedium() { Sfx_Crackle(0.70f, 0.25f); }
inline void Sfx_CrackleHeavy() { Sfx_Crackle(1.00f, 0.35f); }

// Crépitement doux lors d'un frottement/boost. strength ∈ [0..1]
void Sfx_GrazeCrackle(float strength)
{
    if (!s_crackle || !s_crackleLFO || !s_crackleBP || !s_crackleOD) return;
    strength = clamp(strength, 0.0f, 1.0f);

    // Densité de crépitement (S&H)
    float rate   = mapRange(strength, 0.0f, 1.0f, 12.0f, 90.0f);
    float depth  = mix(0.35f, 1.00f, strength);
    float center = mix(0.25f, 0.65f, strength);
    _G.pd->sound->lfo->setRate(s_crackleLFO, rate);
    _G.pd->sound->lfo->setDepth(s_crackleLFO, depth);
    _G.pd->sound->lfo->setCenter(s_crackleLFO, center);

    // Filtre band‑pass et résonance selon la force du frottement
    float bpFreq = mapRange(strength, 0.0f, 1.0f, 2200.0f, 3800.0f);
    float bpQ    = mix(0.35f, 0.75f, strength);
    _G.pd->sound->effect->twopolefilter->setFrequency(s_crackleBP, bpFreq);
    _G.pd->sound->effect->twopolefilter->setResonance(s_crackleBP, bpQ);

    // Overdrive plus prononcé avec la force
    float odGain = mix(0.45f, 0.85f, strength);
    _G.pd->sound->effect->overdrive->setGain(s_crackleOD, odGain);

    // ADSR court et satisfaisant
    _G.pd->sound->synth->setAttackTime(s_crackle, 0.0f);
    _G.pd->sound->synth->setDecayTime(s_crackle, mix(0.10f, 0.28f, strength));
    _G.pd->sound->synth->setSustainLevel(s_crackle, 0.0f);
    _G.pd->sound->synth->setReleaseTime(s_crackle, mix(0.10f, 0.24f, strength));

    // Déclenchement (la fréquence est ignorée pour noise)
    float len = mix(0.14f, 0.30f, strength);
    _G.pd->sound->synth->playNote(s_crackle, 200.0f, mix(0.7f, 1.0f, strength), len, 0);

    // Petit "spark" très court si boost fort (accent satisfaisant, discret)
    if (strength > 0.6f && s_click)
        _G.pd->sound->synth->playNote(s_click, 3200.0f, 0.5f, 0.02f, 0);
}

// Presets pour test
inline void Sfx_GrazeSoft()  { Sfx_GrazeCrackle(0.25f); }
inline void Sfx_GrazeMid()   { Sfx_GrazeCrackle(0.55f); }
inline void Sfx_GrazeBoost() { Sfx_GrazeCrackle(0.85f); }

// Remplacer l'initialisation du vector 'sounds' par cette version étendue (ajoute le vent)
static std::vector< std::pair<std::string, std::function<void(void)> > > sounds = {
    {"Bleep",      [](){ Sfx_Bleep(); }},
    {"Explosion",  [](){ Sfx_Explosion(); }},
    {"Laser",      [](){ Sfx_Laser(); }},

    {"Engine0",    [](){ Sfx_EngineBurst(0); }},
    {"Engine25",   [](){ Sfx_EngineBurst(0.25f); }},
    {"Engine50",   [](){ Sfx_EngineBurst(0.5f); }},
    {"Engine75",   [](){ Sfx_EngineBurst(0.75f); }},
    {"Engine100",  [](){ Sfx_EngineBurst(1.0f); }},

    {"Jet Idle",        [](){ Sfx_JetIdle(); }},
    {"Jet Taxi",        [](){ Sfx_JetTaxi(); }},
    {"Jet Cruise",      [](){ Sfx_JetCruise(); }},
    {"Jet Takeoff",     [](){ Sfx_JetTakeoff(); }},
    {"Jet Afterburner", [](){ Sfx_JetAfterburner(); }},

    // Vent (rafales one-shot)
    {"Wind Light",   [](){ Sfx_WindGust(0.25f); }},
    {"Wind Medium",  [](){ Sfx_WindGust(0.60f); }},
    {"Wind Strong",  [](){ Sfx_WindGust(1.00f); }},

    // Hiss/Crackle existants
    {"Hiss Soft",   [](){ Sfx_HissSoft(); }},
    {"Hiss Mid",    [](){ Sfx_HissMid(); }},
    {"Hiss Boost",  [](){ Sfx_HissBoost(); }},

    {"Crackle Light",   [](){ Sfx_CrackleLight(); }},
    {"Crackle Medium",  [](){ Sfx_CrackleMedium(); }},
    {"Crackle Heavy",   [](){ Sfx_CrackleHeavy(); }},

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
    {"Graze Soft",  [](){ Sfx_GrazeSoft(); }},
    {"Graze Mid",   [](){ Sfx_GrazeMid(); }},
    {"Graze Boost", [](){ Sfx_GrazeBoost(); }},
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

