#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "include/raylib.h"
#include "include/raymath.h"

static const unsigned char icon[] =
{
#embed "icon.png"
};

#define GRAB_BAR_PERCENTAGE 0.1f

#define NORD ((Color){46, 52, 64, 255});

#pragma region Strings

typedef struct
{
    char* begin;
    int length;
} string;

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

#pragma region Records
typedef struct
{
    string name;
    bool done;

    bool active;
} TodoRecord;

typedef struct
{
    TodoRecord* data;
    int length;
    int capacity;
} RecordArray;


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
    
}

int main()
{
    arr_create();
    load_data();

    SetConfigFlags(FLAG_BORDERLESS_WINDOWED_MODE | FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_UNDECORATED | FLAG_VSYNC_HINT);
    InitWindow(600, 200, "todo");
    Image image = LoadImageFromMemory(".png", icon, sizeof(icon));
    SetWindowIcon(image);



    Vector2 mousePosition = GetMousePosition();
    Vector2 windowPosition = GetWindowPosition();
    Vector2 panOffset = mousePosition;
    bool dragWindow = false;

    bool inputOpen = false;
    string inputText = str_create("");

    EnableEventWaiting();
    SetExitKey(KEY_F10);
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(GRAY);

        mousePosition = GetMousePosition();

        int grabBarSize = GetScreenHeight() * GRAB_BAR_PERCENTAGE;
        Rectangle grabRect = (Rectangle){ 0, 0, GetScreenWidth(), GetScreenHeight() };
        if (CheckCollisionPointRec(mousePosition, grabRect))
        {

            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !dragWindow)
            {
                windowPosition = GetWindowPosition();
                dragWindow = true;
                panOffset = mousePosition;
            }
        }

        if (dragWindow)
        {            
            windowPosition.x += (mousePosition.x - panOffset.x);
            windowPosition.y += (mousePosition.y - panOffset.y);

            SetWindowPosition((int)windowPosition.x, (int)windowPosition.y);
            
            if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) dragWindow = false;
        }
        
        
        if (IsKeyPressed(KEY_ENTER))
        {
            inputOpen = !inputOpen;

            if (!inputOpen)
            {
                arr_add((TodoRecord){.name = str_duplicate(&inputText), .done = false, .active = true});
            }
            else
            {
                str_zero(&inputText);
            }
        }

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

            DrawRectangle(0, 0, GetScreenWidth(), grabBarSize, LIGHTGRAY);
            DrawText(inputText.begin, 0, 0, grabBarSize, RED);
        }

        if (IsKeyPressed(KEY_T))
        {
            SetWindowState(FLAG_WINDOW_TOPMOST);
        }

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int xy = grabBarSize;
        for (int i = 0; i < records.length; i++)
        {
            TodoRecord* rec = &records.data[i];
            if (!rec->active)
                continue;
            Rectangle rect = (Rectangle){0, xy, grabBarSize, grabBarSize};
            DrawRectangleRec(rect, rec->done ? GREEN : RED);
            if (CheckCollisionPointRec(mousePosition, rect) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            {
                rec->done = !rec->done;
            }

            rect.width = sw - grabBarSize;
            rect.x += grabBarSize;

            if (CheckCollisionPointRec(mousePosition, rect) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_LEFT_CONTROL))
            {
                arr_remove(i);
            }

            DrawRectangleRec(rect, WHITE);
            DrawText(rec->name.begin, rect.x + 2, rect.y, grabBarSize, BLACK);
            xy += grabBarSize;
        }
        

        EndDrawing();
    }

    save_data();
    
    return 0;
}