#include "SvgLoader.h"
#include "Globals.h"

#include <pd_api.h>

#include <charconv>
#include <cassert>
#include <vector>
#include <cctype>
#include <cstdlib>

// Read the content of the text file
// Return a null terminated str of success, false otherwise
char* readTextFile(const char* filename, int* out_size = nullptr)
{
    PlaydateAPI* pd = _G.pd;
    SDFile* file = pd->file->open(filename, kFileRead);
    if (!file)
    {
        pd->system->logToConsole("Can't open file %s", filename);
        return nullptr;
    }

    pd->file->seek(file, 0, SEEK_END);
    const int size = pd->file->tell(file);
    if (size < 0)
    {
        out_size = 0;
        pd->file->close(file);
        pd->system->logToConsole("Failed to get file size");
        return nullptr;
    }

    pd->file->seek(file, 0, SEEK_SET);
    char* buffer = (char*)malloc(size + 1);
    int bytesRead = pd->file->read(file, buffer, size);
    if (bytesRead != size)
    {
        pd->system->logToConsole("Read error: expected %d, got %d bytes\n", size, bytesRead);
        free(buffer);
        pd->file->close(file);
        return nullptr;
    }

    buffer[size] = '\0';

    pd->file->close(file);
    if (out_size)
    {
        *out_size = size + 1;
    }
    return buffer;
}


const char* strstr_limit(const char* token, const char* start, const char* end)
{
    size_t len = strlen(token);
    const char* p = start;
    while (p + len <= end)
    {
        if (memcmp(p, token, len) == 0)
        {
            return p;
        }

        p++;
    }
    return end;
}


// Search the token between start and end "<tag .... />"
// or <tag .... >content</token>
// 
// startAttribute / endAttribute if contain attribute
// startContent / endContent if have content
// 
// return the pointer nextAfter
const char* nextTag(const char* tag, const char* start, const char* end, const char** startAttribute, const char** endAttribute, const char** startContent, const char** endContent)
{
    // No attribute and No Content
    *startAttribute = *endAttribute = *startContent = *endContent = nullptr;

    const size_t tag_len = strlen(tag);
    assert(tag_len + 3 < 64); // "</>"

    char tmpTag[64];
    tmpTag[0] = '<';
    memcpy(&tmpTag[1], tag, tag_len);
    tmpTag[tag_len + 1] = '\0';


    const char* tag_start = strstr_limit(tmpTag, start, end);
    if (tag_start == end)
    {
        return end;
    }

    const char* tag_end = strstr_limit("/>", tag_start + tag_len + 1, end);
    if (tag_end != end)
    {
        // No content case
        assert(tag_end - (tag_start + tag_len + 1) >= 0);
        if (tag_end - (tag_start + tag_len + 1) > 0)
        {
            *startAttribute = tag_start + tag_len + 1;
            *endAttribute = tag_end;
        }
        return tag_end + sizeof("/>");
    }
    else
    {
        // search content
        const char* tag_end = strstr_limit(">", start + tag_len, end);
        if (tag_end != end)
        {
            assert(tag_end - (tag_start + tag_len + 1) >= 0);
            if (tag_end - (tag_start + tag_len + 1) > 0)
            {
                *startAttribute = tag_start + tag_len + 1;
                *endAttribute = tag_end;
            }

            const char* start_content = tag_end + sizeof(">");

            // Search </tag>
            tmpTag[0] = '<';
            tmpTag[1] = '/';
            memcpy(&tmpTag[2], tag, tag_len);
            tmpTag[tag_len + 2] = '>';
            tmpTag[tag_len + 3] = '\0';
            tag_end = strstr_limit(tag_end + sizeof(">"), start, end);
            if (tag_end != end)
            {
                if (tag_end - start_content > 0)
                {
                    *startContent = start_content;
                    *endContent = tag_end;
                }
                return tag_end + tag_len + 3;
            }
        }
    }

    return end;
}

//
const char* nextAttribute(const char* attr, const char* start, const char* end, const char** startValue, const char** endValue)
{
    *startValue = *endValue = nullptr;

    char tmpAttr[32];
    size_t attr_len = strlen(attr);
    assert(attr_len < 32);

    memcpy(&tmpAttr[0], attr, attr_len);
    tmpAttr[attr_len] = '=';
    tmpAttr[attr_len + 1] = '"';
    tmpAttr[attr_len + 2] = '\0';

    const char* attr_start = strstr_limit(tmpAttr, start, end);
    if (attr_start == end)
    {
        return end;
    }

    const char* attr_end = strstr_limit("\"", attr_start + 3, end);
    if (attr_end != end)
    {
        *startValue = attr_start + 3;
        *endValue = attr_end;
        return attr_end + 1;
    }

    return end;
}


// Same as atoi but take a start and end pointer
bool aoti_n(const char* start, const char* end, int& r)
{
    auto [ptr, ec] = std::from_chars(start, end, r);
    return ec != std::errc();
}

static const char* skipSeparators(const char* s, const char* end) {
    while (s < end && (std::isspace(*s) || *s == ',')) ++s;
    return s;
}

static bool isNumberStart(char c) {
    return std::isdigit(c) || c == '-' || c == '+' || c == '.';
}

static const char* parseFloat(const char* s, const char* end, float& value) {
    s = skipSeparators(s, end);
    if (s >= end) return s;
    char* next = nullptr;
    value = std::strtof(s, &next);
    return (next && next <= end) ? next : s;
}

std::vector<vec2> parsePath(const char* start, const char* end)
{
    std::vector<vec2> v;
    vec2 cur(0, 0);
    vec2 startPoint(0, 0);
    bool firstMove = true;
    char cmd = 0;

    const char* ptr = start;

    while (ptr < end)
    {
        ptr = skipSeparators(ptr, end);
        if (ptr >= end) break;

        if (std::isalpha(*ptr)) {
            cmd = *ptr++;
        }

        ptr = skipSeparators(ptr, end);
        if (!cmd) break;

        switch (cmd)
        {
        case 'M':
        case 'm':
        {
            float x, y;
            while (ptr < end && isNumberStart(*ptr)) {
                ptr = parseFloat(ptr, end, x);
                ptr = parseFloat(ptr, end, y);
                if (firstMove) {
                    if (cmd == 'm')
                        cur = vec2(cur.x + x, cur.y + y);
                    else
                        cur = vec2(x, y);
                    startPoint = cur;
                    v.push_back(cur);
                    firstMove = false;
                }
                else {
                    if (cmd == 'm')
                        cur = vec2(cur.x + x, cur.y + y);
                    else
                        cur = vec2(x, y);
                    v.push_back(cur);
                }
                ptr = skipSeparators(ptr, end);
            }
            break;
        }

        case 'L':
        case 'l':
        {
            float x, y;
            while (ptr < end && isNumberStart(*ptr)) {
                ptr = parseFloat(ptr, end, x);
                ptr = parseFloat(ptr, end, y);
                if (cmd == 'l')
                    cur = vec2(cur.x + x, cur.y + y);
                else
                    cur = vec2(x, y);
                v.push_back(cur);
                ptr = skipSeparators(ptr, end);
            }
            break;
        }

        case 'H':
        case 'h':
        {
            float x;
            while (ptr < end && isNumberStart(*ptr)) {
                ptr = parseFloat(ptr, end, x);
                if (cmd == 'h')
                    cur.x += x;
                else
                    cur.x = x;
                v.push_back(cur);
                ptr = skipSeparators(ptr, end);
            }
            break;
        }

        case 'V':
        case 'v':
        {
            float y;
            while (ptr < end && isNumberStart(*ptr)) {
                ptr = parseFloat(ptr, end, y);
                if (cmd == 'v')
                    cur.y += y;
                else
                    cur.y = y;
                v.push_back(cur);
                ptr = skipSeparators(ptr, end);
            }
            break;
        }

        case 'Z':
        case 'z':
        {
            cur = startPoint;
            v.push_back(cur);
            break;
        }

        default:
            // Skip unsupported command
            ++ptr;
            break;
        }
    }

    return v;
}



std::vector<std::vector<vec2>> svgParsePath(const char* filename)
{
    PlaydateAPI* pd = _G.pd;

    std::vector<std::vector<vec2>> polygons;

    const char* startDocument = readTextFile("./level0.svg");
    if (startDocument)
    {
        const char* endDocument = startDocument + strlen(startDocument);
        const char* startAttribute, * endAttribute, * startContent, * endContent;
        const char* cur = startDocument;
        do
        {
            cur = nextTag("path", cur, endDocument, &startAttribute, &endAttribute, &startContent, &endContent);
            if (startAttribute)
            {
                const char* startValue, * endValue;
                //nextAttribute("id", startAttribute, endAttribute, &startValue, &endValue);
                nextAttribute("d", startAttribute, endAttribute, &startValue, &endValue);
                if (startValue)
                {
                    std::vector<vec2> polygon = parsePath(startValue, endValue);
                    if (polygon.size() >= 2)
                    {
                        polygons.emplace_back(std::move(polygon));
                    }
                }
            }
        } while (cur != endDocument);


        free((void*)startDocument);
        startDocument = nullptr;
    }

    return polygons;
}