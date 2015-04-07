/**************************************************************************
 *  File: fight.c                                           Part of tbaMUD *
 *  Usage: Combat system.                                                  *
 *                                                                         *
 *  All rights reserved.  See license for complete information.            *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 **************************************************************************/

#define __FIGHT_C__

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "spells.h"
#include "screen.h"
#include "constants.h"
#include "dg_scripts.h"
#include "act.h"
#include "class.h"
#include "fight.h"
#include "shop.h"
#include "quest.h"
#include "mud_event.h"
#include "spec_procs.h"
#include "clan.h"
#include "treasure.h"
#include "mudlim.h"
#include "spec_abilities.h"
#include "feats.h"
#include "actions.h"
#include "actionqueues.h"
#include "craft.h"

/* local global */
struct obj_data *last_missile = NULL;


/* head of l-list of fighting chars */
struct char_data *combat_list = NULL;

/* Weapon attack texts */
struct attack_hit_type attack_hit_text[] = {
  {"hit", "hits"}, /* 0 */
  {"sting", "stings"},
  {"whip", "whips"},
  {"slash", "slashes"},
  {"bite", "bites"},
  {"bludgeon", "bludgeons"}, /* 5 */
  {"crush", "crushes"},
  {"pound", "pounds"},
  {"claw", "claws"},
  {"maul", "mauls"},
  {"thrash", "thrashes"}, /* 10 */
  {"pierce", "pierces"},
  {"blast", "blasts"},
  {"punch", "punches"},
  {"stab", "stabs"}
};

/* local (file scope only) variables */
static struct char_data *next_combat_list = NULL;

/* local file scope utility functions */
struct obj_data* get_wielded(struct char_data *ch, int attack_type);
static void perform_group_gain(struct char_data *ch, int base,
        struct char_data *victim);
static void dam_message(int dam, struct char_data *ch, struct char_data *victim,
        int w_type, int offhand);
static void make_corpse(struct char_data *ch);
static void change_alignment(struct char_data *ch, struct char_data *victim);
static void group_gain(struct char_data *ch, struct char_data *victim);
static void solo_gain(struct char_data *ch, struct char_data *victim);
/** @todo refactor this function name */
static char *replace_string(const char *str, const char *weapon_singular,
        const char *weapon_plural);

#define IS_WEAPON(type) (((type) >= TYPE_HIT) && ((type) < TYPE_SUFFERING))

#define MODE_NORMAL_HIT       0 //Normal damage calculating in hit()
#define MODE_DISPLAY_PRIMARY  2 //Display damage info primary
#define MODE_DISPLAY_OFFHAND  3 //Display damage info offhand
#define MODE_DISPLAY_RANGED   4 //Display damage info ranged

/************ utility functions *********************/

/* simple utility function to check if ch is tanking */
bool is_tanking(struct char_data *ch) {
  struct char_data *vict;
  for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room) {
    if (FIGHTING(vict) == ch)
      return TRUE;
  }

  return FALSE;
}

/* code to check if vict is going to be auto-rescued by someone while
 being attacked by ch */
void guard_check(struct char_data *ch, struct char_data *vict) {
  struct char_data *tch;
  struct char_data *next_tch;

  if (!ch || !vict)
    return;

  if (ROOM_FLAGGED(ch->in_room, ROOM_SINGLEFILE))
    return;

  for (tch = world[ch->in_room].people; tch; tch = next_tch) {
    next_tch = tch->next_in_room;
    if (!tch)
      continue;
    if (tch == ch || tch == vict)
      continue;
    if (IS_NPC(tch))
      continue;
    if (GET_POS(tch) < POS_FIGHTING)
      continue;
    if (AFF_FLAGGED(tch, AFF_BLIND))
      continue;
    /*  Require full round action availability to guard.  */
    if (!is_action_available(tch, atSTANDARD, FALSE) ||
        !is_action_available(tch, atMOVE, FALSE))
      continue;
    if (GUARDING(tch) == vict) {
      /* This MUST be changed.  Skills are obsolete.
         Set to a flat 70% chance for now. */
      if (rand_number(1, 100) > 70) {
        act("$N fails to guard you.", FALSE, vict, 0, tch, TO_CHAR);
        act("You fail to guard $n.", FALSE, vict, 0, tch, TO_VICT);
      } else {
        act("$N protects you from attack!", FALSE, vict, 0, tch, TO_CHAR);
        act("$N guards $n succesfully.", FALSE, vict, 0, tch, TO_NOTVICT);
        act("You guard $n succesfully.", FALSE, vict, 0, tch, TO_VICT);

        perform_rescue(tch, vict);
        return;
      }
    }
  }
}

/* rewritten subfunction
   the engine for fleeing */
void perform_flee(struct char_data *ch) {
  int i, found = 0, fleeOptions[DIR_COUNT];
  struct char_data *was_fighting;

  /* disqualifications? */
  if (AFF_FLAGGED(ch, AFF_STUN) || AFF_FLAGGED(ch, AFF_DAZED) ||
          AFF_FLAGGED(ch, AFF_PARALYZED) || char_has_mud_event(ch, eSTUNNED)) {
    send_to_char(ch, "You try to flee, but you are unable to move!\r\n");
    act("$n attemps to flee, but is unable to move!", TRUE, ch, 0, 0, TO_ROOM);
    return;
  }
  /* got to be in a position to flee */
  if (GET_POS(ch) <= POS_SITTING) {
    send_to_char(ch, "You need to be standing to flee!\r\n");
    return;
  }

  USE_MOVE_ACTION(ch);

  //first find which directions are fleeable
  for (i = 0; i < DIR_COUNT; i++) {
    if (CAN_GO(ch, i)) {
      fleeOptions[found] = i;
      found++;
    }
  }

  //no actual fleeable directions
  if (!found) {
    send_to_char(ch, "You have no route of escape!\r\n");
    return;
  }

  //not fighting?  no problems
  if (!FIGHTING(ch)) {
    send_to_char(ch, "You quickly flee the area...\r\n");
    act("$n quickly flees the area!", TRUE, ch, 0, 0, TO_ROOM);

    //pick a random direction
    do_simple_move(ch, fleeOptions[rand_number(0, found - 1)], 3);

  } else {

    send_to_char(ch, "You attempt to flee:  ");
    act("$n attemps to flee...", TRUE, ch, 0, 0, TO_ROOM);

    //ok beat all odds, fleeing
    was_fighting = FIGHTING(ch);

    //pick a random direction
    if (do_simple_move(ch, fleeOptions[rand_number(0, found - 1)], 3)) {
      send_to_char(ch, "You quickly flee from combat...\r\n");
      act("$n quickly flees the battle!", TRUE, ch, 0, 0, TO_ROOM);
      stop_fighting(ch);
      if (was_fighting && ch == FIGHTING(was_fighting))
        stop_fighting(was_fighting);
    } else { //failure
      send_to_char(ch, "You failed to flee the battle...\r\n");
      act("$n failed to flee the battle!", TRUE, ch, 0, 0, TO_ROOM);
    }
  }
}

/* a function for removing sneak/hide/invisibility on a ch
   the forced variable is just used for greater invis */
void appear(struct char_data *ch, bool forced) {

  if (affected_by_spell(ch, SPELL_INVISIBLE))
    affect_from_char(ch, SPELL_INVISIBLE);

  if (AFF_FLAGGED(ch, AFF_SNEAK)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_SNEAK);
    send_to_char(ch, "You stop sneaking...\r\n");
    act("$n stops moving silently...", FALSE, ch, 0, 0, TO_ROOM);
  }

  if (AFF_FLAGGED(ch, AFF_HIDE)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);
    send_to_char(ch, "You step out of the shadows...\r\n");
    act("$n steps out of the shadows...", FALSE, ch, 0, 0, TO_ROOM);
  }

  //this is a hack, so order in this function is important
  if (affected_by_spell(ch, SPELL_GREATER_INVIS)) {
    if (forced) {
      affect_from_char(ch, SPELL_INVISIBLE);
      if (AFF_FLAGGED(ch, AFF_INVISIBLE))
        REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_INVISIBLE);
      send_to_char(ch, "You snap into visibility...\r\n");
      act("$n slowly fades into existence.", FALSE, ch, 0, 0, TO_ROOM);
    } else
      return;
  }

  /* this has to come after greater_invis */
  if (AFF_FLAGGED(ch, AFF_INVISIBLE)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_INVISIBLE);
    send_to_char(ch, "You snap into visibility...\r\n");
    act("$n slowly fades into existence.", FALSE, ch, 0, 0, TO_ROOM);
  }

}

// computing size bonuses in AC/damage
//  A attacking B
// defense, the defender should get this 'bonus'
// offense, the attack should get this 'bonus'

int compute_size_bonus(int sizeA, int sizeB) {

  // necessary and dummy checks
  if (sizeB < SIZE_FINE)
    sizeB = SIZE_FINE;
  if (sizeB > SIZE_COLOSSAL)
    sizeB = SIZE_COLOSSAL;
  if (sizeA < SIZE_FINE)
    sizeA = SIZE_FINE;
  if (sizeA > SIZE_COLOSSAL)
    sizeA = SIZE_COLOSSAL;

  return ((sizeB - sizeA) * 2);
}
/*  has_dex_bonus_to_ac(attacker, ch)
 *  Helper function to determine if a char can apply his dexterity bonus to his AC. */
bool has_dex_bonus_to_ac(struct char_data *attacker, struct char_data *ch) {
  if (!ch)
    return TRUE;
  /* conditions for losing dex to ch */

  /* ch is sleeping */
  if (!AWAKE(ch)) {
    /* debug */
    /*if (FIGHTING(ch))
      send_to_char(ch, "has_dex_bonus_to_ac() - %s not awake  ", GET_NAME(ch));*/
    return FALSE;
  }

  /*under discussion*/ /*(GET_POS(ch) < POS_FIGHTING))*/

  /* ch unable to see attacker WITHOUT blind-fighting feat */
  if (attacker) {
    if ( !CAN_SEE(ch, attacker) && !HAS_FEAT(ch, FEAT_BLIND_FIGHT) ) {
      /* debug */
      /*if (FIGHTING(ch))
        send_to_char(ch, "has_dex_bonus_to_ac() - %s unable to see attacker  ", GET_NAME(ch));*/
      return FALSE;
    }
  }

  /* ch is flat-footed WITHOUT uncanny dodge feat */
  if ((AFF_FLAGGED(ch, AFF_FLAT_FOOTED) && !HAS_FEAT(ch, FEAT_UNCANNY_DODGE))) {
    /* debug */
    /*if (FIGHTING(ch))
      send_to_char(ch, "has_dex_bonus_to_ac() - %s flat-footed  ", GET_NAME(ch));*/
    return FALSE;
  }

  /* ch is stunned */
  if (AFF_FLAGGED(ch, AFF_STUN) || char_has_mud_event(ch, eSTUNNED)) {
    /* debug */
    /*if (FIGHTING(ch))
      send_to_char(ch, "has_dex_bonus_to_ac() - %s stunned  ", GET_NAME(ch));*/
    return FALSE;
  }

  /* ch is paralyzed */
  if (AFF_FLAGGED(ch, AFF_PARALYZED)) {
    /* debug */
    /*if (FIGHTING(ch))
      send_to_char(ch, "has_dex_bonus_to_ac() - %s paralyzed  ", GET_NAME(ch));*/
    return FALSE;
  }

  /* debug */
  /*if (FIGHTING(ch))
    send_to_char(ch, "has_dex_bonus_to_ac() - %s retained dex bonus  ", GET_NAME(ch));*/
  return TRUE; /* ok, made it through, we DO have our dex bonus still */
}

/* our definition of flanking right now simply means the victim (ch's) target
   is not the attacker */
bool is_flanked(struct char_data *attacker, struct char_data *ch) {

  /* some instant disqualifiers */
  if (!attacker)
    return FALSE;
  if (!ch)
    return FALSE;

  /* most common scenario */
  if (FIGHTING(ch) && (FIGHTING(ch) != attacker) &&
      !HAS_FEAT(ch, FEAT_IMPROVED_UNCANNY_DODGE))
    return TRUE;

  /* ok so ch is fighting AND it is not the attacker tanking, by default
   * this is flanked, but we have to check for uncanny dodge */
  if (FIGHTING(ch) && (FIGHTING(ch) != attacker) &&
       HAS_FEAT(ch, FEAT_IMPROVED_UNCANNY_DODGE)) {

    int attacker_level = CLASS_LEVEL(attacker, CLASS_BERSERKER) +
                         CLASS_LEVEL(attacker, CLASS_ROGUE);
    int ch_level = CLASS_LEVEL(ch, CLASS_BERSERKER) +
                   CLASS_LEVEL(ch, CLASS_ROGUE);

    /* 4 or more levels of berserker or rogue will trump imp. uncanny dodge*/
    if (attacker_level >= (ch_level + 4))
      return TRUE;

    return FALSE; /* uncanny dodge WILL help */
  }

  return FALSE; /* default */
}

int roll_initiative(struct char_data *ch) {
  int initiative = 0;

  initiative = dice(1, 20) + GET_DEX_BONUS(ch) + 4 * HAS_FEAT(ch, FEAT_IMPROVED_INITIATIVE);
  //initiative += 2 * HAS_FEAT(ch, FEAT_IMPROVED_REACTION);
  //initiative += HAS_FEAT(ch, FEAT_HEROIC_INITIATIVE);

  return initiative;
}

/* this function will go through all the tests for modifying
   a given ch's AC under the circumstances of being attacked
 * by 'attacker' */
int compute_armor_class(struct char_data *attacker, struct char_data *ch, int is_touch) {
  /* hack to translate old D&D to 3.5 Edition
   * Modified 09/09/2014 : Ornir
   * Changed this to use the AC as-is.  AC has been modified on gear. */
  int armorclass = 0, eq_armoring = 0;
  int i, bonuses[NUM_BONUS_TYPES];

  /* base armor class of stock code = 100 */
  /* equipment is still using a 10 factor, example plate armor in d20 = 8,
   therefore in the code it would be 80 - this has to be consistent otherwise
   the calculation below will get skewed */
  eq_armoring = ((GET_AC(ch) - 100) / 10);
  armorclass = 10; /* base AC */

  /* Initialize bonuses to 0 */
  for (i = 0; i < NUM_BONUS_TYPES; i++)
    bonuses[i] = 0;

  if (char_has_mud_event(ch, eSHIELD_RECOVERY)) {
    if (GET_EQ(ch, WEAR_SHIELD))
      eq_armoring -= (apply_ac(ch, WEAR_SHIELD) / 10);
  }

  /**********/
  /* bonus types */

  /* bonus type natural-armor */
  /* arcana golem */
  if (GET_RACE(ch) == RACE_ARCANA_GOLEM) {
    bonuses[BONUS_TYPE_NATURALARMOR] -= 2;
  }
  if (char_has_mud_event(ch, eSPELLBATTLE) && SPELLBATTLE(ch) > 0) {
    bonuses[BONUS_TYPE_NATURALARMOR] += SPELLBATTLE(ch);
  }
  if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_ARMOR_SKIN)) {
    bonuses[BONUS_TYPE_NATURALARMOR] += HAS_FEAT(ch, FEAT_ARMOR_SKIN);
  }
  /**/

  /* bonus type armor (equipment) */
  /* we assume any ac above 10 will be equipment */
  bonuses[BONUS_TYPE_ARMOR] += eq_armoring;
  /* ...Trelux carapace is not effective vs touch attacks! */
  if (GET_RACE(ch) == RACE_TRELUX) {
    if (GET_LEVEL(ch) >= 5) {
      bonuses[BONUS_TYPE_ARMOR]++;
    }
    if (GET_LEVEL(ch) >= 10) {
      bonuses[BONUS_TYPE_ARMOR]++;
    }
    if (GET_LEVEL(ch) >= 15) {
      bonuses[BONUS_TYPE_ARMOR]++;
    }
    if (GET_LEVEL(ch) >= 20) {
      bonuses[BONUS_TYPE_ARMOR]++;
    }
    if (GET_LEVEL(ch) >= 25) {
      bonuses[BONUS_TYPE_ARMOR]++;
    }
    if (GET_LEVEL(ch) >= 30) {
      bonuses[BONUS_TYPE_ARMOR]++;
    }
  }
  /**/

  /* bonus type shield (usually equipment) */
  if (!is_touch && !IS_NPC(ch) && GET_EQ(ch, WEAR_SHIELD) &&
          GET_SKILL(ch, SKILL_SHIELD_SPECIALIST)) {
    bonuses[BONUS_TYPE_SHIELD] += 2;
  }
  /**/

  /* bonus type deflection */
  /* two weapon defense */
  if (!IS_NPC(ch) && GET_EQ(ch, WEAR_WIELD_2) && HAS_FEAT(ch, FEAT_TWO_WEAPON_DEFENSE)) {
    bonuses[BONUS_TYPE_DEFLECTION]++;
  }
  if (attacker) {
    if (AFF_FLAGGED(ch, AFF_PROTECT_GOOD) && IS_GOOD(attacker)) {
      bonuses[BONUS_TYPE_DEFLECTION] += 2;
    }
    if (AFF_FLAGGED(ch, AFF_PROTECT_EVIL) && IS_EVIL(attacker)) {
      bonuses[BONUS_TYPE_DEFLECTION] += 2;
    }
  }
  /**/

  /* bonus type enhancement (equipment) */
  /**/

  /* bonus type dodge */
  /* Determine if the ch loses their dex bonus to armor class. */
  if (has_dex_bonus_to_ac(attacker, ch)) {

    bonuses[BONUS_TYPE_DODGE] += GET_DEX_BONUS(ch);

    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_DODGE)) {
      bonuses[BONUS_TYPE_DODGE] += 1;
    }

    if (!IS_NPC(ch) && GET_ABILITY(ch, ABILITY_ACROBATICS)) { //caps at 5
      bonuses[BONUS_TYPE_DODGE] += MIN(5, (int) (compute_ability(ch, ABILITY_ACROBATICS) / 7));
    }

    if (AFF_FLAGGED(ch, AFF_EXPERTISE)) {
      bonuses[BONUS_TYPE_DODGE] += COMBAT_MODE_VALUE(ch);
    }
  }
  /**/

  /* bonus type circumstance */
  switch (GET_POS(ch)) { //position penalty
    case POS_RECLINING:
      bonuses[BONUS_TYPE_CIRCUMSTANCE] -= 3;
      break;
    case POS_SITTING:
    case POS_RESTING:
    case POS_STUNNED:
      bonuses[BONUS_TYPE_CIRCUMSTANCE] -= 2;
      break;
    case POS_SLEEPING:
    case POS_INCAP:
    case POS_MORTALLYW:
    case POS_DEAD:
      bonuses[BONUS_TYPE_CIRCUMSTANCE] -= 20;
      break;
    case POS_FIGHTING:
    case POS_STANDING:
    default: break;
  }
  if (char_has_mud_event(ch, eSTUNNED)) {/* POS_STUNNED below in case statement */
    bonuses[BONUS_TYPE_CIRCUMSTANCE] -= 2;
  }
  if (AFF_FLAGGED(ch, AFF_FATIGUED)) {
    bonuses[BONUS_TYPE_CIRCUMSTANCE] -= 2;
  }
  if (char_has_mud_event(ch, eTAUNTED)) {
    bonuses[BONUS_TYPE_CIRCUMSTANCE] -= 6;
  }
  /**/

  /* bonus type size */
  if (attacker) { /* racial bonus vs. larger opponents */
    if ((GET_RACE(ch) == RACE_DWARF ||
            GET_RACE(ch) == RACE_CRYSTAL_DWARF ||
            GET_RACE(ch) == RACE_GNOME ||
            GET_RACE(ch) == RACE_HALFLING
            ) && GET_SIZE(attacker) > GET_SIZE(ch)) {
      bonuses[BONUS_TYPE_SIZE] += compute_size_bonus(GET_SIZE(attacker), (GET_SIZE(ch) - 1));
    } else {
      bonuses[BONUS_TYPE_SIZE] += compute_size_bonus(GET_SIZE(attacker), GET_SIZE(ch));
    }
  }
  /**/

  /* bonus type undefined */
  /* favored enemy */
  if (attacker && attacker != ch && !IS_NPC(ch) && CLASS_LEVEL(ch, CLASS_RANGER)) {
    // checking if we have humanoid favored enemies for PC victims
    if (!IS_NPC(attacker) && IS_FAV_ENEMY_OF(ch, NPCRACE_HUMAN)) {
      bonuses[BONUS_TYPE_UNDEFINED] += CLASS_LEVEL(ch, CLASS_RANGER) / 5 + 2;
    } else if (IS_NPC(attacker) && IS_FAV_ENEMY_OF(ch, GET_RACE(attacker))) {
      bonuses[BONUS_TYPE_UNDEFINED] += CLASS_LEVEL(ch, CLASS_RANGER) / 5 + 2;
    }
  }
  /* These bonuses to AC apply even against touch attacks or when the monk is
   * flat-footed. She loses these bonuses when she is immobilized or helpless,
   * when she wears any armor, when she carries a shield, or when she carries
   * a medium or heavy load. */
  if (CLASS_LEVEL(ch, CLASS_MONK) && monk_gear_ok(ch)) {
    bonuses[BONUS_TYPE_UNDEFINED] += GET_WIS_BONUS(ch);

    if (CLASS_LEVEL(ch, CLASS_MONK) >= 5) {
      bonuses[BONUS_TYPE_UNDEFINED]++;
    }
    if (CLASS_LEVEL(ch, CLASS_MONK) >= 10) {
      bonuses[BONUS_TYPE_UNDEFINED]++;
    }
    if (CLASS_LEVEL(ch, CLASS_MONK) >= 15) {
      bonuses[BONUS_TYPE_UNDEFINED]++;
    }
    if (CLASS_LEVEL(ch, CLASS_MONK) >= 20) {
      bonuses[BONUS_TYPE_UNDEFINED]++;
    }
    if (CLASS_LEVEL(ch, CLASS_MONK) >= 25) {
      bonuses[BONUS_TYPE_UNDEFINED]++;
    }
    if (CLASS_LEVEL(ch, CLASS_MONK) >= 30) {
      bonuses[BONUS_TYPE_UNDEFINED]++;
    }
  }
  /**/

  /* Add up all the bonuses */
  if (is_touch) { /* don't include armor, natural armor, shield */
    for (i = 0; i < NUM_BONUS_TYPES; i++) {
      if (i == BONUS_TYPE_NATURALARMOR)
        continue;
      else if (i == BONUS_TYPE_ARMOR)
        continue;
      else if (i == BONUS_TYPE_SHIELD)
        continue;
      else
        armorclass += bonuses[i];
    }
  } else {
    for (i = 0; i < NUM_BONUS_TYPES; i++)
      armorclass += bonuses[i];
  }

  return (MIN(MAX_AC, armorclass));
}

// the whole update_pos system probably needs to be rethought -zusuk
void update_pos_dam(struct char_data *victim) {
  if (GET_HIT(victim) <= -11)
    GET_POS(victim) = POS_DEAD;
  else if (GET_HIT(victim) <= -6)
    GET_POS(victim) = POS_MORTALLYW;
  else if (GET_HIT(victim) <= -3)
    GET_POS(victim) = POS_INCAP;
  else if (GET_HIT(victim) == 0)
    GET_POS(victim) = POS_STUNNED;

  else { // hp > 0
    if (GET_POS(victim) < POS_RESTING) {
      if (!AWAKE(victim))
        send_to_char(victim, "\tRYour sleep is disturbed!!\tn  ");
      GET_POS(victim) = POS_SITTING;
      send_to_char(victim,
              "You instinctively shift from dangerous positioning to sitting...\r\n");
    }
  }
}
void update_pos(struct char_data *victim) {

  if ((GET_HIT(victim) > 0) && (GET_POS(victim) > POS_STUNNED))
    return;

  if (GET_HIT(victim) <= -11)
    GET_POS(victim) = POS_DEAD;
  else if (GET_HIT(victim) <= -6)
    GET_POS(victim) = POS_MORTALLYW;
  else if (GET_HIT(victim) <= -3)
    GET_POS(victim) = POS_INCAP;
  else if (GET_HIT(victim) == 0)
    GET_POS(victim) = POS_STUNNED;

    // hp > 0 , pos <= stunned
  else {
    GET_POS(victim) = POS_RESTING;
    send_to_char(victim,
            "You find yourself in a resting position...\r\n");
  }
}

/* if appropriate, this function will set the 'killer' flag
   a 'killer' is someone who pkilled against the current ruleset
 */
void check_killer(struct char_data *ch, struct char_data *vict) {
  if (PLR_FLAGGED(vict, PLR_KILLER) || PLR_FLAGGED(vict, PLR_THIEF))
    return;
  if (PLR_FLAGGED(ch, PLR_KILLER) || IS_NPC(ch) || IS_NPC(vict) || ch == vict)
    return;
  if (ROOM_FLAGGED(IN_ROOM(vict), ROOM_PEACEFUL)) {
    send_to_char(ch, "You will not be flagged as a killer for "
            "attempting to attack in a peaceful room...\r\n");
    return;
  }
  if (GET_LEVEL(ch) > LVL_IMMORT) {
    send_to_char(ch, "Normally you would've been flagged a "
            "PKILLER for this action...\r\n");
    return;
  }
  if (GET_LEVEL(vict) > LVL_IMMORT && !IS_NPC(vict)) {
    send_to_char(ch, "You will not be flagged as a killer for "
            "attacking an Immortal...\r\n");
    return;
  }

  SET_BIT_AR(PLR_FLAGS(ch), PLR_KILLER);
  send_to_char(ch, "If you want to be a PLAYER KILLER, so be it...\r\n");
  mudlog(BRF, LVL_IMMORT, TRUE, "PC Killer bit set on %s for "
          "initiating attack on %s at %s.",
          GET_NAME(ch), GET_NAME(vict), world[IN_ROOM(vict)].name);
}

/* a function that sets ch fighting victim */
void set_fighting(struct char_data *ch, struct char_data *vict) {
  struct char_data *current = NULL, *previous = NULL;
  int delay;

  if (ch == vict)
    return;

  if (FIGHTING(ch)) {
    core_dump();
    return;
  }

  GET_INITIATIVE(ch) = roll_initiative(ch);

  if (combat_list == NULL) {
    ch->next_fighting = combat_list;
    combat_list = ch;
  } else {
    for (current = combat_list; current != NULL; current = current->next_fighting) {
      if ((GET_INITIATIVE(ch) > GET_INITIATIVE(current)) ||
          ((GET_INITIATIVE(ch) == GET_INITIATIVE(current)) &&
          (GET_DEX_BONUS(ch) < GET_DEX_BONUS(current)))) {
        previous = current;
        continue;
      }
      break;
    }
    if (previous == NULL) {
      /* First. */
      ch->next_fighting = combat_list;
      combat_list = ch;
    } else {
      ch->next_fighting = current;
      previous->next_fighting = ch;
    }
  }

  if (AFF_FLAGGED(ch, AFF_SLEEP))
    affect_from_char(ch, SPELL_SLEEP);

  /*  The char is flat footed until they take an action,
   *  but only if they are not currently fighting.  */
  if (!FIGHTING(ch)) SET_BIT_AR(AFF_FLAGS(ch), AFF_FLAT_FOOTED);
  FIGHTING(ch) = vict;

  if (!CONFIG_PK_ALLOWED)
    check_killer(ch, vict);

  if (GET_INITIATIVE(ch) >= GET_INITIATIVE(vict))
    delay = 2 RL_SEC;
  else
    delay = 4 RL_SEC;

  //  send_to_char(ch, "DEBUG: SETTING FIGHT EVENT!\r\n");

  //  if (!char_has_mud_event(ch, eCOMBAT_ROUND))
  attach_mud_event(new_mud_event(eCOMBAT_ROUND, ch, strdup("1")), delay);
}

/* remove a char from the list of fighting chars */
void stop_fighting(struct char_data *ch) {
  struct char_data *temp;

  if (ch == next_combat_list)
    next_combat_list = ch->next_fighting;

  REMOVE_FROM_LIST(ch, combat_list, next_fighting);
  ch->next_fighting = NULL;
  FIGHTING(ch) = NULL;
  if (GET_POS(ch) > POS_SITTING)
    GET_POS(ch) = POS_STANDING;
  update_pos(ch);

  /* Reset the combat data */
  GET_TOTAL_AOO(ch) = 0;
  REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_FLAT_FOOTED);
}

/* function for creating corpses */
static void make_corpse(struct char_data *ch) {
  char buf2[MAX_NAME_LENGTH + 64] = {'\0'};
  struct obj_data *corpse = NULL, *o = NULL;
  struct obj_data *money = NULL;
  int i = 0, x = 0, y = 0;

  corpse = create_obj();

  corpse->item_number = NOTHING;
  IN_ROOM(corpse) = NOWHERE;
  corpse->name = strdup("corpse");

  snprintf(buf2, sizeof (buf2), "%sThe corpse of %s%s is lying here.",
          CCNRM(ch, C_NRM), GET_NAME(ch), CCNRM(ch, C_NRM));
  corpse->description = strdup(buf2);

  snprintf(buf2, sizeof (buf2), "%sthe corpse of %s%s", CCNRM(ch, C_NRM),
          GET_NAME(ch), CCNRM(ch, C_NRM));
  corpse->short_description = strdup(buf2);

  GET_OBJ_TYPE(corpse) = ITEM_CONTAINER;
  for (x = y = 0; x < EF_ARRAY_MAX || y < TW_ARRAY_MAX; x++, y++) {
    if (x < EF_ARRAY_MAX)
      GET_OBJ_EXTRA_AR(corpse, x) = 0;
    if (y < TW_ARRAY_MAX)
      corpse->obj_flags.wear_flags[y] = 0;
  }
  SET_BIT_AR(GET_OBJ_WEAR(corpse), ITEM_WEAR_TAKE);
  SET_BIT_AR(GET_OBJ_EXTRA(corpse), ITEM_NODONATE);
  GET_OBJ_VAL(corpse, 0) = 0; /* You can't store stuff in a corpse */
  GET_OBJ_VAL(corpse, 3) = 1; /* corpse identifier */
  GET_OBJ_WEIGHT(corpse) = GET_WEIGHT(ch) + IS_CARRYING_W(ch);
  GET_OBJ_RENT(corpse) = 100000;
  if (IS_NPC(ch))
    GET_OBJ_TIMER(corpse) = CONFIG_MAX_NPC_CORPSE_TIME;
  else
    GET_OBJ_TIMER(corpse) = CONFIG_MAX_PC_CORPSE_TIME;

  /* transfer character's inventory to the corpse */
  corpse->contains = ch->carrying;
  for (o = corpse->contains; o != NULL; o = o->next_content)
    o->in_obj = corpse;
  object_list_new_owner(corpse, NULL);

  /* transfer character's equipment to the corpse */
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i)) {
      remove_otrigger(GET_EQ(ch, i), ch);
      obj_to_obj(unequip_char(ch, i), corpse);
    }

  /* transfer gold */
  if (GET_GOLD(ch) > 0) {
    /* following 'if' clause added to fix gold duplication loophole. The above
     * line apparently refers to the old "partially log in, kill the game
     * character, then finish login sequence" duping bug. The duplication has
     * been fixed (knock on wood) but the test below shall live on, for a
     * while. -gg 3/3/2002 */
    if (IS_NPC(ch) || ch->desc) {
      money = create_money(GET_GOLD(ch));
      obj_to_obj(money, corpse);
    }
    GET_GOLD(ch) = 0;
  }
  ch->carrying = NULL;
  IS_CARRYING_N(ch) = 0;
  IS_CARRYING_W(ch) = 0;

  obj_to_room(corpse, IN_ROOM(ch));
}

/* When ch kills victim */
static void change_alignment(struct char_data *ch, struct char_data *victim) {
  if (GET_ALIGNMENT(victim) < GET_ALIGNMENT(ch) && !rand_number(0, 19)) {
    if (GET_ALIGNMENT(ch) < 1000)
      GET_ALIGNMENT(ch)++;
  } else if (GET_ALIGNMENT(victim) > GET_ALIGNMENT(ch) && !rand_number(0, 19)) {
    if (GET_ALIGNMENT(ch) > -1000)
      GET_ALIGNMENT(ch)--;
  }

  /* new alignment change algorithm: if you kill a monster with alignment A,
   * you move 1/16th of the way to having alignment -A.  Simple and fast. */
  //  GET_ALIGNMENT(ch) += (-GET_ALIGNMENT(victim) - GET_ALIGNMENT(ch)) / 16;
}

/* a function for 'audio' effect of killing, notifies neighboring
 room of a nearby death */
void death_cry(struct char_data *ch) {
  int door;

  act("Your blood freezes as you hear $n's death cry.",
          FALSE, ch, 0, 0, TO_ROOM);

  for (door = 0; door < DIR_COUNT; door++)
    if (CAN_GO(ch, door))
      send_to_room(world[IN_ROOM(ch)].dir_option[door]->to_room,
            "Your blood freezes as you hear someone's death cry.\r\n");
}

/* this message is a replacement in our new (temporary?) death system */
void death_message(struct char_data *ch) {
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\tD'||''|.   '||''''|      |     |''||''| '||'  '||' \r\n");
  send_to_char(ch, " ||   ||   ||  .       |||       ||     ||    ||  \r\n");
  send_to_char(ch, " ||    ||  ||''|      |  ||      ||     ||''''||  \r\n");
  send_to_char(ch, " ||    ||  ||        .''''|.     ||     ||    ||  \r\n");
  send_to_char(ch, ".||...|'  .||.....| .|.  .||.   .||.   .||.  .||. \r\n\tn");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "You awaken... you realize someone has resurrected you...\r\n");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "\r\n");
}

/* Added quest completion for all group members if they are in the room.
 * Oct 6, 2014 - Ornir. */
void kill_quest_completion_check(struct char_data *killer, struct char_data *ch) {
  struct group_data *group = NULL;
  struct char_data *k = NULL;
  struct iterator_data it;

  /* dummy checks */
  if (!killer)
    return;
  if (!ch)
    return;

  /* check for killer first */
  autoquest_trigger_check(killer, ch, NULL, AQ_MOB_KILL);

  /* check for all group members next */
  group = GROUP(killer);

  if (group != NULL) {
    /* Initialize the iterator */

    for (k = (struct char_data *)merge_iterator(&it, group->members); k != NULL; k = (struct char_data *)next_in_list(&it)) {

      if (k == killer) /* should not need this */
        continue;
      if (IS_PET(k))
        continue;
      if (IN_ROOM(k) == IN_ROOM(killer))
        autoquest_trigger_check(k, ch, NULL, AQ_MOB_KILL);
    }
    /* Be kind, rewind. */
    remove_iterator(&it);
  }
}

// we're not extracting anybody anymore, just penalize them xp
// and move them back to the starting room -zusuk
/* we will consider changing this back to corpse creation and dump
   but only on the condition of corpse-saving code */
void raw_kill(struct char_data *ch, struct char_data *killer) {
  struct char_data *k, *temp;

  //stop relevant fighting
  if (FIGHTING(ch))
    stop_fighting(ch);
  for (k = combat_list; k; k = temp) {
    temp = k->next_fighting;
    if (FIGHTING(k) == ch)
      stop_fighting(k);
  }

  /* Wipe character from the memory of hunters and other intelligent NPCs... */
  for (temp = character_list; temp; temp = temp->next) {
    /* PCs can't use MEMORY, and don't use HUNTING() */
    if (!IS_NPC(temp))
      continue;
    /* If "temp" is hunting our extracted char, stop the hunt. */
    if (HUNTING(temp) == ch)
      HUNTING(temp) = NULL;
    /* If "temp" has allocated memory data and our ch is a PC, forget the
     * extracted character (if he/she is remembered) */
    if (!IS_NPC(ch) && MEMORY(temp))
      forget(temp, ch); /* forget() is safe to use without a check. */
  }

  if (ch->followers || ch->master) // handle followers
    die_follower(ch);

  if (GROUP(ch)) {
    send_to_group(ch, GROUP(ch), "%s has died.\r\n", GET_NAME(ch));
    leave_group(ch);
  }

  while (ch->affected) //remove affects
    affect_remove(ch, ch->affected);

  // ordinary commands work in scripts -welcor
  GET_POS(ch) = POS_STANDING;
  if (killer) {
    if (death_mtrigger(ch, killer))
      death_cry(ch);
  } else
    death_cry(ch);

  /* make sure group gets credit for kill if ch involved in quest */
  kill_quest_completion_check(killer, ch);

  update_pos(ch);

  //this replaces extraction
  char_from_room(ch);
  death_message(ch);
  GET_HIT(ch) = 1;
  update_pos(ch);

  /* spec-abil saves on exit, so make sure this does not save */
  DOOM(ch) = 0;
  INCENDIARY(ch) = 0;

  /* move char to starting room */
  char_to_room(ch, r_mortal_start_room);
  act("$n appears in the middle of the room.", TRUE, ch, 0, 0, TO_ROOM);
  look_at_room(ch, 0);
  entry_memory_mtrigger(ch);
  greet_mtrigger(ch, -1);
  greet_memory_mtrigger(ch);

  /* this was commented out for some reason, undid that to make sure
     events clear on death */
  clear_char_event_list(ch);

  save_char(ch, 0);
  Crash_delete_crashfile(ch);
  //end extraction replacement

  if (killer) {
    autoquest_trigger_check(killer, NULL, NULL, AQ_MOB_SAVE);
    autoquest_trigger_check(killer, NULL, NULL, AQ_ROOM_CLEAR);
  }

  /* "punishment" for death */
  start_action_cooldown(ch, atSTANDARD, 12 RL_SEC);
}

/* this is circle/tba stock raw_kill code
   called after striking the mortal blow to ch via instant kill
 * like staff 'kill' command or the die() function */
void raw_kill_old(struct char_data *ch, struct char_data *killer) {
  if (FIGHTING(ch))
    stop_fighting(ch);

  while (ch->affected)
    affect_remove(ch, ch->affected);

  GET_POS(ch) = POS_STANDING;

  if (killer) {
    if (death_mtrigger(ch, killer))
      death_cry(ch);
  } else
    death_cry(ch);

  /* make sure group gets credit for kill if ch involved in quest */
  kill_quest_completion_check(killer, ch);

  update_pos(ch);

  /* random treasure drop */
  if (killer && ch)
    determine_treasure(find_treasure_recipient(killer), ch);

  make_corpse(ch);

  /* this was commented out for some reason, undid that to make sure
     events clear on death */
  clear_char_event_list(ch);

  /* spec-abil saves on exit, so make sure this does not save */
  DOOM(ch) = 0;
  INCENDIARY(ch) = 0;

  /* extraction!  *SLURRRRRRRRRRRRRP* */
  extract_char(ch);

  if (killer) {
    autoquest_trigger_check(killer, NULL, NULL, AQ_MOB_SAVE);
    autoquest_trigger_check(killer, NULL, NULL, AQ_ROOM_CLEAR);
  }
}

/* called after striking the mortal blow to ch */
void die(struct char_data *ch, struct char_data *killer) {
  /*
  if (GET_LEVEL(ch) <= 6) {
    // no xp loss for newbs - Bakarus
  } else {
    // if not a newbie then bang that xp! - Bakarus
    gain_exp(ch, -(GET_EXP(ch) / 2));
  }
  */
  
  if (!IS_NPC(ch)) {
    REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_KILLER);
    REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_THIEF);
    REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_SALVATION);
    if (AFF_FLAGGED(ch, AFF_SPELLBATTLE))
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_SPELLBATTLE);
    SPELLBATTLE(ch) = 0;
  }

  if (!IS_NPC(ch))
    raw_kill(ch, killer);
  else
    raw_kill_old(ch, killer);
}

/* called for splitting xp in a group (engine) */
static void perform_group_gain(struct char_data *ch, int base,
        struct char_data *victim) {
  int share, hap_share;

  share = MIN(CONFIG_MAX_EXP_GAIN, MAX(1, base));

  if ((IS_HAPPYHOUR) && (IS_HAPPYEXP)) {
    /* This only reports the correct amount - the calc is done in gain_exp */
    hap_share = share + (int) ((float) share * ((float) HAPPY_EXP / (float) (100)));
    share = MIN(CONFIG_MAX_EXP_GAIN, MAX(1, hap_share));
  }
  if (share > 1)
    send_to_char(ch, "You receive your share of experience -- %d points.\r\n",
          gain_exp(ch, share));
  else {
    send_to_char(ch, "You receive your share of experience -- "
            "one measly little point!\r\n");
    gain_exp(ch, share);
  }

  change_alignment(ch, victim);
}

/* called for splitting xp in a group (prelim) */
static void group_gain(struct char_data *ch, struct char_data *victim) {
  int tot_members = 0, base, tot_gain;
  struct char_data *k;

  while ((k = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL) {
    if (IS_PET(k))
      continue;
    if (IN_ROOM(ch) == IN_ROOM(k))
      tot_members++;
  }

  /* round up to the nearest tot_members */
  tot_gain = (GET_EXP(victim) / 3) + tot_members - 1;

  /* prevent illegal xp creation when killing players */
  if (!IS_NPC(victim))
    tot_gain = MIN(CONFIG_MAX_EXP_LOSS * 2 / 3, tot_gain);

  if (tot_members >= 1)
    base = MAX(1, tot_gain / tot_members);
  else
    base = 0;

  while ((k = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL) {
    if (IS_PET(k))
      continue;
    if (IN_ROOM(k) == IN_ROOM(ch))
      perform_group_gain(k, base, victim);
  }
}

/* called for splitting xp if NOT in a group (engine) */
static void solo_gain(struct char_data *ch, struct char_data *victim) {
  int exp, happy_exp;

  exp = MIN(CONFIG_MAX_EXP_GAIN, GET_EXP(victim) / 3);

  /* Calculate level-difference bonus */
  if (GET_LEVEL(victim) < GET_LEVEL(ch)) {
    if (IS_NPC(ch))
      exp += MAX(0, (exp * MIN(4, (GET_LEVEL(victim) - GET_LEVEL(ch)))) / 8);
    else
      exp += MAX(0, (exp * MIN(8, (GET_LEVEL(victim) - GET_LEVEL(ch)))) / 8);
  }

  exp = MAX(exp, 1);

  /* if mob isn't within 3 levels, don't give xp -zusuk */
  if ((GET_LEVEL(victim) + 3) < GET_LEVEL(ch))
    exp = 1;

  /* avoid xp abuse in PvP */
  if (!IS_NPC(victim))
    exp = MIN(CONFIG_MAX_EXP_LOSS * 2 / 3, exp);

  if (IS_HAPPYHOUR && IS_HAPPYEXP) {
    happy_exp = exp + (int) ((float) exp * ((float) HAPPY_EXP / (float) (100)));
    exp = MAX(happy_exp, 1);
  }

  if (exp > 1)
    send_to_char(ch, "You receive %d experience points.\r\n", gain_exp(ch, exp));
  else {
    send_to_char(ch, "You receive one lousy experience point.\r\n");
    gain_exp(ch, exp);
  }

  change_alignment(ch, victim);

}

/* this function replaces the #w or #W with an appropriate weapon
   constant dependent on plural or not */
static char *replace_string(const char *str, const char *weapon_singular,
        const char *weapon_plural) {
  static char buf[MEDIUM_STRING];
  char *cp = buf;

  for (; *str; str++) {
    if (*str == '#') {
      switch (*(++str)) {
        case 'W':
          for (; *weapon_plural; *(cp++) = *(weapon_plural++));
          break;
        case 'w':
          for (; *weapon_singular; *(cp++) = *(weapon_singular++));
          break;
        default:
          *(cp++) = '#';
          break;
      }
    } else
      *(cp++) = *str;

    *cp = 0;
  } /* For */

  return (buf);
}

/* message for doing damage with a weapon */
static void dam_message(int dam, struct char_data *ch, struct char_data *victim,
        int w_type, int offhand) {
  char *buf = NULL;
  int msgnum = -1, hp = 0, pct = 0;
  bool is_ranged = FALSE;

  hp = GET_HIT(victim);
  if (GET_HIT(victim) < 1)
    hp = 1;

  pct = 100 * dam / hp;

  if (dam && pct <= 0)
    pct = 1;

  static struct dam_weapon_type {
    const char *to_room;
    const char *to_char;
    const char *to_victim;
  } dam_weapons[] = {

    /* use #w for singular (i.e. "slash") and #W for plural (i.e. "slashes") */

    {
      "\tn$n tries to #w \tn$N, but misses.\tn", /* 0: 0     */
      "You try to #w \tn$N, but miss.\tn",
      "\tn$n tries to #w you, but misses.\tn"
    },

    {
      "\tn$n \tYbarely grazes \tn$N \tYas $e #W $M.\tn", /* 1: dam <= 2% */
      "\tMYou barely graze \tn$N \tMas you #w $M.\tn",
      "\tn$n \tRbarely grazes you as $e #W you.\tn"
    },

    {
      "\tn$n \tYnicks \tn$N \tYas $e #W $M.", /* 2: dam <= 4% */
      "\tMYou nick \tn$N \tMas you #w $M.",
      "\tn$n \tRnicks you as $e #W you.\tn"
    },

    {
      "\tn$n \tYbarely #W \tn$N\tY.\tn", /* 3: dam <= 6%  */
      "\tMYou barely #w \tn$N\tM.\tn",
      "\tn$n \tRbarely #W you.\tn"
    },

    {
      "\tn$n \tY#W \tn$N\tY.\tn", /* 4: dam <= 8%  */
      "\tMYou #w \tn$N\tM.\tn",
      "\tn$n \tR#W you.\tn"
    },

    {
      "\tn$n \tY#W \tn$N \tYhard.\tn", /* 5: dam <= 11% */
      "\tMYou #w \tn$N \tMhard.\tn",
      "\tn$n \tR#W you hard.\tn"
    },

    {
      "\tn$n \tY#W \tn$N \tYvery hard.\tn", /* 6: dam <= 14%  */
      "\tMYou #w \tn$N \tMvery hard.\tn",
      "\tn$n \tR#W you very hard.\tn"
    },

    {
      "\tn$n \tY#W \tn$N \tYextremely hard.\tn", /* 7: dam <= 18%  */
      "\tMYou #w \tn$N \tMextremely hard.\tn",
      "\tn$n \tR#W you extremely hard.\tn"
    },

    {
      "\tn$n \tYinjures \tn$N \tYwith $s #w.\tn", /* 8: dam <= 22%  */
      "\tMYou injure \tn$N \tMwith your #w.\tn",
      "\tn$n \tRinjures you with $s #w.\tn"
    },

    {
      "\tn$n \tYwounds \tn$N \tYwith $s #w.\tn", /* 9: dam <= 27% */
      "\tMYou wound \tn$N \tMwith your #w.\tn",
      "\tn$n \tRwounds you with $s #w.\tn"
    },

    {
      "\tn$n \tYinjures \tn$N \tYharshly with $s #w.\tn", /* 10: dam <= 32%  */
      "\tMYou injure \tn$N \tMharshly with your #w.\tn",
      "\tn$n \tRinjures you harshly with $s #w.\tn"
    },

    {
      "\tn$n \tYseverely wounds \tn$N \tYwith $s #w.\tn", /* 11: dam <= 40% */
      "\tMYou severely wound \tn$N \tMwith your #w.\tn",
      "\tn$n \tRseverely wounds you with $s #w.\tn"
    },

    {
      "\tn$n \tYinflicts grave damage on \tn$N\tY with $s #w.\tn", /* 12: dam <= 50% */
      "\tMYou inflict grave damage on \tn$N \tMwith your #w.\tn",
      "\tn$n \tRinflicts grave damage on you with $s #w.\tn"
    },

    {
      "\tn$n \tYnearly kills \tn$N\tY with $s deadly #w!!\tn", /* (13): > 51   */
      "\tMYou nearly kill \tn$N \tMwith your deadly #w!!\tn",
      "\tn$n \tRnearly kills you with $s deadly #w!!\tn"
    }
  };

  w_type -= TYPE_HIT; /* Change to base of table with text */

  if (pct == 0) msgnum = 0;
  else if (pct <= 2) msgnum = 1;
  else if (pct <= 4) msgnum = 2;
  else if (pct <= 6) msgnum = 3;
  else if (pct <= 8) msgnum = 4;
  else if (pct <= 11) msgnum = 5;
  else if (pct <= 14) msgnum = 6;
  else if (pct <= 18) msgnum = 7;
  else if (pct <= 22) msgnum = 8;
  else if (pct <= 27) msgnum = 9;
  else if (pct <= 32) msgnum = 10;
  else if (pct <= 40) msgnum = 11;
  else if (pct <= 50) msgnum = 12;
  else msgnum = 13;

  if (offhand == 2 && last_missile) { // ranged
    send_to_char(ch, "WHIZZ, you fire %s:  ", last_missile->short_description);
    act("WHIZZ, $n fires $p at $N!", FALSE, ch, last_missile, victim, TO_NOTVICT);
    act("WHIZZ, $n fires $p!", FALSE, ch, last_missile, victim, TO_VICT | TO_SLEEP);
    is_ranged = TRUE;
  }

  /* damage message to onlookers */
  // note, we may have to add more info if we have some way to attack
  // someone that isn't in your room - zusuk
  buf = replace_string(dam_weapons[msgnum].to_room,
            attack_hit_text[w_type].singular, attack_hit_text[w_type].plural), dam;
  act(buf, FALSE, ch, NULL, victim, TO_NOTVICT);

  /* damage message to damager */
  buf = replace_string(dam_weapons[msgnum].to_char,
            attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  act(buf, FALSE, ch, NULL, victim, TO_CHAR);
  send_to_char(ch, CCNRM(ch, C_CMP));

  /* damage message to damagee */
  buf = replace_string(dam_weapons[msgnum].to_victim,
          attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  act(buf, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
  send_to_char(victim, CCNRM(victim, C_CMP));
}


/*  message for doing damage with a spell or skill. Also used for weapon
 *  damage on miss and death blows. */

/* took out attacking-staff-messages -zusuk*/
/* this is so trelux's natural attack reflects an actual object */
#define TRELUX_CLAWS 800

int skill_message(int dam, struct char_data *ch, struct char_data *vict,
        int attacktype, int dualing) {
  int i, j, nr;
  struct message_type *msg;

  struct obj_data *weap = GET_EQ(ch, WEAR_WIELD_1);

  if (GET_EQ(ch, WEAR_WIELD_2H))
    weap = GET_EQ(ch, WEAR_WIELD_2H);
  else if (GET_RACE(ch) == RACE_TRELUX)
    weap = read_object(TRELUX_CLAWS, VIRTUAL);
  else if (dualing)
    weap = GET_EQ(ch, WEAR_WIELD_2);

  /* These attacks use a shield as a weapon. */
  if ((attacktype == SKILL_SHIELD_PUNCH)  ||
      (attacktype == SKILL_SHIELD_CHARGE) ||
      (attacktype == SKILL_SHIELD_SLAM))
    weap = GET_EQ(ch, WEAR_SHIELD);

  for (i = 0; i < MAX_MESSAGES; i++) {
    if (fight_messages[i].a_type == attacktype) {
      nr = dice(1, fight_messages[i].number_of_attacks);
      for (j = 1, msg = fight_messages[i].msg; (j < nr) && msg; j++)
        msg = msg->next;

      /* old location of staff-messages */
      if (dam != 0) {
        if (GET_POS(vict) == POS_DEAD) { // death messages
          /* Don't send redundant color codes for TYPE_SUFFERING & other types
           * of damage without attacker_msg. */
          if (msg->die_msg.attacker_msg) {
            send_to_char(ch, CCYEL(ch, C_CMP));
            act(msg->die_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
            send_to_char(ch, CCNRM(ch, C_CMP));
          }
          send_to_char(vict, CCRED(vict, C_CMP));
          act(msg->die_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
          send_to_char(vict, CCNRM(vict, C_CMP));
          act(msg->die_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
        } else { // not dead
          if (msg->hit_msg.attacker_msg) {
            send_to_char(ch, CCYEL(ch, C_CMP));
            act(msg->hit_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
            send_to_char(ch, CCNRM(ch, C_CMP));
          }
          send_to_char(vict, CCRED(vict, C_CMP));
          act(msg->hit_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
          send_to_char(vict, CCNRM(vict, C_CMP));
          act(msg->hit_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
        }
      } else if (ch != vict) { /* Dam == 0 */
        if (msg->miss_msg.attacker_msg) {
          send_to_char(ch, CCYEL(ch, C_CMP));
          act(msg->miss_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
          send_to_char(ch, CCNRM(ch, C_CMP));
        }
        send_to_char(vict, CCRED(vict, C_CMP));
        act(msg->miss_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
        send_to_char(vict, CCNRM(vict, C_CMP));
        act(msg->miss_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
      }
      return (1);
    }
  }
  return (0);
}
#undef TRELUX_CLAWS


// this is just like damage reduction, except applies to certain type
int compute_energy_absorb(struct char_data *ch, int dam_type) {
  int dam_reduction = 0;

  /* universal bonuses */
  switch (dam_type) {
    case DAM_FIRE:
      if (affected_by_spell(ch, SPELL_RESIST_ENERGY))
        dam_reduction += 3;
      break;
    case DAM_COLD:
      if (affected_by_spell(ch, SPELL_RESIST_ENERGY))
        dam_reduction += 3;
      break;
    case DAM_AIR:
      if (affected_by_spell(ch, SPELL_RESIST_ENERGY))
        dam_reduction += 3;
      break;
    case DAM_EARTH:
      if (affected_by_spell(ch, SPELL_RESIST_ENERGY))
        dam_reduction += 3;
      break;
    case DAM_ACID:
      if (affected_by_spell(ch, SPELL_RESIST_ENERGY))
        dam_reduction += 3;
      break;
    case DAM_HOLY:
      break;
    case DAM_ELECTRIC:
      if (affected_by_spell(ch, SPELL_RESIST_ENERGY))
        dam_reduction += 3;
      break;
    case DAM_UNHOLY:
      break;
    case DAM_SLICE:
      break;
    case DAM_PUNCTURE:
      break;
    case DAM_FORCE:
      break;
    case DAM_SOUND:
      break;
    case DAM_POISON:
      break;
    case DAM_DISEASE:
      break;
    case DAM_NEGATIVE:
      break;
    case DAM_ILLUSION:
      break;
    case DAM_MENTAL:
      break;
    case DAM_LIGHT:
      break;
    case DAM_ENERGY:
      if (affected_by_spell(ch, SPELL_RESIST_ENERGY))
        dam_reduction += 3;
      break;
    default: break;
  }

  return (MIN(MAX_ENERGY_ABSORB, dam_reduction));
}


// can return negative values, which indicates vulnerability
// dam_ defines are in spells.h
int compute_damtype_reduction(struct char_data *ch, int dam_type) {
  int damtype_reduction = 0;

  /* universal bonsues */
  damtype_reduction += GET_RESISTANCES(ch, dam_type);

  switch (dam_type) {
    case DAM_FIRE:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TROLL)
        damtype_reduction += -50;
      if (affected_by_spell(ch, SPELL_ENDURE_ELEMENTS))
        damtype_reduction += 10;
      if (affected_by_spell(ch, SPELL_COLD_SHIELD))
        damtype_reduction += 50;
      if (GET_NPC_RACE(ch) == NPCRACE_ELEMENTAL &&
              HAS_SUBRACE(ch, SUBRACE_FIRE))
        damtype_reduction += 100;
      break;
    case DAM_COLD:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += -20;
      if (affected_by_spell(ch, SPELL_ENDURE_ELEMENTS))
        damtype_reduction += 10;
      if (affected_by_spell(ch, SPELL_FIRE_SHIELD))
        damtype_reduction += 50;
      if (GET_NPC_RACE(ch) == NPCRACE_ELEMENTAL &&
              HAS_SUBRACE(ch, SUBRACE_FIRE))
        damtype_reduction -= 100;
      break;
    case DAM_AIR:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (affected_by_spell(ch, SPELL_ENDURE_ELEMENTS))
        damtype_reduction += 10;
      if (GET_NPC_RACE(ch) == NPCRACE_ELEMENTAL &&
              HAS_SUBRACE(ch, SUBRACE_AIR))
        damtype_reduction += 100;
      break;
    case DAM_EARTH:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (affected_by_spell(ch, SPELL_ENDURE_ELEMENTS))
        damtype_reduction += 10;
      if (affected_by_spell(ch, SPELL_ACID_SHEATH))
        damtype_reduction += 50;
      if (GET_NPC_RACE(ch) == NPCRACE_ELEMENTAL &&
              HAS_SUBRACE(ch, SUBRACE_EARTH))
        damtype_reduction += 100;
      break;
    case DAM_ACID:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TROLL)
        damtype_reduction += -25;
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_CRYSTAL_DWARF)
        damtype_reduction += 10;
      if (affected_by_spell(ch, SPELL_ENDURE_ELEMENTS))
        damtype_reduction += 10;
      if (GET_NPC_RACE(ch) == NPCRACE_ELEMENTAL &&
              HAS_SUBRACE(ch, SUBRACE_EARTH))
        damtype_reduction += 50;
      break;
    case DAM_HOLY:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      break;
    case DAM_ELECTRIC:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (affected_by_spell(ch, SPELL_ENDURE_ELEMENTS))
        damtype_reduction += 10;
      if (GET_NPC_RACE(ch) == NPCRACE_ELEMENTAL &&
              HAS_SUBRACE(ch, SUBRACE_WATER))
        damtype_reduction -= 100;
      break;
    case DAM_UNHOLY:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      break;
    case DAM_SLICE:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      break;
    case DAM_PUNCTURE:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_CRYSTAL_DWARF)
        damtype_reduction += 10;
      break;
    case DAM_FORCE:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      break;
    case DAM_SOUND:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      break;
    case DAM_POISON:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_CRYSTAL_DWARF)
        damtype_reduction += 10;
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TROLL)
        damtype_reduction += 25;
      break;
    case DAM_DISEASE:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_CRYSTAL_DWARF)
        damtype_reduction += 10;
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TROLL)
        damtype_reduction += 50;
      break;
    case DAM_NEGATIVE:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_TIMELESS_BODY))
        damtype_reduction += 25;
//      if (AFF_FLAGGED(ch, AFF_SHADOW_SHIELD))
//        damtype_reduction += 100;
      break;
    case DAM_ILLUSION:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      break;
    case DAM_MENTAL:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      break;
    case DAM_LIGHT:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      break;
    case DAM_ENERGY:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (affected_by_spell(ch, SPELL_ENDURE_ELEMENTS))
        damtype_reduction += 10;
      break;
    case DAM_WATER:
      if (!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX)
        damtype_reduction += 20;
      if (affected_by_spell(ch, SPELL_ENDURE_ELEMENTS))
        damtype_reduction += 10;
      if (GET_NPC_RACE(ch) == NPCRACE_ELEMENTAL &&
              HAS_SUBRACE(ch, SUBRACE_WATER))
        damtype_reduction += 100;
      break;

    default: break;
  }

  return damtype_reduction; //no cap as of yet
}

/* this is straight damage reduction, applies to ALL attacks
 (not melee exclusive damage reduction) */
int compute_damage_reduction(struct char_data *ch, int dam_type) {
  int damage_reduction = 0;

  if (char_has_mud_event(ch, eCRYSTALBODY))
    damage_reduction += 3;
//  if (CLASS_LEVEL(ch, CLASS_BERSERKER))
//    damage_reduction += CLASS_LEVEL(ch, CLASS_BERSERKER) / 4;
//  if (AFF_FLAGGED(ch, AFF_SHADOW_SHIELD))
//    damage_reduction += 12;
  if (HAS_FEAT(ch, FEAT_PERFECT_SELF)) /* temporary mechanic until we upgrade this system */
    damage_reduction += 3;
  if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_RP_HEAVY_SHRUG) && affected_by_spell(ch, SKILL_RAGE))
    damage_reduction += 3;

  //damage reduction cap is 20
  return (MIN(MAX_DAM_REDUC, damage_reduction));
}

/* this is straight avoidance percentage, applies to ALL attacks
 (not exclusive to just melee attacks) */
int compute_concealment(struct char_data *ch) {
  int concealment = 0;

  if (!IS_NPC(ch) && GET_SKILL(ch, SKILL_SELF_CONCEAL_1))
    concealment += 10;
  if (!IS_NPC(ch) && GET_SKILL(ch, SKILL_SELF_CONCEAL_2))
    concealment += 10;
  if (!IS_NPC(ch) && GET_SKILL(ch, SKILL_SELF_CONCEAL_3))
    concealment += 10;
  if (AFF_FLAGGED(ch, AFF_BLUR))
    concealment += 20;
  if (ROOM_AFFECTED(IN_ROOM(ch), RAFF_OBSCURING_MIST))
    concealment += 20;
  if (AFF_FLAGGED(ch, AFF_DISPLACE))
    concealment += 50;

  // concealment cap is 50%
  return (MIN(MAX_CONCEAL, concealment));

}

/* this function lets damage_handling know that the given attacktype
   is VALID for being handled, otherwise ignore this attack */
bool ok_damage_handling(int attacktype) {
  switch (attacktype) {
    case TYPE_SUFFERING:
      return FALSE;
    case SKILL_BASH:
      return FALSE;
    case SKILL_TRIP:
      return FALSE;
    case SPELL_POISON:
      return FALSE;
    case SPELL_SPIKE_GROWTH:
      return FALSE;
    case SKILL_CHARGE:
      return FALSE;
    case SKILL_BODYSLAM:
      return FALSE;
    case SKILL_SPRINGLEAP:
      return FALSE;
    case SKILL_SHIELD_PUNCH:
      return FALSE;
    case SKILL_SHIELD_CHARGE:
      return FALSE;
    case SKILL_SHIELD_SLAM:
      return FALSE;
    case SKILL_DIRT_KICK:
      return FALSE;
    case SKILL_SAP:
      return FALSE;
  }
  return TRUE;
}

/* returns modified damage, process elements/resistance/avoidance
   -1 means we're gonna go ahead and exit damage()
   anything that goes through here will affect ALL damage, whether
   skill or spell, etc */
int damage_handling(struct char_data *ch, struct char_data *victim,
        int dam, int attacktype, int dam_type) {

  if (dam > 0 && ok_damage_handling(attacktype) && victim != ch) {
    int concealment = compute_concealment(victim);
    if (dice(1, 100) <= compute_concealment(victim)) {
      send_to_char(victim, "\tW<conceal:%d>\tn", concealment);
      send_to_char(ch, "\tR<oconceal:%d>\tn", concealment);
      return 0;
    }

    if (GET_RACE(victim) == RACE_TRELUX && !rand_number(0, 4)) {
      send_to_char(victim, "\tWYou leap away avoiding the attack!\tn\r\n");
      send_to_char(ch, "\tRYou fail to cause %s any harm as he leaps away!\tn\r\n",
              GET_NAME(victim));
      act("$n fails to harm $N as $S leaps away!", FALSE, ch, 0, victim,
              TO_NOTVICT);
      return -1;
    }

    /* mirror image gives (1 / (# of image + 1)) chance of hitting */
    if ((affected_by_spell(victim, SPELL_MIRROR_IMAGE) ||
            affected_by_spell(victim, SPELL_GREATER_MIRROR_IMAGE)) && dam > 0) {
      if (GET_IMAGES(victim) > 0) {
        if (rand_number(0, GET_IMAGES(victim))) {
          send_to_char(victim, "\tWOne of your images is destroyed!\tn\r\n");
          send_to_char(ch, "\tRYou have struck an illusionary "
                  "image of %s!\tn\r\n",
                  GET_NAME(victim));
          act("$n struck an illusionary image of $N!", FALSE, ch, 0, victim,
                  TO_NOTVICT);
          GET_IMAGES(victim)--;
          if (GET_IMAGES(victim) <= 0) {
            send_to_char(victim, "\t2All of your illusionary "
                    "images are gone!\tn\r\n");
            affect_from_char(victim, SPELL_MIRROR_IMAGE);
            affect_from_char(victim, SPELL_GREATER_MIRROR_IMAGE);

          }
          return -1;
        }
      } else {
        //dummy check
        send_to_char(victim, "\t2All of your illusionary "
                "images are gone!\tn\r\n");
        affect_from_char(victim, SPELL_MIRROR_IMAGE);
        affect_from_char(victim, SPELL_GREATER_MIRROR_IMAGE);
      }
    }

    int damage_reduction = compute_energy_absorb(ch, dam_type);
    dam -= compute_energy_absorb(ch, dam_type);
    if (dam <= 0 && (ch != victim)) {
      send_to_char(victim, "\tWYou absorb all the damage!\tn\r\n");
      send_to_char(ch, "\tRYou fail to cause %s any harm!\tn\r\n",
              GET_NAME(victim));
      act("$n fails to do any harm to $N!", FALSE, ch, 0, victim,
              TO_NOTVICT);
      return -1;
    } else if (damage_reduction) {
      send_to_char(victim, "\tW<EA:%d>\tn", damage_reduction);
      send_to_char(ch, "\tR<oEA:%d>\tn", damage_reduction);
    }

    float damtype_reduction = (float) compute_damtype_reduction(victim, dam_type);
    damtype_reduction = (((float) (damtype_reduction / 100)) * dam);
    dam -= damtype_reduction;
    if (dam <= 0 && (ch != victim)) {
      send_to_char(victim, "\tWYou absorb all the damage!\tn\r\n");
      send_to_char(ch, "\tRYou fail to cause %s any harm!\tn\r\n",
              GET_NAME(victim));
      act("$n fails to do any harm to $N!", FALSE, ch, 0, victim,
              TO_NOTVICT);
      return -1;
    } else if (damtype_reduction < 0) { // no reduction, vulnerability
      send_to_char(victim, "\tR<TR:%d>\tn", (int) damtype_reduction);
      send_to_char(ch, "\tW<oTR:%d>\tn", (int) damtype_reduction);
    } else if (damtype_reduction > 0) {
      send_to_char(victim, "\tW<TR:%d>\tn", (int) damtype_reduction);
      send_to_char(ch, "\tR<oTR:%d>\tn", (int) damtype_reduction);
    }

    damage_reduction = compute_damage_reduction(victim, dam_type);

    dam -= MIN(dam, damage_reduction);
    if (!dam && (ch != victim)) {
      send_to_char(victim, "\tWYou absorb all the damage!\tn\r\n");
      send_to_char(ch, "\tRYou fail to cause %s any harm!\tn\r\n",
              GET_NAME(victim));
      act("$n fails to do any harm to $N!", FALSE, ch, 0, victim,
              TO_NOTVICT);
      return -1;
    } else if (damage_reduction) {
      send_to_char(victim, "\tW<DR:%d>\tn", damage_reduction);
      send_to_char(ch, "\tR<oDR:%d>\tn", damage_reduction);
    }

    /* inertial barrier - damage absorption using mana */
    if (AFF_FLAGGED(victim, AFF_INERTIAL_BARRIER) && dam && !rand_number(0,1)) {
      send_to_char(ch, "\twYour attack is absorbed by some manner of "
              "invisible barrier.\tn\r\n");
      GET_MANA(victim) -= (1 + (dam / 5));
      if (GET_MANA(victim) <= 0) {
        //affect_from_char(victim, SPELL_INERTIAL_BARRIER);
        REMOVE_BIT_AR(AFF_FLAGS(victim), AFF_INERTIAL_BARRIER);
        send_to_char(victim, "Your mind can not maintain the barrier "
                "anymore.\r\n");
      }
      dam = 0;
    }

    /* Cut damage in half if victim has sanct, to a minimum 1 */
    /* Zusuk - Too imbalancing for clerics, also definitely not according to SRD */
    /*
    if (AFF_FLAGGED(victim, AFF_SANCTUARY) && dam >= 2) {
      dam /= 2;
      send_to_char(victim, "\tW<sanc:%d>\tn", dam);
      send_to_char(ch, "\tR<oSanc:%d>\tn", dam);
    }
    */
  }

  /* this spell is also supposed to injure victims who fail reflex save
  if (attacktype == SPELL_SPIKE_GROWTH) {

  }
   */
  return dam;
}

/* victim died at the hands of ch
 * this is called before die() to handle xp gain, corpse, memory and
 * a handful of other things */
int dam_killed_vict(struct char_data *ch, struct char_data *victim) {
  char local_buf[MEDIUM_STRING] = {'\0'};
  long local_gold = 0, happy_gold = 0;
  struct char_data *tmp_char;
  struct obj_data *corpse_obj;

  GET_POS(victim) = POS_DEAD;

  if (ch != victim && (IS_NPC(victim) || victim->desc)) { //xp gain
    /* pets give xp to their master */
    if (IS_PET(ch)) {
      if (GROUP(ch))
        group_gain(ch->master, victim);
      else
        solo_gain(ch->master, victim);
    } else {
      if (GROUP(ch))
        group_gain(ch, victim);
      else
        solo_gain(ch, victim);
    }
  }

  resetCastingData(victim); //stop casting
  CLOUDKILL(victim) = 0; //stop any cloudkill bursts
  DOOM(victim) = 0; // stop any creeping doom
  INCENDIARY(victim) = 0; //stop any incendiary bursts

  if (!IS_NPC(victim)) { //forget victim, log
    mudlog(BRF, LVL_IMMORT, TRUE, "%s killed by %s at %s", GET_NAME(victim),
            GET_NAME(ch), world[IN_ROOM(victim)].name);
    if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_MEMORY))
      forget(ch, victim);
    if (IS_NPC(ch) && HUNTING(ch) == victim)
      HUNTING(ch) = NULL;
  }

  if (IS_NPC(victim)) { // determine gold before corpse created
    if ((IS_HAPPYHOUR) && (IS_HAPPYGOLD)) {
      happy_gold = (long) (GET_GOLD(victim) * (((float) (HAPPY_GOLD)) / (float) 100));
      happy_gold = MAX(0, happy_gold);
      increase_gold(victim, happy_gold);
    }
    local_gold = GET_GOLD(victim);
    sprintf(local_buf, "%ld", (long) local_gold);
  }

  /* corpse should be made here    */
  die(victim, ch);

  //handle dead mob and PRF_
  if (!IS_NPC(ch) && GROUP(ch) && (local_gold > 0) && PRF_FLAGGED(ch, PRF_AUTOSPLIT)) {
    generic_find("corpse", FIND_OBJ_ROOM, ch, &tmp_char, &corpse_obj);
    if (corpse_obj) {
      do_get(ch, "all.coin corpse", 0, 0);
      do_split(ch, local_buf, 0, 0);
    }
  } else if (!IS_NPC(ch) && (ch != victim) && PRF_FLAGGED(ch, PRF_AUTOGOLD))
    do_get(ch, "all.coin corpse", 0, 0);
  if (!IS_NPC(ch) && (ch != victim) && PRF_FLAGGED(ch, PRF_AUTOLOOT))
    do_get(ch, "all corpse", 0, 0);
  if (IS_NPC(victim) && !IS_NPC(ch) && PRF_FLAGGED(ch, PRF_AUTOSAC))
    do_sac(ch, "corpse", 0, 0);

  return (-1);
}


// death < 0, no dam = 0, damage done > 0
/* ALLLLLL damage goes through this function */
int damage(struct char_data *ch, struct char_data *victim, int dam,
        int attacktype, int dam_type, int offhand) {
  char buf[MAX_INPUT_LENGTH] = {'\0'};
  char buf1[MAX_INPUT_LENGTH] = {'\0'};
  bool is_ranged = FALSE;

  if (offhand == 2)
    is_ranged = TRUE;

  if (GET_POS(victim) <= POS_DEAD) { //delayed extraction
    if (PLR_FLAGGED(victim, PLR_NOTDEADYET) ||
            MOB_FLAGGED(victim, MOB_NOTDEADYET))
      return (-1);
    log("SYSERR: Attempt to damage corpse '%s' in room #%d by '%s'.",
            GET_NAME(victim), GET_ROOM_VNUM(IN_ROOM(victim)), GET_NAME(ch));
    die(victim, ch);
    return (-1);
  }

  if (ch->nr != real_mobile(DG_CASTER_PROXY) &&
          ch != victim && ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return (0);
  }

  if (!ok_damage_shopkeeper(ch, victim) || MOB_FLAGGED(victim, MOB_NOKILL)) {
    send_to_char(ch, "This mob is protected.\r\n");
    if (FIGHTING(ch) && FIGHTING(ch) == victim)
      stop_fighting(ch);
    if (FIGHTING(victim) && FIGHTING(victim) == ch)
      stop_fighting(victim);
    return (0);
  }
  if (!IS_NPC(victim) && ((GET_LEVEL(victim) >= LVL_IMMORT) &&
          PRF_FLAGGED(victim, PRF_NOHASSLE)))
    dam = 0; // immort protection

  if (victim != ch) {
    /* Only auto engage if both parties are unengaged. */
    if (GET_POS(ch) > POS_STUNNED && (FIGHTING(ch) == NULL) && (FIGHTING(victim) == NULL)) // ch -> vict
      set_fighting(ch, victim);

    // vict -> ch
    if (GET_POS(victim) > POS_STUNNED && (FIGHTING(victim) == NULL)) {
      set_fighting(victim, ch);
    }
  }
  if (victim->master == ch) // pet leaves you
    stop_follower(victim);

  if (!CONFIG_PK_ALLOWED) { // PK check
    check_killer(ch, victim);
    if (PLR_FLAGGED(ch, PLR_KILLER) && (ch != victim))
      dam = 0;
  }

  /* add to memory if applicable */
  if (MOB_FLAGGED(victim, MOB_MEMORY) && CAN_SEE(victim, ch)) {
    if (!IS_NPC(ch)) {
      remember(victim, ch);
    } else if (IS_PET(ch) && ch->master && IN_ROOM(ch->master) == IN_ROOM(ch)
            && !IS_NPC(ch->master))
      remember(victim, ch->master);  // help curb pet-fodder methods
  }

  /* set to hunting if applicable */
  if (MOB_FLAGGED(victim, MOB_HUNTER) && CAN_SEE(victim, ch) &&
          !HUNTING(victim)) {
    if (!IS_NPC(ch)) {
      HUNTING(victim) = ch;
    } else if (IS_PET(ch) && ch->master && IN_ROOM(ch->master) == IN_ROOM(ch)
            && !IS_NPC(ch->master))
      HUNTING(victim) = ch->master;  // help curb pet-fodder methods
  }

  dam = damage_handling(ch, victim, dam, attacktype, dam_type); //modify damage
  if (dam == -1) // make sure message handling has been done!
    return 0;

  /* defensive roll, avoids a lethal blow once every X minutes
   * X = about 7 minutes with current settings
   */
  if (!IS_NPC(victim) && ((GET_HIT(victim) - dam) <= 0) &&
          HAS_FEAT(victim, FEAT_DEFENSIVE_ROLL) &&
          !char_has_mud_event(victim, eD_ROLL) && ch != victim) {
    act("\tWYou time a defensive roll perfectly and avoid the attack from"
            " \tn$N\tW!\tn", FALSE, victim, NULL, ch, TO_CHAR);
    act("$n \tRtimes a defensive roll perfectly and avoids your attack!\tn",
            FALSE, victim, NULL, ch, TO_VICT | TO_SLEEP);
    act("$n times a \tWdefensive roll\tn perfectly and avoids an attack "
            "from $N!\tn", FALSE, victim, NULL, ch, TO_NOTVICT);
    attach_mud_event(new_mud_event(eD_ROLL, victim, NULL),
            (2 * SECS_PER_MUD_DAY));
    //increase_skill(victim, SKILL_DEFENSE_ROLL);
    return 0;
  }

  dam = MAX(MIN(dam, 999), 0); //damage cap
  GET_HIT(victim) -= dam;

  if (ch != victim) //xp gain
    gain_exp(ch, GET_LEVEL(victim) * dam);

  if (!dam)
    update_pos(victim);
  else
    update_pos_dam(victim);

  if (dam) { //display damage done
    sprintf(buf1, "[%d]", dam);
    sprintf(buf, "%5s", buf1);
    send_to_char(ch, "\tW%s\tn ", buf);
    send_to_char(victim, "\tR%s\tn ", buf);
  }

  if (attacktype != -1) { //added for mount, etc
    if (!IS_WEAPON(attacktype)) //non weapons use skill_message
      skill_message(dam, ch, victim, attacktype, offhand);
    else {
      //miss and death = skill_message
      if (GET_POS(victim) == POS_DEAD || dam == 0) {
        if (!dam && is_ranged)  // miss with ranged = dam_message()
          dam_message(dam, ch, victim, attacktype, offhand);
        else if (!skill_message(dam, ch, victim, attacktype, offhand))
          //default if no skill_message
          dam_message(dam, ch, victim, attacktype, offhand);
      } else {
        dam_message(dam, ch, victim, attacktype, offhand); //default landed-hit
      }
    }
  }

  switch (GET_POS(victim)) { //act() used in case someone is dead
    case POS_MORTALLYW:
      act("$n is mortally wounded, and will die soon, if not aided.",
              TRUE, victim, 0, 0, TO_ROOM);
      send_to_char(victim, "You are mortally wounded, and will die "
              "soon, if not aided.\r\n");
      break;
    case POS_INCAP:
      act("$n is incapacitated and will slowly die, if not aided.",
              TRUE, victim, 0, 0, TO_ROOM);
      send_to_char(victim, "You are incapacitated and will slowly "
              "die, if not aided.\r\n");
      break;
    case POS_STUNNED:
      act("$n is stunned, but will probably regain consciousness again.",
              TRUE, victim, 0, 0, TO_ROOM);
      send_to_char(victim, "You're stunned, but will probably regain "
              "consciousness again.\r\n");
      break;
    case POS_DEAD:
      act("$n is dead!  R.I.P.", FALSE, victim, 0, 0, TO_ROOM);
      send_to_char(victim, "You are dead!  Sorry...\r\n");
      break;
    default:
      if (dam > (GET_MAX_HIT(victim) / 4))
        send_to_char(victim, "\tYThat really did \tRHURT\tY!\tn\r\n");
      if (GET_HIT(victim) < (GET_MAX_HIT(victim) / 4)) {
        send_to_char(victim, "\tnYou wish that your wounds would stop "
                "\tRB\trL\tRE\trE\tRD\trI\tRN\trG\tn so much!\r\n");

        if (ch != victim && MOB_FLAGGED(victim, MOB_WIMPY)) //wimpy mobs
          if (GET_HIT(victim) > 0)
            if (!IS_CASTING(victim) && GET_POS(victim) >= POS_FIGHTING)
              perform_flee(victim);
      }
      if (!IS_NPC(victim) && GET_WIMP_LEV(victim) && (victim != ch) && //pc wimpy
              GET_HIT(victim) < GET_WIMP_LEV(victim) && GET_HIT(victim) > 0 &&
              IN_ROOM(ch) == IN_ROOM(victim) && !IS_CASTING(victim) &&
              GET_POS(victim) >= POS_FIGHTING) {
        send_to_char(victim, "You wimp out, and attempt to flee!\r\n");
        perform_flee(victim);
      }
      break;
  } //end SWITCH

  //linkless, very abusable, so trying with this off
  /*
  if (!IS_NPC(victim) && !(victim->desc) && GET_POS(victim) > POS_STUNNED) {
    perform_flee(victim);
    if (!FIGHTING(victim)) {
      act("$n is rescued by divine forces.", FALSE, victim, 0, 0, TO_ROOM);
      GET_WAS_IN(victim) = IN_ROOM(victim);
      char_from_room(victim);
      char_to_room(victim, 0);
    }
  }
   */

  //too hurt to continue
  if (GET_POS(victim) <= POS_STUNNED && FIGHTING(victim) != NULL)
    stop_fighting(victim);

  // lose hide/invis
  if (AFF_FLAGGED(ch, AFF_INVISIBLE) || AFF_FLAGGED(ch, AFF_HIDE))
    appear(ch, FALSE);

  if (GET_POS(victim) == POS_DEAD) // victim died
    return (dam_killed_vict(ch, victim));

  return (dam);
}

/* you are going to arrive here from an attack, or viewing mode
 * We have two functions: compute_hit_damage() and compute_damage_bonus() that
 * both basically will compute how much damage a given hit will do or display
 * how much damage potential you have (attacks command).  What is the difference?
 *   Compute_hit_damage() basically calculates bonus damage that will not be
 * displayed, compute_damage_bonus() calculates bonus damage that will be
 * displayed.  compute_hit_damage() always calls compute_damage_bonus() */
/* #define MODE_NORMAL_HIT       0
   #define MODE_DISPLAY_PRIMARY  2
   #define MODE_DISPLAY_OFFHAND  3
   #define MODE_DISPLAY_RANGED   4
 * Valid attack_type(s) are:
 *   ATTACK_TYPE_PRIMARY : Primary hand attack.
 *   ATTACK_TYPE_OFFHAND : Offhand attack.
 *   ATTACK_TYPE_RANGED  : Ranged attack.
 *   ATTACK_TYPE_UNARMED : Unarmed attack.
 *   ATTACK_TYPE_TWOHAND : Two-handed weapon attack. */
int compute_damage_bonus(struct char_data *ch, struct char_data *vict,
        struct obj_data *wielded, int w_type, int mod, int mode, int attack_type) {
  int dambonus = mod;

  /* strength or dexterity damage bonus */
  /* hack-a-licious - dex adds damage bonus to archery */
  if (mode == MODE_DISPLAY_RANGED || attack_type == ATTACK_TYPE_RANGED) {
    dambonus += GET_DEX_BONUS(ch);
    if (vict && IN_ROOM(ch) == IN_ROOM(vict)) {
      if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_POINT_BLANK_SHOT))
        dambonus++;
    }
  /* strength */
  } else if (attack_type == ATTACK_TYPE_TWOHAND) /* 2-hand weapons get 3/2 str bonus */
    dambonus += GET_STR_BONUS(ch) * 3 / 2;
  /* offhand gets half strength bonus */
  else if (mode == MODE_DISPLAY_OFFHAND || attack_type == ATTACK_TYPE_OFFHAND)
    dambonus += GET_STR_BONUS(ch) / 2;
  else /* normal */
    dambonus += GET_STR_BONUS(ch);

  /* Circumstance penalty */
  switch (GET_POS(ch)) {
    case POS_SITTING:
    case POS_RESTING:
    case POS_SLEEPING:
    case POS_STUNNED:
    case POS_INCAP:
    case POS_MORTALLYW:
    case POS_DEAD:
      dambonus -= 2;
      break;
    case POS_FIGHTING:
    case POS_STANDING:
    default: break;
  }

  /* fatigued */
  if (AFF_FLAGGED(ch, AFF_FATIGUED))
    dambonus -= 2;

  /* size */
  if (vict)
    dambonus += compute_size_bonus(GET_SIZE(ch), GET_SIZE(vict));

  /* weapon specialist */
  if (HAS_FEAT(ch, FEAT_WEAPON_SPECIALIZATION)) {
    /* Check the weapon type, make sure it matches. */
    if(((wielded != NULL) && HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_WEAPON_SPECIALIZATION), GET_WEAPON_TYPE(wielded))) ||
       ((wielded == NULL) && HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_WEAPON_SPECIALIZATION), WEAPON_TYPE_UNARMED)))
      dambonus += 2;
  }

  if (HAS_FEAT(ch, FEAT_GREATER_WEAPON_SPECIALIZATION)) {
    /* Check the weapon type, make sure it matches. */
    if(((wielded != NULL) && HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_GREATER_WEAPON_SPECIALIZATION), GET_WEAPON_TYPE(wielded))) ||
       ((wielded == NULL) && HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_GREATER_WEAPON_SPECIALIZATION), WEAPON_TYPE_UNARMED)))
      dambonus += 2;
  }

  /* damroll (should be mostly just gear) */
  dambonus += GET_DAMROLL(ch);

  /* weapon enhancement bonus, might need some work */
  if (wielded)
    dambonus += GET_ENHANCEMENT_BONUS(wielded);

  /* power attack */
  if (AFF_FLAGGED(ch, AFF_POWER_ATTACK))
    dambonus += COMBAT_MODE_VALUE(ch);

  /* crystal fist */
  if (char_has_mud_event(ch, eCRYSTALFIST))
    dambonus += 3;

  /* smite evil (remove after one attack) */
  if (affected_by_spell(ch, SKILL_SMITE) && vict && IS_EVIL(vict)) {
    dambonus += CLASS_LEVEL(ch, CLASS_PALADIN);
    if (mode == MODE_NORMAL_HIT)
      affect_from_char(ch, SKILL_SMITE);
  }

  /* favored enemy */
  if (vict && vict != ch && !IS_NPC(ch) && CLASS_LEVEL(ch, CLASS_RANGER)) {
    // checking if we have humanoid favored enemies for PC victims
    if (!IS_NPC(vict) && IS_FAV_ENEMY_OF(ch, NPCRACE_HUMAN))
      dambonus += CLASS_LEVEL(ch, CLASS_RANGER) / 5 + 2;
    else if (IS_NPC(vict) && IS_FAV_ENEMY_OF(ch, GET_RACE(vict)))
      dambonus += CLASS_LEVEL(ch, CLASS_RANGER) / 5 + 2;
  }

  /* paladin's divine bond */
  /* maximum of 6 damage 1 + level / 3 (past level 5) */
  if (HAS_FEAT(ch, FEAT_DIVINE_BOND)) {
    dambonus += MIN(6, 1 + MAX(0, (CLASS_LEVEL(ch, CLASS_PALADIN) - 5) / 3));
  }

  /* morale bonus */
  if (affected_by_spell(ch, SKILL_POWERFUL_BLOW)) {
    dambonus += CLASS_LEVEL(ch, CLASS_BERSERKER) / 4 + 1;
  } /* THIS IS JUST FOR SHOW, it gets taken out before the damage is calculated
     * the actual damage bonus is inserted in the code below */

  /* if the victim is using 'come and get me' then they will be vulnerable */
  if (vict && affected_by_spell(vict, SKILL_COME_AND_GET_ME)) {
    dambonus += 4;
  }

  /* temporary filler for ki-strike until we get it working right */
  if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_KI_STRIKE))
    dambonus += HAS_FEAT(ch, FEAT_KI_STRIKE);

  /****************************************/
  /**** display, keep mods above this *****/
  /****************************************/
  if  (mode != MODE_NORMAL_HIT) {
    send_to_char(ch, "Dam Bonus:  %d\r\n", dambonus);
  }

  return (MIN(MAX_DAM_BONUS, dambonus));
}

void compute_barehand_dam_dice(struct char_data *ch, int *diceOne, int *diceTwo) {
  if (!ch)
    return;

  int monkLevel = CLASS_LEVEL(ch, CLASS_MONK);

  if (IS_NPC(ch)) {
    *diceOne = ch->mob_specials.damnodice;
    *diceTwo = ch->mob_specials.damsizedice;
  } else {
    if (monkLevel && monk_gear_ok(ch)) { // monk?
      if (monkLevel < 4) {
        *diceOne = 1;
        *diceTwo = 6;
      } else if (monkLevel < 8) {
        *diceOne = 1;
        *diceTwo = 8;
      } else if (monkLevel < 12) {
        *diceOne = 1;
        *diceTwo = 10;
      } else if (monkLevel < 16) {
        *diceOne = 2;
        *diceTwo = 6;
      } else if (monkLevel < 20) {
        *diceOne = 4;
        *diceTwo = 4;
      } else if (monkLevel < 25) {
        *diceOne = 4;
        *diceTwo = 5;
      } else {
        *diceOne = 4;
        *diceTwo = 6;
      }
      if (GET_RACE(ch) == RACE_TRELUX)
        *diceOne = *diceOne + 1;
    } else { // non-monk bare-hand damage
      if (GET_RACE(ch) == RACE_TRELUX) {
        *diceOne = 2;
        *diceTwo = 6;
      } else {
        *diceOne = 1;
        *diceTwo = 2;
      }
    }
  }
}

/* computes damage dice based on bare-hands, weapon, class (monk), or
 npc's (which use special bare hand damage dice) */
/* #define MODE_NORMAL_HIT       0
   #define MODE_DISPLAY_PRIMARY  2
   #define MODE_DISPLAY_OFFHAND  3
   #define MODE_DISPLAY_RANGED   4 */
int compute_dam_dice(struct char_data *ch, struct char_data *victim,
        struct obj_data *wielded, int mode) {
  int diceOne = 0, diceTwo = 0;

  //just information mode
  if (mode == MODE_DISPLAY_PRIMARY) {
    if (!GET_EQ(ch, WEAR_WIELD_1) && !GET_EQ(ch, WEAR_WIELD_2H)) {
      send_to_char(ch, "Bare-hands\r\n");
    } else {
      if (GET_EQ(ch, WEAR_WIELD_2H))
        wielded = GET_EQ(ch, WEAR_WIELD_2H);
      else
        wielded = GET_EQ(ch, WEAR_WIELD_1);
      show_obj_to_char(wielded, ch, SHOW_OBJ_SHORT, 0);
    }
  } else if (mode == MODE_DISPLAY_OFFHAND) {
    if (!GET_EQ(ch, WEAR_WIELD_2)) {
      send_to_char(ch, "Bare-hands\r\n");
    } else {
      wielded = GET_EQ(ch, WEAR_WIELD_2);
      show_obj_to_char(GET_EQ(ch, WEAR_WIELD_2), ch, SHOW_OBJ_SHORT, 0);
    }
  }

  if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON) { //weapon
    diceOne = GET_OBJ_VAL(wielded, 1);
    diceTwo = GET_OBJ_VAL(wielded, 2);
  } else if (mode == MODE_DISPLAY_RANGED && can_fire_arrow(ch, TRUE)) { //ranged info
    struct obj_data *obj = GET_EQ(ch, WEAR_WIELD_2H);
    struct obj_data *ammo_pouch = GET_EQ(ch, WEAR_AMMO_POUCH);

    if (!obj)
      obj = GET_EQ(ch, WEAR_WIELD_1);

    show_obj_to_char(obj, ch, SHOW_OBJ_SHORT, 0);
    diceOne = GET_OBJ_VAL(obj, 1);
    diceTwo = GET_OBJ_VAL(ammo_pouch->contains, 1);
  } else { //barehand
    compute_barehand_dam_dice(ch, &diceOne, &diceTwo);
  }
  if (mode == MODE_DISPLAY_PRIMARY ||
      mode == MODE_DISPLAY_OFFHAND ||
      mode == MODE_DISPLAY_RANGED) {
    send_to_char(ch, "Damage Dice:  %dD%d, ", diceOne, diceTwo);
  }

  return dice(diceOne, diceTwo);
}

/* simple test for testing critical hit */
int is_critical_hit(struct char_data *ch, struct obj_data *wielded, int diceroll,
                  int calc_bab, int victim_ac) {
  int threat_range = 19, confirm_roll = dice(1,20) + calc_bab;

  if (wielded)
    threat_range = 20 - weapon_list[GET_OBJ_VAL(wielded, 0)].critRange;
  else
    threat_range = 20;

  if (HAS_FEAT(ch, FEAT_IMPROVED_CRITICAL)) { /* Check the weapon type, make sure it matches. */
    if(((wielded != NULL) && HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_IMPROVED_CRITICAL), GET_WEAPON_TYPE(wielded))) ||
       ((wielded == NULL) && HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_IMPROVED_CRITICAL), WEAPON_TYPE_UNARMED)))
      threat_range--;
  }

  if (diceroll >= threat_range) { /* critical potential? */
    if (HAS_FEAT(ch, FEAT_POWER_CRITICAL)) { /* Check the weapon type, make sure it matches. */
      if(((wielded != NULL) && HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_POWER_CRITICAL), GET_WEAPON_TYPE(wielded))) ||
         ((wielded == NULL) && HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_POWER_CRITICAL), WEAPON_TYPE_UNARMED)))
        confirm_roll += 4;
    }
    if (confirm_roll >= victim_ac)  /* confirm critical */
      return 1; /* yep, critical! */
  }

  return 0; /* nope, no critical */
}

int determine_attack_type(struct char_data *ch, struct obj_data *wielded,
                          int attack_type) {
  return FALSE;
}

/* you are going to arrive here from an attack, or viewing mode
 * We have two functions: compute_hit_damage() and compute_damage_bonus() that
 * both basically will compute how much damage a given hit will do or display
 * how much damage potential you have (attacks command).  What is the difference?
 *   Compute_hit_damage() basically calculates bonus damage that will not be
 * displayed, compute_damage_bonus() calculates bonus damage that will be
 * displayed.  compute_hit_damage() always calls compute_damage_bonus() */
/* #define MODE_NORMAL_HIT       0
   #define MODE_DISPLAY_PRIMARY  2
   #define MODE_DISPLAY_OFFHAND  3
   #define MODE_DISPLAY_RANGED   4
 * Valid attack_type(s) are:
 *   ATTACK_TYPE_PRIMARY : Primary hand attack.
 *   ATTACK_TYPE_OFFHAND : Offhand attack.
 *   ATTACK_TYPE_RANGED  : Ranged attack.
 *   ATTACK_TYPE_UNARMED : Unarmed attack.
 *   ATTACK_TYPE_TWOHAND : Two-handed weapon attack. */
int compute_hit_damage(struct char_data *ch, struct char_data *victim,
        int w_type, int diceroll, int mode, bool is_critical, int attack_type) {
  int dam = 0, crit_multi = 2 /*default 2x*/;
  /*redundancy necessary due to sometimes arriving here without going through hit()*/
  struct obj_data *wielded = get_wielded(ch, attack_type);

  /* this may be completely unnecessary */
  switch (mode) {
    case MODE_DISPLAY_PRIMARY:
    case MODE_DISPLAY_RANGED:
      if (!wielded)
        wielded = GET_EQ(ch, WEAR_WIELD_2H);
      break;
    case MODE_DISPLAY_OFFHAND:
      wielded = GET_EQ(ch, WEAR_WIELD_2);
      break;
    case MODE_NORMAL_HIT:
    default:
      break;
  }
  if (GET_EQ(ch, WEAR_WIELD_2H) && mode != MODE_DISPLAY_RANGED &&
      attack_type != ATTACK_TYPE_RANGED)
    attack_type = ATTACK_TYPE_TWOHAND;

  /* calculate how much damage to do with a given hit() */
  if (mode == MODE_NORMAL_HIT) {
    /* determine weapon dice damage (or lack of weaopn) */
    dam = compute_dam_dice(ch, victim, wielded, mode);
    /* add any modifers to melee damage: strength, circumstance penalty, fatigue, size, etc etc */
    dam += compute_damage_bonus(ch, victim, wielded, w_type, NO_MOD, mode, attack_type);

    /* calculate bonus to damage based on target position */
    switch (GET_POS(victim)) {
      case POS_SITTING:
        dam += 2;
        break;
      case POS_RESTING:
        dam += 4;
        break;
      case POS_SLEEPING:
        dam *= 2;
        break;
      case POS_STUNNED:
        dam *= 1.25;
        break;
      case POS_INCAP:
        dam *= 1.5;
        break;
      case POS_MORTALLYW:
      case POS_DEAD:
        dam *= 1.75;
        break;
      case POS_STANDING:
      case POS_FIGHTING:
      default: break;
    }

    /* handle critical hit damage here */
    if (is_critical && !(IS_NPC(victim) && GET_RACE(victim) == NPCRACE_UNDEAD)) { /* critical bonus */
      if (wielded) {
        crit_multi = weapon_list[GET_OBJ_VAL(wielded, 0)].critMult;
      }
      dam *= crit_multi;
    }

    /* Add additional damage dice from weapon special abilities. - Ornir */
    if (wielded) {

      /* process weapon abilities - critical */
      if (is_critical && !(IS_NPC(victim) && GET_RACE(victim) == NPCRACE_UNDEAD))
        process_weapon_abilities(wielded, ch, victim, ACTMTD_ON_CRIT, NULL);

      if((obj_has_special_ability(wielded, WEAPON_SPECAB_ANARCHIC))
         && ((IS_LG(victim)) ||
             (IS_LN(victim)) ||
             (IS_LE(victim)) ||
             (IS_NPC(victim) || HAS_SUBRACE(victim, SUBRACE_LAWFUL)))) {
        /* Do 2d6 more damage. */
        dam += dice(2, 6);
      }
      if((obj_has_special_ability(wielded, WEAPON_SPECAB_AXIOMATIC))
         && ((IS_CG(victim)) ||
             (IS_CN(victim)) ||
             (IS_CE(victim)) ||
             (IS_NPC(victim) || HAS_SUBRACE(victim, SUBRACE_CHAOTIC)))) {
        /* Do 2d6 more damage. */
        dam += dice(2, 6);
      }
      if((obj_has_special_ability(wielded, WEAPON_SPECAB_BANE))) {
        /* Check the values in the special ability record for the NPCRACE and SUBRACE. */
        int *value = get_obj_special_ability(wielded, WEAPON_SPECAB_BANE)->value;
        if((GET_RACE(victim) == value[0]) && (HAS_SUBRACE(victim, value[1]))) {
          /*send_to_char(ch, "Your weapon hums in delight as it strikes!\r\n");*/
          dam += dice(2, 6);
        }
      }

      /* mounted charging character using charging weapons, whether this goes up
       * top or bottom of dam calculation can have a dramtic effect on this number */
      if (AFF_FLAGGED(ch, AFF_CHARGING) && RIDING(ch)) {
        if (HAS_FEAT(ch, FEAT_SPIRITED_CHARGE)) { /* mounted, charging with spirited charge feat */
          if (HAS_WEAPON_FLAG(wielded, WEAPON_FLAG_CHARGE)) { /* with lance too */
            /*debug*//*send_to_char(ch, "DEBUG: Weapon Charge Flag Working on Lance!\r\n");*/
            dam *= 3;
          } else
            dam *= 2;
        } else if (HAS_WEAPON_FLAG(wielded, WEAPON_FLAG_CHARGE)) { /* mounted charging, no feat, but with lance */
          /*debug*//*send_to_char(ch, "DEBUG: Weapon Charge Flag Working on Lance!\r\n");*/
          dam *= 2;
        }
      }

    } /* end wielded */

    /* calculate weapon damage for _display_ purposes */
  } else if (mode == MODE_DISPLAY_PRIMARY ||
             mode == MODE_DISPLAY_OFFHAND ||
             mode == MODE_DISPLAY_RANGED) {
    /* calculate dice */
    dam = compute_dam_dice(ch, ch, wielded, mode);
    /* modifiers to melee damage */
    dam += compute_damage_bonus(ch, ch, wielded, TYPE_UNDEFINED_WTYPE, NO_MOD, mode, attack_type);
  }

  return MAX(1, dam); //min damage of 1
}

/* this function takes ch (attacker) against victim (defender) who has
   inflicted dam damage and will reduce damage by X depending on the type
   of 'ward' the defender has (such as stoneskin)
   this will return the modified damage
 * -note- melee only */
#define STONESKIN_ABSORB	16
#define IRONSKIN_ABSORB	36
#define EPIC_WARDING_ABSORB	76
int handle_warding(struct char_data *ch, struct char_data *victim, int dam) {
  int warding = 0;

  if (affected_by_spell(victim, SPELL_STONESKIN)) {
    if (GET_STONESKIN(victim) <= 0) {
      send_to_char(victim, "\tDYour skin has reverted from stone!\tn\r\n");
      affect_from_char(victim, SPELL_STONESKIN);
      GET_STONESKIN(victim) = 0;
      return dam;
    }
    warding = MIN(MIN(STONESKIN_ABSORB, GET_STONESKIN(victim)), dam);

    GET_STONESKIN(victim) -= warding;
    dam -= warding;
    if (GET_STONESKIN(victim) <= 0) {
      send_to_char(victim, "\tDYour skin has reverted from stone!\tn\r\n");
      affect_from_char(victim, SPELL_STONESKIN);
      GET_STONESKIN(victim) = 0;
    }
    if (dam <= 0) {
      send_to_char(victim, "\tWYour skin of stone absorbs the attack!\tn\r\n");
      send_to_char(ch,
              "\tRYou have failed to penetrate the stony skin of %s!\tn\r\n",
              GET_NAME(victim));
      act("$n fails to penetrate the stony skin of $N!", FALSE, ch, 0, victim,
              TO_NOTVICT);
      return -1;
    } else {
      send_to_char(victim, "\tW<stone:%d>\tn", warding);
      send_to_char(ch, "\tR<oStone:%d>\tn", warding);
    }

  } else if (affected_by_spell(victim, SPELL_IRONSKIN)) {
    if (GET_STONESKIN(victim) <= 0) {
      send_to_char(victim, "\tDYour ironskin has faded!\tn\r\n");
      affect_from_char(victim, SPELL_IRONSKIN);
      GET_STONESKIN(victim) = 0;
      return dam;
    }
    warding = MIN(IRONSKIN_ABSORB, GET_STONESKIN(victim));

    GET_STONESKIN(victim) -= warding;
    dam -= warding;
    if (GET_STONESKIN(victim) <= 0) {
      send_to_char(victim, "\tDYour ironskin has fallen!\tn\r\n");
      affect_from_char(victim, SPELL_IRONSKIN);
      GET_STONESKIN(victim) = 0;
    }
    if (dam <= 0) {
      send_to_char(victim, "\tWYour ironskin absorbs the attack!\tn\r\n");
      send_to_char(ch,
              "\tRYou have failed to penetrate the ironskin of %s!\tn\r\n",
              GET_NAME(victim));
      act("$n fails to penetrate the ironskin of $N!", FALSE, ch, 0, victim,
              TO_NOTVICT);
      return -1;
    } else {
      send_to_char(victim, "\tW<ironskin:%d>\tn", warding);
      send_to_char(ch, "\tR<oIronskin:%d>\tn", warding);
    }

  } else if (affected_by_spell(victim, SPELL_EPIC_WARDING)) {
    if (GET_STONESKIN(victim) <= 0) {
      send_to_char(victim, "\tDYour ward has fallen!\tn\r\n");
      affect_from_char(victim, SPELL_EPIC_WARDING);
      GET_STONESKIN(victim) = 0;
      return dam;
    }
    warding = MIN(EPIC_WARDING_ABSORB, GET_STONESKIN(victim));

    GET_STONESKIN(victim) -= warding;
    dam -= warding;
    if (GET_STONESKIN(victim) <= 0) {
      send_to_char(victim, "\tDYour ward has fallen!\tn\r\n");
      affect_from_char(victim, SPELL_EPIC_WARDING);
      GET_STONESKIN(victim) = 0;
    }
    if (dam <= 0) {
      send_to_char(victim, "\tWYour ward absorbs the attack!\tn\r\n");
      send_to_char(ch,
              "\tRYou have failed to penetrate the ward of %s!\tn\r\n",
              GET_NAME(victim));
      act("$n fails to penetrate the ward of $N!", FALSE, ch, 0, victim,
              TO_NOTVICT);
      return -1;
    } else {
      send_to_char(victim, "\tW<ward:%d>\tn", warding);
      send_to_char(ch, "\tR<oWard:%d>\tn", warding);
    }

  } else { // has no warding
    return dam;
  }

  return dam;
}
#undef STONESKIN_ABSORB
#undef EPIC_WARDING_ABSORB
#undef IRONSKIN_ABSORB

bool weapon_bypasses_dr(struct obj_data *weapon, struct damage_reduction_type *dr) {
  bool passed = FALSE;
  int i = 0;

  /* TODO Change this to handle unarmed attacks! */
  if (weapon == NULL)
    return FALSE;

  for (i = 0; i < MAX_DR_BYPASS; i++) {
    if (dr->bypass_cat[i] != DR_BYPASS_CAT_UNUSED) {
      switch (dr->bypass_cat[i]) {
        case DR_BYPASS_CAT_NONE:
          break;
        case DR_BYPASS_CAT_MAGIC:
          if (IS_SET_AR(GET_OBJ_EXTRA(weapon), ITEM_MAGIC))
            passed = TRUE;
          break;
        case DR_BYPASS_CAT_MATERIAL:
          if (GET_OBJ_MATERIAL(weapon) == dr->bypass_val[i])
            passed = TRUE;
          break;
        case DR_BYPASS_CAT_DAMTYPE:
          if ((dr->bypass_val[i] == DR_DAMTYPE_BLUDGEONING) &&
              (HAS_DAMAGE_TYPE(weapon, DAMAGE_TYPE_BLUDGEONING))
              passed = TRUE;
          else if ((dr->bypass_val[i] == DR_DAMTYPE_SLASHING) &&
                   (HAS_DAMAGE_TYPE(weapon, DAMAGE_TYPE_SLASHING))
                   passed = TRUE;
          else if ((dr->bypass_val[i] == DR_DAMTYPE_PIERCING) &&
                   (HAS_DAMAGE_TYPE(weapon, DAMAGE_TYPE_PIERCING))
                   passed = TRUE;
                   break;
      }
    }
  }
  return passed;
}

int apply_damage_reduction(struct char_data *ch, struct char_data *victim, struct obj_data *wielded, int dam) {
  struct damage_reduction_type *dr, *cur;
  //struct damage_reduction_type *temp;
  int reduction = 0;

  /* No DR, just return dam.*/
  if (GET_DR(victim) == NULL)
    return dam;

  dr = NULL;
  for (cur = GET_DR(victim); cur != NULL; cur = cur->next) {
    if (dr == NULL || (dr->amount < cur->amount && (weapon_bypasses_dr(wielded, cur) == FALSE)))
      dr = cur;
  }

  /* Now dr is set to the 'best' DR for the incoming damage. */
  if (weapon_bypasses_dr(wielded, dr) == TRUE) {
    reduction = 0;
  } else
    reduction = MIN(dr->amount, dam);

  if ((reduction > 0) &&
      (dr->max_damage > 0)) {
    /* Damage the DR...*/
    dr->max_damage -= reduction;
    if (dr->max_damage <= 0) {
      /* The DR was destroyed!*/
      if (affected_by_spell(victim, dr->spell)) {
        affect_from_char(victim, dr->spell);
        if (spell_info[dr->spell].wear_off_msg)
          send_to_char(victim, "%s\r\n", spell_info[dr->spell].wear_off_msg);
      }
    }
  }
  return MAX(-1, dam - reduction);
}

/* all weapon poison system is right now is just firing spells off our weapon
   if the weapon has that given spell-num applied to it as a poison
   i have ambitious plans in the future to completely re-work poison in our
   system, and at that time i will re-work this -z */
void weapon_poison(struct char_data *ch, struct char_data *victim, struct obj_data *wielded) {

  /* start with the usual dummy checks */
  if (!ch)
    return;
  if (!victim)
    return;
  if (!wielded)
    return;

  if (!wielded->weapon_poison.poison) /* this weapon is not poisoned */
    return;

  /* 20% chance to fire currently */
  if (rand_number(0, 5))
    return;

  act("The \tGpoison\tn from $p attaches to $n.",
          FALSE, victim, wielded, 0, TO_ROOM);
  call_magic(ch, victim, wielded, wielded->weapon_poison.poison, wielded->weapon_poison.poison_level,
          CAST_POTION);
  wielded->weapon_poison.poison_level -= wielded->weapon_poison.poison_level / 4;
  wielded->weapon_poison.poison_hits--;
  if (wielded->weapon_poison.poison_hits <= 0)
    wielded->weapon_poison.poison = 0;
}

/* this function will call the spell-casting ability of the
   given weapon (wpn) attacker (ch) has when attacking vict
   these are always 'violent' spells */
void weapon_spells(struct char_data *ch, struct char_data *vict,
        struct obj_data *wpn) {
  int i = 0, random;

  if (wpn && HAS_SPELLS(wpn)) {

    for (i = 0; i < MAX_WEAPON_SPELLS; i++) {
      if (GET_WEAPON_SPELL(wpn, i) && GET_WEAPON_SPELL_AGG(wpn, i)) {
        if (ch->in_room != vict->in_room) {
          if (FIGHTING(ch) && FIGHTING(ch) == vict)
            stop_fighting(ch);
          return;
        }
        random = rand_number(1, 100);
        if (GET_WEAPON_SPELL_PCT(wpn, i) >= random) {
          act("$p leaps to action with an attack of its own.",
                  TRUE, ch, wpn, 0, TO_CHAR);
          act("$p leaps to action with an attack of its own.",
                  TRUE, ch, wpn, 0, TO_ROOM);
          if (call_magic(ch, vict, wpn, GET_WEAPON_SPELL(wpn, i),
                  GET_WEAPON_SPELL_LVL(wpn, i), CAST_WAND) < 0)
            return;
        }
      }
    }
  }
}

/* if (ch) has a weapon with weapon spells on it that is
   considered non-offensive, the weapon will target (ch)
   with this spell - does not require to be in combat */
void idle_weapon_spells(struct char_data *ch) {
  int random = 0, j = 0;
  struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD_1);
  struct obj_data *offWield = GET_EQ(ch, WEAR_WIELD_2);

  if (GET_EQ(ch, WEAR_WIELD_2H))
    wielded = GET_EQ(ch, WEAR_WIELD_2H);

  if (wielded && HAS_SPELLS(wielded)) {
    for (j = 0; j < MAX_WEAPON_SPELLS; j++) {
      if (!GET_WEAPON_SPELL_AGG(wielded, j) &&
              GET_WEAPON_SPELL(wielded, j)) {
        random = rand_number(1, 100);
        if (GET_WEAPON_SPELL_PCT(wielded, j) >= random) {
          act("$p leaps to action.",
                  TRUE, ch, wielded, 0, TO_CHAR);
          act("$p leaps to action.",
                  TRUE, ch, wielded, 0, TO_ROOM);
          call_magic(ch, ch, NULL, GET_WEAPON_SPELL(wielded, j),
                  GET_WEAPON_SPELL_LVL(wielded, j), CAST_WAND);
        }
      }
    }
  }

  if (offWield && HAS_SPELLS(offWield)) {
    for (j = 0; j < MAX_WEAPON_SPELLS; j++) {
      if (!GET_WEAPON_SPELL_AGG(offWield, j) &&
              GET_WEAPON_SPELL(offWield, j)) {
        random = rand_number(1, 100);
        if (GET_WEAPON_SPELL_PCT(offWield, j) >= random) {
          act("$p leaps to action.",
                  TRUE, ch, offWield, 0, TO_CHAR);
          act("$p leaps to action.",
                  TRUE, ch, offWield, 0, TO_ROOM);
          call_magic(ch, ch, NULL, GET_WEAPON_SPELL(offWield, j),
                  GET_WEAPON_SPELL_LVL(offWield, j), CAST_WAND);
        }
      }
    }
  }
}

/* weapon spell function for random weapon procs,
 *  modified from original source - Iyachtu */
int weapon_special(struct obj_data *wpn, struct char_data *ch, char *hit_msg) {
  if (!wpn)
    return 0;

  extern struct index_data *obj_index;
  int (*name)(struct char_data *ch, void *me, int cmd, char *argument);

  name = obj_index[GET_OBJ_RNUM(wpn)].func;

  if (!name)
    return 0;

  return (name) (ch, wpn, 0, hit_msg);
}


/* Return the wielded weapon based on the attack type.
 * Valid attack_type(s) are:
 *   ATTACK_TYPE_PRIMARY : Primary hand attack.
 *   ATTACK_TYPE_OFFHAND : Offhand attack.
 *   ATTACK_TYPE_RANGED  : Ranged attack
 *   ATTACK_TYPE_TWOHAND : Two-handed weapon attack. */
struct obj_data *get_wielded(struct char_data *ch, /* Wielder */
                             int attack_type)      /* Type of attack. */
{
  struct obj_data *wielded = NULL;

  /* Check the primary hand location. */
  wielded = GET_EQ(ch, WEAR_WIELD_1);

  if (!wielded && (attack_type == ATTACK_TYPE_RANGED)) {
    wielded = GET_EQ(ch, WEAR_WIELD_2H);
  } else if (attack_type == ATTACK_TYPE_OFFHAND) {
    wielded = GET_EQ(ch, WEAR_WIELD_2);
  } else if ((attack_type == ATTACK_TYPE_PRIMARY) &&
              !wielded) {  // 2-hand weapon, primary hand
    wielded = GET_EQ(ch, WEAR_WIELD_2H);
  } else if (attack_type == ATTACK_TYPE_TWOHAND) {
    wielded = GET_EQ(ch, WEAR_WIELD_2H);
  }  // we either have wielded or NULL

  return wielded;
}

/* Calculate ch's attack bonus when attacking victim, for the type of attack given.
 * Valid attack_type(s) are:
 *   ATTACK_TYPE_PRIMARY : Primary hand attack.
 *   ATTACK_TYPE_OFFHAND : Offhand attack.
 *   ATTACK_TYPE_RANGED  : Ranged attack.
 *   ATTACK_TYPE_UNARMED : Unarmed attack.
 *   ATTACK_TYPE_TWOHAND : Two-handed weapon attack. */
int compute_attack_bonus (struct char_data *ch,     /* Attacker */
                          struct char_data *victim, /* Defender */
                          int attack_type)          /* Type of attack  */
{
  int i = 0;
  int bonuses[NUM_BONUS_TYPES];
  int calc_bab = BAB(ch); /* Start with base attack bonus */

  struct obj_data *wielded;

  if (attack_type == ATTACK_TYPE_UNARMED)
    wielded = NULL;
  else
    wielded = get_wielded(ch, attack_type);

  /* Initialize bonuses to 0 */
  for (i = 0; i < NUM_BONUS_TYPES; i++)
    bonuses[i] = 0;

  /* start with our base bonus of strength (or dex with feat) */
  if ((attack_type == ATTACK_TYPE_RANGED) ||
      (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_WEAPON_FINESSE))) {
    if (GET_DEX_BONUS(ch) >= GET_STR_BONUS(ch))
      calc_bab += GET_DEX_BONUS(ch);
    else
      calc_bab += GET_STR_BONUS(ch);
  } else {
    calc_bab += GET_STR_BONUS(ch);
  }

  /* NOTICE:  This may be something we phase out, but for basic
   function of the game to date, it is still necessary
   10/14/2014 Zusuk */
  calc_bab += GET_HITROLL(ch);
  /******/

  /* Circumstance bonus (stacks)*/
  switch (GET_POS(ch)) {
    case POS_SITTING:
    case POS_RESTING:
    case POS_SLEEPING:
    case POS_STUNNED:
    case POS_INCAP:
    case POS_MORTALLYW:
    case POS_DEAD:
      bonuses[BONUS_TYPE_CIRCUMSTANCE] -= 2;
      break;
    case POS_FIGHTING:
    case POS_STANDING:
    default: break;
  }

  if (AFF_FLAGGED(ch, AFF_FATIGUED))
    bonuses[BONUS_TYPE_CIRCUMSTANCE] -= 2;

  /* Competence bonus */

  /* Enhancement bonus */
  if (wielded)
    bonuses[BONUS_TYPE_ENHANCEMENT] = MAX(bonuses[BONUS_TYPE_ENHANCEMENT], GET_ENHANCEMENT_BONUS(wielded));

  /* Insight bonus  */

  /* Luck bonus */

  /* Morale bonus */
  if (affected_by_spell(ch, SKILL_SUPRISE_ACCURACY)) {
    bonuses[BONUS_TYPE_MORALE] += CLASS_LEVEL(ch, CLASS_BERSERKER) / 4 + 1;
  }

  /* Profane bonus */

  /* Racial bonus */

  /* Sacred bonus */

  /* Size bonus */
  bonuses[BONUS_TYPE_SIZE] = MAX(bonuses[BONUS_TYPE_SIZE], size_modifiers[GET_SIZE(ch)]);

  /* Unnamed bonus (stacks) */

  if (HAS_FEAT(ch, FEAT_WEAPON_FOCUS)) {
    if (wielded) {
      if (HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_WEAPON_FOCUS), GET_WEAPON_TYPE(wielded)) )
        bonuses[BONUS_TYPE_UNDEFINED] += 1;
    } else if (HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_WEAPON_FOCUS), WEAPON_TYPE_UNARMED))
      bonuses[BONUS_TYPE_UNDEFINED] += 1;
  }

  if (HAS_FEAT(ch, FEAT_GREATER_WEAPON_FOCUS)) {
    if (wielded) {
      if (HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_WEAPON_FOCUS), GET_WEAPON_TYPE(wielded)))
        bonuses[BONUS_TYPE_UNDEFINED] += 1;
    } else if (HAS_COMBAT_FEAT(ch, feat_to_cfeat(FEAT_GREATER_WEAPON_FOCUS), WEAPON_TYPE_UNARMED))
      bonuses[BONUS_TYPE_UNDEFINED] += 1;
  }

  if (victim) {
    /* blind fighting */
    if (!CAN_SEE(victim, ch) && !HAS_FEAT(victim, FEAT_BLIND_FIGHT))
      bonuses[BONUS_TYPE_UNDEFINED] += 2;

  /* point blank shot will give +1 bonus to hitroll if you are using a ranged
   * attack in the same room as victim */
    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_POINT_BLANK_SHOT) && IN_ROOM(ch) == IN_ROOM(victim))
      bonuses[BONUS_TYPE_UNDEFINED]++;
  }

  if (affected_by_spell(ch, SKILL_SMITE)) {
    bonuses[BONUS_TYPE_UNDEFINED] += GET_CHA_BONUS(ch);
  }

  /* EPIC PROWESS feat stacks, +1 for each time the feat is taken. */
  if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_EPIC_PROWESS))
    bonuses[BONUS_TYPE_UNDEFINED] += HAS_FEAT(ch, FEAT_EPIC_PROWESS);

  /* paladin's divine bond */
  /* maximum of 6 hitroll
     1 + level / 3 (past level 5) */
  if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_DIVINE_BOND)) {
    bonuses[BONUS_TYPE_UNDEFINED] += MIN(6, 1 + MAX(0, (CLASS_LEVEL(ch, CLASS_PALADIN) - 5) / 3));
  }

  /* temporary filler for ki-strike until we get it working right */
  if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_KI_STRIKE))
    bonuses[BONUS_TYPE_UNDEFINED] += HAS_FEAT(ch, FEAT_KI_STRIKE);

  /* Modify this to store a player-chosen number for power attack and expertise */
  if (AFF_FLAGGED(ch, AFF_POWER_ATTACK) || AFF_FLAGGED(ch, AFF_EXPERTISE))
    bonuses[BONUS_TYPE_UNDEFINED] -= COMBAT_MODE_VALUE(ch);

  /* favored enemy - Needs work */
  if (victim && victim != ch && !IS_NPC(ch) && HAS_FEAT(ch, FEAT_FAVORED_ENEMY)) {
    // checking if we have humanoid favored enemies for PC victims
    if (!IS_NPC(victim) && IS_FAV_ENEMY_OF(ch, NPCRACE_HUMAN))
      bonuses[BONUS_TYPE_UNDEFINED] += CLASS_LEVEL(ch, CLASS_RANGER) / 5 + 2;
    else if (IS_NPC(victim) && IS_FAV_ENEMY_OF(ch, GET_RACE(victim)))
      bonuses[BONUS_TYPE_UNDEFINED] += CLASS_LEVEL(ch, CLASS_RANGER) / 5 + 2;
  }

  /* spellbattle */
  if (char_has_mud_event(ch, eSPELLBATTLE) && SPELLBATTLE(ch) > 0)
    bonuses[BONUS_TYPE_UNDEFINED] -= SPELLBATTLE(ch);

  /* if the victim is using 'come and get me' then they will be vulnerable */
  if (victim && affected_by_spell(victim, SKILL_COME_AND_GET_ME)) {
    bonuses[BONUS_TYPE_UNDEFINED] += 4;
  }

  /*  Check armor/weapon proficiency
   *  If not proficient with weapon, -4 penalty applies. */
  if (wielded) {
    if (!is_proficient_with_weapon(ch, GET_WEAPON_TYPE(wielded))) {
      /*debug*/ //send_to_char(ch, "NOT PROFICIENT\r\n");
      calc_bab -= 4;
    }
  }

  /* Add armor prof here: If not proficient with worn armor, armor check
   * penalty applies to attack roll. */


  /* Add up all the bonuses */
  for (i = 0; i < NUM_BONUS_TYPES; i++)
    calc_bab += bonuses[i];

  return (MIN(MAX_BAB, calc_bab));
}

int compute_cmb (struct char_data *ch,              /*  Attacker */
                          struct char_data *victim, /*  Defender */
                          int attack_type)          /*  Type of attack  */
{
  return compute_attack_bonus(ch, victim, attack_type);
}

int compute_cmd(struct char_data *attacker, struct char_data *ch)
{
  return compute_armor_class(attacker, ch, TRUE) + GET_STR_BONUS(ch);
}

/*
 * Perform an attack, returns the difference of the attacker's roll and the defender's
 * AC.  This value can be negative, and will be on a miss.  Does not deal damage, only
 * checks to see if the attack was successful!
 *
 * Valid attack_type(s) are:
 *   ATTACK_TYPE_PRIMARY : Primary hand attack.
 *   ATTACK_TYPE_OFFHAND : Offhand attack.
 *   ATTACK_TYPE_RANGED  : Ranged attack.
 *   ATTACK_TYPE_UNARMED : Unarmed attack.
 */
int attack_roll(struct char_data *ch,           /* Attacker */
                struct char_data *victim,       /* Defender */
                int attack_type,                /* Type of attack */
                int is_touch,                   /* TRUE/FALSE this is a touch attack? */
                int attack_number)              /* Attack number, determines penalty. */
{

//  struct obj_data *wielded = get_wielded(ch, attack_type);

  int attack_bonus = compute_attack_bonus(ch, victim, attack_type);
  int victim_ac = compute_armor_class(ch, victim, is_touch);

  int diceroll = rand_number(1, 20);
  int result = ((attack_bonus + diceroll) - victim_ac);

//  if (attack_type == ATTACK_TYPE_RANGED) {
    /* 1d20 + base attack bonus + Dexterity modifier + size modifier + range penalty */
    /* Range penalty - only if victim is in a different room. */
//  } else if (attack_type == ATTACK_TYPE_PRIMARY) {
    /* 1d20 + base attack bonus + Strength modifier + size modifier */
//  } else if (attack_type == ATTACK_TYPE_OFFHAND) {
    /* 1d20 + base attack bonus + Strength modifier + size modifier */
//  }

//  send_to_char(ch, "DEBUG: attack bonus: %d, diceroll: %d, victim_ac: %d, result: %d\r\n", attack_bonus, diceroll, victim_ac, result);
  return result;
}

/* Perform an attack of opportunity (ch attacks victim).
 * Very simple, single hit with applied penalty (or bonus!) from ch to victim.
 *
 * If return value is != 0, then attack was a success.  If < 0, victim died.  If > 0 then it is the
 * amount of damage dealt.
 */
int attack_of_opportunity(struct char_data *ch, struct char_data *victim, int penalty) {

  if (AFF_FLAGGED(ch, AFF_FLAT_FOOTED) && !HAS_FEAT(ch, FEAT_COMBAT_REFLEXES))
    return 0;

  if (GET_TOTAL_AOO(ch) < (!HAS_FEAT(ch, FEAT_COMBAT_REFLEXES) ? 1 : GET_DEX_BONUS(ch))) {
    GET_TOTAL_AOO(ch)++;
    return hit(ch, victim, TYPE_ATTACK_OF_OPPORTUNITY, DAM_RESERVED_DBC, penalty, FALSE);
  } else {
    return 0; /* No attack, out of AOOs for this round. */
  }
}

/* Perform an attack of opportunity from every character engaged with ch. */
void attacks_of_opportunity(struct char_data *victim, int penalty) {
  struct char_data *ch;

  /* Check each char in the room, if it is engaged with victim, give it an AOO */
  for (ch = world[victim->in_room].people; ch; ch = ch->next_in_room) {
    /* Check engaged. */
    if (FIGHTING(ch) == victim) {
      attack_of_opportunity(ch, victim, penalty);
    }
  }
}

/* a potentially deprecated function (because of new weapon-types data in feats.c)
 * that will return the weapon-type being used based on attack_type and wielded data */
int determine_weapon_type(struct char_data *ch, struct char_data *victim,
                          struct obj_data *wielded, int attack_type) {
  int w_type = TYPE_HIT;

  if (attack_type == ATTACK_TYPE_RANGED) { // ranged-attack
    if (!wielded)
      w_type = TYPE_HIT;
    else {
      w_type = TYPE_MISSILE;
    }
  } else if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON) { /* !ranged */
    /* This may be made obsolete by weapon types... */
    w_type = GET_OBJ_VAL(wielded, 3) + TYPE_HIT; /* Get the weapon attack type from the wielded weapon. */
  } else {
    if (IS_NPC(ch) && ch->mob_specials.attack_type != 0)
      w_type = ch->mob_specials.attack_type + TYPE_HIT; /* We are a mob, and we have an attack type, so use that. */
    else
      w_type = TYPE_HIT; /* Generic default. */
  }

  return w_type;
}

/* primary function for a single melee attack
   ch -> attacker
   victim -> defender
   type -> SKILL_  /  SPELL_  / TYPE_ / etc. (determined here)
   dam_type -> DAM_FIRE / etc (not used here, passed to dam() function)
   penalty ->  (or bonus)  applied to hitroll, BAB multi-attack for example
   attack_type ->
     #define ATTACK_TYPE_PRIMARY   0
     #define ATTACK_TYPE_OFFHAND   1
     #define ATTACK_TYPE_RANGED    2
     #define ATTACK_TYPE_UNARMED   3
     #define ATTACK_TYPE_TWOHAND   4
   Attack queue will determine what kind of hit this is. */
#define DAM_MES_LENGTH  20
#define HIT_MISS 0
/* We need to figure out what this REALLY does, and how we can split it up logically to
 * make it easier to handle.  Notes below: */
int hit(struct char_data *ch, struct char_data *victim, int type, int dam_type,
        int penalty, int attack_type) {
  int w_type = 0,    /* Weapon type? */
      victim_ac = 0, /* Target's AC, from compute_ac(). */
      calc_bab = penalty,  /* ch's base attack bonus for the attack. */
      diceroll = 0,  /* ch's attack roll. */
      dam = 0,       /* Damage for the attack, with mods. */
      sneakdam = 0;  /* Additional sneak attack damage. */

  bool is_critical = FALSE;

  struct affected_type af; /* for crippling strike */

  char buf[DAM_MES_LENGTH] = {'\0'};  /* Damage message buffers. */
  char buf1[DAM_MES_LENGTH] = {'\0'};

  bool same_room = FALSE; /* Is the victim in the same room as ch? */

  struct obj_data *ammo_pouch = GET_EQ(ch, WEAR_AMMO_POUCH); /* For ranged combat. */
  struct obj_data *missile = NULL; /* For ranged combat. */

  if (!ch || !victim) return (HIT_MISS); /* ch and victim exist? */

  /* This is a bit of cruft from homeland code - It is used to activate a weapon 'special'
    under certain circumstances.  This could be refactored into something else, but it may
    actually be best to refactor the entire homeland 'specials' system and include it into
    weapon special abilities. */
  char *hit_msg = "";

  struct obj_data *wielded = get_wielded(ch, attack_type); /* Wielded weapon for this hand (uses offhand) */
  if (GET_EQ(ch, WEAR_WIELD_2H) && attack_type != ATTACK_TYPE_RANGED)
    attack_type = ATTACK_TYPE_TWOHAND;

  /* First - check the attack queue.  If we have a queued attack, dispatch!
    The attack queue should be more tightly integrated into the combat system.  Basically,
    if there is no attack queued up, we perform the default: A standard melee attack.
    The parameter 'penalty' allows an external procedure to call hit with a to hit modifier
    of some kind - This need not be a penalty, but can rather be a bonus (due to a charge or some
    other buff or situation.)  It is not hit()'s job to determine these bonuses.' */
  if(pending_attacks(ch)) {
    /* Dequeue the pending attack action.*/
    struct attack_action_data *attack = dequeue_attack(GET_ATTACK_QUEUE(ch));
    /* Execute the proper function pointer for that attack action. Notice the painfully bogus
      parameters.  Needs improvement. */
    ((*attack_actions[attack->attack_type]) (ch, attack->argument, -1, -1));
    /* Currently no way to get a result from these kinds of actions, so return something bogus.
      Needs improvement. */
    return -1;
  }

  /* if we come into the hit() function anticipating a ranged attack, we are
   examining obvious cases where the attack will fail */
  if (attack_type == ATTACK_TYPE_RANGED) {
    if (ammo_pouch)
      /* If we need a global variable to make some information available outside
       *  this function, then we might have a bit of an issue with the design.
       * Set the current missile to the first missile in the ammo pouch. */
      last_missile = missile = ammo_pouch->contains;
    if (!missile) { /* no ammo = miss for ranged attacks*/
      send_to_char(ch, "You have no ammo!\r\n");
      return (HIT_MISS);
    }
  }

  /* Activate any scripts on this mob OR PLAYER. */
  fight_mtrigger(ch); //fight trig
  /* If the mob can't fight, just return an automatic miss. */
  if (!MOB_CAN_FIGHT(ch)) { /* this mob can't hit */
    send_to_char(ch, "But you can't perform a normal melee attack routine!\r\n");
    return (HIT_MISS);
  }
  /* ^^ So this means that non fighting mobs with a fight trigger get their fight
    trigger executed if they are ever 'ch' in a run of perform_violence.  Ok.*/

  /* If this is a melee attack, check if the target and the aggressor are in
    the same room. */
  if (attack_type != ATTACK_TYPE_RANGED && IN_ROOM(ch) != IN_ROOM(victim)) {
    /* Not in the same room, so stop fighting. */
    if (FIGHTING(ch) && FIGHTING(ch) == victim) {
      stop_fighting(ch);
    }
    /* Automatic miss. */
    return (HIT_MISS);
  } else if (IN_ROOM(ch) == IN_ROOM(victim))
    same_room = TRUE; /* ch and victim are in the same room, great. */

  /* Make sure that ch and victim have an updated position. */
  update_pos(ch); update_pos(victim);

  /* If ch is dead (or worse) or victim is dead (or worse), return an automatic miss. */
  if (GET_POS(ch) <= POS_DEAD || GET_POS(victim) <= POS_DEAD) return (HIT_MISS);

  // added these two checks in case parry is successful on opening attack -zusuk
  if (ch->nr != real_mobile(DG_CASTER_PROXY) &&
          ch != victim && ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    stop_fighting(ch);
    return (HIT_MISS);
  }

  /* ranged attack and victim in peace room */
  if (ROOM_FLAGGED(IN_ROOM(victim), ROOM_PEACEFUL)) {
    send_to_char(ch, "That room just has such a peaceful, easy feeling...\r\n");
    stop_fighting(ch);
    return (HIT_MISS);
  }

  /* single file rooms restriction */
  if (!FIGHTING(ch)) {
    if (ROOM_FLAGGED(ch->in_room, ROOM_SINGLEFILE) &&
        (ch->next_in_room != victim && victim->next_in_room != ch))
      return (HIT_MISS);
  }

  /* Here is where we start fighting, if we were not fighting before. */
  if (victim != ch) {
    if (same_room && GET_POS(ch) > POS_STUNNED && (FIGHTING(ch) == NULL)) // ch -> vict
      set_fighting(ch, victim); /* Start fighting in one direction. */
    // vict -> ch
    if (GET_POS(victim) > POS_STUNNED && (FIGHTING(victim) == NULL)) {
      if (same_room)
        set_fighting(victim, ch); /* Start fighting in the other direction. */
      if (MOB_FLAGGED(victim, MOB_MEMORY) && !IS_NPC(ch))
        remember(victim, ch); /* If I am a mob with memory, remember the bastard. */
    }
  }

  /* At this point, ch should be fighting vict and vice versa, unless ch and vict
    are in different rooms and this is a ranged attack. */

  // determine weapon type, potentially a deprecated function
  w_type = determine_weapon_type(ch, victim, wielded, attack_type);

  /* some ranged attack handling */
  if (w_type == TYPE_MISSILE && attack_type == ATTACK_TYPE_RANGED) {
    /* This here, I don't think this makes a lot of sense. WE can have slashing arrows,
       blasting arrows, whatever.  As long as builders are sane, it should be ok. */
    w_type = GET_OBJ_VAL(missile, 3) + TYPE_HIT;
    // tag missile so that only this char collects it.
    MISSILE_ID(missile) = GET_IDNUM(ch);
    /* Remove the missile from the ammo_pouch. */
    obj_from_obj(missile);
  }

  /* Get the important numbers : ch's Attack bonus and victim's AC
   * attack rolls: 1 = stumble, 20 = hit, possible crit */
  victim_ac = compute_armor_class(ch, victim, FALSE);
  switch (attack_type) {
    case ATTACK_TYPE_OFFHAND: /* secondary or 'off' hand */
      if (w_type == TYPE_HIT)
        calc_bab += compute_attack_bonus(ch, victim, ATTACK_TYPE_UNARMED);
      else
        calc_bab += compute_attack_bonus(ch, victim, ATTACK_TYPE_OFFHAND);
      break;
    case ATTACK_TYPE_RANGED: /* ranged weapon */
      calc_bab += compute_attack_bonus(ch, victim, ATTACK_TYPE_RANGED);
      break;
    case ATTACK_TYPE_TWOHAND: /* two handed weapon */
      calc_bab += compute_attack_bonus(ch, victim, ATTACK_TYPE_TWOHAND);
      break;
    case ATTACK_TYPE_PRIMARY: /* primary hand and default */
    default:
      if (w_type == TYPE_HIT)
        calc_bab += compute_attack_bonus(ch, victim, ATTACK_TYPE_UNARMED);
      else
        calc_bab += compute_attack_bonus(ch, victim, ATTACK_TYPE_PRIMARY);
      break;
  }

  if (type == TYPE_ATTACK_OF_OPPORTUNITY) {
    if (HAS_FEAT(victim, FEAT_MOBILITY) && has_dex_bonus_to_ac(ch, victim))
      victim_ac += 4;
    if (HAS_FEAT(victim, FEAT_ENHANCED_MOBILITY) && has_dex_bonus_to_ac(ch, victim))
      victim_ac += 4;
    send_to_char(ch, "\tW[\tRAOO\tW]\tn");
    send_to_char(victim, "\tW[\tRAOO\tW]\tn");
  }

  diceroll = rand_number(1, 20);
  if (is_critical_hit(ch, wielded, diceroll, calc_bab, victim_ac)) {
    dam = TRUE;
    hit_msg = "critical";  //for weapon procs, from homeland
    is_critical = TRUE;
  } else if (diceroll == 20) { /*auto hit, not critical though*/
    dam = TRUE;
  } else if (!AWAKE(victim)) {
    send_to_char(ch, "\tW[down!]\tn");
    send_to_char(victim, "\tR[down!]\tn");
    dam = TRUE;
  } else if (diceroll == 1) {
    send_to_char(ch, "[stum!]");
    send_to_char(victim, "[stum!]");
    dam = FALSE;
  } else {
    sprintf(buf1, "\tW[R: %2d]\tn", diceroll);
    sprintf(buf, "%7s", buf1);
    send_to_char(ch, buf);
    sprintf(buf1, "\tR[R: %2d]\tn", diceroll);
    sprintf(buf, "%7s", buf1);
    send_to_char(victim, buf);
    dam = (calc_bab + diceroll >= victim_ac);
    /*  leaving this around for debugging
    send_to_char(ch, "\tc{T:%d+", calc_bab);
    send_to_char(ch, "D:%d>=", diceroll);
    send_to_char(ch, "AC:%d}\tn", victim_ac);
    */
  }

  /* Parry calculation -
   * This only applies if the victim is in parry mode and is based on
   * the parry 'skill'.  You can not parry if you are casting and you have
   * a number of parry attempts equal to the attacks granted by your BAB. */
  int parryDC = calc_bab + diceroll;
  int parry_attempt = 0;
  if (!IS_NPC(victim) &&
       compute_ability(victim, ABILITY_PARRY) &&
       PARRY_LEFT(victim) &&
       AFF_FLAGGED(victim, AFF_PARRY) &&
       !IS_CASTING(victim) &&
       GET_POS(victim) >= POS_SITTING &&
       attack_type != ATTACK_TYPE_RANGED) {

    /* -2 penalty to parry attempts if you are sitting, basically.  You will never ever
     * get here if you are in a lower position than sitting, so the 'less than' is
     * redundant. */
    if (GET_POS(victim) <= POS_SITTING)
      parry_attempt -= 2;

    if (!(parry_attempt = skill_check(victim, ABILITY_PARRY, parryDC))) {
      send_to_char(victim, "You failed to \tcparry\tn the attack from %s!  ",
              GET_NAME(ch));
    } else if (parry_attempt >= 10) {
      /* We can riposte, as the 'skill check' was 10 or more higher than the DC. */
      send_to_char(victim, "You deftly \tcriposte the attack\tn from %s!\r\n",
              GET_NAME(ch));
      send_to_char(ch, "%s \tCparries\tn your attack and counterattacks!\r\n",
              GET_NAME(victim));
      act("$N \tDripostes\tn an attack from $n!", FALSE, ch, 0, victim,
              TO_NOTVICT);

      /* Encapsulate this?  We need better control of 'hit()s' */
      hit(victim, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, ATTACK_TYPE_PRIMARY);
      PARRY_LEFT(victim)--;
      return (HIT_MISS);
    } else {
      send_to_char(victim, "You \tcparry\tn the attack from %s!\r\n",
              GET_NAME(ch));
      send_to_char(ch, "%s \tCparries\tn your attack!\r\n", GET_NAME(victim));
      act("$N \tDparries\tn an attack from $n!", FALSE, ch, 0, victim,
              TO_NOTVICT);
      PARRY_LEFT(victim)--;
      return (HIT_MISS);
    }
  } /* End of parry */

  /* Once per round when your mount is hit in combat, you may attempt a Ride
   * check (as an immediate action) to negate the hit. The hit is negated if
   * your Ride check result is greater than the opponent's attack roll.*/
  if (RIDING(victim) && HAS_FEAT(victim, FEAT_MOUNTED_COMBAT) &&
          MOUNTED_BLOCKS_LEFT(victim) > 0) {
    int mounted_block_dc = calc_bab + diceroll;
    int mounted_block_bonus = compute_ability(victim, ABILITY_RIDE) + dice(1, 20);
    if (mounted_block_dc <= mounted_block_bonus) {
      send_to_char(victim, "You \tcmaneuver %s to block\tn the attack from %s!\r\n",
              GET_NAME(RIDING(victim)), GET_NAME(ch));
      send_to_char(ch, "%s \tCmaneuvers %s to block\tn your attack!\r\n", GET_NAME(victim), GET_NAME(RIDING(victim)));
      act("$N \tDmaneuvers $S mount to block\tn an attack from $n!", FALSE, ch, 0, victim,
              TO_NOTVICT);
      MOUNTED_BLOCKS_LEFT(victim)--;
      return (HIT_MISS);
    }
  } /* end mounted combat check */

  /* So if we have actually hit, then dam > 0.  This is how we process a miss. */
  if (!dam) { //miss

    /* stunning fist, quivering palm, etc need to be expended even if you miss */
    if (affected_by_spell(ch, SKILL_STUNNING_FIST)) {
      send_to_char(ch, "You fail to land your stunning fist attack!  ");
      affect_from_char(ch, SKILL_STUNNING_FIST);
    }
    if (affected_by_spell(ch, SKILL_QUIVERING_PALM)) {
      send_to_char(ch, "You fail to land your quivering palm attack!  ");
      affect_from_char(ch, SKILL_QUIVERING_PALM);
    }
    if (affected_by_spell(ch, SKILL_SUPRISE_ACCURACY)) {
      send_to_char(ch, "You fail to land your suprise accuracy attack!  ");
      affect_from_char(ch, SKILL_SUPRISE_ACCURACY);
    }
    if (affected_by_spell(ch, SKILL_POWERFUL_BLOW)) {
      send_to_char(ch, "You fail to land your powerful blow!  ");
      affect_from_char(ch, SKILL_POWERFUL_BLOW);
    }

    /* Display the flavorful backstab miss messages. This should be changed so we can
     * get rid of the SKILL_ defined (and convert abilities to skills :)) */
    damage(ch, victim, 0, type == SKILL_BACKSTAB ? SKILL_BACKSTAB : w_type,
            dam_type, attack_type);

    /* Ranged miss */
    if (attack_type == ATTACK_TYPE_RANGED)
      /* Drop the ammo to the ground.  Can add a breakage % chance here as well. */
      obj_to_room(missile, IN_ROOM(victim));
    else {
      /* non ranged weapon can possibly set off shield block proc (for now)
       * There really should be some mechanic to determine when the shield is used,
       * for example if the attack is a miss, but the attack roll would be a hit
       * vs AC - shield bonus, then it is a shield block, and we can print the shield
       * block message, trigger procs, etc. */
      struct obj_data *shield = GET_EQ(victim, WEAR_SHIELD);

      if (shield) {

      /* Maybe it would be better to make a typedef for this...*/
      int (*name)(struct char_data *victim, void *me, int cmd, char *argument);

        name = obj_index[GET_OBJ_RNUM(shield)].func;

        if (name) {
          /* currently no actual shield block, so no shield block message */
          //damage(ch, victim, 0, SKILL_SHIELD_BLOCK, DAMBIT_PHYSICAL);
          if ((name) (victim, shield, 0, ""))
            ;
        }
      }
    }

  } else { /* OK, attack should be a success at this stage */
    /* Print descriptive tags - This needs some form of control, via a toggle
     * and also should be formatted in some standard way with standard colors.
     * This section also implement the effects of stunning fist, smite and true strike,
     * reccomended: to be moved outta here and put into their own attack
     * routines, then called as an attack action. */
    if (affected_by_spell(ch, SPELL_TRUE_STRIKE)) {
      send_to_char(ch, "[\tWTRUE-STRIKE\tn] ");
      affect_from_char(ch, SPELL_TRUE_STRIKE);
    }
    /* rage powers */
    if (affected_by_spell(ch, SKILL_SUPRISE_ACCURACY)) {
      send_to_char(ch, "[\tWSUPRISE_ACCURACY\tn] ");
      affect_from_char(ch, SKILL_SUPRISE_ACCURACY);
    }
    int powerful_blow_bonus = 0;
    if (affected_by_spell(ch, SKILL_POWERFUL_BLOW)) {
      send_to_char(ch, "[\tWPOWERFUL_BLOW\tn] ");
      affect_from_char(ch, SKILL_POWERFUL_BLOW);
      powerful_blow_bonus += CLASS_LEVEL(ch, CLASS_BERSERKER) / 4 + 1;
      /* what is this?  because we are removing the affect, it won't
       be calculated properly in damage_bonus, so we just tag it on afterwards */
    }
    if (affected_by_spell(ch, SKILL_SMITE)) {
      if (IS_EVIL(victim)) {
        send_to_char(ch, "[SMITE] ");
        send_to_char(victim, "[\tRSMITE\tn] ");
        act("$n performs a \tYsmiting\tn attack on $N!",
                  FALSE, ch, wielded, victim, TO_NOTVICT);
      }
    }
    if (affected_by_spell(ch, SKILL_STUNNING_FIST)) {
      if(!wielded || (OBJ_FLAGGED(wielded, ITEM_KI_FOCUS)) || (weapon_list[GET_WEAPON_TYPE(wielded)].weaponFamily == WEAPON_FAMILY_MONK)) {
        send_to_char(ch, "[STUNNING-FIST] ");
        send_to_char(victim, "[\tRSTUNNING-FIST\tn] ");
        act("$n performs a \tYstunning fist\tn attack on $N!",
                  FALSE, ch, wielded, victim, TO_NOTVICT);
        if (!char_has_mud_event(victim, eSTUNNED)) {
          attach_mud_event(new_mud_event(eSTUNNED, victim, NULL), 6 * PASSES_PER_SEC);
        }
        affect_from_char(ch, SKILL_STUNNING_FIST);
      }
    }
    if (affected_by_spell(ch, SKILL_QUIVERING_PALM)) {
      int quivering_palm_dc = 10 + (CLASS_LEVEL(ch, CLASS_MONK) / 2) + GET_WIS_BONUS(ch);
      if(!wielded || (OBJ_FLAGGED(wielded, ITEM_KI_FOCUS)) || (weapon_list[GET_WEAPON_TYPE(wielded)].weaponFamily == WEAPON_FAMILY_MONK)) {
        send_to_char(ch, "[QUIVERING-PALM] ");
        send_to_char(victim, "[\tRQUIVERING-PALM\tn] ");
        act("$n performs a \tYquivering palm\tn attack on $N!",
                  FALSE, ch, wielded, victim, TO_NOTVICT);
        /* apply quivering palm affect, muahahahah */
        if (GET_LEVEL(ch) >= GET_LEVEL(victim) &&
                !savingthrow(victim, SAVING_FORT, 0, quivering_palm_dc)) {
          /*GRAND SLAM!*/
          act("$N \tRblows up into little pieces\tn as soon as you make contact with your palm!",
                  FALSE, ch, wielded, victim, TO_CHAR);
          act("You feel your body \tRblow up in to little pieces\tn as $n touches you!",
                  FALSE, ch, wielded, victim, TO_VICT | TO_SLEEP);
          act("You watch as $N's body gets \tRblown into little pieces\tn from a single touch from $n!",
                  FALSE, ch, wielded, victim, TO_NOTVICT);
          dam_killed_vict(ch, victim);
          /* ok, now remove quivering palm */
          affect_from_char(ch, SKILL_QUIVERING_PALM);
          return 0;
        } else { /* quivering palm will still do damage */
          dam += 1 + GET_WIS_BONUS(ch);
        }
        /* ok, now remove quivering palm */
        affect_from_char(ch, SKILL_QUIVERING_PALM);
      }
    }

    /* Calculate sneak attack damage. */
    if (HAS_FEAT(ch, FEAT_SNEAK_ATTACK) &&
        (compute_concealment(victim) == 0) &&
       ((AFF_FLAGGED(victim, AFF_FLAT_FOOTED))  /* Flat-footed */
          || !(has_dex_bonus_to_ac(ch, victim)) /* No dex bonus to ac */
          || is_flanked(ch, victim)             /* Flanked */
       )) {

      /* Display why we are sneak attacking */
      send_to_char(ch, "[");
      if (AFF_FLAGGED(victim, AFF_FLAT_FOOTED))
        send_to_char(ch, "FF");
      if (!has_dex_bonus_to_ac(ch, victim))
        send_to_char(ch,"Dx");
      if (is_flanked(ch, victim))
        send_to_char(ch, "Fk");
      send_to_char(ch, "]");

      sneakdam = dice(HAS_FEAT(ch, FEAT_SNEAK_ATTACK), 6);

      if (sneakdam) {
        send_to_char(ch, "[\tDSNEAK\tn] ");
      }
    }

    /* Calculate damage for this hit */
    dam = compute_hit_damage(ch, victim, w_type, diceroll, 0,
                             is_critical, attack_type);
    dam += powerful_blow_bonus; /* ornir is going to yell at me for this :p  -zusuk */

    /* This comes after computing the other damage since sneak attack damage
     * is not affected by crit multipliers. */
    dam += sneakdam;

    /* Melee warding modifies damage. */
    //if ((dam = handle_warding(ch, victim, dam)) == -1)
    //  return (HIT_MISS);

    /* Apply Damage Reduction */
    if ((dam = apply_damage_reduction(ch, victim, wielded, dam)) == -1)
      return (HIT_MISS); /* This should be changed to something more reasonable */

    /* ok we are about to do damage() so here we are adding a special counter-attack
       for berserkers that is suppose to fire BEFORE damage is done to vict */
    if (ch != victim &&
          affected_by_spell(victim, SKILL_COME_AND_GET_ME) &&
          affected_by_spell(victim, SKILL_RAGE)) {
      GET_TOTAL_AOO(victim)--; /* free aoo and will be incremented in the function */
      attack_of_opportunity(victim, ch, 0);

      /* dummy check */
      update_pos(ch);
      if (GET_POS(ch) <= POS_INCAP)
        return (HIT_MISS);
    }
    /***** end counter attacks ******/

    /* if the 'type' of hit() requires special handling, do it here */
    switch (type) {
      /* More SKILL_ garbage - This needs a better mechanic.  */
      case SKILL_BACKSTAB:
        /* What a horrible hack.  Backstab should pretty much go away anyway, and crippling strike
         * needs a new place to live. */
        damage(ch, victim, sneakdam + (dam - sneakdam) * backstab_mult(ch),
                SKILL_BACKSTAB, dam_type, attack_type);

        break;
      default:
        /* Here we manage the racial specials, Trelux have claws and can not use weapons. */
        if (GET_RACE(ch) == RACE_TRELUX)
          damage(ch, victim, dam, TYPE_CLAW, dam_type, attack_type);
        else {
          /* We hit with a ranged weapon, victim gets a new arrow, stuck neatly in his butt. */
          if (attack_type == ATTACK_TYPE_RANGED) {
            obj_to_char(missile, victim);
          }
          /* charging combat maneuver */
          if (AFF_FLAGGED(ch, AFF_CHARGING)) {
            send_to_char(ch, "You \tYcharge\tn: ");
            send_to_char(victim, "%s \tYcharges\tn toward you: ", GET_NAME(ch));
            act("$n \tYcharges\tn toward $N!", FALSE, ch, NULL, victim, TO_NOTVICT);
          }
          /* So do damage! (We aren't trelux, so do it normally) */
          damage(ch, victim, dam, w_type, dam_type, attack_type);

          if (AFF_FLAGGED(ch, AFF_CHARGING)) { /* only a single strike */
            affect_from_char(ch, SKILL_CHARGE);
          }
        }
        break;
    }

    /* 20% chance to poison as a trelux. This could be made part of the general poison code, once that is
     * implemented, also, shouldn't they be able to control if they poison or not?  Why not make them envenom
     * their claws before an attack? */
    if ( (GET_POS(victim) != POS_DEAD) && GET_RACE(ch) == RACE_TRELUX && !IS_AFFECTED(victim, AFF_POISON)
            && !rand_number(0, 5)) {
      /* We are just using the poison spell for this...Maybe there would be a better way, some unique poison?
       * Note the CAST_INNATE, this removes armor spell failure from the call. */
      call_magic(ch, FIGHTING(ch), 0, SPELL_POISON, GET_LEVEL(ch), CAST_INNATE);
    }

    /* crippling strike */
    if (sneakdam) {
      if (dam && (GET_POS(victim) != POS_DEAD) && HAS_FEAT(ch, FEAT_CRIPPLING_STRIKE) &&
              !affected_by_spell(victim, SKILL_CRIP_STRIKE)) {

        new_affect(&af);
        af.spell = SKILL_CRIP_STRIKE;
        af.duration = 10;
        af.location = APPLY_STR;
        af.modifier = -(dice(2, 4));
        affect_to_char(victim, &af);

        act("Your well placed attack \tTcripples\tn $N!",
                FALSE, ch, wielded, victim, TO_CHAR);
        act("A well placed attack from $n \tTcripples\tn you!",
                FALSE, ch, wielded, victim, TO_VICT | TO_SLEEP);
        act("A well placed attack from $n \tTcripples\tn $N!",
                FALSE, ch, wielded, victim, TO_NOTVICT);
      }
    }

    /* weapon spells - deprecated, although many weapons still have these.  Weapon Special Abilities supercede
     * this implementation. */
    if (ch && victim && wielded)
      weapon_spells(ch, victim, wielded);

    /* Weapon special abilities that trigger on hit. */
    if (ch && victim && wielded)
      process_weapon_abilities(wielded, ch, victim, ACTMTD_ON_HIT, NULL);

    /* our primitive weapon-poison system, needs some love */
    weapon_poison(ch, victim, wielded);

    /* special weapon (or gloves for monk) procedures.  Need to implement something similar for the new system. */
    if (ch && victim && wielded)
      weapon_special(wielded, ch, hit_msg);
    else if (ch && victim && GET_EQ(ch, WEAR_HANDS))
      weapon_special(GET_EQ(ch, WEAR_HANDS), ch, hit_msg);

    /* vampiric curse will do some minor healing to attacker */
    if (!IS_UNDEAD(victim) && IS_AFFECTED(victim, AFF_VAMPIRIC_CURSE)) {
      send_to_char(ch, "\tWYou feel slightly better as you land an attack!\r\n");
      GET_HIT(ch) += MIN(GET_MAX_HIT(ch) - GET_HIT(ch), dice(1, 10));
    }

    /* vampiric touch will do some healing to attacker */
    if (dam > 0 && !IS_UNDEAD(victim) && IS_AFFECTED(ch, AFF_VAMPIRIC_TOUCH)) {
      send_to_char(ch, "\tWYou feel \tRvampiric\tn \tWenergy heal you as you "
              "land an attack!\r\n");
      GET_HIT(ch) += MIN(GET_MAX_HIT(ch) - GET_HIT(ch), dam);
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_VAMPIRIC_TOUCH);
    }

    // damage inflicting shields, like fire shield
    if (attack_type != ATTACK_TYPE_RANGED) {
      if (dam && victim && GET_HIT(victim) >= -1 &&
              IS_AFFECTED(victim, AFF_CSHIELD)) { // cold shield
        damage(victim, ch, dice(1, 6), SPELL_CSHIELD_DAM, DAM_COLD, attack_type);
      } else if (dam && victim && GET_HIT(victim) >= -1 &&
              IS_AFFECTED(victim, AFF_FSHIELD)) { // fire shield
        damage(victim, ch, dice(1, 6), SPELL_FSHIELD_DAM, DAM_FIRE, attack_type);
      } else if (dam && victim && GET_HIT(victim) >= -1 &&
              IS_AFFECTED(victim, AFF_ASHIELD)) { // acid shield
        damage(victim, ch, dice(2, 6), SPELL_ASHIELD_DAM, DAM_ACID, attack_type);
      }
    }
  } /* end damage bracket */

  hitprcnt_mtrigger(victim); //hitprcnt trigger

  return dam;
}
#undef HIT_MISS

/* ch dual wielding or is trelux */
int is_dual_wielding(struct char_data *ch) {

  if (GET_EQ(ch, WEAR_WIELD_2) || GET_RACE(ch) == RACE_TRELUX)
    return TRUE;

  return FALSE;
}

#define MODE_2_WPN       1 /* two weapon fighting equivalent (reduce two weapon fighting penalty) */
#define MODE_IMP_2_WPN   2 /* improved two weapon fighting - extra attack at -5 */
#define MODE_GREAT_2_WPN 3 /* greater two weapon fighting - extra attack at -10 */
#define MODE_EPIC_2_WPN  4 /* perfect two weapon fighting - extra attack */
int is_skilled_dualer(struct char_data *ch, int mode) {
  switch (mode) {
    case MODE_2_WPN:
      if (IS_NPC(ch))
        return TRUE;
      else if (!IS_NPC(ch) && (HAS_FEAT(ch, FEAT_TWO_WEAPON_FIGHTING) ||
                 (proficiency_worn(ch, ARMOR_PROFICIENCY) <= ITEM_PROF_LIGHT_A &&
                  HAS_FEAT(ch, FEAT_DUAL_WEAPON_FIGHTING)))
               ) {
        return TRUE;
      } else
        return FALSE;
    case MODE_IMP_2_WPN:
      if (IS_NPC(ch) && (GET_CLASS(ch) == CLASS_RANGER || GET_CLASS(ch) == CLASS_ROGUE))
        return TRUE;
      else if (!IS_NPC(ch) && (HAS_FEAT(ch, FEAT_IMPROVED_TWO_WEAPON_FIGHTING) ||
                 (proficiency_worn(ch, ARMOR_PROFICIENCY) <= ITEM_PROF_LIGHT_A &&
                  HAS_FEAT(ch, FEAT_IMPROVED_DUAL_WEAPON_FIGHTING)))
               ) {
        return TRUE;
      } else
        return FALSE;
    case MODE_GREAT_2_WPN:
      if (IS_NPC(ch) && GET_LEVEL(ch) >= 17 && (GET_CLASS(ch) == CLASS_RANGER || GET_CLASS(ch) == CLASS_ROGUE))
        return TRUE;
      else if (!IS_NPC(ch) && (HAS_FEAT(ch, FEAT_GREATER_TWO_WEAPON_FIGHTING) ||
                 (proficiency_worn(ch, ARMOR_PROFICIENCY) <= ITEM_PROF_LIGHT_A &&
                  HAS_FEAT(ch, FEAT_GREATER_DUAL_WEAPON_FIGHTING)))
               ) {
        return TRUE;
      } else
        return FALSE;
    case MODE_EPIC_2_WPN:
      if (IS_NPC(ch) && GET_LEVEL(ch) >= 24 && (GET_CLASS(ch) == CLASS_RANGER || GET_CLASS(ch) == CLASS_ROGUE))
        return TRUE;
      else if (!IS_NPC(ch) && (HAS_FEAT(ch, FEAT_PERFECT_TWO_WEAPON_FIGHTING) ||
                 (proficiency_worn(ch, ARMOR_PROFICIENCY) <= ITEM_PROF_LIGHT_A &&
                  HAS_FEAT(ch, FEAT_PERFECT_DUAL_WEAPON_FIGHTING)))
               ) {
        return TRUE;
      } else
        return FALSE;
  }

  log("ERR: is_skilled_dualer() reached end!");
  return 0;
}

/* common dummy check in perform_attacks() */
int valid_fight_cond(struct char_data *ch) {

  if (FIGHTING(ch))
    if (GET_POS(FIGHTING(ch)) != POS_DEAD &&
        IN_ROOM(FIGHTING(ch)) == IN_ROOM(ch))
      return TRUE;

  return FALSE;
}

/* returns # of attacks and has mode functionality */
#define ATTACK_CAP	3  /* MAX # of main-hand BONUS attacks */
#define MONK_CAP	(ATTACK_CAP + 2) /* monks main-hand bonus attack cap */
#define TWO_WPN_PNLTY	-5 /* improved two weapon fighting */
#define GREAT_TWO_PNLY	-10 /* greater two weapon fighting */
#define EPIC_TWO_PNLTY   0 /* perfect two weapon fighting */
/* mode functionality */
#define NORMAL_ATTACK_ROUTINE 0 /*mode = 0  normal attack routine*/
#define RETURN_NUM_ATTACKS 1 /*mode = 1  return # of attacks, nothing else*/
#define DISPLAY_ROUTINE_POTENTIAL 2 /*mode = 2  display attack routine potential*/
/* Phase determines what part of the combat round we are currently
 * in.  Each combat round is broken up into 3 2-second phases.
 * Attacks are split among the phases (for a full-round-action)
 * in the following pattern:
 * For 1 attack:
 *   Phase 1 - Attack once.
 *   Phase 2 - Do Nothing.
 *   Phase 3 - Do Nothing.
 * For 2 attacks:
 *   Phase 1 - Attack once.
 *   Phase 2 - Attack once.
 *   Phase 3 - Do Nothing.
 * For 4 attacks:
 *   Phase 1 - Attack twice.
 *   Phase 2 - Attack once.
 *   Phase 3 - Attack once.  ...ETC... */
#define PHASE_0 0
#define PHASE_1 1
#define PHASE_2 2
#define PHASE_3 3
int perform_attacks(struct char_data *ch, int mode, int phase) {
  int i = 0, penalty = 0, numAttacks = 0, bonusAttacks = 0;
  int attacks_at_max_bab = 0;
  int ranged_attacks = 2; /* ranged combat gets 2 bonus attacks currently */
  bool dual = FALSE;
  bool perform_attack = FALSE;

  /* Check position..  we don't check < POS_STUNNED anymore? */
  if (GET_POS(ch) == POS_DEAD)
    return(0);

  /*  If we have no standard action (and are using regular attack mode.)
   *  Do not attack at all. If we have no move action (and are in regular
   *  attack mode) skip all phases but the first. */
  if ((mode == NORMAL_ATTACK_ROUTINE) && !is_action_available(ch, atSTANDARD, FALSE))
    return (0);
  else if ((mode == NORMAL_ATTACK_ROUTINE) && (phase != PHASE_1) && !is_action_available(ch, atMOVE, FALSE))
    return (0);

  guard_check(ch, FIGHTING(ch)); /* this is the guard skill check */

  /* level based bonus attacks, which is BAB / 5 up to the ATTACK_CAP [note might need to add armor restrictions here?] */
  bonusAttacks = MIN((BAB(ch) - 1) / 5, ATTACK_CAP);

  /* monk flurry of blows */
  if (CLASS_LEVEL(ch, CLASS_MONK) && monk_gear_ok(ch) &&
          AFF_FLAGGED(ch, AFF_FLURRY_OF_BLOWS)) {
    bonusAttacks++;
    attacks_at_max_bab++;
    if (CLASS_LEVEL(ch, CLASS_MONK) < 5)
      penalty = -2; /* flurry penalty */
    else if (CLASS_LEVEL(ch, CLASS_MONK) < 9)
      penalty = -1; /* 9th level+, no more penalty to flurry! */
    if (HAS_FEAT(ch, FEAT_GREATER_FLURRY)) { /* FEAT_GREATER_FLURRY, 11th level */
      bonusAttacks++;
      attacks_at_max_bab++;
      if (CLASS_LEVEL(ch, CLASS_MONK) >= 15) {
        bonusAttacks++;
        attacks_at_max_bab++;
      }
    }
  }

  /* Haste gives one extra attack, ranged or melee, at max BAB. */
  if (AFF_FLAGGED(ch, AFF_HASTE)) {
    ranged_attacks++;
    attacks_at_max_bab++;
  }

  /* Process ranged attacks ------------------------------------------------- */
  int drop_an_attack_at_max_bab = 0;
  /* ranged get extra attacks */
  ranged_attacks += bonusAttacks;
  /* Rapidshot mode gives an extra attack, but with a penalty to all attacks. */
  if (AFF_FLAGGED(ch, AFF_RAPID_SHOT)) {
    penalty -= 2;
    ranged_attacks++;
    attacks_at_max_bab++; /* we have to drop this if this isn't a ranged attack! */
    drop_an_attack_at_max_bab++;

    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_MANYSHOT)) {
      ranged_attacks++;
      attacks_at_max_bab++; /* we have to drop this if this isn't a ranged attack! */
      drop_an_attack_at_max_bab++;
    }
    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_EPIC_MANYSHOT)) {
      ranged_attacks++;
      attacks_at_max_bab++; /* we have to drop this if this isn't a ranged attack! */
      drop_an_attack_at_max_bab++;
    }
  }

  if (FIRING(ch) && mode == NORMAL_ATTACK_ROUTINE) {
    if (is_tanking(ch)) {
      if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_IMPROVED_PRECISE_SHOT))
        penalty += 4;
      else if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_PRECISE_SHOT))
        ;
      else /* not skilled with close combat archery */
        penalty -= 4;

      if (!IS_NPC(ch) && !HAS_FEAT(ch, FEAT_POINT_BLANK_SHOT)) {
        send_to_char(ch, "You are too close to use your ranged weapon.\r\n");
        stop_fighting(ch);
        FIRING(ch) = FALSE;
        return 0;
      }
    }

    if (RIDING(ch) && !IS_NPC(ch) && !HAS_FEAT(ch, FEAT_MOUNTED_ARCHERY))
      penalty -= 4;

    /* FIRE! */
    for (i = 0; i < ranged_attacks; i++) {
      /* phase 1: 1 4 7 10 13
       * phase 2: 2 5 8 11 14
       * phase 3: 3 6 9 12 15 */
      perform_attack = FALSE;
      switch (i) {
        case 1: case 4: case 7: case 10: case 13:
          if (phase == PHASE_0 || phase == PHASE_1 ) {
            perform_attack = TRUE;
          }
          break;
        case 2: case 5: case 8: case 11: case 14:
          if (phase == PHASE_0 || phase == PHASE_2 ) {
            perform_attack = TRUE;
          }
          break;
        case 3: case 6: case 9: case 12: case 15:
          if (phase == PHASE_0 || phase == PHASE_3 ) {
            perform_attack = TRUE;
          }
          break;
      }

      if (perform_attack) {
        if (can_fire_arrow(ch, FALSE) && FIGHTING(ch)) {
          hit(ch, FIGHTING(ch), TYPE_UNDEFINED, DAM_RESERVED_DBC, /* FIRE! */
                penalty, ATTACK_TYPE_RANGED);
          if (attacks_at_max_bab > 0)
            attacks_at_max_bab--;
          else
            penalty -= 3;
        } else if (FIGHTING(ch)) {
          send_to_char(ch, "\tWYou are out of ammunition!\tn\r\n");
          stop_fighting(ch);
          return 0;
        } else
          stop_fighting(ch);
      }
    }
    return 0;

    /* Display Modes */
  } else if (mode == RETURN_NUM_ATTACKS && can_fire_arrow(ch, TRUE)) {
    return ranged_attacks;
  } else if (mode == DISPLAY_ROUTINE_POTENTIAL && can_fire_arrow(ch, TRUE)) {
    while (ranged_attacks > 0) {
      send_to_char(ch, "Ranged Attack Bonus:  %d; ",
                   compute_attack_bonus(ch, ch, ATTACK_TYPE_RANGED) + penalty);
      compute_hit_damage(ch, ch, TYPE_UNDEFINED_WTYPE, NO_DICEROLL, MODE_DISPLAY_RANGED, FALSE, ATTACK_TYPE_RANGED); /* display damage bonus */

      if(attacks_at_max_bab > 0)
        attacks_at_max_bab--;
      else
        penalty -= 3;

      ranged_attacks--;
    }
    return 0;
  }
  if (drop_an_attack_at_max_bab)
    attacks_at_max_bab -= drop_an_attack_at_max_bab;
  /*  End ranged attacks ---------------------------------------------------- */


  //melee: now lets determine base attack(s) and resulting possible penalty
  dual = is_dual_wielding(ch); // trelux or has off-hander equipped

  if (dual) { /*default of one offhand attack for everyone*/
    numAttacks += 2; /* mainhand + offhand */
    if (GET_EQ(ch, WEAR_WIELD_2)) { /* determine if offhand is smaller than ch */
      if (GET_SIZE(ch) <= GET_OBJ_SIZE(GET_EQ(ch, WEAR_WIELD_2)))
        penalty -= 4; /* offhand weapon is not light! */
    }
    if (is_skilled_dualer(ch, MODE_2_WPN)) /* two weapon fighting feat? */
      penalty -= 2; /* yep! */
    else /* nope! */
      penalty -= 4;
    if (mode == NORMAL_ATTACK_ROUTINE) { //normal attack routine
      if (valid_fight_cond(ch))
        if (phase == PHASE_0 || phase == PHASE_1)
          hit(ch, FIGHTING(ch), TYPE_UNDEFINED, DAM_RESERVED_DBC,
                penalty, FALSE); /* whack with mainhand */
        if (valid_fight_cond(ch))
          if (phase == PHASE_0 || phase == PHASE_2)
            hit(ch, FIGHTING(ch), TYPE_UNDEFINED, DAM_RESERVED_DBC,
                  penalty * 2, TRUE); /* whack with offhand */
        //display attack routine
    } else if (mode == DISPLAY_ROUTINE_POTENTIAL) {
      send_to_char(ch, "Mainhand, Attack Bonus:  %d; ",
                   compute_attack_bonus(ch, ch, ATTACK_TYPE_PRIMARY) + penalty);
      compute_hit_damage(ch, ch, TYPE_UNDEFINED_WTYPE, NO_DICEROLL, MODE_DISPLAY_PRIMARY, FALSE, ATTACK_TYPE_PRIMARY); /* display damage bonus */
      send_to_char(ch, "Offhand, Attack Bonus:  %d; ",
                   compute_attack_bonus(ch, ch, ATTACK_TYPE_OFFHAND) + penalty * 2);
      compute_hit_damage(ch, ch, TYPE_UNDEFINED_WTYPE, NO_DICEROLL, MODE_DISPLAY_OFFHAND, FALSE, ATTACK_TYPE_OFFHAND); /* display damage bonus */
    }
  } else {

    // not dual wielding
    numAttacks++;  //default of one attack for everyone
    if (mode == NORMAL_ATTACK_ROUTINE) { //normal attack routine
      if (valid_fight_cond(ch))
        if (phase == PHASE_0 || phase == PHASE_1)
          hit(ch, FIGHTING(ch), TYPE_UNDEFINED, DAM_RESERVED_DBC, penalty, FALSE);
    } else if (mode == DISPLAY_ROUTINE_POTENTIAL) { //display attack routine
      send_to_char(ch, "Mainhand, Attack Bonus:  %d; ",
                   compute_attack_bonus(ch, ch, ATTACK_TYPE_PRIMARY) + penalty);
      compute_hit_damage(ch, ch, TYPE_UNDEFINED_WTYPE, NO_DICEROLL, MODE_DISPLAY_PRIMARY, FALSE, ATTACK_TYPE_PRIMARY);
    }
  }


  /* haste or equivalent? */
  if (AFF_FLAGGED(ch, AFF_HASTE) ||
          (!IS_NPC(ch) && GET_SKILL(ch, SKILL_BLINDING_SPEED))) {
    numAttacks++;
    if (mode == NORMAL_ATTACK_ROUTINE) { //normal attack routine
      attacks_at_max_bab--;
      if (valid_fight_cond(ch))
        if (phase == PHASE_0 || ((phase == PHASE_2) && numAttacks == 2) || ((phase == PHASE_3) && numAttacks == 3))
          hit(ch, FIGHTING(ch), TYPE_UNDEFINED, DAM_RESERVED_DBC, penalty, FALSE);
    }

    else if (mode == DISPLAY_ROUTINE_POTENTIAL) { //display attack routine
      send_to_char(ch, "Mainhand (Haste), Attack Bonus:  %d; ",
                   compute_attack_bonus(ch, ch, ATTACK_TYPE_PRIMARY) + penalty);
      compute_hit_damage(ch, ch, TYPE_UNDEFINED_WTYPE, NO_DICEROLL, MODE_DISPLAY_PRIMARY, FALSE, ATTACK_TYPE_PRIMARY);
    }
  }


  //execute the calculated attacks from above
  int j = 0;
  for (i = 0; i < bonusAttacks; i++) {
    numAttacks++;
    j = numAttacks + i;
    perform_attack = FALSE;
    switch (j) {
      case 1: case 4: case 7: case 10: case 13:
        if (phase == PHASE_0 || phase == PHASE_1 ) {
          perform_attack = TRUE;
        }
        break;
      case 2: case 5: case 8: case 11: case 14:
        if (phase == PHASE_0 || phase == PHASE_2 ) {
          perform_attack = TRUE;
        }
        break;
      case 3: case 6: case 9: case 12: case 15:
        if (phase == PHASE_0 || phase == PHASE_3 ) {
          perform_attack = TRUE;
        }
        break;
    }
    if (perform_attack) {
      if(attacks_at_max_bab > 0)
        attacks_at_max_bab--; /* like monks flurry */
      else
        penalty -= 5;      /* everyone gets -5 penalty per bonus attack by mainhand */

      if (FIGHTING(ch) && mode == NORMAL_ATTACK_ROUTINE) { //normal attack routine
        update_pos(FIGHTING(ch));
        if (valid_fight_cond(ch)) {
          hit(ch, FIGHTING(ch), TYPE_UNDEFINED, DAM_RESERVED_DBC, penalty, FALSE);
        }
      }
    }

    /* Display attack routine. */
    if (mode == DISPLAY_ROUTINE_POTENTIAL) {
      send_to_char(ch, "Mainhand Bonus %d, Attack Bonus:  %d; ",
                   i + 1,
                   compute_attack_bonus(ch, ch, ATTACK_TYPE_PRIMARY) + penalty);
      compute_hit_damage(ch, ch, TYPE_UNDEFINED_WTYPE, NO_DICEROLL, MODE_DISPLAY_PRIMARY, FALSE, ATTACK_TYPE_PRIMARY);
    }
  }


  /*additional off-hand attacks*/
  if (dual) {

    if (!IS_NPC(ch) && is_skilled_dualer(ch, MODE_IMP_2_WPN)) { /* improved 2-weapon fighting */
      numAttacks++;
      if (mode == NORMAL_ATTACK_ROUTINE) { //normal attack routine
        if (valid_fight_cond(ch))
          if (phase == PHASE_0 || ((phase == PHASE_1) && ((numAttacks == 1) ||
                                                (numAttacks == 4) ||
                                                (numAttacks == 7) ||
                                                (numAttacks == 10))) ||
                                  ((phase == PHASE_2) && ((numAttacks == 2) ||
                                                (numAttacks == 5) ||
                                                (numAttacks == 8) ||
                                                (numAttacks == 11))) ||
                                  ((phase == PHASE_1) && ((numAttacks == 3) ||
                                                (numAttacks == 6) ||
                                                (numAttacks == 9) ||
                                                (numAttacks == 12))))
              hit(ch, FIGHTING(ch), TYPE_UNDEFINED, DAM_RESERVED_DBC, TWO_WPN_PNLTY, TRUE);
      } else if (mode == DISPLAY_ROUTINE_POTENTIAL) { //display attack routine
        send_to_char(ch, "Offhand (Improved 2 Weapon Fighting), Attack Bonus:  %d; ",
                     compute_attack_bonus(ch, ch, ATTACK_TYPE_OFFHAND) + TWO_WPN_PNLTY);
        compute_hit_damage(ch, ch, TYPE_UNDEFINED_WTYPE, NO_DICEROLL, MODE_DISPLAY_OFFHAND, FALSE, ATTACK_TYPE_OFFHAND);
      }
    }

    if (!IS_NPC(ch) && is_skilled_dualer(ch, MODE_GREAT_2_WPN)) { /* greater two weapon fighting */
      numAttacks++;
      if (mode == NORMAL_ATTACK_ROUTINE) { //normal attack routine
        if (valid_fight_cond(ch))
          if (phase == PHASE_0 || ((phase == PHASE_1) && ((numAttacks == 1) ||
                                                          (numAttacks == 4) ||
                                                          (numAttacks == 7) ||
                                                          (numAttacks == 10))) ||
                                  ((phase == PHASE_2) && ((numAttacks == 2) ||
                                                          (numAttacks == 5) ||
                                                          (numAttacks == 8) ||
                                                          (numAttacks == 11))) ||
                                  ((phase == PHASE_1) && ((numAttacks == 3) ||
                                                          (numAttacks == 6) ||
                                                          (numAttacks == 9) ||
                                                          (numAttacks == 12))))

            hit(ch, FIGHTING(ch), TYPE_UNDEFINED, DAM_RESERVED_DBC, GREAT_TWO_PNLY, TRUE);
      } else if (mode == DISPLAY_ROUTINE_POTENTIAL) { //display attack routine
        send_to_char(ch, "Offhand (Great 2 Weapon Fighting), Attack Bonus:  %d; ",
                     compute_attack_bonus(ch, ch, ATTACK_TYPE_OFFHAND) + GREAT_TWO_PNLY);
        compute_hit_damage(ch, ch, TYPE_UNDEFINED_WTYPE, NO_DICEROLL, MODE_DISPLAY_OFFHAND, FALSE, ATTACK_TYPE_OFFHAND);
      }
    }

    if (!IS_NPC(ch) && is_skilled_dualer(ch, MODE_EPIC_2_WPN)) { /* perfect two weapon fighting */
      numAttacks++;
      if (mode == NORMAL_ATTACK_ROUTINE) { //normal attack routine
        if (valid_fight_cond(ch))
          if (phase == PHASE_0 || ((phase == PHASE_1) && ((numAttacks == 1) ||
                                                          (numAttacks == 4) ||
                                                          (numAttacks == 7) ||
                                                          (numAttacks == 10))) ||
                                  ((phase == PHASE_2) && ((numAttacks == 2) ||
                                                          (numAttacks == 5) ||
                                                          (numAttacks == 8) ||
                                                          (numAttacks == 11))) ||
                                  ((phase == PHASE_1) && ((numAttacks == 3) ||
                                                          (numAttacks == 6) ||
                                                          (numAttacks == 9) ||
                                                          (numAttacks == 12))))
              hit(ch, FIGHTING(ch), TYPE_UNDEFINED, DAM_RESERVED_DBC, EPIC_TWO_PNLTY, TRUE);
      } else if (mode == DISPLAY_ROUTINE_POTENTIAL) { //display attack routine
        send_to_char(ch, "Offhand (Epic 2 Weapon Fighting), Attack Bonus:  %d; ",
                     compute_attack_bonus(ch, ch, ATTACK_TYPE_OFFHAND) + EPIC_TWO_PNLTY);
        compute_hit_damage(ch, ch, TYPE_UNDEFINED_WTYPE, NO_DICEROLL, MODE_DISPLAY_OFFHAND, FALSE, ATTACK_TYPE_OFFHAND);
      }
    }

  }
  return numAttacks;
}
#undef ATTACK_CAP
#undef MONK_CAP
#undef TWO_WPN_PNLTY
#undef EPIC_TWO_PNLY
#undef NORMAL_ATTACK_ROUTINE
#undef RETURN_NUM_ATTACKS
#undef DISPLAY_ROUTINE_POTENTIAL
#undef PHASE_1
#undef PHASE_2
#undef PHASE_3

/* display condition of FIGHTING() target to ch */
/* this is deprecated with the prompt changes */
void autoDiagnose(struct char_data * ch) {
  struct char_data *char_fighting = NULL, *tank = NULL;
  int percent;

  char_fighting = FIGHTING(ch);
  if (char_fighting && (ch->in_room == char_fighting->in_room) &&
          GET_HIT(char_fighting) > 0) {

    if ((tank = char_fighting->char_specials.fighting) &&
            (ch->in_room == tank->in_room)) {

      if (!IS_NPC(ch) && !PRF_FLAGGED(ch, PRF_COMPACT))
        send_to_char(ch, "\r\n");

      send_to_char(ch, "%sT-%s%s",
              CCCYN(ch, C_NRM), CCNRM(ch, C_NRM),
              (CAN_SEE(ch, tank)) ? GET_NAME(tank) : "someone");
      send_to_char(ch, "%s: ",
              CCCYN(ch, C_NRM));

      if (GET_MAX_HIT(tank) > 0)
        percent = (100 * GET_HIT(tank)) / GET_MAX_HIT(tank);
      else
        percent = -1;
      if (percent >= 100) {
        send_to_char(ch, CBWHT(ch, C_NRM));
        send_to_char(ch, "excellent");
      } else if (percent >= 95) {
        send_to_char(ch, CCNRM(ch, C_NRM));
        send_to_char(ch, "few scratches");
      } else if (percent >= 75) {
        send_to_char(ch, CBGRN(ch, C_NRM));
        send_to_char(ch, "small wounds");
      } else if (percent >= 55) {
        send_to_char(ch, CBBLK(ch, C_NRM));
        send_to_char(ch, "few wounds");
      } else if (percent >= 35) {
        send_to_char(ch, CBMAG(ch, C_NRM));
        send_to_char(ch, "nasty wounds");
      } else if (percent >= 15) {
        send_to_char(ch, CBBLU(ch, C_NRM));
        send_to_char(ch, "pretty hurt");
      } else if (percent >= 1) {
        send_to_char(ch, CBRED(ch, C_NRM));
        send_to_char(ch, "awful");
      } else {
        send_to_char(ch, CBFRED(ch, C_NRM));
        send_to_char(ch, "bleeding, close to death");
        send_to_char(ch, CCNRM(ch, C_NRM));
      }
    } // end tank

    send_to_char(ch, "%s     E-%s%s",
            CBCYN(ch, C_NRM), CCNRM(ch, C_NRM),
            (CAN_SEE(ch, char_fighting)) ? GET_NAME(char_fighting) : "someone");

    send_to_char(ch, "%s: ",
            CBCYN(ch, C_NRM));

    if (GET_MAX_HIT(char_fighting) > 0)
      percent = (100 * GET_HIT(char_fighting)) / GET_MAX_HIT(char_fighting);
    else
      percent = -1;
    if (percent >= 100) {
      send_to_char(ch, CBWHT(ch, C_NRM));
      send_to_char(ch, "excellent");
    } else if (percent >= 95) {
      send_to_char(ch, CCNRM(ch, C_NRM));
      send_to_char(ch, "few scratches");
    } else if (percent >= 75) {
      send_to_char(ch, CBGRN(ch, C_NRM));
      send_to_char(ch, "small wounds");
    } else if (percent >= 55) {
      send_to_char(ch, CBBLK(ch, C_NRM));
      send_to_char(ch, "few wounds");
    } else if (percent >= 35) {
      send_to_char(ch, CBMAG(ch, C_NRM));
      send_to_char(ch, "nasty wounds");
    } else if (percent >= 15) {
      send_to_char(ch, CBBLU(ch, C_NRM));
      send_to_char(ch, "pretty hurt");
    } else if (percent >= 1) {
      send_to_char(ch, CBRED(ch, C_NRM));
      send_to_char(ch, "awful");
    } else {
      send_to_char(ch, CBFRED(ch, C_NRM));
      send_to_char(ch, "bleeding, close to death");
      send_to_char(ch, CCNRM(ch, C_NRM));
    }
    send_to_char(ch, "\tn\r\n");
    if (!IS_NPC(ch) && !PRF_FLAGGED(ch, PRF_COMPACT))
      send_to_char(ch, "\r\n");
  }
}

/* Fight control event.  Replaces the violence loop. */
EVENTFUNC(event_combat_round) {
  struct char_data *ch = NULL;
  struct mud_event_data *pMudEvent = NULL;

  /*  This is just a dummy check, but we'll do it anyway */
  if (event_obj == NULL)
    return 0;

  /*  For the sake of simplicity, we will place the event data in easily
   * referenced pointers */
  pMudEvent = (struct mud_event_data *) event_obj;
  ch = (struct char_data *) pMudEvent->pStruct;

  if ((!IS_NPC(ch) && (ch->desc != NULL && !IS_PLAYING(ch->desc))) || (FIGHTING(ch) == NULL)){
//    send_to_char(ch, "DEBUG: RETURNING 0 FROM COMBAT EVENT.\r\n");
    stop_fighting(ch);
    return 0;
  }

  execute_next_action(ch);

  perform_violence(ch, (pMudEvent->sVariables != NULL && is_number(pMudEvent->sVariables) ? atoi(pMudEvent->sVariables) : 0));

  if (pMudEvent->sVariables != NULL)
    sprintf(pMudEvent->sVariables, "%d", (atoi(pMudEvent->sVariables) < 3 ? atoi(pMudEvent->sVariables) + 1 : 1));

  return 2 RL_SEC; /* 6 second rounds, hack! */

}

/* control the fights going on.
 * Called from combat round event. */
void perform_violence(struct char_data *ch, int phase) {
  struct char_data *tch = NULL, *charmee;
  struct list_data *room_list = NULL;

  /* Reset combat data */
  GET_TOTAL_AOO(ch) = 0;
  REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_FLAT_FOOTED);

  if (AFF_FLAGGED(ch, AFF_FEAR) && !IS_NPC(ch) &&
          HAS_FEAT(ch, FEAT_AURA_OF_COURAGE)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_FEAR);
    send_to_char(ch, "Your divine courage overcomes the fear!\r\n");
    act("$n \tWis bolstered by $s courage and overcomes $s \tDfear!\tn\tn",
            TRUE, ch, 0, 0, TO_ROOM);
    return;
  }

  if (AFF_FLAGGED(ch, AFF_FEAR) && !IS_NPC(ch) &&
          HAS_FEAT(ch, FEAT_RP_FEARLESS_RAGE) &&
          affected_by_spell(ch, SKILL_RAGE)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_FEAR);
    send_to_char(ch, "Your fearless rage overcomes the fear!\r\n");
    act("$n \tWis bolstered by $s fearless rage and overcomes $s \tDfear!\tn\tn",
            TRUE, ch, 0, 0, TO_ROOM);
    return;
  }

  if (FIGHTING(ch) == NULL || IN_ROOM(ch) != IN_ROOM(FIGHTING(ch))) {
    stop_fighting(ch);
    return;
  }

  if (phase == 1 || phase == 0) { /* make sure this doesn't happen more than once a round */
#define RETURN_NUM_ATTACKS 1
    PARRY_LEFT(ch) = perform_attacks(ch, RETURN_NUM_ATTACKS, phase);
#undef RETURN_NUM_ATTACKS

    /* Once per round when your mount is hit in combat, you may attempt a Ride
     * check (as an immediate action) to negate the hit. The hit is negated if
     * your Ride check result is greater than the opponent's attack roll. */
    if (RIDING(ch) && HAS_FEAT(ch, FEAT_MOUNTED_COMBAT))
      MOUNTED_BLOCKS_LEFT(ch) = 1;
  }

  if (AFF_FLAGGED(ch, AFF_PARALYZED)) {
    if (AFF_FLAGGED(ch, AFF_FREE_MOVEMENT)) {
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_PARALYZED);
      send_to_char(ch, "Your free movement breaks the paralysis!\r\n");
      act("$n's free movement breaks the paralysis!",
              TRUE, ch, 0, 0, TO_ROOM);
    } else {
      send_to_char(ch, "You are paralyzed and unable to react!\r\n");
      act("$n seems to be paralyzed and unable to react!",
              TRUE, ch, 0, 0, TO_ROOM);
      return;
    }
  }

  if (AFF_FLAGGED(ch, AFF_NAUSEATED)) {
    send_to_char(ch, "You are too nauseated to fight!\r\n");
    act("$n seems to be too nauseated to fight!",
            TRUE, ch, 0, 0, TO_ROOM);
    return;
  }

  if (AFF_FLAGGED(ch, AFF_DAZED)) {
    send_to_char(ch, "You are too dazed to fight!\r\n");
    act("$n seems too dazed to fight!", TRUE, ch, 0, 0, TO_ROOM);
    return;
  }

  if (char_has_mud_event(ch, eSTUNNED)) {
    if (AFF_FLAGGED(ch, AFF_FREE_MOVEMENT)) {
      change_event_duration(ch, eSTUNNED, 0);
      send_to_char(ch, "Your free movement breaks the stun!\r\n");
      act("$n's free movement breaks the stun!",
              TRUE, ch, 0, 0, TO_ROOM);
    } else {
      send_to_char(ch, "You are stunned and unable to react!\r\n");
      act("$n seems to be stunned and unable to react!",
              TRUE, ch, 0, 0, TO_ROOM);
      return;
    }
  }

  /* we'll break stun here if under free-movement affect */
  if (AFF_FLAGGED(ch, AFF_STUN)) {
    if (AFF_FLAGGED(ch, AFF_FREE_MOVEMENT)) {
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_STUN);
      send_to_char(ch, "Your free movement breaks the stun!\r\n");
      act("$n's free movement breaks the stun!",
              TRUE, ch, 0, 0, TO_ROOM);
    }
  }

  /* make sure this goes after attack-stopping affects like paralyze */
  if (!MOB_CAN_FIGHT(ch)) {
    /* this should be called in hit() but need a copy here for !fight flag */
    fight_mtrigger(ch); //fight trig
    return;
  }

  if (IS_NPC(ch) && (phase == 1)) {
    if (GET_MOB_WAIT(ch) > 0 || HAS_WAIT(ch)) {
      GET_MOB_WAIT(ch) -= PULSE_VIOLENCE;
    } else {
      GET_MOB_WAIT(ch) = 0;
      if ((GET_POS(ch) < POS_FIGHTING) && (GET_POS(ch) > POS_STUNNED)) {
        GET_POS(ch) = POS_FIGHTING;
        attacks_of_opportunity(ch, 0);
        send_to_char(ch, "You scramble to your feet!\r\n");
        act("$n scrambles to $s feet!", TRUE, ch, 0, 0, TO_ROOM);
      }
    }
  }

    /* Positions
   POS_DEAD       0
   POS_MORTALLYW	1
   POS_INCAP      2
   POS_STUNNED	3
   POS_SLEEPING	4
   POS_RESTING	5
   POS_SITTING	6
   POS_FIGHTING	7
   POS_STANDING	8	*/
  if (GET_POS(ch) < POS_SITTING) {
    send_to_char(ch, "You are in no position to fight!!\r\n");
    return;
  }

  // confusion code
  // 20% chance to act normal
  if (AFF_FLAGGED(ch, AFF_CONFUSED) && rand_number(0, 4)) {

      // 30% to do nothing
    if (rand_number(1, 100) <= 30) {
      send_to_char(ch, "\tDConfusion\tc overcomes you and you stand dumbfounded!\tn  ");
      act("$n \tcis overcome with \tDconfusion and stands dumbfounded\tc!\tn",
                TRUE, ch, 0, 0, TO_ROOM);
      stop_fighting(ch);
      USE_FULL_ROUND_ACTION(ch);
      return;
    }// 20% to flee
    else if (rand_number(1, 100) <= 20) {
      send_to_char(ch, "\tDFear\tc overcomes you!\tn  ");
      act("$n \tcis overcome with \tDfear\tc!\tn",
                TRUE, ch, 0, 0, TO_ROOM);
      perform_flee(ch);
      perform_flee(ch);
      return;
    }// 30% to attack random
    else {
      /* allocate memory for list */
      room_list = create_list();

      /* dummy check */
      if (!IN_ROOM(ch))
        return;

      /* build list */
      for (tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room)
        if (tch)
          add_to_list(tch, room_list);

      /* If our list is empty or has "0" entries, we free it from memory
       * and flee for the hills */
      if (room_list->iSize == 0) {
        free_list(room_list);
        return;
      }

      /* ok we should have something in the list, pick randomly and switch
         to our new target */
      tch = random_from_list(room_list);
      if (tch) {
        stop_fighting(ch);
        hit(ch, tch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
        send_to_char(ch, "\tDConfusion\tc overcomes you and you lash out!\tn  ");
        act("$n \tcis overcome with \tDconfusion and lashes out\tc!\tn",
                TRUE, ch, 0, 0, TO_ROOM);
      }

      /* we're done, free the list */
      free_list(room_list);
      return;
    }
  }

  /* group members will autoassist if
   1)  npc or
   2)  pref flagged autoassist
   */
  if (GROUP(ch)) {
    while ((tch = (struct char_data *) simple_list(GROUP(ch)->members))
            != NULL) {
      if (tch == ch)
        continue;
      if (!IS_NPC(tch) && !PRF_FLAGGED(tch, PRF_AUTOASSIST))
        continue;
      if (IN_ROOM(ch) != IN_ROOM(tch))
        continue;
      if (FIGHTING(tch))
        continue;
      if (GET_POS(tch) != POS_STANDING)
        continue;
      if (!CAN_SEE(tch, ch))
        continue;

      perform_assist(tch, ch);
    }
  }

  //your charmee, even if not groupped, should assist
  for (charmee = world[IN_ROOM(ch)].people; charmee;
          charmee = charmee->next_in_room)
    if (AFF_FLAGGED(charmee, AFF_CHARM) && charmee->master == ch &&
            !FIGHTING(charmee) &&
            GET_POS(charmee) == POS_STANDING && CAN_SEE(charmee, ch))
      perform_assist(charmee, ch);

  if (AFF_FLAGGED(ch, AFF_PARRY))
    send_to_char(ch, "You continue the battle in defensive positioning!\r\n");

  /* here is our entry point for melee attack rotation */
  if (!IS_CASTING(ch) && !AFF_FLAGGED(ch, AFF_PARRY))
#define NORMAL_ATTACK_ROUTINE 0
    perform_attacks(ch, NORMAL_ATTACK_ROUTINE, phase);
#undef NORMAL_ATTACK_ROUTINE

  if (MOB_FLAGGED(ch, MOB_SPEC) && GET_MOB_SPEC(ch) &&
          !MOB_FLAGGED(ch, MOB_NOTDEADYET)) {
    char actbuf[MAX_INPUT_LENGTH] = "";
    (GET_MOB_SPEC(ch)) (ch, ch, 0, actbuf);
  }

  // the mighty awesome fear code
  if (AFF_FLAGGED(ch, AFF_FEAR) && !rand_number(0, 2)) {
    send_to_char(ch, "\tDFear\tc overcomes you!\tn  ");
    act("$n \tcis overcome with \tDfear\tc!\tn",
            TRUE, ch, 0, 0, TO_ROOM);
    perform_flee(ch);
  }

}


#undef MODE_NORMAL_HIT      //Normal damage calculating in hit()
#undef MODE_DISPLAY_PRIMARY //Display damage info primary
#undef MODE_DISPLAY_OFFHAND //Display damage info offhand
#undef MODE_DISPLAY_RANGED  //Display damage info ranged

#undef MODE_2_WPN       /* two weapon fighting equivalent (reduce two weapon fighting penalty) */
#undef MODE_IMP_2_WPN   /* improved two weapon fighting - extra attack at -5 */
#undef MODE_GREAT_2_WPN /* greater two weapon fighting - extra attack at -10 */
#undef MODE_EPIC_2_WPN  /* perfect two weapon fighting - extra attack */
