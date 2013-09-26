/********************************************************************
 * Name:   mysql.h 
 * Author: Ornir (James McLaughlin)
 *
 * MySQL Header file for Luminari MUD.
 ********************************************************************/

#ifndef _MYSQL_H
#define _MYSQL_H 

#include <mysql/mysql.h> /* System headerfile for mysql. */

void connect_to_mysql();
void disconnect_from_mysql();

/* Wilderness */
struct wilderness_data* load_wilderness(zone_vnum zone);
void load_regions();
struct region_list* get_enclosing_regions(zone_rnum zone, int x, int y);
#endif
