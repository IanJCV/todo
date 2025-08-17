#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "include/raylib.h"
#include "include/raymath.h"
#include "font.h"

static const unsigned char icon[] =
{
#embed "icon.png"
};

#define GRAB_BAR_PERCENTAGE 0.1f

#define COLOR_BACKGROUND (Color){46, 52, 64, 255}
#define COLOR_HIGHLIGHT (Color){127, 177, 193, 255}

// useful smooth range: 1 to 25 (slow to fast)
inline float lerp(float a, float b, float smooth, float dt)
{
    return b + (a - b) * expf(-smooth * dt);
}

#pragma region Typedefs

typedef struct
{
    char* begin;
    int length;
} string;

typedef struct
{
    string name;
    bool done;

    bool active;

    float anim;
} TodoRecord;

typedef struct
{
    TodoRecord* data;
    int length;
    int capacity;
} RecordArray;

#pragma endregion

#pragma region String Functions


#define STR_ALLOCSIZE 128

static string str_create(const char* text)
{
    int len = TextLength(text);
    char* data = MemAlloc(STR_ALLOCSIZE);
    strncpy(data, text, len);
    return (string){.begin = data, .length = len};
}

static void str_destroy(string* str)
{
    MemFree(str->begin);
}

static void str_append(string* str, char c)
{
    str->begin[str->length++] = c;
    str->begin[str->length] = '\0';
    assert(str->length < STR_ALLOCSIZE - 1);
}

static void str_backspace(string* str)
{
    if (str->length < 1)
        return;
    str->length--;
    str->begin[str->length] = '\0';
}

static string str_duplicate(string* str)
{
    string s = str_create(str->begin);
    return s;
}

static void str_zero(string* str)
{
    str->begin[0] = '\0';
    str->length = 0;
}

static int str_serialize(string* str, unsigned char* stream)
{
    int len = 0;
    memcpy(stream, &str->length, sizeof(str->length));
    len += sizeof(str->length);
    memcpy(&stream[len], str->begin, str->length);
    len += str->length;
    return len;
}

static string str_deserialize(unsigned char* stream, int* bytes_read)
{
    string str = str_create("");
    memcpy(&str.length, stream, sizeof(str.length));
    stream += sizeof(str.length);
    memcpy(str.begin, stream, str.length);
    str.begin[str.length] = '\0';
    *bytes_read = sizeof(int) + str.length;
    return str;
}

#pragma endregion

#pragma region Record Functions

RecordArray records = (RecordArray){.data = NULL, .length = 0, .capacity = 8};

static void arr_remove(int index)
{
    assert(index >= 0 && index < records.length);
    str_destroy(&records.data[index].name);
    records.data[index].active = false;
}

static void arr_add(TodoRecord record)
{
    record.active = true;
    record.anim = 0.f;
    for (int i = 0; i < records.length; i++)
    {
        if (records.data[i].active == false)
        {
            records.data[i] = record;
            return;
        }
    }
    
    if (records.length >= records.capacity - 1)
    {
        records.capacity *= 2;
        records.data = MemRealloc(records.data, records.capacity);
    }

    records.data[records.length++] = record;
}

static void arr_create()
{
    if (records.data != NULL)
    {
        MemFree(records.data);
    }

    records.data = MemAlloc(sizeof(RecordArray) * records.capacity);
}


#pragma endregion

#pragma region Serialization
static void save_data()
{
    int totalSize = sizeof(int);
    int activeRecords = 0;
    for (int i = 0; i < records.length; i++)
    {
        if (!records.data[i].active)
            continue;
        
        totalSize += sizeof(int);
        totalSize += records.data[i].name.length;
        totalSize += sizeof(bool);
        activeRecords++;
    }
    
    unsigned char* bytes = MemAlloc(totalSize);
    int totalWritten = 0;

    memcpy(bytes, &activeRecords, sizeof(int));
    printf("wrote active records: %d\n", activeRecords);
    totalWritten += sizeof(int);
    for (int i = 0; i < records.length; i++)
    {
        if (!records.data[i].active)
            continue;
        
        totalWritten += str_serialize(&records.data[i].name, &bytes[totalWritten]);
        printf("wrote %d bytes of string\n", totalWritten);
        memcpy(&bytes[totalWritten], &records.data[i].done, sizeof(bool));
        totalWritten += sizeof(bool);
    }

    printf("totalsize %d totalwritten %d\n", totalSize, totalWritten);
    assert(totalSize == totalWritten);

    SaveFileData("./todo.bin", bytes, totalWritten);
    MemFree(bytes);
}

static void load_data()
{
    if (!FileExists("./todo.bin"))
    {
        save_data();
        return;
    }

    int len = 0;
    unsigned char* data = LoadFileData("./todo.bin", &len);

    int totalRead = 0;
    int arrlen = 0;
    memcpy(&arrlen, data, sizeof(int));
    printf("arrlen %d\n", arrlen);
    totalRead += sizeof(int);
    for (int i = 0; i < arrlen; i++)
    {
        int strlen = 0;
        TodoRecord rec;
        rec.active = true;

        rec.name = str_deserialize(&data[totalRead], &strlen);
        printf("rec[%d].name %s\n", i, rec.name.begin);
        totalRead += strlen;
        
        memcpy(&rec.done, &data[totalRead], sizeof(bool));
        totalRead += sizeof(bool);

        arr_add(rec);
    }

    assert(arrlen == records.length);
    
    MemFree(data);
}
#pragma endregion

ConfigFlags flags = FLAG_WINDOW_UNDECORATED | FLAG_VSYNC_HINT;

Font font;

int main()
{
    arr_create();
    load_data();
    
    SetConfigFlags(flags);
    InitWindow(600, 200, "todo");
    Image image = LoadImageFromMemory(".png", icon, sizeof(icon));
    SetWindowIcon(image);

    font = LoadFont_IBM();

    Vector2 mousePosition = GetMousePosition();
    Vector2 windowPosition = GetWindowPosition();
    Vector2 panOffset = mousePosition;
    bool dragWindow = false;

    bool inputOpen = false;
    string inputText = str_create("");

    EnableEventWaiting();
    SetExitKey(KEY_ESCAPE);
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(COLOR_BACKGROUND);

        if (!inputOpen && IsKeyPressed(KEY_T))
        {
            if (IsWindowState(FLAG_WINDOW_TOPMOST))
                ClearWindowState(FLAG_WINDOW_TOPMOST);
            else
                SetWindowState(FLAG_WINDOW_TOPMOST);
        }

        mousePosition = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !dragWindow)
        {
            SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
            windowPosition = GetWindowPosition();
            dragWindow = true;
            panOffset = mousePosition;
        }

        if (dragWindow)
        {            
            windowPosition.x += (mousePosition.x - panOffset.x);
            windowPosition.y += (mousePosition.y - panOffset.y);

            SetWindowPosition((int)windowPosition.x, (int)windowPosition.y);
            
            if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON))
            {
                SetMouseCursor(MOUSE_CURSOR_DEFAULT);
                dragWindow = false;
            }
        }
        
        
        if (IsKeyPressed(KEY_ENTER))
        {
            inputOpen = !inputOpen;

            if (!inputOpen && inputText.length > 0)
            {
                arr_add((TodoRecord){.name = str_duplicate(&inputText), .done = false, .active = true});
            }
            else
            {
                str_zero(&inputText);
            }
        }


        int lineHeight = GetScreenHeight() * GRAB_BAR_PERCENTAGE;
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int ypad = 2;
        int y0 = 0;

        if (inputOpen)
        {
            char c = 0;
            while ((c = GetCharPressed()) != 0)
            {
                str_append(&inputText, c);
            }
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE))
            {
                str_backspace(&inputText);
            }

            DrawRectangle(4, y0, GetScreenWidth() - 4, lineHeight, LIGHTGRAY);
            DrawTextEx(font, inputText.begin, (Vector2){4, y0}, lineHeight, 0.f, BLACK);

            Vector2 textSize = MeasureTextEx(font, inputText.begin, lineHeight, 0.f);
            DrawRectangle(textSize.x + 2, 0, 10, textSize.y, BLACK);
            y0 += lineHeight + ypad;
        }

        EnableEventWaiting();
        for (int i = 0; i < records.length; i++)
        {
            TodoRecord* rec = &records.data[i];
            if (!rec->active)
                continue;
            Rectangle fullRect = (Rectangle){0, y0, lineHeight, lineHeight};
            Color btnColor = rec->done ? GREEN : RED;
            if (CheckCollisionPointRec(mousePosition, fullRect))
            {
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                {
                    rec->done = !rec->done;
                }
                btnColor = COLOR_HIGHLIGHT;
            }
            Rectangle btnRect = {fullRect.x + 2, fullRect.y + 2, fullRect.width - 4, fullRect.height - 4};
            DrawRectangleRounded(btnRect, 0.15f, 0.f, btnColor);

            fullRect.width = sw - lineHeight;
            fullRect.x += lineHeight;

            Color lineColor = { 195, 195, 195, 255 };
            if (CheckCollisionPointRec(mousePosition, fullRect) && IsKeyDown(KEY_LEFT_CONTROL))
            {
                lineColor = (Color){ 255, 195, 195, 255 };
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                {
                    arr_remove(i);
                }
            }

            Rectangle lineRect = {fullRect.x + 4, fullRect.y + 1, fullRect.width - 4.f, fullRect.height - 2};

            rec->anim = lerp(rec->anim, 1.f, 5.f, GetFrameTime());
            DrawRectangleRounded(lineRect, 0.15f, 0.f, ColorLerp(GREEN, lineColor, rec->anim));
            DrawTextEx(font, rec->name.begin, (Vector2){lineRect.x + 4, lineRect.y}, lineHeight - 2, 0.f, BLACK);

            if (fabsf(rec->anim - (fullRect.width - 4.f)) >= 1.f)
            {
                DisableEventWaiting();
            }
            y0 += lineHeight + ypad;
        }

        EndDrawing();
    }

    save_data();
    
    return 0;
}