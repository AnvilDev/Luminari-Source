/*
 * File:   domains_schools.h
 * Author: Zusuk
 *
 */

#ifndef DOMAINS_SCHOOLS_H
#define	DOMAINS_SCHOOLS_H

#ifdef	__cplusplus
extern "C" {
#endif
/******************************************/

/* if these two values are changed, you have to adjust the respect add_
 functions */
#define MAX_GRANTED_POWERS      5
#define MAX_DOMAIN_SPELLS       9

#define NOSCHOOL           0
#define SCHOOL_NOSCHOOL    0
#define ABJURATION     1
#define CONJURATION    2
#define DIVINATION     3
#define ENCHANTMENT    4
#define EVOCATION      5
#define ILLUSION       6
#define NECROMANCY     7
#define TRANSMUTATION  8
/****************/
#define NUM_SCHOOLS    9

/* domains are in structs.h */

#define DOMAIN_POWER_UNDEFINED                0
#define DOMAIN_POWER_LIGHTNING_ARC            1
#define DOMAIN_POWER_ELECTRICITY_RESISTANCE   2
#define DOMAIN_POWER_ACID_DART                3
#define DOMAIN_POWER_ACID_RESISTANCE          4
#define DOMAIN_POWER_FIRE_BOLT                5
#define DOMAIN_POWER_FIRE_RESISTANCE          6
#define DOMAIN_POWER_ICICLE                   7
#define DOMAIN_POWER_COLD_RESISTANCE          8
  /****************/
#define NUM_DOMAIN_POWERS                     9
/****************/

  /****************/
  /****************/

struct domain_info {
  char *name;
  int granted_powers[MAX_GRANTED_POWERS];
  int domain_spells[MAX_DOMAIN_SPELLS];
  ubyte favored_weapon;
  char *description;
};
struct domain_info domain_list[NUM_DOMAINS];

struct school_info {
  char *name;
  int ethos;
  int alignment;
  ubyte domains[6];
  ubyte favored_weapon;
  sbyte pantheon;
  char *portfolio;
  char *description;
};
struct school_info school_list[NUM_SCHOOLS];

/******************************************/

void assign_domains(void);
void init_domain_spell_level(void);
void assign_domain_spells(struct char_data *ch);
int has_domain_power(struct char_data *ch, int domain_power);

/******************************************/

ACMD(do_domain);

/******************************************/

#ifdef	__cplusplus
}
#endif

#endif	/* DOMAINS_SCHOOLS_H */

