/**************************************************************************
*  File: magic.c                                           Part of tbaMUD *
*  Usage: Low-level functions for magic; spell template code.             *
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
#include "spells.h"
#include "handler.h"
#include "db.h"
#include "interpreter.h"
#include "constants.h"
#include "dg_scripts.h"
#include "class.h"
#include "fight.h"


/* local file scope function prototypes */
static int mag_materials(struct char_data *ch, IDXTYPE item0, IDXTYPE item1,
        IDXTYPE item2, int extract, int verbose);
static void perform_mag_groups(int level, struct char_data *ch, 
        struct char_data *tch, struct obj_data *obj, int spellnum,
        int savetype);

//external
extern struct raff_node *raff_list;



// Magic Resistance, ch is challenger, vict is resistor, modifier applys to vict
int compute_spell_res(struct char_data *ch, struct char_data *vict, int modifier){
  int resist = GET_SPELL_RES(vict);

  //adjustments passed to mag_resistance
  resist += modifier;
  //additional adjustmenets
  if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_SPELL_RESIST_1))
    resist += 2;
  if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_SPELL_RESIST_2))
    resist += 2;
  if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_SPELL_RESIST_3))
    resist += 2;
  if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_SPELL_RESIST_4))
    resist += 2;
  if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_SPELL_RESIST_5))
    resist += 2;

  return MIN(99, MAX(0, resist));
}
// TRUE = reisted
// FALSE = Failed to resist
int mag_resistance(struct char_data *ch, struct char_data *vict, int modifier)
{
  int challenge = dice(1,20),
	resist = compute_spell_res(ch, vict, modifier);

  // should be modified - zusuk
  challenge += (DIVINE_LEVEL(ch) + MAGIC_LEVEL(ch));

  //insert challenge bonuses here (ch)
  if (!IS_NPC(ch) && GET_SKILL(ch, SKILL_SPELLPENETRATE))
    challenge += 2;
  if (!IS_NPC(ch) && GET_SKILL(ch, SKILL_SPELLPENETRATE_2))
    challenge += 2;
  if (!IS_NPC(ch) && GET_SKILL(ch, SKILL_SPELLPENETRATE_3))
    challenge += 4;

  //success?
  if (resist > challenge) {
    send_to_char(vict, "\tW*(%d>%d)you resist*\tn", resist, challenge);
    if (ch)
      send_to_char(ch, "\tR*(%d<%d)resisted*\tn", challenge, resist);
    return TRUE;
  }
  //failed to resist the spell
  return FALSE;
}

// Saving Throws, ch is challenger, vict is resistor, modifier applys to vict
int compute_mag_saves(struct char_data *vict,
	int type, int modifier){

  int saves = 0;
  
  switch (type) {
    case SAVING_FORT:
      saves += GET_CON_BONUS(vict);
      if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_LUCK_OF_HEROES))
        saves++;
      if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_GREAT_FORTITUDE))
        saves += 2;
      if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_EPIC_FORTITUDE))
        saves += 3;
      break;
    case SAVING_REFL:
      saves += GET_DEX_BONUS(vict);
      if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_LUCK_OF_HEROES))
        saves++;
      if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_LIGHTNING_REFLEXES))
        saves += 2;
      if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_EPIC_REFLEXES))
        saves += 3;
      break;
    case SAVING_WILL:
      saves += GET_WIS_BONUS(vict);
      if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_LUCK_OF_HEROES))
        saves++;
      if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_IRON_WILL))
        saves += 2;
      if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_EPIC_WILL))
        saves += 3;
      break;
  }

  if (IS_NPC(vict))
    saves += (GET_LEVEL(vict) / 3) + 1;
  else
    saves += saving_throws(vict, type);
  saves += GET_SAVE(vict, type);
  saves += modifier;

  return MIN(50, MAX(saves, 0));
}
const char *save_names[] = { "Fort", "Refl", "Will", "", "" };
// TRUE = resisted
// FALSE = Failed to resist
// modifier applies to victim, higher the better (for the victim)
int mag_savingthrow(struct char_data *ch, struct char_data *vict,
	int type, int modifier)
{
  int challenge = 10,  // 10 is base DC
	diceroll = dice(1,20),
	savethrow = compute_mag_saves(vict, type, modifier) + diceroll;

  //can add challenge bonus/penalties here (ch)
  challenge += (DIVINE_LEVEL(ch) + MAGIC_LEVEL(ch)) / 2;
  if (DIVINE_LEVEL(ch) > MAGIC_LEVEL(ch))
    challenge += GET_WIS_BONUS(ch);
  else
    challenge += GET_INT_BONUS(ch);

  if (diceroll != 1 && (savethrow > challenge || diceroll == 20)) {
    send_to_char(vict, "\tW*(%s:%d<%d)saved*\tn", save_names[type],
		savethrow, challenge);
    if (ch)
      send_to_char(ch, "\tR*(%s:%d>%d)opp saved*\tn", save_names[type],
		challenge, savethrow);
    return (TRUE);
  }

  send_to_char(vict, "\tR*(%s:%d>%d)failed save*\tn", save_names[type],
		savethrow, challenge);
  if (ch)
    send_to_char(ch, "\tW*(%s:%d<%d)opp failed saved*\tn", save_names[type],
		challenge, savethrow);
  return (FALSE);
}

/* added this function to add wear off messages for skills -zusuk */
void alt_wear_off_msg(struct char_data *ch, int skillnum)
{
  if (skillnum < (MAX_SPELLS + 1)) 
    return;
  if (skillnum >= MAX_SKILLS)
    return;
  
  switch (skillnum) {
    case SKILL_RAGE:
      send_to_char(ch, "Your rage has calmed...\r\n");
      break;
    default:
      break;
  }
          
}

/* affect_update: called from comm.c (causes spells to wear off) */
void affect_update(void)
{
  struct affected_type *af, *next;
  struct char_data *i;
  struct raff_node *raff, *next_raff, *temp;
  int has_message = 0;

  for (i = character_list; i; i = i->next)
    for (af = i->affected; af; af = next) {
      next = af->next;
      if (af->duration >= 1)
        af->duration--;
      else if (af->duration == -1) /* No action */
        ;
      else {
        if ((af->spell > 0) && (af->spell <= MAX_SPELLS))
          if (!af->next || (af->next->spell != af->spell) ||
                  (af->next->duration > 0))
            if (spell_info[af->spell].wear_off_msg) {
              send_to_char(i, "%s\r\n", spell_info[af->spell].wear_off_msg);
              has_message = 1;
            }
        if (!has_message)
          alt_wear_off_msg(i, af->spell);
        affect_remove(i, af);
      }
    }

/* update the room affections */

  for (raff = raff_list; raff; raff = next_raff) {
    next_raff = raff->next;
    raff->timer--;

    if (raff->timer <= 0) {

      /* this room affection has expired */
      send_to_room(raff->room, spell_info[raff->spell].wear_off_msg);
      send_to_room(raff->room, "\r\n");

      /* remove the affection */
      REMOVE_BIT(world[(int)raff->room].room_affections, raff->affection);
      REMOVE_FROM_LIST(raff, raff_list, next)
      free(raff);
    }
  }
}

/* Checks for up to 3 vnums (spell reagents) in the player's inventory. If
 * multiple vnums are passed in, the function ANDs the items together as
 * requirements (ie. if one or more are missing, the spell will not fail).
 * @param ch The caster of the spell.
 * @param item0 The first required item of the spell, NOTHING if not required.
 * @param item1 The second required item of the spell, NOTHING if not required.
 * @param item2 The third required item of the spell, NOTHING if not required.
 * @param extract TRUE if mag_materials should consume (destroy) the items in
 * the players inventory, FALSE if not. Items will only be removed on a
 * successful cast.
 * @param verbose TRUE to provide some generic failure or success messages,
 * FALSE to send no in game messages from this function.
 * @retval int TRUE if ch has all materials to cast the spell, FALSE if not.
 */
static int mag_materials(struct char_data *ch, IDXTYPE item0,
    IDXTYPE item1, IDXTYPE item2, int extract, int verbose)
{
  /* Begin Local variable definitions. */
  /*------------------------------------------------------------------------*/
  /* Used for object searches. */
  struct obj_data *tobj = NULL;
  /* Points to found reagents. */
  struct obj_data *obj0 = NULL, *obj1 = NULL, *obj2 = NULL;
  /*------------------------------------------------------------------------*/
  /* End Local variable definitions. */

  /* Begin success checks. Checks must pass to signal a success. */
  /*------------------------------------------------------------------------*/
  /* Check for the objects in the players inventory. */
  for (tobj = ch->carrying; tobj; tobj = tobj->next_content)
  {
    if ((item0 != NOTHING) && (GET_OBJ_VNUM(tobj) == item0))
    {
      obj0 = tobj;
      item0 = NOTHING;
    }
    else if ((item1 != NOTHING) && (GET_OBJ_VNUM(tobj) == item1))
    {
      obj1 = tobj;
      item1 = NOTHING;
    }
    else if ((item2 != NOTHING) && (GET_OBJ_VNUM(tobj) == item2))
    {
      obj2 = tobj;
      item2 = NOTHING;
    }
  }

  /* If we needed items, but didn't find all of them, then the spell is a
   * failure. */
  if ((item0 != NOTHING) || (item1 != NOTHING) || (item2 != NOTHING))
  {
    /* Generic spell failure messages. */
    if (verbose)
    {
      switch (rand_number(0, 2))
      {
      case 0:
        send_to_char(ch, "A wart sprouts on your nose.\r\n");
        break;
      case 1:
        send_to_char(ch, "Your hair falls out in clumps.\r\n");
        break;
      case 2:
        send_to_char(ch, "A huge corn develops on your big toe.\r\n");
        break;
      }
    }
    /* Return fales, the material check has failed. */
    return (FALSE);
  }
  /*------------------------------------------------------------------------*/
  /* End success checks. */

  /* From here on, ch has all required materials in their inventory and the
   * material check will return a success. */

  /* Begin Material Processing. */
  /*------------------------------------------------------------------------*/
  /* Extract (destroy) the materials, if so called for. */
  if (extract)
  {
    if (obj0 != NULL)
      extract_obj(obj0);
    if (obj1 != NULL)
      extract_obj(obj1);
    if (obj2 != NULL)
      extract_obj(obj2);
    /* Generic success messages that signals extracted objects. */
    if (verbose)
    {
      send_to_char(ch, "A puff of smoke rises from your pack.\r\n");
      act("A puff of smoke rises from $n's pack.", TRUE, ch, NULL, NULL, TO_ROOM);
    }
  }

  /* Don't extract the objects, but signal materials successfully found. */
  if(!extract && verbose)
  {
    send_to_char(ch, "Your pack rumbles.\r\n");
    act("Something rumbles in $n's pack.", TRUE, ch, NULL, NULL, TO_ROOM);
  }
  /*------------------------------------------------------------------------*/
  /* End Material Processing. */

  /* Signal to calling function that the materials were successfully found
   * and processed. */
  return (TRUE);
}


// save = -1  ->  you get no save
// default    ->  magic resistance
// returns damage, -1 if dead
int mag_damage(int level, struct char_data *ch, struct char_data *victim,
		     struct obj_data *wpn, int spellnum, int savetype)
{
  int dam = 0, element = 0, num_dice = 0, save = savetype, size_dice = 0, 
          bonus = 0, magic_level = 0, divine_level = 0, mag_resist = TRUE;

  if (victim == NULL || ch == NULL)
    return (0);
  
  magic_level = MAGIC_LEVEL(ch);
  divine_level = DIVINE_LEVEL(ch);
  if (wpn)
    if (HAS_SPELLS(wpn))
      magic_level = divine_level = level;
  
  /* need to include:
   * 1)  save = SAVING_x   -1 means no saving throw
   * 2)  mag_resist = TRUE/FALSE (TRUE - default, resistable or FALSE - not)
   * 3)  element = DAM_x
   */
  
  switch (spellnum) {
    // magical
  case SPELL_MAGIC_MISSILE:  //evocation
    if (affected_by_spell(victim, SPELL_SHIELD)) {
      send_to_char(ch, "Your target is shielded by magic!\r\n");
    } else {
      mag_resist = TRUE;
      save = -1;
      
      num_dice = MIN(8, (MAX(1, magic_level / 2)));
      size_dice = 6;
      bonus = num_dice;
    }
    element = DAM_FORCE;
    break;
  case SPELL_NEGATIVE_ENERGY_RAY:  //necromancy
    save = SAVING_WILL;
    mag_resist = TRUE;
    element = DAM_NEGATIVE;
    
    num_dice = 2;
    size_dice = 6;
    bonus = 3;
    break;
  case SPELL_ICE_DAGGER:  //conjurations
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_COLD;
    
    num_dice = MIN(7, magic_level);
    size_dice = 8;
    bonus = 0;
    break;
  case SPELL_CHILL_TOUCH:  //necromancy
    // *note chill touch also has an effect, only save on effect
    save = -1;
    mag_resist = TRUE;
    element = DAM_COLD;
    
    num_dice = 1;
    size_dice = 10;
    bonus = 0;
    break;
  case SPELL_HORIZIKAULS_BOOM:  //evocation
    // *note also has an effect
    save = SAVING_FORT;
    mag_resist = TRUE;
    element = DAM_SOUND;
    
    num_dice = MIN(8, magic_level);
    size_dice = 4;
    bonus = num_dice;
    break;
  case SPELL_BURNING_HANDS:  //evocation
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_FIRE;
    
    num_dice = MIN(8, magic_level);
    size_dice = 6;
    bonus = 0;
    break;
  case SPELL_SHOCKING_GRASP:  //evocation
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_ELECTRIC;
    
    num_dice = MIN(10, magic_level);
    size_dice = 8;
    bonus = 0;
    break;
  case SPELL_SCORCHING_RAY:  //evocation
    save = -1;
    mag_resist = TRUE;
    element = DAM_FIRE;
    
    num_dice = MIN(22, magic_level*2);
    size_dice = 6;
    bonus = 0;
    break;
  case SPELL_ACID_ARROW:  //conjuration
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_ACID;
    
    num_dice = 4;
    size_dice = 6;
    bonus = 0;
    break;
  case SPELL_ENERGY_SPHERE:  //abjuration
    save = SAVING_FORT;
    mag_resist = TRUE;
    element = DAM_ENERGY;
    
    num_dice = MIN(10, magic_level);
    size_dice = 8;
    bonus = 0;
    break;
  case SPELL_LIGHTNING_BOLT:  //evocation
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_ELECTRIC;
    
    num_dice = MIN(15, magic_level);
    size_dice = 10;
    bonus = 0;
    break;
  case SPELL_FIREBALL:  //evocation
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_FIRE;
    
    num_dice = MIN(15, magic_level);
    size_dice = 10;
    bonus = 0;
    break;
  case SPELL_COLOR_SPRAY:  //illusion
    //  has effect too
    save = SAVING_WILL;
    mag_resist = TRUE;
    element = DAM_ILLUSION;
    
    num_dice = 1;
    size_dice = 4;
    bonus = 0;
    break;

/* cone of cold */

  case SPELL_ICE_STORM:
    //AoE
    save = -1;
    mag_resist = TRUE;
    element = DAM_COLD;
    
    num_dice = MIN(15, magic_level);
    size_dice = 8;
    bonus = 0;
    break;
  case SPELL_BALL_OF_LIGHTNING:
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_ELECTRIC;
    
    num_dice = MIN(22, magic_level);
    size_dice = 10;
    bonus = 0;
    break;
  case SPELL_MISSILE_STORM:
    save = SAVING_FORT;
    mag_resist = TRUE;
    element = DAM_FORCE;
    
    num_dice = MIN(26, magic_level);
    size_dice = 10;
    bonus = magic_level;
    break;
  case SPELL_CHAIN_LIGHTNING:
    //AoE
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_ELECTRIC;
    
    num_dice = MIN(28, magic_level);
    size_dice = 9;
    bonus = magic_level;
    break;
  case SPELL_METEOR_SWARM:
    //AoE
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_FIRE;
    
    num_dice = magic_level + 4;
    size_dice = 12;
    bonus = magic_level + 8;
    break;
  case SPELL_GREATER_RUIN:
    save = SAVING_WILL;
    mag_resist = TRUE;
    element = DAM_PUNCTURE;
    
    num_dice = magic_level + 6;
    size_dice = 12;
    bonus = magic_level + 35;
    break;
  case SPELL_HELLBALL:
    //AoE
    save = SAVING_FORT;
    mag_resist = TRUE;
    element = DAM_ENERGY;
    
    num_dice = magic_level + 8;
    size_dice = 12;
    bonus = magic_level + 50;
    break;


    // divine
  case SPELL_CAUSE_LIGHT_WOUNDS:
    save = SAVING_WILL; 
    mag_resist = TRUE;
    element = DAM_HOLY;
    
    num_dice = 1;
    size_dice = 12;
    bonus = MIN(7, divine_level);
    break;
  case SPELL_CAUSE_MODERATE_WOUNDS:
    save = SAVING_WILL; 
    mag_resist = TRUE;
    element = DAM_HOLY;
    
    num_dice = 2;
    size_dice = 12;
    bonus = MIN(15, divine_level);
    break;
  case SPELL_CAUSE_SERIOUS_WOUNDS:
    save = SAVING_WILL; 
    mag_resist = TRUE;
    element = DAM_HOLY;
    
    num_dice = 3;
    size_dice = 12;
    bonus = MIN(22, divine_level);
    break;
  case SPELL_CAUSE_CRITICAL_WOUNDS:
    save = SAVING_WILL; 
    mag_resist = TRUE;
    element = DAM_HOLY;
    
    num_dice = 4;
    size_dice = 12;
    bonus = MIN(30, divine_level);
    break;
  case SPELL_FLAME_STRIKE:
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_FIRE;
    
    num_dice = MIN(20, divine_level);
    size_dice = 8;
    bonus = 0;
    break;
  case SPELL_DISPEL_EVIL:
    if (IS_EVIL(ch)) {
      victim = ch;
    } else if (IS_GOOD(victim)) {
      act("The gods protect $N.", FALSE, ch, 0, victim, TO_CHAR);
      return (0);
    }
    save = SAVING_WILL;
    mag_resist = TRUE;
    element = DAM_HOLY;
    
    num_dice = divine_level;
    size_dice = 9;
    bonus = divine_level;
    break;
  case SPELL_DISPEL_GOOD:
    if (IS_GOOD(ch)) {
      victim = ch;
    } else if (IS_EVIL(victim)) {
      act("The gods protect $N.", FALSE, ch, 0, victim, TO_CHAR);
      return (0);
    }
    save = SAVING_WILL;
    mag_resist = TRUE;
    element = DAM_UNHOLY;
    
    num_dice = divine_level;
    size_dice = 9;
    bonus = divine_level;
    break;
  case SPELL_HARM:
    save = SAVING_FORT;
    mag_resist = TRUE;
    element = DAM_HOLY;
    
    num_dice = MIN(22, divine_level);
    size_dice = 8;
    bonus = num_dice;
    break;
  case SPELL_CALL_LIGHTNING:
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_ELECTRIC;
    
    num_dice = MIN(24, divine_level);
    size_dice = 8;
    bonus = num_dice + 10;
    break;
  case SPELL_DESTRUCTION:
    save = SAVING_FORT;
    mag_resist = TRUE;
    element = DAM_NEGATIVE;
    
    num_dice = MIN(26, divine_level);
    size_dice = 8;
    bonus = num_dice + 20;
    break;
  case SPELL_ENERGY_DRAIN:
    //** Magic AND Divine
    save = SAVING_FORT;
    mag_resist = TRUE;
    element = DAM_NEGATIVE;
    
    if (GET_LEVEL(victim) < CASTER_LEVEL(ch))
      num_dice = 2;
    else
      num_dice = 1;
    size_dice = 200;
    bonus = 0;
    break;
  case SPELL_EARTHQUAKE:
    //AoE
    save = SAVING_REFL;
    mag_resist = TRUE;
    element = DAM_EARTH;
    
    num_dice = divine_level;
    size_dice = 8;
    bonus = num_dice + 30;
    break;

  } /* switch(spellnum) */

  dam = dice(num_dice, size_dice) + bonus;

  //resistances to magic
  if (dam && mag_resist)
    if (mag_resistance(ch, victim, 0))
      return 0;

  int dwarf_bonus = 0;  //dwarven racial bonus to magic
  if (GET_RACE(victim) == RACE_DWARF)
    dwarf_bonus += 2;
  
  if (dam && (save != -1))  //saving throw for half damage if applies
    if (mag_savingthrow(ch, victim, save, dwarf_bonus))
      dam /= 2;

  if (!element)  //want to make sure all spells have some sort of damage category
    log("SYSERR: %d is lacking DAM_", spellnum);    

  return (damage(ch, victim, dam, spellnum, element, FALSE));
}


/* make sure you don't stack armor spells for silly high AC */
int isMagicArmored(struct char_data *victim)
{
  if (  affected_by_spell(victim, SPELL_EPIC_MAGE_ARMOR) ||
	affected_by_spell(victim, SPELL_MAGE_ARMOR) ||
	affected_by_spell(victim, SPELL_SHIELD) ||
	affected_by_spell(victim, SPELL_ARMOR)			) {

    send_to_char(victim, "Your target already has magical armoring!\r\n");
    return TRUE;
  }

  return FALSE;
}


// converted affects to rounds
// 20 rounds = 1 real minute
// 1200 rounds = 1 real hour
// old tick = 75 seconds, or 1.25 minutes or 25 rounds
#define MAX_SPELL_AFFECTS 9	/* change if more needed */
void mag_affects(int level, struct char_data *ch, struct char_data *victim,
		      struct obj_data *wpn, int spellnum, int savetype)
{
  struct affected_type af[MAX_SPELL_AFFECTS];
  bool accum_affect = FALSE, accum_duration = FALSE;
  const char *to_vict = NULL, *to_room = NULL;
  int i, j, magic_level = 0, divine_level = 0;
  int elf_bonus = 0, gnome_bonus = 0;

  if (victim == NULL || ch == NULL)
    return;

  for (i = 0; i < MAX_SPELL_AFFECTS; i++) {  //init affect array
    new_affect(&(af[i]));
    af[i].spell = spellnum;
  }

  /* racial resistance */
  switch (GET_RACE(victim)) {
    case RACE_H_ELF:
    case RACE_ELF:  //enchantments
      elf_bonus += 2;
      break;
    case RACE_GNOME:  // illusions
      gnome_bonus += 2;
      break;
    default:
      break;
  }
    
  magic_level = MAGIC_LEVEL(ch);
  divine_level = DIVINE_LEVEL(ch);
  if (wpn)
    if (HAS_SPELLS(wpn))
      magic_level = divine_level = level;

  switch (spellnum) {

  case SPELL_SCARE:  //illusion
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_WILL, gnome_bonus)) {
      return;
    }
    if (GET_LEVEL(victim) >= 7) {
      send_to_char(ch, "The victim is too powerful for this illusion!\r\n");
      return;
    }

    SET_BIT_AR(af[0].bitvector, AFF_FEAR);
    af[0].duration = dice(2, 6);
    to_room = "$n is imbued with fear!";
    to_vict = "You feel scared and fearful!";
    break;

  case SPELL_HORIZIKAULS_BOOM:
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_FORT, 0)) {
      return;
    }

    SET_BIT_AR(af[0].bitvector, AFF_DEAF);
    af[0].duration = dice(2, 4);
    to_room = "$n is deafened by the blast!";
    to_vict = "You feel deafened by the blast!";
    break;

  case SPELL_COLOR_SPRAY:  //enchantment
    if (GET_LEVEL(victim) > 5) {
      send_to_char(ch, "Your target is too powerful to be stunned by this illusion.\r\n");
      return;
    }
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_WILL, gnome_bonus)) {
      return;
    }

    SET_BIT_AR(af[0].bitvector, AFF_STUN);
    af[0].duration = dice(1, 4);
    to_room = "$n is stunned by the colors!";
    to_vict = "You are stunned by the colors!";
    break;

  case SPELL_DAZE_MONSTER:  //enchantment
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_WILL, elf_bonus)) {
      return;
    }

    SET_BIT_AR(af[0].bitvector, AFF_STUN);
    af[0].duration = dice(2, 4);
    to_room = "$n is dazed by the spell!";
    to_vict = "You are dazed by the spell!";
    break;

  case SPELL_HIDEOUS_LAUGHTER:  //enchantment
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_WILL, elf_bonus)) {
      return;
    }

    SET_BIT_AR(af[0].bitvector, AFF_PARALYZED);
    af[0].duration = dice(1, 4);
    to_room = "$n is overcome by a fit of hideous laughter!";
    to_vict = "You are overcome by a fit of hideous luaghter!";
    break;

  case SPELL_CHILL_TOUCH:  //necromancy
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_FORT, 0))
      return;

    af[0].location = APPLY_STR;
    af[0].duration = 4 + magic_level;
    af[0].modifier = -2;
    accum_duration = TRUE;
    to_room = "$n's strength is withered!";
    to_vict = "You feel your strength wither!";
    break;

  case SPELL_SHIELD:  //transmutation
    if (isMagicArmored(victim))
      return;

    af[0].location = APPLY_AC;
    af[0].modifier = -20;
    af[0].duration = 300;
    to_vict = "You feel someone protecting you.";
    to_room = "$n is surrounded by magical armor!";
    break;

  case SPELL_ARMOR:
    if (isMagicArmored(victim))
      return;

    af[0].location = APPLY_AC;
    af[0].modifier = -20;
    af[0].duration = 600;
    accum_duration = TRUE;
    to_vict = "You feel someone protecting you.";
    to_room = "$n is surrounded by magical armor!";
    break;

  case SPELL_GREASE:  //divination
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_REFL, 0)) {
      send_to_char(ch, "You fail.\r\n");
      return;
    }

    af[0].location = APPLY_MOVE;
    af[0].modifier = -20 - magic_level;
    af[0].duration = magic_level * 2;
    to_vict = "Your feet are all greased up!";
    to_room = "$n now has greasy feet!";
    break;

  case SPELL_EXPEDITIOUS_RETREAT:  //transmutation
    af[0].location = APPLY_MOVE;
    af[0].modifier = 20 + magic_level;
    af[0].duration = magic_level * 2;
    to_vict = "You feel expeditious.";
    to_room = "$n is now expeditious!";
    break;

  case SPELL_MAGE_ARMOR:  //conjuration
    if (isMagicArmored(victim))
      return;

    af[0].location = APPLY_AC;
    af[0].modifier = -20;
    af[0].duration = 600;
    accum_duration = FALSE;
    to_vict = "You feel someone protecting you.";
    to_room = "$n is surrounded by magical armor!";
    break;

  case SPELL_IRON_GUTS:  //transmutation
    af[0].location = APPLY_SAVING_FORT;
    af[0].modifier = 3;
    af[0].duration = 300;

    to_room = "$n now has guts tough as iron!";
    to_vict = "You feel like your guts are tough as iron!";
    break;

  case SPELL_BLESS:
    af[0].location = APPLY_HITROLL;
    af[0].modifier = 2;
    af[0].duration = 300;

    af[1].location = APPLY_SAVING_WILL;
    af[1].modifier = 1;
    af[1].duration = 300;

    accum_duration = TRUE;
    to_room = "$n is now righteous!";
    to_vict = "You feel righteous.";
    break;

  case SPELL_FALSE_LIFE:  //necromancy
    af[1].location = APPLY_HIT;
    af[1].modifier = 30;
    af[1].duration = 300;

    accum_duration = TRUE;
    to_room = "$n grows strong with \tDdark\tn life!";
    to_vict = "You grow strong with \tDdark\tn life!";
    break;

  case SPELL_BLINDNESS:  //necromancy
    if (MOB_FLAGGED(victim, MOB_NOBLIND)) {
      send_to_char(ch, "Your opponent doesn't seem blindable.\r\n");
      return;
    }
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_FORT, 0)) {
      send_to_char(ch, "You fail.\r\n");
      return;
    }

    af[0].location = APPLY_HITROLL;
    af[0].modifier = -4;
    af[0].duration = 50;
    SET_BIT_AR(af[0].bitvector, AFF_BLIND);

    af[1].location = APPLY_AC;
    af[1].modifier = 40;
    af[1].duration = 50;
    SET_BIT_AR(af[1].bitvector, AFF_BLIND);

    to_room = "$n seems to be blinded!";
    to_vict = "You have been blinded!";
    break;

  case SPELL_DEAFNESS:  //necromancy
    /*
    if (MOB_FLAGGED(victim, MOB_NODEAF)) {
      send_to_char(ch, "Your opponent doesn't seem deafable.\r\n");
      return;
    }
    */
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_FORT, 0)) {
      send_to_char(ch, "You fail.\r\n");
      return;
    }

    af[0].duration = 50;
    SET_BIT_AR(af[0].bitvector, AFF_BLIND);

    to_room = "$n seems to be deafened!";
    to_vict = "You have been deafened!";
    break;


  case SPELL_WEB:  //conjuration
    /*
    if (MOB_FLAGGED(victim, MOB_NOGRAPPLE)) {
      send_to_char(ch, "Your opponent doesn't seem webbable.\r\n");
      return;
    }
    */
    if (mag_savingthrow(ch, victim, SAVING_REFL, 0)) {
      send_to_char(ch, "You fail.\r\n");
      return;
    }

    af[0].duration = 7 * magic_level;
    SET_BIT_AR(af[0].bitvector, AFF_GRAPPLED);

    to_room = "$n is covered in a sticky magical web!";
    to_vict = "You are covered in a sticky magical web!";
    break;

  case SPELL_BLUR:  //illusion
    af[0].location = APPLY_AC;
    af[0].modifier = -1;
    af[0].duration = 1200;
    to_room = "$n's images becomes blurry!.";
    to_vict = "You observe as your image becomes blurry.";
    SET_BIT_AR(af[0].bitvector, AFF_BLUR);
    accum_duration = FALSE;
    break;

  case SPELL_CURSE:
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_WILL, 0)) {
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
      return;
    }

    af[0].location = APPLY_HITROLL;
    af[0].duration = 25 + (CASTER_LEVEL(ch) * 12);
    af[0].modifier = -1;
    SET_BIT_AR(af[0].bitvector, AFF_CURSE);

    af[1].location = APPLY_DAMROLL;
    af[1].duration = 25 + (CASTER_LEVEL(ch) * 12);
    af[1].modifier = -1;
    SET_BIT_AR(af[1].bitvector, AFF_CURSE);

    accum_duration = TRUE;
    accum_affect = TRUE;
    to_room = "$n briefly glows red!";
    to_vict = "You feel very uncomfortable.";
    break;

  case SPELL_TOUCH_OF_IDIOCY:
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_WILL, 0)) {
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
      return;
    }

    af[0].location = APPLY_INT;
    af[0].duration = 25 + (CASTER_LEVEL(ch) * 12);
    af[0].modifier = -(dice(1,6));

    af[1].location = APPLY_WIS;
    af[1].duration = 25 + (CASTER_LEVEL(ch) * 12);
    af[1].modifier = -(dice(1,6));

    af[2].location = APPLY_CHA;
    af[2].duration = 25 + (CASTER_LEVEL(ch) * 12);
    af[2].modifier = -(dice(1,6));
    
    accum_duration = TRUE;
    accum_affect = TRUE;
    to_room = "A look of idiocy crosses $n's face!";
    to_vict = "You feel very idiotic.";
    break;

  case SPELL_RAY_OF_ENFEEBLEMENT:  //necromancy
    if (mag_resistance(ch, victim, 0))
      return;
    if (mag_savingthrow(ch, victim, SAVING_REFL, 0)) {
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
      return;
    }

    af[0].location = APPLY_STR;
    af[0].duration = 25 + (CASTER_LEVEL(ch) * 12);
    af[0].modifier = -dice(2,4);
    accum_duration = TRUE;
    to_room = "$n is hit by a ray of enfeeblement!";
    to_vict = "You feel enfeebled!";
    break;

  case SPELL_DETECT_ALIGN:
    af[0].duration = 300 + CASTER_LEVEL(ch) * 25;
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_ALIGN);
    accum_duration = TRUE;
    to_room = "$n's eyes become sensitive to motives!";
    to_vict = "Your eyes become sensitive to motives.";
    break;

  case SPELL_DETECT_INVIS:  //divination
    af[0].duration = 300 + magic_level * 25;
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_INVIS);
    accum_duration = TRUE;
    to_vict = "Your eyes tingle, now sensitive to invisibility.";
    to_room = "$n's eyes become sensitive to invisibility!";
    break;

  case SPELL_DETECT_MAGIC:  //divination
    af[0].duration = 300 + CASTER_LEVEL(ch) * 25;
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_MAGIC);
    accum_duration = TRUE;
    to_room = "$n's eyes become sensitive to magic!";
    to_vict = "Magic becomes clear as your eyes tingle.";
    break;

  case SPELL_ENDURE_ELEMENTS:  //abjuration
    af[0].duration = 600;
    SET_BIT_AR(af[0].bitvector, AFF_ELEMENT_PROT);
    to_vict = "You feel a slight protection from the elements!";
    to_room = "$n begins to feel slightly protected from the elements!";
    break;

  case SPELL_RESIST_ENERGY:  //abjuration
    af[0].duration = 600;
    SET_BIT_AR(af[0].bitvector, AFF_ELEMENT_PROT);
    to_vict = "You feel a slight protection from energy!";
    to_room = "$n begins to feel slightly protected from energy!";
    break;

  case SPELL_TRUE_STRIKE:  //illusion
    af[0].location = APPLY_HITROLL;
    af[0].duration = (magic_level * 12) + 100;
    af[0].modifier = 20;
    accum_duration = TRUE;
    to_vict = "You feel able to strike true!";
    to_room = "$n is now able to strike true!";
    break;

  case SPELL_ENDURANCE:  //transmutation
    af[0].location = APPLY_CON;
    af[0].duration = (CASTER_LEVEL(ch) * 12) + 100;
    af[0].modifier = 2 + (CASTER_LEVEL(ch) / 5);
    accum_duration = TRUE;
    to_vict = "You feel more hardy!";
    to_room = "$n begins to feel more hardy!";
    break;

  case SPELL_EPIC_MAGE_ARMOR:  //epic
    if (isMagicArmored(victim))
      return;

    af[0].location = APPLY_AC;
    af[0].modifier = -125;
    af[0].duration = 1200;
    af[1].location = APPLY_DEX;
    af[1].modifier = 7;
    af[1].duration = 1200;
    accum_duration = FALSE;
    to_vict = "You feel magic protecting you.";
    to_room = "$n is surrounded by magical bands of armor!";
    break;

  case SPELL_EPIC_WARDING:
    if (affected_by_spell(victim, SPELL_STONESKIN)) {
      send_to_char(ch, "A magical ward is already in effect on target.\r\n");
      return;
    }
    af[0].location = APPLY_AC;
    af[0].modifier = -1;
    af[0].duration = 1200;
    to_room = "$n becomes surrounded by a powerful magical ward!";
    to_vict = "You become surrounded by a powerful magical ward!";
    GET_STONESKIN(victim) = MIN(700, CASTER_LEVEL(ch) * 60);
    break;

  case SPELL_FLY:
    af[0].duration = 600;
    SET_BIT_AR(af[0].bitvector, AFF_FLYING);
    accum_duration = TRUE;
    to_room = "$n begins to fly above the ground!";
    to_vict = "You fly above the ground.";
    break;

  case SPELL_INFRAVISION:
    af[0].duration = 300 + GET_LEVEL(ch) * 25;
    SET_BIT_AR(af[0].bitvector, AFF_INFRAVISION);
    accum_duration = TRUE;
    to_vict = "Your eyes glow red.";
    to_room = "$n's eyes glow red.";
    break;

  case SPELL_INVISIBLE:  //illusion
    if (!victim)
      victim = ch;

    af[0].duration = 300 + (magic_level * 6);
    af[0].modifier = -40;
    af[0].location = APPLY_AC;
    SET_BIT_AR(af[0].bitvector, AFF_INVISIBLE);
    accum_duration = TRUE;
    to_vict = "You vanish.";
    to_room = "$n slowly fades out of existence.";
    break;

  case SPELL_MIRROR_IMAGE:  //illusion
    af[0].location = APPLY_AC;
    af[0].modifier = -1;
    af[0].duration = 600;
    to_room = "$n grins as multiple images pop up and smile!";
    to_vict = "You watch as multiple images pop up and smile at you!";
    GET_IMAGES(victim) = 5 + MIN(5, (int) (magic_level / 3));
    break;

  case SPELL_POISON:
    if (mag_resistance(ch, victim, 0))
      return;
    int bonus = 0;
    if (GET_RACE(ch) == RACE_DWARF ||  //dwarf dwarven poison resist
            GET_RACE(ch) == RACE_CRYSTAL_DWARF)
      bonus += 2;
    if (mag_savingthrow(ch, victim, SAVING_FORT, bonus)) {
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
      return;
    }

    af[0].location = APPLY_STR;
    af[0].duration = CASTER_LEVEL(ch) * 25;
    af[0].modifier = -2;
    SET_BIT_AR(af[0].bitvector, AFF_POISON);
    to_vict = "You feel very sick.";
    to_room = "$n gets violently ill!";
    break;

  case SPELL_PROT_FROM_EVIL:  // abjuration
    af[0].duration = 600;
    SET_BIT_AR(af[0].bitvector, AFF_PROTECT_EVIL);
    accum_duration = TRUE;
    to_vict = "You feel invulnerable to evil!";
    break;

  case SPELL_PROT_FROM_GOOD:  // abjuration
    af[0].duration = 600;
    SET_BIT_AR(af[0].bitvector, AFF_PROTECT_GOOD);
    accum_duration = TRUE;
    to_vict = "You feel invulnerable to good!";
    break;

  case SPELL_SANCTUARY:
    af[0].duration = 100;
    SET_BIT_AR(af[0].bitvector, AFF_SANCTUARY);

    accum_duration = TRUE;
    to_vict = "A white aura momentarily surrounds you.";
    to_room = "$n is surrounded by a white aura.";
    break;

  case SPELL_SLEEP:  //enchantment
    if (GET_LEVEL(victim) >= 7 || (!IS_NPC(victim) && GET_RACE(victim) == RACE_ELF)) {
      send_to_char(ch, "The target is too powerful for this enchantment!\r\n");
      return;
    }
    if (mag_resistance(ch, victim, 0))
      return;
    if (!CONFIG_PK_ALLOWED && !IS_NPC(ch) && !IS_NPC(victim))
      return;
    if (MOB_FLAGGED(victim, MOB_NOSLEEP)) {
      send_to_char(ch, "Your victim doesn't seem vulnerable to your spell.");
      return;
    }
    if (mag_savingthrow(ch, victim, SAVING_WILL, 0)) {
      return;
    }

    af[0].duration = 100 + (magic_level * 6);
    SET_BIT_AR(af[0].bitvector, AFF_SLEEP);

    if (GET_POS(victim) > POS_SLEEPING) {
      send_to_char(victim, "You feel very sleepy...  Zzzz......\r\n");
      act("$n goes to sleep.", TRUE, victim, 0, 0, TO_ROOM);
      if (FIGHTING(victim))
        stop_fighting(victim);
      GET_POS(victim) = POS_SLEEPING;
      if (FIGHTING(ch) == victim)
        stop_fighting(ch);
    }
    break;

  case SPELL_STONESKIN:
    if (affected_by_spell(victim, SPELL_EPIC_WARDING)) {
      send_to_char(ch, "A magical ward is already in effect on target.\r\n");
      return;
    }
    af[0].location = APPLY_AC;
    af[0].modifier = -1;
    af[0].duration = 1200;
    to_room = "$n's skin becomes hard as rock!";
    to_vict = "Your skin becomes hard as stone.";
    GET_STONESKIN(victim) = MIN(225, magic_level * 15);
    break;

  case SPELL_STRENGTH:  //transmutation
    af[0].location = APPLY_STR;
    af[0].duration = (CASTER_LEVEL(ch) * 12) + 100;
    af[0].modifier = 2 + (CASTER_LEVEL(ch) / 5);
    accum_duration = TRUE;
    to_vict = "You feel stronger!";
    to_room = "$n's muscles begin to bulge!";
    break;

  case SPELL_GRACE:  //transmutation
    af[0].location = APPLY_DEX;
    af[0].duration = (CASTER_LEVEL(ch) * 12) + 100;
    af[0].modifier = 2 + (CASTER_LEVEL(ch) / 5);
    accum_duration = TRUE;
    to_vict = "You feel more dextrous!";
    to_room = "$n's appears to be more dextrous!";
    break;

  case SPELL_SENSE_LIFE:
    to_vict = "Your feel your awareness improve.";
    to_room = "$n's eyes become aware of life forms!";
    af[0].duration = divine_level * 25;
    SET_BIT_AR(af[0].bitvector, AFF_SENSE_LIFE);
    accum_duration = TRUE;
    break;

  case SPELL_WATERWALK:
    af[0].duration = 600;
    SET_BIT_AR(af[0].bitvector, AFF_WATERWALK);
    accum_duration = TRUE;
    to_vict = "You feel webbing between your toes.";
    to_room = "$n's feet grow webbing!";
    break;
  }

  /* If this is a mob that has this affect set in its mob file, do not perform
   * the affect.  This prevents people from un-sancting mobs by sancting them
   * and waiting for it to fade, for example. */
  if (IS_NPC(victim) && !affected_by_spell(victim, spellnum)) {
    for (i = 0; i < MAX_SPELL_AFFECTS; i++) {
      for (j=0; j<NUM_AFF_FLAGS; j++) {
        if (IS_SET_AR(af[i].bitvector, j) && AFF_FLAGGED(victim, j)) {
          send_to_char(ch, "%s", CONFIG_NOEFFECT);
          return;
        }
      }
    }
  }

  /* If the victim is already affected by this spell, and the spell does not
   * have an accumulative effect, then fail the spell. */
  if (affected_by_spell(victim,spellnum) && !(accum_duration||accum_affect)) {
    send_to_char(ch, "%s", CONFIG_NOEFFECT);
    return;
  }

  for (i = 0; i < MAX_SPELL_AFFECTS; i++)
    if (af[i].bitvector[0] || af[i].bitvector[1] ||
        af[i].bitvector[2] || af[i].bitvector[3] ||
        (af[i].location != APPLY_NONE))
      affect_join(victim, af+i, accum_duration, FALSE, accum_affect, FALSE);

  if (to_vict != NULL)
    act(to_vict, FALSE, victim, 0, ch, TO_CHAR);
  if (to_room != NULL)
    act(to_room, TRUE, victim, 0, ch, TO_ROOM);
}

/* This function is used to provide services to mag_groups.  This function is
 * the one you should change to add new group spells. */
static void perform_mag_groups(int level, struct char_data *ch,
			struct char_data *tch, struct obj_data *obj, int spellnum,
               int savetype)
{
  switch (spellnum) {
    case SPELL_GROUP_HEAL:
    mag_points(level, ch, tch, obj, SPELL_HEAL, savetype);
    break;
  case SPELL_GROUP_ARMOR:
    mag_affects(level, ch, tch, obj, SPELL_ARMOR, savetype);
    break;
  case SPELL_GROUP_RECALL:
    spell_recall(level, ch, tch, NULL);
    break;
  }
}

/* Every spell that affects the group should run through here perform_mag_groups
 * contains the switch statement to send us to the right magic. Group spells
 * affect everyone grouped with the caster who is in the room, caster last. To
 * add new group spells, you shouldn't have to change anything in mag_groups.
 * Just add a new case to perform_mag_groups. */
void mag_groups(int level, struct char_data *ch, struct obj_data *obj,
        int spellnum, int savetype)
{
  struct char_data *tch;

  if (ch == NULL)
    return;

  if (!GROUP(ch))
    return;

  while ((tch = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL) {
    if (IN_ROOM(tch) != IN_ROOM(ch))
      continue;
    perform_mag_groups(level, ch, tch, obj, spellnum, savetype);
  }
}


/* Mass spells affect every creature in the room except the caster. No spells
 * of this class currently implemented. */
void mag_masses(int level, struct char_data *ch, struct obj_data *obj, 
        int spellnum, int savetype)
{
  struct char_data *tch, *tch_next;

  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch_next) {
    tch_next = tch->next_in_room;
    if (tch == ch)
      continue;

    switch (spellnum) {
    }
  }
}

// CHARM PERSON CHARM_PERSON - enchantment (found in spells.c)
// ENCHANT WEAPON ENCHANT_WEAPON - enchantment (spells.c)


//  pass values spellnum
// spellnum = spellnum
// -1 = no spellnum, no special handling
// -2 = tailsweep
//  return values
// 0 = not allowed to hit target
// 1 = allowed to hit target
int aoeOK(struct char_data *ch, struct char_data *tch, int spellnum)
{
    // skip self - tested
  if (tch == ch)
    return 0;

    // immorts that are nohas
  if (!IS_NPC(tch) && GET_LEVEL(tch) >= LVL_IMMORT &&
	PRF_FLAGGED(tch, PRF_NOHASSLE))
    return 0;

    // earthquake currently doesn't work on flying victims
  if ((spellnum == SPELL_EARTHQUAKE) && AFF_FLAGGED(tch, AFF_FLYING))
    return 0;

    // tailsweep currently doesn't work on flying victims
  if ((spellnum == -2) && AFF_FLAGGED(tch, AFF_FLYING))
    return 0;

    // same group, skip
  if (GROUP(tch) && GROUP(ch) && GROUP(ch) == GROUP(tch))
    return 0;

    // don't hit the charmee of a group member
  if (tch->master)
    if (AFF_FLAGGED(tch, AFF_CHARM) &&
      GROUP(tch->master) && GROUP(ch) && GROUP(ch) == GROUP(tch->master))
        return 0;

    // charmee, don't hit a group member of master
  if (ch->master)
    if (AFF_FLAGGED(ch, AFF_CHARM) &&
      GROUP(ch->master) && GROUP(tch) && GROUP(tch) == GROUP(ch->master))
        return 0;

    // charmee, don't hit a charmee of group member of master
  if (ch->master && tch->master)
    if (AFF_FLAGGED(ch, AFF_CHARM) && AFF_FLAGGED(tch, AFF_CHARM) &&
	GROUP(ch->master) && GROUP(tch->master) &&
	GROUP(ch->master) == GROUP(tch->master))
        return 0;

    // don't hit your master
  if (ch->master)
    if (AFF_FLAGGED(ch, AFF_CHARM) && ch->master == tch)
      return 0;

    // don't hit your charmee
  if (tch->master)
    if (AFF_FLAGGED(tch, AFF_CHARM) && tch->master == ch)
      return 0;

    // npc that isn't charmed shouldn't hurt other npc's
  if (IS_NPC(ch) && !AFF_FLAGGED(ch, AFF_CHARM) && IS_NPC(tch))
    return 0;

  // PK MUD settings
  if (CONFIG_PK_ALLOWED) {
  // !PK settings
  } else {

    // if pc cast, !pk mud, skip pc
    if (!IS_NPC(ch) && !IS_NPC(tch))
      return 0;

    // do not hit pc charmee
    if (!IS_NPC(ch) && AFF_FLAGGED(tch, AFF_CHARM) && !IS_NPC(tch->master))
      return 0;

    // charmee shouldn't hit pc's
    if (IS_NPC(ch) && AFF_FLAGGED(ch, AFF_CHARM) && !IS_NPC(ch->master) &&
	!IS_NPC(tch))
      return 0;
    
  }

  return 1;
}


/* Every spell that affects an area (room) runs through here.  These are
 * generally offensive spells.  This calls mag_damage to do the actual damage.
 * All spells listed here must also have a case in mag_damage() in order for
 * them to work. Area spells have limited targets within the room. */
void mag_areas(int level, struct char_data *ch, struct obj_data *obj,
        int spellnum, int savetype)
{
  struct char_data *tch, *next_tch;
  const char *to_char = NULL, *to_room = NULL;

  if (ch == NULL)
    return;

  /* to add spells just add the message here plus an entry in mag_damage for
   * the damaging part of the spell.   */
  switch (spellnum) {
  case SPELL_EARTHQUAKE:
    to_char = "You gesture and the earth begins to shake all around you!";
    to_room ="$n gracefully gestures and the earth begins to shake violently!";
    break;
  case SPELL_ICE_STORM:
    to_char = "You conjure a storm of ice!";
    to_room ="$n conjures a storm of ice!";
    break;
  case SPELL_CHAIN_LIGHTNING:
    to_char = "Bolts of lightning steams from your body!";
    to_room = "Bolts of lightning stream from $n!";
    break;
  case SPELL_METEOR_SWARM:
    to_char = "You invoke a swarm of meteors to fall from the sky!";
    to_room ="$n invokes a swarm of meteors to fall from the sky!";
    break;
  case SPELL_HELLBALL:
    to_char = "\tMYou invoke a HELLBALL!\tn";
    to_room ="\tM$n invokes a HELLBALL!\tn";
    break;
  }

  if (to_char != NULL)
    act(to_char, FALSE, ch, 0, 0, TO_CHAR);
  if (to_room != NULL)
    act(to_room, FALSE, ch, 0, 0, TO_ROOM);


  for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch) {
    next_tch = tch->next_in_room;

    if (aoeOK(ch, tch, spellnum))
      mag_damage(level, ch, tch, obj, spellnum, 1);
  }
}

/*----------------------------------------------------------------------------*/
/* Begin Magic Summoning - Generic Routines and Local Globals */
/*----------------------------------------------------------------------------*/

/* Every spell which summons/gates/conjours a mob comes through here. */
/* These use act(), don't put the \r\n. */
static const char *mag_summon_msgs[] = {
  "\r\n",
  "$n makes a strange magical gesture; you feel a strong breeze!",
  "$n animates a corpse!",
  "$N appears from a cloud of thick blue smoke!",
  "$N appears from a cloud of thick green smoke!",
  "$N appears from a cloud of thick red smoke!",
  "$N disappears in a thick black cloud!"
  "As $n makes a strange magical gesture, you feel a strong breeze.",
  "As $n makes a strange magical gesture, you feel a searing heat.",
  "As $n makes a strange magical gesture, you feel a sudden chill.",
  "As $n makes a strange magical gesture, you feel the dust swirl.",
  "$n magically divides!",
  "$n animates a corpse!",
  "$N breaks through the ground and bows before $n.",
  "With a roar $N soars to the ground next to $n.",  //13
  "$N pops into existence next to $n.",  //14
  "$N skimpers into the area, then quickly moves next to $n.",  //15
  "$N charges into the area, looks left, then right... then quickly moves next to $n.",  //16
};

/* Keep the \r\n because these use send_to_char. */
static const char *mag_summon_fail_msgs[] = {
  "\r\n",
  "There are no such creatures.\r\n",
  "Uh oh...\r\n",
  "Oh dear.\r\n",
  "Gosh durnit!\r\n",
  "The elements resist!\r\n",
  "You failed.\r\n",
  "There is no corpse!\r\n"
};

/* Defines for Mag_Summons */
#define MOB_CLONE            10   /**< vnum for the clone mob. */
#define OBJ_CLONE            161  /**< vnum for clone material. */
#define MOB_ZOMBIE           11   /**< vnum for the zombie mob. */
#define MOB_GHOUL		35
#define MOB_GIANT_SKELETON	36
#define MOB_MUMMY		37
#define MOB_MUMMY_LORD		38
#define MOB_RED_DRAGON		39
#define MOB_SHELGARNS_BLADE	40
#define MOB_DIRE_BADGER		41
#define MOB_DIRE_BOAR		42
void mag_summons(int level, struct char_data *ch, struct obj_data *obj,
		      int spellnum, int savetype)
{
  struct char_data *mob = NULL;
  struct obj_data *tobj, *next_obj;
  int pfail = 0, msg = 0, fmsg = 0, num = 1, handle_corpse = FALSE, i;
  mob_vnum mob_num;

  if (ch == NULL)
    return;

  switch (spellnum) {


  case SPELL_CLONE:
    msg = 10;
    fmsg = rand_number(2, 6);	/* Random fail message. */
    mob_num = MOB_CLONE;
    /*
     * We have designated the clone spell as the example for how to use the
     * mag_materials function.
     * In stock tbaMUD it checks to see if the character has item with
     * vnum 161 which is a set of sacrificial entrails. If we have the entrails
     * the spell will succeed,  and if not, the spell will fail 102% of the time
     * (prevents random success... see below).
     * The object is extracted and the generic cast messages are displayed.
     */
    if( !mag_materials(ch, OBJ_CLONE, NOTHING, NOTHING, TRUE, TRUE) )
      pfail = 102; /* No materials, spell fails. */
    else
      pfail = 0;	/* We have the entrails, spell is successfully cast. */
    break;


  case SPELL_ANIMATE_DEAD:
    if (obj == NULL || !IS_CORPSE(obj)) {
      act(mag_summon_fail_msgs[7], FALSE, ch, 0, 0, TO_CHAR);
      return;
    }
    handle_corpse = TRUE;
    msg = 11;
    fmsg = rand_number(2, 6);	/* Random fail message. */

    if (CASTER_LEVEL(ch) >= 30)
      mob_num = MOB_MUMMY;
    else if (CASTER_LEVEL(ch) >= 20)
      mob_num = MOB_GIANT_SKELETON;
    else if (CASTER_LEVEL(ch) >= 10)
      mob_num = MOB_GHOUL;
    else
      mob_num = MOB_ZOMBIE;

    pfail = 10;	/* 10% failure, should vary in the future. */
    break;


  case SPELL_MUMMY_DUST:
    handle_corpse = FALSE;
    msg = 12;
    fmsg = rand_number(2, 6);	/* Random fail message. */

    mob_num = MOB_MUMMY_LORD;

    pfail = 0;
    break;


  case SPELL_DRAGON_KNIGHT:
    handle_corpse = FALSE;
    msg = 13;
    fmsg = rand_number(2, 6);	/* Random fail message. */

    mob_num = MOB_RED_DRAGON;

    pfail = 0;
    break;


  case SPELL_SHELGARNS_BLADE:  //divination
    handle_corpse = FALSE;
    msg = 14;
    fmsg = rand_number(2, 6);	/* Random fail message. */

    mob_num = MOB_SHELGARNS_BLADE;

    pfail = 0;
    break;


  case SPELL_SUMMON_CREATURE_1: //conjuration
    handle_corpse = FALSE;
    msg = 15;
    fmsg = rand_number(2, 6);	/* Random fail message. */

    mob_num = MOB_DIRE_BADGER;

    pfail = 0;
    break;


  case SPELL_SUMMON_CREATURE_2:  //conjuration
    handle_corpse = FALSE;
    msg = 16;
    fmsg = rand_number(2, 6);	/* Random fail message. */

    mob_num = MOB_DIRE_BOAR;

    pfail = 0;
    break;


  default:
    return;
  }

  if (AFF_FLAGGED(ch, AFF_CHARM)) {
    send_to_char(ch, "You are too giddy to have any followers!\r\n");
    return;
  }
  if (rand_number(0, 101) < pfail) {
    send_to_char(ch, "%s", mag_summon_fail_msgs[fmsg]);
    return;
  }
  for (i = 0; i < num; i++) {
    if (!(mob = read_mobile(mob_num, VIRTUAL))) {
      send_to_char(ch, "You don't quite remember how to make that creature.\r\n");
      return;
    }
    char_to_room(mob, IN_ROOM(ch));
    IS_CARRYING_W(mob) = 0;
    IS_CARRYING_N(mob) = 0;
    SET_BIT_AR(AFF_FLAGS(mob), AFF_CHARM);
    if (spellnum == SPELL_CLONE) {
      /* Don't mess up the prototype; use new string copies. */
      mob->player.name = strdup(GET_NAME(ch));
      mob->player.short_descr = strdup(GET_NAME(ch));
    }
    act(mag_summon_msgs[msg], FALSE, ch, 0, mob, TO_ROOM);
    load_mtrigger(mob);
    add_follower(mob, ch);
    if (GROUP(ch) && GROUP_LEADER(GROUP(ch)) == ch)
      join_group(mob, GROUP(ch));
  }
  if (handle_corpse) {
    for (tobj = obj->contains; tobj; tobj = next_obj) {
      next_obj = tobj->next_content;
      obj_from_obj(tobj);
      obj_to_char(tobj, mob);
    }
    extract_obj(obj);
  }
}
#undef MOB_CLONE
#undef OBJ_CLONE
#undef MOB_ZOMBIE
#undef MOB_GHOUL		
#undef MOB_GIANT_SKELETON	
#undef MOB_MUMMY		
#undef MOB_RED_DRAGON
#undef MOB_SHELGARNS_BLADE
#undef MOB_DIRE_BADGER
#undef MOB_DIRE_BOAR


/*----------------------------------------------------------------------------*/
/* End Magic Summoning - Generic Routines and Local Globals */
/*----------------------------------------------------------------------------*/


void mag_points(int level, struct char_data *ch, struct char_data *victim,
		     struct obj_data *obj, int spellnum, int savetype)
{
  int healing = 0, move = 0;
  const char *to_room = NULL;

  if (victim == NULL)
    return;

  switch (spellnum) {
  case SPELL_CURE_LIGHT:
    healing = dice(1, 8) + 1 + (level / 4);

    to_room = "$n is \twcured\tn of light wounds.";
    send_to_char(victim, "You feel \twbetter\tn.\r\n");
    break;
  case SPELL_CURE_CRITIC:
    healing = dice(3, 8) + 3 + (level / 4);

    to_room = "$n is \twcured\tn of critical wounds.";
    send_to_char(victim, "You feel a \twlot better\tn!\r\n");
    break;
  case SPELL_HEAL:
    healing = 100 + dice(3, 8);

    to_room = "$n's wounds are \tWhealed\tn.";
    send_to_char(victim, "A \tWwarm feeling\tn floods your body.\r\n");
    break;
  }

  if (to_room != NULL)
    act(to_room, TRUE, victim, 0, ch, TO_ROOM);

  GET_HIT(victim) = MIN(GET_MAX_HIT(victim), GET_HIT(victim) + healing);
  GET_MOVE(victim) = MIN(GET_MAX_MOVE(victim), GET_MOVE(victim) + move);
  update_pos(victim);
}

void mag_unaffects(int level, struct char_data *ch, struct char_data *victim,
		        struct obj_data *obj, int spellnum, int type)
{
  int spell = 0, msg_not_affected = TRUE;
  const char *to_vict = NULL, *to_room = NULL;

  if (victim == NULL)
    return;

  switch (spellnum) {
  case SPELL_HEAL:
    /* Heal also restores health, so don't give the "no effect" message if the
     * target isn't afflicted by the 'blindness' spell. */
    msg_not_affected = FALSE;
    /* fall-through */
  case SPELL_CURE_BLIND:
    spell = SPELL_BLINDNESS;
    to_vict = "Your vision returns!";
    to_room = "There's a momentary gleam in $n's eyes.";
    break;
  case SPELL_REMOVE_POISON:
    spell = SPELL_POISON;
    to_vict = "A warm feeling runs through your body!";
    to_room = "$n looks better.";
    break;
  case SPELL_REMOVE_CURSE:
    spell = SPELL_CURSE;
    to_vict = "You don't feel so unlucky.";
    break;
  default:
    log("SYSERR: unknown spellnum %d passed to mag_unaffects.", spellnum);
    return;
  }

  if (!affected_by_spell(victim, spell)) {
    if (msg_not_affected)
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
    return;
  }

  affect_from_char(victim, spell);
  if (to_vict != NULL)
    act(to_vict, FALSE, victim, 0, ch, TO_CHAR);
  if (to_room != NULL)
    act(to_room, TRUE, victim, 0, ch, TO_ROOM);
}

void mag_alter_objs(int level, struct char_data *ch, struct obj_data *obj,
		         int spellnum, int savetype)
{
  const char *to_char = NULL, *to_room = NULL;

  if (obj == NULL)
    return;

  switch (spellnum) {
    case SPELL_BLESS:
      if (!OBJ_FLAGGED(obj, ITEM_BLESS) &&
	  (GET_OBJ_WEIGHT(obj) <= 5 * DIVINE_LEVEL(ch))) {
	SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_BLESS);
	to_char = "$p glows briefly.";
      }
      break;
    case SPELL_CURSE:
      if (!OBJ_FLAGGED(obj, ITEM_NODROP)) {
	SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_NODROP);
	if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
	  GET_OBJ_VAL(obj, 2)--;
	to_char = "$p briefly glows red.";
      }
      break;
    case SPELL_INVISIBLE:
      if (!OBJ_FLAGGED(obj, ITEM_NOINVIS) && !OBJ_FLAGGED(obj, ITEM_INVISIBLE)) {
        SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_INVISIBLE);
        to_char = "$p vanishes.";
      }
      break;
    case SPELL_POISON:
      if (((GET_OBJ_TYPE(obj) == ITEM_DRINKCON) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOUNTAIN) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOOD)) && !GET_OBJ_VAL(obj, 3)) {
      GET_OBJ_VAL(obj, 3) = 1;
      to_char = "$p steams briefly.";
      }
      break;
    case SPELL_REMOVE_CURSE:
      if (OBJ_FLAGGED(obj, ITEM_NODROP)) {
        REMOVE_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_NODROP);
        if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
          GET_OBJ_VAL(obj, 2)++;
        to_char = "$p briefly glows blue.";
      }
      break;
    case SPELL_REMOVE_POISON:
      if (((GET_OBJ_TYPE(obj) == ITEM_DRINKCON) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOUNTAIN) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOOD)) && GET_OBJ_VAL(obj, 3)) {
        GET_OBJ_VAL(obj, 3) = 0;
        to_char = "$p steams briefly.";
      }
      break;
  }

  if (to_char == NULL)
    send_to_char(ch, "%s", CONFIG_NOEFFECT);
  else
    act(to_char, TRUE, ch, obj, 0, TO_CHAR);

  if (to_room != NULL)
    act(to_room, TRUE, ch, obj, 0, TO_ROOM);
  else if (to_char != NULL)
    act(to_char, TRUE, ch, obj, 0, TO_ROOM);
}

void mag_creations(int level, struct char_data *ch, struct obj_data *obj,
        int spellnum)
{
  struct obj_data *tobj;
  obj_vnum z;

  if (ch == NULL)
    return;
  /* level = MAX(MIN(level, LVL_IMPL), 1); - Hm, not used. */

  switch (spellnum) {
  case SPELL_CREATE_FOOD:
    z = 10;
    break;
  case SPELL_CONTINUAL_FLAME:
    z = 222;
    break;
  default:
    send_to_char(ch, "Spell unimplemented, it would seem.\r\n");
    return;
  }

  if (!(tobj = read_object(z, VIRTUAL))) {
    send_to_char(ch, "I seem to have goofed.\r\n");
    log("SYSERR: spell_creations, spell %d, obj %d: obj not found",
	    spellnum, z);
    return;
  }
  obj_to_char(tobj, ch);
  act("$n creates $p.", FALSE, ch, tobj, 0, TO_ROOM);
  act("You create $p.", FALSE, ch, tobj, 0, TO_CHAR);
  load_otrigger(tobj);
}


void mag_room(int level, struct char_data * ch, struct obj_data *obj,
        int spellnum)
{
  long aff; /* what affection */
  int rounds; /* how many rounds this spell lasts */
  char *to_char = NULL;
  char *to_room = NULL, buf[MAX_INPUT_LENGTH];
  struct raff_node *raff;
  extern struct raff_node *raff_list;

  aff = rounds = 0;

  if (ch == NULL)
    return;

  level = MAX(MIN(level, LVL_IMPL), 1);

  switch (spellnum) {
    case SPELL_WALL_OF_FOG:
      to_char = "You create a fog out of nowhere.";
      to_room = "$n creates a fog out of nowhere.";
      aff = RAFF_FOG;
      rounds = 4;
      break;

    case SPELL_DARKNESS:  //divination
      to_char = "You create a blanket of pitch black.";
      to_room = "$n creates a blanket of pitch black.";
      aff = RAFF_DARKNESS;
      rounds = 12;
      break;

    /* add more room spells here */

    default:
      sprintf(buf, "SYSERR: unknown spellnum %d passed to mag_unaffects", spellnum);
      log(buf);
      break;
  }


  /* create, initialize, and link a room-affection node */
  CREATE(raff, struct raff_node, 1);
  raff->room = ch->in_room;
  raff->timer = rounds;
  raff->affection = aff;
  raff->spell = spellnum;
  raff->next = raff_list;
  raff_list = raff;

  /* set the affection */
  SET_BIT(ROOM_AFFECTIONS(raff->room), aff);

  if (to_char == NULL)
    send_to_char(ch, "%s", CONFIG_NOEFFECT);
  else
    act(to_char, TRUE, ch, 0, 0, TO_CHAR);

  if (to_room != NULL)
    act(to_room, TRUE, ch, 0, 0, TO_ROOM);
  else if (to_char != NULL)
    act(to_char, TRUE, ch, 0, 0, TO_ROOM);
}

