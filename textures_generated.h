SDL_Texture* tile_textures[256] = { NULL };
enum
{
	AIR_TILE,
	COBBLE_TILE,
	GRASS_TILE,
	GRASS_ROCKS_TILE,
	HOT_GRASS_TILE,
	HOT_GRASS_ROCKS_TILE,
	SNOW_GRASS_TILE,
	SNOW_GRASS_ROCKS_TILE,
	STONE_BRICKS_1_TILE,
	STONE_BRICKS_2_TILE,
	STONE_BRICKS_3_TILE,
	STONE_BRICKS_TILE,
	WATER_TILE
};
void loadAllTextures(SDL_Renderer *renderer)
{
	SDL_Surface *temp_surface;
	temp_surface = SDL_LoadBMP("air.bmp");
	tile_textures[AIR_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("cobble.bmp");
	tile_textures[COBBLE_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("grass.bmp");
	tile_textures[GRASS_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("grass_rocks.bmp");
	tile_textures[GRASS_ROCKS_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("hot_grass.bmp");
	tile_textures[HOT_GRASS_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("hot_grass_rocks.bmp");
	tile_textures[HOT_GRASS_ROCKS_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("snow_grass.bmp");
	tile_textures[SNOW_GRASS_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("snow_grass_rocks.bmp");
	tile_textures[SNOW_GRASS_ROCKS_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("stone_bricks_1.bmp");
	tile_textures[STONE_BRICKS_1_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("stone_bricks_2.bmp");
	tile_textures[STONE_BRICKS_2_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("stone_bricks_3.bmp");
	tile_textures[STONE_BRICKS_3_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("stone_bricks.bmp");
	tile_textures[STONE_BRICKS_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
	temp_surface = SDL_LoadBMP("water.bmp");
	tile_textures[WATER_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
	SDL_FreeSurface(temp_surface);
}
