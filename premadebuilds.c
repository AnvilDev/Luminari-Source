/*
 * Premade build code for LuminariMUD by Gicker aka Steve Squires
 */

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "modify.h"
#include "screen.h"
#include "spells.h"
#include "handler.h"
#include "interpreter.h"
#include "class.h"
#include "race.h"
#include "spec_procs.h"  // for compute_ability
#include "mud_event.h"  // for purgemob event
#include "feats.h"
#include "spec_abilities.h"
#include "assign_wpn_armor.h"
#include "wilderness.h"
#include "domains_schools.h"
#include "constants.h"
#include "dg_scripts.h"
#include "templates.h"
#include "oasis.h"
#include "spell_prep.h"
#include "premadebuilds.h"

void give_premade_skill(struct char_data *ch, bool verbose, int skill, int amount)
{
  SET_ABILITY(ch, skill, GET_ABILITY(ch, skill) + amount);
  if (verbose)
    send_to_char(ch, "You have improved your %s skill by %d.\r\n", spell_info[skill].name, amount);
}

void increase_skills(struct char_data *ch, int chclass, bool verbose, int level)
{

  int amount = 1;
  if (level == 1) amount = 4;

  switch (chclass) {
    case CLASS_WARRIOR:
      give_premade_skill(ch, verbose, ABILITY_HEAL, amount);
      give_premade_skill(ch, verbose, ABILITY_LORE, amount);
      give_premade_skill(ch, verbose, ABILITY_INTIMIDATE, amount);
      give_premade_skill(ch, verbose, ABILITY_DISCIPLINE, amount);
      if (GET_REAL_RACE(ch) == RACE_HUMAN)
        give_premade_skill(ch, verbose, ABILITY_TOTAL_DEFENSE, amount);
      break;
    case CLASS_ROGUE:
      give_premade_skill(ch, verbose, ABILITY_HEAL, amount);
      give_premade_skill(ch, verbose, ABILITY_LORE, amount);
      give_premade_skill(ch, verbose, ABILITY_ACROBATICS, amount);
      give_premade_skill(ch, verbose, ABILITY_BLUFF, amount);
      give_premade_skill(ch, verbose, ABILITY_DISABLE_DEVICE, amount);
      give_premade_skill(ch, verbose, ABILITY_PERCEPTION, amount);
      give_premade_skill(ch, verbose, ABILITY_STEALTH, amount);
      give_premade_skill(ch, verbose, ABILITY_SLEIGHT_OF_HAND, amount);
      give_premade_skill(ch, verbose, ABILITY_APPRAISE, amount);
      if (GET_REAL_RACE(ch) == RACE_HUMAN)
        give_premade_skill(ch, verbose, ABILITY_RIDE, amount);
      break;
    case CLASS_MONK:
      give_premade_skill(ch, verbose, ABILITY_HEAL, amount);
      give_premade_skill(ch, verbose, ABILITY_LORE, amount);
      give_premade_skill(ch, verbose, ABILITY_ACROBATICS, amount);
      give_premade_skill(ch, verbose, ABILITY_DISCIPLINE, amount);
      if (GET_REAL_RACE(ch) == RACE_HUMAN)
        give_premade_skill(ch, verbose, ABILITY_SWIM, amount);
      break;
    case CLASS_CLERIC:
      give_premade_skill(ch, verbose, ABILITY_LORE, amount);
      give_premade_skill(ch, verbose, ABILITY_SPELLCRAFT, amount);
      give_premade_skill(ch, verbose, ABILITY_CONCENTRATION, amount);
      if (GET_REAL_RACE(ch) == RACE_HUMAN)
        give_premade_skill(ch, verbose, ABILITY_HEAL, amount);
      break;
    case CLASS_BERSERKER:
      give_premade_skill(ch, verbose, ABILITY_HEAL, amount);
      give_premade_skill(ch, verbose, ABILITY_LORE, amount);
      give_premade_skill(ch, verbose, ABILITY_DISCIPLINE, amount);
      give_premade_skill(ch, verbose, ABILITY_INTIMIDATE, amount);
      give_premade_skill(ch, verbose, ABILITY_SWIM, amount);
      if (GET_REAL_RACE(ch) == RACE_HUMAN)
        give_premade_skill(ch, verbose, ABILITY_CLIMB, amount);
      break;
    case CLASS_WIZARD:
      give_premade_skill(ch, verbose, ABILITY_HEAL, amount);
      give_premade_skill(ch, verbose, ABILITY_LORE, amount);
      give_premade_skill(ch, verbose, ABILITY_DISCIPLINE, amount);
      give_premade_skill(ch, verbose, ABILITY_SPELLCRAFT, amount);
      give_premade_skill(ch, verbose, ABILITY_CONCENTRATION, amount);
      give_premade_skill(ch, verbose, ABILITY_APPRAISE, amount);
      if (CLASS_LEVEL(ch, CLASS_WIZARD) >= 4) //int goes up to 18
        give_premade_skill(ch, verbose, ABILITY_USE_MAGIC_DEVICE, amount);
      if (CLASS_LEVEL(ch, CLASS_WIZARD) >= 12) // int goes up to 20
        give_premade_skill(ch, verbose, ABILITY_SWIM, amount);
      if (CLASS_LEVEL(ch, CLASS_WIZARD) >= 20) // int goes up to 22
        give_premade_skill(ch, verbose, ABILITY_RIDE, amount);
      if (GET_REAL_RACE(ch) == RACE_HUMAN)
        give_premade_skill(ch, verbose, ABILITY_SENSE_MOTIVE, amount);
      break;
    case CLASS_SORCERER:
      give_premade_skill(ch, verbose, ABILITY_HEAL, amount);
      give_premade_skill(ch, verbose, ABILITY_LORE, amount);
      give_premade_skill(ch, verbose, ABILITY_SPELLCRAFT, amount);
      give_premade_skill(ch, verbose, ABILITY_CONCENTRATION, amount);
      if (GET_REAL_RACE(ch) == RACE_HUMAN)
        give_premade_skill(ch, verbose, ABILITY_APPRAISE, amount);
      break;

  }

}

void give_premade_feat(struct char_data *ch, bool verbose, int feat, int subfeat)
{

  if (subfeat > 0) {
    SET_FEAT(ch, feat, 1);
    SET_COMBAT_FEAT(ch, feat_to_cfeat(feat), subfeat);
    if (verbose) {
      if (feat_list[feat].combat_feat == TRUE)
        send_to_char(ch, "You have learned the %s (%s) feat.\r\n", feat_list[feat].name, weapon_family[subfeat]);
      else
        send_to_char(ch, "You have learned the %s feat.\r\n", feat_list[feat].name);
      do_help(ch, feat_list[feat].name, 0, 0);
    }
  } else {
    SET_FEAT(ch, feat, 1);
    if (verbose) {
      send_to_char(ch, "You have learned the %s feat.\r\n", feat_list[feat].name);
      do_help(ch, feat_list[feat].name, 0, 0);
    }
  }
}

void set_premade_stats(struct char_data *ch, int chclass, int level)
{

  switch (chclass) {
    case CLASS_WARRIOR:
        switch (level) {
          case 1:
            GET_REAL_STR(ch) = 16 + race_list[GET_REAL_RACE(ch)].ability_mods[0];
            GET_REAL_CON(ch) = 14 + race_list[GET_REAL_RACE(ch)].ability_mods[1];
            GET_REAL_INT(ch) = 14 + race_list[GET_REAL_RACE(ch)].ability_mods[2];
            GET_REAL_WIS(ch) = 10 + race_list[GET_REAL_RACE(ch)].ability_mods[3];
            GET_REAL_DEX(ch) = 14 + race_list[GET_REAL_RACE(ch)].ability_mods[4];
            GET_REAL_CHA(ch) =  8 + race_list[GET_REAL_RACE(ch)].ability_mods[5];
            if (GET_REAL_RACE(ch) == RACE_HUMAN) GET_REAL_CON(ch) += 2;
          break;
          case 4: case 8: case 12: case 16: case 20:
            GET_REAL_STR(ch)++;
            break;
        }
      break;
    case CLASS_ROGUE:
        switch (level) {
          case 1:
            GET_REAL_STR(ch) = 12 + race_list[GET_REAL_RACE(ch)].ability_mods[0];
            GET_REAL_CON(ch) = 10 + race_list[GET_REAL_RACE(ch)].ability_mods[1];
            GET_REAL_INT(ch) = 12 + race_list[GET_REAL_RACE(ch)].ability_mods[2];
            GET_REAL_WIS(ch) = 10 + race_list[GET_REAL_RACE(ch)].ability_mods[3];
            GET_REAL_DEX(ch) = 18 + race_list[GET_REAL_RACE(ch)].ability_mods[4];
            GET_REAL_CHA(ch) =  8 + race_list[GET_REAL_RACE(ch)].ability_mods[5];
            if (GET_REAL_RACE(ch) == RACE_HUMAN) { GET_REAL_WIS(ch) += 2; GET_REAL_STR(ch) += 2; }
          break;
          case 4: case 8: case 12: case 16: case 20:
            GET_REAL_DEX(ch)++;
            break;
        }
      break;
    case CLASS_MONK:
        switch (level) {
          case 1:
            GET_REAL_STR(ch) = 14 + race_list[GET_REAL_RACE(ch)].ability_mods[0];
            GET_REAL_CON(ch) = 10 + race_list[GET_REAL_RACE(ch)].ability_mods[1];
            GET_REAL_INT(ch) = 10 + race_list[GET_REAL_RACE(ch)].ability_mods[2];
            GET_REAL_WIS(ch) = 16 + race_list[GET_REAL_RACE(ch)].ability_mods[3];
            GET_REAL_DEX(ch) = 14 + race_list[GET_REAL_RACE(ch)].ability_mods[4];
            GET_REAL_CHA(ch) =  8 + race_list[GET_REAL_RACE(ch)].ability_mods[5];
            if (GET_REAL_RACE(ch) == RACE_HUMAN) { GET_REAL_DEX(ch) += 2; }
          break;
          case 4: case 8: case 12: case 16: case 20:
            GET_REAL_WIS(ch)++;
            break;
        }
      break;
    case CLASS_CLERIC:
        switch (level) {
          case 1:
            GET_REAL_STR(ch) = 14 + race_list[GET_REAL_RACE(ch)].ability_mods[0];
            GET_REAL_CON(ch) = 10 + race_list[GET_REAL_RACE(ch)].ability_mods[1];
            GET_REAL_INT(ch) = 12 + race_list[GET_REAL_RACE(ch)].ability_mods[2];
            GET_REAL_WIS(ch) = 16 + race_list[GET_REAL_RACE(ch)].ability_mods[3];
            GET_REAL_DEX(ch) = 14 + race_list[GET_REAL_RACE(ch)].ability_mods[4];
            GET_REAL_CHA(ch) =  8 + race_list[GET_REAL_RACE(ch)].ability_mods[5];
            if (GET_REAL_RACE(ch) == RACE_HUMAN) { GET_REAL_CON(ch) += 2; }
          break;
          case 4: case 8: case 12: 
            GET_REAL_WIS(ch)++;
            break;
          case 16: case 20:
            GET_REAL_STR(ch)++;
            break;
        }
      break;
    case CLASS_BERSERKER:
        switch (level) {
          case 1:
            GET_REAL_STR(ch) = 16 + race_list[GET_REAL_RACE(ch)].ability_mods[0];
            GET_REAL_CON(ch) = 14 + race_list[GET_REAL_RACE(ch)].ability_mods[1];
            GET_REAL_INT(ch) = 13 + race_list[GET_REAL_RACE(ch)].ability_mods[2];
            GET_REAL_WIS(ch) =  8 + race_list[GET_REAL_RACE(ch)].ability_mods[3];
            GET_REAL_DEX(ch) = 13 + race_list[GET_REAL_RACE(ch)].ability_mods[4];
            GET_REAL_CHA(ch) =  8 + race_list[GET_REAL_RACE(ch)].ability_mods[5];
            if (GET_REAL_RACE(ch) == RACE_HUMAN) { GET_REAL_CON(ch) += 2; }
          break;
          case 4: case 8: case 12: case 16: case 20:
            GET_REAL_STR(ch)++;
            break;
        }
      break;
    case CLASS_WIZARD:
        switch (level) {
          case 1:
            GET_REAL_STR(ch) = 10 + race_list[GET_REAL_RACE(ch)].ability_mods[0];
            GET_REAL_CON(ch) = 12 + race_list[GET_REAL_RACE(ch)].ability_mods[1];
            GET_REAL_INT(ch) = 17 + race_list[GET_REAL_RACE(ch)].ability_mods[2];
            GET_REAL_WIS(ch) = 10 + race_list[GET_REAL_RACE(ch)].ability_mods[3];
            GET_REAL_DEX(ch) = 13 + race_list[GET_REAL_RACE(ch)].ability_mods[4];
            GET_REAL_CHA(ch) =  8 + race_list[GET_REAL_RACE(ch)].ability_mods[5];
            if (GET_REAL_RACE(ch) == RACE_HUMAN) { GET_REAL_CON(ch) += 2; GET_REAL_DEX(ch) += 1; GET_REAL_WIS(ch) += 1;}
          break;
          case 4: case 8: case 12: case 16: case 20:
            GET_REAL_STR(ch)++;
            break;
        }
      break;
    case CLASS_SORCERER:
        switch (level) {
          case 1:
            GET_REAL_STR(ch) = 10 + race_list[GET_REAL_RACE(ch)].ability_mods[0];
            GET_REAL_CON(ch) = 12 + race_list[GET_REAL_RACE(ch)].ability_mods[1];
            GET_REAL_INT(ch) = 12 + race_list[GET_REAL_RACE(ch)].ability_mods[2];
            GET_REAL_WIS(ch) =  8 + race_list[GET_REAL_RACE(ch)].ability_mods[3];
            GET_REAL_DEX(ch) = 14 + race_list[GET_REAL_RACE(ch)].ability_mods[4];
            GET_REAL_CHA(ch) = 16 + race_list[GET_REAL_RACE(ch)].ability_mods[5];
            if (GET_REAL_RACE(ch) == RACE_HUMAN) { GET_REAL_CON(ch) += 2; GET_REAL_DEX(ch) += 2;}
          break;
          case 4: case 8: case 12: case 16: case 20:
            GET_REAL_CHA(ch)++;
            break;
        }
      break;
  }

}

void add_premade_sorcerer_spells(struct char_data *ch, int level)
{
  switch (level) {
    case 1:
      known_spells_add(ch, CLASS_SORCERER, SPELL_MAGIC_MISSILE, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_MAGE_ARMOR, FALSE);
      break;
    case 2:
      known_spells_add(ch, CLASS_SORCERER, SPELL_SHIELD, FALSE);
      break;
    case 3:
      known_spells_add(ch, CLASS_SORCERER, SPELL_BURNING_HANDS, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_IDENTIFY, FALSE);
      break;
    case 4:
      known_spells_add(ch, CLASS_SORCERER, SPELL_SCORCHING_RAY, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_MIRROR_IMAGE, FALSE);
      break;
    case 5:
      known_spells_add(ch, CLASS_SORCERER, SPELL_EXPEDITIOUS_RETREAT, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_BLUR, FALSE);
      break;
    case 6:
      known_spells_add(ch, CLASS_SORCERER, SPELL_FIREBALL, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_HASTE, FALSE);
      break;
    case 7:
      known_spells_add(ch, CLASS_SORCERER, SPELL_SHELGARNS_BLADE, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_GRACE, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_CHARISMA, FALSE);
      break;
    case 8:
      known_spells_add(ch, CLASS_SORCERER, SPELL_STONESKIN, FALSE);
      break;
    case 9:
      known_spells_add(ch, CLASS_SORCERER, SPELL_PHANTOM_STEED, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_FIRE_SHIELD, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_GREATER_INVIS, FALSE);
      break;
    case 10:
      known_spells_add(ch, CLASS_SORCERER, SPELL_STRENGTH, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_FIREBRAND, FALSE);
      break;
    case 11:
      known_spells_add(ch, CLASS_SORCERER, SPELL_ENDURANCE, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_HEROISM, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_MINOR_GLOBE, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_CONE_OF_COLD, FALSE);
      break;
    case 12:
      known_spells_add(ch, CLASS_SORCERER, SPELL_GREATER_MIRROR_IMAGE, FALSE);
      break;
    case 13:
      known_spells_add(ch, CLASS_SORCERER, SPELL_ICE_STORM, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_FAITHFUL_HOUND, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_FREEZING_SPHERE, FALSE);
      break;
    case 14:
      known_spells_add(ch, CLASS_SORCERER, SPELL_MISSILE_STORM, FALSE);
      break;
    case 15:
      known_spells_add(ch, CLASS_SORCERER, SPELL_BALL_OF_LIGHTNING, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_TRANSFORMATION, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_DISPLACEMENT, FALSE);
      break;
    case 16:
      known_spells_add(ch, CLASS_SORCERER, SPELL_SUNBURST, FALSE);
      break;
    case 17:
      known_spells_add(ch, CLASS_SORCERER, SPELL_ENCHANT_WEAPON, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_HORRID_WILTING, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_PRISMATIC_SPRAY, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_FEEBLEMIND, FALSE);
      break;
    case 18:
      known_spells_add(ch, CLASS_SORCERER, SPELL_METEOR_SWARM, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_CHAIN_LIGHTNING, FALSE);
      break;
    case 19:
      known_spells_add(ch, CLASS_SORCERER, SPELL_IRONSKIN, FALSE);
      known_spells_add(ch, CLASS_SORCERER, SPELL_GATE, FALSE);

      break;
    case 20:
      known_spells_add(ch, CLASS_SORCERER, SPELL_SUMMON_CREATURE_9, FALSE);
      break;
  }
}

void levelup_warrior(struct char_data *ch, int level, bool verbose)
{
  int chclass = CLASS_WARRIOR;
  switch (level) {
    case 1:
      set_premade_stats(ch, chclass, 1);
      give_premade_feat(ch, verbose, FEAT_WEAPON_FOCUS, WEAPON_FAMILY_HEAVY_BLADE);
      give_premade_feat(ch, verbose, FEAT_LUCK_OF_HEROES, 0);
      if (GET_REAL_RACE(ch) == RACE_HUMAN) {
        give_premade_feat(ch, verbose, FEAT_GREAT_FORTITUDE, 0);
      }
      break;
    case 2:
      give_premade_feat(ch, verbose, FEAT_POWER_ATTACK, 0);
      break;
    case 3:
      give_premade_feat(ch, verbose, FEAT_TOUGHNESS, 0);
      break;
    case 4:
      set_premade_stats(ch, chclass, 4);
      give_premade_feat(ch, verbose, FEAT_WEAPON_SPECIALIZATION, WEAPON_FAMILY_HEAVY_BLADE);
      break;
    case 6:
      give_premade_feat(ch, verbose, FEAT_LIGHTNING_REFLEXES, 0);
      give_premade_feat(ch, verbose, FEAT_DODGE, 0);
      break;
    case 8:
      set_premade_stats(ch, chclass, 8);
      give_premade_feat(ch, verbose, FEAT_IMPROVED_CRITICAL, WEAPON_FAMILY_HEAVY_BLADE);
      break;
    case 9:
      give_premade_feat(ch, verbose, FEAT_POWER_CRITICAL, WEAPON_FAMILY_HEAVY_BLADE);
      break;
    case 10:
      give_premade_feat(ch, verbose, FEAT_GREATER_WEAPON_FOCUS, WEAPON_FAMILY_HEAVY_BLADE);
      break;
    case 12:
      set_premade_stats(ch, chclass, 12);
      give_premade_feat(ch, verbose, FEAT_FAST_HEALER, 0);
      give_premade_feat(ch, verbose, FEAT_ARMOR_SPECIALIZATION_HEAVY, 0);
      break;
    case 14:
      give_premade_feat(ch, verbose, FEAT_GREATER_WEAPON_SPECIALIZATION, WEAPON_FAMILY_HEAVY_BLADE);
      break;
    case 15:
      give_premade_feat(ch, verbose, FEAT_MOBILITY, 0);
      break;
    case 16:
      set_premade_stats(ch, chclass, 16);
      give_premade_feat(ch, verbose, FEAT_SPRING_ATTACK, 0);
      break;
    case 18:
      give_premade_feat(ch, verbose, FEAT_IRON_WILL, 0);
      give_premade_feat(ch, verbose, FEAT_COMBAT_EXPERTISE, 0);
      break;
    case 20:
      set_premade_stats(ch, chclass, 20);
      give_premade_feat(ch, verbose, FEAT_WHIRLWIND_ATTACK, 0);
      break;
  }
  increase_skills(ch, chclass, TRUE, level);
}

void levelup_rogue(struct char_data *ch, int level, bool verbose)
{
  int chclass = CLASS_ROGUE;
  switch (level) {
    case 1:
      set_premade_stats(ch, chclass, 1);
      give_premade_feat(ch, verbose, FEAT_TWO_WEAPON_FIGHTING, 0);
      if (GET_REAL_RACE(ch) == RACE_HUMAN) {
        give_premade_feat(ch, verbose, FEAT_DODGE, 0);
      }
      break;
    case 3:
      give_premade_feat(ch, verbose, FEAT_WEAPON_FOCUS, WEAPON_FAMILY_LIGHT_BLADE);
      break;
    case 4:
      set_premade_stats(ch, chclass, 4);
      break;
    case 6:
      give_premade_feat(ch, verbose, FEAT_IMPROVED_TWO_WEAPON_FIGHTING, 0);
      break;
    case 8:
      set_premade_stats(ch, chclass, 8);
      break;
    case 9:
      give_premade_feat(ch, verbose, FEAT_LUCK_OF_HEROES, 0);
      break;
    case 12:
      set_premade_stats(ch, chclass, 12);
      give_premade_feat(ch, verbose, FEAT_GREATER_TWO_WEAPON_FIGHTING, 0);
      break;
    case 15:
      give_premade_feat(ch, verbose, FEAT_MOBILITY, 0);
      break;
    case 16:
      set_premade_stats(ch, chclass, 16);
      break;
    case 18:
      give_premade_feat(ch, verbose, FEAT_SPRING_ATTACK, 0);
      break;
    case 20:
      set_premade_stats(ch, chclass, 20);
      break;
  }
  increase_skills(ch, chclass, TRUE, level);
}

void levelup_monk(struct char_data *ch, int level, bool verbose)
{
  int chclass = CLASS_MONK;
  switch (level) {
    case 1:
      set_premade_stats(ch, chclass, 1);
      give_premade_feat(ch, verbose, FEAT_LUCK_OF_HEROES, 0);
      if (GET_REAL_RACE(ch) == RACE_HUMAN) {
        give_premade_feat(ch, verbose, FEAT_TOUGHNESS, 0);
      }
      break;
    case 3:
      give_premade_feat(ch, verbose, FEAT_WEAPON_FOCUS, WEAPON_FAMILY_MONK);
      break;
    case 4:
      set_premade_stats(ch, chclass, 4);
      break;
    case 6:
      give_premade_feat(ch, verbose, FEAT_POWER_CRITICAL, WEAPON_FAMILY_MONK);
      break;
    case 8:
      set_premade_stats(ch, chclass, 8);
      break;
    case 9:
      give_premade_feat(ch, verbose, FEAT_POWER_ATTACK, 0);
      break;
    case 12:
      set_premade_stats(ch, chclass, 12);
      give_premade_feat(ch, verbose, FEAT_IMPROVED_CRITICAL, WEAPON_FAMILY_MONK);
      break;
    case 15:
      give_premade_feat(ch, verbose, FEAT_FAST_HEALER, 0);
      break;
    case 16:
      set_premade_stats(ch, chclass, 16);
      break;
    case 18:
      give_premade_feat(ch, verbose, FEAT_DODGE, 0);
      break;
    case 20:
      set_premade_stats(ch, chclass, 20);
      break;
  }
  increase_skills(ch, chclass, TRUE, level);
}

void levelup_cleric(struct char_data *ch, int level, bool verbose)
{
  int chclass = CLASS_CLERIC;
  switch (level) {
    case 1:
      set_premade_stats(ch, chclass, 1);
      give_premade_feat(ch, verbose, FEAT_LUCK_OF_HEROES, 0);
      if (GET_REAL_RACE(ch) == RACE_HUMAN) {
        give_premade_feat(ch, verbose, FEAT_TOUGHNESS, 0);
      }
      GET_1ST_DOMAIN(ch) = DOMAIN_WAR;
      GET_2ND_DOMAIN(ch) = DOMAIN_HEALING;
      /* set spells learned for domain */
      assign_domain_spells(ch);
      /* in case adding or changing clear domains, clean up and re-assign */
      clear_domain_feats(ch);
      add_domain_feats(ch);
      break;
    case 3:
      give_premade_feat(ch, verbose, FEAT_QUICK_CHANT, 0);
      break;
    case 4:
      set_premade_stats(ch, chclass, 4);
      break;
    case 6:
      give_premade_feat(ch, verbose, FEAT_FASTER_MEMORIZATION, 0);
      break;
    case 8:
      set_premade_stats(ch, chclass, 8);
      break;
    case 9:
      give_premade_feat(ch, verbose, FEAT_WEAPON_FOCUS, WEAPON_FAMILY_HAMMER);
      break;
    case 12:
      set_premade_stats(ch, chclass, 12);
      give_premade_feat(ch, verbose, FEAT_IMPROVED_CRITICAL, WEAPON_FAMILY_HAMMER);
      break;
    case 15:
      give_premade_feat(ch, verbose, FEAT_POWER_CRITICAL, WEAPON_FAMILY_HAMMER);
      break;
    case 16:
      set_premade_stats(ch, chclass, 16);
      break;
    case 18:
      give_premade_feat(ch, verbose, FEAT_POWER_ATTACK, 0);
      break;
    case 20:
      set_premade_stats(ch, chclass, 20);
      break;
  }
  increase_skills(ch, chclass, TRUE, level);
}

void levelup_berserker(struct char_data *ch, int level, bool verbose)
{
  int chclass = CLASS_BERSERKER;
  switch (level) {
    case 1:
      set_premade_stats(ch, chclass, 1);
      give_premade_feat(ch, verbose, FEAT_POWER_ATTACK, 0);
      if (GET_REAL_RACE(ch) == RACE_HUMAN) {
        give_premade_feat(ch, verbose, FEAT_LUCK_OF_HEROES, 0);
      }
      break;
    case 3:
      give_premade_feat(ch, verbose, FEAT_DODGE, 0);
      break;
    case 4:
      set_premade_stats(ch, chclass, 4);
      break;
    case 6:
      give_premade_feat(ch, verbose, FEAT_MOBILITY, 0);
      break;
    case 8:
      set_premade_stats(ch, chclass, 8);
      break;
    case 9:
      give_premade_feat(ch, verbose, FEAT_IMPROVED_CRITICAL, WEAPON_FAMILY_AXE);
      break;
    case 12:
      set_premade_stats(ch, chclass, 12);
      give_premade_feat(ch, verbose, FEAT_SPRING_ATTACK, 0);
      break;
    case 15:
      give_premade_feat(ch, verbose, FEAT_COMBAT_EXPERTISE, 0);
      break;
    case 16:
      set_premade_stats(ch, chclass, 16);
      break;
    case 18:
      give_premade_feat(ch, verbose, FEAT_WHIRLWIND_ATTACK, 0);
      break;
    case 20:
      set_premade_stats(ch, chclass, 20);
      break;
  }
  increase_skills(ch, chclass, TRUE, level);
}

void levelup_wizard(struct char_data *ch, int level, bool verbose)
{
  int chclass = CLASS_WIZARD;
  switch (level) {
    case 1:
      set_premade_stats(ch, chclass, 1);
      give_premade_feat(ch, verbose, FEAT_FASTER_MEMORIZATION, 0);
      if (GET_REAL_RACE(ch) == RACE_HUMAN) {
        give_premade_feat(ch, verbose, FEAT_LUCK_OF_HEROES, 0);
      }
      GET_SPECIALTY_SCHOOL(ch) = EVOCATION;
      GET_FAMILIAR(ch) = 85; // imp
      break;
    case 3:
      give_premade_feat(ch, verbose, FEAT_COMBAT_CASTING, 0);
      break;
    case 4:
      set_premade_stats(ch, chclass, 4);
      break;
    case 5:
      give_premade_feat(ch, verbose, FEAT_ENHANCED_SPELL_DAMAGE, 0);
      break;
    case 6:
      give_premade_feat(ch, verbose, FEAT_SPELL_PENETRATION, 0);
      break;
    case 8:
      set_premade_stats(ch, chclass, 8);
      break;
    case 9:
      give_premade_feat(ch, verbose, FEAT_SPELL_FOCUS, EVOCATION);
      break;
    case 10:
      give_premade_feat(ch, verbose, FEAT_MAXIMIZE_SPELL, 0);
      break;
    case 12:
      set_premade_stats(ch, chclass, 12);
      give_premade_feat(ch, verbose, FEAT_GREATER_SPELL_PENETRATION, 0);
      break;
    case 15:
      give_premade_feat(ch, verbose, FEAT_GREATER_SPELL_FOCUS, EVOCATION);
      break;
    case 16:
      set_premade_stats(ch, chclass, 16);
      break;
    case 18:
      give_premade_feat(ch, verbose, FEAT_TOUGHNESS, 0);
      break;
    case 20:
      set_premade_stats(ch, chclass, 20);
      give_premade_feat(ch, verbose, FEAT_IMPROVED_FAMILIAR, 0);
      break;
  }
  increase_skills(ch, chclass, TRUE, level);
}

void levelup_sorcerer(struct char_data *ch, int level, bool verbose)
{
  int chclass = CLASS_SORCERER;
  switch (level) {
    case 1:
      set_premade_stats(ch, chclass, 1);
      give_premade_feat(ch, verbose, FEAT_FASTER_MEMORIZATION, 0);
      if (GET_REAL_RACE(ch) == RACE_HUMAN) {
        give_premade_feat(ch, verbose, FEAT_LUCK_OF_HEROES, 0);
      }
      GET_FAMILIAR(ch) = 87; // faerie dragon
      SET_FEAT(ch, FEAT_SORCERER_BLOODLINE_DRACONIC, 1);
      SET_FEAT(ch, FEAT_DRACONIC_HERITAGE_CLAWS, 1);
      SET_FEAT(ch, FEAT_DRACONIC_BLOODLINE_ARCANA, 1);
      if (IS_EVIL(ch))
        ch->player_specials->saved.sorcerer_bloodline_subtype = DRACONIC_HERITAGE_RED; // red dragon
      else
        ch->player_specials->saved.sorcerer_bloodline_subtype = DRACONIC_HERITAGE_GOLD; // gold dragon
      break;
    case 3:
      give_premade_feat(ch, verbose, FEAT_COMBAT_CASTING, 0);
      break;
    case 4:
      set_premade_stats(ch, chclass, 4);
      break;
    case 5:
      give_premade_feat(ch, verbose, FEAT_ENHANCED_SPELL_DAMAGE, 0);
      break;
    case 6:
      give_premade_feat(ch, verbose, FEAT_SPELL_PENETRATION, 0);
      break;
    case 8:
      set_premade_stats(ch, chclass, 8);
      break;
    case 9:
      give_premade_feat(ch, verbose, FEAT_SPELL_FOCUS, EVOCATION);
      break;
    case 10:
      give_premade_feat(ch, verbose, FEAT_MAXIMIZE_SPELL, 0);
      break;
    case 12:
      set_premade_stats(ch, chclass, 12);
      give_premade_feat(ch, verbose, FEAT_GREATER_SPELL_PENETRATION, 0);
      break;
    case 15:
      give_premade_feat(ch, verbose, FEAT_GREATER_SPELL_FOCUS, EVOCATION);
      break;
    case 16:
      set_premade_stats(ch, chclass, 16);
      break;
    case 18:
      give_premade_feat(ch, verbose, FEAT_TOUGHNESS, 0);
      break;
    case 20:
      set_premade_stats(ch, chclass, 20);
      give_premade_feat(ch, verbose, FEAT_IMPROVED_FAMILIAR, 0);
      break;
  }
  increase_skills(ch, chclass, TRUE, level);
  add_premade_sorcerer_spells(ch, level);
}

void setup_premade_levelup(struct char_data *ch, int chclass)
{
  GET_FEAT_POINTS(ch) = 0;
  GET_CLASS_FEATS(ch, chclass) = 0;
  GET_EPIC_FEAT_POINTS(ch) = 0;
  GET_EPIC_CLASS_FEATS(ch, chclass) = 0;
  GET_PRACTICES(ch) = 0;
  GET_TRAINS(ch) = 0;
  GET_BOOSTS(ch) = 0;
}

void advance_premade_build(struct char_data *ch)
{
  int chclass = GET_PREMADE_BUILD_CLASS(ch),
      level   = CLASS_LEVEL(ch, chclass);

  setup_premade_levelup(ch, chclass);

  switch (chclass) {
    case CLASS_WARRIOR:
      levelup_warrior(ch, level, TRUE);
      break;
    case CLASS_ROGUE:
      levelup_rogue(ch, level, TRUE);
      break;
    case CLASS_MONK:
      levelup_monk(ch, level, TRUE);
      break;
    case CLASS_CLERIC:
      levelup_cleric(ch, level, TRUE);
      break;
    case CLASS_BERSERKER:
      levelup_berserker(ch, level, TRUE);
      break;
    case CLASS_WIZARD:
      levelup_wizard(ch, level, TRUE);
      break;
    case CLASS_SORCERER:
      levelup_sorcerer(ch, level, TRUE);
      break;
  }
}
