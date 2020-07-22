#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 256
const char *prefix = "tiles/";

void toAllCapsAndUnderScores(char *str)
{
    do
    {
        if (*str >= 'a' && *str <= 'z')
        {
            *str -= 'a' - 'A';
        }
        else if (*str == ' ' || *str == '-')
        {
            *str = '_';
        }
        else if (*str == '.')
        {
            *str = '\0';
        }
    } while (*(str++));
}

int main(int argc, char** argv)
{
    char buffer[BUFFER_SIZE] = { 0 };
    FILE *header_file = fopen("textures_generated.h", "w+");
    fprintf(header_file, "SDL_Texture* tile_textures[256] = { NULL };\n");
    // first, generate the enum
    fprintf(header_file, "enum\n{\n");
    if (argc < 2) exit(EXIT_FAILURE);
    strncpy(buffer, argv[1], BUFFER_SIZE - 1);
    toAllCapsAndUnderScores(buffer);
    fprintf(header_file, "\t%s_TILE", buffer);
    for (int i = 2; i < argc; i++)
    {
        strncpy(buffer, argv[i], BUFFER_SIZE - 1);
        toAllCapsAndUnderScores(buffer);
        fprintf(header_file, ",\n\t%s_TILE", buffer);
    }
    fprintf(header_file, "\n};\n");

    fprintf(header_file, "void loadAllTextures(SDL_Renderer *renderer)\n{\n\tSDL_Surface *temp_surface;");
    for (int i = 1; i < argc; i++)
    {
        strncpy(buffer, argv[i], BUFFER_SIZE - 1);
        toAllCapsAndUnderScores(buffer);
        fprintf(header_file, "\n\ttemp_surface = SDL_LoadBMP(\"%s%s\");", prefix, argv[i]);
        fprintf(header_file, "\n\ttile_textures[%s_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);", buffer);
        fprintf(header_file, "\n\tSDL_FreeSurface(temp_surface);");
    }
    fprintf(header_file, "\n}\n");
}
    
