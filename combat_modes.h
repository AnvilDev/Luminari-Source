/* *************************************************************************
 *   File: combat_modes.h                              Part of LuminariMUD *
 *  Usage: Header file for combat modes.                                   *
 * Author: Ornir                                                           *
 ***************************************************************************
 * In Luminari, certain feats and classes allow the use of 'combat modes', *
 * a mode where the char gains certain bonuses and have certain penalties. *
 * A good example of this is power attack - Gain a bonus in damage by      *
 * taking a penalty to you attacks.  These modes can be changed during     *
 * combat, increasing the versatility of the fighter classes and providing *
 * additional options for the fighting man.                                *
 *                                                                         *
 ***************************************************************************/

#ifndef _COMBAT_MODES_H_
#define _COMBAT_MODES_H_

#define MODE_NONE                0
#define MODE_POWER_ATTACK        1
#define MODE_COMBAT_EXPERTISE    2
#define MODE_SPELLBATTLE         3
#define MODE_TOTAL_DEFENSE       4
#define MODE_DUAL_WIELD          5
#define MODE_FLURRY_OF_BLOWS     6
#define MODE_RAPID_SHOT          7
#define MODE_COUNTERSPELL        8
#define MODE_DEFENSIVE_CASTING   9
#define MODE_WHIRLWIND_ATTACK   10
#define MAX_MODES               11

#define MODE_GROUP_NONE          0
#define MODE_GROUP_1             1
#define MODE_GROUP_2             2
#define MODE_GROUP_3             3
#define MAX_MODE_GROUPS          4

struct combat_mode_data {
  const char* name;
  int affect_flag;
  int required_feat;
  bool has_value;
  int group;
};

extern struct combat_mode_data combat_mode_info[MAX_MODES];

ACMD(do_mode);
ACMD(do_powerattack);
ACMD(do_flurryofblows);
ACMD(do_expertise);
ACMD(do_rapidshot);
ACMD(do_totaldefense);
ACMD(do_spellbattle);
ACMD(do_flurry);
ACMD(do_whirlwind);

#endif
