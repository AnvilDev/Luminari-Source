/**************************************************************************
 *  File: act.offensive.c                                   Part of tbaMUD *
 *  Usage: Player-level commands of an offensive nature.                   *
 *                                                                         *
 *  All rights reserved.  See license for complete information.            *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 **************************************************************************/

#include <zconf.h>

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "act.h"
#include "fight.h"
#include "mud_event.h"
#include "constants.h"
#include "spec_procs.h"
#include "class.h"
#include "mudlim.h"


/**** Utility functions *******/

/* ranged combat (archery, etc)
 * this function will check to make sure ammo is ready for firing
 */
bool has_missile_in_quiver(struct char_data *ch, struct obj_data *wielded, bool silent) {
  struct obj_data *quiver = GET_EQ(ch, WEAR_QUIVER);
  
  if (!quiver) {
    if (!silent)
      send_to_char(ch, "You have no ammo pouch!\r\n");
    FIRING(ch) = FALSE;
    return FALSE;
  }
  
  if (!quiver->contains) {
    if (!silent)
      send_to_char(ch, "Your ammo pouch is empty!\r\n");
    FIRING(ch) = FALSE;
    return FALSE;
  }

  if (GET_OBJ_TYPE(quiver->contains) != ITEM_MISSILE) {
    if (!silent)
      send_to_char(ch, "Your ammo pouch needs to be filled with only ammo!\r\n");
    FIRING(ch) = FALSE;
    return FALSE;
  }
  
  if (GET_OBJ_VAL(wielded, 0) != GET_OBJ_VAL(quiver->contains, 0)) {
    if (!silent)
      act("Your $p does not fit your weapon.", FALSE, ch, quiver->contains, NULL, TO_CHAR);
    FIRING(ch) = FALSE;
    return FALSE;
  }
  
  return TRUE;
}

/* ranged combat (archery, etc)
 * this function will check for a ranged weapon, ammo and does
 * a check of "has_missile_in_quiver"
 */
bool can_fire_arrow(struct char_data *ch, bool silent) {
  if (!GET_EQ(ch, WEAR_QUIVER)) {
    if (!silent)
      send_to_char(ch, "But you do not wear an ammo pouch.\r\n");
    FIRING(ch) = FALSE;
    return FALSE;
  }
  
  struct obj_data *obj = GET_EQ(ch, WEAR_WIELD_2H);
  
  if (!obj)
    obj = GET_EQ(ch, WEAR_WIELD_1);
  
  if (!obj) {
    if (!silent)
      send_to_char(ch, "You are not wielding anything!");
    FIRING(ch) = FALSE;
    return FALSE;
  }
  
  if (GET_OBJ_TYPE(obj) != ITEM_FIREWEAPON) {
    if (!silent)
      send_to_char(ch, "But you are not wielding a ranged weapon.\r\n");
    FIRING(ch) = FALSE;
    return FALSE;    
  }
  
  if (!has_missile_in_quiver(ch, obj, TRUE)) {
    if (!silent)
      send_to_char(ch, "You have no ammo!\r\n");
    FIRING(ch) = FALSE;
    return FALSE;
  }
  
  /* ok! */
  return TRUE;
}

/* simple function to check whether ch has a piercing weapon, has 3 modes:
   wield == 1  --  primary one hand weapon
   wield == 2  --  off hand weapon
   wield == 3  --  two hand weapon
 */
/* NOTE - this is probably deprecated by the IS_PIERCE() macro */
bool has_piercing_weapon(struct char_data *ch, int wield) {
  if (!ch)
    return FALSE;

  if (wield < 1 || wield > 3)
    return FALSE;

  if (wield == 1 && GET_EQ(ch, WEAR_WIELD_1) &&
          IS_PIERCE(GET_EQ(ch, WEAR_WIELD_1))) {
    return TRUE;
  }

  if (wield == 2 && GET_EQ(ch, WEAR_WIELD_2) &&
          GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD_2), 3) == TYPE_PIERCE - TYPE_HIT) {
    return TRUE;
  }

  if (wield == 3 && GET_EQ(ch, WEAR_WIELD_2H) &&
          GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD_2H), 3) == TYPE_PIERCE - TYPE_HIT) {
    return TRUE;
  }

  return FALSE;
}

/* stunningfist engine */
void perform_stunningfist(struct char_data *ch, struct char_data *vict) {
  int percent, prob;

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  
  if (char_has_mud_event(ch, eSTUNNINGFIST)) {
    send_to_char(ch, "You must wait longer before you can use this ability again.\r\n");
    return;
  }
  
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
      ch->next_in_room != vict && vict->next_in_room != ch) {
    send_to_char(ch, "You simply can't reach that far.\r\n");
    return;
  }
  
  if (char_has_mud_event(vict, eSTUNNED)) {
    send_to_char(ch, "Your target is already stunned...\r\n");
    return;
  }

  /* 101% is a complete failure */
  percent = rand_number(1, 101);
  if (IS_NPC(ch))
    prob = 60;
  else
    prob = GET_SKILL(ch, SKILL_STUNNING_FIST);

  if (!IS_NPC(vict) && compute_ability(vict, ABILITY_DISCIPLINE))
    percent += compute_ability(vict, ABILITY_DISCIPLINE);

  if (percent > prob) {
    damage(ch, vict, 0, SKILL_STUNNING_FIST, DAM_FORCE, FALSE);
  } else {
    damage(ch, vict, (dice(1, 8) + GET_DAMROLL(ch)), SKILL_STUNNING_FIST,
            DAM_FORCE, FALSE);
    attach_mud_event(new_mud_event(eSTUNNED, vict, NULL), 6 * PASSES_PER_SEC);
  }
  attach_mud_event(new_mud_event(eSTUNNINGFIST, ch, NULL), 150 * PASSES_PER_SEC);

  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_STUNNING_FIST);
  
}

#define RAGE_AFFECTS 4

/* rage (berserk) engine */
void perform_rage(struct char_data *ch) {
  struct affected_type af[RAGE_AFFECTS];
  int bonus = 0, duration = 0, i = 0;
  
  if (char_has_mud_event(ch, eRAGE)) {
    send_to_char(ch, "You must wait longer before you can use this ability "
            "again.\r\n");
    return;
  }

  if (affected_by_spell(ch, SKILL_RAGE)) {
    send_to_char(ch, "You are already raging!\r\n");
    return;
  }
  
  if (IS_NPC(ch) || IS_MORPHED(ch)) {
    bonus = (GET_LEVEL(ch) / 3) + 3;
  } else {
    bonus = (CLASS_LEVEL(ch, CLASS_BERSERKER) / 3) + 3;
  }
  duration = 6 + GET_CON_BONUS(ch) * 2;

  send_to_char(ch, "You go into a \tRR\trA\tRG\trE\tn!.\r\n");
  act("$n goes into a \tRR\trA\tRG\trE\tn!", FALSE, ch, 0, 0, TO_ROOM);

  /* init affect array */
  for (i = 0; i < RAGE_AFFECTS; i++) {
    new_affect(&(af[i]));
    af[i].spell = SKILL_RAGE;
    af[i].duration = duration;
  }

  af[0].location = APPLY_STR;
  af[0].modifier = bonus;

  af[1].location = APPLY_CON;
  af[1].modifier = bonus;
  GET_HIT(ch) += GET_LEVEL(ch) * bonus / 2; //little boost in current hps

  af[2].location = APPLY_SAVING_WILL;
  af[2].modifier = bonus;

  //this is a penalty
  af[3].location = APPLY_AC;
  af[3].modifier = bonus * 5;

  for (i = 0; i < RAGE_AFFECTS; i++)
    affect_join(ch, af + i, FALSE, FALSE, FALSE, FALSE);
  
  attach_mud_event(new_mud_event(eRAGE, ch, NULL), (180 * PASSES_PER_SEC));

  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_RAGE);
}

/* rescue skill mechanic */
void perform_rescue(struct char_data *ch, struct char_data *vict) {
  struct char_data *tmp_ch;
  int percent, prob;
  
  if (vict == ch) {
    send_to_char(ch, "What about fleeing instead?\r\n");
    return;
  }
  
  if (FIGHTING(ch) == vict) {
    send_to_char(ch, "How can you rescue someone you are trying to kill?\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
      ch->next_in_room != vict && vict->next_in_room != ch) {
    send_to_char(ch, "You simply can't reach that far.\r\n");
    return;
  }
  
  for (tmp_ch = world[IN_ROOM(ch)].people; tmp_ch &&
          (FIGHTING(tmp_ch) != vict); tmp_ch = tmp_ch->next_in_room);

  if ((FIGHTING(vict) != NULL) && (FIGHTING(ch) == FIGHTING(vict)) && (tmp_ch == NULL)) {
    tmp_ch = FIGHTING(vict);
    if (FIGHTING(tmp_ch) == ch) {
      send_to_char(ch, "You have already rescued %s from %s.\r\n", GET_NAME(vict), GET_NAME(FIGHTING(ch)));
      return;
    }
  }

  if (!tmp_ch) {
    act("But nobody is fighting $M!", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }
  
  percent = rand_number(1, 101); /* 101% is a complete failure */
  if (IS_NPC(ch))
    prob = 70;
  else
    prob = GET_SKILL(ch, SKILL_RESCUE);
  if (percent > prob) {
    send_to_char(ch, "You fail the rescue!\r\n");
    return;
  }
  
  send_to_char(ch, "Banzai!  To the rescue...\r\n");
  act("You are rescued by $N, you are confused!", FALSE, vict, 0, ch, TO_CHAR);
  act("$n heroically rescues $N!", FALSE, ch, 0, vict, TO_NOTVICT);

  if (FIGHTING(vict) == tmp_ch)
    stop_fighting(vict);
  if (FIGHTING(tmp_ch))
    stop_fighting(tmp_ch);
  if (FIGHTING(ch))
    stop_fighting(ch);

  set_fighting(ch, tmp_ch);
  set_fighting(tmp_ch, ch);

  SET_WAIT(ch, PULSE_VIOLENCE);
  SET_WAIT(vict, PULSE_VIOLENCE);
  
  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_RESCUE);  
}

/* charge mechanic */
void perform_charge(struct char_data *ch, struct char_data *vict) {
  struct affected_type af;
  extern struct index_data *mob_index;
  int (*name)(struct char_data *ch, void *me, int cmd, char *argument);
  int prob = 0;
  
  if (!RIDING(ch)) {
    send_to_char(ch, "You must be mounted to charge.\r\n");
    return;
  }

  if (!vict) {
    send_to_char(ch, "Charge who?\r\n");
    return;
  }
  
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  
  if (!CAN_SEE(ch, vict)) {
    send_to_char(ch, "You don't see well enough to attempt that.\r\n");
    return;
  }

  if (ROOM_FLAGGED(ch->in_room, ROOM_SINGLEFILE)) {
    if (ch->next_in_room != vict && vict->next_in_room != ch) {
      send_to_char(ch, "You simply can't reach that far.\r\n");
      return;
    }
  }

  if (IS_NPC(ch))
    prob = 60;
  else
    prob = GET_SKILL(ch, SKILL_CHARGE);
  
  if (rand_number(1, 101) >= prob) {
    damage(ch, vict, 0, SKILL_CHARGE, DAM_FORCE, FALSE);
  } else {
    name = mob_index[GET_MOB_RNUM(RIDING(ch))].func;
    if (name)
      (name)(ch, RIDING(ch), 0, "charge");
    else
      damage(ch, vict, GET_DAMROLL(ch) + GET_SKILL(ch, SKILL_CHARGE) / 2,
            SKILL_CHARGE, DAM_FORCE, FALSE);
    if (rand_number(0, 100) < GET_LEVEL(ch)) {
      new_affect(&af);
      af.spell = SKILL_CHARGE;
      SET_BIT_AR(af.bitvector, AFF_STUN);
      af.duration = dice(1, 4) + 2;
      affect_join(vict, &af, 1, FALSE, FALSE, FALSE);
      act("You charge into $N, stunning $E!", FALSE, ch, 0, vict, TO_CHAR);
      act("$n charges into $N, stunning $E!", FALSE, ch, 0, vict, TO_ROOM);
    } else
      WAIT_STATE(vict, 1 RL_SEC);
  }
  
  SET_WAIT(ch, PULSE_VIOLENCE * 2);
  
}

/* engine for knockdown, used in bash/trip/etc */
bool perform_knockdown(struct char_data *ch, struct char_data *vict, 
        int skill) {
  int percent = 0, prob = 0, dam = dice(1, 4);
  float rounds_wait = 1.75;
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
      ch->next_in_room != vict && vict->next_in_room != ch) {
    send_to_char(ch, "You simply can't reach that far.\r\n");
    return FALSE;
  }
  
  if (MOB_FLAGGED(vict, MOB_NOKILL)) {
    send_to_char(ch, "This mob is protected.\r\n");
    return FALSE;
  }
  
  if ((GET_SIZE(ch) - GET_SIZE(vict)) >= 2) {
    send_to_char(ch, "Your target is too small!\r\n");
    return FALSE;
  }
  
  if ((GET_SIZE(vict) - GET_SIZE(ch)) >= 2) {
    send_to_char(ch, "Your target is too big!\r\n");
    return FALSE;
  }

  if (IS_INCORPOREAL(vict)) {
    act("You sprawl completely through $N as you try to attack them!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n sprawls completely through $N as $e tries to attack $M.", FALSE, ch, 0, vict, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    SET_WAIT(ch, PULSE_VIOLENCE);
    return FALSE;
  }

  switch (skill) {
    case SKILL_BASH:
      if (!IS_NPC(ch) && GET_SKILL(ch, SKILL_IMPROVED_BASH)) {
        increase_skill(ch, SKILL_IMPROVED_BASH);
        prob += GET_SKILL(ch, SKILL_IMPROVED_BASH) / 5;
        dam *= 2;
      }
      break;
    case SKILL_BODYSLAM:
      dam *= 4;
      rounds_wait = rounds_wait + 1.0;
      break;
    case SKILL_TRIP:
      if (AFF_FLAGGED(vict, AFF_FLYING)) {
        send_to_char(ch, "Impossible, your target is flying!\r\n");
        return FALSE;
      }
      if (!IS_NPC(ch) && GET_SKILL(ch, SKILL_IMPROVED_TRIP)) {
        increase_skill(ch, SKILL_IMPROVED_TRIP);
        prob += GET_SKILL(ch, SKILL_IMPROVED_TRIP) / 6;
        dam *= 2;
      }
      break;
    default:
      log("Invalid skill sent to perform knockdown!\r\n");
      return FALSE;
  }
    
  percent += rand_number(1, 101); /* 101% is a complete failure */
  if (IS_NPC(ch))
    prob += 60;
  else
    prob += GET_SKILL(ch, skill);

  if (MOB_FLAGGED(vict, MOB_NOBASH)) {
    send_to_char(ch, "You realize you will probably not succeed:  ");
    percent = 101;
  }
  
  if (GET_POS(vict) == POS_SITTING) {
    send_to_char(ch, "It is difficult to knock down something already down:  ");
    percent += 75;
  }

  if (!IS_NPC(vict) && compute_ability(vict, ABILITY_DISCIPLINE))
    percent += compute_ability(vict, ABILITY_DISCIPLINE);

  if (GET_RACE(ch) == RACE_DWARF ||
          GET_RACE(ch) == RACE_CRYSTAL_DWARF) // dwarf dwarven stability
    percent += 4;

  if (percent > prob) {
    GET_POS(ch) = POS_SITTING;
    damage(ch, vict, 0, skill, DAM_FORCE, FALSE);
  } else {
    GET_POS(vict) = POS_SITTING;
    if (damage(ch, vict, dam, skill, DAM_FORCE, FALSE) > 0)
      SET_WAIT(vict, PULSE_VIOLENCE * rounds_wait);
  }

  SET_WAIT(ch, PULSE_VIOLENCE * 2);
  if (!IS_NPC(ch))
    increase_skill(ch, skill);
  
  return TRUE;
  
}

bool perform_shieldpunch(struct char_data *ch, struct char_data *vict) {
  struct affected_type af;
  extern struct index_data *obj_index;
  int (*name)(struct char_data *ch, void *me, int cmd, char *argument);
  struct obj_data *shield = GET_EQ(ch, WEAR_SHIELD);
  int prob = 0;
  
  if (!shield) {
    send_to_char(ch, "You need a shield to make it a success..\r\n");
    return FALSE;
  }
  
  if (!vict) {
    send_to_char(ch, "Shieldpunch who?\r\n");
    return FALSE;
  }
  
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return FALSE;
  }
  
  if (!CAN_SEE(ch, vict)) {
    send_to_char(ch, "You don't see well enough to attempt that.\r\n");
    return FALSE;
  }

  if (ROOM_FLAGGED(ch->in_room, ROOM_SINGLEFILE)) {
    if (ch->next_in_room != vict && vict->next_in_room != ch) {
      send_to_char(ch, "You simply can't reach that far.\r\n");
      return FALSE;
    }
  }
  
  if (IS_NPC(ch))
    prob = 60;
  else
    prob = GET_SKILL(ch, SKILL_SHIELD_PUNCH);

  if (rand_number(1, 101) >= prob) {
    damage(ch, vict, 0, SKILL_SHIELD_PUNCH, DAM_FORCE, FALSE);
  } else {
    name = obj_index[GET_OBJ_RNUM(shield)].func;
    if (name)
      (name)(ch, shield, 0, "shieldpunch");
    else {
      damage(ch, vict, GET_DAMROLL(ch) + GET_SKILL(ch, SKILL_SHIELD_PUNCH) / 2,
              SKILL_SHIELD_PUNCH, DAM_FORCE, FALSE);
    }
    
    if (rand_number(0, 60) < GET_LEVEL(ch)) {
      new_affect(&af);
      af.spell = SKILL_SHIELD_PUNCH;
      SET_BIT_AR(af.bitvector, AFF_STUN);
      af.duration = dice(1, 4) + 1;
      affect_join(vict, &af, 1, FALSE, FALSE, FALSE);
      act("You slam $p into $N, stunning $E!", FALSE, ch, shield, vict, TO_CHAR);
      act("$n slams $p into $N, stunning $E!", FALSE, ch, shield, vict, TO_ROOM);
    } else
      WAIT_STATE(vict, 1 RL_SEC);
  }
  SET_WAIT(ch, PULSE_VIOLENCE * 2);
  
  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_SHIELD_PUNCH);
  
  return TRUE;
}

/* engine for headbutt skill */
void perform_headbutt(struct char_data *ch, struct char_data *vict) {
  struct affected_type af;
  int prob = 0;
  
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(ch->in_room, ROOM_SINGLEFILE)) {
    if (ch->next_in_room != vict && vict->next_in_room != ch) {
      send_to_char(ch, "You simply can't reach that far.\r\n");
      return;
    }
  }

  if (GET_POS(ch) <= POS_SITTING) {
    send_to_char(ch, "You need to get on your feet to do a headbutt.\r\n");
    return;
  }

  if (GET_SIZE(ch) != GET_SIZE(vict)) {
    send_to_char(ch, "Its too difficult to headbutt that target due to size!\r\n");
    SET_WAIT(ch, PULSE_VIOLENCE);
    return;
  }

  if (IS_INCORPOREAL(vict)) {
    act("You sprawl completely through $N as you try to attack them!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n sprawls completely through $N as $e tries to attack $M.", FALSE, ch, 0, vict, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    SET_WAIT(ch, PULSE_VIOLENCE);
    return;
  }

  if (IS_NPC(ch))
    prob = 60;
  else
    prob = GET_SKILL(ch, SKILL_HEADBUTT);
  
  if (rand_number(1, 101) < prob) {
    damage(ch, vict, dice(2, GET_LEVEL(ch)), SKILL_HEADBUTT, DAM_FORCE, FALSE);
    
    if (!rand_number(0, 2)) {
      new_affect(&af);
      af.spell = SKILL_HEADBUTT;
      SET_BIT_AR(af.bitvector, AFF_PARALYZED);
      af.duration = rand_number(1, 3);
      affect_join(vict, &af, 1, FALSE, FALSE, FALSE);
      act("You slam your head into $N with \tRVICIOUS\tn force!", FALSE, ch, 0, vict, TO_CHAR);
      act("$n slams $s head into $N with \tRVICIOUS\tn force!", FALSE, ch, 0, vict, TO_ROOM);
    }    
  } else {
    damage(ch, vict, 0, SKILL_HEADBUTT, DAM_FORCE, FALSE);
    
  }
  
  SET_WAIT(ch, PULSE_VIOLENCE * 2);
  
  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_HEADBUTT);
}

/* engine for layonhands skill */
void perform_layonhands(struct char_data *ch, struct char_data *vict) {
  if (char_has_mud_event(ch, eLAYONHANDS)) {
    send_to_char(ch, "You must wait longer before you can use this ability again.\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
      ch->next_in_room != vict && vict->next_in_room != ch) {
    send_to_char(ch, "You simply can't reach that far.\r\n");
    return;
  }

  send_to_char(ch, "Your hands flash \tWbright white\tn as you reach out...\r\n");
  act("You are \tWhealed\tn by $N!", FALSE, vict, 0, ch, TO_CHAR);
  act("$n \tWheals\tn $N!", FALSE, ch, 0, vict, TO_NOTVICT);

  attach_mud_event(new_mud_event(eLAYONHANDS, ch, NULL), 2 * SECS_PER_MUD_DAY);
  GET_HIT(vict) += MIN(GET_MAX_HIT(vict) - GET_HIT(vict),
          20 + GET_LEVEL(ch) +
          (GET_CHA_BONUS(ch) * CLASS_LEVEL(ch, CLASS_PALADIN)));
  update_pos(vict);

  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_LAY_ON_HANDS);  
}

/* engine for sap skill */
void perform_sap(struct char_data *ch, struct char_data *vict) {
  int dam = 0;
  int percent = rand_number(1, 101), prob = 0;
  struct affected_type af;
  struct obj_data *wep = NULL;
  
  if (vict == ch) {
    send_to_char(ch, "How can you sneak up on yourself?\r\n");
    return;
  }
  
  if (ch->in_room != vict->in_room) {
    send_to_char(ch, "That would be a bit hard.\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(ch->in_room, ROOM_SINGLEFILE)) {
    if (ch->next_in_room != vict && vict->next_in_room != ch) {
      send_to_char(ch, "You simply can't reach that far.\r\n");
      return;
    }
  }
  
  if (!AFF_FLAGGED(ch, AFF_HIDE) || !AFF_FLAGGED(ch, AFF_SNEAK)) {
    send_to_char(ch, "You have to be hidden and sneaking before you can sap "
            "anyone.\r\n");
    return;
  }
  
  if (IS_INCORPOREAL(vict)) {
    send_to_char(ch, "There is simply nothing solid there to sap.\r\n");
    return;
  }

  if (GET_SIZE(ch) < GET_SIZE(vict)) {
    send_to_char(ch, "You can't reach that high.\r\n");
    return;
  }
  
  if (GET_SIZE(ch) - GET_SIZE(vict) >= 2) {
    send_to_char(ch, "The target is too small.\r\n");
    return;
  }
  
  /* 2h bludgeon */
  if (GET_EQ(ch, WEAR_WIELD_2H) && IS_BLUNT(GET_EQ(ch, WEAR_WIELD_2H))) {
    prob += 30;
    wep = GET_EQ(ch, WEAR_WIELD_2H);
  }
          
  /* 1h bludgeon off-hand */
  if (GET_EQ(ch, WEAR_WIELD_2) && IS_BLUNT(GET_EQ(ch, WEAR_WIELD_2))) {
    prob += 10;
    wep = GET_EQ(ch, WEAR_WIELD_2);    
  }
  
  /* 1h bludgeon primary */
  if (GET_EQ(ch, WEAR_WIELD_1) && IS_BLUNT(GET_EQ(ch, WEAR_WIELD_1))) {
    prob += 10;
    wep = GET_EQ(ch, WEAR_WIELD_1);    
  }
  
  if (!prob) {
    send_to_char(ch, "You need a bludgeon weapon to make this a success...\r\n");
    return;
  }
  
  if (IS_NPC(ch))
    prob += 30;
  else
    prob += GET_SKILL(ch, SKILL_SAP) / 2;
  prob += GET_DEX(ch);
  prob -= GET_CON(vict);
  
  if (!CAN_SEE(vict, ch))
    prob += 50;
  
  if (percent < prob) {
    dam = 5 + dice(GET_LEVEL(ch), 3);
    damage(ch, vict, dam, SKILL_SAP, DAM_FORCE, FALSE);
    SET_WAIT(vict, PULSE_VIOLENCE * 2);
    GET_POS(vict) = POS_RECLINING;
    
    /* success!  critical? */
    if (prob - percent >= 20) {
      /* critical! */
      dam *= 2;
      new_affect(&af);
      af.spell = SKILL_SAP;
      SET_BIT_AR(af.bitvector, AFF_PARALYZED);
      af.duration = 2;
      affect_join(vict, &af, 1, FALSE, FALSE, FALSE);      
      act("You \tYsavagely\tn beat $N with $p!", FALSE, ch, wep, vict, TO_CHAR);
      act("$n \tYsavagely\tn beats $N with $p!!", FALSE, ch, wep, vict, TO_ROOM);
    }
  } else {
    damage(ch, vict, 0, SKILL_SAP, DAM_FORCE, FALSE);    
  }
  
  SET_WAIT(ch, PULSE_VIOLENCE * 2);
  
  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_SAP);
}

/* main engine for dirt-kick mechanic */
bool perform_dirtkick(struct char_data *ch, struct char_data *vict) {
  struct affected_type af;
  int dam = 0;
  int base_probability = 0;

  if (!CAN_SEE(ch, vict)) {
    send_to_char(ch, "You don't see well enough to attempt that.\r\n");
    return FALSE;
  }
  
  if (ROOM_FLAGGED(ch->in_room, ROOM_SINGLEFILE)) {
    if (ch->next_in_room != vict && vict->next_in_room != ch) {
      send_to_char(ch, "You simply can't reach that far.\r\n");
      return FALSE;
    }
  }

  if (AFF_FLAGGED(ch, AFF_IMMATERIAL)) {
    send_to_char(ch, "Its pretty hard to kick with immaterial legs.\r\n");
    return FALSE;
  }
  
  if (MOB_FLAGGED(vict, MOB_NOBLIND)) {
    send_to_char(ch, "Your technique is ineffective...  ");
    damage(ch, vict, 0, SKILL_DIRT_KICK, 0, FALSE);
    return FALSE;
  }
  
  if (GET_SIZE(vict) - GET_SIZE(ch) > 1) {
    send_to_char(ch, "Your target is too large for this technique to be effective!\r\n");
    return FALSE;
  }
  
  if (GET_SIZE(ch) - GET_SIZE(vict) >= 2) {
    send_to_char(ch, "Your target is too small for this technique to be effective!\r\n");
    return FALSE;
  }
  
  if (IS_NPC(ch))
    base_probability = 60;  //flate rate 60% right now
  else
    base_probability = GET_SKILL(ch, SKILL_DIRT_KICK);
  
  base_probability -= GET_LEVEL(vict) / 2;
  base_probability -= GET_DEX(vict);
  
  if (dice(1, 101) < base_probability) {
    dam = 2 + dice(1, GET_LEVEL(ch));
    damage(ch, vict, dam, SKILL_DIRT_KICK, 0, FALSE);
    
    new_affect(&af);
    af.spell = SKILL_DIRT_KICK;
    SET_BIT_AR(af.bitvector, AFF_BLIND);
    af.modifier = -4;
    af.duration = dice(1, GET_LEVEL(ch) / 5);
    af.location = APPLY_HITROLL;
    affect_join(vict, &af, 1, FALSE, FALSE, FALSE);    
  } else
    damage(ch, vict, 0, SKILL_DIRT_KICK, 0, FALSE);
  
  SET_WAIT(ch, PULSE_VIOLENCE * 2);

  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_DIRT_KICK);
  
  return TRUE;
}

/* main engine for assist mechanic */
void perform_assist(struct char_data *ch, struct char_data *helpee) {
  struct char_data *opponent = NULL;
  
  if (!ch)
    return;
  
  /* hit same opponent as person you are helping */  
  if (FIGHTING(helpee))
    opponent = FIGHTING(helpee);
  else
    for (opponent = world[IN_ROOM(ch)].people;
            opponent && (FIGHTING(opponent) != helpee);
            opponent = opponent->next_in_room);

  if (!opponent)
    act("But nobody is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
  else if (!CAN_SEE(ch, opponent))
    act("You can't see who is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
    /* prevent accidental pkill */
  else if (!CONFIG_PK_ALLOWED && !IS_NPC(opponent))
    send_to_char(ch, "You cannot kill other players.\r\n");
  else if (!MOB_CAN_FIGHT(ch)) {
    send_to_char(ch, "You can't fight!\r\n");
  } else  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
              ch->next_in_room != opponent && opponent->next_in_room != ch) {
      send_to_char(ch, "You simply can't reach that far.\r\n");
  } else {
    send_to_char(ch, "You join the fight!\r\n");
    act("$N assists you!", 0, helpee, 0, ch, TO_CHAR);
    act("$n assists $N.", FALSE, ch, 0, helpee, TO_NOTVICT);
    hit(ch, opponent, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
  }  
}

/* the primary engine for springleap */
void perform_springleap(struct char_data *ch, struct char_data *vict) {
  struct affected_type af;
  int dam = 0, prob = 0;
  
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }

  if (!CAN_SEE(ch, vict)) {
    send_to_char(ch, "You don't see well enough to attempt that.\r\n");
    return;
  }

  if (ROOM_FLAGGED(ch->in_room, ROOM_SINGLEFILE)) {
    if (ch->next_in_room != vict && vict->next_in_room != ch) {
      send_to_char(ch, "You simply can't reach that far.\r\n");
      return;
    }
  }
  
  if (IS_INCORPOREAL(vict)) {
    act("You sprawl completely through $N as you try to springleap them!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n sprawls completely through $N as $e tries to springleap $M.", FALSE, ch, 0, vict, TO_ROOM);
    SET_WAIT(ch, PULSE_VIOLENCE);
    return;
  }

  if (IS_NPC(ch))
    prob = 60;
  else
    prob = GET_SKILL(ch, SKILL_SPRINGLEAP);
  
  if (rand_number(0, 100) < prob) {
    dam = dice(6, (GET_LEVEL(ch) / 5) + 2);
    damage(ch, vict, dam, SKILL_SPRINGLEAP, DAM_FORCE, FALSE);
    SET_WAIT(vict, PULSE_VIOLENCE);
    
    new_affect(&af);
    af.spell = SKILL_SPRINGLEAP;
    if (!rand_number(0, 5))
      SET_BIT_AR(af.bitvector, AFF_PARALYZED);
    else
      SET_BIT_AR(af.bitvector, AFF_STUN);
    af.duration = dice(1, 2);
    affect_join(vict, &af, 1, FALSE, FALSE, FALSE);
  } else {
    damage(ch, vict, 0, SKILL_SPRINGLEAP, DAM_FORCE, FALSE);
  }

  GET_POS(ch) = POS_STANDING;
  SET_WAIT(ch, PULSE_VIOLENCE * 2);

  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_SPRINGLEAP);
}

/* smite evil (eventually good?) engine */
void perform_smite(struct char_data *ch, long cooldown) {
  struct affected_type af;
  
  if (char_has_mud_event(ch, eSMITE)) {
    send_to_char(ch, "You must wait longer before you can use this ability again.\r\n");
    return;
  }

  new_affect(&af);

  af.spell = SKILL_SMITE;
  af.duration = 24;

  affect_to_char(ch, &af);
  attach_mud_event(new_mud_event(eSMITE, ch, NULL), cooldown);
  send_to_char(ch, "You prepare to wreak vengeance upon your foe.\r\n");
  act("The mighty force of $N's faith blasts $n out of existence!", FALSE, NULL,
          NULL, ch, TO_NOTVICT);

  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_SMITE);
  
}

/* the primary engine for backstab */
bool perform_backstab(struct char_data *ch, struct char_data *vict) {
  int percent = -1, percent2 = -1, prob = -1, successful = 0;
  
  percent = rand_number(1, 101); /* 101% is a complete failure */
  percent2 = rand_number(1, 101); /* 101% is a complete failure */
  if (IS_NPC(ch))
    prob = 60;  // flat 60% success rate for now
  else
    prob = GET_SKILL(ch, SKILL_BACKSTAB);
  
  if (AFF_FLAGGED(ch, AFF_HIDE))
    prob += 4; //minor bonus for being hidden
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    prob += 4; //minor bonus for being sneaky

  if ((!IS_NPC(ch)&& GET_RACE(ch) == RACE_TRELUX) || (GET_EQ(ch, WEAR_WIELD_1)
          && IS_PIERCE(GET_EQ(ch, WEAR_WIELD_1)))) {
    if (AWAKE(vict) && (percent > prob)) {
      damage(ch, vict, 0, SKILL_BACKSTAB, DAM_PUNCTURE, FALSE);
    } else {
      hit(ch, vict, SKILL_BACKSTAB, DAM_PUNCTURE, 0, FALSE);
    }
    successful++;
  }

  update_pos(vict);

  if (vict && GET_POS(vict) >= POS_DEAD) {
    if (GET_RACE(ch) == RACE_TRELUX || (GET_EQ(ch, WEAR_WIELD_2) &&
            IS_PIERCE(GET_EQ(ch, WEAR_WIELD_2)))) {
      if (AWAKE(vict) && (percent2 > prob)) {
        damage(ch, vict, 0, SKILL_BACKSTAB, DAM_PUNCTURE, TRUE);
      } else {
        hit(ch, vict, SKILL_BACKSTAB, DAM_PUNCTURE, 0, TRUE);
      }
      successful++;
    }
  }

  update_pos(vict);

  if (vict) {
    if (GET_EQ(ch, WEAR_WIELD_2H) &&
            IS_PIERCE(GET_EQ(ch, WEAR_WIELD_2H)) &&
            GET_POS(vict) >= POS_DEAD) {
      if (AWAKE(vict) && (percent2 > prob)) {
        damage(ch, vict, 0, SKILL_BACKSTAB, DAM_PUNCTURE, TRUE);
      } else {
        hit(ch, vict, SKILL_BACKSTAB, DAM_PUNCTURE, 0, TRUE);
      }
      successful++;
    }
  }

  if (successful) {
    if (!IS_NPC(ch))
      increase_skill(ch, SKILL_BACKSTAB);
    SET_WAIT(ch, 2 * PULSE_VIOLENCE);
    return TRUE;
  } else
    send_to_char(ch, "You have no piercing weapon equipped.\r\n");  
  
  return FALSE;
}

/* this is the event function for whirlwind */
EVENTFUNC(event_whirlwind) {
  struct char_data *ch = NULL, *tch = NULL;
  struct mud_event_data *pMudEvent = NULL;
  struct list_data *room_list = NULL;
  int count = 0;

  /* This is just a dummy check, but we'll do it anyway */
  if (event_obj == NULL)
    return 0;

  /* For the sake of simplicity, we will place the event data in easily
   * referenced pointers */
  pMudEvent = (struct mud_event_data *) event_obj;
  ch = (struct char_data *) pMudEvent->pStruct;
  if (!IS_PLAYING(ch->desc))
    return 0;

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE)) {
    send_to_char(ch, "You simply can't reach in this area.\r\n");
    return 0;
  }
  
  /* When using a list, we have to make sure to allocate the list as it
   * uses dynamic memory */
  room_list = create_list();

  /* We search through the "next_in_room", and grab all NPCs and add them
   * to our list */
  if (!IN_ROOM(ch))
    return 0;

  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room)
    if (IS_NPC(tch))
      add_to_list(tch, room_list);

  /* If our list is empty or has "0" entries, we free it from memory and
   * close off our event */
  if (room_list->iSize == 0) {
    free_list(room_list);
    send_to_char(ch, "There is no one in the room to whirlwind!\r\n");
    return 0;
  }

  if (GET_HIT(ch) < 1)
    return 0;

  if (!IS_NPC(ch)) {
    increase_skill(ch, SKILL_WHIRLWIND);
    if (GET_SKILL(ch, SKILL_IMPROVED_WHIRL))
      increase_skill(ch, SKILL_IMPROVED_WHIRL);
  }

  /* We spit out some ugly colour, making use of the new colour options,
   * to let the player know they are performing their whirlwind strike */
  send_to_char(ch, "\t[f313]You deliver a vicious \t[f014]\t[b451]WHIRLWIND!!!\tn\r\n");

  /* Lets grab some a random NPC from the list, and hit() them up */
  for (count = dice(1, 4); count > 0; count--) {
    tch = random_from_list(room_list);
    hit(ch, tch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
  }

  /* Now that our attack is done, let's free out list */
  if (room_list)
    free_list(room_list);

  /* The "return" of the event function is the time until the event is called
   * again. If we return 0, then the event is freed and removed from the list, but
   * any other numerical response will be the delay until the next call */
  if (GET_SKILL(ch, SKILL_WHIRLWIND) < rand_number(1, 101)) {
    send_to_char(ch, "You stop spinning.\r\n");
    return 0;
  } else if (GET_SKILL(ch, SKILL_IMPROVED_WHIRL))
    return 1.5 * PASSES_PER_SEC;
  else
    return 4 * PASSES_PER_SEC;
}

/******* start offensive commands *******/

/* turn undead skill (clerics, paladins, etc) */
ACMD(do_turnundead) {
  struct char_data *vict = NULL;
  int turn_level = 0, percent = 0;
  int turn_difference = 0, turn_result = 0, turn_roll = 0;
  char buf[MAX_STRING_LENGTH] = {'\0'};

  if (CLASS_LEVEL(ch, CLASS_PALADIN) > 2)
    turn_level += CLASS_LEVEL(ch, CLASS_PALADIN) - 2;
  turn_level += CLASS_LEVEL(ch, CLASS_CLERIC);

  if (turn_level <= 0) {
    send_to_char(ch, "You do not possess the divine favor!\r\n");
    return;
  }

  one_argument(argument, buf);

  if (!(vict = get_char_room_vis(ch, buf, NULL))) {
    send_to_char(ch, "Turn who?\r\n");
    return;
  }

  if (vict == ch) {
    send_to_char(ch, "How do you plan to turn yourself?\r\n");
    return;
  }

  if (!IS_UNDEAD(vict)) {
    send_to_char(ch, "You can only attempt to turn undead!\r\n");
    return;
  }

  if (char_has_mud_event(ch, eTURN_UNDEAD)) {
    send_to_char(ch, "You must wait longer before you can use this ability again.\r\n");
    return;
  }

  percent = GET_SKILL(ch, SKILL_TURN_UNDEAD);

  if (!percent) {
    send_to_char(ch, "You lost your concentration!\r\n");
    return;
  }

  /* add cooldown, increase skill */
  attach_mud_event(new_mud_event(eTURN_UNDEAD, ch, NULL), 120 * PASSES_PER_SEC);
  increase_skill(ch, SKILL_TURN_UNDEAD);

  /* too powerful */
  if (GET_LEVEL(vict) >= LVL_IMMORT) {
    send_to_char(ch, "This undead is too powerful!\r\n");
    return;
  }

  turn_difference = (turn_level - GET_LEVEL(vict));
  turn_roll = rand_number(1, 20);

  switch (turn_difference) {
    case -5:
    case -4:
      if (turn_roll >= 20)
        turn_result = 1;
      break;
    case -3:
      if (turn_roll >= 17)
        turn_result = 1;
      break;
    case -2:
      if (turn_roll >= 15)
        turn_result = 1;
      break;
    case -1:
      if (turn_roll >= 13)
        turn_result = 1;
      break;
    case 0:
      if (turn_roll >= 11)
        turn_result = 1;
      break;
    case 1:
      if (turn_roll >= 9)
        turn_result = 1;
      break;
    case 2:
      if (turn_roll >= 6)
        turn_result = 1;
      break;
    case 3:
      if (turn_roll >= 3)
        turn_result = 1;
      break;
    case 4:
    case 5:
      if (turn_roll >= 2)
        turn_result = 1;
      break;
    default:
      turn_result = 0;
      break;
  }

  if (turn_difference <= -6)
    turn_result = 0;
  else if (turn_difference >= 6)
    turn_result = 2;

  switch (turn_result) {
    case 0: /* Undead resists turning */
      act("$N blasphemously mocks your faith!", FALSE, ch, 0, vict, TO_CHAR);
      act("You blasphemously mock $N and $S faith!", FALSE, vict, 0, ch, TO_CHAR);
      act("$n blasphemously mocks $N and $S faith!", FALSE, vict, 0, ch, TO_NOTVICT);
      hit(vict, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
      break;
    case 1: /* Undead is turned */
      act("The power of your faith overwhelms $N, who flees!", FALSE, ch, 0, vict, TO_CHAR);
      act("The power of $N's faith overwhelms you! You flee in terror!!!", FALSE, vict, 0, ch, TO_CHAR);
      act("The power of $N's faith overwhelms $n, who flees!", FALSE, vict, 0, ch, TO_NOTVICT);
      do_flee(vict, 0, 0, 0);
      break;
    case 2: /* Undead is automatically destroyed */
      act("The mighty force of your faith blasts $N out of existence!", FALSE, ch, 0, vict, TO_CHAR);
      act("The mighty force of $N's faith blasts you out of existence!", FALSE, vict, 0, ch, TO_CHAR);
      act("The mighty force of $N's faith blasts $n out of existence!", FALSE, vict, 0, ch, TO_NOTVICT);
      dam_killed_vict(ch, vict);
      break;
  }

  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_TURN_UNDEAD);
}

/* rage skill (berserk) primarily for berserkers character class */
ACMD(do_rage) {
  
  struct affected_type af, aftwo, afthree, affour;

  int bonus = 0, duration = 0;
  
  if (affected_by_spell(ch, SKILL_RAGE)) {
    send_to_char(ch, "You are already raging!\r\n");
    return;
  }
  if (!IS_ANIMAL(ch)) {
    if (!IS_NPC(ch) && !GET_SKILL(ch, SKILL_RAGE)) {
      send_to_char(ch, "You don't know how to rage.\r\n");
      return;
    }
  }
  if (char_has_mud_event(ch, eRAGE)) {
    send_to_char(ch, "You must wait longer before you can use this ability "
                     "again.\r\n");
    return;
  }

  if (IS_NPC(ch) || IS_MORPHED(ch)) {
    bonus = (GET_LEVEL(ch) / 3) + 3;
  } else {
    bonus = (CLASS_LEVEL(ch, CLASS_BERSERKER) / 3) + 3;
  }
  duration = 6 + GET_CON_BONUS(ch) * 2;
  
  send_to_char(ch, "You go into a \tRR\trA\tRG\trE\tn!.\r\n");
  act("$n goes into a \tRR\trA\tRG\trE\tn!", FALSE, ch, 0, 0, TO_ROOM);

  new_affect(&af);
  new_affect(&aftwo);
  new_affect(&afthree);
  new_affect(&affour);

  af.spell = SKILL_RAGE;
  af.duration = duration;
  af.location = APPLY_STR;
  af.modifier = bonus;

  aftwo.spell = SKILL_RAGE;
  aftwo.duration = duration;
  aftwo.location = APPLY_CON;
  aftwo.modifier = bonus;
  GET_HIT(ch) += GET_LEVEL(ch) * bonus / 2;  //little boost in current hps

  afthree.spell = SKILL_RAGE;
  afthree.duration = duration;
  afthree.location = APPLY_SAVING_WILL;
  afthree.modifier = bonus;

  //this is a penalty
  affour.spell = SKILL_RAGE;
  affour.duration = duration;
  affour.location = APPLY_AC;
  affour.modifier = bonus * 5;

  affect_to_char(ch, &af);
  affect_to_char(ch, &aftwo);
  affect_to_char(ch, &afthree);
  affect_to_char(ch, &affour);
  attach_mud_event(new_mud_event(eRAGE, ch, NULL), (180 * PASSES_PER_SEC));
  
  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_RAGE);  
 
}
#undef RAGE_AFFECTS

ACMD(do_assist) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *helpee = NULL;

  if (FIGHTING(ch)) {
    send_to_char(ch, "You're already fighting!  How can you assist someone else?\r\n");
    return;
  }
  one_argument(argument, arg);

  if (!*arg)
    send_to_char(ch, "Whom do you wish to assist?\r\n");
  else if (!(helpee = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
    send_to_char(ch, "%s", CONFIG_NOPERSON);
  else if (helpee == ch)
    send_to_char(ch, "You can't help yourself any more than this!\r\n");
  else
    perform_assist(ch, helpee);
}

ACMD(do_hit) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  struct char_data *vict = NULL;
  int chInitiative = dice(1, 20), victInitiative = dice(1, 20);

  if (!MOB_CAN_FIGHT(ch)) {
    send_to_char(ch, "But you can't fight!\r\n");
    return;
  }

  one_argument(argument, arg);

  if (!*arg)
    send_to_char(ch, "Hit who?\r\n");
  else if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
    send_to_char(ch, "They don't seem to be here.\r\n");
  else if (vict == ch) {
    send_to_char(ch, "You hit yourself...OUCH!.\r\n");
    act("$n hits $mself, and says OUCH!", FALSE, ch, 0, vict, TO_ROOM);
  } else if (FIGHTING(ch) == vict) {
    send_to_char(ch, "You are already fighting that one!\r\n");
  } else if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master == vict))
    act("$N is just such a good friend, you simply can't hit $M.", FALSE, ch, 0, vict, TO_CHAR);
  else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
      ch->next_in_room != vict && vict->next_in_room != ch) {
        send_to_char(ch, "You simply can't reach that far.\r\n");
  }
  else {
    if (!CONFIG_PK_ALLOWED && !IS_NPC(vict) && !IS_NPC(ch))
      check_killer(ch, vict);
    /* already fighting */
    if (!FIGHTING(ch)) {
      if (!IS_NPC(ch) && GET_SKILL(ch, SKILL_INITIATIVE))
        chInitiative += 8;
      chInitiative += GET_DEX(ch);
      if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_INITIATIVE))
        victInitiative += 8;
      victInitiative += GET_DEX(vict);
      if (chInitiative >= victInitiative || GET_POS(vict) < POS_FIGHTING)
        hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE); /* ch first */
      else {
        send_to_char(vict, "\tYYour superior initiative grants the first strike!\tn\r\n");
        send_to_char(ch,
                "\tyYour opponents superior \tYinitiative\ty grants the first strike!\tn\r\n");
        hit(vict, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE); // victim is first 
        update_pos(ch);
        if (!IS_NPC(vict) && GET_SKILL(vict, SKILL_INITIATIVE) &&
                GET_POS(ch) > POS_DEAD) {
          send_to_char(vict, "\tYYour superior initiative grants another attack!\tn\r\n");
          hit(vict, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
        }
      }
      SET_WAIT(ch, PULSE_VIOLENCE);
      /* not fighting, so switch opponents */
    } else {
      stop_fighting(ch);
      send_to_char(ch, "You switch opponents!\r\n");
      act("$n switches opponents!", FALSE, ch, 0, vict, TO_ROOM);
      hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
      SET_WAIT(ch, PULSE_VIOLENCE);

      //everyone gets a free shot at you unless you make a tumble check
      //15 is DC
      vict = FIGHTING(ch);
      if (FIGHTING(ch) && FIGHTING(vict)) {
        if (!IS_NPC(ch) && dice(1, 20) + compute_ability(ch, ABILITY_TUMBLE) <= 15) {
          send_to_char(ch, "\tR*Opponent Attack Opportunity*\tn");
          send_to_char(vict, "\tW*Attack Opportunity*\tn");
          hit(vict, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
          update_pos(ch);
          update_pos(vict);
        } else {
          send_to_char(ch, "\tW*Tumble Success*\tn");
          send_to_char(vict, "\tR*Tumbled vs AoO*\tn");
        }
      }

    }
  }
}

ACMD(do_kill) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  struct char_data *vict = NULL;

  if (GET_LEVEL(ch) < LVL_IMMORT || IS_NPC(ch) ||
          !PRF_FLAGGED(ch, PRF_NOHASSLE)) {
    do_hit(ch, argument, cmd, subcmd);
    return;
  }
  one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "Kill who?\r\n");
  } else {
    if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
      send_to_char(ch, "They aren't here.\r\n");
    else if (ch == vict)
      send_to_char(ch, "Your mother would be so sad.. :(\r\n");
    else if (GET_LEVEL(ch) <= GET_LEVEL(vict) ||
            PRF_FLAGGED(vict, PRF_NOHASSLE)) {
      do_hit(ch, argument, cmd, subcmd);
    } else {
      if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
              ch->next_in_room != vict && vict->next_in_room != ch) {
        send_to_char(ch, "You simply can't reach that far.\r\n");
        return;
      }

      act("You chop $M to pieces!  Ah!  The blood!", FALSE, ch, 0, vict, TO_CHAR);
      act("$N chops you to pieces!", FALSE, vict, 0, ch, TO_CHAR);
      act("$n brutally slays $N!", FALSE, ch, 0, vict, TO_NOTVICT);
      if (!IS_NPC(vict))
        raw_kill(vict, ch);
      else
        raw_kill_old(vict, ch);
    }
  }
}

ACMD(do_backstab) {
  char buf[MAX_INPUT_LENGTH];
  struct char_data *vict;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BACKSTAB)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (GET_RACE(ch) == RACE_TRELUX)
    ;
  else if (!GET_EQ(ch, WEAR_WIELD_1) && !GET_EQ(ch, WEAR_WIELD_2) && !GET_EQ(ch, WEAR_WIELD_2H)) {
    send_to_char(ch, "You need to wield a weapon to make it a success.\r\n");
    return;
  }

  one_argument(argument, buf);
  if (!(vict = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Backstab who?\r\n");
    return;
  }
  if (vict == ch) {
    send_to_char(ch, "How can you sneak up on yourself?\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
      ch->next_in_room != vict && vict->next_in_room != ch) {
    send_to_char(ch, "You simply can't reach that far.\r\n");
    return;
  }

  if (FIGHTING(vict)) {
    send_to_char(ch, "You can't backstab a fighting person -- they're too alert!\r\n");
    return;
  }

  if (MOB_FLAGGED(vict, MOB_AWARE) && AWAKE(vict)) {
    act("You notice $N lunging at you!", FALSE, vict, 0, ch, TO_CHAR);
    act("$e notices you lunging at $m!", FALSE, vict, 0, ch, TO_VICT);
    act("$n notices $N lunging at $m!", FALSE, vict, 0, ch, TO_NOTVICT);
    hit(vict, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
    return;
  }
  
  perform_backstab(ch, vict);
}

ACMD(do_order) {
  char name[MAX_INPUT_LENGTH], message[MAX_INPUT_LENGTH];
  bool found = FALSE;
  struct char_data *vict;
  struct follow_type *k;

  half_chop(argument, name, message);

  if (!*name || !*message)
    send_to_char(ch, "Order who to do what?\r\n");
  else if (!(vict = get_char_vis(ch, name, NULL, FIND_CHAR_ROOM)) &&
          !is_abbrev(name, "followers"))
    send_to_char(ch, "That person isn't here.\r\n");
  else if (ch == vict)
    send_to_char(ch, "You obviously suffer from schizophrenia.\r\n");
  else {
    if (AFF_FLAGGED(ch, AFF_CHARM)) {
      send_to_char(ch, "Your superior would not approve of you giving orders.\r\n");
      return;
    }
    if (vict) {
      char buf[MAX_STRING_LENGTH];

      snprintf(buf, sizeof (buf), "$N orders you to '%s'", message);
      act(buf, FALSE, vict, 0, ch, TO_CHAR);
      act("$n gives $N an order.", FALSE, ch, 0, vict, TO_ROOM);

      if ((vict->master != ch) || !AFF_FLAGGED(vict, AFF_CHARM))
        act("$n has an indifferent look.", FALSE, vict, 0, 0, TO_ROOM);
      else {
        send_to_char(ch, "%s", CONFIG_OK);
        command_interpreter(vict, message);
      }
    } else { /* This is order "followers" */
      char buf[MAX_STRING_LENGTH];

      snprintf(buf, sizeof (buf), "$n issues the order '%s'.", message);
      act(buf, FALSE, ch, 0, 0, TO_ROOM);

      for (k = ch->followers; k; k = k->next) {
        if (IN_ROOM(ch) == IN_ROOM(k->follower))
          if (AFF_FLAGGED(k->follower, AFF_CHARM)) {
            found = TRUE;
            command_interpreter(k->follower, message);
          }
      }

      if (found)
        send_to_char(ch, "%s", CONFIG_OK);
      else
        send_to_char(ch, "Nobody here is a loyal subject of yours!\r\n");
    }
  }
}

ACMD(do_flee) {
  char arg[MAX_INPUT_LENGTH];
  int i;

  if (GET_POS(ch) < POS_FIGHTING || GET_HIT(ch) <= 0) {
    send_to_char(ch, "You are in pretty bad shape, unable to flee!\r\n");
    return;
  }

  if (argument)
    one_argument(argument, arg);

  if (!*arg) {
    perform_flee(ch);
  } else if ((*arg && !IS_NPC(ch) && !GET_SKILL(ch, SKILL_SPRING_ATTACK))) {
    perform_flee(ch);
  } else {// there is an argument, check if its valid
    if (!GET_SKILL(ch, SKILL_SPRING_ATTACK)) {
      send_to_char(ch, "You don't have the option to choose which way to flee!\r\n");
      return;
    }
    //actually direction?
    if ((i = search_block(arg, dirs, FALSE)) >= 0) {
      if (CAN_GO(ch, i)) {
        if (do_simple_move(ch, i, 3)) {
          send_to_char(ch, "You make a tactical retreat from battle!\r\n");
          act("$n makes a tactical retreat from the battle!",
                  TRUE, ch, 0, 0, TO_ROOM);
        } else {
          send_to_char(ch, "You can't escape that direction!\r\n");
          return;
        }
      } else {
        send_to_char(ch, "You can't escape that direction!\r\n");
        return;
      }
    } else {
      send_to_char(ch, "That isn't a valid direction!\r\n");
      return;
    }
  }
  /* trying without delay */
  //SET_WAIT(ch, 10);
}


#define SPELLBATTLE_CAP  12
#define SPELLBATTLE_AFFECTS 4

ACMD(do_spellbattle) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  int number = -1, cap = SPELLBATTLE_CAP, duration = 1, i = 0;
  struct affected_type af[SPELLBATTLE_AFFECTS];

  if (IS_NPC(ch) || (!IS_NPC(ch) && GET_RACE(ch) != RACE_ARCANA_GOLEM)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_EXPERTISE) ||
          AFF_FLAGGED(ch, AFF_PARRY) ||
          AFF_FLAGGED(ch, AFF_POWER_ATTACK)) {
    send_to_char(ch, "You can't be in a combat-mode and enter "
            "spell-battle.\r\n");
    return;
  }

  if (argument)
    one_argument(argument, arg);

  /* OK no argument, that means we're attempting to turn it off */
  if (!*arg) {
    if (AFF_FLAGGED(ch, AFF_SPELLBATTLE) &&
            !char_has_mud_event(ch, eSPELLBATTLE)) {
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_SPELLBATTLE);
      send_to_char(ch, "You leave 'spellbattle' mode.\r\n");
      SPELLBATTLE(ch) = 0;
      return;
    }
    if (char_has_mud_event(ch, eSPELLBATTLE)) {
      send_to_char(ch, "You can't exit spellbattle until the cooldown wears"
              " off.\r\n");
      return;
    } else {
      send_to_char(ch, "Spellbattle requires an argument to be activated!\r\n");
      return;
    }
  }

  if (char_has_mud_event(ch, eSPELLBATTLE) ||
          affected_by_spell(ch, SKILL_SPELLBATTLE)) {
    send_to_char(ch, "You are already in spellbattle mode!\r\n");
    return;
  }

  /* ok we have an arg, lets make sure its valid */
  if (is_number(arg))
    number = atoi(arg);
  else {
    send_to_char(ch, "The argument needs to be a number!\r\n");
    return;
  }

  /* we have a cap of SPELLBATTLE_CAP or BAB or CASTER_LEVEL, whatever is
   lowest */
  if (cap > BAB(ch))
    cap = BAB(ch);
  if (cap > CASTER_LEVEL(ch))
    cap = CASTER_LEVEL(ch);

  if (number > cap) {
    send_to_char(ch, "Your maximum spellbattle level is %d!\r\n", cap);
    return;
  }

  /* passed all the tests, we should have a valid number for spellbattle */
  SPELLBATTLE(ch) = number;
  send_to_char(ch, "You are now in 'spellbattle' mode.\r\n");
  duration = (1 * SECS_PER_REAL_HOUR) / PULSE_VIOLENCE; // this should match our event duration

  /* init affect array */
  for (i = 0; i < SPELLBATTLE_AFFECTS; i++) {
    new_affect(&(af[i]));
    af[i].spell = SKILL_SPELLBATTLE;
    af[i].duration = duration;
  }

  af[0].location = APPLY_INT;
  af[0].modifier = -2;
  af[1].location = APPLY_WIS;
  af[1].modifier = -2;
  af[2].location = APPLY_CHA;
  af[2].modifier = -2;
  af[3].location = APPLY_HIT;
  af[3].modifier = SPELLBATTLE(ch) * 10;

  for (i = 0; i < SPELLBATTLE_AFFECTS; i++)
    affect_join(ch, af + i, FALSE, FALSE, FALSE, FALSE);

  SET_BIT_AR(AFF_FLAGS(ch), AFF_SPELLBATTLE);
  attach_mud_event(new_mud_event(eSPELLBATTLE, ch, NULL),
          1 * SECS_PER_REAL_HOUR);
  
  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_SPELLBATTLE);
}
#undef SPELLBATTLE_CAP
#undef SPELLBATTLE_AFFECTS

ACMD(do_expertise) {
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_EXPERTISE)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_SPELLBATTLE)) {
    send_to_char(ch, "You can't enter this mode while in spellbattle!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_EXPERTISE)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_EXPERTISE);
    send_to_char(ch, "You leave 'expertise' mode.\r\n");
    return;
  }

  send_to_char(ch, "You are now in 'expertise' mode.\r\n");

  SET_BIT_AR(AFF_FLAGS(ch), AFF_EXPERTISE);
  SET_WAIT(ch, 10);
}

ACMD(do_parry) {
  if (IS_NPC(ch) || !GET_ABILITY(ch, ABILITY_PARRY)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_SPELLBATTLE)) {
    send_to_char(ch, "You can't enter this mode while in spellbattle!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_PARRY)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_PARRY);
    send_to_char(ch, "You leave 'parry' mode.\r\n");
    return;
  }

  send_to_char(ch, "You are now in 'parry' mode.\r\n");

  SET_BIT_AR(AFF_FLAGS(ch), AFF_PARRY);
  SET_WAIT(ch, 10);
}

ACMD(do_powerattack) {
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_POWER_ATTACK)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_SPELLBATTLE)) {
    send_to_char(ch, "You can't enter this mode while in spellbattle!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_POWER_ATTACK)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_POWER_ATTACK);
    send_to_char(ch, "You leave 'power attack' mode.\r\n");
    return;
  }

  send_to_char(ch, "You are now in 'power attack' mode.\r\n");

  SET_BIT_AR(AFF_FLAGS(ch), AFF_POWER_ATTACK);
  SET_WAIT(ch, 10);
}

ACMD(do_disengage) {
  struct char_data *vict;

  if (!FIGHTING(ch)) {
    send_to_char(ch, "You aren't even fighting anyone, calm down.\r\n");
    return;
  } else if (GET_POS(ch) < POS_STANDING) {
    send_to_char(ch, "Maybe you should get on your feet first.\r\n");
    return;
  } else {
    vict = FIGHTING(ch);
    if (FIGHTING(vict) == ch) {
      send_to_char(ch, "You are too busy fighting for your life!\r\n");
      return;
    }

    stop_fighting(ch);
    send_to_char(ch, "You disengage from the fight.\r\n");
    act("$n disengages from the fight.", FALSE, ch, 0, 0, TO_ROOM);

    SET_WAIT(ch, PULSE_VIOLENCE);
  }
}

ACMD(do_taunt) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  int attempt = dice(1, 20), resist = dice(1, 20);

  one_argument(argument, arg);

  if (IS_NPC(ch) || !GET_ABILITY(ch, ABILITY_TAUNT)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch))) {
      vict = FIGHTING(ch);
    } else {
      send_to_char(ch, "Taunt who?\r\n");
      return;
    }
  }
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  if (MOB_FLAGGED(vict, MOB_NOKILL)) {
    send_to_char(ch, "This mob is protected.\r\n");
    return;
  }
  if (char_has_mud_event(ch, eTAUNTED)) {
    send_to_char(ch, "Your target is already taunted...\r\n");
    return;
  }
  if (char_has_mud_event(ch, eTAUNT)) {
    send_to_char(ch, "You must wait longer before you can use this ability again.\r\n");
    return;
  }

  attempt += compute_ability(ch, ABILITY_TAUNT);
  if (!IS_NPC(vict))
    resist += compute_ability(vict, ABILITY_CONCENTRATION);
  else
    resist += (GET_LEVEL(vict) / 2);

  if (attempt >= resist) {
    send_to_char(ch, "You taunt your opponent!\r\n");
    act("You are \tRtaunted\tn by $N!", FALSE, vict, 0, ch, TO_CHAR);
    act("$n \tWtaunts\tn $N!", FALSE, ch, 0, vict, TO_NOTVICT);
    attach_mud_event(new_mud_event(eTAUNTED, vict, NULL), 4 * PASSES_PER_SEC);
  } else {
    send_to_char(ch, "You fail to taunt your opponent!\r\n");
    act("$N fails to \tRtaunt\tn you...", FALSE, vict, 0, ch, TO_CHAR);
    act("$n fails to \tWtaunt\tn $N!", FALSE, ch, 0, vict, TO_NOTVICT);
  }
  if (!FIGHTING(vict))
    hit(vict, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
  attach_mud_event(new_mud_event(eTAUNT, ch, NULL), 8 * PASSES_PER_SEC);
}

ACMD(do_frightful) {
  struct char_data *vict, *next_vict;

  if (!IS_DRAGON(ch)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }  

  send_to_char(ch, "You ROAR!\r\n");
  act("$n lets out a mighty ROAR!", FALSE, ch, 0, 0, TO_ROOM);

  for (vict = world[IN_ROOM(ch)].people; vict; vict = next_vict) {
    next_vict = vict->next_in_room;

    if (aoeOK(ch, vict, -1) &&
            (!IS_NPC(ch) && !GET_SKILL(ch, SKILL_COURAGE))) {
      send_to_char(ch, "You roar at %s.\r\n", GET_NAME(vict));
      send_to_char(vict, "A mighty roar from %s is directed at you!\r\n",
              GET_NAME(ch));
      act("$n roars at $N!", FALSE, ch, 0, vict,
              TO_NOTVICT);

      perform_flee(vict);
      perform_flee(vict);
      perform_flee(vict);
      SET_WAIT(vict, PULSE_VIOLENCE * 3);
    }
  }

  SET_WAIT(ch, PULSE_VIOLENCE * 3);
}

ACMD(do_breathe) {
  struct char_data *vict, *next_vict;

  if (!IS_DRAGON(ch)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  send_to_char(ch, "You exhale breathing out fire!\r\n");
  act("$n exhales breathing fire!", FALSE, ch, 0, 0, TO_ROOM);

  for (vict = world[IN_ROOM(ch)].people; vict; vict = next_vict) {
    next_vict = vict->next_in_room;

    if (aoeOK(ch, vict, SPELL_FIRE_BREATHE)) {
      SET_WAIT(vict, PULSE_VIOLENCE * 1);
      if (GET_LEVEL(ch) <= 15)
        damage(ch, vict, dice(GET_LEVEL(ch), 6), SPELL_FIRE_BREATHE, DAM_FIRE,
              FALSE);
      else
        damage(ch, vict, dice(GET_LEVEL(ch), 14), SPELL_FIRE_BREATHE, DAM_FIRE,
              FALSE);
    }
  }

  SET_WAIT(ch, PULSE_VIOLENCE * 3);
}

ACMD(do_tailsweep) {
  struct char_data *vict, *next_vict;
  int percent = 0, prob = 0;

  if (!IS_DRAGON(ch)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE)) {
    send_to_char(ch, "It is too narrow to try that here.\r\n");
    return;
  }

  send_to_char(ch, "You lash out with your mighty tail!\r\n");
  act("$n lashes out with $s mighty tail!", FALSE, ch, 0, 0, TO_ROOM);

  for (vict = world[IN_ROOM(ch)].people; vict; vict = next_vict) {
    next_vict = vict->next_in_room;

    if (vict == ch)
      continue;
    if (IS_INCORPOREAL(vict))
      continue;

    // pass -2 as spellnum to handle tailsweep
    if (aoeOK(ch, vict, -2)) {
      percent = rand_number(1, 101);
      prob = rand_number(75, 100);

      if (percent > prob) {
        send_to_char(ch, "You failed to knock over %s.\r\n", GET_NAME(vict));
        send_to_char(vict, "You were able to dodge a tailsweep from %s.\r\n",
                GET_NAME(ch));
        act("$N dodges a tailsweep from $n.", FALSE, ch, 0, vict,
                TO_NOTVICT);
      } else {
        GET_POS(vict) = POS_SITTING;
        SET_WAIT(vict, PULSE_VIOLENCE * 2);

        send_to_char(ch, "You knock over %s.\r\n", GET_NAME(vict));
        send_to_char(vict, "You were knocked down by a tailsweep from %s.\r\n",
                GET_NAME(ch));
        act("$N is knocked down by a tailsweep from $n.", FALSE, ch, 0, vict,
                TO_NOTVICT);
      }

      if (GET_POS(ch) > POS_STUNNED && (FIGHTING(ch) == NULL)) // ch -> vict
        set_fighting(ch, vict);
      if (GET_POS(vict) > POS_STUNNED && (FIGHTING(vict) == NULL)) { // vict -> ch
        set_fighting(vict, ch);
        if (MOB_FLAGGED(vict, MOB_MEMORY) && !IS_NPC(ch))
          remember(vict, ch);
      }

    }
  }

  SET_WAIT(ch, PULSE_VIOLENCE * 2);
}

ACMD(do_bash) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BASH)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  
  one_argument(argument, arg);
  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch))) {
      vict = FIGHTING(ch);
    } else {
      send_to_char(ch, "Bash who?\r\n");
      return;
    }
  }
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  perform_knockdown(ch, vict, SKILL_BASH);
}

ACMD(do_trip) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

  one_argument(argument, arg);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_TRIP)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch))) {
      vict = FIGHTING(ch);
    } else {
      send_to_char(ch, "Trip who?\r\n");
      return;
    }
  }

  perform_knockdown(ch, vict, SKILL_TRIP);
}

ACMD(do_layonhands) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  struct char_data *vict = NULL;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_LAY_ON_HANDS)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  one_argument(argument, arg);
  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Whom do you want to lay hands on?\r\n");
    return;
  }
  
  perform_layonhands(ch, vict);
}

ACMD(do_crystalfist) {
  if (GET_RACE(ch) != RACE_CRYSTAL_DWARF) {
    send_to_char(ch, "How do you plan on doing that?\r\n");
    return;
  }

  if (char_has_mud_event(ch, eCRYSTALFIST)) {
    send_to_char(ch, "You must wait longer before you can use "
            "this ability again.\r\n");
    return;
  }

  send_to_char(ch, "\tCYour hands and harms grow LARGE crystals!\tn\r\n");
  act("\tCYou watch as $n's arms and hands grow LARGE crystals!\tn",
          FALSE, ch, 0, 0, TO_NOTVICT);
  attach_mud_event(new_mud_event(eCRYSTALFIST, ch, NULL),
          (8 * SECS_PER_MUD_HOUR));
}

ACMD(do_crystalbody) {
  if (GET_RACE(ch) != RACE_CRYSTAL_DWARF) {
    send_to_char(ch, "How do you plan on doing that?\r\n");
    return;
  }

  if (char_has_mud_event(ch, eCRYSTALBODY)) {
    send_to_char(ch, "You must wait longer before you can use "
            "this ability again.\r\n");
    return;
  }

  send_to_char(ch, "\tCYour crystal-like body becomes harder!\tn\r\n");
  act("\tCYou watch as $n's crystal-like body becomes harder!\tn",
          FALSE, ch, 0, 0, TO_NOTVICT);
  attach_mud_event(new_mud_event(eCRYSTALBODY, ch, NULL),
          (8 * SECS_PER_MUD_HOUR));
}

ACMD(do_treatinjury) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

  if (IS_NPC(ch)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Whom do you want to treat?\r\n");
    return;
  }

  if (char_has_mud_event(ch, eTREATINJURY)) {
    send_to_char(ch, "You must wait longer before you can use this "
            "ability again.\r\n");
    return;
  }
  
  if (FIGHTING(ch) && GET_POS(ch) < POS_FIGHTING) {
    send_to_char(ch, "You need to be in a better position in combat in order"
            " to use this ability!\r\n");
    return;
  }

  send_to_char(ch, "You skillfully dress the wounds...\r\n");
  act("Your injuries are \tWtreated\tn by $N!", FALSE, vict, 0, ch, TO_CHAR);
  act("$n \tWtreats\tn $N's injuries!", FALSE, ch, 0, vict, TO_NOTVICT);
  attach_mud_event(new_mud_event(eTREATINJURY, ch, NULL),
          (6 * SECS_PER_MUD_HOUR));
  GET_HIT(vict) += MIN((GET_MAX_HIT(vict) - GET_HIT(vict)),
          (10 + (compute_ability(ch, ABILITY_TREAT_INJURY) * 2)));
  update_pos(vict);
}

ACMD(do_rescue) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_RESCUE)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Whom do you want to rescue?\r\n");
    return;
  }

  perform_rescue(ch, vict);
}

/* built initially by vatiken as an illustration of event/lists systems 
 * of TBA, adapted to Luminari mechanics */
/*TODO:  definitely needs more balance tweaking and dummy checks for usage */
ACMD(do_whirlwind) {

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_WHIRLWIND)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (GET_POS(ch) < POS_FIGHTING) {
    send_to_char(ch, "You must be on your feet to perform a whirlwind.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE)) {
    send_to_char(ch, "It is too narrow to try that here.\r\n");
    return;
  }
  
  /* First thing we do is check to make sure the character is not in the middle
   * of a whirl wind attack.
   * 
   * "char_had_mud_event() will sift through the character's event list to see if
   * an event of type "eWHIRLWIND" currently exists. */
  if (char_has_mud_event(ch, eWHIRLWIND)) {
    send_to_char(ch, "You are already attempting that!\r\n");
    return;
  }

  send_to_char(ch, "You begin to spin rapidly in circles.\r\n");
  act("$n begins to rapidly spin in a circle!", FALSE, ch, 0, 0, TO_ROOM);

  /* NEW_EVENT() will add a new mud event to the event list of the character.
   * This function below adds a new event of "eWHIRLWIND", to "ch", and passes "NULL" as
   * additional data. The event will be called in "3 * PASSES_PER_SEC" or 3 seconds */
  NEW_EVENT(eWHIRLWIND, ch, NULL, 3 * PASSES_PER_SEC);
  SET_WAIT(ch, PULSE_VIOLENCE * 3);
}

ACMD(do_stunningfist) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_STUNNING_FIST)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  
  one_argument(argument, arg);
  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch))) {
      vict = FIGHTING(ch);
    } else {
      send_to_char(ch, "Use your stunning fist on who?\r\n");
      return;
    }
  }
  
  perform_stunningfist(ch, vict);
  
}

ACMD(do_smite) {
  int cooldown = (3 * SECS_PER_MUD_DAY);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_SMITE)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  
  if (CLASS_LEVEL(ch, CLASS_PALADIN) >= 20)
    cooldown /= 5;
  else if (CLASS_LEVEL(ch, CLASS_PALADIN) >= 15)
    cooldown /= 4;
  else if (CLASS_LEVEL(ch, CLASS_PALADIN) >= 10)
    cooldown /= 3;
  else if (CLASS_LEVEL(ch, CLASS_PALADIN) >= 5)
    cooldown /= 2;

  perform_smite(ch, cooldown);
}

/* kick engine */
void perform_kick(struct char_data *ch, struct char_data *vict) {
  int percent = 0, prob = 0;
  
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
      ch->next_in_room != vict && vict->next_in_room != ch) {
    send_to_char(ch, "You simply can't reach that far.\r\n");
    return;
  }
  
  if (IS_INCORPOREAL(vict)) {
    act("You sprawl completely through $N as you try to attack them!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n sprawls completely through $N as $e tries to attack $M.", FALSE, ch, 0, vict, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    SET_WAIT(ch, PULSE_VIOLENCE);
    return;
  }
  
  /* 101% is a complete failure */
  percent = rand_number(1, 101);
  if (IS_NPC(ch))
    prob = 60;
  else
    prob = GET_SKILL(ch, SKILL_KICK);

  if (!IS_NPC(vict) && compute_ability(vict, ABILITY_DISCIPLINE))
    percent += compute_ability(vict, ABILITY_DISCIPLINE);

  if (percent > prob) {
    damage(ch, vict, 0, SKILL_KICK, DAM_FORCE, FALSE);
  } else
    damage(ch, vict, dice(1, GET_LEVEL(ch)), SKILL_KICK, DAM_FORCE, FALSE);

  if (!IS_NPC(ch))
    increase_skill(ch, SKILL_KICK);
  SET_WAIT(ch, PULSE_VIOLENCE * 3);
  
}

ACMD(do_kick) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  struct char_data *vict = NULL;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_KICK)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  one_argument(argument, arg);
  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch))) {
      vict = FIGHTING(ch);
    } else {
      send_to_char(ch, "Kick who?\r\n");
      return;
    }
  }
  
  perform_kick(ch, vict);
}

ACMD(do_hitall) {
  int lag = 1;
  int count = 0;
  struct char_data *vict, *next_vict;
  
  if (!MOB_CAN_FIGHT(ch))
    return;

  if ((IS_NPC(ch) || !GET_SKILL(ch, SKILL_HITALL)) && (!IS_PET(ch) || IS_FAMILIAR(ch))) {
    send_to_char(ch, "But you do not know how to do that.\r\n");
    return;
  }

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE)) {
    send_to_char(ch, "It is too narrow to try that here.\r\n");
    return;
  }
  
  for (vict = world[ch->in_room].people; vict; vict = next_vict) {
    next_vict = vict->next_in_room;
    if (IS_NPC(vict) && !IS_PET(vict) && 
            (CAN_SEE(ch, vict) || 
            IS_SET_AR(ROOM_FLAGS(IN_ROOM(ch)), ROOM_MAGICDARK))) {
      count++;
      
      if (rand_number(0, 111) < GET_SKILL(ch, SKILL_HITALL) ||
              (IS_PET(ch) && rand_number(0, 101) > GET_LEVEL(ch)))
        lag++;

      if (aoeOK(ch, vict, SKILL_HITALL))
        hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, 0);
    }
  }
  lag = 1 + lag / 3 + count / 4;
  if (lag > 4)
    lag = 4;

  SET_WAIT(ch, lag * PULSE_VIOLENCE);
}


/* 
 * Original by Vhaerun, a neat skill for rogues to get an additional attack 
 * while not tanking. 
 */
ACMD(do_circle) {
  char buf[MAX_INPUT_LENGTH] = {'\0'};
  struct char_data *vict = NULL;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BACKSTAB)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (!FIGHTING(ch)) {
    send_to_char(ch, "You can only circle while in combat.\r\n");
    return;
  }

  if (is_tanking(ch)) {
    send_to_char(ch, "You are too busy defending yourself to try that.\r\n");
    return;
  }
  
  if (GET_RACE(ch) == RACE_TRELUX)
    ;
  else if (!GET_EQ(ch, WEAR_WIELD_1) && !GET_EQ(ch, WEAR_WIELD_2) && !GET_EQ(ch, WEAR_WIELD_2H)) {
    send_to_char(ch, "You need to wield a weapon to make it a success.\r\n");
    return;
  }
  
  one_argument(argument, buf);
  if (!*buf && FIGHTING(ch))
    vict = FIGHTING(ch);
  else if (!(vict = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Circle who?\r\n");
    return;
  }

  if (vict == ch) {
    send_to_char(ch, "Do not be ridiculous!\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
      ch->next_in_room != vict && vict->next_in_room != ch) {
    send_to_char(ch, "You simply can't reach that far.\r\n");
    return;
  }
  
  perform_backstab(ch, vict);
}

/* 
Vhaerun: 
    A rather useful type of bash for large humanoids to start a combat with. 
 */
ACMD(do_bodyslam) {
  struct char_data *vict;
  char buf[MAX_INPUT_LENGTH] = {'\0'};

  one_argument(argument, buf);

  if (!GET_SKILL(ch, SKILL_BODYSLAM)) {
    send_to_char(ch, "You have no idea how!\r\n");
    return;
  }

  if (IS_NPC(ch)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (FIGHTING(ch)) {
    send_to_char(ch, "You can't bodyslam while fighting!");
    return;
  }

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;

  }

  if (!*buf) {
    send_to_char(ch, "Bodyslam who?\r\n");
    return;
  }
  
  if (!(vict = get_char_room_vis(ch, buf, NULL))) {
    send_to_char(ch, "Bodyslam who?\r\n");
    return;
  }

  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  
  perform_knockdown(ch, vict, SKILL_BODYSLAM);
}

ACMD(do_headbutt) {
  struct char_data *vict = 0;
  char arg[MAX_INPUT_LENGTH] = {'\0'};

  one_argument(argument, arg);

  if (!IS_NPC(ch))
    if (!GET_SKILL(ch, SKILL_HEADBUTT)) {
      send_to_char(ch, "You have no idea how.\r\n");
      return;
    }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (!*arg || !(vict = get_char_room_vis(ch, arg, NULL))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
      vict = FIGHTING(ch);
  }
  
  if (!vict) {
    send_to_char(ch, "Headbutt who?\r\n");
    return;
  }
  
  perform_headbutt(ch, vict);
}

ACMD(do_sap) {
  struct char_data *vict = NULL;
  char buf[MAX_INPUT_LENGTH] = {'\0'};
  
  one_argument(argument, buf);

  if (!GET_SKILL(ch, SKILL_SAP)) {
    send_to_char(ch, "But you do not know how?\r\n");
    return;
  }

  if (!(vict = get_char_room_vis(ch, buf, NULL))) {
    send_to_char(ch, "Sap who?\r\n");
    return;
  }
  
  perform_sap(ch, vict);
}

ACMD(do_guard) {
  struct char_data *vict;
  char arg[MAX_INPUT_LENGTH] = {'\0'};

  if (IS_NPC(ch))
    return;

  if (affected_by_spell(ch, SKILL_RAGE)) {
    send_to_char(ch, "All you want is BLOOD!\r\n");
    return;
  }
  
  if (AFF_FLAGGED(ch, AFF_BLIND)) {
    send_to_char(ch, "You are blind, and couldn't rescue a mouse.\r\n");
    return;
  }

  one_argument(argument, arg);
  if (!*arg) {
    if (GUARDING(ch))
      act("You are guarding $N", FALSE, ch, 0, ch->char_specials.guarding, TO_CHAR);
    else
      send_to_char(ch, "You are not guarding anyone.\r\n");
    return;
  }

  if (!GET_SKILL(ch, SKILL_RESCUE)) {
    send_to_char(ch, "But you have no idea how!\r\n");
    return;
  }
  
  if (!(vict = get_char_room_vis(ch, arg, NULL))) {
    send_to_char(ch, "Whom do you want to guard?\r\n");
    return;
  }
  
  if (IS_NPC(vict) && !IS_PET(vict)) {
    send_to_char(ch, "Not even funny..\r\n");
    return;
  }

  if (GUARDING(ch)) {
    act("$n stops guarding $N", FALSE, ch, 0, GUARDING(ch), TO_ROOM);
    act("You stop guarding $N", FALSE, ch, 0, GUARDING(ch), TO_CHAR);
  }

  GUARDING(ch) = vict;
  act("$n now guards $N", FALSE, ch, 0, vict, TO_ROOM);
  act("You now guard $N", FALSE, ch, 0, vict, TO_CHAR);

}

ACMD(do_dirtkick) {
  struct char_data *vict = NULL;
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  
  one_argument(argument, arg);

  if (!IS_NPC(ch) && !GET_SKILL(ch, SKILL_DIRT_KICK)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_IMMATERIAL)) {
    send_to_char(ch, "You got no material feet to dirtkick with.\r\n");
    return;
  }
  
  if (!*arg || !(vict = get_char_room_vis(ch, arg, NULL))) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
      vict = FIGHTING(ch);
  }
  
  if (!vict) {
    send_to_char(ch, "Dirtkick who?\r\n");
    return;
  }

  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  
  perform_dirtkick(ch, vict);
}

/* 
Vhaerun: 
  Monks gamble to take down a caster, if fail, they are down for quite a while. 
 */
ACMD(do_springleap) {
  struct char_data *vict = NULL;
  char arg[MAX_INPUT_LENGTH] = {'\0'};

  one_argument(argument, arg);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_SPRINGLEAP)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (GET_POS(ch) != POS_SITTING && GET_POS(ch) != POS_RECLINING) {
    send_to_char(ch, "You must be sitting or reclining to springleap.\r\n");
    return;
  }

  if (!*arg) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
      vict = FIGHTING(ch);
  }
  else
    vict = get_char_room_vis(ch, arg, NULL);

  if (!vict) {
    send_to_char(ch, "Springleap who?\r\n");
    return;
  }
  perform_springleap(ch, vict);
}

/* 
 * Vhaerun:  A warrior skill to Stun !BASH mobs. 
 */
ACMD(do_shieldpunch) {
  struct char_data *vict = NULL;
  char arg[MAX_INPUT_LENGTH] = {'\0'};

  one_argument(argument, arg);

  if (!IS_NPC(ch) && !GET_SKILL(ch, SKILL_SHIELD_PUNCH)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (GET_POS(ch) <= POS_SITTING) {
    send_to_char(ch, "You need to get on your feet to shieldpunch.\r\n");
    return;
  }

  if (!GET_EQ(ch, WEAR_SHIELD)) {
    send_to_char(ch, "You need to wear a shield to be able to shieldpunch.\r\n");
    return;
  }

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (!*arg) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
      vict = FIGHTING(ch);
  } else
    vict = get_char_room_vis(ch, arg, NULL);

  perform_shieldpunch(ch, vict);
}



/*
 * Vhaerun:  Charging when mounted
 */
ACMD(do_charge) {
  struct char_data *vict = NULL;
  char arg[MAX_INPUT_LENGTH] = {'\0'};

  one_argument(argument, arg);

  if (!IS_NPC(ch) && !GET_SKILL(ch, SKILL_CHARGE)) {
    send_to_char(ch, "You do not know how to charge.\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (GET_POS(ch) <= POS_SITTING) {
    send_to_char(ch, "You need to stand in the stirrups to charge!\r\n");
    return;
  }
  
  if (!*arg) {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
      vict = FIGHTING(ch);
  } else
    vict = get_char_room_vis(ch, arg, NULL);

  perform_charge(ch, vict);
}

/* ranged-weapons combat, archery
 * fire command, fires single arrow - checks can_fire_arrow()
 */
ACMD(do_fire) {
  struct char_data *vict = NULL, *tch = NULL;
  char arg1[MAX_INPUT_LENGTH] = { '\0' };
  char arg2[MAX_INPUT_LENGTH] = { '\0' };
  room_rnum room = NOWHERE;
  int direction = -1, original_loc = NOWHERE;

  if (IS_NPC(ch)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  
  two_arguments(argument, arg1, arg2);
  
  /* no 2nd argument?  target room has to be same room */
  if (!*arg2) {
    room = IN_ROOM(ch);
  } else {
    /* try to find target room */
    direction = search_block(arg2, dirs, FALSE);
    if (direction < 0) {
      send_to_char(ch, "That is not a direction!\r\n");
      return;
    }
    if (!CAN_GO(ch, direction)) {
      send_to_char(ch, "You can't fire in that direction!\r\n");
      return;
    }
    room = EXIT(ch, direction)->to_room;
  }
  
  /* since we could possible no longer be in room, check if combat is ok
   in new room */
  if (ROOM_FLAGGED(room, ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  
  /* no arguments?  no go! */
  if (!*arg1) {
    send_to_char(ch, "Fire at who?\r\n");
    return;
  }
  
  /* a location has been found. */
  original_loc = IN_ROOM(ch);
  char_from_room(ch);
  char_to_room(ch, room);
  vict = get_char_room_vis(ch, arg1, NULL);

  /* check if the char is still there */
  if (IN_ROOM(ch) == room) {
    char_from_room(ch);
    char_to_room(ch, original_loc);
  }  
  
  if (!vict) {
    send_to_char(ch, "Fire at who?\r\n");
    return;
  }

  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }

  /* if target is group member, we presume you meant to assist */
  if (GROUP(ch) && room == IN_ROOM(ch)) {
    while ((tch = (struct char_data *) simple_list(GROUP(ch)->members)) !=
            NULL) {
      if (IN_ROOM(tch) != IN_ROOM(vict))
        continue;
      if (vict == tch) {
        vict = FIGHTING(vict);
        break;
      }
    }
  }
  
  /* maybe its your pet?  so assist */
  if (IS_PET(vict) && vict->master == ch && room == IN_ROOM(ch))
    vict = FIGHTING(vict);
  
  if (can_fire_arrow(ch, FALSE)) {
    hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, 2);  // 2 in last arg indicates ranged
    if (IN_ROOM(ch) != IN_ROOM(vict))
      stop_fighting(ch);
    else
      FIRING(ch) = TRUE;      
    SET_WAIT(ch, PULSE_VIOLENCE);
  }
}

/* ranged-weapons combat, archery
 * autofire command, fires single arrow - checks can_fire_arrow()
 * sets FIRING()
 */
ACMD(do_autofire) {
  char arg[MAX_INPUT_LENGTH] = { '\0' };  
  struct char_data *vict = NULL, *tch = NULL;

  one_argument(argument, arg);

  if (IS_NPC(ch)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  
  if (!*arg) {
    send_to_char(ch, "Fire at who?\r\n");
    return;
  }
  
  vict = get_char_room_vis(ch, arg, NULL);

  while ((tch = (struct char_data *) simple_list(GROUP(ch)->members)) !=
          NULL) {
    if (IN_ROOM(tch) != IN_ROOM(vict))
      continue;
    if (vict == tch) {
      vict = FIGHTING(vict);
      break;
    }
  }
  
  if (!vict) {
    send_to_char(ch, "Fire at who?\r\n");
    return;
  }

  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }

  if (can_fire_arrow(ch, FALSE)) {
    hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, 2);  // 2 in last arg indicates ranged
    FIRING(ch) = TRUE;
    SET_WAIT(ch, PULSE_VIOLENCE);
  }
}

/* function used to gather up all the ammo in the room/corpses-in-room */
ACMD(do_collect) {
  struct obj_data *quiver = GET_EQ(ch, WEAR_QUIVER);
  struct obj_data *obj = NULL;
  struct obj_data *nobj = NULL;
  struct obj_data *cobj = NULL;
  struct obj_data *next_obj = NULL;
  int ammo = 0;
  bool fit = TRUE;
  char buf[MAX_INPUT_LENGTH] = { '\0' };  
  
  if (!quiver) {
    send_to_char(ch, "But you don't have an ammo pouch to collect to.\r\n");
    return;
  }

  for (obj = world[ch->in_room].contents; obj; obj = nobj) {
    nobj = obj->next_content;

    /* checking corpse for ammo first */
    if (IS_CORPSE(obj)) {
      for (cobj = obj->contains; cobj; cobj = next_obj) {
        next_obj = cobj->next_content;
        if (GET_OBJ_TYPE(cobj) == ITEM_MISSILE &&
                MISSILE_ID(cobj) == GET_IDNUM(ch)) {
          if (num_obj_in_obj(quiver) < GET_OBJ_VAL(quiver, 0)) {
            obj_from_obj(cobj);
            obj_to_obj(cobj, quiver);
            ammo++;
            act("You get $p.", FALSE, ch, cobj, 0, TO_CHAR);
          } else {
            fit = FALSE;
            break;
          }
        }
      }

      /* checking room for ammo */
    } else if (GET_OBJ_TYPE(obj) == ITEM_MISSILE &&
            MISSILE_ID(obj) == GET_IDNUM(ch)) {
      if (num_obj_in_obj(quiver) < GET_OBJ_VAL(quiver, 0)) {
        obj_from_room(obj);
        obj_to_obj(obj, quiver);
        ammo++;
        act("You get $p.", FALSE, ch, obj, 0, TO_CHAR);
      } else {
        fit = FALSE;
        break;
      }
    }
  }

  sprintf(buf, "You collected ammo:  %d.\r\n", ammo);
  send_to_char(ch, buf);
  if (!fit)
    send_to_char(ch, "There are still some of your ammunition laying around that does not\r\nfit into your currently"
          " equipped ammo pouch.\r\n");

  act("$n gathers $s ammunition.", FALSE, ch, 0, 0, TO_ROOM);
}


/* unfinished */
/*
ACMD(do_disarm) {
  struct char_data *vict = FIGHTING(ch);
  int pos;
  struct obj_data *wielded = 0;
  int mod = 0;

  if (!GET_SKILL(ch, SKILL_DISARM)) {
    send_to_char(ch, "But you do not know how.\r\n");
    return;
  }


  if (AFF2_FLAGGED(ch, AFF2_MAJOR_PARA) || AFF2_FLAGGED(ch, AFF2_MINOR_PARA)) {
    send_to_char(ch, "You are paralysed to the bone.\r\n");
    return;
  }

  if (!vict) {
    send_to_char(ch, "You can only try to disarm the opponent you are fighting.\r\n");
    return;
  }

  if (!CAN_SEE(ch, vict)) {
    send_to_char(ch, "You don't see well enough to attempt that.\r\n");
    return;
  }

  if (GET_EQ(vict, WEAR_WIELD_2H)) {
    wielded = GET_EQ(vict, WEAR_WIELD_2H);
    pos = WEAR_WIELD_2H;
    mod = -25;
  } else {
    wielded = GET_EQ(vict, WEAR_WIELD_P);
    pos = WEAR_WIELD_P;
  }
  if (!wielded) {
    wielded = GET_EQ(vict, WEAR_WIELD_S);
    pos = WEAR_WIELD_S;
  }
  if (!wielded) {
    act("But $N is not wielding anything.", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }


  if (skill_test(ch, SKILL_DISARM, 500, mod + GET_R_DEX(ch) / 10 - GET_R_STR(vict) / 15) && !IS_OBJ_STAT(wielded, ITEM_NODROP)) {
    act("$n disarms $N of $S $p.", FALSE, ch, wielded, vict, TO_ROOM);
    act("You manage to knock $p out of $N's hands.", FALSE, ch, wielded, vict, TO_CHAR);
    obj_to_room(unequip_char(vict, pos), vict->in_room);
  } else {
    act("$n failed to disarm $N.", FALSE, ch, 0, vict, TO_ROOM);
    act("You failed to disarm $N.", FALSE, ch, 0, vict, TO_CHAR);
  }

  WAIT_STATE(ch, 3 * PULSE_VIOLENCE);
}
*/



