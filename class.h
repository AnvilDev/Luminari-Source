/**
* @file class.h
* Header file for class specific functions and variables.
*
* Part of the core tbaMUD source code distribution, which is a derivative
* of, and continuation of, CircleMUD.
*
* All rights reserved.  See license for complete information.
* Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University
* CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.
*
*/
#ifndef _CLASS_H_
#define _CLASS_H_

/* defines */
#define NUM_CHURCHES 13

#define LF_CLASS    0
#define LF_RACE     1
#define LF_STACK    2
#define LF_MIN_LVL  3
#define LF_FEAT     4
#define LEVEL_FEATS 5
#define MAX_NUM_TITLES 11
#define NUM_PREFERRED_SAVES 5
/* end defines */

/* spell data for class */
struct class_spell_assign {
  int spell_num; /*spellnum like SPELL_ARMOR */
  int circle; /*actually level class receives*/
  struct class_spell_assign *next; /*linked list*/
};

/* prereq data for class */
struct class_prerequisite {
  /* FEAT_PREREQ_* values determine the type */
  int  prerequisite_type;
  char *description; /* Generated string value describing prerequisite. */

  /* 0: ability score, class, feat, race, casting type, BAB
   * 1: ability score value, class, feat, race, prep type, min BAB
   * 2: N/A, class level, feat ranks, N/A, minimum circle, N/A */
  int values[3];

  /* Linked list */
  struct class_prerequisite *next;
};


/* class data, layout for storing class information for each class */
struct class_table {
  char *name; /* full name of class, ex. wizard (no color) */
  char *abbrev; /* abbreviation of class, ex. wiz (no color) */
  char *colored_abbrev; /* same as abbrev, but colored */
  char *menu_name; /* colored full name of class for menu(s) */
  int max_level; /* maximum number of levels you can take in this class, -1 unlimited */
  bool locked_class; /* whether by default this class is locked or not */
  bool prestige_class; /* prestige class? */
  int base_attack_bonus; /* whether high, medium or low */
  int hit_dice; /* how many hp this class can get on level up */
  int mana_gain; /* how much mana this class gets on level up */
  int move_gain; /* how much moves this class gets on level up */
  int trains_gain; /* how many trains this class gets before int bonus */
  bool in_game; /* class currently in the game? */
  int unlock_cost; /* if locked, cost to unlock in account xp */
  /*!(CLASS_LEVEL(ch, class) % EPIC_FEAT_PROGRESSION) && IS_EPIC(ch)*/
  int epic_feat_progression;
  char *descrip; /* class description */
  
  char *titles[MAX_NUM_TITLES];  /* titles every 5 levels, 3 staff, default */
  int preferred_saves[NUM_PREFERRED_SAVES];  /*high or low saving throw values */
  int class_abil[NUM_ABILITIES];  /*class ability (not avail, cross-class, class-skill)*/
  
  struct class_spell_assign *spellassign_list; /* list of spell assigns */
  struct class_prerequisite *prereq_list; /* A list of prerequisite sctructures */
};

extern struct class_table class_list[];

/* Functions available through class.c */
bool display_class_info(struct char_data *ch, char *classname);
int backstab_mult(struct char_data *ch);
void do_start(struct char_data *ch);
void newbieEquipment(struct char_data *ch);
bitvector_t find_class_bitvector(const char *arg);
int invalid_class(struct char_data *ch, struct obj_data *obj);
int level_exp(struct char_data *ch, int level);
int parse_class(char arg);
int parse_class_long(char *arg);
void roll_real_abils(struct char_data *ch);
byte saving_throws(struct char_data *, int type);
int BAB(struct char_data *ch);
const char *titles(int chclass, int level);
int modify_class_ability(struct char_data *ch, int ability, int class);
void init_class(struct char_data *ch, int class, int level);
void load_class_list(void);

ACMD(do_classlist);

/* Global variables */

#ifndef __CLASS_C__

extern const char *church_types[];
extern int prac_params[][NUM_CLASSES];
extern struct guild_info_type guild_info[];
extern int level_feats[][LEVEL_FEATS];
extern const int *class_bonus_feats[NUM_CLASSES];

#endif /* __CLASS_C__ */

#endif /* _CLASS_H_*/
