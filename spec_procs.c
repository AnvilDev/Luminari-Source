/**************************************************************************
 *  File: spec_procs.c                                      Part of tbaMUD *
 *  Usage: Implementation of special procedures for mobiles/objects/rooms. *
 *                                                                         *
 *  All rights reserved.  See license for complete information.            *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 **************************************************************************/


#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "constants.h"
#include "act.h"
#include "spec_procs.h"
#include "class.h"
#include "fight.h"
#include "modify.h"
#include "house.h"
#include "clan.h"
#include "mudlim.h"
#include "graph.h"
#include "dg_scripts.h" /* for send_to_zone() */


/* locally defined functions of local (file) scope */
static int compare_spells(const void *x, const void *y);
static void npc_steal(struct char_data *ch, struct char_data *victim);
void zone_yell(struct char_data *ch, char buf[256]);

/* Special procedures for mobiles. */
int spell_sort_info[MAX_SKILLS + 1];
int sorted_spells[MAX_SKILLS + 1];
int sorted_skills[MAX_SKILLS + 1];
//int sorted_spells[MAX_SPELLS + 1];
//int sorted_skills[MAX_SKILLS - MAX_SPELLS + 1];

static int compare_spells(const void *x, const void *y) {
  int a = *(const int *) x,
          b = *(const int *) y;
  
  if (a <= 1 || b <= 1)
    return 0;

  if (a > MAX_SKILLS || b > MAX_SKILLS)
    return 0;

  return strcmp(spell_info[a].name, spell_info[b].name);
}

/* this will create a full list, added two more lists
   to seperate the skills/spells */
void sort_spells(void) {
  int a;

  /* full list */
  
  /* initialize array, avoiding reserved. */
  for (a = 1; a <= MAX_SKILLS; a++) {
    spell_sort_info[a] = a;
    sorted_spells[a] = -1;
    sorted_skills[a] = -1;
  }

  qsort(&spell_sort_info[1], MAX_SKILLS, sizeof (int), 
          compare_spells);

  /* spell list */
  
  /* initialize array, avoiding reserved. */
  for (a = 1; a <= MAX_SPELLS; a++)
    sorted_spells[a] = a;

  qsort(&sorted_spells[1], MAX_SKILLS, sizeof (int), 
          compare_spells);
  
  /* spell list */
  
  /* initialize array, avoiding reserved. */
  for (a = 0; a <= (MAX_SKILLS - MAX_SPELLS); a++)
    sorted_skills[a] = a + MAX_SPELLS;

  qsort(&sorted_skills[1], MAX_SKILLS,
          sizeof (int), compare_spells);
}


const char *prac_types[] = {
  "spell",
  "skill"
};
#define LEARNED_LEVEL	0	/* % known which is considered "learned" */
#define MAX_PER_PRAC	1	/* max percent gain in skill per practice */
#define MIN_PER_PRAC	2	/* min percent gain in skill per practice */
#define PRAC_TYPE	3	/* should it say 'spell' or 'skill'?	 */
#define LEARNED(ch) (prac_params[LEARNED_LEVEL][GET_CLASS(ch)])
#define MINGAIN(ch) (prac_params[MIN_PER_PRAC][GET_CLASS(ch)])
#define MAXGAIN(ch) (prac_params[MAX_PER_PRAC][GET_CLASS(ch)])
#define SPLSKL(ch) (prac_types[prac_params[PRAC_TYPE][GET_CLASS(ch)]])


//returns true if you have all the requisites for the skill
//false if you don't

int meet_skill_reqs(struct char_data *ch, int skillnum) {
  //doesn't apply to staff
  if (GET_LEVEL(ch) >= LVL_IMMORT)
    return TRUE;
  //spells should return true
  if (skillnum < NUM_SPELLS && skillnum > 0)
    return TRUE;

  /* i'm -trying- to keep this organized */
  switch (skillnum) {

      /* proficiencies */
    case SKILL_PROF_BASIC:
      if (GET_SKILL(ch, SKILL_PROF_MINIMAL))
        return TRUE;
      else return FALSE;
    case SKILL_PROF_ADVANCED:
      if (GET_SKILL(ch, SKILL_PROF_BASIC))
        return TRUE;
      else return FALSE;
    case SKILL_PROF_MASTER:
      if (GET_SKILL(ch, SKILL_PROF_ADVANCED))
        return TRUE;
      else return FALSE;
    case SKILL_PROF_EXOTIC:
      if (GET_SKILL(ch, SKILL_PROF_MASTER))
        return TRUE;
      else return FALSE;
    case SKILL_PROF_MEDIUM_A:
      if (GET_SKILL(ch, SKILL_PROF_LIGHT_A))
        return TRUE;
      else return FALSE;
    case SKILL_PROF_HEAVY_A:
      if (GET_SKILL(ch, SKILL_PROF_MEDIUM_A))
        return TRUE;
      else return FALSE;
    case SKILL_PROF_T_SHIELDS:
      if (GET_SKILL(ch, SKILL_PROF_SHIELDS))
        return TRUE;
      else return FALSE;

      /* epic spells */
    case SKILL_MUMMY_DUST:
      if (GET_ABILITY(ch, ABILITY_SPELLCRAFT) >= 23 && GET_LEVEL(ch) >= 20)
        return TRUE;
      else return FALSE;
    case SKILL_DRAGON_KNIGHT:
      if (GET_ABILITY(ch, ABILITY_SPELLCRAFT) >= 25 && GET_LEVEL(ch) >= 20 &&
              (CLASS_LEVEL(ch, CLASS_WIZARD) > 17 ||
              CLASS_LEVEL(ch, CLASS_SORCERER) > 19)
              )
        return TRUE;
      else return FALSE;
    case SKILL_GREATER_RUIN:
      if (GET_ABILITY(ch, ABILITY_SPELLCRAFT) >= 27 && GET_LEVEL(ch) >= 20)
        return TRUE;
      else return FALSE;
    case SKILL_HELLBALL:
      if (GET_ABILITY(ch, ABILITY_SPELLCRAFT) >= 29 && GET_LEVEL(ch) >= 20 &&
              (CLASS_LEVEL(ch, CLASS_WIZARD) > 16 ||
              CLASS_LEVEL(ch, CLASS_SORCERER) > 18)
              )
        return TRUE;
      else return FALSE;
      /* magical based epic spells (not accessable by divine) */
    case SKILL_EPIC_MAGE_ARMOR:
      if (GET_ABILITY(ch, ABILITY_SPELLCRAFT) >= 31 && GET_LEVEL(ch) >= 20
              && (CLASS_LEVEL(ch, CLASS_WIZARD) > 13 ||
              CLASS_LEVEL(ch, CLASS_SORCERER) > 13))
        return TRUE;
      else return FALSE;
    case SKILL_EPIC_WARDING:
      if (GET_ABILITY(ch, ABILITY_SPELLCRAFT) >= 33 && GET_LEVEL(ch) >= 20
              && (CLASS_LEVEL(ch, CLASS_WIZARD) > 15 ||
              CLASS_LEVEL(ch, CLASS_SORCERER) > 15))
        return TRUE;
      else return FALSE;

      /* 'epic' skills */
    case SKILL_BLINDING_SPEED:
      if (GET_REAL_DEX(ch) >= 21 && GET_LEVEL(ch) >= 20)
        return TRUE;
      else return FALSE;
    case SKILL_EPIC_TOUGHNESS:
      if (GET_LEVEL(ch) >= 20)
        return TRUE;
      else return FALSE;
    case SKILL_EPIC_PROWESS:
      if (GET_LEVEL(ch) >= 20 && GET_SKILL(ch, SKILL_PROWESS))
        return TRUE;
      else return FALSE;
    case SKILL_SPELLPENETRATE_3:
      if (GET_LEVEL(ch) >= 20 && GET_SKILL(ch, SKILL_SPELLPENETRATE_2))
        return TRUE;
      else return FALSE;
    case SKILL_SPELL_RESIST_4:
      if (GET_LEVEL(ch) >= 20 && GET_SKILL(ch, SKILL_SPELL_RESIST_3))
        return TRUE;
      else return FALSE;
    case SKILL_SPELL_RESIST_5:
      if (GET_LEVEL(ch) >= 25 && GET_SKILL(ch, SKILL_SPELL_RESIST_4))
        return TRUE;
      else return FALSE;
    case SKILL_IMPROVED_BASH:
      if (GET_SKILL(ch, SKILL_BASH) && GET_LEVEL(ch) >= 20)
        return TRUE;
      else return FALSE;
    case SKILL_IMPROVED_WHIRL:
      if (GET_SKILL(ch, SKILL_WHIRLWIND) && GET_LEVEL(ch) >= 20)
        return TRUE;
      else return FALSE;
    case SKILL_ARMOR_SKIN:
      if (GET_LEVEL(ch) >= 20)
        return TRUE;
      else return FALSE;
    case SKILL_SELF_CONCEAL_3:
      if (GET_REAL_DEX(ch) >= 21 && GET_SKILL(ch, SKILL_SELF_CONCEAL_2))
        return TRUE;
      else return FALSE;
    case SKILL_OVERWHELMING_CRIT:
      if (GET_LEVEL(ch) >= 20)
        return TRUE;
      else return FALSE;
    case SKILL_DAMAGE_REDUC_3:
      if (GET_REAL_CON(ch) >= 19 && GET_SKILL(ch, SKILL_DAMAGE_REDUC_2))
        return TRUE;
      else return FALSE;
    case SKILL_EPIC_REFLEXES:
    case SKILL_EPIC_FORTITUDE:
    case SKILL_EPIC_WILL:
      if (GET_LEVEL(ch) >= 20)
        return TRUE;
      else return FALSE;
    case SKILL_EPIC_2_WEAPON:
      if (GET_REAL_DEX(ch) >= 21 && GET_SKILL(ch, SKILL_TWO_WEAPON_FIGHT))
        return TRUE;
      else return FALSE;
    case SKILL_IMPROVED_TRIP:
      if (GET_SKILL(ch, SKILL_TRIP) && GET_LEVEL(ch) >= 20)
        return TRUE;
      else return FALSE;
    case SKILL_HEADBUTT:
      if (GET_LEVEL(ch) >= 20 && 
              (GET_REAL_CON(ch) + GET_REAL_STR(ch) >= 32))
        return TRUE;
      else return FALSE;

      /* melee combat related */
    case SKILL_AMBIDEXTERITY:
      if (GET_REAL_DEX(ch) >= 13)
        return TRUE;
      else return FALSE;
    case SKILL_TWO_WEAPON_FIGHT:
      if (GET_REAL_DEX(ch) >= 17 && GET_SKILL(ch, SKILL_AMBIDEXTERITY))
        return TRUE;
      else return FALSE;
    case SKILL_FINESSE:
      if (GET_REAL_DEX(ch) >= 13)
        return TRUE;
      else return FALSE;
    case SKILL_POWER_ATTACK:
      if (GET_REAL_STR(ch) >= 13)
        return TRUE;
      else return FALSE;
    case SKILL_EXPERTISE:
      if (GET_REAL_INT(ch) >= 13)
        return TRUE;
      else return FALSE;
    case SKILL_INITIATIVE:
      if (GET_REAL_DEX(ch) >= 13)
        return TRUE;
      else return FALSE;
    case SKILL_BASH:
      if (GET_REAL_STR(ch) >= 13)
        return TRUE;
      else return FALSE;
    case SKILL_TRIP:
      if (GET_REAL_DEX(ch) >= 13)
        return TRUE;
      else return FALSE;
    case SKILL_WHIRLWIND:
      if (GET_SKILL(ch, SKILL_SPRING_ATTACK))
        return TRUE;
      else return FALSE;
    case SKILL_DODGE:
      if (GET_REAL_DEX(ch) >= 13)
        return TRUE;
      else return FALSE;
    case SKILL_DAMAGE_REDUC_1:
      if (GET_REAL_CON(ch) >= 15)
        return TRUE;
      else return FALSE;
    case SKILL_DAMAGE_REDUC_2:
      if (GET_REAL_CON(ch) >= 17 && GET_SKILL(ch, SKILL_DAMAGE_REDUC_1))
        return TRUE;
      else return FALSE;
    case SKILL_SELF_CONCEAL_1:
      if (GET_REAL_DEX(ch) >= 15)
        return TRUE;
      else return FALSE;
    case SKILL_SELF_CONCEAL_2:
      if (GET_REAL_DEX(ch) >= 17 && GET_SKILL(ch, SKILL_SELF_CONCEAL_1))
        return TRUE;
      else return FALSE;
    case SKILL_EPIC_CRIT:
      if (GET_LEVEL(ch) >= 10 && GET_SKILL(ch, SKILL_IMPROVED_CRITICAL))
        return TRUE;
      else return FALSE;
      
      /* more caster related */
    case SKILL_SPELLPENETRATE:
      if (GET_LEVEL(ch) >= 5 && IS_CASTER(ch))
        return TRUE;
      else return FALSE;
    case SKILL_SPELLPENETRATE_2:
      if (GET_LEVEL(ch) >= 9 && GET_SKILL(ch, SKILL_SPELLPENETRATE))
        return TRUE;
      else return FALSE;
    case SKILL_SPELL_RESIST_1:
      if (GET_LEVEL(ch) >= 5)
        return TRUE;
      else return FALSE;
    case SKILL_SPELL_RESIST_2:
      if (GET_LEVEL(ch) >= 10 && GET_SKILL(ch, SKILL_SPELL_RESIST_1))
        return TRUE;
      else return FALSE;
    case SKILL_SPELL_RESIST_3:
      if (GET_LEVEL(ch) >= 15 && GET_SKILL(ch, SKILL_SPELL_RESIST_2))
        return TRUE;
      else return FALSE;
    case SKILL_QUICK_CHANT:
      if (CASTER_LEVEL(ch))
        return TRUE;
      else return FALSE;
    case SKILL_SCRIBE:
      if (CASTER_LEVEL(ch))
        return TRUE;
      else return FALSE;

      /* special restrictions, i.e. not restricted to one class, etc */
    case SKILL_USE_MAGIC: /* shared - with casters and rogue */
      if ((CLASS_LEVEL(ch, CLASS_ROGUE) >= 9) ||
              (IS_CASTER(ch) && GET_LEVEL(ch) >= 2))
        return TRUE;
      else return FALSE;
    case SKILL_CALL_FAMILIAR: //sorc, wiz only
      if (CLASS_LEVEL(ch, CLASS_SORCERER) || CLASS_LEVEL(ch, CLASS_WIZARD))
        return TRUE;
      else return FALSE;
    case SKILL_RECHARGE: //casters only
      if (CASTER_LEVEL(ch) >= 14)
        return TRUE;
      else return FALSE;
    case SKILL_TRACK: // rogue / ranger / x-stats only
      if (CLASS_LEVEL(ch, CLASS_ROGUE) || CLASS_LEVEL(ch, CLASS_RANGER) ||
              (GET_WIS(ch) + GET_INT(ch) >= 28))
        return TRUE;
      else return FALSE;
    case SKILL_MOBILITY: /* shared, free for rogues */
      if (GET_SKILL(ch, SKILL_DODGE) || (CLASS_LEVEL(ch, CLASS_ROGUE) >= 2))
        return TRUE;
      else return FALSE;
    case SKILL_SPRING_ATTACK: /* shared, free for rogues */
      if (GET_SKILL(ch, SKILL_MOBILITY) ||
              (CLASS_LEVEL(ch, CLASS_ROGUE) >= 6))
        return TRUE;
      else return FALSE;
    case SKILL_CHARGE:
      if (GET_ABILITY(ch, ABILITY_MOUNT) >= 10)
        return TRUE;
      else return FALSE;
    case SKILL_HITALL:
      if ((GET_REAL_STR(ch) + GET_REAL_CON(ch)) >= 29)
        return TRUE;
      else return FALSE;
    case SKILL_SHIELD_PUNCH:
      if (GET_SKILL(ch, SKILL_SHIELD_SPECIALIST))
        return TRUE;
      else return FALSE;
    case SKILL_BODYSLAM:
      if (GET_RACE(ch) == RACE_TROLL)
        return TRUE;
      else return FALSE;


      /* ranger */
    case SKILL_FAVORED_ENEMY:
      if (CLASS_LEVEL(ch, CLASS_RANGER))
        return TRUE;
      else return FALSE;
    case SKILL_DUAL_WEAPONS:
      if (CLASS_LEVEL(ch, CLASS_RANGER) >= 2)
        return TRUE;
      else return FALSE;
    case SKILL_NATURE_STEP: //shared with druid
      if (CLASS_LEVEL(ch, CLASS_RANGER) >= 3 ||
              CLASS_LEVEL(ch, CLASS_DRUID) >= 6)
        return TRUE;
      else return FALSE;
    case SKILL_ANIMAL_COMPANION: //shared with druid
      if (CLASS_LEVEL(ch, CLASS_RANGER) >= 4 ||
              CLASS_LEVEL(ch, CLASS_DRUID))
        return TRUE;
      else return FALSE;

      /* druid */
      // animal companion - level 1 (shared with ranger)
      // nature step - level 6 (shared with ranger)

      /* warrior */
    case SKILL_WEAPON_SPECIALIST:  // not a free skill
      if (CLASS_LEVEL(ch, CLASS_WARRIOR) >= 4)
        return TRUE;
      else return FALSE;
    case SKILL_SHIELD_SPECIALIST:  // not a free skill
      if (CLASS_LEVEL(ch, CLASS_WARRIOR) >= 6)
        return TRUE;
      else return FALSE;

      /* monk */
    case SKILL_STUNNING_FIST:
      if (CLASS_LEVEL(ch, CLASS_MONK) >= 2)
        return TRUE;
      else return FALSE;
    case SKILL_SPRINGLEAP:
      if (CLASS_LEVEL(ch, CLASS_MONK) >= 6)
        return TRUE;
      else return FALSE;

      /* bard */
    case SKILL_PERFORM:
      if (CLASS_LEVEL(ch, CLASS_BARD) >= 2)
        return TRUE;
      else return FALSE;

      /* paladin */
    case SKILL_LAY_ON_HANDS:
      if (CLASS_LEVEL(ch, CLASS_PALADIN))
        return TRUE;
      else return FALSE;
    case SKILL_GRACE:
      if (CLASS_LEVEL(ch, CLASS_PALADIN) >= 2)
        return TRUE;
      else return FALSE;
    case SKILL_DIVINE_HEALTH:
      if (CLASS_LEVEL(ch, CLASS_PALADIN) >= 3)
        return TRUE;
      else return FALSE;
    case SKILL_COURAGE:
      if (CLASS_LEVEL(ch, CLASS_PALADIN) >= 4)
        return TRUE;
      else return FALSE;
    case SKILL_SMITE:
      if (CLASS_LEVEL(ch, CLASS_PALADIN) >= 5)
        return TRUE;
      else return FALSE;
    case SKILL_REMOVE_DISEASE:
      if (CLASS_LEVEL(ch, CLASS_PALADIN) >= 7)
        return TRUE;
      else return FALSE;
    case SKILL_PALADIN_MOUNT:
      if (CLASS_LEVEL(ch, CLASS_PALADIN) >= 8)
        return TRUE;
      else return FALSE;

      /* rogue */
    case SKILL_BACKSTAB:
      if (CLASS_LEVEL(ch, CLASS_ROGUE))
        return TRUE;
      else return FALSE;
    case SKILL_DIRTY_FIGHTING:
      if (CLASS_LEVEL(ch, CLASS_ROGUE) >= 4)
        return TRUE;
      else return FALSE;
    case SKILL_EVASION:
      if (CLASS_LEVEL(ch, CLASS_ROGUE) >= 8)
        return TRUE;
      else return FALSE;
     case SKILL_SAP:  // not a free skill
      if (CLASS_LEVEL(ch, CLASS_ROGUE) >= 10)
        return TRUE;
      else return FALSE;
   case SKILL_CRIP_STRIKE:
      if (CLASS_LEVEL(ch, CLASS_ROGUE) >= 12)
        return TRUE;
      else return FALSE;
    case SKILL_SLIPPERY_MIND:
      if (CLASS_LEVEL(ch, CLASS_ROGUE) >= 15)
        return TRUE;
      else return FALSE;
    case SKILL_DEFENSE_ROLL:
      if (CLASS_LEVEL(ch, CLASS_ROGUE) >= 18)
        return TRUE;
      else return FALSE;
    case SKILL_IMP_EVASION:
      if (CLASS_LEVEL(ch, CLASS_ROGUE) >= 21)
        return TRUE;
      else return FALSE;
    case SKILL_DIRT_KICK:
      if (GET_LEVEL(ch) >= 20 && GET_REAL_DEX(ch) >= 17) {
        if (CLASS_LEVEL(ch, CLASS_ROGUE) >= 15)
          return TRUE;
      } else return FALSE;

      /* berserker */
    case SKILL_RAGE:
      if (CLASS_LEVEL(ch, CLASS_BERSERKER) >= 2)
        return TRUE;
      else return FALSE;

      /*** no reqs ***/
    case SKILL_RESCUE:
    case SKILL_LUCK_OF_HEROES:
    case SKILL_TOUGHNESS:
    case SKILL_KICK:
    case SKILL_IMPROVED_CRITICAL:
    case SKILL_PROWESS:
    case SKILL_PROF_MINIMAL:
    case SKILL_PROF_SHIELDS:
    case SKILL_PROF_LIGHT_A:
    case SKILL_IRON_WILL:
    case SKILL_GREAT_FORTITUDE:
    case SKILL_LIGHTNING_REFLEXES:
    case SKILL_STEALTHY:
    case SKILL_MINING:
    case SKILL_HUNTING:
    case SKILL_FORESTING:
    case SKILL_KNITTING:
    case SKILL_CHEMISTRY:
    case SKILL_ARMOR_SMITHING:
    case SKILL_WEAPON_SMITHING:
    case SKILL_JEWELRY_MAKING:
    case SKILL_LEATHER_WORKING:
    case SKILL_FAST_CRAFTER:
      return TRUE;

      /**
       *  not implemented yet or
       * unattainable
       *  **/
    case SKILL_MURMUR:
    case SKILL_PROPAGANDA:
    case SKILL_LOBBY:
    case SKILL_BONE_ARMOR:
    case SKILL_ELVEN_CRAFTING:
    case SKILL_MASTERWORK_CRAFTING:
    case SKILL_DRACONIC_CRAFTING:
    case SKILL_DWARVEN_CRAFTING:
    case SKILL_SPELLBATTLE:  //arcana golem innate
    default: return FALSE;
  }
  return FALSE;
}

/* completely re-written for Luminari, probably needs to be rewritten again :P
   this is the engine for the 'spells' and 'spelllist' commands
   class - you can send -1 for a 'default' class
   mode = 0:  known spells
   mode = anything else: full spelllist for given class 
 */
void list_spells(struct char_data *ch, int mode, int class) {
  int i = 0, slot = 0, sinfo = 0;
  size_t len = 0, nlen = 0;
  char buf2[MAX_STRING_LENGTH] = {'\0'};
  const char *overflow = "\r\n**OVERFLOW**\r\n";

  //default class case
  if (class == -1) {
    class = GET_CLASS(ch);
    if (!CLASS_LEVEL(ch, class))
      send_to_char(ch, "You don't have any levels in your current class.\r\n");
  }

  if (mode == 0) {
    len = snprintf(buf2, sizeof (buf2), "\tCKnown Spell List\tn\r\n");

    for (slot = getCircle(ch, class); slot > 0; slot--) {
      nlen = snprintf(buf2 + len, sizeof (buf2) - len,
              "\r\n\tCSpell Circle Level %d\tn\r\n", slot);
      if (len + nlen >= sizeof (buf2) || nlen < 0)
        break;
      len += nlen;

      for (i = 1; i < NUM_SPELLS; i++) {
        sinfo = spell_info[i].min_level[class];

        if (class == CLASS_SORCERER && sorcKnown(ch, i, CLASS_SORCERER) &&
                spellCircle(CLASS_SORCERER, i) == slot) {
          nlen = snprintf(buf2 + len, sizeof (buf2) - len,
                  "%-20s \tWMastered\tn\r\n", spell_info[i].name);
          if (len + nlen >= sizeof (buf2) || nlen < 0)
            break;
          len += nlen;
        } else if (class == CLASS_BARD && sorcKnown(ch, i, CLASS_BARD) &&
                spellCircle(CLASS_BARD, i) == slot) {
          nlen = snprintf(buf2 + len, sizeof (buf2) - len,
                  "%-20s \tWMastered\tn\r\n", spell_info[i].name);
          if (len + nlen >= sizeof (buf2) || nlen < 0)
            break;
          len += nlen;
        } else if (class == CLASS_WIZARD && spellbook_ok(ch, i, class, FALSE) &&
                CLASS_LEVEL(ch, class) >= sinfo && spellCircle(class, i) == slot &&
                GET_SKILL(ch, i)) {
          nlen = snprintf(buf2 + len, sizeof (buf2) - len,
                  "%-20s \tRReady\tn\r\n", spell_info[i].name);
          if (len + nlen >= sizeof (buf2) || nlen < 0)
            break;
          len += nlen;
        } else if (class != CLASS_SORCERER && class != CLASS_BARD && class != CLASS_WIZARD &&
                CLASS_LEVEL(ch, class) >= sinfo && spellCircle(class, i) == slot &&
                GET_SKILL(ch, i)) {
          nlen = snprintf(buf2 + len, sizeof (buf2) - len,
                  "%-20s \tWMastered\tn\r\n", spell_info[i].name);
          if (len + nlen >= sizeof (buf2) || nlen < 0)
            break;
          len += nlen;
        }
      }
    }

  } else {
    len = snprintf(buf2, sizeof (buf2), "\tCFull Spell List\tn\r\n");

    if (class == CLASS_PALADIN || class == CLASS_RANGER)
      slot = 4;
    else
      slot = 9;

    for (; slot > 0; slot--) {
      nlen = snprintf(buf2 + len, sizeof (buf2) - len,
              "\r\n\tCSpell Circle Level %d\tn\r\n", slot);
      if (len + nlen >= sizeof (buf2) || nlen < 0)
        break;
      len += nlen;

      for (i = 1; i < NUM_SPELLS; i++) {
        sinfo = spell_info[i].min_level[class];

        if (spellCircle(class, i) == slot) {
          nlen = snprintf(buf2 + len, sizeof (buf2) - len,
                  "%-20s\r\n", spell_info[i].name);
          if (len + nlen >= sizeof (buf2) || nlen < 0)
            break;
          len += nlen;
        }
      }
    }
  }
  if (len >= sizeof (buf2))
    strcpy(buf2 + sizeof (buf2) - strlen(overflow) - 1, overflow); /* strcpy: OK */

  page_string(ch->desc, buf2, TRUE);
}

void list_skills(struct char_data *ch) {
  int i, printed = 0;

  if (IS_NPC(ch))
    return;

  /* Active Skills */
  send_to_char(ch, "\tCActive Skills\tn\r\n\r\n");
  for (i = MAX_SPELLS + 1; i < NUM_SKILLS; i++) {
    if (GET_LEVEL(ch) >= spell_info[i].min_level[GET_CLASS(ch)] &&
            spell_info[i].schoolOfMagic == ACTIVE_SKILL) {
      if (meet_skill_reqs(ch, i)) {
        send_to_char(ch, "%-24s", spell_info[i].name);
        if (!GET_SKILL(ch, i))
          send_to_char(ch, "  \tYUnlearned\tn ");
        else if (GET_SKILL(ch, i) >= 99)
          send_to_char(ch, "  \tWMastered \tn ");
        else if (GET_SKILL(ch, i) >= 95)
          send_to_char(ch, "  \twSuperb \tn ");
        else if (GET_SKILL(ch, i) >= 90)
          send_to_char(ch, "  \tMExcellent \tn ");
        else if (GET_SKILL(ch, i) >= 85)
          send_to_char(ch, "  \tmAdvanced \tn ");
        else if (GET_SKILL(ch, i) >= 80)
          send_to_char(ch, "  \tBSkilled \tn ");
        else
          send_to_char(ch, "  \tGLearned  \tn ");
        printed++;
        if (!(printed % 2))
          send_to_char(ch, "\r\n");
      }
    }
  }
  send_to_char(ch, "\r\n\r\n");

  /* Passive Skills */
  send_to_char(ch, "\tCPassive Skills\tn\r\n\r\n");
  for (i = MAX_SPELLS + 1; i < NUM_SKILLS; i++) {
    if (GET_LEVEL(ch) >= spell_info[i].min_level[GET_CLASS(ch)] &&
            spell_info[i].schoolOfMagic == PASSIVE_SKILL) {
      if (meet_skill_reqs(ch, i)) {
        send_to_char(ch, "%-24s", spell_info[i].name);
        if (!GET_SKILL(ch, i))
          send_to_char(ch, "  \tYUnlearned\tn ");
        else if (GET_SKILL(ch, i) >= 99)
          send_to_char(ch, "  \tWMastered \tn ");
        else if (GET_SKILL(ch, i) >= 95)
          send_to_char(ch, "  \twSuperb \tn ");
        else if (GET_SKILL(ch, i) >= 90)
          send_to_char(ch, "  \tMExcellent \tn ");
        else if (GET_SKILL(ch, i) >= 85)
          send_to_char(ch, "  \tmAdvanced \tn ");
        else if (GET_SKILL(ch, i) >= 80)
          send_to_char(ch, "  \tBSkilled \tn ");
        else
          send_to_char(ch, "  \tGLearned  \tn ");
        printed++;
        if (!(printed % 2))
          send_to_char(ch, "\r\n");
      }
    }
  }
  send_to_char(ch, "\r\n\r\n");

  /* Caster Skills */
  send_to_char(ch, "\tCCaster Skills\tn\r\n\r\n");
  for (i = MAX_SPELLS + 1; i < NUM_SKILLS; i++) {
    if (GET_LEVEL(ch) >= spell_info[i].min_level[GET_CLASS(ch)] &&
            spell_info[i].schoolOfMagic == CASTER_SKILL) {
      if (meet_skill_reqs(ch, i)) {
        send_to_char(ch, "%-24s", spell_info[i].name);
        if (!GET_SKILL(ch, i))
          send_to_char(ch, "  \tYUnlearned\tn ");
        else if (GET_SKILL(ch, i) >= 99)
          send_to_char(ch, "  \tWMastered \tn ");
        else if (GET_SKILL(ch, i) >= 95)
          send_to_char(ch, "  \twSuperb \tn ");
        else if (GET_SKILL(ch, i) >= 90)
          send_to_char(ch, "  \tMExcellent \tn ");
        else if (GET_SKILL(ch, i) >= 85)
          send_to_char(ch, "  \tmAdvanced \tn ");
        else if (GET_SKILL(ch, i) >= 80)
          send_to_char(ch, "  \tBSkilled \tn ");
        else
          send_to_char(ch, "  \tGLearned  \tn ");
        printed++;
        if (!(printed % 2))
          send_to_char(ch, "\r\n");
      }
    }
  }
  send_to_char(ch, "\r\n\r\n");

  /* Crafting Skills */
  send_to_char(ch, "\tCCrafting Skills\tn\r\n\r\n");
  for (i = MAX_SPELLS + 1; i < NUM_SKILLS; i++) {
    if (GET_LEVEL(ch) >= spell_info[i].min_level[GET_CLASS(ch)] &&
            spell_info[i].schoolOfMagic == CRAFTING_SKILL) {
      if (meet_skill_reqs(ch, i)) {
        send_to_char(ch, "%-24s %d          ", spell_info[i].name, GET_SKILL(ch, i));
        printed++;
        if (!(printed % 2))
          send_to_char(ch, "\r\n");
      }
    }
  }
  send_to_char(ch, "\r\n\r\n");

  send_to_char(ch, "\tCPractice Session(s): %d\tn\r\n\r\n",
          GET_PRACTICES(ch));

}

int compute_ability(struct char_data *ch, int abilityNum) {
  int value = 0;

  if (!ch)
    return -1;
  
  if (abilityNum < 1 || abilityNum > NUM_ABILITIES)
    return -1;
  
  /* this dummy check was added to to possible problems with checking
   affected_by_spell on a target that just died */
  if (GET_HIT(ch) <= 0 || GET_POS(ch) <= POS_STUNNED)
    return -1;

  //universal bonuses
  if (affected_by_spell(ch, SPELL_HEROISM))
    value += 2;
  else if (affected_by_spell(ch, SPELL_GREATER_HEROISM))
    value += 4;
  if (affected_by_spell(ch, SKILL_PERFORM))
    value += SONG_AFF_VAL(ch);

  // try to avoid sending NPC's here, but just in case:
  if (IS_NPC(ch))
    value += GET_LEVEL(ch);
  else
    value += GET_ABILITY(ch, abilityNum);

  switch (abilityNum) {
    case ABILITY_TUMBLE:
      value += GET_DEX_BONUS(ch);
      value += compute_gear_penalty_check(ch);
      return value;
    case ABILITY_HIDE:
      value += GET_DEX_BONUS(ch);
      if (GET_SKILL(ch, SKILL_STEALTHY))
        value += 2;
      if (GET_RACE(ch) == RACE_HALFLING)
        value += 2;
      if (AFF_FLAGGED(ch, AFF_REFUGE))
        value += 15;
      if (IS_MORPHED(ch) && SUBRACE(ch) == PC_SUBRACE_PANTHER)
        value += 4;
      value += compute_gear_penalty_check(ch);
      return value;
    case ABILITY_SNEAK:
      value += GET_DEX_BONUS(ch);
      if (GET_SKILL(ch, SKILL_STEALTHY))
        value += 2;
      if (GET_RACE(ch) == RACE_HALFLING)
        value += 2;
      if (AFF_FLAGGED(ch, AFF_REFUGE))
        value += 15;
      if (IS_MORPHED(ch) && SUBRACE(ch) == PC_SUBRACE_PANTHER)
        value += 4;
      value += compute_gear_penalty_check(ch);
      return value;
    case ABILITY_SPOT:
      value += GET_WIS_BONUS(ch);
      if (GET_RACE(ch) == RACE_ELF)
        value += 2;
      return value;
    case ABILITY_LISTEN:
      value += GET_WIS_BONUS(ch);
      if (GET_RACE(ch) == RACE_GNOME)
        value += 2;
      if (GET_RACE(ch) == RACE_ELF)
        value += 2;
      return value;
    case ABILITY_TREAT_INJURY:
      value += GET_WIS_BONUS(ch);
      return value;
    case ABILITY_TAUNT:
      value += GET_CHA_BONUS(ch);
      return value;
    case ABILITY_CONCENTRATION:
      if (GET_RACE(ch) == RACE_GNOME)
        value += 2;
      value += GET_CON_BONUS(ch);
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_ARCANA_GOLEM) {
        value += GET_LEVEL(ch) / 6;
      }
      return value;
    case ABILITY_SPELLCRAFT:
      value += GET_INT_BONUS(ch);
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_ARCANA_GOLEM) {
        value += GET_LEVEL(ch) / 6;
      }
      return value;
    case ABILITY_APPRAISE:
      value += GET_INT_BONUS(ch);
      return value;
    case ABILITY_DISCIPLINE:
      if (GET_RACE(ch) == RACE_H_ELF)
        value += 2;
      value += GET_STR_BONUS(ch);
      return value;
    case ABILITY_PARRY:
      value += GET_DEX_BONUS(ch);
      value += compute_gear_penalty_check(ch);
      return value;
    case ABILITY_LORE:
      if (GET_RACE(ch) == RACE_H_ELF)
        value += 2;
      value += GET_INT_BONUS(ch);
      return value;
    case ABILITY_MOUNT:
      value += GET_DEX_BONUS(ch);
      return value;
    case ABILITY_PICK_LOCK:
      value += GET_DEX_BONUS(ch);
      value += compute_gear_penalty_check(ch);
      return value;
    case ABILITY_STEAL:
      value += GET_DEX_BONUS(ch);
      value += compute_gear_penalty_check(ch);
      return value;
    case ABILITY_SEARCH:
      value += GET_INT_BONUS(ch);
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_ELF) 
        value += 2;
      else if (!IS_NPC(ch) && GET_RACE(ch) == RACE_H_ELF)
        value += 1;
      return value;
    default: return -1;
  }
}


/** cross-class or not? **/
const char *cross_names[] = {
  "\tRNot Available to Your Class\tn",
  "\tcCross-Class Ability\tn",
  "\tWClass Ability\tn"
};

void list_abilities(struct char_data *ch) {
  int i;

  if (IS_NPC(ch))
    return;

  send_to_char(ch, "\tCYou have %d training session%s remaining.\r\n"
          "You know of the following abilities:\tn\r\n", GET_TRAINS(ch),
          GET_TRAINS(ch) == 1 ? "" : "s");

  for (i = 1; i < NUM_ABILITIES; i++) {
    send_to_char(ch, "%-20s [%d] \tC[%d]\tn %s\r\n",
            ability_names[i], GET_ABILITY(ch, i), compute_ability(ch, i),
            cross_names[class_ability[i][GET_CLASS(ch)]]);
  }
}


//further expansion -zusuk

void process_skill(struct char_data *ch, int skillnum) {
  switch (skillnum) {
    case SKILL_EPIC_TOUGHNESS:
      GET_REAL_MAX_HIT(ch) += GET_LEVEL(ch);
      send_to_char(ch, "\tMYou gained %d hp!\tn\r\n", GET_LEVEL(ch));
      return;
    case SKILL_TOUGHNESS:
      GET_REAL_MAX_HIT(ch) += GET_LEVEL(ch);
      send_to_char(ch, "\tMYou gained %d hp!\tn\r\n", GET_LEVEL(ch));
      return;

      // epic spells

    case SKILL_MUMMY_DUST:
      send_to_char(ch, "\tMYou gained Epic Spell:  Mummy Dust!\tn\r\n");
      SET_SKILL(ch, SPELL_MUMMY_DUST, 99);
      return;
    case SKILL_DRAGON_KNIGHT:
      send_to_char(ch, "\tMYou gained Epic Spell:  Dragon Knight!\tn\r\n");
      SET_SKILL(ch, SPELL_DRAGON_KNIGHT, 99);
      return;
    case SKILL_GREATER_RUIN:
      send_to_char(ch, "\tMYou gained Epic Spell:  Greater Ruin!\tn\r\n");
      SET_SKILL(ch, SPELL_GREATER_RUIN, 99);
      return;
    case SKILL_HELLBALL:
      send_to_char(ch, "\tMYou gained Epic Spell:  Hellball!\tn\r\n");
      SET_SKILL(ch, SPELL_HELLBALL, 99);
      return;
    case SKILL_EPIC_MAGE_ARMOR:
      send_to_char(ch, "\tMYou gained Epic Spell:  Epic Mage Armor!\tn\r\n");
      SET_SKILL(ch, SPELL_EPIC_MAGE_ARMOR, 99);
      return;
    case SKILL_EPIC_WARDING:
      send_to_char(ch, "\tMYou gained Epic Spell:  Epic Warding!\tn\r\n");
      SET_SKILL(ch, SPELL_EPIC_WARDING, 99);
      return;

    default: return;
  }
  return;
}

/********************************************************************/
/******************** Mobile Procs    *******************************/
/********************************************************************/

/**** General special procedures for mobiles. ****/

static void npc_steal(struct char_data *ch, struct char_data *victim) {
  int gold;

  if (IS_NPC(victim))
    return;
  if (GET_LEVEL(victim) >= LVL_IMMORT)
    return;
  if (!CAN_SEE(ch, victim))
    return;

  if (AWAKE(victim) && (rand_number(0, GET_LEVEL(ch)) == 0)) {
    act("You discover that $n has $s hands in your wallet.", FALSE, ch, 0, victim, TO_VICT);
    act("$n tries to steal gold from $N.", TRUE, ch, 0, victim, TO_NOTVICT);
  } else {
    /* Steal some gold coins */
    gold = (GET_GOLD(victim) * rand_number(1, 10)) / 100;
    if (gold > 0) {
      increase_gold(ch, gold);
      decrease_gold(victim, gold);
    }
  }
}

/* this function will cause basically all the mobiles in the same zone
   to hunt someone down */
void zone_yell(struct char_data *ch, char buf[256]) {
  struct char_data *i;
  struct char_data *vict;

  for (i = character_list; i; i = i->next) {
    if (world[ch->in_room].zone == world[i->in_room].zone) {

      if (PROC_FIRED(ch) == FALSE) {
        send_to_char(i, buf);
      }

      if (i == ch || !IS_NPC(i))
        continue;

      if (((IS_EVIL(ch) && IS_EVIL(i)) || (IS_GOOD(ch) && IS_GOOD(i))) && 
              MOB_FLAGGED(i, MOB_HELPER)) {
        if (i->in_room == ch->in_room && !FIGHTING(i)) {
          for (vict = world[i->in_room].people; vict; vict = vict->next_in_room)
            if (FIGHTING(vict) == ch) {
              act("$n jumps to the aid of $N!", FALSE, i, 0, ch, TO_ROOM);
              hit(i, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
              break;
            }
        } else {
          HUNTING(i) = ch;
          hunt_victim(i);
        }
      }
    }
  }
  PROC_FIRED(ch) = TRUE;
}

/* another hl port, checks if object with given vnum is being worn */
bool is_wearing(struct char_data *ch, obj_vnum vnum) {
  int i;

  for (i = 0; i < NUM_WEARS; i++) {
    if (GET_EQ(ch, i))
      if (GET_OBJ_VNUM(GET_EQ(ch, i)) == vnum)
        return TRUE;
  }
  return FALSE;
}

/******* end general procedures for mobile procs ****/

/** begin actual mob procs **/

SPECIAL(guild) {
  int skill_num, percent;

  if (IS_NPC(ch) || (!CMD_IS("practice") && !CMD_IS("train") && !CMD_IS("boosts")))
    return (FALSE);

  skip_spaces(&argument);

  // Practice code
  if (CMD_IS("practice")) {
    if (!*argument) {
      list_skills(ch);
      return (TRUE);
    }
    if (GET_PRACTICES(ch) <= 0) {
      send_to_char(ch, "You do not seem to be able to practice now.\r\n");
      return (TRUE);
    }

    skill_num = find_skill_num(argument);

    if (skill_num < 1 ||
            GET_LEVEL(ch) < spell_info[skill_num].min_level[(int) GET_CLASS(ch)]) {
      send_to_char(ch, "You do not know of that %s.\r\n", SPLSKL(ch));
      return (TRUE);
    }

    if (GET_SKILL(ch, skill_num) >= LEARNED(ch)) {
      send_to_char(ch, "You are already learned in that area.\r\n");
      return (TRUE);
    }

    if (skill_num > SPELL_RESERVED_DBC && skill_num < MAX_SPELLS) {
      send_to_char(ch, "You can't practice spells.\r\n");
      return (TRUE);
    }

    if (!meet_skill_reqs(ch, skill_num)) {
      send_to_char(ch, "You haven't met the pre-requisites for that skill.\r\n");
      return (TRUE);
    }

    /* added with addition of crafting system so you can't use your
     'practice points' for training your crafting skills which have
     a much lower base value than 75 */

    if (GET_SKILL(ch, skill_num)) {
      send_to_char(ch, "You already have this skill trained.\r\n");
      return TRUE;
    }

    send_to_char(ch, "You practice '%s' with your trainer...\r\n",
            spell_info[skill_num].name);
    GET_PRACTICES(ch)--;

    percent = GET_SKILL(ch, skill_num);
    percent += MIN(MAXGAIN(ch), MAX(MINGAIN(ch), int_app[GET_INT(ch)].learn));

    SET_SKILL(ch, skill_num, MIN(LEARNED(ch), percent));

    if (GET_SKILL(ch, skill_num) >= LEARNED(ch))
      send_to_char(ch, "You are now \tGlearned\tn in '%s.'\r\n",
            spell_info[skill_num].name);

    //for further expansion - zusuk
    process_skill(ch, skill_num);

    return (TRUE);

  } else if (CMD_IS("train")) {

    //training code

    if (!*argument) {
      list_abilities(ch);
      return (TRUE);
    }

    if (GET_TRAINS(ch) <= 0) {
      send_to_char(ch, "You do not seem to be able to train now.\r\n");
      return (TRUE);
    }

    skill_num = find_ability_num(argument);

    if (skill_num < 1) {
      send_to_char(ch, "You do not know of that ability.\r\n");
      return (TRUE);
    }

    //ability not available to this class
    if (class_ability[skill_num][GET_CLASS(ch)] == 0) {
      send_to_char(ch, "This ability is not available to your class...\r\n");
      return (TRUE);
    }

    //cross-class ability
    if (GET_TRAINS(ch) < 2 && class_ability[skill_num][GET_CLASS(ch)] == 1) {
      send_to_char(ch, "(Cross-Class) You don't have enough training sessions to train that ability...\r\n");
      return (TRUE);
    }
    if (GET_ABILITY(ch, skill_num) >= ((int) ((GET_LEVEL(ch) + 3) / 2)) && class_ability[skill_num][GET_CLASS(ch)] == 1) {
      send_to_char(ch, "You are already trained in that area.\r\n");
      return (TRUE);
    }

    //class ability
    if (GET_ABILITY(ch, skill_num) >= (GET_LEVEL(ch) + 3) && class_ability[skill_num][GET_CLASS(ch)] == 2) {
      send_to_char(ch, "You are already trained in that area.\r\n");
      return (TRUE);
    }

    send_to_char(ch, "You train for a while...\r\n");
    GET_TRAINS(ch)--;
    if (class_ability[skill_num][GET_CLASS(ch)] == 1) {
      GET_TRAINS(ch)--;
      send_to_char(ch, "You used two training sessions to train a cross-class ability...\r\n");
    }
    GET_ABILITY(ch, skill_num)++;

    if (GET_ABILITY(ch, skill_num) >= (GET_LEVEL(ch) + 3))
      send_to_char(ch, "You are now trained in that area.\r\n");
    if (GET_ABILITY(ch, skill_num) >= ((int) ((GET_LEVEL(ch) + 3) / 2)) && class_ability[skill_num][GET_CLASS(ch)] == 1)
      send_to_char(ch, "You are already trained in that area.\r\n");

    return (TRUE);
  } else if (CMD_IS("boosts")) {
    if (!argument || !*argument)
      send_to_char(ch, "\tCStat boost sessions remaining: %d\tn\r\n"
            "\tcStats:\tn\r\n"
            "Strength\r\n"
            "Constitution\r\n"
            "Dexterity\r\n"
            "Intelligence\r\n"
            "Wisdom\r\n"
            "Charisma\r\n"
            "\r\n",
            GET_BOOSTS(ch));
    else if (!GET_BOOSTS(ch))
      send_to_char(ch, "You have no ability training sessions.\r\n");
    else if (!strncasecmp("strength", argument, strlen(argument))) {
      send_to_char(ch, CONFIG_OK);
      send_to_char(ch, "\tMYour strength increases!\tn\r\n");
      GET_REAL_STR(ch) += 1;
      GET_BOOSTS(ch) -= 1;
    } else if (!strncasecmp("constitution", argument, strlen(argument))) {
      send_to_char(ch, CONFIG_OK);
      send_to_char(ch, "\tMYour constitution increases!\tn\r\n");
      GET_REAL_CON(ch) += 1;
      /* Give them retroactive hit points for constitution */
      if (!(GET_REAL_CON(ch) % 2)) {
        GET_REAL_MAX_HIT(ch) += GET_LEVEL(ch);
        send_to_char(ch, "\tMYou gain %d hitpoints!\tn\r\n", GET_LEVEL(ch));
      }
      GET_BOOSTS(ch) -= 1;
    } else if (!strncasecmp("dexterity", argument, strlen(argument))) {
      send_to_char(ch, CONFIG_OK);
      send_to_char(ch, "\tMYour dexterity increases!\tn\r\n");
      GET_REAL_DEX(ch) += 1;
      GET_BOOSTS(ch) -= 1;
    } else if (!strncasecmp("intelligence", argument, strlen(argument))) {
      send_to_char(ch, CONFIG_OK);
      send_to_char(ch, "\tMYour intelligence increases!\tn\r\n");
      GET_REAL_INT(ch) += 1;
      /* Give extra skill practice, but only for this level */
      if (!(GET_REAL_INT(ch) % 2))
        GET_TRAINS(ch)++;
      GET_BOOSTS(ch) -= 1;
    } else if (!strncasecmp("wisdom", argument, strlen(argument))) {
      send_to_char(ch, CONFIG_OK);
      send_to_char(ch, "\tMYour wisdom increases!\tn\r\n");
      GET_REAL_WIS(ch) += 1;
      GET_BOOSTS(ch) -= 1;
    } else if (!strncasecmp("charisma", argument, strlen(argument))) {
      send_to_char(ch, CONFIG_OK);
      send_to_char(ch, "\tMYour charisma increases!\tn\r\n");
      GET_REAL_CHA(ch) += 1;
      GET_BOOSTS(ch) -= 1;
    } else
      send_to_char(ch, "\tCStat boost sessions remaining: %d\tn\r\n"
            "\tcStats:\tn\r\n"
            "Strength\r\n"
            "Constitution\r\n"
            "Dexterity\r\n"
            "Intelligence\r\n"
            "Wisdom\r\n"
            "Charisma\r\n"
            "\r\n",
            GET_BOOSTS(ch));
    affect_total(ch);
    send_to_char(ch, "\tDType 'practice' to see your skills\tn\r\n");
    send_to_char(ch, "\tDType 'train' to see your abilities\tn\r\n");
    send_to_char(ch, "\tDType 'boost' to adjust your stats\tn\r\n");
    send_to_char(ch, "\tDType 'spells <classname>' to see your currently known spells\tn\r\n");
    return (TRUE);
  }

  //should not be able to get here
  log("Reached the unreachable in SPECIAL(guild) in spec_procs.c");
  return (FALSE);

}

SPECIAL(mayor) {
  char actbuf[MAX_INPUT_LENGTH];

  const char open_path[] =
          "W3a3003b33000c111d0d111Oe333333Oe22c222112212111a1S.";
  const char close_path[] =
          "W3a3003b33000c111d0d111CE333333CE22c222112212111a1S.";

  static const char *path = NULL;
  static int path_index;
  static bool move = FALSE;

  if (!move) {
    if (time_info.hours == 6) {
      move = TRUE;
      path = open_path;
      path_index = 0;
    } else if (time_info.hours == 20) {
      move = TRUE;
      path = close_path;
      path_index = 0;
    }
  }
  if (cmd || !move || (GET_POS(ch) < POS_SLEEPING) || FIGHTING(ch))
    return (FALSE);

  switch (path[path_index]) {
    case '0':
    case '1':
    case '2':
    case '3':
      perform_move(ch, path[path_index] - '0', 1);
      break;

    case 'W':
      GET_POS(ch) = POS_STANDING;
      act("$n awakens and groans loudly.", FALSE, ch, 0, 0, TO_ROOM);
      break;

    case 'S':
      GET_POS(ch) = POS_SLEEPING;
      act("$n lies down and instantly falls asleep.", FALSE, ch, 0, 0, TO_ROOM);
      break;

    case 'a':
      act("$n says 'Hello Honey!'", FALSE, ch, 0, 0, TO_ROOM);
      act("$n smirks.", FALSE, ch, 0, 0, TO_ROOM);
      break;

    case 'b':
      act("$n says 'What a view!  I must get something done about that dump!'",
              FALSE, ch, 0, 0, TO_ROOM);
      break;

    case 'c':
      act("$n says 'Vandals!  Youngsters nowadays have no respect for anything!'",
              FALSE, ch, 0, 0, TO_ROOM);
      break;

    case 'd':
      act("$n says 'Good day, citizens!'", FALSE, ch, 0, 0, TO_ROOM);
      break;

    case 'e':
      act("$n says 'I hereby declare the bazaar open!'", FALSE, ch, 0, 0, TO_ROOM);
      break;

    case 'E':
      act("$n says 'I hereby declare Midgen closed!'", FALSE, ch, 0, 0, TO_ROOM);
      break;

    case 'O':
      do_gen_door(ch, strcpy(actbuf, "gate"), 0, SCMD_UNLOCK); /* strcpy: OK */
      do_gen_door(ch, strcpy(actbuf, "gate"), 0, SCMD_OPEN); /* strcpy: OK */
      break;

    case 'C':
      do_gen_door(ch, strcpy(actbuf, "gate"), 0, SCMD_CLOSE); /* strcpy: OK */
      do_gen_door(ch, strcpy(actbuf, "gate"), 0, SCMD_LOCK); /* strcpy: OK */
      break;

    case '.':
      move = FALSE;
      break;

  }

  path_index++;
  return (FALSE);
}

/* Quite lethal to low-level characters. */
SPECIAL(snake) {
  if (cmd || !FIGHTING(ch))
    return (FALSE);

  if (IN_ROOM(FIGHTING(ch)) != IN_ROOM(ch) || rand_number(0, GET_LEVEL(ch)) != 0)
    return (FALSE);

  act("$n bites $N!", 1, ch, 0, FIGHTING(ch), TO_NOTVICT);
  act("$n bites you!", 1, ch, 0, FIGHTING(ch), TO_VICT);
  call_magic(ch, FIGHTING(ch), 0, SPELL_POISON, GET_LEVEL(ch), CAST_SPELL);
  return (TRUE);
}

SPECIAL(hound) {
  struct char_data *i;
  int door;
  room_rnum room;

  if (cmd || GET_POS(ch) != POS_STANDING || FIGHTING(ch))
    return (FALSE);

  /* first go through all the directions */
  for (door = 0; door < DIR_COUNT; door++) {
    if (CAN_GO(ch, door)) {
      room = world[IN_ROOM(ch)].dir_option[door]->to_room;

      /* ok found a neighboring room, now cycle through the peeps */
      for (i = world[room].people; i; i = i->next_in_room) {
        /* is this guy a hostile? */
        if (i && IS_NPC(i) && MOB_FLAGGED(i, MOB_AGGRESSIVE)) {
          act("$n howls a warning!", FALSE, ch, 0, 0, TO_ROOM);
          return (TRUE);
        }
      } // end peeps cycle
    } // can_go
  } // end room cycle

  return (FALSE);
}

SPECIAL(thief) {
  struct char_data *cons;

  if (cmd || GET_POS(ch) != POS_STANDING)
    return (FALSE);

  for (cons = world[IN_ROOM(ch)].people; cons; cons = cons->next_in_room)
    if (!IS_NPC(cons) && GET_LEVEL(cons) < LVL_IMMORT && !rand_number(0, 4)) {
      npc_steal(ch, cons);
      return (TRUE);
    }

  return (FALSE);
}

SPECIAL(wizard) {
  struct char_data *vict;

  if (cmd || !FIGHTING(ch))
    return (FALSE);

  /* pseudo-randomly choose someone in the room who is fighting me */
  for (vict = world[IN_ROOM(ch)].people; vict; vict = vict->next_in_room)
    if (FIGHTING(vict) == ch && !rand_number(0, 4))
      break;

  /* if I didn't pick any of those, then just slam the guy I'm fighting */
  if (vict == NULL && IN_ROOM(FIGHTING(ch)) == IN_ROOM(ch))
    vict = FIGHTING(ch);

  /* Hm...didn't pick anyone...I'll wait a round. */
  if (vict == NULL)
    return (TRUE);

  if (GET_LEVEL(ch) > 13 && rand_number(0, 10) == 0)
    cast_spell(ch, vict, NULL, SPELL_POISON);

  if (GET_LEVEL(ch) > 7 && rand_number(0, 8) == 0)
    cast_spell(ch, vict, NULL, SPELL_BLINDNESS);

  if (GET_LEVEL(ch) > 12 && rand_number(0, 12) == 0) {
    if (IS_EVIL(ch))
      cast_spell(ch, vict, NULL, SPELL_ENERGY_DRAIN);
    else if (IS_GOOD(ch))
      cast_spell(ch, vict, NULL, SPELL_DISPEL_EVIL);
  }

  if (rand_number(0, 4))
    return (TRUE);

  switch (GET_LEVEL(ch)) {
    case 4:
    case 5:
      cast_spell(ch, vict, NULL, SPELL_MAGIC_MISSILE);
      break;
    case 6:
    case 7:
      cast_spell(ch, vict, NULL, SPELL_CHILL_TOUCH);
      break;
    case 8:
    case 9:
      cast_spell(ch, vict, NULL, SPELL_BURNING_HANDS);
      break;
    case 10:
    case 11:
      cast_spell(ch, vict, NULL, SPELL_SHOCKING_GRASP);
      break;
    case 12:
    case 13:
      cast_spell(ch, vict, NULL, SPELL_LIGHTNING_BOLT);
      break;
    case 14:
    case 15:
    case 16:
    case 17:
      cast_spell(ch, vict, NULL, SPELL_COLOR_SPRAY);
      break;
    default:
      cast_spell(ch, vict, NULL, SPELL_FIREBALL);
      break;
  }
  return (TRUE);
}

SPECIAL(wall) {
  if (!IS_MOVE(cmd))
    return (FALSE);

  /* acceptable ways to avoid the wall */
  /* */

  /* failed to get past wall */
  send_to_char(ch, "You can't get by the magical wall!\r\n");
  act("$n fails to get past the magical wall!", FALSE, ch, 0, 0, TO_ROOM);
  return (TRUE);
}

SPECIAL(guild_guard) {
  int i, direction;
  struct char_data *guard = (struct char_data *) me;
  const char *buf = "The guard humiliates you, and blocks your way.\r\n";
  const char *buf2 = "The guard humiliates $n, and blocks $s way.";

  if (!IS_MOVE(cmd) || AFF_FLAGGED(guard, AFF_BLIND))
    return (FALSE);

  if (GET_LEVEL(ch) >= LVL_IMMORT)
    return (FALSE);

  /* find out what direction they are trying to go */
  for (direction = 0; direction < NUM_OF_DIRS; direction++)
    if (!strcmp(cmd_info[cmd].command, dirs[direction]))
      for (direction = 0; direction < DIR_COUNT; direction++)
        if (!strcmp(cmd_info[cmd].command, dirs[direction]) ||
                !strcmp(cmd_info[cmd].command, autoexits[direction]))
          break;

  for (i = 0; guild_info[i].guild_room != NOWHERE; i++) {
    /* Wrong guild. */
    if (GET_ROOM_VNUM(IN_ROOM(ch)) != guild_info[i].guild_room)
      continue;

    /* Wrong direction. */
    if (direction != guild_info[i].direction)
      continue;

    /* Allow the people of the guild through. */
    /* Can't use GET_CLASS anymore, need CLASS_LEVEL(ch, i)!!  - 04/08/2013 Ornir */
    if (!IS_NPC(ch) && (CLASS_LEVEL(ch, guild_info[i].pc_class) > 0))
      continue;

    send_to_char(ch, "%s", buf);
    act(buf2, FALSE, ch, 0, 0, TO_ROOM);
    return (TRUE);
  }
  return (FALSE);
}

SPECIAL(puff) {
  char actbuf[MAX_INPUT_LENGTH];

  if (cmd)
    return (FALSE);

  switch (rand_number(0, 60)) {
    case 0:
      do_say(ch, strcpy(actbuf, "My god!  It's full of stars!"), 0, 0); /* strcpy: OK */
      return (TRUE);
    case 1:
      do_say(ch, strcpy(actbuf, "How'd all those fish get up here?"), 0, 0); /* strcpy: OK */
      return (TRUE);
    case 2:
      do_say(ch, strcpy(actbuf, "I'm a very female dragon."), 0, 0); /* strcpy: OK */
      return (TRUE);
    case 3:
      do_say(ch, strcpy(actbuf, "I've got a peaceful, easy feeling."), 0, 0); /* strcpy: OK */
      return (TRUE);
    default:
      return (FALSE);
  }
}

SPECIAL(fido) {
  struct obj_data *i, *temp, *next_obj;

  if (cmd || !AWAKE(ch))
    return (FALSE);

  for (i = world[IN_ROOM(ch)].contents; i; i = i->next_content) {
    if (!IS_CORPSE(i))
      continue;

    act("$n savagely devours a corpse.", FALSE, ch, 0, 0, TO_ROOM);
    for (temp = i->contains; temp; temp = next_obj) {
      next_obj = temp->next_content;
      obj_from_obj(temp);
      obj_to_room(temp, IN_ROOM(ch));
    }
    extract_obj(i);
    return (TRUE);
  }
  return (FALSE);
}

SPECIAL(janitor) {
  struct obj_data *i;

  if (cmd || !AWAKE(ch))
    return (FALSE);

  for (i = world[IN_ROOM(ch)].contents; i; i = i->next_content) {
    if (!CAN_WEAR(i, ITEM_WEAR_TAKE))
      continue;
    if (GET_OBJ_TYPE(i) != ITEM_DRINKCON && GET_OBJ_COST(i) >= 15)
      continue;
    act("$n picks up some trash.", FALSE, ch, 0, 0, TO_ROOM);
    obj_from_room(i);
    obj_to_char(i, ch);
    return (TRUE);
  }
  return (FALSE);
}

SPECIAL(cityguard) {
  struct char_data *tch, *evil, *spittle;
  int max_evil, min_cha;

  if (cmd || !AWAKE(ch) || FIGHTING(ch))
    return (FALSE);

  max_evil = 1000;
  min_cha = 6;
  spittle = evil = NULL;

  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room) {
    if (!CAN_SEE(ch, tch))
      continue;
    if (!IS_NPC(tch) && PLR_FLAGGED(tch, PLR_KILLER)) {
      act("$n screams 'HEY!!!  You're one of those PLAYER KILLERS!!!!!!'", FALSE, ch, 0, 0, TO_ROOM);
      hit(ch, tch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
      return (TRUE);
    }

    if (!IS_NPC(tch) && PLR_FLAGGED(tch, PLR_THIEF)) {
      act("$n screams 'HEY!!!  You're one of those PLAYER THIEVES!!!!!!'", FALSE, ch, 0, 0, TO_ROOM);
      hit(ch, tch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
      return (TRUE);
    }

    if (FIGHTING(tch) && GET_ALIGNMENT(tch) < max_evil && (IS_NPC(tch) || IS_NPC(FIGHTING(tch)))) {
      max_evil = GET_ALIGNMENT(tch);
      evil = tch;
    }

    if (GET_CHA(tch) < min_cha) {
      spittle = tch;
      min_cha = GET_CHA(tch);
    }
  }

  /*
  if (evil && GET_ALIGNMENT(FIGHTING(evil)) >= 0) {
    act("$n screams 'PROTECT THE INNOCENT!  BANZAI!  CHARGE!  ARARARAGGGHH!'", FALSE, ch, 0, 0, TO_ROOM);
    hit(ch, evil, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
    return (TRUE);
  }
   */

  /* Reward the socially inept. */
  if (spittle && !rand_number(0, 9)) {
    static int spit_social;

    if (!spit_social)
      spit_social = find_command("spit");

    if (spit_social > 0) {
      char spitbuf[MAX_NAME_LENGTH + 1];
      strncpy(spitbuf, GET_NAME(spittle), sizeof (spitbuf)); /* strncpy: OK */
      spitbuf[sizeof (spitbuf) - 1] = '\0';
      do_action(ch, spitbuf, spit_social, 0);
      return (TRUE);
    }
  }
  return (FALSE);
}

SPECIAL(clan_cleric) {
  int i;
  char buf[MAX_STRING_LENGTH];
  zone_vnum clanhall;
  clan_vnum clan;
  struct char_data *this_mob = (struct char_data *) me;

  struct price_info {
    short int number;
    char name[25];
    short int price;
  } clan_prices[] = {
    /* Spell Num (defined)      Name shown        Price  */
    { SPELL_ARMOR, "armor             ", 75},
    { SPELL_BLESS, "bless            ", 150},
    { SPELL_REMOVE_POISON, "remove poison    ", 525},
    { SPELL_CURE_BLIND, "cure blindness   ", 375},
    { SPELL_CURE_CRITIC, "critic           ", 525},
    { SPELL_SANCTUARY, "sanctuary       ", 3000},
    { SPELL_HEAL, "heal            ", 3500},

    /* The next line must be last, add new spells above. */
    { -1, "\r\n", -1}
  };

  if (CMD_IS("buy") || CMD_IS("list")) {
    argument = one_argument(argument, buf);

    /* Which clanhall is this cleric in? */
    clanhall = zone_table[(GET_ROOM_ZONE(IN_ROOM(this_mob)))].number;
    if ((clan = zone_is_clanhall(clanhall)) == NO_CLAN) {
      log("SYSERR: clan_cleric spec (%s) not in a known clanhall (room %d)", GET_NAME(this_mob), world[(IN_ROOM(this_mob))].number);
      return FALSE;
    }
    if (clan != GET_CLAN(ch)) {
      sprintf(buf, "$n will only serve members of %s", CLAN_NAME(real_clan(clan)));
      act(buf, TRUE, this_mob, 0, ch, TO_VICT);
      return TRUE;
    }

    if (FIGHTING(ch)) {
      send_to_char(ch, "You can't do that while fighting!\r\n");
      return TRUE;
    }

    if (*buf) {
      for (i = 0; clan_prices[i].number > SPELL_RESERVED_DBC; i++) {
        if (is_abbrev(buf, clan_prices[i].name)) {
          if (GET_GOLD(ch) < clan_prices[i].price) {
            act("$n tells you, 'You don't have enough gold for that spell!'",
                    FALSE, this_mob, 0, ch, TO_VICT);
            return TRUE;
          } else {

            act("$N gives $n some money.",
                    FALSE, this_mob, 0, ch, TO_NOTVICT);
            send_to_char(ch, "You give %s %d coins.\r\n",
                    GET_NAME(this_mob), clan_prices[i].price);
            decrease_gold(ch, clan_prices[i].price);
            /* Uncomment the next line to make the mob get RICH! */
            /* increase_gold(this_mob, clan_prices[i].price); */

            cast_spell(this_mob, ch, NULL, clan_prices[i].number);
            return TRUE;

          }
        }
      }
      act("$n tells you, 'I do not know of that spell!"
              "  Type 'buy' for a list.'", FALSE, this_mob,
              0, ch, TO_VICT);

      return TRUE;
    } else {
      act("$n tells you, 'Here is a listing of the prices for my services.'",
              FALSE, this_mob, 0, ch, TO_VICT);
      for (i = 0; clan_prices[i].number > SPELL_RESERVED_DBC; i++) {
        send_to_char(ch, "%s%d\r\n", clan_prices[i].name, clan_prices[i].price);
      }
      return TRUE;
    }
  }
  return FALSE;
}

SPECIAL(clan_guard) {
  zone_vnum clanhall, to_zone;
  clan_vnum clan;
  struct char_data *guard = (struct char_data *) me;
  char *buf = "The guard humiliates you, and blocks your way.\r\n";
  char *buf2 = "The guard humiliates $n, and blocks $s way.";

  if (!IS_MOVE(cmd) || IS_AFFECTED(guard, AFF_BLIND))
    return FALSE;

  if (GET_LEVEL(ch) >= LVL_IMMORT)
    return FALSE;

  /* Which clanhall is this cleric in? */
  clanhall = zone_table[(GET_ROOM_ZONE(IN_ROOM(guard)))].number;
  if ((clan = zone_is_clanhall(clanhall)) == NO_CLAN) {
    log("SYSERR: clan_guard spec (%s) not in a known clanhall (room %d)", GET_NAME(guard), world[(IN_ROOM(guard))].number);
    return FALSE;
  }

  /* This is the player's clanhall, allow them to pass */
  if (GET_CLAN(ch) == clan) {
    return FALSE;
  }

  /* If the exit leads to another clanhall room, block it */
  /* NOTE: cmd equals the direction for directional commands */
  if (EXIT(ch, cmd) && EXIT(ch, cmd)->to_room && EXIT(ch, cmd)->to_room != NOWHERE) {
    to_zone = zone_table[(GET_ROOM_ZONE(EXIT(ch, cmd)->to_room))].number;
    if (to_zone == clanhall) {
      act(buf, FALSE, ch, 0, 0, TO_CHAR);
      act(buf2, FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    }
  }

  /* If we get here, player is allowed to leave */
  return FALSE;
}

/* from homeland */
SPECIAL(shar_heart) {
  struct char_data *vict = FIGHTING(ch);
  struct affected_type af;
  int dam = 0;

  if (!ch || cmd || !vict)
    return 0;

  if (rand_number(0, 15))
    return 0;

  act("\tmThe \tMHeart of Shar \tn\tmpulses erratically in\r\n"
          "your hand before striking $N \tmwith a beam of\r\n"
          "\tLmalevolent light\tn\tm, bathing and filling $M with\r\n"
          "the virulence of the \tLL\tMady of \tLL\tMoss.\tn"
          , FALSE, ch, 0, vict, TO_CHAR);

  act("\tmThe amethyst orb wielded by \tL$n \tn\tmpulses\r\n"
          "erratically before a beam of \tLmalevolent light\r\n"
          "\tn\tmshoots from it, striking you in the chest!\tn"
          , FALSE, ch, 0, vict, TO_VICT);

  act("\tL$n \tn\tmis bathed in an amethyst radiance as $s\r\n"
          "\tMHeart of Shar \tn\tmpulses erratically.  Suddenly a\r\n"
          "sickly beam of \tLmalevolent light \tn\tmblazes\r\n"
          "towards $N\tm, filling $S body with the \tLvirulence\r\n"
          "\tn\tmof the \tLL\tMady of \tLL\tMoss.\tn"
          , FALSE, ch, 0, vict, TO_ROOM);

  af.duration = 5;
  af.modifier = -4;
  af.location = APPLY_STR;
  af.spell = SPELL_POISON;
  affect_join(vict, &af, FALSE, FALSE, FALSE, FALSE);

  dam = dice(6, 3) + 4;
  GET_HIT(vict) -= dam;
  return 1;
}

/* from homeland */
SPECIAL(shar_statue) {
  struct char_data *mob;
  
  if (!FIGHTING(ch))
    return FALSE;
  if (cmd)
    return FALSE;

  if (!rand_number(0, 8) || !PROC_FIRED(ch)) {
    PROC_FIRED(ch) = TRUE;
    send_to_room(ch->in_room,
            "\tLThe statue raises her ebon arms, screaming out to\r\n"
            "her deity in a booming voice, '\tn\tmLady of loss,\r\n"
            "mistress of the night, smite those who befoul your\r\n"
            "house.  Send forth your faithful to quench the light\r\n"
            "of their moon!\tL'\tn\r\n");

    if (dice(1, 100) < 50)
      mob = read_mobile(106241, VIRTUAL);
    else
      mob = read_mobile(106240, VIRTUAL);

    if (!mob)
      return FALSE;
      
    char_to_room(mob, ch->in_room);
    add_follower(mob, ch);

    return TRUE;
  }
  return FALSE;
}

/* from homeland */
SPECIAL(dog) {
  int random = 0;
  struct affected_type af;
  struct char_data *pet = (struct char_data *) me;

  if (!argument)
    return 0;
  if (!cmd)
    return 0;

  skip_spaces(&argument);

  if (!isname(argument, GET_NAME(pet)))
    return 0;

  if (CMD_IS("pet") || CMD_IS("pat")) {
    random = dice(1, 3);
    switch (random) {
      case 3:
        act("$n tries to lick your hand as you pet $m.", FALSE, pet, 0, ch, TO_VICT);
        act("$n tries to lick the hand of $N as $E pet $m.", FALSE, pet, 0, ch, TO_NOTVICT);
        break;
      case 2:
        act("$n looks at you with adoring eyes as you pet $m.", FALSE, pet, 0, ch, TO_VICT);
        act("$n looks at $N with adoring eyes as $E pet $m.", FALSE, pet, 0, ch, TO_NOTVICT);
        break;
      case 1:
      default:
        act("$n wags $s tail happily, as you pet $m.", FALSE, pet, 0, ch, TO_VICT);
        act("$n wags $s tail happily, as $N pets $m.", FALSE, pet, 0, ch, TO_NOTVICT);
        break;
    }
    
    if (GET_LEVEL(pet) < 2 && ch->followers == 0 && ch->master == 0 && pet->master == 0 && !circle_follow(pet, ch)) {
      add_follower(pet, ch);
      af.spell = SPELL_CHARM;
      af.duration = 24000;
      af.modifier = 0;
      af.location = 0;
      SET_BIT_AR(af.bitvector, AFF_CHARM);
      affect_to_char(pet, &af);
    }
    return 1;
  }
  return 0;
}

/* from homeland */
SPECIAL(illithid_gguard) {
  const char *buf = "$N \tLsteps in front of you, blocking you from accessing the gate.\tn";
  const char *buf2 = "$N \tLsteps in front of $n\tL, blocking access the gate.\tn";

  if (!IS_MOVE(cmd))
    return FALSE;

  //if (cmd == SCMD_EAST && GET_RACE(ch) != RACE_ILLITHID) {
  if (cmd == SCMD_EAST) {
    act(buf, FALSE, ch, 0, (struct char_data *) me, TO_CHAR);
    act(buf2, FALSE, ch, 0, (struct char_data *) me, TO_ROOM);
    return TRUE;
  }

  return FALSE;
}

/* from homeland */
SPECIAL(duergar_guard) {
  const char *buf = "$N steps into the opening and blocks your path.\r\n";
  const char *buf2 = "$N steps into the opening blocking it.";

  if (!IS_MOVE(cmd))
    return FALSE;

  if (cmd == SCMD_DOWN) {
    act(buf, FALSE, ch, 0, (struct char_data *) me, TO_CHAR);
    act(buf2, FALSE, ch, 0, (struct char_data *) me, TO_ROOM);
    return TRUE;
  }

  return FALSE;
}

/* from homeland */
SPECIAL(bandit_guard) {
  const char *buf = "$N blocks your access into the castle.\r\n";
  const char *buf2 = "$N blocks $n's access into the castle..";

  if (!IS_MOVE(cmd))
    return FALSE;

  if (GET_LEVEL(ch) < 12)
    return FALSE;

  if (cmd == SCMD_EAST || cmd == SCMD_SOUTH || cmd == SCMD_WEST) {
    act(buf, FALSE, ch, 0, (struct char_data *) me, TO_CHAR);
    act(buf2, FALSE, ch, 0, (struct char_data *) me, TO_ROOM);
    return TRUE;
  }

  return FALSE;
}

/* from homeland */
SPECIAL(secomber_guard) {
  const char *buf = "\tLThe doorguard steps before you, blocking your way with an upraised hand.\tn\r\n";
  const char *buf2 = "\tLThe doorguard blocks \tn$n\tL's way, placing one meaty hand on $s chest.\tn";

  if (!IS_MOVE(cmd))
    return FALSE;

  if (cmd == SCMD_EAST) {
    send_to_char(ch, buf);
    act(buf2, FALSE, ch, 0, 0, TO_ROOM);
    return TRUE;
  }

  return FALSE;
}

/* from homeland */
/*
SPECIAL(guild_golem) {
  bool found = TRUE;
  const char *msg1 = "The golem humiliates you, and blocks your way.\r\n";
  const char *msg2 = "The golem humiliates $n, and blocks $s way.";

  if (!IS_MOVE(cmd))
    return FALSE;

  int i = cmd - 1;
  
  if (i < 0) {
    send_to_char("Index error in guild golem\r\n", ch);
    return FALSE;
  }

  if (!EXIT(ch, i))
    found = FALSE;
  else {
    int room_number = world[ch->in_room].dir_option[i]->to_room;
    if (world[room_number].guild_index) {
      if (GET_GUILD(ch) != world[room_number].guild_index && GET_ALT(ch) != world[room_number].guild_index)
        found = FALSE;
    }
  }

  if (!found) {
    send_to_char(msg1, ch);
    act(msg2, FALSE, ch, 0, 0, TO_ROOM);
    return TRUE;
  }

  return FALSE;
}
*/

/* from Homeland */
/*
SPECIAL(guild_guard) {
  int i;
  bool found = TRUE;
  const char *buf = "The guard humiliates you, and blocks your way.\r\n";
  const char *buf2 = "The guard humiliates $n, and blocks $s way.";

  if (!IS_MOVE(cmd))
    return FALSE;

  if (GET_LEVEL(ch) >= LVL_IMMORT)
    return FALSE;

  for (i = 0; guild_info[i][0] != -1; i++) {
    if (GET_ROOM_VNUM(IN_ROOM(ch)) == guild_info[i][1] &&
            cmd == guild_info[i][2]) {
      if (IS_NPC(ch) || GET_CLASS(ch) != guild_info[i][0]) {
        found = FALSE;
      } else {
        found = TRUE;
        break;
      }
    }
  }

  if (!found) {
    send_to_char(buf, ch);
    act(buf2, FALSE, ch, 0, 0, TO_ROOM);
    return TRUE;
  }

  return FALSE;
}
*/

/* from Homeland */
//doesnt work properly if multiple instances.. :) -V
SPECIAL(practice_dummy) {
  int rounddam = 0;
  static int round_count;
  static int max_hit;
  char buf[MAX_INPUT_LENGTH];

  if (cmd)
    return 0;

  if (!FIGHTING(ch)) {
    GET_MAX_HIT(ch) = 20000;
    GET_HIT(ch) = 20000;
    max_hit = 0;
    round_count = 0;
  } else {
    rounddam = GET_MAX_HIT(ch) - GET_HIT(ch);
    max_hit += rounddam;
    round_count++;

    sprintf(buf, "\tP%d damage last round!\tn  \tc(total: %d rounds: %d)\tn\r\n", 
            rounddam, max_hit, round_count);
    send_to_room(ch->in_room, buf);
    GET_HIT(ch) = GET_MAX_HIT(ch);
    return 1;
  }
  return 0;
}

/* from Homeland */
SPECIAL(wraith) {
  if (cmd)
    return 0;

  if (GET_POS(ch) == POS_DEAD || !ch->master) {
    act("With a loud shriek, $n crumbles into dust.", FALSE, ch, NULL, 0, TO_ROOM);
    extract_char(ch);
    return 1;
  }
  
  if (ch->master && ch->in_room == ch->master->in_room)
    if (FIGHTING(ch->master) && rand_number(0, 1)) {
      perform_assist(ch, ch->master);
      return 1;
    }

  return 0;
}

/* from Homeland */
SPECIAL(skeleton_zombie) {
  if (cmd)
    return 0;

  if (GET_POS(ch) == POS_DEAD || !ch->master) {
    act("With a loud shriek, $n crumbles into dust.", FALSE, ch, NULL, 0, TO_ROOM);
    extract_char(ch);
    return 1;
  }

  if (ch->master && ch->in_room == ch->master->in_room)
    if (FIGHTING(ch->master) && !rand_number(0, 2)) {
      perform_assist(ch, ch->master);
      return 1;
    }

  return 0;
}

/* from Homeland */
SPECIAL(vampire) {
  struct char_data *vict;

  if (cmd)
    return 0;

  if (GET_POS(ch) == POS_DEAD || !ch->master) {
    act("With a loud shriek, $n crumbles into dust.", FALSE, ch, NULL, 0, TO_ROOM);
    extract_char(ch);
    return 1;
  }

  if (ch->master && ch->in_room == ch->master->in_room) {
    for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room) {
      if (FIGHTING(vict) == ch->master && !rand_number(0, 1)) {
        perform_rescue(ch, ch->master);
        return 1;
      }
    }
  }

  return 0;
}

/* from Homeland */
SPECIAL(totemanimal) {
  if (cmd)
    return 0;
  if (!ch->master)
    return 0;

  if (ch->master && ch->in_room == ch->master->in_room)
    if (FIGHTING(ch->master))
      perform_assist(ch, ch->master);
  return 0;
}

/* from Homeland */
SPECIAL(shades) {
  if (cmd)
    return 0;

  if (GET_MAX_HIT(ch) > 1 && GET_HIT(ch) > 1) {
    GET_MAX_HIT(ch) = 1;
    GET_HIT(ch) = 1;
  }

  if (GET_POS(ch) == POS_DEAD)
    return 0;
  if (GET_HIT(ch) < GET_MAX_HIT(ch) || !ch->master) {
    act("A shade evaporates into thin air.", FALSE, ch, NULL, 0, TO_ROOM);
    extract_char(ch);
    return 1;
  }

  if (ch->in_room != ch->master->in_room) {
    HUNTING(ch) = ch->master;
    hunt_victim(ch);
    return 1;
  }
  return 0;
}

/* from Homeland */
SPECIAL(solid_elemental) {
  struct char_data *vict;

  if (cmd)
    return 0;

  if (GET_POS(ch) == POS_DEAD || (!ch->master && !MOB_FLAGGED(ch, MOB_MEMORY))) {
    act("With a loud shriek, $n returns to $s home plane.", FALSE, ch, NULL, 0, TO_ROOM);
    extract_char(ch);
    return 1;
  }

  if (GET_HIT(ch) > 0) {
    if (ch->master && ch->in_room == ch->master->in_room && !rand_number(0, 1)) {
      for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room) {
        if (FIGHTING(vict) == ch->master) {
          perform_rescue(ch, ch->master);
          return 1;
        }
      }
    }


    if (!FIGHTING(ch) && ch->master && FIGHTING(ch->master) && ch->in_room == ch->master->in_room) {
      perform_assist(ch, ch->master);
      return 1;
    }
  }

  // auto stand if down
  if (GET_POS(ch) < POS_FIGHTING && GET_POS(ch) >= POS_STUNNED) {
    GET_POS(ch) = POS_STANDING;
    act("$n clambers to $s feet.\r\n", FALSE, ch, 0, 0, TO_ROOM);
    return 1;
  }

  // we're fighting something we dont want to fight...
  if (!ch->master && FIGHTING(ch) && IS_NPC(FIGHTING(ch)) && !IS_PET(FIGHTING(ch)))
    do_flee(ch, 0, 0, 0);

  return 0;
}

/* from Homeland */
SPECIAL(wraith_elemental) {
  struct char_data *vict;

  if (cmd)
    return 0;

  if (GET_POS(ch) == POS_DEAD || (!ch->master && !MOB_FLAGGED(ch, MOB_MEMORY))) {
    act("With a loud shriek, $n returns to $s home plane.", FALSE, ch, NULL, 0, TO_ROOM);
    extract_char(ch);
    return 1;
  }

  if (GET_HIT(ch) > 0) {
    if (ch->master && ch->in_room == ch->master->in_room && !rand_number(0, 1)) {
      for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room) {
        if (FIGHTING(vict) == ch->master) {
          perform_rescue(ch, ch->master);
          return 1;
        }
      }
    }

    if (!FIGHTING(ch) && ch->master && FIGHTING(ch->master) && ch->in_room == ch->master->in_room) {
      perform_assist(ch, ch->master);
      return 1;
    }
  }
  
  // auto stand if down
  if (GET_POS(ch) < POS_FIGHTING && GET_POS(ch) >= POS_STUNNED) {
    GET_POS(ch) = POS_STANDING;
    act("$n clambers to $s feet.\r\n", FALSE, ch, 0, 0, TO_ROOM);
    return 1;
  }

  // we're fighting something we dont want to fight...
  if (!ch->master && FIGHTING(ch) && IS_NPC(FIGHTING(ch)) && !IS_PET(FIGHTING(ch)))
    do_flee(ch, 0, 0, 0);

  return 0;
}

/* from homeland */
SPECIAL(planewalker) {
  if (cmd)
    return 0;

  if (ROOM_FLAGGED(ch->in_room, ROOM_SOUNDPROOF)) {
    act("$n looks around in panic when he realizes that his spells\r\n"
            "would fizzle. He reaches down into his pockets and pulls out an ancient\r\n"
            "rod. He taps the rod and suddenly disappears!", FALSE
            , ch, 0, 0, TO_ROOM);
    call_magic(ch, 0, 0, SPELL_TELEPORT, 30, CAST_WAND);
    return 1;
  }
  if (!FIGHTING(ch) && GET_HIT(ch) < GET_MAX_HIT(ch)) {
    act("$n checks on his wounds, and grabs a potion from his pockets."
            , FALSE, ch, 0, 0, TO_ROOM);
    call_magic(ch, ch, 0, SPELL_HEAL, 30, CAST_POTION);
    return 1;
  }
  return 0;
}

/* from homeland */
SPECIAL(phantom) {
  struct char_data *vict;
  struct char_data *next_vict;
  int prob, percent;

  if (cmd)
    return 0;

  if (!FIGHTING(ch))
    return 0;
  if (rand_number(0, 4))
    return 0;

  act("$n \tLlets out a \trfrightening\tL wail\tn",
          FALSE, ch, 0, 0, TO_ROOM);

  for (vict = world[ch->in_room].people; vict; vict = next_vict) {
    next_vict = vict->next_in_room;

    if (vict == ch)
      continue;
    if (IS_NPC(vict) && !IS_PET(vict))
      continue;

    percent = rand_number(1, 111); /* 101% is a complete failure */
    prob = GET_WIS(vict) + 5;
    if (FIGHTING(vict))
      prob *= 2;
    if (prob > 100)
      prob = 100;

    if (percent > prob)
      do_flee(vict, NULL, 0, 0);
  }
  return 1;
}

/* from homeland */
SPECIAL(lichdrain) {
  struct char_data *tch = 0;
  struct char_data *vict = 0;
  int dam = 0;
  
  if (cmd || GET_POS(ch) == POS_DEAD)
    return 0;
  if (rand_number(0, 3))
    return 0;
  if (!FIGHTING(ch))
    return 0;
  
  if (AFF_FLAGGED(ch, AFF_PARALYZED))
    return 0;

  for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room) {
    if (!IS_NPC(tch) || IS_PET(tch)) {
      if (!vict || !rand_number(0, 2)) {
        vict = tch;
      }
    }
  }
  
  if (!vict)
    return 0;

  act("\tn$n\tL looks deep into your soul with $s horrid gaze.\tn\r\n"
          "\tLand $e simply leeches your \tWlifeforce\tL out of you.\r\n",
          FALSE, ch, 0, vict, TO_VICT);

  act("\tn$n\tL looks deep into the eyes of $N\tL with $s horrid gaze.\tn\r\n"
          "\tLand $e simply leeches $S \tWlifeforce\tL out of $M.\r\n",
          TRUE, ch, 0, vict, TO_NOTVICT);

  act("\tWYou reach out and suck the life force away from $N!", TRUE, ch, 0, 
          vict, TO_CHAR);
  dam = GET_HIT(vict) + 5;
  if (GET_HIT(ch) + dam < GET_MAX_HIT(ch))
    GET_HIT(ch) += dam;
  GET_HIT(vict) -= dam;
  WAIT_STATE(vict, PULSE_VIOLENCE);
  return 1;
}

/* from homeland */
SPECIAL(harpell) {
  struct char_data *i = NULL;
  char buf[MAX_INPUT_LENGTH];

  if (cmd || GET_POS(ch) == POS_DEAD)
    return 0;

  if (!FIGHTING(ch))
    PROC_FIRED(ch) = FALSE;

  if (FIGHTING(ch) && !ROOM_FLAGGED(ch->in_room, ROOM_SOUNDPROOF)) {
    if (AFF_FLAGGED(FIGHTING(ch), AFF_CHARM) && FIGHTING(ch)->master)
      sprintf(buf, "%s shouts, 'HELP! %s has ordered his pets to kill "
              "me!!'\r\n", ch->player.short_descr, 
              GET_NAME(FIGHTING(ch)->master));
    else
      sprintf(buf, "%s shouts, 'HELP! %s is trying to kill me!\r\n",
            ch->player.short_descr, GET_NAME(FIGHTING(ch)));
    for (i = character_list; i; i = i->next) {
      if (!FIGHTING(i) && IS_NPC(i) && (GET_MOB_VNUM(i) == 106831 ||
              GET_MOB_VNUM(i) == 106841 || GET_MOB_VNUM(i) == 106842 ||
              GET_MOB_VNUM(i) == 106844 || GET_MOB_VNUM(i) == 106845 ||
              GET_MOB_VNUM(i) == 106846) && ch != i && !rand_number(0, 2)) {
        if (AFF_FLAGGED(FIGHTING(ch), AFF_CHARM) && FIGHTING(ch)->master &&
                (FIGHTING(ch)->master->in_room != FIGHTING(ch)->in_room)) {
          if (FIGHTING(ch)->master->in_room != i->in_room)
            cast_spell(i, FIGHTING(ch)->master, NULL, SPELL_TELEPORT);
          else
            hit(i, FIGHTING(ch)->master, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, 
                    FALSE);
        } else {
          if (FIGHTING(ch)->in_room != i->in_room)
            cast_spell(i, FIGHTING(ch), NULL, SPELL_TELEPORT);
          else
            hit(i, FIGHTING(ch), TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
        }
      }

      if (world[ch->in_room].zone == world[i->in_room].zone && !PROC_FIRED(ch))
        send_to_char(i, buf);
    }
    PROC_FIRED(ch) = TRUE;
    return 1;
  } // for loop

  return 0;
}

/* from homeland */
SPECIAL(bonedancer) {
  struct char_data *vict;
  struct char_data *next_vict;

  if (cmd)
    return 0;
  if (GET_POS(ch) == POS_DEAD || !ch->master) {
    act("With a loud shriek, $n crumbles into dust.", FALSE, ch, NULL, 0, 
            TO_ROOM);
    extract_char(ch);
    return 1;
  }
  
  if (!FIGHTING(ch) && GET_HIT(ch) > 0) {
    for (vict = world[ch->in_room].people; vict; vict = next_vict) {
      next_vict = vict->next_in_room;
      if (vict != ch && CAN_SEE(ch, vict)) {
        hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
        return 1;
      }
    }
  }

  return 0;
}

/* from homeland */
SPECIAL(wallach) {

  if (cmd || GET_POS(ch) == POS_DEAD)
    return 0;

  if (GET_ROOM_VNUM(GET_MOB_LOADROOM(ch)) != 112638)
    GET_MOB_LOADROOM(ch) = real_room(112638);

  return 0;
}

/* from homeland */
SPECIAL(beltush) {
  struct char_data *i;

  if (cmd || GET_POS(ch) == POS_DEAD || GET_ROOM_VNUM(ch->in_room) != 112648)
    return 0;


  for (i = character_list; i; i = i->next)
    if (!IS_NPC(i) && GET_ROOM_VNUM(i->in_room) == 112602) {
      do_enter(ch, "mirror", 0, 0);
      act("Beltush says, 'FOOLS!! How dare you attempt to enter the flaming "
              "tower!!", FALSE, ch, 0, 0, TO_ROOM);
      return 1;
    }

  return 0;
}

/* from homeland */
SPECIAL(mereshaman) {
  if (cmd)
    return 0;
  
  if (FIGHTING(ch) && !PROC_FIRED(ch)) {
    PROC_FIRED(ch) = TRUE;
    send_to_room(ch->in_room,
            "\tLThe \tglizardman \tLshaman chants loudly, '\tGUktha slithiss "
            "Semuanya! Ssithlarss sunggar uk!\tL'\tn\r\n"
            "\tLThe monitor lizard statues shudder and vibrate then take on \tn\r\n"
            "\tLa \tGbright green glow\tL. Each opens up like a cocoon releasing the\tn\r\n"
            "\tLreptilian beast contained within.\tn\r\n");

    char_to_room(read_mobile(126725, VIRTUAL), ch->in_room);
    char_to_room(read_mobile(126725, VIRTUAL), ch->in_room);
    char_to_room(read_mobile(126725, VIRTUAL), ch->in_room);
    char_to_room(read_mobile(126725, VIRTUAL), ch->in_room);
    return 1;
  }
  return 0;
}

/* from homeland */
/*
SPECIAL(willowisp) {
  room_rnum room = real_room(126899);
  
  if (cmd)    
    return 0;
  
  if (FIGHTING(ch))
    return 0;

  if (ch->in_room != room && weather_info.sunlight == SUN_LIGHT) {
    act("$n fades away in the sunlight!", FALSE, ch, 0, 0, TO_ROOM);
    ch->mob_specials.temp_data = ch->in_room;
    char_from_room(ch);
    char_to_room(ch, room);

    return 1;
  }

  if (ch->in_room == room && weather_info.sunlight != SUN_LIGHT) {
    char_from_room(ch);
    char_to_room(ch, ch->mob_specials.temp_data);
    act("$n appears with the dark of the night!", FALSE, ch, 0, 0, TO_ROOM);
    return 1;
  }

  return 0;
}
*/

/* from homeland */
SPECIAL(naga_golem) {
  if (cmd || GET_POS(ch) == POS_DEAD)
    return 0;

  if (!FIGHTING(ch))
    PROC_FIRED(ch) = FALSE;

  if (FIGHTING(ch)) {
    zone_yell(ch,
            "\r\n\tLThe golem rings an alarm bell, which echoes through "
            "the pit.\tn\r\n");
    return 1;
  }

  return 0;
}

/* from homeland */
/*
SPECIAL(naga) {
  struct char_data *tch = 0;
  struct char_data *vict = 0;
  int dam = 0;
  
  if (cmd || GET_POS(ch) == POS_DEAD)
    return 0;
  if (rand_number(0, 3))
    return 0;
  if (!FIGHTING(ch))
    return 0;
  
  for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room) {
    if ((!IS_NPC(tch) || IS_PET(tch)) && !AFF_FLAGGED(tch, AFF_NOSLEEP)) {
      if (!vict || !rand_number(0, 3)) {
        vict = tch;
      }
    }
  }
  if (!vict)
    return 0;
  
  if (AFF_FLAGGED(vict, AFF_NOSLEEP))
    return 0;
  act(    "$n\tL thrusts its powerful barbed tail-stinger into your flesh causing\tn\r\n"
          "\tLyou to scream in agony.  As it snaps back its tail, poison oozes into the large\tn\r\n"
          "\tLwound that is opened.  You begin to fall into a drug induced "
          "sleep.\tn\r\n", FALSE, ch, 0, vict, TO_VICT);


  act(    "$n\tL thrusts its powerful barbed tail-stinger into $N's flesh causing\tn\r\n"
          "\tL$M\tL to scream in agony.  As it snaps back its tail, poison oozes into the large\tn\r\n"
          "\tLwound that is opened.  \tn$N\tL begin to fall into a drug "
          "induced sleep.\tn\r\n", TRUE, ch, 0, vict, TO_NOTVICT);

  act("\tLYour poison stinger hits $N!\tn", TRUE, ch, 0, vict, TO_CHAR);
  dam = GET_LEVEL(ch)*2 + dice(2, GET_LEVEL(ch));
  if (dam > GET_HIT(vict))
    dam = GET_HIT(vict);
  if (dam < 0)
    dam = 0;
  GET_HIT(vict) -= dam;
  stop_fighting(vict);
  GET_POS(vict) = POS_SLEEPING;
  WAIT_STATE(vict, PULSE_VIOLENCE * 2);
  return 1;
}
*/

/********************************************************************/
/******************** Room Procs      *******************************/
/********************************************************************/


SPECIAL(dump) {
  struct obj_data *k;
  int value = 0;

  for (k = world[IN_ROOM(ch)].contents; k; k = world[IN_ROOM(ch)].contents) {
    act("$p vanishes in a puff of smoke!", FALSE, 0, k, 0, TO_ROOM);
    extract_obj(k);
  }

  if (!CMD_IS("drop"))
    return (FALSE);

  do_drop(ch, argument, cmd, SCMD_DROP);

  for (k = world[IN_ROOM(ch)].contents; k; k = world[IN_ROOM(ch)].contents) {
    act("$p vanishes in a puff of smoke!", FALSE, 0, k, 0, TO_ROOM);
    value += MAX(1, MIN(50, GET_OBJ_COST(k) / 10));
    extract_obj(k);
  }

  if (value) {
    send_to_char(ch, "You are awarded for outstanding performance.\r\n");
    act("$n has been awarded for being a good citizen.", TRUE, ch, 0, 0, TO_ROOM);

    if (GET_LEVEL(ch) < 3)
      gain_exp(ch, value);
    else
      increase_gold(ch, value);
  }
  return (TRUE);
}


#define PET_PRICE(pet) (GET_LEVEL(pet) * 300)

SPECIAL(pet_shops) {
  char buf[MAX_STRING_LENGTH], pet_name[MEDIUM_STRING];
  room_rnum pet_room;
  struct char_data *pet;

  /* Gross. */
  pet_room = IN_ROOM(ch) + 1;

  if (CMD_IS("list")) {
    send_to_char(ch, "Available pets are:\r\n");
    for (pet = world[pet_room].people; pet; pet = pet->next_in_room) {
      /* No, you can't have the Implementor as a pet if he's in there. */
      if (!IS_NPC(pet))
        continue;
      send_to_char(ch, "%8d - %s\r\n", PET_PRICE(pet), GET_NAME(pet));
    }
    return (TRUE);
  } else if (CMD_IS("buy")) {

    two_arguments(argument, buf, pet_name);

    if (!(pet = get_char_room(buf, NULL, pet_room)) || !IS_NPC(pet)) {
      send_to_char(ch, "There is no such pet!\r\n");
      return (TRUE);
    }
    if (GET_GOLD(ch) < PET_PRICE(pet)) {
      send_to_char(ch, "You don't have enough gold!\r\n");
      return (TRUE);
    }
    decrease_gold(ch, PET_PRICE(pet));

    pet = read_mobile(GET_MOB_RNUM(pet), REAL);
    GET_EXP(pet) = 0;
    SET_BIT_AR(AFF_FLAGS(pet), AFF_CHARM);

    if (*pet_name) {
      snprintf(buf, sizeof (buf), "%s %s", pet->player.name, pet_name);
      /* free(pet->player.name); don't free the prototype! */
      pet->player.name = strdup(buf);

      snprintf(buf, sizeof (buf), "%sA small sign on a chain around the neck says 'My name is %s'\r\n",
              pet->player.description, pet_name);
      /* free(pet->player.description); don't free the prototype! */
      pet->player.description = strdup(buf);
    }
    char_to_room(pet, IN_ROOM(ch));
    add_follower(pet, ch);

    /* Be certain that pets can't get/carry/use/wield/wear items */
    IS_CARRYING_W(pet) = 1000;
    IS_CARRYING_N(pet) = 100;

    send_to_char(ch, "May you enjoy your pet.\r\n");
    act("$n buys $N as a pet.", FALSE, ch, 0, pet, TO_ROOM);

    return (TRUE);
  }

  /* All commands except list and buy */
  return (FALSE);
}



/********************************************************************/
/******************** Object Procs    *******************************/
/********************************************************************/

/****  object procs general functions ****/


/* NOTE to be confused with the weapon-spells code used in OLC, etc */
/*  This was ported to accomodate the HL objects that were imported */
void weapons_spells(char *to_ch, char *to_vict, char *to_room,
        struct char_data *ch, struct char_data *vict,
        struct obj_data *obj, int spl) {
  int level;

  level = GET_LEVEL(ch);
  
  if (level > 30)
    level = 30;

  act(to_ch, FALSE, ch, obj, vict, TO_CHAR);
  act(to_vict, FALSE, ch, obj, vict, TO_VICT);
  act(to_room, FALSE, ch, obj, vict, TO_NOTVICT);
  call_magic(ch, vict, 0, spl, level, CAST_WAND);
}



/*** end object procs general functions ***/

SPECIAL(bank) {
  int amount;

  if (CMD_IS("balance")) {
    if (GET_BANK_GOLD(ch) > 0)
      send_to_char(ch, "Your current balance is %d coins.\r\n", GET_BANK_GOLD(ch));
    else
      send_to_char(ch, "You currently have no money deposited.\r\n");
    return (TRUE);
  } else if (CMD_IS("deposit")) {
    if ((amount = atoi(argument)) <= 0) {
      send_to_char(ch, "How much do you want to deposit?\r\n");
      return (TRUE);
    }
    if (GET_GOLD(ch) < amount) {
      send_to_char(ch, "You don't have that many coins!\r\n");
      return (TRUE);
    }
    decrease_gold(ch, amount);
    increase_bank(ch, amount);
    send_to_char(ch, "You deposit %d coins.\r\n", amount);
    act("$n makes a bank transaction.", TRUE, ch, 0, FALSE, TO_ROOM);
    return (TRUE);
  } else if (CMD_IS("withdraw")) {
    if ((amount = atoi(argument)) <= 0) {
      send_to_char(ch, "How much do you want to withdraw?\r\n");
      return (TRUE);
    }
    if (GET_BANK_GOLD(ch) < amount) {
      send_to_char(ch, "You don't have that many coins deposited!\r\n");
      return (TRUE);
    }
    increase_gold(ch, amount);
    decrease_bank(ch, amount);
    send_to_char(ch, "You withdraw %d coins.\r\n", amount);
    act("$n makes a bank transaction.", TRUE, ch, 0, FALSE, TO_ROOM);
    return (TRUE);
  } else
    return (FALSE);
}

/*
   Portal that will jump to a player's clanhall
   Exit depends on which clan player belongs to
   Created by Jamdog - 4th July 2006
 */
SPECIAL(clanportal) {
  int iPlayerClan = -1;
  struct obj_data *obj = (struct obj_data *) me;
  struct obj_data *port;
  zone_vnum z;
  room_vnum r;
  char obj_name[MAX_INPUT_LENGTH];
  room_rnum was_in = IN_ROOM(ch);
  struct follow_type *k;

  if (!CMD_IS("enter")) return FALSE;

  argument = one_argument(argument, obj_name);

  /* Check that the player is trying to enter THIS portal */
  if (!(port = get_obj_in_list_vis(ch, obj_name, NULL, world[(IN_ROOM(ch))].contents))) {
    return (FALSE);
  }

  if (port != obj)
    return (FALSE);

  iPlayerClan = GET_CLAN(ch);

  if (iPlayerClan == NO_CLAN) {
    send_to_char(ch, "You try to enter the portal, but it returns you back to the same room!\n\r");
    return TRUE;
  }

  if ((z = get_clanhall_by_char(ch)) == NOWHERE) {
    send_to_char(ch, "Your clan does not have a clanhall!\n\r");
    log("Warning: Clan Portal - No clanhall (Player: %s, Clan ID: %d)", GET_NAME(ch), iPlayerClan);
    return TRUE;
  }

  //  r = (z * 100) + 1;    /* Get room xxx01 in zone xxx */
  /* for now lets have the exit room be 3000, until we get hometowns in, etc */
  r = 3000;

  if (!(real_room(r))) {
    send_to_char(ch, "Your clanhall is currently broken - contact an Imm!\n\r");
    log("Warning: Clan Portal failed (Player: %s, Clan ID: %d)", GET_NAME(ch), iPlayerClan);
    return TRUE;
  }

  /* First, move the player */
  if (!(House_can_enter(ch, r))) {
    send_to_char(ch, "That's private property -- no trespassing!\r\n");
    return TRUE;
  }

  act("$n enters $p, and vanishes!", FALSE, ch, port, 0, TO_ROOM);
  act("You enter $p, and you are transported elsewhere", FALSE, ch, port, 0, TO_CHAR);
  char_from_room(ch);
  char_to_room(ch, real_room(r));
  look_at_room(ch, 0);
  act("$n appears from thin air!", FALSE, ch, 0, 0, TO_ROOM);

  /* Then, any followers should auto-follow (Jamdog 19th June 2006) */
  for (k = ch->followers; k; k = k->next) {
    if ((IN_ROOM(k->follower) == was_in) && (GET_POS(k->follower) >= POS_STANDING)) {
      act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
      char_from_room(k->follower);
      char_to_room(k->follower, real_room(r));
      look_at_room(k->follower, 0);
      act("$n appears from thin air!", FALSE, k->follower, 0, 0, TO_ROOM);
    }
  }
  return TRUE;
}

/* from homeland */
SPECIAL(hellfire) {
  if (!cmd && !strcmp(argument, "identify")) {
    send_to_char(ch, "Invoke haste and fireshield on armor by saying 'Hellfire'.\r\n");
    return 1;
  }

  if (!cmd)
    return 0;
  if (!ch)
    return 0;
  if (!argument)
    return 0;

  skip_spaces(&argument);

  if (!is_wearing(ch, 132102))
    return 0;

  if (!strcmp(argument, "hellfire") && cmd_info[cmd].command_pointer == do_say) {
    if (GET_OBJ_SPECTIMER((struct obj_data *) me, 0) > 0) {
      send_to_char(ch, "Nothing happens.\r\n");
      return 1;
    }

    act("\tLThe pure flames of your $p\tL is invoked.\tn\r\n"
            "\tLThe flames rise and protects YOU!\tn\r\n",
            FALSE, ch, (struct obj_data *) me, 0, TO_CHAR);
    act("\tLThe pure flames of $n\tL's $p\tL is invoked.\tn\r\n"
            "\tLThe flames rise and protects $m!\tn\r\n",
            FALSE, ch, (struct obj_data *) me, 0, TO_ROOM);

    call_magic(ch, ch, 0, SPELL_FIRE_SHIELD, 26, CAST_POTION);
    call_magic(ch, ch, 0, SPELL_HASTE, 26, CAST_POTION);

    GET_OBJ_SPECTIMER((struct obj_data *) me, 0) = 12;
    return 1;
  }
  return 0;
}

/* from homeland */
SPECIAL(angel_leggings) {
  if (!cmd && !strcmp(argument, "identify")) {
    send_to_char(ch, "Invoke fly by keyword 'Elysium'.\r\n");
    return 1;
  }

  if (!cmd)
    return 0;
  if (!ch)
    return 0;
  if (!argument)
    return 0;
  
  skip_spaces(&argument);
  
  if (!is_wearing(ch, 106021))
    return 0;
  
  if (!strcmp(argument, "elysium") && cmd_info[cmd].command_pointer ==
          do_say) {
    if (GET_OBJ_SPECTIMER((struct obj_data *) me, 0) > 0) {
      send_to_char(ch, "Nothing happens.\r\n");
      return 1;
    }

    act("\tWThe power of $p\tW is invoked.\tn\r\n"
            "\tcYour feet slowly raise off the ground.\tn\r\n",
            FALSE, ch, (struct obj_data *) me, 0, TO_CHAR);
    act("\tWThe power of $n\tW's $p\tW is invoked.\tn\r\n"
            "\tw$s feet slowly raise of the ground!\tn\r\n",
            FALSE, ch, (struct obj_data *) me, 0, TO_ROOM);

    call_magic(ch, ch, 0, SPELL_FLY, 30, CAST_POTION);
    GET_OBJ_SPECTIMER((struct obj_data *) me, 0) = 168;
    return 1;
  }
  return 0;
}

/* from homeland */
SPECIAL(bought_pet) {
  struct char_data *pet;

  if (cmd)
    return 0;

  struct obj_data *obj = (struct obj_data*) me;

  if (obj->carried_by == 0)
    return 0;

  if (IS_NPC(obj->carried_by))
    return 0;

  pet = read_mobile(GET_OBJ_VNUM(obj), VIRTUAL);
  if (pet) {
    char_to_room(pet, obj->carried_by->in_room);
    add_follower(pet, obj->carried_by);
    SET_BIT_AR(AFF_FLAGS(pet), AFF_CHARM);
    GET_MAX_MOVE(pet) = 150 + dice(GET_LEVEL(pet), 5);
    GET_MOVE(pet) = GET_MAX_MOVE(pet);

    extract_obj(obj);
    return 1;
  }
  return 0;
}

/* from homeland, i doubt we are going to port this, houses replace these */
/*
SPECIAL(storage_chest) {
  if (cmd)
    return 0;
  struct obj_data *chest = (struct obj_data*) me;
  if (GET_OBJ_VAL(chest, 3) != 0)
    return 0;
  ch = chest->carried_by;
  if (!ch) {
    REMOVE_BIT(GET_OBJ_EXTRA(chest), ITEM_INVISIBLE);
    return 0;
  }
  if (IS_NPC(ch) || IS_PET(ch))
    return 0;

  sprintf(buf2, "chest storage %s", GET_NAME(ch));
  chest->name = str_dup(buf2);

  if (GET_OBJ_VNUM(chest) == 1291) {
    sprintf(buf2, "\tLAn ornate \tcmithril\tL chest owned by \tw%s\tL rests here.\tn", GET_NAME(ch));
    chest->description = str_dup(buf2);
    sprintf(buf2, "\tLan ornate \tcmithril\tL chest owned by \tw%s\tn", GET_NAME(ch));
    chest->short_description = str_dup(buf2);

  } else {
    sprintf(buf2, "\tLA storage chest owned by \tW%s\tL is standing here.\tn", GET_NAME(ch));
    chest->description = str_dup(buf2);
    sprintf(buf2, "\tLa chest owned by \tW%s\tn", GET_NAME(ch));
    chest->short_description = str_dup(buf2);
  }
  GET_OBJ_VAL(chest, 3) = -GET_IDNUM(ch);
  SET_BIT(GET_OBJ_SAVED(chest), SAVE_OBJ_VALUES);
  SET_BIT(GET_OBJ_SAVED(chest), SAVE_OBJ_NAME);
  SET_BIT(GET_OBJ_SAVED(chest), SAVE_OBJ_DESC);
  SET_BIT(GET_OBJ_SAVED(chest), SAVE_OBJ_SHORT);
  SET_BIT(GET_OBJ_SAVED(chest), SAVE_OBJ_TYPE);
  SET_BIT(GET_OBJ_SAVED(chest), SAVE_OBJ_WEAR);
  SET_BIT(GET_OBJ_SAVED(chest), SAVE_OBJ_EXTRA);
  SET_BIT(GET_OBJ_SAVED(chest), SAVE_OBJ_TIMER);
  SET_BIT(GET_OBJ_SAVED(chest), SAVE_OBJ_WEIGHT);
  SET_BIT(GET_OBJ_SAVED(chest), SAVE_OBJ_COST);
  save_chests();
  return 1;
}
 */

/* from homeland */
SPECIAL(clang_bracer) {
  if (!cmd && !strcmp(argument, "identify")) {
    send_to_char(ch, "Dwarf-Only.  Invoke battle prowess by saying 'clangeddin'.\r\n");
    return 1;
  }
  if (ch && is_wearing(ch, 121456)) {
    if (!cmd && GET_RACE(ch) != RACE_DWARF) {
      act("\tLThe bracer begins to glow on your arm, clenching tighter and "
              "tighter until you rip it off in agony.\tn", FALSE, ch, 0, 0, 
              TO_CHAR);
      act("\tLA bracer on $n\tL's arm begins to glow brightly and a look of "
              "intense pain crosses $s face as $e rips the bracer free.\tn", 
              FALSE, ch, 0, 0, TO_ROOM);

      if (GET_EQ(ch, WEAR_WRIST_R) == (obj_data*) me)
        obj_to_char(unequip_char(ch, WEAR_WRIST_R), ch);
      else if (GET_EQ(ch, WEAR_WRIST_L) == (obj_data*) me)
        obj_to_char(unequip_char(ch, WEAR_WRIST_L), ch);
      return 1;
    }

    // invoke it!
    if (!cmd)
      return 0;
    if (!argument)
      return 0;
    if (!CMD_IS("say"))
      return 0;
    skip_spaces(&argument);

    if (!strcmp(argument, "clangeddin")) {
      if (GET_OBJ_SPECTIMER((struct obj_data *) me, 0) > 0) {
        send_to_char(ch, "Nothing happens.\r\n");
        return 1;
      }

      call_magic(ch, ch, 0, SPELL_MASS_ENHANCE, 30, CAST_POTION);
      GET_OBJ_SPECTIMER((struct obj_data *) me, 0) = 24;
      return 1;
    }

  }
  return 0;
}

/* from homeland */
SPECIAL(mercenary) {
  int hit;
  int base = 1;
  
  if (!ch)
    return 0;
  if (cmd)
    return 0;

  // a recruited merc should get reasonable amounts of hp.
  if (PROC_FIRED(ch) == FALSE && IS_PET(ch)) {
    switch (GET_CLASS(ch)) {
      case CLASS_RANGER:
      case CLASS_PALADIN:
      case CLASS_BERSERKER:
      case CLASS_WARRIOR:
        base = 8;
        break;
      case CLASS_ROGUE:
      case CLASS_MONK:
        base = 5;
        break;

      default:
        base = 3;
        break;
    }

    hit = dice(GET_LEVEL(ch), (1 + GET_CON_BONUS(ch))) + GET_LEVEL(ch) * base;
    GET_MAX_HIT(ch) = hit;
    if (GET_HIT(ch) > hit)
      GET_HIT(ch) = hit;
    PROC_FIRED(ch) = TRUE;
    return 1;
  }
  return 0;
}

/* from homeland */
SPECIAL(battlemaze_guard) {
  const char *buf = "$N \tL tells you, 'You don't want to go any farther, young one. \tn\r\n"
          "\tL You must be at least level ten to go into the more advanced\tn\r\n"
          "\tL parts of the battlemaze.'\tn";
  const char *buf2 = "$N \tLsteps in front of $n\tL, blocking access the gate.\tn";

  if (!IS_MOVE(cmd))
    return FALSE;

  if (cmd == SCMD_NORTH && GET_LEVEL(ch) < 10) {
    act(buf, FALSE, ch, 0, (struct char_data *) me, TO_CHAR);
    act(buf2, FALSE, ch, 0, (struct char_data *) me, TO_ROOM);
    return TRUE;
  }

  return FALSE;
}

/* from homeland */
SPECIAL(menzo_chokers) {
  struct affected_type *af2;
  struct affected_type af;

  if (!ch)
    return 0;

  for (af2 = ch->affected; af2; af2 = af2->next) {
    if (af2->spell == AFF_MENZOCHOKER) {
      if (!is_wearing(ch, 135626) || !is_wearing(ch, 135627)) {
        send_to_char(ch, "\tLYou suddenly feel bereft of your \tmgoddess's\tL"
                " touch.\tn\r\n");
        affect_from_char(ch, AFF_MENZOCHOKER);
      }
      return 0;
    }
  }

  if (is_wearing(ch, 135626) && is_wearing(ch, 135627)) {
    //if (GET_RACE(ch) == RACE_DROW) {
    if (GET_RACE(ch) == RACE_ELF) {
      send_to_char(ch, "\tLYour blood quickens, as if your soul has been touched "
              "by a higher power.\tn\r\n");
      af.location = APPLY_HITROLL;
      af.duration = 5;
      af.modifier = 1;
      SET_BIT_AR(af.bitvector, AFF_MENZOCHOKER);
      affect_join(ch, &af, FALSE, FALSE, TRUE, FALSE);
      return 0;
    }
  }
  return 0;
}

