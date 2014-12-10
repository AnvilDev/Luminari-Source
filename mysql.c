#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "modify.h"
#include "mysql.h"

#include "wilderness.h"

MYSQL *conn = NULL;

void after_world_load()
{
}

void connect_to_mysql()
{
  char host[128], database[128], username[128], password[128];
  char line[128], key[128], val[128];
  FILE *file;

  /* Read the mysql configuration file */
  if (!(file = fopen("mysql_config", "r"))) {
    log("SYSERR: Unable to read MySQL configuration.");
    exit(1);
  }

  /* fgets includes newline character */
  while (!feof(file) && fgets(line, 127, file)) {
    if (*line == '#' || strlen(line) <= 1)
      continue; /* comment or empty line */
    else if (sscanf(line, "%s = %s", key, val) == 2) {
      if (!str_cmp(key, "mysql_host"))
        strcpy(host, val);
      else if (!str_cmp(key, "mysql_database"))
        strcpy(database, val);
      else if (!str_cmp(key, "mysql_username"))
        strcpy(username, val);
      else if (!str_cmp(key, "mysql_password"))
        strcpy(password, val);
      else {
        log("SYSERR: Unknown line in MySQL configuration: %s", line);
      }
    }
    else {
      log("SYSERR: Unknown line in MySQL configuration: %s", line);
      exit(1);
    }
  }

  if (fclose(file)) {
    log("SYSERR: Unable to read MySQL configuration.");
    exit(1);
  }

  if (mysql_library_init(0, NULL, NULL)) {
    log("SYSERR: Unable to initialize MySQL library.");
    exit(1);
  }

  if (!(conn = mysql_init(NULL))) {
    log("SYSERR: Unable to initialize MySQL connection.");
    exit(1);
  }

  my_bool reconnect = 1;
  mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

  if (!mysql_real_connect(conn, host, username, password, database, 0, NULL, 0)) {
    log("SYSERR: Unable to connect to MySQL: %s", mysql_error(conn));
    exit(1);
  }
}

void disconnect_from_mysql()
{
  mysql_close(conn);
  mysql_library_end();
}


/* Load the wilderness data for the specified zone. */
struct wilderness_data* load_wilderness(zone_vnum zone) {

  MYSQL_RES *result;
  MYSQL_ROW row;
  char buf[1024];

  struct wilderness_data *wild = NULL;

  log("INFO: Loading wilderness data for zone: %d", zone);

  sprintf(buf, "SELECT f.id, f.nav_vnum, f.dynamic_vnum_pool_start, f.dynamic_vnum_pool_end, f.x_size, f.y_size, f.elevation_seed, f.distortion_seed, f.moisture_seed, f.min_temp, f.max_temp from wilderness_data as f where f.zone_vnum = %d", zone);


  if (mysql_query(conn, buf)) {
    log("SYSERR: Unable to SELECT from wilderness_data: %s", mysql_error(conn));
    exit(1);
  } 
  
  if (!(result = mysql_store_result(conn))) {
    log("SYSERR: Unable to SELECT from wilderness_data: %s", mysql_error(conn));
    exit(1);
  }

  if (mysql_num_rows(result) > 1) {
    log("SYSERR: Too many rows returned on SELECT from wilderness_data for zone: %d", zone);
  }
 
  CREATE(wild, struct wilderness_data, 1);
  
  /* Just use the first row. */
  row = mysql_fetch_row(result);

  if (row) {
    wild->id        = atoi(row[0]);
    wild->zone      = real_zone(zone);
    wild->nav_vnum  = atoi(row[1]);
    wild->dynamic_vnum_pool_start = atoi(row[2]);
    wild->dynamic_vnum_pool_end   = atoi(row[3]);
    wild->x_size    = atoi(row[4]);
    wild->y_size    = atoi(row[5]);
    wild->elevation_seed  = atoi(row[6]);
    wild->distortion_seed = atoi(row[7]); 
    wild->moisture_seed   = atoi(row[8]);
    wild->min_temp        = atoi(row[9]);
    wild->max_temp        = atoi(row[10]); 
  } 

  mysql_free_result(result);
  return wild;   
}

/* String tokenizer. */
char** tokenize(const char* input, const char* delim)
{
    char* str = strdup(input);
    int count = 0;
    int capacity = 10;
    char** result = malloc(capacity*sizeof(*result));

    char* tok=strtok(str,delim); 

    while(1)
    {
        if (count >= capacity)
            result = realloc(result, (capacity*=2)*sizeof(*result));

        result[count++] = tok? strdup(tok) : tok;

        if (!tok) break;

        tok=strtok(NULL,delim);
    } 

    free(str);
    return result;
}


void load_regions() {
  /* region_data* region_table */

  MYSQL_RES *result;
  MYSQL_ROW row;

  int i = 0, vtx = 0;
  int numrows;

  char buf[1024];
  char buf2[1024];

  char** tokens;  /* Storage for tokenized linestring points */
  char** it;      /* Token iterator */

  log("INFO: Loading region data from MySQL");

  sprintf(buf, "SELECT vnum, "
                      "zone_vnum, " 
                      "name, "
                      "region_type, "
                      "NumPoints(ExteriorRing(`region_polygon`)), "
                      "AsText(ExteriorRing(region_polygon)), "
                      "region_props "
               "  from region_data");


  if (mysql_query(conn, buf)) {
    log("SYSERR: Unable to SELECT from region_data: %s", mysql_error(conn));
    exit(1);
  }

  if (!(result = mysql_store_result(conn))) {
    log("SYSERR: Unable to SELECT from region_data: %s", mysql_error(conn));
    exit(1);
  }

 if ( (numrows = mysql_num_rows(result)) < 1) 
   return;
 else {
    /* Allocate memory for all of the region data. */
    CREATE(region_table, struct region_data, numrows);
  }
 
  while ((row = mysql_fetch_row(result))) { 
    region_table[i].vnum         = atoi(row[0]);
    region_table[i].rnum         = i;
    region_table[i].zone         = real_zone(atoi(row[1]));
    region_table[i].name         = strdup(row[2]);
    region_table[i].region_type  = atoi(row[3]);
    region_table[i].num_vertices = atoi(row[4]);
    region_table[i].region_props = atoi(row[5]);

    /* Parse the polygon text data to get the vertices, etc.
       eg: LINESTRING(0 0,10 0,10 10,0 10,0 0) */
    sscanf(row[5], "LINESTRING(%[^)])", buf2);
    tokens = tokenize(buf2, ",");
   
    CREATE(region_table[i].vertices, struct region_vertex, region_table[i].num_vertices);

    vtx = 0;

    for(it=tokens; it && *it; ++it) {
      sscanf(*it, "%d %d", &(region_table[i].vertices[vtx].x), &(region_table[i].vertices[vtx].y));
      vtx++;
      free(*it);
    }      

    top_of_region_table = i; 
    i++;
  } 
  mysql_free_result(result);
}

struct region_list* get_enclosing_regions(zone_rnum zone, int x, int y) {
  MYSQL_RES *result;
  MYSQL_ROW row;

  struct region_list *regions = NULL;
  struct region_list *new_node = NULL; 

 
  char buf[1024];
 
  sprintf(buf, "SELECT vnum "
               "  from region_index "
               "  where zone_vnum = %d "
               "  and GISWithin(GeomFromText('POINT(%d %d)'), region_polygon)",
               zone_table[zone].number, x, y);
 
  /* Check the connection, reconnect if necessary. */
  mysql_ping(conn);

  if (mysql_query(conn, buf)) {
    log("SYSERR: Unable to SELECT from region_index: %s", mysql_error(conn));
    exit(1);
  }
 
  if (!(result = mysql_store_result(conn))) {
    log("SYSERR: Unable to SELECT from region_index: %s", mysql_error(conn));
    exit(1);
  }
  
  while ((row = mysql_fetch_row(result))) {
 
    /* Allocate memory for the region data. */
    CREATE(new_node, struct region_list, 1);
    new_node->rnum = real_region(atoi(row[0]));
    new_node->next = regions;
    regions = new_node;
    new_node = NULL; 
  }
  mysql_free_result(result);

  return regions;
}

void save_regions() {

}

#ifdef DO_NOT_COMPILE_EXAMPLES

void who_to_mysql()
{
  struct descriptor_data *d;
  struct char_data *tch;
  char buf[1024], buf2[MAX_TITLE_LENGTH * 2];

  if (mysql_query(conn, "DELETE FROM who")) {
    mudlog(NRM, LVL_GOD, TRUE, "SYSERR: Unable to DELETE in who: %s", mysql_error(conn));
    return;
  }

  for (d = descriptor_list; d; d = d->next) {
    if (d->original)
      tch = d->original;
    else if (!(tch = d->character))
      continue;

    /* Hide those who are not 'playing' */
    if (!IS_PLAYING(d))
      continue;
    /* Hide invisible immortals */
    if (GET_INVIS_LEV(tch) > 1)
      continue;
    /* Hide invisible and hidden mortals */
    if (AFF_FLAGGED((tch), AFF_INVISIBLE) || AFF_FLAGGED((tch), AFF_HIDE))
      continue;

    /* title could have ' characters, we need to escape it */
    /* the mud crashed here on 16 Oct 2012, made some changes and checks */
    if (GET_TITLE(tch) != NULL && strlen(GET_TITLE(tch)) <= MAX_TITLE_LENGTH)
      mysql_real_escape_string(conn, buf2, GET_TITLE(tch), strlen(GET_TITLE(tch)));
    else
      buf2[0] = '\0';

    /* Hide level for anonymous players */
    if (PRF_FLAGGED(tch, PRF_ANON)) {
      sprintf(buf, "INSERT INTO who (player, title, killer, thief) VALUES ('%s', '%s', %d, %d)",
        GET_NAME(tch), buf2, PLR_FLAGGED(tch, PLR_KILLER) ? 1 : 0, PLR_FLAGGED(tch, PLR_THIEF) ? 1 : 0);
    }
    else {
      sprintf(buf, "INSERT INTO who (player, level, title, killer, thief) VALUES ('%s', %d, '%s', %d, %d)",
        GET_NAME(tch), GET_LEVEL(tch), buf2,
        PLR_FLAGGED(tch, PLR_KILLER) ? 1 : 0, PLR_FLAGGED(tch, PLR_THIEF) ? 1 : 0);
    }

    if (mysql_query(conn, buf)) {
      mudlog(NRM, LVL_GOD, TRUE, "SYSERR: Unable to INSERT in who: %s", mysql_error(conn));
    }
  }
}


void read_factions_from_mysql()
{
  struct faction_data *fact = NULL;
  MYSQL_RES *result;
  MYSQL_ROW row;
  int i = 0, total;

  if (mysql_query(conn, "SELECT f.id, f.name, f.flags, f.gold, f.tax, COUNT(r.rank) FROM factions AS f LEFT JOIN faction_ranks AS r ON f.id = r.faction GROUP BY f.id ORDER BY f.name")) {
    log("SYSERR: Unable to SELECT from factions: %s", mysql_error(conn));
    exit(1);
  }

  if (!(result = mysql_store_result(conn))) {
    log("SYSERR: Unable to SELECT from factions: %s", mysql_error(conn));
    exit(1);
  }

  faction_count = mysql_num_rows(result);
  CREATE(factions, struct faction_data, faction_count);

  while ((row = mysql_fetch_row(result))) {
    factions[i].id = strdup(row[0]);
    factions[i].name = strdup(row[1]);
    factions[i].flags = atoi(row[2]);
    factions[i].gold = atoll(row[3]);
    factions[i].tax = atof(row[4]);
    factions[i].num_ranks = atoi(row[5]);

    if (factions[i].num_ranks > 0)
      CREATE(factions[i].ranks, struct faction_rank, factions[i].num_ranks);

    i++;
  }

  mysql_free_result(result);

  /* Read faction ranks */
  if (mysql_query(conn, "SELECT faction, rank, name FROM faction_ranks ORDER BY faction, rank")) {
    log("SYSERR: Unable to SELECT from faction_ranks: %s", mysql_error(conn));
    exit(1);
  }

  if (!(result = mysql_store_result(conn))) {
    log("SYSERR: Unable to SELECT from faction_ranks: %s", mysql_error(conn));
    exit(1);
  }

  while ((row = mysql_fetch_row(result))) {
    /* Select correct faction */
    if (!fact || strcmp(fact->id, row[0]))
        fact = find_faction(row[0], NULL);

    /* If we were unable to select the correct faction, exit with a serious error */
    if (!fact) {
      log("SYSERR: Rank for unexisting faction %s.", row[0]);
      exit(1);
    }

    fact->ranks[atoi(row[1])].name = strdup(row[2]);
  }

  mysql_free_result(result);

  /* Read faction skillgroups */
  if (mysql_query(conn, "SELECT faction_id, skillgroup_id FROM faction_skillgroups ORDER BY faction_id")) {
    log("SYSERR: Unable to SELECT from faction_skillgroups: %s", mysql_error(conn));
    exit(1);
  }
  if (!(result = mysql_store_result(conn))) {
    log("SYSERR: Unable to SELECT from faction_skillgroups: %s", mysql_error(conn));
    exit(1);
  }

  while ((row = mysql_fetch_row(result))) {
    /* Select correct faction */
    if (!fact || strcmp(fact->id, row[0]))
        fact = find_faction(row[0], NULL);

    /* If we were unable to select the correct faction, exit with a serious error */
    if (!fact) {
      log("SYSERR: Skillgroup for unexisting faction %s.", row[0]);
      exit(1);
    }

    i = atoi(row[1]);

    if (i < 0 || i >= NUM_SKILLGROUPS) {
      log("SYSERR: Invalid skillgroup (%d) for faction %s.", i, fact->id);
      exit(1);
    }

    fact->skillgroups[i] = 1;
  }

  mysql_free_result(result);

  /* Read monster ownership */
  if (mysql_query(conn, "SELECT monster, faction, count FROM mobile_shares ORDER BY count DESC")) {
    log("SYSERR: Unable to SELECT from mobile_shares: %s", mysql_error(conn));
    exit(1);
  }
  if (!(result = mysql_store_result(conn))) {
    log("SYSERR: Unable to SELECT from mobile_shares: %s", mysql_error(conn));
    exit(1);
  }

  while ((row = mysql_fetch_row(result))) {
    if ((fact = find_faction(row[1], NULL))) {
      i = atoi(row[0]);
      total = atoi(row[2]);

      if (i <= 0 || total <= 0 || total > TOTAL_SHARES)
        log("SYSERR: Invalid mob %s or sharecount %s for faction %s.", row[0], row[2], row[1]);
      else
        set_monster_ownership(i, fact, total);
    }
    else {
      log("SYSERR: Mob %s owns shares in unknown faction %s.", row[0], row[1]);
    }
  }

  mysql_free_result(result);

  /* Read player ownership */
  if (mysql_query(conn, "SELECT player, faction, count FROM player_shares ORDER BY count DESC")) {
    log("SYSERR: Unable to SELECT from player_shares: %s", mysql_error(conn));
    exit(1);
  }
  if (!(result = mysql_store_result(conn))) {
    log("SYSERR: Unable to SELECT from player_shares: %s", mysql_error(conn));
    exit(1);
  }

  while ((row = mysql_fetch_row(result))) {
    if ((fact = find_faction(row[1], NULL))) {
      total = atoi(row[2]);

      if (total <= 0 || total > TOTAL_SHARES)
        log("SYSERR: Invalid player %s sharecount %s for faction %s.", row[0], row[2], row[1]);
      else
        set_player_ownership(row[0], fact, total);
    }
    else {
      log("SYSERR: Player %s owns shares in unknown faction %s.", row[0], row[1]);
    }
  }

  mysql_free_result(result);

  /* Check that we don't go above TOTAL_SHARES */
  for (i = 0; i < faction_count; i++) {
    total = faction_total_ownership(&factions[i]);
    if (total > TOTAL_SHARES) {
      log("SYSERR: Faction %s has %d total shares (max %d).", factions[i].id, total, TOTAL_SHARES);
    }
  }
}

#endif
