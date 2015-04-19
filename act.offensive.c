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
#include "actions.h"
#include "actionqueues.h"
#include "assign_wpn_armor.h"

/**** Utility functions *******/


/* ranged combat (archery, etc)
 * this function will check to make sure ammo is ready for firing
 */
bool has_missile_in_ammo_pouch(struct char_data *ch, struct obj_data *wielded, bool silent) {
  struct obj_data *ammo_pouch = GET_EQ(ch, WEAR_AMMO_POUCH);

  if (!ammo_pouch) {
    if (!silent)
      send_to_char(ch, "You have no ammo pouch!\r\n");
    FIRING(ch) = FALSE;
    return FALSE;
  }

  if (!ammo_pouch->contains) {
    if (!silent)
      send_to_char(ch, "Your ammo pouch is empty!\r\n");
    FIRING(ch) = FALSE;
    return FALSE;
  }

  if (GET_OBJ_TYPE(ammo_pouch->contains) != ITEM_MISSILE) {
    if (!silent)
      send_to_char(ch, "Your ammo pouch needs to be filled with only ammo!\r\n");
    FIRING(ch) = FALSE;
    return FALSE;
  }

  if (GET_OBJ_VAL(wielded, 0) != GET_OBJ_VAL(ammo_pouch->contains, 0)) {
    if (!silent)
      act("Your $p does not fit your weapon.", FALSE, ch, ammo_pouch->contains, NULL, TO_CHAR);
    FIRING(ch) = FALSE;
    return FALSE;
  }

  return TRUE;
}

/* ranged combat (archery, etc)
 * this function will check for a ranged weapon, ammo and does
 * a check of "has_missile_in_ammo_pouch"
 */
bool can_fire_arrow(struct char_data *ch, bool silent) {
  if (!GET_EQ(ch, WEAR_AMMO_POUCH)) {
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

  if (!has_missile_in_ammo_pouch(ch, obj, TRUE)) {
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

  if (wield == 2 && GET_EQ(ch, WEAR_WIELD_OFFHAND) &&
          GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD_OFFHAND), 3) == TYPE_PIERCE - TYPE_HIT) {
    return TRUE;
  }

  if (wield == 3 && GET_EQ(ch, WEAR_WIELD_2H) &&
          GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD_2H), 3) == TYPE_PIERCE - TYPE_HIT) {
    return TRUE;
  }

  return FALSE;
}

/*  End utility */


/* quivering palm engine: The quivering palm is reliant on a successful UNARMED
 * attack (or an attack with a KI_STRIKE weapon) */
void perform_quiveringpalm(struct char_data *ch) {
  struct affected_type af;

  new_affect(&af);
  af.spell = SKILL_QUIVERING_PALM;
  af.duration = 24;

  affect_to_char(ch, &af);

  if (!IS_NPC(ch))
    start_daily_use_cooldown(ch, FEAT_QUIVERING_PALM);

  send_to_char(ch, "You beging to meditate and focus all your ki energy towards your palms...  \tBYour body begins to vibrate with massive amounts of \tYenergy\tB.\tn");
  act("$n's body begins to \tBvibrate\tn with massive amounts of \tYenergy\tn!", FALSE, ch, 0, 0, TO_ROOM);
}

/* stunningfist engine */
/* The stunning fist is reliant on a successful UNARMED attack (or an attack with a KI_STRIKE weapon) */
void perform_stunningfist(struct char_data *ch) {
  struct affected_type af;

  new_affect(&af);
  af.spell = SKILL_STUNNING_FIST;
  af.duration = 24;

  affect_to_char(ch, &af);

  if (!IS_NPC(ch))
    start_daily_use_cooldown(ch, FEAT_STUNNING_FIST);

  send_to_char(ch, "You focus your Ki energies and prepare a disabling unarmed attack.\r\n");
  act("$n's focuses $s Ki, preparing a disabling unarmed attack!", FALSE, ch, 0, 0, TO_ROOM);
}

/* rp_suprise_accuracy engine */
/* The suprise-accuracy is reliant on rage */
void perform_supriseaccuracy(struct char_data *ch) {
  struct affected_type af;

  new_affect(&af);
  af.spell = SKILL_SUPRISE_ACCURACY;
  af.duration = 24;

  affect_to_char(ch, &af);

  attach_mud_event(new_mud_event(eSUPRISE_ACCURACY, ch, NULL), SECS_PER_MUD_DAY * 1);

  send_to_char(ch, "You focus your rage and prepare a surprise accurate attack.\r\n");
  act("$n's focuses $s rage, preparing a surprise accuracy attack!", FALSE, ch, 0, 0, TO_ROOM);
}

/* rp_come_and_get_me engine */
/* The come and get me is reliant on rage */
void perform_comeandgetme(struct char_data *ch) {
  struct affected_type af;

  new_affect(&af);
  af.spell = SKILL_COME_AND_GET_ME;
  af.duration = 2;

  affect_to_char(ch, &af);

  attach_mud_event(new_mud_event(eCOME_AND_GET_ME, ch, NULL), SECS_PER_MUD_DAY * 1);

  send_to_char(ch, "You focus your rage and prepare to SMASH.\r\n");
  act("$n's focuses $s rage, preparing to SMASH!", FALSE, ch, 0, 0, TO_ROOM);
}

/* rp_powerful_blow engine */
/* The powerful blow is reliant on rage */
void perform_powerfulblow(struct char_data *ch) {
  struct affected_type af;

  new_affect(&af);
  af.spell = SKILL_POWERFUL_BLOW;
  af.duration = 24;

  affect_to_char(ch, &af);

  attach_mud_event(new_mud_event(ePOWERFUL_BLOW, ch, NULL), SECS_PER_MUD_DAY * 1);

  send_to_char(ch, "You focus your rage and prepare a powerful blow.\r\n");
  act("$n's focuses $s rage, preparing a powerful blow!", FALSE, ch, 0, 0, TO_ROOM);
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

  if (AFF_FLAGGED(ch, AFF_FATIGUED)) {
    send_to_char(ch, "You are are too fatigued to rage!\r\n");
    return;
  }

  if (affected_by_spell(ch, SKILL_RAGE)) {
    send_to_char(ch, "You are already raging!\r\n");
    return;
  }

  if (IS_NPC(ch) || IS_MORPHED(ch)) {
    bonus = (GET_LEVEL(ch) / 3) + 3;
  } else {
    bonus = 4;
    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_GREATER_RAGE))
      bonus += 2;
    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_MIGHTY_RAGE))
      bonus += 3;
    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_INDOMITABLE_RAGE))
      bonus += 3;
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
  af[0].bonus_type = BONUS_TYPE_MORALE;

  af[1].location = APPLY_CON;
  af[1].modifier = bonus;
  GET_HIT(ch) += GET_LEVEL(ch) * bonus; //little boost in current hps
  af[1].bonus_type = BONUS_TYPE_MORALE;

  af[2].location = APPLY_SAVING_WILL;
  af[2].modifier = bonus;
  af[2].bonus_type = BONUS_TYPE_MORALE;

  //this is a penalty
  af[3].location = APPLY_AC_NEW;
  af[3].modifier = -2;

  for (i = 0; i < RAGE_AFFECTS; i++)
    affect_join(ch, af + i, FALSE, FALSE, FALSE, FALSE);

  /* Add another affect for heavy shrug. */
//  if (HAS_FEAT(ch, FEAT_RP_HEAVY_SHRUG)) {
//    struct affected_type heavy_shrug_af;
//    struct damage_reduction_type *new_dr;
//
//    new_affect(&heavy_shrug_af);
//    heavy_shrug_af.spell = SKILL_RAGE;
//    heavy_shrug_af.duration = duration;
//    heavy_shrug_af.location = APPLY_DR;
//    heavy_shrug_af.modifier = 0;
//
//    CREATE(new_dr, struct damage_reduction_type, 1);
//
//    new_dr->bypass_cat[0] = DR_BYPASS_CAT_NONE;
//    new_dr->bypass_val[0] = 0;
//
//    new_dr->bypass_cat[1] = DR_BYPASS_CAT_UNUSED;
//    new_dr->bypass_val[1] = 0; /* Unused. */
//
//    new_dr->bypass_cat[2] = DR_BYPASS_CAT_UNUSED;
//    new_dr->bypass_val[2] = 0; /* Unused. */
//
//    new_dr->amount     = 3;
//    new_dr->max_damage = -1;
//    new_dr->spell      = SKILL_RAGE;
//    new_dr->feat       = FEAT_RP_HEAVY_SHRUG;
//    new_dr->next       = GET_DR(ch);
//    GET_DR(ch) = new_dr;
//
//    affect_join(ch, &heavy_shrug_af, FALSE, FALSE, FALSE, FALSE);
//
//  }

  attach_mud_event(new_mud_event(eRAGE, ch, NULL), (180 * PASSES_PER_SEC));

  USE_STANDARD_ACTION(ch);
}

/* rescue skill mechanic */
void perform_rescue(struct char_data *ch, struct char_data *vict) {
  struct char_data *tmp_ch;

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

  if (attack_roll(ch, vict, ATTACK_TYPE_PRIMARY, FALSE, 1) <= 0) {
    send_to_char(ch, "You fail the rescue!\r\n");
    return;
  }

  act("You place yourself between $N and $S opponent, rescuing $M!", FALSE, ch, 0, vict, TO_CHAR);
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

  USE_FULL_ROUND_ACTION(ch);
}

/* charge mechanic */
#define CHARGE_AFFECTS 3
void perform_charge(struct char_data *ch) {
  struct affected_type af[CHARGE_AFFECTS];
  extern struct index_data *mob_index;
  int (*name)(struct char_data *ch, void *me, int cmd, char *argument);
  int i = 0;

  if (AFF_FLAGGED(ch, AFF_CHARGING)) {
    send_to_char(ch, "You are already charging!\r\n");
    return;
  }

  /* init affect array */
  for (i = 0; i < CHARGE_AFFECTS; i++) {
    new_affect(&(af[i]));
    af[i].spell = SKILL_CHARGE;
    af[i].duration = 2;
  }

  SET_BIT_AR(af[0].bitvector, AFF_CHARGING);

  af[1].location = APPLY_HITROLL; /* bonus */
  af[1].modifier = 2;

  af[2].location = APPLY_AC; /* penalty */
  af[2].modifier = -2;

  for (i = 0; i < CHARGE_AFFECTS; i++)
    affect_join(ch, af + i, FALSE, FALSE, FALSE, FALSE);

  if (RIDING(ch)) {
    act("You urge $N forward for a \tYcharge\tn!", FALSE, ch, NULL, RIDING(ch), TO_CHAR);
    act("$n urges you forward for a \tYcharge\tn!", FALSE, ch, NULL, RIDING(ch), TO_VICT);
    act("$n urges $N forward for a \tYcharge\tn!", FALSE, ch, NULL, RIDING(ch), TO_NOTVICT);
    name = mob_index[GET_MOB_RNUM(RIDING(ch))].func;
    if (name)
      (name)(ch, RIDING(ch), 0, "charge");
  } else {
    act("You run forward for a \tYcharge\tn!", FALSE, ch, NULL, NULL, TO_CHAR);
    act("$n runs forward for a \tYcharge\tn!", FALSE, ch, NULL, NULL, TO_NOTVICT);
  }

  if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_RIDE_BY_ATTACK)) {
    USE_MOVE_ACTION(ch);
  } else {
    USE_FULL_ROUND_ACTION(ch);
  }
}
#undef CHARGE_AFFECTS

/* engine for knockdown, used in bash/trip/etc */
bool perform_knockdown(struct char_data *ch, struct char_data *vict, int skill) {
  int penalty = 0, attack_check = 0, defense_check = 0;
  bool success = FALSE, counter_success = FALSE;

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
  if (GET_POS(vict) == POS_SITTING) {
    send_to_char(ch, "You can't knock down something that is already down!\r\n");
    return FALSE;
  }
  if (IS_INCORPOREAL(vict)) {
    act("You sprawl completely through $N as you try to attack $M, slamming into the ground!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n sprawls completely through $N as $e tries to attack $M, slamming into the ground!", FALSE, ch, 0, vict, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    return FALSE;
  }
  if (MOB_FLAGGED(vict, MOB_NOBASH)) {
    send_to_char(ch, "You realize you will probably not succeed:  ");
    penalty = -100;
  }

  switch (skill) {
    case SKILL_BODYSLAM:
    case SKILL_SHIELD_CHARGE:
      attack_check = GET_STR_BONUS(ch);
      break;
    case SKILL_BASH:
      attack_check = GET_STR_BONUS(ch);
      if (AFF_FLAGGED(vict, AFF_FLYING)) {
        send_to_char(ch, "Impossible, your target is flying!\r\n");
        return FALSE;
      }
      if (!HAS_FEAT(ch, FEAT_IMPROVED_TRIP))
        attack_of_opportunity(vict, ch, 0);
      break;
    case SKILL_TRIP:
      attack_check = GET_DEX_BONUS(ch);
      if (AFF_FLAGGED(vict, AFF_FLYING)) {
        send_to_char(ch, "Impossible, your target is flying!\r\n");
        return FALSE;
      }
      if (!HAS_FEAT(ch, FEAT_IMPROVED_TRIP))
        attack_of_opportunity(vict, ch, 0);
      break;
    default:
      log("Invalid skill sent to perform knockdown!\r\n");
      return FALSE;
  }

  /* Perform the unarmed touch attack */
  if ((attack_roll(ch, vict, ATTACK_TYPE_UNARMED, TRUE, 1) + penalty) > 0) {
    /* Successful unarmed touch attacks. */

    /* Perform strength check. */
    attack_check += dice(1, 20) + size_modifiers[GET_SIZE(ch)]; /* we added stat bonus above */
    defense_check = dice(1, 20) + MAX(GET_STR_BONUS(vict), GET_DEX_BONUS(vict)) + size_modifiers[GET_SIZE(vict)];

    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_IMPROVED_TRIP)) {
      /* You do not provoke an attack of opportunity when you attempt to trip an opponent while you are unarmed.
       * You also gain a +4 bonus on your Strength check to trip your opponent.
       *
       * If you trip an opponent in melee combat, you immediately get a melee attack against that opponent as if
       * you hadn't used your attack for the trip attempt.
       *
       * Normal:
       * Without this feat, you provoke an attack of opportunity when you attempt to trip an opponent while you
       * are unarmed. */
      attack_check += 4;
    }

    if (GET_RACE(vict) == RACE_DWARF ||
            GET_RACE(vict) == RACE_CRYSTAL_DWARF) /* dwarven stability */
      defense_check += 4;

    /*DEBUG*/ /*send_to_char(ch, "attack check: %d, defense_check: %d\r\n", attack_check, defense_check);*/
    if (attack_check >= defense_check) {
      if (attack_check == defense_check) {
        /* Check the bonuses. */
        if ((GET_STR_BONUS(ch) + (GET_SIZE(ch) - GET_SIZE(vict))*4) >= (MAX(GET_STR_BONUS(vict), GET_DEX_BONUS(vict))))
          success = TRUE;
        else
          success = FALSE;
      } else {
        success = TRUE;
      }
    }

    if (success == TRUE) {
      /* Messages for shield charge */
      if (skill == SKILL_SHIELD_CHARGE) {
        act("\tyYou knock $N to the ground!\tn", FALSE, ch, NULL, vict, TO_CHAR);
        act("\ty$n knocks you to the ground!\tn", FALSE, ch, NULL, vict, TO_VICT);
        act("\ty$n knocks $N to the ground!\tn", FALSE, ch, NULL, vict, TO_NOTVICT);
      } else {
        /* Messages for successful trip */
        act("\tyYou grab and overpower $N, throwing $M to the ground!\tn", FALSE, ch, NULL, vict, TO_CHAR);
        act("\ty$n grabs and overpowers you, throwing you to the ground!\tn", FALSE, ch, NULL, vict, TO_VICT);
        act("\ty$n grabs and overpowers $N, throwing $M to the ground!\tn", FALSE, ch, NULL, vict, TO_NOTVICT);
      }
    } else {
      /* Messages for shield charge */
      if (skill == SKILL_SHIELD_CHARGE) {
        /* just moved this to damage-messages */
      } else {
        /* Messages for failed trip */
        if (GET_STR_BONUS(vict) >= GET_DEX_BONUS(vict)) {
          act("\tyYou grab $N but $E tears out of your grasp!\tn", FALSE, ch, NULL, vict, TO_CHAR);
          act("\ty$n grabs you but you tear yourself from $s grasp!\tn", FALSE, ch, NULL, vict, TO_VICT);
          act("\ty$n grabs $N but $E tears out of $s grasp!\tn", FALSE, ch, NULL, vict, TO_NOTVICT);
        } else {
          act("\tyYou grab $N but $E deftly turns away from your attack!\tn", FALSE, ch, NULL, vict, TO_CHAR);
          act("\ty$n grabs you and you deftly turn away from $s attack!\tn", FALSE, ch, NULL, vict, TO_VICT);
          act("\ty$n grabs $N but $E deftly turns away from $s attack!\tn", FALSE, ch, NULL, vict, TO_NOTVICT);
        }
      }
      if (skill != SKILL_SHIELD_CHARGE) {
        /* Victim gets a chance to countertrip */
        attack_check = (dice(1, 20) + GET_STR_BONUS(vict) + (GET_SIZE(vict) - GET_SIZE(ch))*4);
        defense_check = (dice(1, 20) + MAX(GET_STR_BONUS(ch), GET_DEX_BONUS(ch)));

        if (GET_RACE(ch) == RACE_DWARF ||
                GET_RACE(ch) == RACE_CRYSTAL_DWARF) /* Dwarves get a stability bonus. */
          defense_check += 4;
        /*DEBUG*/ /*send_to_char(ch, "counterattack check: %d, defense_check: %d\r\n", attack_check, defense_check);*/

        if (attack_check >= defense_check) {
          if (attack_check == defense_check) {
            /* Check the bonuses. */
            if ((GET_STR_BONUS(vict) + (GET_SIZE(vict) - GET_SIZE(ch))*4) >= (MAX(GET_STR_BONUS(ch), GET_DEX_BONUS(ch))))
              counter_success = TRUE;
            else
              counter_success = FALSE;
          } else {
            counter_success = TRUE;
          }
        }

        /* No tripping someone already on the ground. */
        if (GET_POS(ch) == POS_SITTING) {
          act("\tyYou can't counter-trip someone who is already down!\tn", FALSE, ch, NULL, vict, TO_VICT);
        } else if (counter_success == TRUE) {
          /* Messages for successful counter-trip */
          act("\tyTaking advantage of your failed attack, $N throws you to the ground!\tn", FALSE, ch, NULL, vict, TO_CHAR);
          act("\tyTaking advantage of $n's failed attack, you throw $m to the ground!\tn", FALSE, ch, NULL, vict, TO_VICT);
          act("\tyTaking advantage of $n's failed attack, $N throws $m to the ground!\tn", FALSE, ch, NULL, vict, TO_NOTVICT);
        } else {
          /* Messages for failed counter-trip */
          if (GET_STR_BONUS(ch) >= GET_DEX_BONUS(ch)) {
            act("\tyYou resist $N's attempt to take advantage of your failed attack.\tn", FALSE, ch, NULL, vict, TO_CHAR);
            act("\ty$n resists your attempt to take advantage of $s failed attack.\tn", FALSE, ch, NULL, vict, TO_VICT);
            act("\ty$n resists $N's attempt to take advantage of $s failed attack.\tn", FALSE, ch, NULL, vict, TO_NOTVICT);
          } else {
            act("\tyYou twist away from $N's attempt to take advantage of your failed attack.\tn", FALSE, ch, NULL, vict, TO_CHAR);
            act("\ty$n twists away from your attempt to take advantage of $s failed attack.\tn", FALSE, ch, NULL, vict, TO_VICT);
            act("\ty$n twists away from $N's attempt to take advantage of $s failed attack.\t\n", FALSE, ch, NULL, vict, TO_NOTVICT);
          }
        }
      }
    }
  } else {
    /* Messages for a missed unarmed touch attack. */
    if (skill == SKILL_SHIELD_CHARGE) {
        /* just moved this to damage-messages */
    } else {
      act("\tyYou are unable to grab $N!\tn", FALSE, ch, NULL, vict, TO_CHAR);
      act("\ty$n tries to grab you, but you dodge easily away!\tn", FALSE, ch, NULL, vict, TO_VICT);
      act("\ty$n tries to grab $N, but $N dodges easily away!\tn", FALSE, ch, NULL, vict, TO_NOTVICT);
    }
  }

  if (!success) {
    if (counter_success) {
      GET_POS(ch) = POS_SITTING;
    }
  } else {
    GET_POS(vict) = POS_SITTING;
    if ((skill == SKILL_TRIP) ||
            (skill == SKILL_BASH) ||
            (skill == SKILL_SHIELD_CHARGE)) {
      /* Successful trip. */
      if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_IMPROVED_TRIP)) {
        /* You get a free swing on the tripped opponent. */
        hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
      }
    }
  }
  if (vict != ch) {
    if (GET_POS(ch) > POS_STUNNED && (FIGHTING(ch) == NULL))
      set_fighting(ch, vict);
    if (GET_POS(vict) > POS_STUNNED && (FIGHTING(vict) == NULL)) {
      set_fighting(vict, ch);
    }
  }

  return TRUE;
}

/* shieldpunch engine :
 * Perform the shield punch, check for proficiency and the required
 * equipment, also check for any enhancing feats. */
bool perform_shieldpunch(struct char_data *ch, struct char_data *vict) {
  extern struct index_data *obj_index;
  int (*name)(struct char_data *ch, void *me, int cmd, char *argument);
  struct obj_data *shield = GET_EQ(ch, WEAR_SHIELD);

  if (!shield) {
    send_to_char(ch, "You need a shield to do that.\r\n");
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

  if (!HAS_FEAT(ch, FEAT_ARMOR_PROFICIENCY_SHIELD)) {
    send_to_char(ch, "You are not proficient enough in the use of your shield to shieldpunch.\r\n");
    return FALSE;
  }

  if (!HAS_FEAT(ch, FEAT_IMPROVED_SHIELD_PUNCH)) {
    /* Remove shield bonus from ac. */
    attach_mud_event(new_mud_event(eSHIELD_RECOVERY, ch, NULL), PULSE_VIOLENCE);
  }

  /*  Use an attack mechanic to determine success. */
  if (attack_roll(ch, vict, ATTACK_TYPE_OFFHAND, FALSE, 1) <= 0) {
    damage(ch, vict, 0, SKILL_SHIELD_PUNCH, DAM_FORCE, FALSE);
  } else {
    damage(ch, vict, dice(1, 6) + (GET_STR_BONUS(ch) / 2), SKILL_SHIELD_PUNCH, DAM_FORCE, FALSE);
    name = obj_index[GET_OBJ_RNUM(shield)].func;
    if (name)
      (name)(ch, shield, 0, "shieldpunch");
  }

  return TRUE;
}

/* shieldcharge engine :
 * Perform the shieldcharge, check for proficiency and the required
 * equipment, also check for any enhancing feats. Perform a trip attack
 * on success
 *
 * Note - Charging gives +2 to your attack
 */
bool perform_shieldcharge(struct char_data *ch, struct char_data *vict) {
  extern struct index_data *obj_index;
  int (*name)(struct char_data *ch, void *me, int cmd, char *argument);
  struct obj_data *shield = GET_EQ(ch, WEAR_SHIELD);

  if (!shield) {
    send_to_char(ch, "You need a shield to do that.\r\n");
    return FALSE;
  }

  if (!vict) {
    send_to_char(ch, "Shieldcharge who?\r\n");
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

  if (!HAS_FEAT(ch, FEAT_ARMOR_PROFICIENCY_SHIELD)) {
    send_to_char(ch, "You are not proficient enough in the use of your shield to shieldpunch.\r\n");
    return FALSE;
  }

  /*  Use an attack mechanic to determine success.
   *  Add 2 for the charge bonus!  */
  if (attack_roll(ch, vict, ATTACK_TYPE_OFFHAND, FALSE, 1) + 2 <= 0) {
    damage(ch, vict, 0, SKILL_SHIELD_CHARGE, DAM_FORCE, FALSE);
  } else {
    damage(ch, vict, dice(1, 6) + GET_STR_BONUS(ch), SKILL_SHIELD_CHARGE, DAM_FORCE, FALSE);
    name = obj_index[GET_OBJ_RNUM(shield)].func;
    if (name)
      (name)(ch, shield, 0, "shieldcharge");

    perform_knockdown(ch, vict, SKILL_SHIELD_CHARGE);
  }

  USE_STANDARD_ACTION(ch);
  USE_MOVE_ACTION(ch);

  return TRUE;
}

/* shieldslam engine :
 * Perform the shield slam, check for proficiency and the required
 * equipment, also check for any enhancing feats. */
bool perform_shieldslam(struct char_data *ch, struct char_data *vict) {
  struct affected_type af;
  extern struct index_data *obj_index;
  int (*name)(struct char_data *ch, void *me, int cmd, char *argument);
  struct obj_data *shield = GET_EQ(ch, WEAR_SHIELD);

  if (!shield) {
    send_to_char(ch, "You need a shield to do that.\r\n");
    return FALSE;
  }

  if (!HAS_FEAT(ch, FEAT_SHIELD_SLAM)) {
    send_to_char(ch, "You don't know how to do that.\r\n");
    return FALSE;
  }

  if (!vict) {
    send_to_char(ch, "Shieldslam who?\r\n");
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

  if (!HAS_FEAT(ch, FEAT_ARMOR_PROFICIENCY_SHIELD)) {
    send_to_char(ch, "You are not proficient enough in the use of your shield to shieldslam.\r\n");
    return FALSE;
  }

  /*  Use an attack mechanic to determine success. */
  if (attack_roll(ch, vict, ATTACK_TYPE_OFFHAND, FALSE, 1) <= 0) {
    damage(ch, vict, 0, SKILL_SHIELD_SLAM, DAM_FORCE, FALSE);
  } else {
    damage(ch, vict, dice(1, 6), SKILL_SHIELD_SLAM, DAM_FORCE, FALSE);
    name = obj_index[GET_OBJ_RNUM(shield)].func;
    if (name)
      (name)(ch, shield, 0, "shieldslam");

    if (savingthrow(vict, SAVING_FORT, 0, (10 + (GET_LEVEL(ch) / 2) + GET_STR_BONUS(ch)))) {
      new_affect(&af);
      af.spell = SKILL_SHIELD_SLAM;
      SET_BIT_AR(af.bitvector, AFF_DAZED);
      af.duration = 1; /* One round */
      affect_join(vict, &af, 1, FALSE, FALSE, FALSE);
      act("$N appears to be dazed by your blow!", FALSE, ch, shield, vict, TO_CHAR);
      act("$N appears to be dazed by the blow!", FALSE, ch, shield, vict, TO_ROOM);
    }
  }

  USE_STANDARD_ACTION(ch);
  USE_MOVE_ACTION(ch);

  return TRUE;
}

/* engine for headbutt skill */
void perform_headbutt(struct char_data *ch, struct char_data *vict) {
  struct affected_type af;

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
    return;
  }

  if (IS_INCORPOREAL(vict)) {
    act("You sprawl completely through $N as you try to attack them!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n sprawls completely through $N as $e tries to attack $M.", FALSE, ch, 0, vict, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    return;
  }

  if (attack_roll(ch, vict, ATTACK_TYPE_UNARMED, FALSE, 1) > 0) {
    damage(ch, vict, dice((HAS_FEAT(ch, FEAT_IMPROVED_UNARMED_STRIKE) ? 2 : 1), 8), SKILL_HEADBUTT, DAM_FORCE, FALSE);

    if (!rand_number(0, 4)) {
      new_affect(&af);
      af.spell = SKILL_HEADBUTT;
      SET_BIT_AR(af.bitvector, AFF_PARALYZED);
      af.duration = 1;
      affect_join(vict, &af, 1, FALSE, FALSE, FALSE);
      act("You slam your head into $N with \tRVICIOUS\tn force!", FALSE, ch, 0, vict, TO_CHAR);
      act("$n slams $s head into $N with \tRVICIOUS\tn force!", FALSE, ch, 0, vict, TO_ROOM);
    }
  } else {
    damage(ch, vict, 0, SKILL_HEADBUTT, DAM_FORCE, FALSE);

  }

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

  USE_STANDARD_ACTION(ch);
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
  if (GET_EQ(ch, WEAR_WIELD_OFFHAND) && IS_BLUNT(GET_EQ(ch, WEAR_WIELD_OFFHAND))) {
    prob += 10;
    wep = GET_EQ(ch, WEAR_WIELD_OFFHAND);
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

  /* Redo this.  Figure out what to do with it.  */
  prob += 30;
  prob += GET_DEX(ch);
  prob -= GET_CON(vict);

  if (!CAN_SEE(vict, ch))
    prob += 50;

  if (percent < prob) {
    dam = 5 + dice(GET_LEVEL(ch), 3);
    damage(ch, vict, dam, SKILL_SAP, DAM_FORCE, FALSE);
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

  USE_STANDARD_ACTION(ch);
  USE_MOVE_ACTION(ch);
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

  base_probability = 75; //flat rate 75% right now

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

  USE_STANDARD_ACTION(ch);

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
  } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_SINGLEFILE) &&
          ch->next_in_room != opponent && opponent->next_in_room != ch) {
    send_to_char(ch, "You simply can't reach that far.\r\n");
  } else {
    send_to_char(ch, "You join the fight!\r\n");
    act("$N assists you!", 0, helpee, 0, ch, TO_CHAR);
    act("$n assists $N.", FALSE, ch, 0, helpee, TO_NOTVICT);
    set_fighting(ch, opponent);

    //hit(ch, opponent, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
  }
}

/* the primary engine for springleap */
void perform_springleap(struct char_data *ch, struct char_data *vict) {
  //struct affected_type af;
  int dam = 0;

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
    return;
  }

  if (attack_roll(ch, vict, ATTACK_TYPE_UNARMED, FALSE, 1) > 0) {
    dam = dice(6, (GET_LEVEL(ch) / 5) + 2);
    damage(ch, vict, dam, SKILL_SPRINGLEAP, DAM_FORCE, FALSE);

    /* ornir decided to disable this, so i changed the skill from full around action
     * to a move action
     *
        new_affect(&af);
        af.spell = SKILL_SPRINGLEAP;
        if (!rand_number(0, 5))
          SET_BIT_AR(af.bitvector, AFF_PARALYZED);
        else
          SET_BIT_AR(af.bitvector, AFF_STUN);
        af.duration = dice(1, 2);
        affect_join(vict, &af, 1, FALSE, FALSE, FALSE);
     */
  } else {
    damage(ch, vict, 0, SKILL_SPRINGLEAP, DAM_FORCE, FALSE);
  }

  GET_POS(ch) = POS_STANDING;
  USE_MOVE_ACTION(ch);
}

/* smite evil (eventually good?) engine */
void perform_smite(struct char_data *ch) {
  struct affected_type af;

  new_affect(&af);

  af.spell = SKILL_SMITE;
  af.duration = 24;

  affect_to_char(ch, &af);

  if (!IS_NPC(ch))
    start_daily_use_cooldown(ch, FEAT_SMITE_EVIL);

  send_to_char(ch, "You prepare to wreak vengeance upon your foe.\r\n");
  //  act("The mighty force of $N's faith blasts $n out of existence!", FALSE, NULL,
  //          NULL, ch, TO_NOTVICT);

}

/* the primary engine for backstab */
bool perform_backstab(struct char_data *ch, struct char_data *vict) {
  int percent = -1, percent2 = -1, prob = -1, successful = 0;

  percent = rand_number(1, 101); /* 101% is a complete failure */
  percent2 = rand_number(1, 101); /* 101% is a complete failure */

  prob = 60; // flat 60% success rate for now

  if (AFF_FLAGGED(ch, AFF_HIDE))
    prob += 4; //minor bonus for being hidden
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    prob += 4; //minor bonus for being sneaky

  if ((!IS_NPC(ch) && GET_RACE(ch) == RACE_TRELUX) || (GET_EQ(ch, WEAR_WIELD_1)
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
    if (GET_RACE(ch) == RACE_TRELUX || (GET_EQ(ch, WEAR_WIELD_OFFHAND) &&
            IS_PIERCE(GET_EQ(ch, WEAR_WIELD_OFFHAND)))) {
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
    USE_FULL_ROUND_ACTION(ch);
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

  if (GET_HIT(ch) < 1) {
    return 0;

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
  if (20 < rand_number(1, 101)) {
    send_to_char(ch, "You stop spinning.\r\n");
    return 0;
  } else
    return 4 * PASSES_PER_SEC;
}

/******* start offensive commands *******/

/* turn undead skill (clerics, paladins, etc) */
ACMD(do_turnundead) {
  struct char_data *vict = NULL;
  int turn_level = 0;
  int turn_difference = 0, turn_result = 0, turn_roll = 0;
  char buf[MAX_STRING_LENGTH] = {'\0'};

  if (!HAS_FEAT(ch, FEAT_TURN_UNDEAD)) {
    send_to_char(ch, "You do not possess divine favor!\r\n");
    return;
  }

  if (CLASS_LEVEL(ch, CLASS_PALADIN) > 2)
    turn_level += CLASS_LEVEL(ch, CLASS_PALADIN) - 2;
  turn_level += CLASS_LEVEL(ch, CLASS_CLERIC);

  if (turn_level <= 0) {
    send_to_char(ch, "You do not possess divine favor!\r\n");
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

  /* add cooldown, increase skill */
  attach_mud_event(new_mud_event(eTURN_UNDEAD, ch, NULL), 120 * PASSES_PER_SEC);

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

  /* Actions */
  USE_STANDARD_ACTION(ch);
}

/* a function to clear rage and do other dirty work associated with that */
void clear_rage(struct char_data *ch) {
  send_to_char(ch, "You calm down from your rage...\r\n");

  if (!IS_NPC(ch) && !HAS_FEAT(ch, FEAT_TIRELESS_RAGE) &&
      !AFF_FLAGGED(ch, AFF_FATIGUED)) {
    struct affected_type fatigued_af;

    send_to_char(ch, "You are left fatigued from the rage!\r\n");
    new_affect(&fatigued_af);
    fatigued_af.spell = SKILL_RAGE_FATIGUE;
    fatigued_af.duration = 10;
    SET_BIT_AR(fatigued_af.bitvector, AFF_FATIGUED);
    affect_join(ch, &fatigued_af, 1, FALSE, FALSE, FALSE);
  }

  /* clearing some rage powers */
  if (char_has_mud_event(ch, eSUPRISE_ACCURACY)) {
    change_event_duration(ch, eSUPRISE_ACCURACY, 0);
  }
  if (char_has_mud_event(ch, ePOWERFUL_BLOW)) {
    change_event_duration(ch, ePOWERFUL_BLOW, 0);
  }
  if (char_has_mud_event(ch, eCOME_AND_GET_ME)) {
    change_event_duration(ch, eCOME_AND_GET_ME, 0);
  }

}

/* rage skill (berserk) primarily for berserkers character class */
ACMD(do_rage) {
  struct affected_type af, aftwo, afthree, affour;
  int bonus = 0, duration = 0, uses_remaining = 0;

  if (AFF_FLAGGED(ch, AFF_FATIGUED)) {
    send_to_char(ch, "You are are too fatigued to rage!\r\n");
    return;
  }

  if (affected_by_spell(ch, SKILL_RAGE)) {
    clear_rage(ch);
    affect_from_char(ch, SKILL_RAGE);

    return;
  }

  if (!IS_ANIMAL(ch)) {
    if (!IS_NPC(ch) && !HAS_FEAT(ch, FEAT_RAGE)) {
      send_to_char(ch, "You don't know how to rage.\r\n");
      return;
    }
  }

  if (!IS_NPC(ch) && ((uses_remaining = daily_uses_remaining(ch, FEAT_RAGE)) == 0)) {
    send_to_char(ch, "You must recover before you can go into a rage.\r\n");
    return;
  }

  /* bonus */
  if (IS_NPC(ch) || IS_MORPHED(ch)) {
    bonus = (GET_LEVEL(ch) / 3) + 3;
  } else {
    bonus = 4;
    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_GREATER_RAGE))
      bonus += 2;
    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_MIGHTY_RAGE))
      bonus += 3;
    if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_INDOMITABLE_RAGE))
      bonus += 3;
  }

  /* duration */
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

  afthree.spell = SKILL_RAGE;
  afthree.duration = duration;
  afthree.location = APPLY_SAVING_WILL;
  afthree.modifier = bonus;
  if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_INDOMITABLE_WILL))
    afthree.modifier += 4;

  //this is a penalty
  affour.spell = SKILL_RAGE;
  affour.duration = duration;
  affour.location = APPLY_AC_NEW;
  affour.modifier = -2;

  affect_to_char(ch, &af);
  affect_to_char(ch, &aftwo);
  affect_to_char(ch, &afthree);
  affect_to_char(ch, &affour);

  if (!IS_NPC(ch))
    start_daily_use_cooldown(ch, FEAT_RAGE);

  if (!IS_NPC(ch) && CLASS_LEVEL(ch, CLASS_BERSERKER) < 1) {
    USE_STANDARD_ACTION(ch);
  }

  if (GET_HIT(ch) < GET_MAX_HIT(ch)) {
    //little boost in current hps
    GET_HIT(ch) += MIN(GET_MAX_HIT(ch) + 170 - GET_HIT(ch),
          CLASS_LEVEL(ch, CLASS_BERSERKER) * bonus / 2);
  }
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
  } else {
    if (!CONFIG_PK_ALLOWED && !IS_NPC(vict) && !IS_NPC(ch))
      check_killer(ch, vict);
    /* already fighting */
    if (!FIGHTING(ch)) {
      if (!IS_NPC(ch) && HAS_FEAT(ch, FEAT_IMPROVED_INITIATIVE))
        chInitiative += 4;
      chInitiative += GET_DEX(ch);
      if (!IS_NPC(vict) && HAS_FEAT(vict, FEAT_IMPROVED_INITIATIVE))
        victInitiative += 4;
      victInitiative += GET_DEX(vict);
      if (chInitiative >= victInitiative || GET_POS(vict) < POS_FIGHTING) {

        /* ch is taking an action so loses the Flat-footed flag */
        if (AFF_FLAGGED(ch, AFF_FLAT_FOOTED))
          REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_FLAT_FOOTED);
        set_fighting(ch, vict);
        hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE); /* ch first */
      } else {

        /* this is just to avoid silly messages in peace rooms -zusuk */
        if (!ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL) && !ROOM_FLAGGED(IN_ROOM(vict), ROOM_PEACEFUL)) {
          send_to_char(vict, "\tYYour superior initiative grants the first strike!\tn\r\n");
          send_to_char(ch,
                  "\tyYour opponents superior \tYinitiative\ty grants the first strike!\tn\r\n");
        }

        /* vict is taking an action so loses the Flat-footed flag */
        if (AFF_FLAGGED(vict, AFF_FLAT_FOOTED))
          REMOVE_BIT_AR(AFF_FLAGS(vict), AFF_FLAT_FOOTED);

        set_fighting(vict, ch);

        hit(vict, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE); // victim is first
        update_pos(ch);
        if (!IS_NPC(vict) && HAS_FEAT(vict, FEAT_IMPROVED_INITIATIVE) &&
                GET_POS(ch) > POS_DEAD) {
          send_to_char(vict, "\tYYour superior initiative grants another attack!\tn\r\n");
          hit(vict, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
        }
      }
      /* not fighting, so switch opponents */
    } else {
      USE_STANDARD_ACTION(ch);
      stop_fighting(ch);
      send_to_char(ch, "You switch opponents!\r\n");
      act("$n switches opponents!", FALSE, ch, 0, vict, TO_ROOM);

      set_fighting(ch, vict);

      hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);

      //everyone gets a free shot at you unless you make a acrobatics check
      //15 is DC
      if (FIGHTING(ch) && FIGHTING(vict)) {
        if (!skill_check(ch, ABILITY_ACROBATICS, 15))
          attacks_of_opportunity(ch, 0);
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

  if (IS_NPC(ch) || (CLASS_LEVEL(ch, CLASS_ROGUE) < 1)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (GET_RACE(ch) == RACE_TRELUX)
    ;
  else if (!GET_EQ(ch, WEAR_WIELD_1) && !GET_EQ(ch, WEAR_WIELD_OFFHAND) && !GET_EQ(ch, WEAR_WIELD_2H)) {
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
  } else if (*arg && !IS_NPC(ch) && !HAS_FEAT(ch, FEAT_SPRING_ATTACK)) {
    send_to_char(ch, "You don't have the option to choose which way to flee, and flee randomly!\r\n");
    perform_flee(ch);
  } else {// there is an argument, check if its valid
    if (!HAS_FEAT(ch, FEAT_SPRING_ATTACK)) {
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
          USE_MOVE_ACTION(ch);
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

    USE_MOVE_ACTION(ch);
    stop_fighting(ch);
    send_to_char(ch, "You disengage from the fight.\r\n");
    act("$n disengages from the fight.", FALSE, ch, 0, 0, TO_ROOM);

  }
}

ACMD(do_taunt) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  int attempt = dice(1, 20), resist = dice(1, 20);

  one_argument(argument, arg);

  if (IS_NPC(ch) || !GET_ABILITY(ch, ABILITY_INTIMIDATE)) {
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
  //  if (char_has_mud_event(ch, eTAUNT)) {
  //    send_to_char(ch, "You must wait longer before you can use this ability again.\r\n");
  //    return;
  //  }

  attempt += compute_ability(ch, ABILITY_INTIMIDATE);
  if (!IS_NPC(vict))
    resist += compute_ability(vict, ABILITY_CONCENTRATION);
  else
    resist += (GET_LEVEL(vict) / 2);

  /* Should last one round, plus one second for every point over the resist */
  if (attempt >= resist) {
    send_to_char(ch, "You taunt your opponent!\r\n");
    act("You are \tRtaunted\tn by $N!", FALSE, vict, 0, ch, TO_CHAR);
    act("$n \tWtaunts\tn $N!", FALSE, ch, 0, vict, TO_NOTVICT);
    attach_mud_event(new_mud_event(eTAUNTED, vict, NULL), 6 * PASSES_PER_SEC + (attempt - resist));
  } else {
    send_to_char(ch, "You fail to taunt your opponent!\r\n");
    act("$N fails to \tRtaunt\tn you...", FALSE, vict, 0, ch, TO_CHAR);
    act("$n fails to \tWtaunt\tn $N!", FALSE, ch, 0, vict, TO_NOTVICT);
  }
  if (!FIGHTING(vict))
    hit(vict, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
  //  attach_mud_event(new_mud_event(eTAUNT, ch, NULL), 8 * PASSES_PER_SEC);

  USE_STANDARD_ACTION(ch);
}

/* do_frightful - Perform an AoE attack that terrifies the victims, causign them to flee.
 * Currently this is limited to dragons, but really it should be doable by any fear-inspiring
 * creature.  Don't tell me the tarrasque isn't scary! :)
 * This ability SHOULD be able to be resisted with a successful save.
 * The paladin ability AURA OF COURAGE should give a +4 bonus to all saves against fear,
 * usually will saves. */
ACMD(do_frightful) {
  struct char_data *vict, *next_vict, *tch;
  int modifier = 0;

  /*  DC for the will save is = the level of the creature generating the effect. */
  int dc = GET_LEVEL(ch);

  /*  Only dragons can do this. */
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

    /* Check to see if the victim is affected by an AURA OF COURAGE */
    if (GROUP(ch) != NULL) {
      while ((tch = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL) {
        if (IN_ROOM(tch) != IN_ROOM(ch))
          continue;
        if (HAS_FEAT(tch, FEAT_AURA_OF_COURAGE)) {
          modifier += 4;
          /* Can only have one morale bonus. */
          break;
        }
      }
    }

    if (aoeOK(ch, vict, -1)) {
      send_to_char(ch, "You roar at %s.\r\n", GET_NAME(vict));
      send_to_char(vict, "A mighty roar from %s is directed at you!\r\n",
              GET_NAME(ch));
      act("$n roars at $N!", FALSE, ch, 0, vict,
              TO_NOTVICT);

      /* Check the save. */
      if (HAS_FEAT(vict, FEAT_AURA_OF_COURAGE))
        send_to_char(vict, "You are unaffected!\r\n");
      else if (savingthrow(vict, SAVING_WILL, modifier, dc)) {
        /* Lucky you, you saved! */
        send_to_char(vict, "You stand your ground!\r\n");
      } else {
        /* Failed save, tough luck. */
        send_to_char(vict, "You PANIC!\r\n");
        perform_flee(vict);
        perform_flee(vict);
      }
    }
  }
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
      if (GET_LEVEL(ch) <= 15)
        damage(ch, vict, dice(GET_LEVEL(ch), 6), SPELL_FIRE_BREATHE, DAM_FIRE,
              FALSE);
      else
        damage(ch, vict, dice(GET_LEVEL(ch), 14), SPELL_FIRE_BREATHE, DAM_FIRE,
              FALSE);
    }
  }
  USE_STANDARD_ACTION(ch);
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
  USE_STANDARD_ACTION(ch);
}

ACMD(do_bash) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

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

  //  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_TRIP)) {
  //    send_to_char(ch, "You have no idea how.\r\n");
  //    return;
  //  }

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

  if (IS_NPC(ch) || !HAS_FEAT(ch, FEAT_LAYHANDS)) {
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
  int uses_remaining = 0;

  //  if (GET_RACE(ch) != RACE_CRYSTAL_DWARF) {
  if (!IS_NPC(ch) && !HAS_FEAT(ch, FEAT_CRYSTAL_FIST)) {
    send_to_char(ch, "How do you plan on doing that?\r\n");
    return;
  }


  if ((uses_remaining = daily_uses_remaining(ch, FEAT_CRYSTAL_FIST)) == 0) {
    send_to_char(ch, "You are too exhausted to use crystal fist.\r\n");
    return;
  }

  send_to_char(ch, "\tCLarge, razor sharp crystals sprout from your hands and arms!\tn\r\n");
  act("\tCYou watch as razor sharp crystals sprout from $n's arms and hands!\tn",
          FALSE, ch, 0, 0, TO_NOTVICT);

  if (!IS_NPC(ch))
    start_daily_use_cooldown(ch, FEAT_CRYSTAL_FIST);
}

ACMD(do_crystalbody) {
  int uses_remaining = 0;
  //struct affected_type af;

  //  if (GET_RACE(ch) != RACE_CRYSTAL_DWARF) {
  if (!IS_NPC(ch) && !HAS_FEAT(ch, FEAT_CRYSTAL_BODY)) {
    send_to_char(ch, "How do you plan on doing that?\r\n");
    return;
  }

  if ((uses_remaining = daily_uses_remaining(ch, FEAT_CRYSTAL_BODY)) == 0) {
    send_to_char(ch, "You are too exhausted to harden your body.\r\n");
    return;
  }

  send_to_char(ch, "\tCYour crystalline body becomes harder!\tn\r\n");
  act("\tCYou watch as $n's crystalline body becomes harder!\tn",
          FALSE, ch, 0, 0, TO_NOTVICT);

  if (!IS_NPC(ch))
    start_daily_use_cooldown(ch, FEAT_CRYSTAL_BODY);

  /* Create the affect that gives the crystal body DR bonus. */

}

ACMD(do_renewedvigor) {

  if (IS_NPC(ch)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (!HAS_FEAT(ch, FEAT_RP_RENEWED_VIGOR)) {
    send_to_char(ch, "You have no idea how to do that!\r\n");
    return;
  }

  if (char_has_mud_event(ch, eRENEWEDVIGOR)) {
    send_to_char(ch, "You must wait longer before you can use this "
            "ability again.\r\n");
    return;
  }

  if (FIGHTING(ch) && GET_POS(ch) < POS_FIGHTING) {
    send_to_char(ch, "You need to be in a better position in combat in order"
            " to use this ability!\r\n");
    return;
  }

  if (!affected_by_spell(ch, SKILL_RAGE)) {
    send_to_char(ch, "You need to be raging to do that!\r\n");
    return;
  }

  send_to_char(ch, "Your body glows \tRred\tn as your wounds heal...\r\n");
  act("$n's body glows \tRred\tn as some wounds heal!", FALSE, ch, 0, NULL, TO_NOTVICT);
  attach_mud_event(new_mud_event(eRENEWEDVIGOR, ch, NULL),
          (2 * SECS_PER_MUD_DAY));
  GET_HIT(ch) += MIN((GET_MAX_HIT(ch) - GET_HIT(ch)),
          (dice(CLASS_LEVEL(ch, CLASS_BERSERKER) / 4 + 3, 8) + 10 + GET_CON_BONUS(ch)));
  update_pos(ch);

  /* Actions */
  USE_SWIFT_ACTION(ch);
}

ACMD(do_wholenessofbody) {

  if (IS_NPC(ch)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (!HAS_FEAT(ch, FEAT_WHOLENESS_OF_BODY)) {
    send_to_char(ch, "You have no idea how to do that!\r\n");
    return;
  }

  if (char_has_mud_event(ch, eWHOLENESSOFBODY)) {
    send_to_char(ch, "You must wait longer before you can use this "
            "ability again.\r\n");
    return;
  }

  if (FIGHTING(ch) && GET_POS(ch) < POS_FIGHTING) {
    send_to_char(ch, "You need to be in a better position in combat in order"
            " to use this ability!\r\n");
    return;
  }

  send_to_char(ch, "Your body glows \tWwhite\tn as your wounds heal...\r\n");
  act("$n's body glows \tWwhite\tn as some wounds heal!", FALSE, ch, 0, NULL, TO_NOTVICT);
  attach_mud_event(new_mud_event(eWHOLENESSOFBODY, ch, NULL),
          (4 * SECS_PER_MUD_DAY));
  GET_HIT(ch) += MIN((GET_MAX_HIT(ch) - GET_HIT(ch)),
          (20 + (CLASS_LEVEL(ch, CLASS_MONK) * 2)));
  update_pos(ch);

  /* Actions */
  USE_STANDARD_ACTION(ch);
}

ACMD(do_emptybody) {
  struct affected_type af;

  if (IS_NPC(ch)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (!HAS_FEAT(ch, FEAT_EMPTY_BODY)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (char_has_mud_event(ch, eEMPTYBODY)) {
    send_to_char(ch, "You must wait longer before you can use this "
            "ability again.\r\n");
    return;
  }

  if (FIGHTING(ch) && GET_POS(ch) < POS_FIGHTING) {
    send_to_char(ch, "You need to be in a better position in combat in order"
            " to use this ability!\r\n");
    return;
  }

  send_to_char(ch, "You focus your Ki energy, drawing energy from the Ethereal plane to transition your body to an 'empty' state.\r\n");
  act("$n briefly closes $s eyes in focus, and you watch as $s body phases partially out of the realm!", FALSE, ch, 0, NULL, TO_NOTVICT);

  attach_mud_event(new_mud_event(eEMPTYBODY, ch, NULL),
          (2 * SECS_PER_MUD_DAY));

  new_affect(&af);

  af.spell = SPELL_DISPLACEMENT;
  af.duration = CLASS_LEVEL(ch, CLASS_MONK) + 2;
  SET_BIT_AR(af.bitvector, AFF_DISPLACE);

  affect_to_char(ch, &af);

  /* Actions */
  USE_MOVE_ACTION(ch);
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
          (18 * SECS_PER_MUD_HOUR));

  /* first attempt to recover lost health */
  if (GET_MAX_HIT(vict) != GET_HIT(vict)) {
    GET_HIT(vict) += MIN((GET_MAX_HIT(vict) - GET_HIT(vict)),
            (10 + compute_ability(ch, ABILITY_HEAL) + GET_WIS_BONUS(ch)));
    update_pos(vict);
  }
  /* TODO: poison */
  /* TODO: disease */


  /* Actions */
  USE_STANDARD_ACTION(ch);
}

ACMD(do_rescue) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

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

  if (IS_NPC(ch) || !HAS_FEAT(ch, FEAT_WHIRLWIND_ATTACK)) {
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
  USE_FULL_ROUND_ACTION(ch);
}

ACMD(do_quiveringpalm) {
  int uses_remaining = 0;

  if (IS_NPC(ch) || !HAS_FEAT(ch, FEAT_QUIVERING_PALM)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (affected_by_spell(ch, SKILL_QUIVERING_PALM)) {
    send_to_char(ch, "You have already focused your ki!\r\n");
    return;
  }

  if ((uses_remaining = daily_uses_remaining(ch, FEAT_QUIVERING_PALM)) == 0) {
    send_to_char(ch, "You must recover before you can focus your ki in this way again.\r\n");
    return;
  }

  if (uses_remaining < 0) {
    send_to_char(ch, "You are not experienced enough.\r\n");
    return;
  }

  perform_quiveringpalm(ch);
}

ACMD(do_stunningfist) {
  int uses_remaining = 0;

  if (IS_NPC(ch) || !HAS_FEAT(ch, FEAT_STUNNING_FIST)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (affected_by_spell(ch, SKILL_STUNNING_FIST)) {
    send_to_char(ch, "You have already focused your ki!\r\n");
    return;
  }

  if ((uses_remaining = daily_uses_remaining(ch, FEAT_STUNNING_FIST)) == 0) {
    send_to_char(ch, "You must recover before you can focus your ki in this way again.\r\n");
    return;
  }

  if (uses_remaining < 0) {
    send_to_char(ch, "You are not experienced enough.\r\n");
    return;
  }

  perform_stunningfist(ch);
}

ACMD(do_supriseaccuracy) {
  if (IS_NPC(ch) || !HAS_FEAT(ch, FEAT_RP_SUPRISE_ACCURACY)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (affected_by_spell(ch, SKILL_SUPRISE_ACCURACY)) {
    send_to_char(ch, "You have already focused your rage into accuracy!\r\n");
    return;
  }

  if (char_has_mud_event(ch, eSUPRISE_ACCURACY)) {
    send_to_char(ch, "You are too exhausted to use suprise accuracy again!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_FATIGUED)) {
    send_to_char(ch, "You are are too fatigued to use suprise accuracy!\r\n");
    return;
  }

  if (!affected_by_spell(ch, SKILL_RAGE)) {
    send_to_char(ch, "You need to be raging to use suprise accuracy!\r\n");
    return;
  }

  perform_supriseaccuracy(ch);
}

ACMD(do_comeandgetme) {
  if (IS_NPC(ch) || !HAS_FEAT(ch, FEAT_RP_COME_AND_GET_ME)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (affected_by_spell(ch, SKILL_COME_AND_GET_ME)) {
    send_to_char(ch, "You have already focused your rage into 'come and get me'!\r\n");
    return;
  }

  if (char_has_mud_event(ch, eCOME_AND_GET_ME)) {
    send_to_char(ch, "You are too exhausted to use 'come and get me' again!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_FATIGUED)) {
    send_to_char(ch, "You are are too fatigued to use 'come and get me'!\r\n");
    return;
  }

  if (!affected_by_spell(ch, SKILL_RAGE)) {
    send_to_char(ch, "You need to be raging to use 'come and get me'!\r\n");
    return;
  }

  perform_comeandgetme(ch);
}

ACMD(do_powerfulblow) {

  if (IS_NPC(ch) || !HAS_FEAT(ch, FEAT_RP_POWERFUL_BLOW)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (affected_by_spell(ch, SKILL_POWERFUL_BLOW)) {
    send_to_char(ch, "You have already focused your rage into a powerful blow!\r\n");
    return;
  }

  if (char_has_mud_event(ch, ePOWERFUL_BLOW)) {
    send_to_char(ch, "You are too exhausted to use powerful blow again!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_FATIGUED)) {
    send_to_char(ch, "You are are too fatigued to use powerful blow!\r\n");
    return;
  }

  if (!affected_by_spell(ch, SKILL_RAGE)) {
    send_to_char(ch, "You need to be raging to use powerful blow!\r\n");
    return;
  }

  perform_powerfulblow(ch);
}

ACMD(do_smite) {
  int uses_remaining = 0;

  if (IS_NPC(ch) || !HAS_FEAT(ch, FEAT_SMITE_EVIL)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if ((uses_remaining = daily_uses_remaining(ch, FEAT_SMITE_EVIL)) == 0) {
    send_to_char(ch, "You must recover the divine energy required to smite evil.\r\n");
    return;
  }

  if (uses_remaining < 0) {
    send_to_char(ch, "You are not experienced enough.\r\n");
    return;
  }

  perform_smite(ch);
}

/* kick engine */
void perform_kick(struct char_data *ch, struct char_data *vict) {
  int discipline_bonus = 0, dc = 0, diceOne = 0, diceTwo = 0;

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
    /* Change this so GHOST_TOUCH boots will kick incorporeal creatures. */
    act("You sprawl completely through $N as you try to attack them!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n sprawls completely through $N as $e tries to attack $M.", FALSE, ch, 0, vict, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    return;
  }

  /* maneuver bonus/penalty */
  if (!IS_NPC(ch) && compute_ability(ch, ABILITY_DISCIPLINE))
    discipline_bonus += compute_ability(ch, ABILITY_DISCIPLINE);
  if (!IS_NPC(vict) && compute_ability(vict, ABILITY_DISCIPLINE))
    discipline_bonus -= compute_ability(vict, ABILITY_DISCIPLINE);

  /* saving throw dc */
  dc = GET_LEVEL(ch) / 2 + GET_STR_BONUS(ch);

  /* monk damage? */
  compute_barehand_dam_dice(ch, &diceOne, &diceTwo);
  if (diceOne < 1)
    diceOne = 1;
  if (diceTwo < 2)
    diceTwo = 2;

  if (combat_maneuver_check(ch, vict, COMBAT_MANEUVER_TYPE_KICK) > 0) {
    damage(ch, vict, dice(diceOne, diceTwo) + GET_STR_BONUS(ch), SKILL_KICK, DAM_FORCE, FALSE);
    if (!savingthrow(vict, SAVING_REFL, GET_STR_BONUS(vict), dc)) {
      USE_MOVE_ACTION(vict);
      act("You are thrown off-balance by a kick from $N!", FALSE, vict, 0, ch, TO_CHAR);
      act("$e is thrown off-blance by your kick at $m!", FALSE, vict, 0, ch, TO_VICT);
      act("$n is thrown off-balance by a kick from $N!", FALSE, vict, 0, ch, TO_NOTVICT);
    }
  } else
    damage(ch, vict, 0, SKILL_KICK, DAM_FORCE, FALSE);
}

ACMD(do_kick) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  struct char_data *vict = NULL;

  if (IS_NPC(ch)) {
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

  /* added this check because of abuse */
  if (IS_NPC(ch) || IS_FAMILIAR(ch) || IS_PET(ch)) {
    return;
  }

  if ((IS_NPC(ch) || !HAS_FEAT(ch, FEAT_WHIRLWIND_ATTACK)) && (!IS_PET(ch) || IS_FAMILIAR(ch))) {
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

      if (rand_number(0, 111) < 20 ||
              (IS_PET(ch) && rand_number(0, 101) > GET_LEVEL(ch)))
        lag++;

      if (aoeOK(ch, vict, SKILL_HITALL))
        hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, 0);
    }
  }
  lag = 1 + lag / 3 + count / 4;
  if (lag > 4)
    lag = 4;

  USE_FULL_ROUND_ACTION(ch);
}

/*
 * Original by Vhaerun, a neat skill for rogues to get an additional attack
 * while not tanking.
 */
ACMD(do_circle) {
  char buf[MAX_INPUT_LENGTH] = {'\0'};
  struct char_data *vict = NULL;

  if (IS_NPC(ch) || (CLASS_LEVEL(ch, CLASS_ROGUE) < 1)) {
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
  else if (!GET_EQ(ch, WEAR_WIELD_1) && !GET_EQ(ch, WEAR_WIELD_OFFHAND) && !GET_EQ(ch, WEAR_WIELD_2H)) {
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

  send_to_char(ch, "Unimplemented.\r\n");
  return;

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
    if (!(HAS_FEAT(ch, FEAT_IMPROVED_UNARMED_STRIKE))) {
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

  if (CLASS_LEVEL(ch, CLASS_ROGUE) < 10) {
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
    send_to_char(ch, "You can't see well enough to guard anywone!.\r\n");
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

  if (!IS_NPC(ch) && !HAS_FEAT(ch, FEAT_DIRT_KICK)) {
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

  if (IS_NPC(ch) || !HAS_FEAT(ch, FEAT_SPRING_ATTACK)) {
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
  } else
    vict = get_char_room_vis(ch, arg, NULL);

  if (!vict) {
    send_to_char(ch, "Springleap who?\r\n");
    return;
  }
  perform_springleap(ch, vict);
}

/* Shieldpunch :
 * Use your shield as a weapon, bashing out with it and doing a
 * small amount of damage.  The feat FEAT_IMPROVED_SHIELD_PUNCH allows
 * you to retain the AC of your shield when you perform a shield punch.
 */
ACMD(do_shieldpunch) {
  struct char_data *vict = NULL;
  char arg[MAX_INPUT_LENGTH] = {'\0'};

  one_argument(argument, arg);


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

/* Shieldcharge :
 *
 * Use your shield as a weapon, bashing out with it and doing a
 * small amount of damage, also attempts to trip the opponent, if
 * possible.
 *
 * Requires FEAT_SHIELD_CHARGE
 *
 * (old comment)
 * Vhaerun:  A warrior skill to Stun !BASH mobs.
 */
ACMD(do_shieldcharge) {
  struct char_data *vict = NULL;
  char arg[MAX_INPUT_LENGTH] = {'\0'};

  one_argument(argument, arg);


  if (GET_POS(ch) <= POS_SITTING) {
    send_to_char(ch, "You need to get on your feet to shieldcharge.\r\n");
    return;
  }

  if (!GET_EQ(ch, WEAR_SHIELD)) {
    send_to_char(ch, "You need to wear a shield to be able to shieldcharge.\r\n");
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

  perform_shieldcharge(ch, vict);
}

/* Shieldslam :
 *
 * Use your shield as a weapon, bashing out with it and doing a
 * small amount of damage, also daze the opponent on a failed fort save..
 *
 * Requires FEAT_SHIELD_SLAM
 *
 * (old comment)
 * Vhaerun:  A warrior skill to Stun !BASH mobs.
 */
ACMD(do_shieldslam) {
  struct char_data *vict = NULL;
  char arg[MAX_INPUT_LENGTH] = {'\0'};

  one_argument(argument, arg);


  if (GET_POS(ch) <= POS_SITTING) {
    send_to_char(ch, "You need to get on your feet to shieldslam.\r\n");
    return;
  }

  if (!GET_EQ(ch, WEAR_SHIELD)) {
    send_to_char(ch, "You need to wear a shield to be able to shieldslam.\r\n");
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

  perform_shieldslam(ch, vict);
}

/* charging system for combat */
ACMD(do_charge) {

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (GET_POS(ch) <= POS_SITTING) {
    send_to_char(ch, "You need to stand to charge!\r\n");
    return;
  }

  perform_charge(ch);
}

/* ranged-weapons combat, archery
 * fire command, fires single arrow - checks can_fire_arrow()
 */
ACMD(do_fire) {
  struct char_data *vict = NULL, *tch = NULL;
  char arg1[MAX_INPUT_LENGTH] = {'\0'};
  char arg2[MAX_INPUT_LENGTH] = {'\0'};
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

    if (!IS_NPC(ch) && !HAS_FEAT(ch, FEAT_FAR_SHOT)) {
      send_to_char(ch, "You need the 'far shot' feat to shoot outside of your"
              " immediate area!\r\n");
      return;
    }

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
    send_to_char(ch, "You need to select a target!\r\n");
    return;
  }

  /* a location has been found. */
  original_loc = IN_ROOM(ch);
  char_from_room(ch);

  if (ZONE_FLAGGED(GET_ROOM_ZONE(room), ZONE_WILDERNESS)) {
    X_LOC(ch) = world[room].coords[0];
    Y_LOC(ch) = world[room].coords[1];
  }

  char_to_room(ch, room);
  vict = get_char_room_vis(ch, arg1, NULL);

  /* check if the char is still there */
  if (IN_ROOM(ch) == room) {
    char_from_room(ch);

    if (ZONE_FLAGGED(GET_ROOM_ZONE(original_loc), ZONE_WILDERNESS)) {
      X_LOC(ch) = world[original_loc].coords[0];
      Y_LOC(ch) = world[original_loc].coords[1];
    }

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
    hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, 2); // 2 in last arg indicates ranged
    if (IN_ROOM(ch) != IN_ROOM(vict))
      stop_fighting(ch);
    else
      FIRING(ch) = TRUE;
    USE_STANDARD_ACTION(ch);
  }
}

/* ranged-weapons combat, archery
 * autofire command, fires single arrow - checks can_fire_arrow()
 * sets FIRING()
 */
ACMD(do_autofire) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
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
    hit(ch, vict, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, 2); // 2 in last arg indicates ranged
    FIRING(ch) = TRUE;
    USE_STANDARD_ACTION(ch);
  }
}

/* function used to gather up all the ammo in the room/corpses-in-room */
ACMD(do_collect) {
  struct obj_data *ammo_pouch = GET_EQ(ch, WEAR_AMMO_POUCH);
  struct obj_data *obj = NULL;
  struct obj_data *nobj = NULL;
  struct obj_data *cobj = NULL;
  struct obj_data *next_obj = NULL;
  int ammo = 0;
  bool fit = TRUE;
  char buf[MAX_INPUT_LENGTH] = {'\0'};

  if (!ammo_pouch) {
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
          if (num_obj_in_obj(ammo_pouch) < GET_OBJ_VAL(ammo_pouch, 0)) {
            obj_from_obj(cobj);
            obj_to_obj(cobj, ammo_pouch);
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
      if (num_obj_in_obj(ammo_pouch) < GET_OBJ_VAL(ammo_pouch, 0)) {
        obj_from_room(obj);
        obj_to_obj(obj, ammo_pouch);
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
int perform_disarm(struct char_data *ch, struct char_data *vict, int mod) {
  int pos, aoo_dam;
  struct obj_data *wielded = NULL;

  if (!vict) {
    send_to_char(ch, "You can only try to disarm the opponent you are fighting.\r\n");
    return -1;
  }

  if (ch == vict) {
    send_to_char(ch, "You can just remove your weapon instead.\r\n");
    return -1;
  }

  if (!CAN_SEE(ch, vict)) {
    send_to_char(ch, "You can't see well enough to attempt that.\r\n");
    return -1;
  }

  // Determine what we are going to disarm. Check for a 2H weapon first.
  if (GET_EQ(vict, WEAR_WIELD_2H)) {
    wielded = GET_EQ(vict, WEAR_WIELD_2H);
    pos = WEAR_WIELD_2H;
  } else {
    // Check for a 1h weapon, primary hand.
    wielded = GET_EQ(vict, WEAR_WIELD_1);
    pos = WEAR_WIELD_1;
  }
  //  If neither of those was successful, check for a 1H weapon in the secondary hand.
  if (!wielded) {
    wielded = GET_EQ(vict, WEAR_WIELD_OFFHAND);
    pos = WEAR_WIELD_OFFHAND;
  }

  // If wielded is NULL, then the victim is weilding no weapon!
  if (!wielded) {
    act("But $N is not wielding anything.", FALSE, ch, 0, vict, TO_CHAR);
    return -1;
  }

  // Trigger AOO, save damage for modifying the CMD roll.
  if (!HAS_FEAT(ch, FEAT_IMPROVED_DISARM))
    aoo_dam = attack_of_opportunity(vict, ch, 0);

  // Check to see what we are wielding.
  if ((GET_EQ(ch, WEAR_WIELD_2H) == NULL) &&
      (GET_EQ(ch, WEAR_WIELD_1) == NULL) &&
      (GET_EQ(ch, WEAR_WIELD_OFFHAND) == NULL) &&
      (!HAS_FEAT(ch, FEAT_IMPROVED_UNARMED_STRIKE))) {
    // Trying an unarmed disarm, -4.
    mod -= 4;
  }

  int result = combat_maneuver_check(ch, vict, COMBAT_MANEUVER_TYPE_DISARM);
  if (result > 0) { /* success! */
    act("$n disarms $N of $S $p.", FALSE, ch, wielded, vict,TO_ROOM );
    act("You manage to knock $p out of $N's hands.", FALSE, ch, wielded, vict,TO_CHAR );
    obj_to_room(unequip_char(vict, pos), vict->in_room);
  } else if (result <= -10) { /* critical failure */
    /* have to check if we have a weapon to lose */
    if (GET_EQ(ch, WEAR_WIELD_2H)) {
      wielded = GET_EQ(ch, WEAR_WIELD_2H);
      pos = WEAR_WIELD_2H;
    } else { /* check 1h weapon, primary hand */
      wielded = GET_EQ(ch, WEAR_WIELD_1);
      pos = WEAR_WIELD_1;
    }
    /* If neither successful, check for a 1H weapon in the secondary hand. */
    if (!wielded) {
      wielded = GET_EQ(ch, WEAR_WIELD_OFFHAND);
      pos = WEAR_WIELD_OFFHAND;
    }
    if (!wielded) { /* not wielding */
      act("$n attempt to disarm $N fails terribly.", FALSE, ch, wielded, vict, TO_ROOM);
      act("You fail terribly in your attempt to disarm $N.", FALSE, ch, wielded, vict, TO_CHAR);
    } else {
      act("$n fails the disarm maneuver on $N, stumbles and drops $s $p.", FALSE, ch, wielded, vict, TO_ROOM);
      act("You drop $p in your attempt to disarm $N!", FALSE, ch, wielded, vict, TO_CHAR);
      obj_to_room(unequip_char(ch, pos), ch->in_room);
    }
  } else { /* failure */
    act("$n fails to disarm $N of $S $p.", FALSE, ch, wielded, vict,TO_ROOM );
    act("You fail to disarm $p out of $N's hands.", FALSE, ch, wielded, vict,TO_CHAR );
  }


  /*
  if (skill_test(ch, SKILL_DISARM, 500, mod + GET_R_DEX(ch) / 10 - GET_R_STR(vict) / 15) && !IS_OBJ_STAT(wielded, ITEM_NODROP)) {
    act("$n disarms $N of $S $p.", FALSE, ch, wielded, vict, TO_ROOM);
    act("You manage to knock $p out of $N's hands.", FALSE, ch, wielded, vict, TO_CHAR);
    obj_to_room(unequip_char(vict, pos), vict->in_room);
  } else {
    act("$n failed to disarm $N.", FALSE, ch, 0, vict, TO_ROOM);
    act("You failed to disarm $N.", FALSE, ch, 0, vict, TO_CHAR);
  }
   * */
  return 0;
}

/* charging system for combat */
ACMD(do_disarm) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  int mod = 0;
  struct char_data *vict = NULL;

  if (IS_NPC(ch)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (GET_POS(ch) <= POS_SITTING) {
    send_to_char(ch, "You need to stand to disarm!\r\n");
    return;
  }
  if (!*arg) {
    send_to_char(ch, "Disarm who?\r\n");
    return;
  }

  one_argument(argument, arg);

  vict = get_char_room_vis(ch, arg, NULL);

  if (!vict) {
    send_to_char(ch, "Disarm who?\r\n");
    return;
  }
  if (vict == ch) {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }

  perform_disarm(ch, vict, mod);
}

/* do_process_attack()
 *
 * This is the Luminari Attack engine - All attack actions go through this function.
 * When an attack is processed, it is sent to the attack queue and a hit command is generated
 * based on the character's combat status and any supplied arguments.
 *
 * This is a KEY part of the Luminari Action system.
 */
ACMD(do_process_attack) {

  bool fail = FALSE;
  struct attack_action_data *attack = NULL;
  const char *fail_msg = "You have no idea how to";

  /* Check if ch can perform the attack... */
  switch (subcmd) {
    case AA_HEADBUTT:
      if (!HAS_FEAT(ch, FEAT_IMPROVED_UNARMED_STRIKE)) {
        fail = TRUE;
      }
      break;
  }

  if (fail == TRUE) {
    send_to_char(ch, "%s %s.\r\n", fail_msg, complete_cmd_info[cmd].command);
    return;
  }

  CREATE(attack, struct attack_action_data, 1);
  attack->command = cmd;
  attack->attack_type = subcmd;
  attack->argument = strdup(argument);

  enqueue_attack(GET_ATTACK_QUEUE(ch), attack);

  if (FIGHTING(ch) || (!FIGHTING(ch) && (strcmp(argument, "") == 0)))
    send_to_char(ch, "Attack queued.\r\n");

  if (!FIGHTING(ch) && (strcmp(argument, "") != 0)) {
    do_hit(ch, argument, cmd, subcmd);
  }
}



