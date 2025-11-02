#pragma once


void AudioSfx_Initialize();
void AudioSfx_Finalize();


// In order to use those functions, AudioSfx_Init() must have been called before.
void SfxHissGraze(float strength);