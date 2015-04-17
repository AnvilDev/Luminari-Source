 /*****************************************************************************
 ** assign_wpn_armor.c
  *
  * Assigning weapon and armor values for respective types                   **
 *****************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "db.h"
#include "assign_wpn_armor.h"
#include "craft.h"
#include "feats.h"

/* global */
struct armor_table armor_list[NUM_SPEC_ARMOR_TYPES];
struct weapon_table weapon_list[NUM_WEAPON_TYPES];
const char *weapon_type[NUM_WEAPON_TYPES];

/* simply checks if ch has proficiency with given weapon_type */
int is_proficient_with_weapon(struct char_data *ch, int weapon) {

  if (has_feat(ch, FEAT_SIMPLE_WEAPON_PROFICIENCY) &&
          IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_SIMPLE))
    return TRUE;

  if (has_feat(ch, FEAT_MARTIAL_WEAPON_PROFICIENCY) &&
          IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_MARTIAL))
    return TRUE;

  if (has_feat(ch, FEAT_EXOTIC_WEAPON_PROFICIENCY) &&
          IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_EXOTIC))
    return TRUE;

  if (CLASS_LEVEL(ch, CLASS_MONK) &&
          weapon_list[weapon].weaponFamily == WEAPON_FAMILY_MONK)
    return TRUE;

  if (has_feat(ch, FEAT_WEAPON_PROFICIENCY_DRUID) ||
      CLASS_LEVEL(ch, CLASS_DRUID) > 0) {
    switch (weapon) {
      case WEAPON_TYPE_CLUB:
      case WEAPON_TYPE_DAGGER:
      case WEAPON_TYPE_QUARTERSTAFF:
      case WEAPON_TYPE_DART:
      case WEAPON_TYPE_SICKLE:
      case WEAPON_TYPE_SCIMITAR:
      case WEAPON_TYPE_SHORTSPEAR:
      case WEAPON_TYPE_SPEAR:
      case WEAPON_TYPE_SLING:
        return TRUE;
    }
  }

  if (CLASS_LEVEL(ch, CLASS_BARD) > 0) {
    switch (weapon) {
      case WEAPON_TYPE_LONG_SWORD:
      case WEAPON_TYPE_RAPIER:
      case WEAPON_TYPE_SAP:
      case WEAPON_TYPE_SHORT_SWORD:
      case WEAPON_TYPE_SHORT_BOW:
      case WEAPON_TYPE_WHIP:
        return TRUE;
    }
  }

  if (has_feat(ch, FEAT_WEAPON_PROFICIENCY_ROGUE) ||
      CLASS_LEVEL(ch, CLASS_ROGUE) > 0) {
    switch (weapon) {
      case WEAPON_TYPE_HAND_CROSSBOW:
      case WEAPON_TYPE_RAPIER:
      case WEAPON_TYPE_SAP:
      case WEAPON_TYPE_SHORT_SWORD:
      case WEAPON_TYPE_SHORT_BOW:
        return TRUE;
    }
  }

  if (has_feat(ch, FEAT_WEAPON_PROFICIENCY_WIZARD) ||
      CLASS_LEVEL(ch, CLASS_WIZARD) > 0) {
    switch (weapon) {
      case WEAPON_TYPE_DAGGER:
      case WEAPON_TYPE_QUARTERSTAFF:
      case WEAPON_TYPE_CLUB:
      case WEAPON_TYPE_HEAVY_CROSSBOW:
      case WEAPON_TYPE_LIGHT_CROSSBOW:
        return TRUE;
    }
  }

  if (has_feat(ch, FEAT_WEAPON_PROFICIENCY_ELF) ||
      IS_ELF(ch)) {
    switch (weapon) {
      case WEAPON_TYPE_LONG_SWORD:
      case WEAPON_TYPE_RAPIER:
      case WEAPON_TYPE_LONG_BOW:
      case WEAPON_TYPE_COMPOSITE_LONGBOW:
      case WEAPON_TYPE_SHORT_BOW:
      case WEAPON_TYPE_COMPOSITE_SHORTBOW:
        return TRUE;
    }
  }

  if (IS_DWARF(ch) &&
      has_feat(ch, FEAT_MARTIAL_WEAPON_PROFICIENCY)) {
    switch (weapon) {
      case WEAPON_TYPE_DWARVEN_WAR_AXE:
      case WEAPON_TYPE_DWARVEN_URGOSH:
        return TRUE;
    }
  }

  /* TODO: Adapt this - Focus on an aspect of the divine, not a deity. */
  /*  if (has_feat((char_data *) ch, FEAT_DEITY_WEAPON_PROFICIENCY) && weapon == deity_list[GET_DEITY(ch)].favored_weapon)
      return TRUE;
   */

  /*  //deprecated
  if (HAS_COMBAT_FEAT(ch, CFEAT_EXOTIC_WEAPON_PROFICIENCY, DAMAGE_TYPE_SLASHING) &&
          IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_EXOTIC) &&
          IS_SET(weapon_list[weapon].damageTypes, DAMAGE_TYPE_SLASHING)) {
    return TRUE;
  }
  if (HAS_COMBAT_FEAT(ch, CFEAT_EXOTIC_WEAPON_PROFICIENCY, DAMAGE_TYPE_PIERCING) &&
          IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_EXOTIC) &&
          IS_SET(weapon_list[weapon].damageTypes, DAMAGE_TYPE_PIERCING)) {
    return TRUE;
  }
  if (HAS_COMBAT_FEAT(ch, CFEAT_EXOTIC_WEAPON_PROFICIENCY, DAMAGE_TYPE_BLUDGEONING) &&
          IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_EXOTIC) &&
          IS_SET(weapon_list[weapon].damageTypes, DAMAGE_TYPE_BLUDGEONING)) {
    return TRUE;
  }
  */

  /* nope not proficient with given weapon! */
  return FALSE;
}

/* more weapon flags we have to deal with */

//#define WEAPON_FLAG_SIMPLE      (1 << 0)
//#define WEAPON_FLAG_MARTIAL     (1 << 1)
//#define WEAPON_FLAG_EXOTIC      (1 << 2)
//#define WEAPON_FLAG_RANGED      (1 << 3)
//#define WEAPON_FLAG_THROWN      (1 << 4)

/* Reach: You use a reach weapon to strike opponents 10 feet away, but you can't
 * use it against an adjacent foe. */
//#define WEAPON_FLAG_REACH       (1 << 5)

//#define WEAPON_FLAG_ENTANGLE    (1 << 6)

/* Trip*: You can use a trip weapon to make trip attacks. If you are tripped
 * during your own trip attempt, you can drop the weapon to avoid being tripped
 * (*see FAQ/Errata.) */
//#define WEAPON_FLAG_TRIP        (1 << 7)

/* Disarm: When you use a disarm weapon, you get a +2 bonus on Combat Maneuver
 * Checks to disarm an enemy. */
//#define WEAPON_FLAG_DISARM      (1 << 9)

/* Nonlethal: These weapons deal nonlethal damage (see Combat). */
//#define WEAPON_FLAG_NONLETHAL   (1 << 10)

//#define WEAPON_FLAG_SLOW_RELOAD (1 << 11)
//#define WEAPON_FLAG_BALANCED    (1 << 12)
//#define WEAPON_FLAG_CHARGE      (1 << 13)
//#define WEAPON_FLAG_REPEATING   (1 << 14)
//#define WEAPON_FLAG_TWO_HANDED  (1 << 15)

//#define WEAPON_FLAG_LIGHT       (1 << 16)
bool is_using_light_weapon(struct char_data *ch, struct obj_data *wielded) {

  if (!wielded)
    return FALSE;
  if (GET_OBJ_SIZE(wielded) > GET_SIZE(ch))
    return FALSE;

  if (IS_SET(weapon_list[GET_OBJ_VAL(wielded, 0)].weaponFlags, WEAPON_FLAG_LIGHT))
    return TRUE;

  return FALSE;
}

/* Blocking: When you use this weapon to fight defensively, you gain a +1 shield
 * bonus to AC. Source: Ultimate Combat. */
//#define WEAPON_FLAG_BLOCKING    (1 << 17)

/* Brace: If you use a readied action to set a brace weapon against a charge,
 * you deal double damage on a successful hit against a charging creature
 * (see Combat). */
//#define WEAPON_FLAG_BRACING     (1 << 18)

/* Deadly: When you use this weapon to deliver a coup de grace, it gains a +4
 * bonus to damage when calculating the DC of the Fortitude saving throw to see
 * whether the target of the coup de grace dies from the attack. The bonus is
 * not added to the actual damage of the coup de grace attack.
 * Source: Ultimate Combat. */
//#define WEAPON_FLAG_DEADLY      (1 << 19)

/* Distracting: You gain a +2 bonus on Bluff skill checks to feint in combat
 * while wielding this weapon. Source: Ultimate Combat. */
//#define WEAPON_FLAG_DISTRACTING (1 << 20)

/* Fragile: Weapons and armor with the fragile quality cannot take the beating
 * that sturdier weapons can. A fragile weapon gains the broken condition if the
 * wielder rolls a natural 1 on an attack roll with the weapon. If a fragile
 * weapon is already broken, the roll of a natural 1 destroys it instead.
 * Masterwork and magical fragile weapons and armor lack these flaws unless
 * otherwise noted in the item description or the special material description.
 * If a weapon gains the broken condition in this way, that weapon is considered
 * to have taken damage equal to half its hit points +1. This damage is repaired
 * either by something that addresses the effect that granted the weapon the
 * broken condition (like quick clear in the case of firearm misfires or the
 * Field Repair feat) or by the repair methods described in the broken condition.
 * When an effect that grants the broken condition is removed, the weapon
 * regains the hit points it lost when the broken condition was applied. Damage
 * done by an attack against a weapon (such as from a sunder combat maneuver)
 * cannot be repaired by an effect that removes the broken condition.
 * Source: Ultimate Combat.*/
//#define WEAPON_FLAG_FRAGILE     (1 << 21)

/* Grapple: On a successful critical hit with a weapon of this type, you can
 * grapple the target of the attack. The wielder can then attempt a combat
 * maneuver check to grapple his opponent as a free action. This grapple attempt
 * does not provoke an attack of opportunity from the creature you are
 * attempting to grapple if that creature is not threatening you. While you
 * grapple the target with a grappling weapon, you can only move or damage the
 * creature on your turn. You are still considered grappled, though you do not
 * have to be adjacent to the creature to continue the grapple. If you move far
 * enough away to be out of the weapon’s reach, you end the grapple with that
 * action. Source: Ultimate Combat. */
//#define WEAPON_FLAG_GRAPPLING   (1 << 22)

/* Performance: When wielding this weapon, if an attack or combat maneuver made
 * with this weapon prompts a combat performance check, you gain a +2 bonus on
 * that check. See Gladiator Weapons below for more information. */
//#define WEAPON_FLAG_PERFORMANCE (1 << 23)

/* ***Strength (#): This feature is usually only applied to ranged weapons (such
 * as composite bows). Some weapons function better in the hands of stronger
 * users. All such weapons are made with a particular Strength rating (that is,
 * each requires a minimum Strength modifier to use with proficiency and this
 * number is included in parenthesis). If your Strength bonus is less than the
 * strength rating of the weapon, you can't effectively use it, so you take a –2
 * penalty on attacks with it. For example, the default (lowest form of)
 * composite longbow requires a Strength modifier of +0 or higher to use with
 * proficiency. A weapon with the Strength feature allows you to add your
 * Strength bonus to damage, up to the maximum bonus indicated for the bow. Each
 * point of Strength bonus granted by the bow adds 100 gp to its cost. If you
 * have a penalty for low Strength, apply it to damage rolls when you use a
 * composite longbow. Editor's Note: The "Strength" weapon feature was 'created'
 * by d20pfsrd.com as a shorthand note to the composite bow mechanics. This is
 * not "Paizo" or "official" content. */
//#define WEAPON_FLAG_STRENGTH    (1 << 24)

/* Sunder: When you use a sunder weapon, you get a +2 bonus on Combat Maneuver
 * Checks to sunder attempts. */
//#define WEAPON_FLAG_SUNDER      (1 << 25)

/* Double: You can use a double weapon to fight as if fighting with two weapons,
 * but if you do, you incur all the normal attack penalties associated with
 * fighting with two weapons, just as if you were using a one-handed weapon and
 * a light weapon. You can choose to wield one end of a double weapon two-handed,
 * but it cannot be used as a double weapon when wielded in this way—only one
 * end of the weapon can be used in any given round. */
//#define WEAPON_FLAG_DOUBLE      (1 << 8)
/* we are going to say it is not enough that the weapon just be flagged
   double, we also need the weapon to be a size-class larger than the player */
bool is_using_double_weapon(struct char_data *ch) {
  struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD_2H);

  if (!wielded)
    return FALSE;
  if (GET_OBJ_SIZE(wielded) <= GET_SIZE(ch))
    return FALSE;

  if (IS_SET(weapon_list[GET_OBJ_VAL(wielded, 0)].weaponFlags, WEAPON_FLAG_DOUBLE))
    return TRUE;

  return FALSE;
}

/* end utility, start base set/load/init functions for weapons/armor */

void setweapon(int type, char *name, int numDice, int diceSize, int critRange, int critMult,
        int weaponFlags, int cost, int damageTypes, int weight, int range, int weaponFamily, int size,
        int material, int handle_type, int head_type) {
  weapon_type[type] = strdup(name);
  weapon_list[type].name = name;
  weapon_list[type].numDice = numDice;
  weapon_list[type].diceSize = diceSize;
  weapon_list[type].critRange = critRange;
  if (critMult == 2)
    weapon_list[type].critMult = CRIT_X2;
  else if (critMult == 3)
    weapon_list[type].critMult = CRIT_X3;
  else if (critMult == 4)
    weapon_list[type].critMult = CRIT_X4;
  else if (critMult == 5)
    weapon_list[type].critMult = CRIT_X5;
  else if (critMult == 6)
    weapon_list[type].critMult = CRIT_X6;
  weapon_list[type].weaponFlags = weaponFlags;
  weapon_list[type].cost = cost;
  weapon_list[type].damageTypes = damageTypes;
  weapon_list[type].weight = weight;
  weapon_list[type].range = range;
  weapon_list[type].weaponFamily = weaponFamily;
  weapon_list[type].size = size;
  weapon_list[type].material = material;
  weapon_list[type].handle_type = handle_type;
  weapon_list[type].head_type = head_type;
}

void initialize_weapons(int type) {
  weapon_list[type].name = "unused weapon";
  weapon_list[type].numDice = 1;
  weapon_list[type].diceSize = 1;
  weapon_list[type].critRange = 0;
  weapon_list[type].critMult = 1;
  weapon_list[type].weaponFlags = 0;
  weapon_list[type].cost = 0;
  weapon_list[type].damageTypes = 0;
  weapon_list[type].weight = 0;
  weapon_list[type].range = 0;
  weapon_list[type].weaponFamily = 0;
  weapon_list[type].size = 0;
  weapon_list[type].material = 0;
  weapon_list[type].handle_type = 0;
  weapon_list[type].head_type = 0;
}

void load_weapons(void) {
  int i = 0;

  for (i = 0; i < NUM_WEAPON_TYPES; i++)
    initialize_weapons(i);

  /*	(weapon num, name, numDamDice, sizeDamDice, critRange, critMult, weapon flags, cost,
   * damageType, weight, range, weaponFamily, Size, material,
   * handle, head) */
  setweapon(WEAPON_TYPE_UNARMED, "unarmed", 1, 3, 0, 2, WEAPON_FLAG_SIMPLE, 200,
          DAMAGE_TYPE_BLUDGEONING, 1, 0, WEAPON_FAMILY_MONK, SIZE_SMALL, MATERIAL_ORGANIC,
          HANDLE_TYPE_GLOVE, HEAD_TYPE_FIST);
  setweapon(WEAPON_TYPE_DAGGER, "dagger", 1, 4, 1, 2, WEAPON_FLAG_THROWN |
          WEAPON_FLAG_SIMPLE, 200, DAMAGE_TYPE_PIERCING, 1, 10, WEAPON_FAMILY_SMALL_BLADE, SIZE_TINY,
          MATERIAL_STEEL, HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_LIGHT_MACE, "light mace", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE, 500,
          DAMAGE_TYPE_BLUDGEONING, 4, 0, WEAPON_FAMILY_CLUB, SIZE_SMALL, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_SICKLE, "sickle", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE, 600,
          DAMAGE_TYPE_SLASHING, 2, 0, WEAPON_FAMILY_SMALL_BLADE, SIZE_SMALL, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_CLUB, "club", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE, 10,
          DAMAGE_TYPE_BLUDGEONING, 3, 0, WEAPON_FAMILY_CLUB, SIZE_SMALL, MATERIAL_WOOD,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_HEAVY_MACE, "heavy mace", 1, 8, 0, 2, WEAPON_FLAG_SIMPLE, 1200,
          DAMAGE_TYPE_BLUDGEONING, 8, 0, WEAPON_FAMILY_CLUB, SIZE_MEDIUM, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_MORNINGSTAR, "morningstar", 1, 8, 0, 2, WEAPON_FLAG_SIMPLE, 800,
          DAMAGE_TYPE_BLUDGEONING | DAMAGE_TYPE_PIERCING, 6, 0, WEAPON_FAMILY_FLAIL, SIZE_MEDIUM,
          MATERIAL_STEEL, HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_SHORTSPEAR, "shortspear", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE |
          WEAPON_FLAG_THROWN, 100, DAMAGE_TYPE_PIERCING, 3, 20, WEAPON_FAMILY_SPEAR, SIZE_MEDIUM,
          MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
  setweapon(WEAPON_TYPE_LONGSPEAR, "longspear", 1, 8, 0, 3, WEAPON_FLAG_SIMPLE |
          WEAPON_FLAG_REACH, 500, DAMAGE_TYPE_PIERCING, 9, 0, WEAPON_FAMILY_SPEAR, SIZE_LARGE,
          MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
  setweapon(WEAPON_TYPE_QUARTERSTAFF, "quarterstaff", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE,
          10, DAMAGE_TYPE_BLUDGEONING, 4, 0, WEAPON_FAMILY_MONK, SIZE_LARGE,
          MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_SPEAR, "spear", 1, 8, 0, 3, WEAPON_FLAG_SIMPLE |
          WEAPON_FLAG_THROWN | WEAPON_FLAG_REACH, 200, DAMAGE_TYPE_PIERCING, 6, 20, WEAPON_FAMILY_SPEAR, SIZE_LARGE,
          MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
  setweapon(WEAPON_TYPE_HEAVY_CROSSBOW, "heavy crossbow", 1, 10, 1, 2, WEAPON_FLAG_SIMPLE
          | WEAPON_FLAG_SLOW_RELOAD | WEAPON_FLAG_RANGED, 5000, DAMAGE_TYPE_PIERCING, 8, 120,
          WEAPON_FAMILY_CROSSBOW, SIZE_LARGE, MATERIAL_WOOD, HANDLE_TYPE_HANDLE, HEAD_TYPE_BOW);
  setweapon(WEAPON_TYPE_LIGHT_CROSSBOW, "light crossbow", 1, 8, 1, 2, WEAPON_FLAG_SIMPLE
          | WEAPON_FLAG_SLOW_RELOAD | WEAPON_FLAG_RANGED, 3500, DAMAGE_TYPE_PIERCING, 4, 80,
          WEAPON_FAMILY_CROSSBOW, SIZE_MEDIUM, MATERIAL_WOOD, HANDLE_TYPE_HANDLE, HEAD_TYPE_BOW);
  /*	(weapon num, name, numDamDice, sizeDamDice, critRange, critMult, weapon flags, cost, damageType, weight, range, weaponFamily, Size, material, handle, head) */
  setweapon(WEAPON_TYPE_DART, "dart", 1, 4, 0, 2, WEAPON_FLAG_SIMPLE | WEAPON_FLAG_THROWN
          | WEAPON_FLAG_RANGED, 50, DAMAGE_TYPE_PIERCING, 1, 20, WEAPON_FAMILY_THROWN, SIZE_TINY,
          MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
  setweapon(WEAPON_TYPE_JAVELIN, "javelin", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE |
          WEAPON_FLAG_THROWN | WEAPON_FLAG_RANGED, 100, DAMAGE_TYPE_PIERCING, 2, 30,
          WEAPON_FAMILY_SPEAR, SIZE_MEDIUM, MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
  setweapon(WEAPON_TYPE_SLING, "sling", 1, 4, 0, 2, WEAPON_FLAG_SIMPLE |
          WEAPON_FLAG_RANGED, 10, DAMAGE_TYPE_BLUDGEONING, 1, 50, WEAPON_FAMILY_THROWN, SIZE_SMALL,
          MATERIAL_LEATHER, HANDLE_TYPE_STRAP, HEAD_TYPE_POUCH);
  setweapon(WEAPON_TYPE_THROWING_AXE, "throwing axe", 1, 6, 0, 2, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_THROWN, 800, DAMAGE_TYPE_SLASHING, 2, 10, WEAPON_FAMILY_AXE, SIZE_SMALL,
          MATERIAL_STEEL, HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_LIGHT_HAMMER, "light hammer", 1, 4, 0, 2, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_THROWN, 100, DAMAGE_TYPE_BLUDGEONING, 2, 20, WEAPON_FAMILY_HAMMER, SIZE_SMALL,
          MATERIAL_STEEL, HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_HAND_AXE, "hand axe", 1, 6, 0, 3, WEAPON_FLAG_MARTIAL, 600,
          DAMAGE_TYPE_SLASHING, 3, 0, WEAPON_FAMILY_AXE, SIZE_SMALL, MATERIAL_STEEL, HANDLE_TYPE_HANDLE,
          HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_KUKRI, "kukri", 1, 4, 2, 2, WEAPON_FLAG_MARTIAL, 800,
          DAMAGE_TYPE_SLASHING, 2, 0, WEAPON_FAMILY_SMALL_BLADE, SIZE_SMALL, MATERIAL_STEEL,
          HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_LIGHT_PICK, "light pick", 1, 4, 0, 4, WEAPON_FLAG_MARTIAL, 400,
          DAMAGE_TYPE_PIERCING, 3, 0, WEAPON_FAMILY_PICK, SIZE_SMALL, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_SAP, "sap", 1, 6, 0, 2, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_NONLETHAL, 100, DAMAGE_TYPE_BLUDGEONING | DAMAGE_TYPE_NONLETHAL, 2, 0,
          WEAPON_FAMILY_CLUB, SIZE_SMALL, MATERIAL_LEATHER, HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_SHORT_SWORD, "short sword", 1, 6, 1, 2, WEAPON_FLAG_MARTIAL,
          1000, DAMAGE_TYPE_PIERCING, 2, 0, WEAPON_FAMILY_SMALL_BLADE, SIZE_SMALL, MATERIAL_STEEL,
          HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_BATTLE_AXE, "battle axe", 1, 8, 0, 3, WEAPON_FLAG_MARTIAL, 1000,
          DAMAGE_TYPE_SLASHING, 6, 0, WEAPON_FAMILY_AXE, SIZE_MEDIUM, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
  /*	(weapon num, name, numDamDice, sizeDamDice, critRange, critMult, weapon flags, cost, damageType, weight, range, weaponFamily, Size, material, handle, head) */
  setweapon(WEAPON_TYPE_FLAIL, "flail", 1, 8, 0, 2, WEAPON_FLAG_MARTIAL, 800,
          DAMAGE_TYPE_BLUDGEONING, 5, 0, WEAPON_FAMILY_FLAIL, SIZE_MEDIUM, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_LONG_SWORD, "long sword", 1, 8, 1, 2, WEAPON_FLAG_MARTIAL, 1500,
          DAMAGE_TYPE_SLASHING, 4, 0, WEAPON_FAMILY_MEDIUM_BLADE, SIZE_MEDIUM, MATERIAL_STEEL,
          HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_HEAVY_PICK, "heavy pick", 1, 6, 0, 4, WEAPON_FLAG_MARTIAL, 800,
          DAMAGE_TYPE_PIERCING, 6, 0, WEAPON_FAMILY_PICK, SIZE_MEDIUM, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_RAPIER, "rapier", 1, 6, 2, 2, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_BALANCED, 2000, DAMAGE_TYPE_PIERCING, 2, 0, WEAPON_FAMILY_SMALL_BLADE,
          SIZE_SMALL, MATERIAL_STEEL, HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_SCIMITAR, "scimitar", 1, 6, 2, 2, WEAPON_FLAG_MARTIAL, 1500,
          DAMAGE_TYPE_SLASHING, 4, 0, WEAPON_FAMILY_MEDIUM_BLADE, SIZE_MEDIUM, MATERIAL_STEEL,
          HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_TRIDENT, "trident", 1, 8, 0, 2, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_THROWN, 1500, DAMAGE_TYPE_PIERCING, 4, 0, WEAPON_FAMILY_SPEAR, SIZE_MEDIUM,
          MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
  setweapon(WEAPON_TYPE_WARHAMMER, "warhammer", 1, 8, 0, 3, WEAPON_FLAG_MARTIAL, 1200,
          DAMAGE_TYPE_BLUDGEONING, 5, 0, WEAPON_FAMILY_HAMMER, SIZE_MEDIUM, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_FALCHION, "falchion", 2, 4, 2, 2, WEAPON_FLAG_MARTIAL, 7500,
          DAMAGE_TYPE_SLASHING, 8, 0, WEAPON_FAMILY_LARGE_BLADE, SIZE_LARGE, MATERIAL_STEEL,
          HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_GLAIVE, "glaive", 1, 10, 0, 3, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_REACH, 800, DAMAGE_TYPE_SLASHING, 10, 0, WEAPON_FAMILY_POLEARM, SIZE_LARGE,
          MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_GREAT_AXE, "great axe", 1, 12, 0, 3, WEAPON_FLAG_MARTIAL, 2000,
          DAMAGE_TYPE_SLASHING, 12, 0, WEAPON_FAMILY_AXE, SIZE_LARGE, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_GREAT_CLUB, "great club", 1, 10, 0, 2, WEAPON_FLAG_MARTIAL, 500,
          DAMAGE_TYPE_BLUDGEONING, 8, 0, WEAPON_FAMILY_CLUB, SIZE_LARGE, MATERIAL_WOOD,
          HANDLE_TYPE_SHAFT, HEAD_TYPE_HEAD);
  /*	(weapon num, name, numDamDice, sizeDamDice, critRange, critMult, weapon flags, cost, damageType, weight, range, weaponFamily, Size, material, handle, head) */
  setweapon(WEAPON_TYPE_HEAVY_FLAIL, "heavy flail", 1, 10, 1, 2, WEAPON_FLAG_MARTIAL,
          1500, DAMAGE_TYPE_BLUDGEONING, 10, 0, WEAPON_FAMILY_FLAIL, SIZE_LARGE, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_GREAT_SWORD, "great sword", 2, 6, 1, 2, WEAPON_FLAG_MARTIAL,
          5000, DAMAGE_TYPE_SLASHING, 8, 0, WEAPON_FAMILY_LARGE_BLADE, SIZE_LARGE, MATERIAL_STEEL,
          HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_GUISARME, "guisarme", 2, 4, 0, 3, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_REACH, 900, DAMAGE_TYPE_SLASHING, 12, 0, WEAPON_FAMILY_POLEARM, SIZE_LARGE,
          MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_HALBERD, "halberd", 1, 10, 0, 3, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_REACH, 1000, DAMAGE_TYPE_SLASHING | DAMAGE_TYPE_PIERCING, 12, 0,
          WEAPON_FAMILY_POLEARM, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_LANCE, "lance", 1, 8, 0, 3, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_REACH | WEAPON_FLAG_CHARGE, 1000, DAMAGE_TYPE_PIERCING, 10, 0,
          WEAPON_FAMILY_POLEARM, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
  setweapon(WEAPON_TYPE_RANSEUR, "ranseur", 2, 4, 0, 3, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_REACH, 1000, DAMAGE_TYPE_PIERCING, 10, 0, WEAPON_FAMILY_POLEARM, SIZE_LARGE,
          MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
  setweapon(WEAPON_TYPE_SCYTHE, "scythe", 2, 4, 0, 4, WEAPON_FLAG_MARTIAL, 1800,
          DAMAGE_TYPE_SLASHING | DAMAGE_TYPE_PIERCING, 10, 0, WEAPON_FAMILY_POLEARM, SIZE_LARGE,
          MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_LONG_BOW, "long bow", 1, 8, 0, 3, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_RANGED, 7500, DAMAGE_TYPE_PIERCING, 3, 100, WEAPON_FAMILY_BOW, SIZE_MEDIUM,
          MATERIAL_WOOD, HANDLE_TYPE_STRING, HEAD_TYPE_BOW);
  setweapon(WEAPON_TYPE_COMPOSITE_LONGBOW, "composite long bow", 1, 8, 0, 3,
          WEAPON_FLAG_MARTIAL | WEAPON_FLAG_RANGED, 10000, DAMAGE_TYPE_PIERCING, 3, 110,
          WEAPON_FAMILY_BOW, SIZE_MEDIUM, MATERIAL_WOOD, HANDLE_TYPE_STRING, HEAD_TYPE_BOW);
  setweapon(WEAPON_TYPE_SHORT_BOW, "short bow", 1, 6, 0, 3, WEAPON_FLAG_MARTIAL |
          WEAPON_FLAG_RANGED, 3000, DAMAGE_TYPE_PIERCING, 2, 60, WEAPON_FAMILY_BOW, SIZE_SMALL,
          MATERIAL_WOOD, HANDLE_TYPE_STRING, HEAD_TYPE_BOW);
  setweapon(WEAPON_TYPE_COMPOSITE_SHORTBOW, "composite short bow", 1, 6, 0, 3,
          WEAPON_FLAG_MARTIAL | WEAPON_FLAG_RANGED, 7500, DAMAGE_TYPE_PIERCING, 2, 70,
          WEAPON_FAMILY_BOW, SIZE_SMALL, MATERIAL_WOOD, HANDLE_TYPE_STRING, HEAD_TYPE_BOW);
  /*	(weapon num, name, numDamDice, sizeDamDice, critRange, critMult, weapon flags, cost, damageType, weight, range, weaponFamily, Size, material, handle, head) */
  setweapon(WEAPON_TYPE_KAMA, "kama", 1, 6, 0, 2, WEAPON_FLAG_EXOTIC, 200,
          DAMAGE_TYPE_SLASHING, 2, 0, WEAPON_FAMILY_MONK, SIZE_SMALL, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_NUNCHAKU, "nunchaku", 1, 6, 1, 2, WEAPON_FLAG_EXOTIC, 200,
          DAMAGE_TYPE_BLUDGEONING, 2, 0, WEAPON_FAMILY_MONK, SIZE_SMALL, MATERIAL_WOOD,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_SAI, "sai", 1, 4, 1, 2, WEAPON_FLAG_EXOTIC | WEAPON_FLAG_THROWN,
          100, DAMAGE_TYPE_BLUDGEONING, 1, 10, WEAPON_FAMILY_MONK, SIZE_SMALL, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_POINT);
  setweapon(WEAPON_TYPE_SIANGHAM, "siangham", 1, 6, 1, 2, WEAPON_FLAG_EXOTIC, 300,
          DAMAGE_TYPE_PIERCING, 1, 0, WEAPON_FAMILY_MONK, SIZE_SMALL, MATERIAL_STEEL,
          HANDLE_TYPE_HANDLE, HEAD_TYPE_POINT);
  setweapon(WEAPON_TYPE_BASTARD_SWORD, "bastard sword", 1, 10, 1, 2, WEAPON_FLAG_EXOTIC,
          3500, DAMAGE_TYPE_SLASHING, 6, 0, WEAPON_FAMILY_MEDIUM_BLADE, SIZE_MEDIUM, MATERIAL_STEEL,
          HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_DWARVEN_WAR_AXE, "dwarven war axe", 1, 10, 0, 3,
          WEAPON_FLAG_EXOTIC, 3000, DAMAGE_TYPE_SLASHING, 8, 0, WEAPON_FAMILY_AXE, SIZE_MEDIUM,
          MATERIAL_STEEL, HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_WHIP, "whip", 1, 3, 0, 2, WEAPON_FLAG_EXOTIC | WEAPON_FLAG_REACH
          | WEAPON_FLAG_DISARM | WEAPON_FLAG_TRIP, 100, DAMAGE_TYPE_SLASHING, 2, 0, WEAPON_FAMILY_WHIP,
          SIZE_MEDIUM, MATERIAL_LEATHER, HANDLE_TYPE_HANDLE, HEAD_TYPE_CORD);
  setweapon(WEAPON_TYPE_SPIKED_CHAIN, "spiked chain", 2, 4, 0, 2, WEAPON_FLAG_EXOTIC |
          WEAPON_FLAG_REACH | WEAPON_FLAG_DISARM | WEAPON_FLAG_TRIP, 2500, DAMAGE_TYPE_PIERCING, 10, 0,
          WEAPON_FAMILY_WHIP, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_GRIP, HEAD_TYPE_CHAIN);
  setweapon(WEAPON_TYPE_DOUBLE_AXE, "double-headed axe", 1, 8, 0, 3, WEAPON_FLAG_EXOTIC |
          WEAPON_FLAG_DOUBLE, 6500, DAMAGE_TYPE_SLASHING, 15, 0, WEAPON_FAMILY_DOUBLE, SIZE_LARGE,
          MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_DIRE_FLAIL, "dire flail", 1, 8, 0, 2, WEAPON_FLAG_EXOTIC |
          WEAPON_FLAG_DOUBLE, 9000, DAMAGE_TYPE_BLUDGEONING, 10, 0, WEAPON_FAMILY_DOUBLE, SIZE_LARGE,
          MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_HEAD);
  setweapon(WEAPON_TYPE_HOOKED_HAMMER, "hooked hammer", 1, 6, 0, 4, WEAPON_FLAG_EXOTIC |
          WEAPON_FLAG_DOUBLE, 2000, DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_BLUDGEONING, 6, 0,
          WEAPON_FAMILY_DOUBLE, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_HEAD);
  /*	(weapon num, name, numDamDice, sizeDamDice, critRange, critMult, weapon flags, cost, damageType, weight, range, weaponFamily, Size, material, handle, head) */
  setweapon(WEAPON_TYPE_2_BLADED_SWORD, "two-bladed sword", 1, 8, 1, 2,
          WEAPON_FLAG_EXOTIC | WEAPON_FLAG_DOUBLE, 10000, DAMAGE_TYPE_SLASHING, 10, 0,
          WEAPON_FAMILY_DOUBLE, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_DWARVEN_URGOSH, "dwarven urgosh", 1, 7, 0, 3, WEAPON_FLAG_EXOTIC
          | WEAPON_FLAG_DOUBLE, 5000, DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_SLASHING, 12, 0,
          WEAPON_FAMILY_DOUBLE, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
  setweapon(WEAPON_TYPE_HAND_CROSSBOW, "hand crossbow", 1, 4, 1, 2, WEAPON_FLAG_EXOTIC |
          WEAPON_FLAG_RANGED, 10000, DAMAGE_TYPE_PIERCING, 2, 30, WEAPON_FAMILY_CROSSBOW, SIZE_SMALL,
          MATERIAL_WOOD, HANDLE_TYPE_HANDLE, HEAD_TYPE_BOW);
  setweapon(WEAPON_TYPE_HEAVY_REP_XBOW, "heavy repeating crossbow", 1, 10, 1, 2,
          WEAPON_FLAG_EXOTIC | WEAPON_FLAG_RANGED | WEAPON_FLAG_REPEATING, 40000, DAMAGE_TYPE_PIERCING, 12, 120,
          WEAPON_FAMILY_CROSSBOW, SIZE_LARGE, MATERIAL_WOOD, HANDLE_TYPE_HANDLE, HEAD_TYPE_BOW);
  setweapon(WEAPON_TYPE_LIGHT_REP_XBOW, "light repeating crossbow", 1, 8, 1, 2,
          WEAPON_FLAG_EXOTIC | WEAPON_FLAG_RANGED, 25000, DAMAGE_TYPE_PIERCING, 6, 80,
          WEAPON_FAMILY_CROSSBOW, SIZE_MEDIUM, MATERIAL_WOOD, HANDLE_TYPE_HANDLE, HEAD_TYPE_BOW);
  setweapon(WEAPON_TYPE_BOLA, "bola", 1, 4, 0, 2, WEAPON_FLAG_EXOTIC | WEAPON_FLAG_THROWN
          | WEAPON_FLAG_TRIP, 500, DAMAGE_TYPE_BLUDGEONING, 2, 10, WEAPON_FAMILY_THROWN, SIZE_MEDIUM,
          MATERIAL_LEATHER, HANDLE_TYPE_GRIP, HEAD_TYPE_CORD);
  setweapon(WEAPON_TYPE_NET, "net", 1, 1, 0, 1, WEAPON_FLAG_EXOTIC | WEAPON_FLAG_THROWN |
          WEAPON_FLAG_ENTANGLE, 2000, DAMAGE_TYPE_BLUDGEONING, 6, 10, WEAPON_FAMILY_THROWN, SIZE_LARGE,
          MATERIAL_LEATHER, HANDLE_TYPE_GRIP, HEAD_TYPE_MESH);
  setweapon(WEAPON_TYPE_SHURIKEN, "shuriken", 1, 2, 0, 2, WEAPON_FLAG_EXOTIC |
          WEAPON_FLAG_THROWN, 20, DAMAGE_TYPE_PIERCING, 1, 10, WEAPON_FAMILY_MONK, SIZE_SMALL,
          MATERIAL_STEEL, HANDLE_TYPE_GRIP, HEAD_TYPE_BLADE);
}

/************** ------- ARMOR ----------************************************/

/* some utility functions necessary for our piecemail armor system, everything
 * is up for changes since this is highly experimental system */

/* Armor types */ /*
#define ARMOR_TYPE_NONE     0
#define ARMOR_TYPE_LIGHT    1
#define ARMOR_TYPE_MEDIUM   2
#define ARMOR_TYPE_HEAVY    3
#define ARMOR_TYPE_SHIELD   4
#define ARMOR_TYPE_TOWER_SHIELD   5
#define NUM_ARMOR_TYPES     6 */
/* we have to be strict here, some classes such as monk require armor_type
   check, we are going to return the lowest armortype-value that the given
   ch is wearing */
int compute_gear_armor_type(struct char_data *ch) {
  int armor_type = ARMOR_TYPE_NONE, armor_compare = ARMOR_TYPE_NONE, i;
  struct obj_data *obj = NULL;

  for (i = 0; i < NUM_WEARS; i++) {
    obj = GET_EQ(ch, i);
    if (obj && GET_OBJ_TYPE(obj) == ITEM_ARMOR) {
      /* ok we have an armor piece... */
      armor_compare = armor_list[GET_OBJ_VAL(obj, 1)].armorType;
      if (armor_compare < ARMOR_TYPE_SHIELD && armor_compare > armor_type) {
        armor_type = armor_compare;
      }
    }
  }

  return armor_type;
}

int compute_gear_shield_type(struct char_data *ch) {
  int shield_type = ARMOR_TYPE_NONE;
  struct obj_data *obj = GET_EQ(ch, WEAR_SHIELD);

  if (obj) {
    shield_type = armor_list[GET_OBJ_VAL(obj, 1)].armorType;
    if (shield_type != ARMOR_TYPE_SHIELD && shield_type != ARMOR_TYPE_TOWER_SHIELD) {
      shield_type = ARMOR_TYPE_NONE;
    }
  }

  return shield_type;
}

/* enhancement bonus + material bonus */
int compute_gear_enhancement_bonus(struct char_data *ch) {
  struct obj_data *obj = NULL;
  int enhancement_bonus = 0, material_bonus = 0, i, count = 0;

  for (i = 0; i < NUM_WEARS; i++) {
    /* exit slots */
    switch (i) {
      default:
        break;
    }
    obj = GET_EQ(ch, i);
    if (obj && GET_OBJ_TYPE(obj) == ITEM_ARMOR &&
        (i == WEAR_BODY || i == WEAR_HEAD || i == WEAR_LEGS || i == WEAR_ARMS ||
         i == WEAR_SHIELD) ) {
      count++;
      /* ok we have an armor piece... */
      enhancement_bonus += GET_OBJ_VAL(obj, 4);
      switch (GET_OBJ_MATERIAL(obj)) {
        case MATERIAL_ADAMANTINE:
        case MATERIAL_MITHRIL:
        case MATERIAL_DRAGONHIDE:
        case MATERIAL_DIAMOND:
        case MATERIAL_DARKWOOD:
          material_bonus++;
          break;
        default:
          break;
      }
    }
  }

  if (count) {/* divide by zero! :p */
    enhancement_bonus = enhancement_bonus / count;
    enhancement_bonus += MAX(0, material_bonus / count);
  }

  return enhancement_bonus;
}

/* should return a percentage */
int compute_gear_spell_failure(struct char_data *ch) {
  int spell_failure = 0, i, count = 0;
  struct obj_data *obj = NULL;

  for (i = 0; i < NUM_WEARS; i++) {
    obj = GET_EQ(ch, i);
    if (obj && GET_OBJ_TYPE(obj) == ITEM_ARMOR &&
        (i == WEAR_BODY || i == WEAR_HEAD || i == WEAR_LEGS || i == WEAR_ARMS ||
         i == WEAR_SHIELD)) {
      if (i != WEAR_SHIELD) /* shield and armor combined increase spell failure chance */
        count++;
      /* ok we have an armor piece... */
      spell_failure += armor_list[GET_OBJ_VAL(obj, 1)].spellFail;
    }
  }

  if (count) {
    spell_failure = spell_failure / count;
  }

  /* 5% improvement in spell success with this feat */
  spell_failure -= HAS_FEAT(ch, FEAT_ARMORED_SPELLCASTING) * 5;

  if (spell_failure < 0)
    spell_failure = 0;
  if (spell_failure > 100)
    spell_failure = 100;

  return spell_failure;
}

/* for doing (usually) dexterity based tasks */
int compute_gear_armor_penalty(struct char_data *ch) {
  int armor_penalty = 0, i, count = 0;

  struct obj_data *obj = NULL;

  for (i = 0; i < NUM_WEARS; i++) {
    obj = GET_EQ(ch, i);
    if (obj && GET_OBJ_TYPE(obj) == ITEM_ARMOR &&
        (i == WEAR_BODY || i == WEAR_HEAD || i == WEAR_LEGS || i == WEAR_ARMS ||
         i == WEAR_SHIELD)) {
      count++;
      /* ok we have an armor piece... */
      armor_penalty += armor_list[GET_OBJ_VAL(obj, 1)].armorCheck;
    }
  }

  if (count) {
    armor_penalty = armor_penalty / count;
  }

  if (armor_penalty > 0)
    armor_penalty = 0;
  if (armor_penalty < -10)
    armor_penalty = -10;

  return armor_penalty;
}

/* maximum dexterity bonus, returns 99 for no limit */
int compute_gear_max_dex(struct char_data *ch) {
  int dexterity_cap = 0;
  int armor_max_dexterity = 0, i, count = 0;

  struct obj_data *obj = NULL;

  for (i = 0; i < NUM_WEARS; i++) {
    obj = GET_EQ(ch, i);
    if (obj && GET_OBJ_TYPE(obj) == ITEM_ARMOR &&
        (i == WEAR_BODY || i == WEAR_HEAD || i == WEAR_LEGS || i == WEAR_ARMS ||
         i == WEAR_SHIELD)) {
      /* ok we have an armor piece... */
      armor_max_dexterity = armor_list[GET_OBJ_VAL(obj, 1)].dexBonus;
      if (armor_max_dexterity > 8) /* no limit */
        armor_max_dexterity = 9;
      dexterity_cap += armor_max_dexterity;
      count++;
    }
  }

  if (count) {
    dexterity_cap = dexterity_cap / count;
  } else
    dexterity_cap = 99;

  if (dexterity_cap > 8)
    dexterity_cap = 99; /* no limit */
  if (dexterity_cap < 0)
    dexterity_cap = 0;

  return dexterity_cap;
}

int is_proficient_with_shield(struct char_data *ch) {
  struct obj_data *shield = GET_EQ(ch, WEAR_SHIELD);

  if (IS_NPC(ch))
    return FALSE;

  if (!shield)
    return TRUE;

  switch (GET_ARMOR_TYPE_PROF(shield)) {
    case ARMOR_TYPE_SHIELD:
      if (HAS_FEAT(ch, FEAT_ARMOR_PROFICIENCY_SHIELD))
        return TRUE;
      break;
    case ARMOR_TYPE_TOWER_SHIELD:
      if (HAS_FEAT(ch, FEAT_ARMOR_PROFICIENCY_TOWER_SHIELD))
        return TRUE;
      break;
  }

  return FALSE;
}

int is_proficient_with_body_armor(struct char_data *ch) {
  struct obj_data *body_armor = GET_EQ(ch, WEAR_BODY);

  if (IS_NPC(ch))
    return FALSE;

  if (!body_armor)
    return TRUE;

  switch (GET_ARMOR_TYPE_PROF(body_armor)) {
    case ARMOR_TYPE_NONE:
      return TRUE;
      break;
    case ARMOR_TYPE_LIGHT:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_LIGHT))
        return TRUE;
      break;
    case ARMOR_TYPE_MEDIUM:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_MEDIUM))
        return TRUE;
      break;
    case ARMOR_TYPE_HEAVY:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_HEAVY))
        return TRUE;
      break;
    default:
      break;
  }

  return FALSE;
}

int is_proficient_with_helm(struct char_data *ch) {
  struct obj_data *head_armor = GET_EQ(ch, WEAR_HEAD);

  if (IS_NPC(ch))
    return FALSE;

  if (!head_armor)
    return TRUE;

  switch (GET_ARMOR_TYPE_PROF(head_armor)) {
    case ARMOR_TYPE_NONE:
      return TRUE;
      break;
    case ARMOR_TYPE_LIGHT:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_LIGHT))
        return TRUE;
      break;
    case ARMOR_TYPE_MEDIUM:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_MEDIUM))
        return TRUE;
      break;
    case ARMOR_TYPE_HEAVY:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_HEAVY))
        return TRUE;
      break;
    default:
      break;
  }

  return FALSE;
}

int is_proficient_with_sleeves(struct char_data *ch) {
  struct obj_data *arm_armor = GET_EQ(ch, WEAR_ARMS);

  if (IS_NPC(ch))
    return FALSE;

  if (!arm_armor)
    return TRUE;

  switch (GET_ARMOR_TYPE_PROF(arm_armor)) {
    case ARMOR_TYPE_NONE:
      return TRUE;
      break;
    case ARMOR_TYPE_LIGHT:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_LIGHT))
        return TRUE;
      break;
    case ARMOR_TYPE_MEDIUM:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_MEDIUM))
        return TRUE;
      break;
    case ARMOR_TYPE_HEAVY:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_HEAVY))
        return TRUE;
      break;
    default:
      break;
  }

  return FALSE;
}

int is_proficient_with_leggings(struct char_data *ch) {
  struct obj_data *leg_armor = GET_EQ(ch, WEAR_LEGS);

  if (IS_NPC(ch))
    return FALSE;

  if (!leg_armor)
    return TRUE;

  switch (GET_ARMOR_TYPE_PROF(leg_armor)) {
    case ARMOR_TYPE_NONE:
      return TRUE;
      break;
    case ARMOR_TYPE_LIGHT:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_LIGHT))
        return TRUE;
      break;
    case ARMOR_TYPE_MEDIUM:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_MEDIUM))
        return TRUE;
      break;
    case ARMOR_TYPE_HEAVY:
      if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_HEAVY))
        return TRUE;
      break;
    default:
      break;
  }

  return FALSE;
}

/* simply checks if ch has proficiency with given armor_type */
int is_proficient_with_armor(struct char_data *ch) {
  if (
      is_proficient_with_leggings(ch) &&
      is_proficient_with_sleeves(ch) &&
      is_proficient_with_helm(ch) &&
      is_proficient_with_body_armor(ch) &&
      is_proficient_with_shield(ch)
      )
    return TRUE;

  return FALSE;
}

void setarmor(int type, char *name, int armorType, int cost, int armorBonus,
              int dexBonus, int armorCheck, int spellFail, int thirtyFoot,
              int twentyFoot, int weight, int material, int wear) {
  armor_list[type].name = name;
  armor_list[type].armorType = armorType;
  armor_list[type].cost = cost;
  armor_list[type].armorBonus = armorBonus;
  armor_list[type].dexBonus = dexBonus;
  armor_list[type].armorCheck = armorCheck;
  armor_list[type].spellFail = spellFail;
  armor_list[type].thirtyFoot = thirtyFoot;
  armor_list[type].twentyFoot = twentyFoot;
  armor_list[type].weight = weight;
  armor_list[type].material = material;
  armor_list[type].wear = wear;
}

void initialize_armor(int type) {
  armor_list[type].name = "unused armor";
  armor_list[type].armorType = 0;
  armor_list[type].cost = 0;
  armor_list[type].armorBonus = 0;
  armor_list[type].dexBonus = 0;
  armor_list[type].armorCheck = 0;
  armor_list[type].spellFail = 0;
  armor_list[type].thirtyFoot = 0;
  armor_list[type].twentyFoot = 0;
  armor_list[type].weight = 0;
  armor_list[type].material = 0;
  armor_list[type].wear = ITEM_WEAR_TAKE;
}

void load_armor(void) {
  int i = 0;

  for (i = 0; i <= NUM_SPEC_ARMOR_TYPES; i++)
    initialize_armor(i);

  /* (armor, name, type,
   *    cost, AC, dexBonusCap, armorCheckPenalty, spellFailChance, (move)30ft, (move)20ft,
   *    weight, material, wear) */
  setarmor(SPEC_ARMOR_TYPE_CLOTHING, "body clothing", ARMOR_TYPE_NONE,
    10, 0, 99, 0, 0, 30, 20,
    1, MATERIAL_COTTON, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_CLOTHING_HEAD, "clothing hood", ARMOR_TYPE_NONE,
    10, 0, 99, 0, 0, 30, 20,
    1, MATERIAL_COTTON, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_CLOTHING_ARMS, "cloth sleeves", ARMOR_TYPE_NONE,
    10, 0, 99, 0, 0, 30, 20,
    1, MATERIAL_COTTON, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_CLOTHING_LEGS, "cloth leggings", ARMOR_TYPE_NONE,
    10, 0, 99, 0, 0, 30, 20,
    1, MATERIAL_COTTON, ITEM_WEAR_LEGS);

  setarmor(SPEC_ARMOR_TYPE_PADDED, "padded body armor", ARMOR_TYPE_LIGHT,
    50, 7, 8, 0, 5, 30, 20,
    7, MATERIAL_COTTON, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_PADDED_HEAD, "padded armor helm", ARMOR_TYPE_LIGHT,
    50, 1, 8, 0, 5, 30, 20,
    1, MATERIAL_COTTON, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_PADDED_ARMS, "padded armor sleeves", ARMOR_TYPE_LIGHT,
    50, 1, 8, 0, 5, 30, 20,
    1, MATERIAL_COTTON, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_PADDED_LEGS, "padded armor leggings", ARMOR_TYPE_LIGHT,
    50, 1, 8, 0, 5, 30, 20,
    1, MATERIAL_COTTON, ITEM_WEAR_LEGS);

  setarmor(SPEC_ARMOR_TYPE_LEATHER, "leather armor", ARMOR_TYPE_LIGHT,
    100, 11, 6, 0, 10, 30, 20,
    9, MATERIAL_LEATHER, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_LEATHER_HEAD, "leather helm", ARMOR_TYPE_LIGHT,
    100, 3, 6, 0, 10, 30, 20,
    2, MATERIAL_LEATHER, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_LEATHER_ARMS, "leather sleeves", ARMOR_TYPE_LIGHT,
    100, 3, 6, 0, 10, 30, 20,
    2, MATERIAL_LEATHER, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_LEATHER_LEGS, "leather leggings", ARMOR_TYPE_LIGHT,
    100, 3, 6, 0, 10, 30, 20,
    2, MATERIAL_LEATHER, ITEM_WEAR_LEGS);

  /* (armor, name, type,
   *    cost, AC, dexBonusCap, armorCheckPenalty, spellFailChance, (move)30ft, (move)20ft,
   *    weight, material, wear) */
  setarmor(SPEC_ARMOR_TYPE_STUDDED_LEATHER, "studded leather armor", ARMOR_TYPE_LIGHT,
    250, 15, 5, -1, 15, 30, 20,
    11, MATERIAL_LEATHER, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_STUDDED_LEATHER_HEAD, "studded leather helm", ARMOR_TYPE_LIGHT,
    250, 5, 5, -1, 15, 30, 20,
    3, MATERIAL_LEATHER, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_STUDDED_LEATHER_ARMS, "studded leather sleeves", ARMOR_TYPE_LIGHT,
    250, 5, 5, -1, 15, 30, 20,
    3, MATERIAL_LEATHER, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_STUDDED_LEATHER_LEGS, "studded leather leggings", ARMOR_TYPE_LIGHT,
    250, 5, 5, -1, 15, 30, 20,
    3, MATERIAL_LEATHER, ITEM_WEAR_LEGS);

  setarmor(SPEC_ARMOR_TYPE_LIGHT_CHAIN, "light chainmail armor", ARMOR_TYPE_LIGHT,
    1000, 19, 4, -2, 20, 30, 20,
    13, MATERIAL_STEEL, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_LIGHT_CHAIN_HEAD, "light chainmail helm", ARMOR_TYPE_LIGHT,
    1000, 7, 4, -2, 20, 30, 20,
    4, MATERIAL_STEEL, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_LIGHT_CHAIN_ARMS, "light chainmail sleeves", ARMOR_TYPE_LIGHT,
    1000, 7, 4, -2, 20, 30, 20,
    4, MATERIAL_STEEL, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_LIGHT_CHAIN_LEGS, "light chainmail leggings", ARMOR_TYPE_LIGHT,
    1000, 7, 4, -2, 20, 30, 20,
    4, MATERIAL_STEEL, ITEM_WEAR_LEGS);

  setarmor(SPEC_ARMOR_TYPE_HIDE, "hide armor", ARMOR_TYPE_MEDIUM,
    150, 19, 4, -3, 20, 20, 15,
    13, MATERIAL_LEATHER, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_HIDE_HEAD, "hide helm", ARMOR_TYPE_MEDIUM,
    150, 7, 4, -3, 20, 20, 15,
    4, MATERIAL_LEATHER, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_HIDE_ARMS, "hide sleeves", ARMOR_TYPE_MEDIUM,
    150, 7, 4, -3, 20, 20, 15,
    4, MATERIAL_LEATHER, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_HIDE_LEGS, "hide leggings", ARMOR_TYPE_MEDIUM,
    150, 7, 4, -3, 20, 20, 15,
    4, MATERIAL_LEATHER, ITEM_WEAR_LEGS);

  /* (armor, name, type,
   *    cost, AC, dexBonusCap, armorCheckPenalty, spellFailChance, (move)30ft, (move)20ft,
   *    weight, material, wear) */
  setarmor(SPEC_ARMOR_TYPE_SCALE, "scale armor", ARMOR_TYPE_MEDIUM,
    500, 23, 3, -4, 25, 20, 15,
    15, MATERIAL_STEEL, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_SCALE_HEAD, "scale helm", ARMOR_TYPE_MEDIUM,
    500, 9, 3, -4, 25, 20, 15,
    5, MATERIAL_STEEL, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_SCALE_ARMS, "scale sleeves", ARMOR_TYPE_MEDIUM,
    500, 9, 3, -4, 25, 20, 15,
    5, MATERIAL_STEEL, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_SCALE_LEGS, "scale leggings", ARMOR_TYPE_MEDIUM,
    500, 9, 3, -4, 25, 20, 15,
    5, MATERIAL_STEEL, ITEM_WEAR_LEGS);

  setarmor(SPEC_ARMOR_TYPE_CHAINMAIL, "chainmail armor", ARMOR_TYPE_MEDIUM,
    1500, 27, 2, -5, 30, 20, 15,
    27, MATERIAL_STEEL, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_CHAINMAIL_HEAD, "chainmail helm", ARMOR_TYPE_MEDIUM,
    1500, 11, 2, -5, 30, 20, 15,
    11, MATERIAL_STEEL, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_CHAINMAIL_ARMS, "chainmail sleeves", ARMOR_TYPE_MEDIUM,
    1500, 11, 2, -5, 30, 20, 15,
    11, MATERIAL_STEEL, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_CHAINMAIL_LEGS, "chainmail leggings", ARMOR_TYPE_MEDIUM,
    1500, 11, 2, -5, 30, 20, 15,
    11, MATERIAL_STEEL, ITEM_WEAR_LEGS);

  setarmor(SPEC_ARMOR_TYPE_PIECEMEAL, "piecemeal armor", ARMOR_TYPE_MEDIUM,
    2000, 25, 3, -4, 25, 20, 15,
    19, MATERIAL_STEEL, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_PIECEMEAL_HEAD, "piecemeal helm", ARMOR_TYPE_MEDIUM,
    2000, 10, 3, -4, 25, 20, 15,
    7, MATERIAL_STEEL, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_PIECEMEAL_ARMS, "piecemeal sleeves", ARMOR_TYPE_MEDIUM,
    2000, 10, 3, -4, 25, 20, 15,
    7, MATERIAL_STEEL, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_PIECEMEAL_LEGS, "piecemeal leggings", ARMOR_TYPE_MEDIUM,
    2000, 10, 3, -4, 25, 20, 15,
    7, MATERIAL_STEEL, ITEM_WEAR_LEGS);

  /* (armor, name, type,
   *    cost, AC, dexBonusCap, armorCheckPenalty, spellFailChance, (move)30ft, (move)20ft,
   *    weight, material, wear) */
  setarmor(SPEC_ARMOR_TYPE_SPLINT, "splint mail armor", ARMOR_TYPE_HEAVY,
    2000, 31, 0, -7, 40, 20, 15,
    21, MATERIAL_STEEL, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_SPLINT_HEAD, "splint mail helm", ARMOR_TYPE_HEAVY,
    2000, 13, 0, -7, 40, 20, 15,
    8, MATERIAL_STEEL, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_SPLINT_ARMS, "splint mail sleeves", ARMOR_TYPE_HEAVY,
    2000, 13, 0, -7, 40, 20, 15,
    8, MATERIAL_STEEL, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_SPLINT_LEGS, "splint mail leggings", ARMOR_TYPE_HEAVY,
    2000, 13, 0, -7, 40, 20, 15,
    8, MATERIAL_STEEL, ITEM_WEAR_LEGS);

  setarmor(SPEC_ARMOR_TYPE_BANDED, "banded mail armor", ARMOR_TYPE_HEAVY,
    2500, 31, 1, -6, 35, 20, 15,
    17, MATERIAL_STEEL, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_BANDED_HEAD, "banded mail helm", ARMOR_TYPE_HEAVY,
    2500, 13, 1, -6, 35, 20, 15,
    6, MATERIAL_STEEL, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_BANDED_ARMS, "banded mail sleeves", ARMOR_TYPE_HEAVY,
    2500, 13, 1, -6, 35, 20, 15,
    6, MATERIAL_STEEL, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_BANDED_LEGS, "banded mail leggings", ARMOR_TYPE_HEAVY,
    2500, 13, 1, -6, 35, 20, 15,
    6, MATERIAL_STEEL, ITEM_WEAR_LEGS);

  setarmor(SPEC_ARMOR_TYPE_HALF_PLATE, "half plate armor", ARMOR_TYPE_HEAVY,
    6000, 35, 1, -6, 40, 20, 15,
    23, MATERIAL_STEEL, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_HALF_PLATE_HEAD, "half plate helm", ARMOR_TYPE_HEAVY,
    6000, 15, 1, -6, 40, 20, 15,
    9, MATERIAL_STEEL, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_HALF_PLATE_ARMS, "half plate sleeves", ARMOR_TYPE_HEAVY,
    6000, 15, 1, -6, 40, 20, 15,
    9, MATERIAL_STEEL, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_HALF_PLATE_LEGS, "half plate leggings", ARMOR_TYPE_HEAVY,
    6000, 15, 1, -6, 40, 20, 15,
    9, MATERIAL_STEEL, ITEM_WEAR_LEGS);

  /* (armor, name, type,
   *    cost, AC, dexBonusCap, armorCheckPenalty, spellFailChance, (move)30ft, (move)20ft,
   *    weight, material, wear) */
  setarmor(SPEC_ARMOR_TYPE_FULL_PLATE, "full plate armor", ARMOR_TYPE_HEAVY,
    15000, 39, 1, -6, 35, 20, 15,
    23, MATERIAL_STEEL, ITEM_WEAR_BODY);
  setarmor(SPEC_ARMOR_TYPE_FULL_PLATE_HEAD, "full plate helm", ARMOR_TYPE_HEAVY,
    15000, 17, 1, -6, 35, 20, 15,
    9, MATERIAL_STEEL, ITEM_WEAR_HEAD);
  setarmor(SPEC_ARMOR_TYPE_FULL_PLATE_ARMS, "full plate sleeves", ARMOR_TYPE_HEAVY,
    15000, 17, 1, -6, 35, 20, 15,
    9, MATERIAL_STEEL, ITEM_WEAR_ARMS);
  setarmor(SPEC_ARMOR_TYPE_FULL_PLATE_LEGS, "full plate leggings", ARMOR_TYPE_HEAVY,
    15000, 17, 1, -6, 35, 20, 15,
    9, MATERIAL_STEEL, ITEM_WEAR_LEGS);

  /* (armor, name, type,
   *    cost, AC, dexBonusCap, armorCheckPenalty, spellFailChance, (move)30ft, (move)20ft,
   *    weight, material, wear) */
  setarmor(SPEC_ARMOR_TYPE_BUCKLER, "buckler shield", ARMOR_TYPE_SHIELD,
    150, 10, 99, -1, 5, 999, 999,
    5, MATERIAL_WOOD, ITEM_WEAR_SHIELD);
  setarmor(SPEC_ARMOR_TYPE_SMALL_SHIELD, "small shield", ARMOR_TYPE_SHIELD,
    90, 10, 99, -1, 5, 999, 999,
    6, MATERIAL_WOOD, ITEM_WEAR_SHIELD);
  setarmor(SPEC_ARMOR_TYPE_LARGE_SHIELD, "heavy shield", ARMOR_TYPE_SHIELD,
    200, 20, 99, -2, 15, 999, 999,
    13, MATERIAL_WOOD, ITEM_WEAR_SHIELD);
  setarmor(SPEC_ARMOR_TYPE_TOWER_SHIELD, "tower shield", ARMOR_TYPE_TOWER_SHIELD,
    300, 40, 2, -10, 50, 999, 999,
    45, MATERIAL_WOOD, ITEM_WEAR_SHIELD);
}

/******* special mixed checks (such as monk) */

/* our simple little function to make sure our monk
   is following his martial-arts requirements for gear */
bool monk_gear_ok(struct char_data *ch) {
  struct obj_data *obj = NULL;

  /* hands have to be free, or wielding monk family weapon */
  if (GET_EQ(ch, WEAR_HOLD_1))
    return FALSE;

  if (GET_EQ(ch, WEAR_HOLD_2))
    return FALSE;

  if (GET_EQ(ch, WEAR_SHIELD))
    return FALSE;

  obj = GET_EQ(ch, WEAR_WIELD_1);
  if (obj &&
      (weapon_list[GET_WEAPON_TYPE(obj)].weaponFamily != WEAPON_FAMILY_MONK))
    return FALSE;

  obj = GET_EQ(ch, WEAR_WIELD_OFFHAND);
  if (obj &&
      (weapon_list[GET_WEAPON_TYPE(obj)].weaponFamily != WEAPON_FAMILY_MONK))
    return FALSE;

  obj = GET_EQ(ch, WEAR_WIELD_2H);
  if (obj &&
      (weapon_list[GET_WEAPON_TYPE(obj)].weaponFamily != WEAPON_FAMILY_MONK))
    return FALSE;

  /* now check to make sure he isn't wearing invalid armor */
  if (compute_gear_armor_type(ch) != ARMOR_TYPE_NONE)
    return FALSE;

  /* monk gear is ok */
  return TRUE;
}


/*********** deprecated functions *****************/

/* Proficiency Related Functions */

/*
#define ITEM_PROF_NONE		0	// no proficiency required
#define ITEM_PROF_MINIMAL	1	//  "Minimal Weapon Proficiency"
#define ITEM_PROF_BASIC		2	//  "Basic Weapon Proficiency"
#define ITEM_PROF_ADVANCED	3	//  "Advanced Weapon Proficiency"
#define ITEM_PROF_MASTER 	4	//  "Master Weapon Proficiency"
#define ITEM_PROF_EXOTIC 	5	//  "Exotic Weapon Proficiency"
#define ITEM_PROF_LIGHT_A	6	// light armor prof
#define ITEM_PROF_MEDIUM_A	7	// medium armor prof
#define ITEM_PROF_HEAVY_A	8	// heavy armor prof
#define ITEM_PROF_SHIELDS	9	// shield prof
#define ITEM_PROF_T_SHIELDS	10	// tower shield prof
 */

/* a function to check the -highest- level of proficiency of gear
   worn on a character
 * in:  requires character (pc only), type is either weapon/armor/shield
 * out:  value of highest proficiency worn
 *  */

/* deprecated */
/*
int proficiency_worn(struct char_data *ch, int type) {
  int prof = ITEM_PROF_NONE, i = 0;

  for (i = 0; i < NUM_WEARS; i++) {
    if (GET_EQ(ch, i)) {
      if (type == WEAPON_PROFICIENCY && (
              i == WEAR_WIELD_1 ||
              i == WEAR_WIELD_OFFHAND ||
              i == WEAR_WIELD_2H
              )) {
        if (GET_OBJ_PROF(GET_EQ(ch, i)) > prof) {
          prof = GET_OBJ_PROF(GET_EQ(ch, i));
        }
      } else if (type == SHIELD_PROFICIENCY && (
              i == WEAR_SHIELD
              )) {
        if (GET_OBJ_PROF(GET_EQ(ch, i)) > prof) {
          prof = GET_OBJ_PROF(GET_EQ(ch, i));
        }
      } else if (type == ARMOR_PROFICIENCY && (
              i != WEAR_WIELD_1 &&
              i != WEAR_WIELD_OFFHAND &&
              i != WEAR_WIELD_2H &&
              i != WEAR_SHIELD
              )) {
        if (GET_OBJ_PROF(GET_EQ(ch, i)) > prof) {
          prof = GET_OBJ_PROF(GET_EQ(ch, i));
        }
      }
    }
  }

  switch (type) {
    case WEAPON_PROFICIENCY:
      if (prof < 0 || prof >= NUM_WEAPON_PROFS)
        return ITEM_PROF_NONE;
      break;
    case ARMOR_PROFICIENCY:
      if (prof < NUM_WEAPON_PROFS || prof >= NUM_ARMOR_PROFS)
        return ITEM_PROF_NONE;
      break;
    case SHIELD_PROFICIENCY:
      if (prof < NUM_ARMOR_PROFS || prof >= NUM_SHIELD_PROFS)
        return ITEM_PROF_NONE;
      break;
  }

  return prof;
}
*/

/* deprecated */
/*
int determine_gear_weight(struct char_data *ch, int type) {
  int i = 0, weight = 0;

  for (i = 0; i < NUM_WEARS; i++) {
    if (GET_EQ(ch, i)) {
      if (type == WEAPON_PROFICIENCY && (
              i == WEAR_WIELD_1 ||
              i == WEAR_WIELD_OFFHAND ||
              i == WEAR_WIELD_2H
              )) {
        weight += GET_OBJ_WEIGHT(GET_EQ(ch, i));
      } else if (type == SHIELD_PROFICIENCY && (
              i == WEAR_SHIELD
              )) {
        weight += GET_OBJ_WEIGHT(GET_EQ(ch, i));
      } else if (type == ARMOR_PROFICIENCY && (
              i == WEAR_HEAD ||
              i == WEAR_BODY ||
              i == WEAR_ARMS ||
              i == WEAR_LEGS
              )) {
        weight += GET_OBJ_WEIGHT(GET_EQ(ch, i));
      }
    }
  }

  return weight;
}
*/

/* this function will determine the penalty (or lack of) created
 by the gear the character is wearing - this penalty is mostly in
 regards to rogue-like skills such as sneak/hide */
/* deprecated */
/*
int compute_gear_penalty_check(struct char_data *ch) {
  int factor = determine_gear_weight(ch, ARMOR_PROFICIENCY);
  factor += determine_gear_weight(ch, SHIELD_PROFICIENCY);

  if (factor > 51)
    return -8;
  if (factor >= 45)
    return -6;
  if (factor >= 40)
    return -5;
  if (factor >= 35)
    return -4;
  if (factor >= 30)
    return -3;
  if (factor >= 25)
    return -2;
  if (factor >= 20)
    return -1;

  return 0; //should be less than 10
}
*/

/* this function will determine the % penalty created by the
   gear the char is wearing - this penalty is unique to
   arcane casting only (sorc, wizard, bard, etc) */
/* deprecated */
/*
int compute_gear_arcane_fail(struct char_data *ch) {
  int factor = determine_gear_weight(ch, ARMOR_PROFICIENCY);
  factor += determine_gear_weight(ch, SHIELD_PROFICIENCY);

  factor -= HAS_FEAT(ch, FEAT_ARMORED_SPELLCASTING) * 5;

  if (factor > 51)
    return 50;
  if (factor >= 45)
    return 40;
  if (factor >= 40)
    return 35;
  if (factor >= 35)
    return 30;
  if (factor >= 30)
    return 25;
  if (factor >= 25)
    return 20;
  if (factor >= 20)
    return 15;
  if (factor >= 15)
    return 10;
  if (factor >= 10)
    return 5;

  return 0; //should be less than 10

}
*/

/* this function will determine the dam-reduc created by the
   gear the char is wearing  */
/* deprecated */
/*
int compute_gear_dam_reduc(struct char_data *ch) {
  int factor = determine_gear_weight(ch, ARMOR_PROFICIENCY);
  int shields = determine_gear_weight(ch, SHIELD_PROFICIENCY);

  if (shields > factor)
    factor = shields;

  if (factor > 51)
    return 6;
  if (factor >= 45)
    return 4;
  if (factor >= 40)
    return 4;
  if (factor >= 35)
    return 3;
  if (factor >= 30)
    return 3;
  if (factor >= 25)
    return 2;
  if (factor >= 20)
    return 2;
  if (factor >= 15)
    return 1;
  if (factor >= 10)
    return 1;

  return 0; //should be less than 10
}
*/

/* this function will determine the max-dex created by the
   gear the char is wearing  */
/* deprecated */
/*
int compute_gear_max_dex(struct char_data *ch) {
  int factor = determine_gear_weight(ch, ARMOR_PROFICIENCY);
  int shields = determine_gear_weight(ch, SHIELD_PROFICIENCY);

  if (shields > factor)
    factor = shields;

  if (factor > 51)
    return 0;
  if (factor >= 45)
    return 1;
  if (factor >= 40)
    return 2;
  if (factor >= 35)
    return 3;
  if (factor >= 30)
    return 4;
  if (factor >= 25)
    return 5;
  if (factor >= 20)
    return 7;
  if (factor >= 15)
    return 9;
  if (factor >= 10)
    return 11;

  if (factor >= 1)
    return 13;
  else
    return 99; // wearing no weight!
}
*/