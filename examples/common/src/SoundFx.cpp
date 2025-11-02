#pragma once

#include "SoundFx.h"
#include "Globals.h"
#include "SimpleMath.h"
#include <pd_api.h>

static inline void setup_amp_adsr(PDSynth* s, float a, float d, float sus, float r)
{
    _G.pd->sound->synth->setAttackTime(s, a);
    _G.pd->sound->synth->setDecayTime(s, d);
    _G.pd->sound->synth->setSustainLevel(s, sus);
    _G.pd->sound->synth->setReleaseTime(s, r);
}

struct Sfx_HissGraze
{
    // Crackle
    PDSynth* s_crackle = nullptr;
    PDSynthLFO* s_crackleLFO = nullptr;

    // Canal et effets dédiés au crackle (pour ne pas filtrer le reste)
    SoundChannel* s_crackleCh = nullptr;
    TwoPoleFilter* s_crackleBP = nullptr; // band-pass 2-4 kHz
    Overdrive* s_crackleOD = nullptr; // léger grain
    
    void initialize()
    {
        auto snd = _G.pd->sound;

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
    }

    void play(float strength)
    {
        if (!s_crackle || !s_crackleLFO || !s_crackleBP || !s_crackleOD) return;
        strength = clamp(strength, 0.0f, 1.0f);

        float rate = mapRange(strength, 0.0f, 1.0f, 12.0f, 90.0f);
        float depth = mix(0.35f, 1.00f, strength);
        float center = mix(0.25f, 0.65f, strength);
        _G.pd->sound->lfo->setRate(s_crackleLFO, rate);
        _G.pd->sound->lfo->setDepth(s_crackleLFO, depth);
        _G.pd->sound->lfo->setCenter(s_crackleLFO, center);

        float bpFreq = mapRange(strength, 0.0f, 1.0f, 2200.0f, 3800.0f);
        float bpQ = mix(0.35f, 0.75f, strength);
        _G.pd->sound->effect->twopolefilter->setFrequency(s_crackleBP, bpFreq);
        _G.pd->sound->effect->twopolefilter->setResonance(s_crackleBP, bpQ);

        float odGain = mix(0.45f, 0.85f, strength);
        _G.pd->sound->effect->overdrive->setGain(s_crackleOD, odGain);

        _G.pd->sound->synth->setAttackTime(s_crackle, 0.0f);
        _G.pd->sound->synth->setDecayTime(s_crackle, mix(0.10f, 0.28f, strength));
        _G.pd->sound->synth->setSustainLevel(s_crackle, 0.0f);
        _G.pd->sound->synth->setReleaseTime(s_crackle, mix(0.10f, 0.24f, strength));


        float len = mix(0.14f, 0.30f, strength);
        _G.pd->sound->synth->playNote(s_crackle, 200.0f, mix(0.7f, 1.0f, strength), len, 0);

        //if (strength > 0.6f && s_click)
            //_G.pd->sound->synth->playNote(s_click, 3200.0f, 0.5f, 0.02f, 0);
    }

    void finalize()
    {
        auto snd = _G.pd->sound;
        if (s_crackleLFO) { snd->lfo->freeLFO(s_crackleLFO); s_crackleLFO = nullptr; }
        if (s_crackle) { snd->synth->freeSynth(s_crackle); s_crackle = nullptr; }

        // TODO : check if it's necessary to remove source/effects before freeing channel
        if (s_crackleCh) {          
            if (s_crackle)   snd->channel->removeSource(s_crackleCh, (SoundSource*)s_crackle);
            if (s_crackleOD) snd->channel->removeEffect(s_crackleCh, (SoundEffect*)s_crackleOD);
            if (s_crackleBP) snd->channel->removeEffect(s_crackleCh, (SoundEffect*)s_crackleBP);
            snd->removeChannel(s_crackleCh);
            snd->channel->freeChannel(s_crackleCh);
            s_crackleCh = nullptr;
        }
        if (s_crackleOD) { snd->effect->overdrive->freeOverdrive(s_crackleOD); s_crackleOD = nullptr; }
        if (s_crackleBP) { snd->effect->twopolefilter->freeFilter(s_crackleBP); s_crackleBP = nullptr; }

    }
};

struct Sfx_SlideHiss
{
    PDSynth*      s_hiss   = nullptr;
    PDSynthLFO*   s_ampLFO = nullptr;
    SoundChannel* s_ch     = nullptr;
    TwoPoleFilter* s_bp    = nullptr;

    void initialize()
    {
        auto snd = _G.pd->sound;

        s_hiss = snd->synth->newSynth();
        snd->synth->setWaveform(s_hiss, kWaveformNoise);
        setup_amp_adsr(s_hiss, 0.005f, 0.18f, 0.0f, 0.16f);
        snd->synth->setVolume(s_hiss, 0.85f, 0.85f);

        s_ampLFO = snd->lfo->newLFO(kLFOTypeSine);
        snd->lfo->setRate(s_ampLFO, 4.0f);
        snd->lfo->setCenter(s_ampLFO, 0.70f);
        snd->lfo->setDepth(s_ampLFO, 0.12f);
        snd->synth->setAmplitudeModulator(s_hiss, (PDSynthSignalValue*)s_ampLFO);

        s_ch = snd->channel->newChannel();
        snd->channel->setVolume(s_ch, 0.7f);
        snd->addChannel(s_ch);
        snd->channel->addSource(s_ch, (SoundSource*)s_hiss);

        s_bp = snd->effect->twopolefilter->newFilter();
        snd->effect->twopolefilter->setType(s_bp, kFilterTypeBandPass);
        snd->effect->twopolefilter->setFrequency(s_bp, 2600.0f);
        snd->effect->twopolefilter->setResonance(s_bp, 0.20f);
        snd->channel->addEffect(s_ch, (SoundEffect*)s_bp);
        snd->effect->setMix((SoundEffect*)s_bp, 1.0f);
    }

    void play(float strength)
    {
        if (!s_hiss || !s_ampLFO || !s_ch || !s_bp) return;
        strength = clamp(strength, 0.0f, 1.0f);

        float centerHz = mapRange(strength, 0.0f, 1.0f, 1800.0f, 4200.0f);
        float res      = mix(0.15f, 0.35f, strength);
        _G.pd->sound->effect->twopolefilter->setFrequency(s_bp, centerHz);
        _G.pd->sound->effect->twopolefilter->setResonance(s_bp, res);

        _G.pd->sound->lfo->setRate(s_ampLFO,  mapRange(strength, 0.0f, 1.0f, 3.0f, 9.0f));
        _G.pd->sound->lfo->setDepth(s_ampLFO, mix(0.08f, 0.22f, strength));
        _G.pd->sound->lfo->setCenter(s_ampLFO, mix(0.60f, 0.90f, strength));

        _G.pd->sound->synth->setAttackTime(s_hiss, 0.005f);
        _G.pd->sound->synth->setDecayTime(s_hiss,  mix(0.12f, 0.24f, strength));
        _G.pd->sound->synth->setSustainLevel(s_hiss, 0.0f);
        _G.pd->sound->synth->setReleaseTime(s_hiss, mix(0.12f, 0.20f, strength));

        float len = mix(0.14f, 0.25f, strength);
        float vel = mix(0.50f, 0.85f, strength);
        _G.pd->sound->synth->playNote(s_hiss, 200.0f, vel, len, 0);
    }

    void finalize()
    {
        auto snd = _G.pd->sound;

        if (s_ampLFO) { snd->lfo->freeLFO(s_ampLFO); s_ampLFO = nullptr; }
        if (s_hiss)   { snd->synth->freeSynth(s_hiss); s_hiss = nullptr; }

        if (s_ch)
        {
            if (s_hiss) snd->channel->removeSource(s_ch, (SoundSource*)s_hiss);
            if (s_bp)   snd->channel->removeEffect(s_ch, (SoundEffect*)s_bp);
            snd->removeChannel(s_ch);
            snd->channel->freeChannel(s_ch);
            s_ch = nullptr;
        }
        if (s_bp) { snd->effect->twopolefilter->freeFilter(s_bp); s_bp = nullptr; }
    }
};

Sfx_HissGraze sSfx_HissGraze;
void SfxHissGraze(float strength)
{
    sSfx_HissGraze.play(strength);
}

Sfx_SlideHiss sSfx_SlideHiss;
void SfxSlideHiss(float strength)
{
    sSfx_SlideHiss.play(strength);
}

void AudioSfx_Initialize()
{
    auto snd = _G.pd->sound;
    sSfx_HissGraze.initialize();
    sSfx_SlideHiss.initialize();
};

void AudioSfx_Finalize()
{
    auto snd = _G.pd->sound;
    sSfx_SlideHiss.finalize();
    sSfx_HissGraze.finalize();
};