
#ifndef _WILDERNESS_H_
#define _WILDERNESS_H_

#define X_COORD 0
#define Y_COORD 1

#define WILD_X_SIZE 2048
#define WILD_Y_SIZE 2048

#define WILD_ZONE_VNUM 10000

#define WILD_ROOM_VNUM_START         1000000 /* The start of the STATIC wilderness rooms. */
#define WILD_ROOM_VNUM_END           1003999 /* The end of the STATIC wilderness rooms. */

#define WILD_DYNAMIC_ROOM_VNUM_START 1004000 /* The start of the vnums for the dynamic room pool. */
#define WILD_DYNAMIC_ROOM_VNUM_END   1005999 /* The end of the vnums for the dynamic room pool. */

/* Utility macros */
#define IS_DYNAMIC(rnum) ((world[rnum].number >= WILD_DYNAMIC_ROOM_VNUM_START) && \
                          (world[rnum].number <= WILD_DYNAMIC_ROOM_VNUM_END))

#define WATERLINE               138
#define SHALLOW_WATER_THRESHOLD  20 
#define COASTLINE_THRESHOLD      5 // 10
#define PLAINS_THRESHOLD         35
#define HIGH_MOUNTAIN_THRESHOLD  55
#define MOUNTAIN_THRESHOLD       70
#define HILL_THRESHOLD           80

#define TERRAIN_TILE_TYPE_SHALLOW_WATER  "\tB~\tn"
#define TERRAIN_TILE_TYPE_DEEP_WATER     "\tb~\tn"
#define TERRAIN_TILE_TYPE_COASTLINE      "\tY.\tn"
#define TERRAIN_TILE_TYPE_PLAINS         "\tG.\tn"
#define TERRAIN_TILE_TYPE_MOUNTAIN       "\tw^\tn"
#define TERRAIN_TILE_TYPE_HILL           "\tyn\tn"
#define TERRAIN_TILE_TYPE_FOREST         "\tgY\tn"

/* Wilderness noise definitions */
#define NOISE_MATERIAL_PLANE_ELEV       0
#define NOISE_MATERIAL_PLANE_ELEV_DIST  1
#define NOISE_MATERIAL_PLANE_MOISTURE   2

#define NUM_NOISE                       3 /* Always < MAX_GENERATED_NOISE (24) in perlin.h */

#define NOISE_MATERIAL_PLANE_ELEV_SEED         822344//113//3193//300 //242423 //Yang //3743
#define NOISE_MATERIAL_PLANE_MOISTURE_SEED     834//133//3//6737
#define NOISE_MATERIAL_PLANE_ELEV_DIST_SEED    74233//8301 

/* Region types. */
#define REGION_GEOGRAPHIC	1
#define REGION_ENCOUNTER	2
#define REGION_SECTOR_TRANSFORM 3
#define REGION_SECTOR 4

/* Path types. */
#define PATH_ROAD       1 /* Path Props = Sector type to convert to. */
#define PATH_RIVER      2
#define PATH_GEOGRAPHIC 3 

/* Map shapes */
#define WILD_MAP_SHAPE_CIRCLE 1
#define WILD_MAP_SHAPE_RECT   2

extern struct kdtree* kd_wilderness_rooms;

struct vertex {
  int x;
  int y;

};

struct region_data {

  region_vnum vnum; /* Vnum for this region. */
  region_rnum rnum; /* Array index for this region. */
  
  zone_rnum zone;   /* Zone that contains this region. */
  char *name;       /* Name of the region. */

  int region_type;  /* Type of region. */
  int region_props; /* Name: Value pairs, stores data based on region_type. */

  struct vertex *vertices; /* Vertex list. */  
  int num_vertices;        /* The number of vertices. */

};

struct path_data {

  region_vnum vnum; /* Vnum for this path. */
  region_rnum rnum; /* Array index for this path. */
  
  zone_rnum zone;   /* Zone that contains this path. */
  char *name;       /* Name of the path. */

  int path_type;  /* Type of region. */
  int path_props; /* Name: Value pairs, stores data based on region_type. */

  struct vertex *vertices; /* Vertex list. */  
  int num_vertices;        /* The number of vertices. */

};

struct sector_limits {

  int sector_type;
  int min_value;
  int max_value;

};

struct wilderness_data {

  int id;

  int elevation_seed;
  int distortion_seed;
  int moisture_seed;

  int x_size; /* X size, centered on (0,0) */
  int y_size; /* Y size, centered on (0,0) */

  zone_rnum zone; /* ZOne assocuated with this wilderness. */

  room_vnum nav_vnum;
  room_vnum dynamic_vnum_pool_start;
  room_vnum dynamic_vnum_pool_end;

  int min_temp;
  int max_temp;

  struct sector_limits sector_map[NUM_ROOM_SECTORS][3]; /* Elevation, moisture, temp mappings to sectors. */

  struct kdtree* kd_wilderness_rooms; /* KDTree of the static rooms, for speed. */
  
  /* Regarding Regions - First I am going to try to leverage the use of an index-table in the 
   * database, using MyIsam and the spatial extensions that allows for very very quick spatial
   * queries.  Therefore there is no link between wilderness and region, since regions are contained
   * in their own global array. */

};

/* Mapping */

struct wild_map_tile {
  int vis;
  int sector_type;
  char *glyph;
};

void get_map(int xsize, int ysize, int center_x, int center_y, struct wild_map_tile **map);
int get_sector_type(int elevation, int temperature, int moisture);
void show_wilderness_map(struct char_data *ch, int size, int x, int y);
void save_map_to_file(const char *fn, int xsize, int ysize);
void save_noise_to_file(int idx, const char* fn, int xsize, int ysize, int zoom);
char *gen_ascii_wilderness_map(int size, int x, int y);

/* Wilderness */

void initialize_wilderness_lists();
room_rnum find_available_wilderness_room(); /* Get the next empty room in the pool. */
room_rnum find_room_by_coordinates(int x, int y); /* Get the room at coordinates (x,y) */
room_rnum find_static_room_by_coordinates(int x, int y);
void assign_wilderness_room(room_rnum room, int x, int y); /* Assign the room to the provided coordinates, adjusting descriptions, etc. */


/* Regions */
/*
void region_add(struct wilderness_data *wild, struct region_data *region);
void region_delete(struct wilderness_data *wild, region_rnum region);

int  region_contains_point(region_rnum region, int x, int y);
 
void region_add_vertex    (region_rnum region, int x, int y);
int  region_delete_vertex (region_rnum region, int x, int y);
*/

/* Struct for returning a list of containing regions. */
struct region_list {
  region_rnum rnum;
  struct region_list* next;
};

/* Struct for returning a list of containing paths. */
struct path_list {
  region_rnum rnum;
  char *glyph;
  struct path_list* next;
};
#endif
