#include "Sandbox.h"
#include "Globals.h"
#include "SimpleMath.h"
#include "SvgLoader.h"

#include <assert.h>

#include <pdcpp/pdnewlib.h>

#include <vector>


// The easiest way to use pattern is not my overkill method with bitmap
// but by creating a uint8_t pattern[16]
// First 8 bytes represent the Color of 8x8 image
// The next 8 byte represent mask
// Simply cast the pattern pointer into LCDColor
// example :  pd->graphics->fillRect(x, y, 50, 50, (LCDColor)patternStripes);


static const uint8_t patternStripes[] = {
    0b10101010,
    0b01010101,
    0b10101010,
    0b01010101,
    0b10101010,
    0b01010101,
    0b10101010,
    0b01010101,

    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

const uint8_t pattern16x2[] = {
    0x55,0x24,
    0x00,0x48,
    0x55,0x90,
    0x00,0x21,
    0x55,0x42,
    0x00,0x84,
    0x55,0x09,
    0x00,0x12
};

// https://github.com/ace-dent/8x8.me
const uint8_t bayerDither04[] = {
    0x55,
    0x00,
    0x55,
    0x00,
    0x55,
    0x00,
    0x55,
    0x00,

    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

const uint8_t dexterPinstripe[] = {
    0x24,
    0x48,
    0x90,
    0x21,
    0x42,
    0x84,
    0x09,
    0x12,

    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

// Magic: "$H\220!B\204\t\22"[i%8]
const uint8_t dexterPinstripeMedium[] = {
    0x64,
    0xC8,
    0x91,
    0x23,
    0x46,
    0x8C,
    0x19,
    0x32,

    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

// Shift on the left and down
void shiftSprite8x8(const uint8_t src[8], uint8_t dst[8], int shiftX, int shiftY)
{
    for (int y = 0; y < 8; ++y)
    {
        int srcY = (y - shiftY) & 7; // (y - shiftY + 8) % 8
        uint8_t row = src[srcY];

        // ROR
        dst[y] = (uint8_t)((row << (8-shiftX)) | (row >> (shiftX)));
    }
}


// Pre-compute all combinatory of 8x8 sprite shift
// Overkill, and to Bitmap alloc
struct FixedPattern8x8_Deprecated
{
    FixedPattern8x8_Deprecated(const uint8_t PatterData[8])
    {
        for (int i = 0; i < 64; ++i)
        {
            Bitmap[i] = _G.pd->graphics->newBitmap(8, 8, kColorWhite);

            int x = i & 0x7;    // i%8
            int y = i >> 3;     // i/8

            uint8_t Shifted[8];
            shiftSprite8x8(PatterData, Shifted, x, y);
            fillData(Shifted, Bitmap[i]);

        }
    }

    ~FixedPattern8x8_Deprecated()
    {
        for (int i = 0; i < 64; ++i)
        {
            _G.pd->graphics->freeBitmap(Bitmap[i]);
        }
    }

    void fillData(const uint8_t SrcData[8], LCDBitmap* Bitmap)
    {
        int dstWidth, dstHeight, dstRowBytes;
        uint8_t* dstMask = nullptr;
        uint8_t* dstData = nullptr;
        _G.pd->graphics->getBitmapData(Bitmap, &dstWidth, &dstHeight, &dstRowBytes, &dstMask, &dstData);

        // Warning : Bitmap width seem align on 32bit
        for (int y = 0; y < 8; ++y)
        {
            dstData[y * dstRowBytes + 0] = SrcData[y];
        }
    }


    LCDColor fixedPatternByIndex(int Index)
    {
        assert(Index < 64);
        LCDColor pattern;
        _G.pd->graphics->setColorToPattern(&pattern, Bitmap[Index], 0, 0);
        return pattern;
    }

    // Return the pattern adapted at the position in order to obtain a fixed pattern
    LCDColor fixedPattern(int x, int y)
    {
        LCDColor pattern;
        int sX = -x & 0x7;
        int sY = y & 0x7;
        if (sX < 0) sX += 8;
        if (sY < 0) sY += 8;
        assert(sY < 8 && sY >= 0);
        assert(sX < 8 && sX >= 0);
        _G.pd->graphics->setColorToPattern(&pattern, Bitmap[sY*8 + sX], 0, 0);
        return pattern;
    }

    LCDBitmap* Bitmap[64];
};


// Fixed pattern take 1024 Bytes !
// 64 combinations of the origibal 16-bytes mask
struct FixedPattern8x8
{
    FixedPattern8x8(const uint8_t PatterData[16])
    {
        for (int i = 0; i < 64; ++i)
        {
            int x = i & 0x7;    // i%8
            int y = i >> 3;     // i/8

            
            uint8_t Shifted[16];
            shiftSprite8x8(PatterData, Shifted, x, y);
            shiftSprite8x8(PatterData+8, Shifted+8, x, y);
            memcpy(Patterns + i * 16, Shifted, 16);          
        }
    }

    LCDColor fixedPatternByIndex(int Index)
    {
        assert(Index < 64);
        return LCDColor(&Patterns[Index*16]);
    }

    // Return the pattern adapted at the position in order to obtain a fixed pattern
    LCDColor fixedPattern(int x, int y)
    {
        LCDColor pattern;
        int sX = -x & 0x7;
        int sY = y & 0x7;
        if (sX < 0) sX += 8;
        if (sY < 0) sY += 8;
        assert(sY < 8 && sY >= 0);
        assert(sX < 8 && sX >= 0);
        return LCDColor(&Patterns[sY * 16 * 8 + sX * 16]);
    }

    uint8_t Patterns[16 * 64];
};

// Same as FixedPattern8x8 but recompute the mask each time,
// Care each call modify a static variable
LCDColor shiftPatternWithMask(const uint8_t Pattern[16], int x, int y)
{
    static uint8_t Shifted[16];
    x = x & 0x7;    
    y = y & 0x7;   
    shiftSprite8x8(Pattern, Shifted, x, y);
    shiftSprite8x8(Pattern + 8, Shifted + 8, x, y);
    return LCDColor(Shifted);
}


// Same as FixedPattern8x8 but recompute the mask each time,
// Care each call modify a static variable
LCDColor shiftPattern(const uint8_t Pattern[8], int x, int y)
{    
    static uint8_t Shifted[16] = LCDOpaquePattern(0,0,0,0,0,0,0,0);
    x = x & 0x7;
    y = y & 0x7;
    shiftSprite8x8(Pattern, Shifted, x, y);
    return LCDColor(Shifted);
}

void testSprite(float Time)
{
    PlaydateAPI* pd = _G.pd;
    static bool IsInit = false;
    if (!IsInit)
    {
        IsInit = true;
    }
}

static LCDBitmap* makeDiagonalPattern(void)
{
    PlaydateAPI* pd = _G.pd;
    LCDBitmap* bmp = pd->graphics->newBitmap(8, 8, kColorWhite);

    pd->graphics->pushContext(bmp);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if ((x + y) % 2 == 0)  // diagonales altern�es
                pd->graphics->setPixel(x, y, kColorBlack);
        }
    }
    pd->graphics->popContext();

    return bmp;
}

// Initialize a Bitmap with a sequential image
// Bitmap rowBytes can be different from SrcData
// 
// To Emulate the lua function setPattern
// patternBayer = pd->graphics->newBitmap(8, 8, kColorWhite);
// setBitmapData(bayerDither04, 8, 8, patternBayer);
// LCDColor plop;
// pd->graphics->setColorToPattern(&plop, patternBayer, 0, 0);
static void setBitmapData(const uint8_t* SrcData, int SrcWidth, int SrcHeight, LCDBitmap* Bitmap)
{
    PlaydateAPI* pd = _G.pd;

    int dstWidth, dstHeight, dstRowBytes;
    uint8_t* dstMask = nullptr;
    uint8_t* dstData = nullptr;
    pd->graphics->getBitmapData(Bitmap, &dstWidth, &dstHeight, &dstRowBytes, &dstMask, &dstData);
    assert(dstWidth == SrcWidth);
    assert(dstHeight == SrcHeight);
    assert(dstData);
    
    int srcRowBytes = SrcWidth / 8;
    assert(srcRowBytes <= dstRowBytes);

    // Warning : Bitmap width seem align on 32bit
    for (int y = 0; y < SrcHeight; ++y)
    {
        for (int x = 0; x < srcRowBytes; ++x)
        {
            dstData[y * dstRowBytes + x] = SrcData[y * srcRowBytes + x];
        }
    }
}



void testShiftPattern(float Time)
{
    PlaydateAPI* pd = _G.pd;

    static FixedPattern8x8 fixedPattern(dexterPinstripe);
    static bool IsInit = false;
    if (!IsInit)
    {
    }

    // Visualize the fixed pattern technique
    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {            
            LCDColor fixedPatternColor = fixedPattern.fixedPatternByIndex(y * 8 + x);
            pd->graphics->fillRect(x * 8, y*8, 8, 8, fixedPatternColor);
        }
    }
}

void testDrawing(float Time)
{
    PlaydateAPI* pd = _G.pd;
    

    static FixedPattern8x8 fixedPattern(dexterPinstripe);
    static bool IsInit = false;
    static LCDBitmap* bmp = nullptr;
    static LCDBitmap* bmPattern = makeDiagonalPattern();
    static LCDBitmap* bmpPatternBayer = nullptr;
    static LCDBitmap* bmpPatternDexterPinstripe = nullptr;
    static LCDBitmap* bmpPatternDexterPinstripeMedium = nullptr;
    static LCDBitmap* bmpPattern16x2 = nullptr;
    if (!IsInit)
    {
        IsInit = true;

        bmp = pd->graphics->newBitmap(100, 100, kColorWhite);

        bmpPatternBayer = pd->graphics->newBitmap(8, 8, kColorWhite);
        setBitmapData(bayerDither04, 8, 8, bmpPatternBayer);

        bmpPatternDexterPinstripe = pd->graphics->newBitmap(8, 8, kColorWhite);
        setBitmapData(dexterPinstripe, 8, 8, bmpPatternDexterPinstripe);

        bmpPatternDexterPinstripeMedium = pd->graphics->newBitmap(8, 8, kColorWhite);
        setBitmapData(dexterPinstripeMedium, 8, 8, bmpPatternDexterPinstripeMedium);

        bmpPattern16x2 = pd->graphics->newBitmap(16, 8, kColorWhite);
        setBitmapData(pattern16x2, 16, 8, bmpPattern16x2);
    }

    float Angle = 180.0f * cosf(Time);
    float Size = 50.f + 100.f * fabsf(sinf(Time));
    
    //pd->graphics->setColorToPattern
    const char* text = "Hello World\nHolla chica!";
    pd->graphics->drawRoundRect(10, 10, Size, Size, 5, 2, kColorBlack);
    pd->graphics->drawTextInRect(text, strlen(text), kASCIIEncoding, 10, 10, Size, Size, kWrapCharacter, kAlignTextCenter);
    
    //pd->graphics->drawEllipse(100, 100, Size, 50 + Size, 2, 0, Angle, kColorBlack);

    
    // Fill pattern example
    int shiftPatternIndex = (int)fmodf(Time, 8);
    LCDColor patternColor;
    pd->graphics->setColorToPattern(&patternColor, bmpPatternBayer, 0, 0);
    
    
    pd->graphics->fillEllipse(100, 100, 100, 100, 0, 270, patternColor);
    pd->graphics->fillRect(200, 100, 50, 50, patternColor);


    char tmpStr[256] = { '\0' };
    snprintf(tmpStr, 256, "Shift=%d", shiftPatternIndex);
    pd->graphics->drawText(tmpStr, strlen(tmpStr), kASCIIEncoding, 250, 100 - 14);
    pd->graphics->setColorToPattern(&patternColor, bmpPatternBayer, 0, shiftPatternIndex);
    pd->graphics->fillRect(250, 100, 50, 50, patternColor);

    pd->graphics->fillRect(300, 100, 50, 50, kColorBlack);

    pd->graphics->setColorToPattern(&patternColor, bmpPattern16x2, 0, 0);
    pd->graphics->fillRect(350, 100, 50, 50, patternColor);


    
    float tY = fabsf(sinf(Time*0.5)) * 50;
    float tYFast = fabsf(sinf(Time*2.0)) * 50;
    float tYFast2 = fabsf(sinf(Time * 4.0)) * 50;    
    pd->graphics->setColorToPattern(&patternColor, bmpPatternBayer, 0, 0);
    pd->graphics->fillRect(200, 150 + tY, 50, 50, patternColor);
    pd->graphics->setColorToPattern(&patternColor, bmpPatternDexterPinstripe, 0, 0);
    pd->graphics->fillRect(250, 150 + tYFast, 50, 50, patternColor);
    pd->graphics->setColorToPattern(&patternColor, bmpPatternDexterPinstripeMedium, 0, 0);
    pd->graphics->fillRect(300, 150 + tYFast2, 50, 50, patternColor);


    // Test pattern fixe sur l'axe X et Y
    // pattern independant de la position
    int x = 200;
    int y = 0 + tY;

    const uint8_t* patternTable[] = { bayerDither04, dexterPinstripe, dexterPinstripe };
    int patternIndex = int(Time/4) % 3;

    //LCDColor fixedPatternColor = fixedPattern.fixedPattern(x, y);
    LCDColor fixedPatternColor = shiftPattern(patternTable[patternIndex], x, y);
    pd->graphics->fillRect(x, y, 50, 50, fixedPatternColor);

    x = 200 + tY;
    y = 0;
    fixedPatternColor = shiftPattern(patternTable[patternIndex], x, y);
    pd->graphics->fillRect(x, y, 50, 50, fixedPatternColor);

    
    // Test macro LCDOpaquePattern
    static uint8_t PatternTest[16] = LCDOpaquePattern(0xff, 0, 0xff, 0, 0xff, 0, 0xff, 0);

    x = 300;
    y = 0 + tY;
    pd->graphics->fillRect(x, y, 50, 50, (LCDColor)patternStripes);

    x = 300 + tY;
    y = 0;
    pd->graphics->fillRect(x, y, 50, 50, (LCDColor)patternStripes);
   
}

void testLineDrawing(float Time)
{
    PlaydateAPI* pd = _G.pd;

    float speed = 10.0;
    float tx = sinf(Time * speed) * 20.0;
    //pd->graphics->setDrawOffset(fabsf(tx), 0);

    int startx = 0;
    int starty = 0;
    int xspacing = 60;
    int yspacing = 10;
    int line_length = 50;
    int half_length = line_length / 2;
    int i, j;
    for ( j = 0; j < 3; ++j)
    {
        pd->graphics->setLineCapStyle((LCDLineCapStyle)j);
        for (i = 0; i < 10; ++i)
        {
            pd->graphics->drawLine(startx + xspacing * j, i * yspacing, startx + xspacing * j + line_length, i * yspacing, i + 1, kColorBlack);
        }
    }

    startx = 200;
    starty = 0;
    yspacing = 20;
    for (j = 0; j < 3; ++j)
    {
        pd->graphics->setLineCapStyle((LCDLineCapStyle)j);
        for (i = 0; i < 10; ++i)
        {
            pd->graphics->drawLine(startx + xspacing * j, i * yspacing, startx + xspacing * j + line_length, i * yspacing + 10, i + 1, kColorBlack);
        }
    }

    // Test center on line with width > 1
    startx = 10;
    starty = 150;
    yspacing = 50;
    int width = 1;
    float angle = Time;
    for (j = 0; j < 3; ++j)
    {
        /*pd->graphics->setLineCapStyle(kLineCapStyleButt);
        pd->graphics->drawLine(startx + xspacing * j, starty + i * yspacing, startx + xspacing * j + line_length, starty + i * yspacing, i + 1, kColorBlack);
        pd->graphics->drawLine( startx + xspacing * j + line_length * 0.5, starty + i * yspacing - yspacing*0.5,
                                startx + xspacing * j + line_length * 0.5, starty + i * yspacing + yspacing * 0.5, i + 1, kColorBlack);*/

        width = 1;
        line_length = 64;
        half_length = line_length / 2;

        pd->graphics->setLineCapStyle(kLineCapStyleButt);
        int xcenter = startx + line_length * 0.5;
        vec2 center(startx + line_length * 0.5, starty);

        {
            vec2 a(xcenter - half_length, starty);
            vec2 b(xcenter + half_length, starty);
            vec2 c(xcenter, starty - half_length);
            vec2 d(xcenter, starty + half_length);
            a = rotate(a, center, angle);
            b = rotate(b, center, angle);
            c = rotate(c, center, angle);
            d = rotate(d, center, angle);
            pd->graphics->drawLine(a.x, a.y, b.x, b.y, width, kColorBlack);
            pd->graphics->drawLine(c.x, c.y, d.x, d.y, width, kColorBlack);
        }


        pd->graphics->setLineCapStyle(kLineCapStyleRound);
        line_length = 32;
        half_length = line_length / 2;
        width = 5;

        {
            vec2 a(xcenter - half_length, starty);
            vec2 b(xcenter + half_length, starty);
            vec2 c(xcenter, starty - half_length);
            vec2 d(xcenter, starty + half_length);
            a = rotate(a, center, angle);
            b = rotate(b, center, angle);
            c = rotate(c, center, angle);
            d = rotate(d, center, angle);
            pd->graphics->drawLine(a.x, a.y, b.x, b.y, width, kColorBlack);
            pd->graphics->drawLine(c.x, c.y, d.x, d.y, width, kColorBlack);
        }

    }
}


void testPicoSvg(float Time)
{
    PlaydateAPI* pd = _G.pd;

    static bool sIsInit = false;
    static std::vector<std::vector<vec2>> polygons;
    if (!sIsInit)
    {
        sIsInit = true;
        polygons = svgParsePath("level0.svg");
    }

    float speed = 10.0;
    int offset = cosf(Time * speed) * 100.0f;
    pd->graphics->setDrawOffset(offset, 0);
    pd->graphics->setLineCapStyle(kLineCapStyleRound);
    for (size_t p = 0; p < polygons.size(); ++p)
    {
        for (size_t i = 0; i < polygons[p].size() - 1; ++i)
        {
            const vec2& p0 = polygons[p][i];
            const vec2& p1 = polygons[p][i+1];
            pd->graphics->drawLine(p0.x, p0.y, p1.x, p1.y, 3, kColorBlack);
        }
    }
}