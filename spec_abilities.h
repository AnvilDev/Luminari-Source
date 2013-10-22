/* *************************************************************************
 *   File: spec_abilities.h                            Part of LuminariMUD *
 *  Usage: Header file for special abilities for weapons, armor and        *
 *         shields.                                                        *
 * Author: Ornir                                                           *
 ***************************************************************************
 *                                                                         *
 * In d20/Dungeons and Dragons, special abilities are what make magic      *
 * items -magical-.  These abilities, being wreathed in fire, exploding    *
 * with frost on a critical hit etc. are part of what defineds D&D.        *
 *                                                                         *
 * In order to implement these thing in LuminariMUD, some additions to the *
 * stock object model have been made (in structs.h).  These changes allow  *
 * the addition of any number of the defined special abilities to be added *
 * to the weapon, armor or shield in addition to any APPLY_ values that    *
 * the object has.  Additionally, an activation method must be defined.    *
 *                                                                         *
 * The code is defined similarly to the spells and commands in stock code, *
 * in that macros and an array of structures are used to define new        *
 * special abilities.                                                      *
 ***************************************************************************/
#ifndef _SPEC_ABILITIES_H_
#define _SPEC_ABILITIES_H_



#define HAS_SPECIAL_ABILITIES( obj ) (obj->special_abilities == NULL ? FALSE : TRUE)

/* Activation methods */
#define ACTMTD_NONE                    (1 << 0)       /* No activation required. */
#define ACTMTD_WEAR                    (1 << 1)       /* Activates when worn. */
#define ACTMTD_USE                     (1 << 2)       /* Activates when 'use'd. */
#define ACTMTD_COMMAND_WORD            (1 << 3)       /* Activates when command word is 'utter'ed */
#define ACTMTD_ON_HIT                  (1 << 4)       /* Activates on a successful hit. */
#define ACTMTD_ON_CRIT                 (1 << 5)       /* Activates on a successful crit. */

#define NUM_ACTIVATION_METHODS        6 

extern const char* activation_methods[];

/* Special abilities for weapons, armor and shields. - 19/08/2013 Ornir                      
 * These abilities have been taken from the d20srd, 
 * augmented by other sourcebooks. */

#define WEAPON_SPECAB_NONE             0       /* No ability, placeholder. */

/* Weapons */
#define WEAPON_SPECAB_ANARCHIC         1       /* Infused with the power of Chaos. */
#define WEAPON_SPECAB_AXIOMATIC        2       /* Infused with the power of Law. */
#define WEAPON_SPECAB_BANE             3       /* Excels at attacking one type or subtype of creature. */
#define WEAPON_SPECAB_BRILLIANT_ENERGY 4       /* Significant portion of wweapon is made of light. */
#define WEAPON_SPECAB_DANCING          5       /* Weapon attacks/defends on it's own. */       
#define WEAPON_SPECAB_DEFENDING        6       /* Enhancement bonus can be transferred to AC. */
#define WEAPON_SPECAB_DISRUPTION       7       /* Enhanced damage versus undead, like turn. */
#define WEAPON_SPECAB_DISTANCE         8       /* Doubles the range of missile weapons. */
#define WEAPON_SPECAB_FLAMING          9       /* Wreathe the weapon in fire on command. */
#define WEAPON_SPECAB_FLAMING_BURST   10       /* Explode with flame on critical hits. */
#define WEAPON_SPECAB_FROST           11       /* Wreathe the weapon in frost on command. */
#define WEAPON_SPECAB_GHOST_TOUCH     12       /* Deals damage to incorporeal creatures. */
#define WEAPON_SPECAB_HOLY            13       /* Infused with holy power. */
#define WEAPON_SPECAB_ICY_BURST       14       /* Explode with frost on critical hits. */
#define WEAPON_SPECAB_KEEN            15       /* Double the threat range of the given BLADED weapon. */
#define WEAPON_SPECAB_KI_FOCUS        16       /* Allow monk ki-attacks using the weapon. */ 
#define WEAPON_SPECAB_MERCIFUL        17       /* Deals extra non-lethal damage. */
#define WEAPON_SPECAB_MIGHTY_CLEAVING 18       /* Allow one additional cleave attack each round. */
#define WEAPON_SPECAB_RETURNING       19       /* Thrown weapons only.  Weapon returns to thrower. */
#define WEAPON_SPECAB_SEEKING         20       /* Ranged weapons only.  Ignores target bonuses... */
#define WEAPON_SPECAB_SHOCK           21       /* Wreathe the weapon in electricity on command. */
#define WEAPON_SPECAB_SHOCKING_BURST  22       /* Explode with electricity on critical hits. */
#define WEAPON_SPECAB_SPEED           23       /* Grant extra attack, some restrictions. */
#define WEAPON_SPECAB_SPELL_STORING   24       /* Allow storing of up to 3rd level spell */
#define WEAPON_SPECAB_THUNDERING      25       /* Sonic damage on critical hits. */
#define WEAPON_SPECAB_THROWING        26       /* Allow melee weapon to be thrown. */
#define WEAPON_SPECAB_UNHOLY          27       /* Infused with Unholy power. */
#define WEAPON_SPECAB_VICIOUS         28       /* Weapon does extra damage but also damages wielder. */
#define WEAPON_SPECAB_VORPAL          29       /* Sever the head of the opponent on natural 20. */
#define WEAPON_SPECAB_WOUNDING        30       /* Cause blood loss on hit. (Constitution damage) */

#define NUM_WEAPON_SPECABS            31       /* Number of Special Abilities for weapons and armor. */

/* Armor and Shields */
/* NOTE: Some of these could be implemented using APPLY_* tags.  If this works, then they can be removed
 * from this list.  They are included here for completeness. */
#define ARMOR_SPECAB_NONE             0	      /* No ability, placeholder. */  
#define ARMOR_SPECAB_ACID_RESIST      1       /* Reduce damage from Acid-based attacks. */
#define ARMOR_SPECAB_ANIMATED         2       /* Shields only, shield floats in front of the wielder. */
#define ARMOR_SPECAB_ARROW_CATCHING   3       /* Shields only, divert projectiles from other targets to the shield. */
#define ARMOR_SPECAB_ARROW_DEFLECT    4       /* Shields only, deflects mundane arrows and projectiles. */
#define ARMOR_SPECAB_BASHING          5       /* Shields only, bonus to shield bash. (+1)*/
#define ARMOR_SPECAB_BLINDING         6       /* Shields only, flashes with brilliant light, short group blind. */
#define ARMOR_SPECAB_COLD_RESIST      7       /* Reduce damage from Cold-based attacks. */
#define ARMOR_SPECAB_ELEC_RESIST      8       /* Reduce damage from Electricity-based attacks. */
#define ARMOR_SPECAB_ETHEREALNESS     9       /* Allows wearer to become ethereal. */
#define ARMOR_SPECAB_FIRE_RESIST     10       /* Reduce damage from Fire-based attacks. */
#define ARMOR_SPECAB_FORTIFICATION   11       /* Proctect the wearer from sneak attack and critical hits. */
#define ARMOR_SPECAB_GHOST_TOUCH     12       /* Protects from attacks by incorporeal creatures. */
#define ARMOR_SPECAB_GLAMERED        13       /* Disguise armor as normal clothing on command. */
#define ARMOR_SPECAB_INVULNERABILTY  14       /* Provide damage Reduction 5/magic. */
#define ARMOR_SPECAB_REFLECTING      15       /* Reflect a spell back to it's caster 1x/day */
#define ARMOR_SPECAB_SHADOW          16       /* Blurs the wearer during hide attempts, bonus to hide. */
#define ARMOR_SPECAB_SILENT_MOVES    17       /* Allows wearer to move silently, bonus to sneak. */
#define ARMOR_SPECAB_SLICK           18       /* Armor is covered by a slick oil, bonus to escape artist. */
#define ARMOR_SPECAB_SONIC_RESIST    19       /* Reduce damage from Sonic-based attacks. */
#define ARMOR_SPECAB_SPELL_RESIST    20       /* Grant the wearer spell resistance. */
#define ARMOR_SPECAB_UNDEAD_CONTROL  21       /* Control up to 26HD of undead/day, lose control at dawn. */
#define ARMOR_SPECAB_WILD            22       /* Preserve armo/enhancement bonus while in wild shape. */

#define NUM_ARMOR_SPECABS            23       /* Number of Special Abilities for weapons and armor. */

bool obj_has_special_ability(struct obj_data *obj, int ability); 
struct obj_special_ability* get_obj_special_ability(struct obj_data *obj, int ability);

#define SPECAB_PROC_DEF( specab_proc ) \
                        void (*specab_proc)\
                             (struct obj_special_ability *specab, /* The ability structure, to get values. */\
                              struct obj_data *obj,               /* The item with the ability. */\
                              struct char_data *ch,               /* The wearer/wielder of the item. */\
                              struct char_data *victim,           /* The target of the ability. */\
                              int    actmtd)                      /* The activation method triggered. */

/* Structure to hold ability data. */
struct special_ability_info_type {  
  int activation_method;
  int level;
  byte violent;
  int targets; /* See below for use with TAR_XXX  */
  const char *name; /* Input size not limited. Originates from string constants. */  
  int time; /* Time required to process the ability */
  int school; /* School of magic, necessary for detect magic. */
  int cost; /* Enhancement Bonus cost. */  

  SPECAB_PROC_DEF(special_ability_proc);
};

struct special_ability_info_type weapon_special_ability_info[NUM_WEAPON_SPECABS];
struct special_ability_info_type armor_special_ability_info[NUM_ARMOR_SPECABS];

/* Macros for defining the actual abilities */
#define WEAPON_SPECIAL_ABILITY(abilityname) \
void abilityname( struct obj_special_ability *specab, \
                  struct obj_data *weapon, \
                  struct char_data *ch, \
                  struct char_data *victim, \
                  int    actmtd)

#define ARMOR_SPECIAL_ABILITY(abilityname) \
void abilityname( struct obj_special_ability *specab, \
                  struct obj_data *armor, \
                  struct char_data *ch, \
                  struct char_data *victim, \
                  int    actmtd)

void initialize_special_abilities(void);

/* Process weapon abilities for the specified activation method. */
int  process_weapon_abilities(struct obj_data  *weapon, /* The weapon to check for special abilities. */
                              struct char_data *ch,     /* The wielder of the weapon. */
                              struct char_data *victim, /* The target of the ability (either fighting or 
                                                         * specified explicitly. */
                              int    actmtd,            /* Activation method */
                              char   *cmdword);          /* Command word (optional, NULL if none. */

/* Prototypes for weapon special abilities */
WEAPON_SPECIAL_ABILITY(weapon_specab_bane);
WEAPON_SPECIAL_ABILITY(weapon_specab_flaming);
WEAPON_SPECIAL_ABILITY(weapon_specab_flaming_burst);
//WEAPON_SPECIAL_ABILITY(weapon_specab_frost);
//WEAPON_SPECIAL_ABILITY(weapon_specab_frost_burst);


#endif
