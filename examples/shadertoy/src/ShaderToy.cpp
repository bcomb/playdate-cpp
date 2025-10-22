#include "Shadertoy.h"
#include "Globals.h"
#include "SimpleMath.h"
#include "ImageLoader.h"

#include <pdcpp/pdnewlib.h>
#include <malloc.h>
#include <memory.h>

/******************************************************************************/
#define SCREEN_X	400
#define SCREEN_Y	240
#define SCREEN_STRIDE_BYTES 52

static uint8_t* s_blue_noise = nullptr;
uint8_t g_screen_buffer[SCREEN_X * SCREEN_Y];

/******************************************************************************/
void draw_dithered_scanline(const uint8_t* values, int y, int bias, uint8_t* framebuffer)
{
    uint8_t scanline[SCREEN_STRIDE_BYTES];
    const uint8_t* noise_row = s_blue_noise + y * SCREEN_X;
    int px = 0;
    for (int bx = 0; bx < SCREEN_X / 8; ++bx) {
        uint8_t pixbyte = 0xFF;
        for (int ib = 0; ib < 8; ++ib, ++px) {
            if (values[px] <= noise_row[px] + bias) {
                pixbyte &= ~(1 << (7 - ib));
            }
        }
        scanline[bx] = pixbyte;
    }

    uint8_t* row = framebuffer + y * SCREEN_STRIDE_BYTES;
    memcpy(row, scanline, sizeof(scanline));
}

/******************************************************************************/
void draw_dithered_screen(uint8_t* framebuffer, int bias)
{
    const uint8_t* src = g_screen_buffer;
    for (int y = 0; y < SCREEN_Y; ++y)
    {
        draw_dithered_scanline(src, y, bias, framebuffer);
        src += SCREEN_X;
    }
}

/******************************************************************************/
// https://www.shadertoy.com/view/XsBfRW
void PrettyHipShader(vec4& fragColor, vec2 fragCoord, const vec2 iResolution, float iTime)
{
    float aspect = iResolution.y / iResolution.x;
    float value;
    vec2 uv = fragCoord / iResolution.x;
    //uv -= vec2(0.5f, 0.5f * aspect);
    //float rot = radians(45.0); 
    //float rot = radians(45.0*sinf(iTime));
    //mat2 m = mat2(cosf(rot), -sinf(rot), sinf(rot), cosf(rot));
    //uv = m * uv;
    //uv += vec2(0.5f, 0.5f * aspect);
    uv.y += 0.5f * (1.0f - aspect);
    vec2 pos = uv * 10.0f;
    vec2 rep = fract(pos);
    float dist = 2.0f * min(min(rep.x, 1.0f - rep.x), min(rep.y, 1.0f - rep.y));
    float squareDist = length((floor(pos) + vec2(0.5f)) - vec2(5.0f));

    float edge = sinf(iTime - squareDist * 0.5f) * 0.5f + 0.5f;

    edge = (iTime - squareDist * 0.5f) * 0.5f;
    edge = 2.0f * fract(edge * 0.5f);
    value = fract(dist * 2.0f);
    value = mix(value, 1.0f - value, step(1.0f, edge));
    edge = powf(fabsf(1.0f - edge), 2.0f);
    value = smoothstep(edge - 0.05f, edge, 0.95f * value);

    value += squareDist * .1f;
    fragColor = mix(vec4(1.0f, 1.0f, 1.0f, 1.0f), vec4(0.5f, 0.75f, 1.0f, 1.0f), value);
    fragColor.a = 0.25f * clamp(value, 0.0f, 1.0f);
}

//******************************************************************************
void ShaderToy::Initialize()
{
    int bn_w, bn_h;
    s_blue_noise = read_tga_file_grayscale("images/BlueNoise.tga", &bn_w, &bn_h);
    if (bn_w != SCREEN_X || bn_h != SCREEN_Y) {
        free(s_blue_noise);
        s_blue_noise = NULL;
    }
}

//******************************************************************************
void ShaderToy::Finalize()
{
    if (s_blue_noise) {
        free(s_blue_noise);
        s_blue_noise = NULL;
    }
}

//******************************************************************************
void ShaderToy::Render(float Time)
{
    uint8_t* FrameBuffer = _G.pd->graphics->getFrame();

    int Effect = int(fmodf(Time, 15.0f) / 5.0f);

    for (int y = 0; y < SCREEN_Y; ++y)
    {
        for (int x = 0; x < SCREEN_X; ++x)
        {
            switch (Effect)
            {
                // Simple gradient on X
                case 0:
                {
                    g_screen_buffer[y * SCREEN_X + x] = (uint8_t)(((float)x / (float)SCREEN_X) * 255.f);
                }
                break;
                // Sin wave
                case 1:
                {
                    float gray = fabsf(sinf(Time + (float)x * 0.1f + (float)y * 0.1f));
                    uint8_t gray8 = (uint8_t)(gray * 255.0f);
                    g_screen_buffer[y * SCREEN_X + x] = gray8;
                }
                break;
                // More complex shader from shadertoy
                case 2:
                {
                    vec4 fragColor(0.0f, 0.0f, 0.0f, 0.0f);
                    vec2 fragCoord((float)x, (float)y);
                    vec2 iResolution((float)SCREEN_X, (float)SCREEN_Y);
                    PrettyHipShader(fragColor, fragCoord, iResolution, Time);

                    float gray = luminance(fragColor.r, fragColor.g, fragColor.b);
                    uint8_t gray8 = (uint8_t)(gray * 255.0f);
                    g_screen_buffer[y * SCREEN_X + x] = gray8;
                }
                break;
                default:
                    break;
            }            
        }
    }

    // Apply dither and copy on playdate framebuffer
    draw_dithered_screen(FrameBuffer, 0);
}