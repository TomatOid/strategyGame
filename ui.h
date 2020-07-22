#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

SDL_Texture *checkbox_unchecked;
SDL_Texture *checkbox_checked;
#define DROPDOWN_MARGIN 2

enum
{
    UI_SIZE_MANUAL,
    UI_SIZE_INHERIT,
    UI_SIZE_AUTO
};

enum
{
    UI_LAYOUT_CENTERED,
    UI_LAYOUT_LEFT,
    UI_LAYOUT_RIGHT,
    UI_LAYOUT_LEFT_RIGHT,
};

enum { CHECKBOX_ELEMENT, DROPDOWN_ELEMENT, ENTRY_BOX_ELEMENT };

struct UIFlags
{
    unsigned int width : 2;
    unsigned int height : 2;
    unsigned int layout : 2;
    unsigned int needs_refresh : 1;
    unsigned int is_focused : 1;
};

typedef struct UIElement
{
    SDL_Rect space, clickable_box;
    SDL_Texture *texture_cache;
    int margin_left, margin_right, margin_top, margin_bottom;
    char *label;
    struct UIFlags flags;
    int (*focused_event_callback)(struct UIElement *, SDL_Event); // this function returns one if the event was handled.
    int (*click_event_callback)(struct UIElement *, int, int);
    short element_type;
    union
    {
        struct { int *value; } checkbox;
        struct { size_t options_count; char **options_labels; int *selected_value; } dropdown;
    };
};

// this function returns the y-value of the bottom of this element
// UI is so boring, I just want to get this over with.
// This is the reason for the mediocre quality. Chances are this won't even be used
// in the main game, just the level editor and settings menu
int drawUIElement(UIElement *ui_element, TTF_Font *font, SDL_Color color, SDL_Renderer *renderer, SDL_Texture *window_texture, SDL_Rect window_rectangle)
{
    if (ui_element->flags.needs_refresh)
    {
        SDL_DestroyTexture(ui_element->texture_cache);
        SDL_Surface *text_surface = TTF_RenderText_Solid(font, ui_element->label, color);
        SDL_Texture *label_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
        SDL_FreeSurface(text_surface);
        // Now make a surface for the checkbox / dropdown / etc
        SDL_Texture *element_texture;
        SDL_SetRenderTarget(renderer, element_texture);
        int element_height, element_width;
        switch (ui_element->element_type)
        {
        case CHECKBOX_ELEMENT:
            element_height = 7;
            element_width = 7;
            if (ui_element->checkbox.value && *ui_element->checkbox.value)
            {
                if (!checkbox_checked)
                {
                    SDL_Surface *temp = SDL_LoadBMP("ui_textures/checkbox_checked.bmp");
                    checkbox_checked = SDL_CreateTextureFromSurface(renderer, temp);
                    SDL_FreeSurface(temp);
                }
                SDL_RenderCopy(renderer, checkbox_checked, NULL, NULL);
            }
            else
            {
                if (!checkbox_unchecked)
                {
                    SDL_Surface *temp = SDL_LoadBMP("ui_textures/checkbox_unchecked.bmp");
                    checkbox_unchecked = SDL_CreateTextureFromSurface(renderer, temp);
                    SDL_FreeSurface(temp);
                }
                SDL_RenderCopy(renderer, checkbox_unchecked, NULL, NULL);
            }
            break;
        case DROPDOWN_ELEMENT:
        {
            int temp_width;
            int element_width = 0;
            // We want the box to be able to support the largest text
            for (int i = 0; i < ui_element->dropdown.options_count; i++)
            {
                TTF_SizeText(font, ui_element->dropdown.options_labels[i], &temp_width, &element_height);
                if (temp_width > element_width) { element_width = temp_width; }
            }
            SDL_Rect background_box = { 0, 0, element_width + 2 * DROPDOWN_MARGIN + element_height, (ui_element->flags.is_focused ? 1 : ui_element->dropdown.options_count) * (element_height + 2 * DROPDOWN_MARGIN) };
            
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE / 2);
            SDL_RenderFillRect(renderer, &background_box);
            // the selected element always gets drawn
            text_surface = TTF_RenderText_Solid(font, ui_element->dropdown.options_labels[*ui_element->dropdown.selected_value], color);
            SDL_Texture *text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
            SDL_FreeSurface(text_surface);
            TTF_SizeText(font, ui_element->dropdown.options_labels[*ui_element->dropdown.selected_value], &temp_width, NULL);
            SDL_Rect dest_rectangle = { DROPDOWN_MARGIN, DROPDOWN_MARGIN, temp_width, element_height };
            SDL_RenderCopy(renderer, text_texture, NULL, &dest_rectangle);
            SDL_DestroyTexture(text_texture);
            // if the element is focused, draw the full list
            if (ui_element->flags.is_focused)
            {
                for (int i = 0; i < ui_element->dropdown.options_count; i++)
                {
                    text_surface = TTF_RenderText_Solid(font, ui_element->dropdown.options_labels[i], color);
                    text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
                    SDL_FreeSurface(text_surface);
                    TTF_SizeText(font, ui_element->dropdown.options_labels[i], &temp_width, NULL);
                    dest_rectangle = { DROPDOWN_MARGIN * (2 + i) + element_height * (1 + i), DROPDOWN_MARGIN, temp_width, element_height };
                    SDL_RenderCopy(renderer, text_texture, NULL, &dest_rectangle);
                    SDL_DestroyTexture(text_texture);
                }
            }
            break;
        }
        default:
            break;
        }
        SDL_SetRenderTarget(renderer, ui_element->texture_cache);
        int label_width, label_height;
        SDL_QueryTexture(label_texture, NULL, NULL, &label_width, &label_height);
        SDL_Rect label_rectangle, element_rectangle;
        // determine layout based on style
        switch (ui_element->flags.layout)
        {
        // TODO: Center the element in the y axis
        case UI_LAYOUT_LEFT:
            // | LABEL <ELEMENT>     |
            label_rectangle = { ui_element->margin_left, ui_element->margin_top, label_width, label_height };
            element_rectangle = { 2 * ui_element->margin_left + label_width, ui_element->margin_top, element_width, element_height };
            break;
        case UI_LAYOUT_LEFT_RIGHT:
            // | LABEL     <ELEMENT> |
            label_rectangle = { ui_element->margin_left, ui_element->margin_top, label_width, label_height };
            element_rectangle = { ui_element->space.w - ui_element->margin_right - element_width, ui_element->margin_top, element_width, element_height };
            break;
        default:
            break;
        }
        // update the clickable box
        {
            int temp_width, temp_height;
            SDL_QueryTexture(element_texture, NULL, NULL, &temp_width, &temp_height);
            ui_element->clickable_box = { element_rectangle.x, element_rectangle.y, temp_width, temp_height };
        }
        SDL_RenderCopy(renderer, label_texture, NULL, &label_rectangle);
        SDL_RenderCopy(renderer, element_texture, NULL, &element_rectangle);
        SDL_DestroyTexture(label_texture);
        SDL_DestroyTexture(element_texture);
        // TODO: maybe store the initial value at the begining and restore it here
        SDL_SetRenderTarget(renderer, NULL);
    }
}