#pragma once
#include <SDL2/SDL.h>

enum
{
    UI_SIZE_MANUAL,
    UI_SIZE_INHERIT,
    UI_SIZE_AUTO
};

enum
{
    UI_STYLE_LEFT,
    UI_STYLE_CENTERED,
    UI_STYLE_RIGHT,
    UI_STYLE_LEFT_RIGHT,
};

struct UIFlags
{
    unsigned int width : 2;
    unsigned int height : 2;
    unsigned int style : 2;
    unsigned int needs_refresh : 1;
    unsigned int needs_resize : 1;
};

typedef struct UIElement
{
    SDL_Rect space, clickable_box;
    SDL_Texture *texture_cache;
    int margin_left, margin_right, margin_top, margin_bottom;
    char *label;
    struct UIFlags flags;
    int (*focused_event_callback)(struct UIElement *, SDL_Event);
    int (*click_event_callback)(struct UIElement *, int, int);
    enum element_type { CHECKBOX_ELEMENT, DROPDOWN_ELEMENT };
    union
    {
        struct checkbox { int *value; };
        struct dropdown { size_t options_count; char **options_labels; int *selected_value; };
    };
    size_t children_count;
    struct UIElement *children;
    struct UIElement *parent;
};

void updatePositionsAndSizes(UIElement *ui_element)
{
    if (ui_element->children_count)
    {
        int total_height = 0;
        int max_child_width = 0;
        for (int i = 0; i < ui_element->children_count; i++)
        {
            updatePositionsAndSizes(&ui_element->children[i]);
            total_height += ui_element->children[i].space.h;
            max_child_width = (ui_element->children[i].space.w > max_child_width) ? ui_element->children[i].space.w : max_child_width;
        }
        if (ui_element->flags.height == )
    }
}