
/* ***********************************************************************
 *    File:   study.c                                Part of LuminariMUD  *
 * Purpose:   To provide menus for sorc-type casters known spells         *
 *            Header info in oasis.h                                      *
 *  Author:   Zusuk                                                       *
 ************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"

#include "comm.h"
#include "db.h"
#include "oasis.h"
#include "screen.h"
#include "interpreter.h"
#include "modify.h"
#include "spells.h"
#include "feats.h"
#include "class.h"
#include "constants.h"
#include "assign_wpn_armor.h"
#include "domains_schools.h"

/*-------------------------------------------------------------------*/
/*. Function prototypes . */

static void sorc_study_menu(struct descriptor_data *d, int circle);
static void bard_study_menu(struct descriptor_data *d, int circle);
static void favored_enemy_submenu(struct descriptor_data *d, int favored);
static void favored_enemy_menu(struct descriptor_data *d);
static void animal_companion_menu(struct descriptor_data *d);
static void familiar_menu(struct descriptor_data *d);

static void display_main_menu(struct descriptor_data *d);
static void generic_main_disp_menu(struct descriptor_data *d);
static void main_feat_disp_menu(struct descriptor_data *d);

void init_study(struct descriptor_data *d, int class);
void finalize_study(struct descriptor_data *d);
/*-------------------------------------------------------------------*/

#define MENU_OPT(i) ((i) ? grn : "\tD"), ((i) ? nrm : "\tD")

/* list of possible animal companions, use in-game vnums for this
 * These lists should actually be redesigned to be dynamic, settable
 * via an in-game OLC.  The results should be stored in the database
 * and retrieved when needed. */
#define C_BEAR      60
#define C_BOAR      61
#define C_LION      62
#define C_CROCODILE 63
#define C_HYENA     64
#define C_SNOW_LEOPARD 65
#define C_SKULL_SPIDER 66
#define C_FIRE_BEETLE  67
#define C_CAYHOUND     68
#define C_DRACAVES     69

/*--- paladin mount(s) -----*/
#define C_W_WARHORSE   70
#define C_B_DESTRIER   71
#define C_STALLION     72
#define C_A_DESTRIER   73
#define C_G_WARHORSE   74
#define C_P_WARHORSE   75
#define C_C_DESTRIER   76
#define C_WARDOG       77
#define C_WARPONY      78
#define C_GRIFFON      79
/*--- familiars -----*/
#define F_HUNTER       80
#define F_PANTHER      81
#define F_MOUSE        82
#define F_EAGLE        83
#define F_RAVEN        84
#define F_IMP          85
#define F_PIXIE        86
#define F_FAERIE_DRAGON 87
#define F_PSEUDO_DRAGON 88
#define F_HELLHOUND    89

/* make a list of vnums corresponding in order, first animals  */
int animal_vnums[] = {
  0,
  C_BEAR, // 60, 1
  C_BOAR, // 61, 2
  C_LION, // 62, 3
  C_CROCODILE, // 63, 4
  C_HYENA, // 64, 5
  C_SNOW_LEOPARD, // 65, 6
  C_SKULL_SPIDER, // 66, 7
  C_FIRE_BEETLE, // 67, 8
  C_CAYHOUND, // 68, 9
  C_DRACAVES, // 69, 10
  -1 /* end with this */
};
#define NUM_ANIMALS 10
/* now paladin mounts */
int mount_vnums[] = {
  0,
  C_W_WARHORSE, //70, 1
  C_B_DESTRIER, //71, 2
  C_STALLION, //72, 3
  C_A_DESTRIER, //73, 4
  C_G_WARHORSE, //74, 5
  C_P_WARHORSE, //75, 6
  C_C_DESTRIER, //76, 7
  C_WARDOG, //77, 8
  C_WARPONY, //78, 9
  C_GRIFFON, //79, 10
  -1 /* end with this */
};
#define NUM_MOUNTS 10
/* now familiars */
int familiar_vnums[] = {
  0,
  F_HUNTER, //80, 1
  F_PANTHER, // 81, 2
  F_MOUSE, //82, 3
  F_EAGLE, //83, 4
  F_RAVEN, //84, 5
  F_IMP, //85, 6
  F_PIXIE, // 86, 7
  F_FAERIE_DRAGON, //87, 8
  F_PSEUDO_DRAGON, //88, 9
  F_HELLHOUND, //89, 10
  -1 /* end with this */
};
#define NUM_FAMILIARS 10

/* DEBUG:  just checking first 8 animals right now -zusuk */
#define TOP_OF_C  8
/****************/

/* make a list of names in order, first animals */
const char *animal_names[] = {
  "Unknown",
  "1) Black Bear",
  "2) Boar",
  "3) Lion",
  "4) Crocodile",
  "5) Hyena",
  "6) Snow Leopard",
  "7) Skull Spider",
  "8) Fire Beetle",
  "\n" /* end with this */
};
/* ... now mounts */
const char *mount_names[] = {
  "Unknown",
  "1) Heavy White Warhorse",
  "2) Black Destrier",
  "3) Stallion",
  "4) Armored Destrier",
  "5) Golden Warhorse",
  "6) Painted Warhorse",
  "7) Bright Destrier",
  "8) Wardog",
  "9) Warpony",
  "\n" /* end with this */
};
/* ... now familiars */
const char *familiar_names[] = {
  "Unknown",
  "1) Night Hunter",
  "2) Black Panther",
  "3) Tiny Mouse",
  "4) Eagle",
  "5) Raven",
  "6) Imp",
  "7) Pixie",
  "8) Faerie Dragon",
  "\n" /* end with this */
};

/* NOTE: The above static menus should be converted to dynamic menus.
 * All familiars/companions/etc. are not available to all characters.
 * Additionally, there could be some quest-based familiar/companion
 * choices that might have prerequisites that must be fullfilled prior
 * to their selection. */

/*-------------------------------------------------------------------*\
  utility functions
 \*-------------------------------------------------------------------*/

void init_study(struct descriptor_data *d, int class) {
  struct char_data *ch = d->character;
  int i = 0, j = 0;

  if (d->olc) {
    mudlog(BRF, LVL_IMMORT, TRUE,
            "SYSERR: do_study: Player already had olc structure.");
    free(d->olc);
  }

  CREATE(d->olc, struct oasis_olc_data, 1);

  if (LEVELUP(ch)) {
    mudlog(BRF, LVL_IMMORT, TRUE,
            "SYSERR: do_study: Player already had levelup structure.");
    free(LEVELUP(ch));
  }

  CREATE(LEVELUP(ch), struct level_data, 1);

  STATE(d) = CON_STUDY;

  /* Now copy the player's data to the levelup structure - a scratch area
   * used during the study process. */
  LEVELUP(ch)->class = class;
  LEVELUP(ch)->level = CLASS_LEVEL(ch, class);
  LEVELUP(ch)->feat_points  = GET_FEAT_POINTS(ch);
  LEVELUP(ch)->class_feat_points = GET_CLASS_FEATS(ch, class);
  LEVELUP(ch)->epic_feat_points = GET_EPIC_FEAT_POINTS(ch);
  LEVELUP(ch)->epic_class_feat_points = GET_EPIC_CLASS_FEATS(ch, class);
  LEVELUP(ch)->practices = GET_PRACTICES(ch);
  LEVELUP(ch)->trains = GET_TRAINS(ch);
  LEVELUP(ch)->num_boosts = GET_BOOSTS(ch);

  LEVELUP(ch)->str = GET_REAL_STR(ch);
  LEVELUP(ch)->dex = GET_REAL_DEX(ch);
  LEVELUP(ch)->con = GET_REAL_CON(ch);
  LEVELUP(ch)->inte = GET_REAL_INT(ch);
  LEVELUP(ch)->wis = GET_REAL_WIS(ch);
  LEVELUP(ch)->cha = GET_REAL_CHA(ch);

  //send_to_char(ch, "%d %d %d %d\r\n", LEVELUP(ch)->feat_points,
  //                                LEVELUP(ch)->class_feat_points,
  //                                LEVELUP(ch)->epic_feat_points,
  //                                LEVELUP(ch)->epic_class_feat_points);

  /* The following data elements are used to store the player's choices during the
   * study process - Just initialize these values. */
  LEVELUP(ch)->spell_circle = -1;
  LEVELUP(ch)->favored_slot = -1;
  LEVELUP(ch)->feat_type = -1;

  LEVELUP(ch)->tempFeat = 0;

  for (i = 0; i < 6; i++)
    LEVELUP(ch)->boosts[i] = 0;
  for (i = 0; i < NUM_FEATS; i++) {
    LEVELUP(ch)->feats[i] = 0;
    LEVELUP(ch)->feat_weapons[i] = 0;
    LEVELUP(ch)->feat_skills[i] = 0;
  }
  for (i = 0; i < NUM_CFEATS; i++)
    for (j = 0; j < FT_ARRAY_MAX; j++)
      LEVELUP(ch)->combat_feats[i][j] = 0;
  for (i = 0; i < MAX_ABILITIES; i++)
    for (j = 0; j < NUM_SKFEATS; j++)
      LEVELUP(ch)->skill_focus[i][j] = FALSE;
  for (i = 0; i < NUM_SFEATS; i ++)
    LEVELUP(ch)->school_feats[i] = 0;

  /* Finished with initialization - Now determine what the player can study. */

}

void finalize_study(struct descriptor_data *d) {

  struct char_data *ch = d->character;
  int i = 0, j = 0, subfeat = 0;
  struct damage_reduction_type *dr;

  /* Finalize the chosen data, applying the levelup structure to
   * the character structure. */
  GET_FEAT_POINTS(ch) = LEVELUP(ch)->feat_points;
  GET_CLASS_FEATS(ch, LEVELUP(ch)->class) = LEVELUP(ch)->class_feat_points;
  GET_EPIC_FEAT_POINTS(ch) = LEVELUP(ch)->epic_feat_points;
  GET_EPIC_CLASS_FEATS(ch, LEVELUP(ch)->class) = LEVELUP(ch)->epic_class_feat_points;
  GET_PRACTICES(ch) = LEVELUP(ch)->practices;
  GET_TRAINS(ch) = LEVELUP(ch)->trains;
  GET_BOOSTS(ch) = LEVELUP(ch)->num_boosts;

  GET_REAL_STR(ch)  = LEVELUP(ch)->str;
  GET_REAL_DEX(ch)  = LEVELUP(ch)->dex;
  GET_REAL_CON(ch)  = LEVELUP(ch)->con;
  GET_REAL_INT(ch)  = LEVELUP(ch)->inte;
  GET_REAL_WIS(ch)  = LEVELUP(ch)->wis;
  GET_REAL_CHA(ch)  = LEVELUP(ch)->cha;

/*
  for (i = 0; i < 6; i++)
    LEVELUP(ch)->boosts[i] = 0;
*/

  for (i = 0; i < NUM_FEATS; i++) {
    if (LEVELUP(ch)->feats[i]) {

      /* zusuk was here */
      SET_FEAT(ch, i, HAS_REAL_FEAT(ch, i) + LEVELUP(ch)->feats[i]);

      if ((subfeat = feat_to_skfeat(i)) != -1) {
        for (j = 0; j < MAX_ABILITIES + 1; j++)
          if (LEVELUP(ch)->skill_focus[j][subfeat])
            GET_SKILL_FEAT(ch, j, subfeat) = TRUE;
      }

      if ((subfeat = feat_to_cfeat(i)) != -1) {
        for (j = 0; j < NUM_WEAPON_TYPES; j++)
          if (HAS_LEVELUP_COMBAT_FEAT(ch, subfeat, j))
            SET_COMBAT_FEAT(ch, subfeat, j);
      }

      if ((subfeat = feat_to_sfeat(i)) != -1) {
        for (j = 1; j < NUM_SCHOOLS; j++)
          if (HAS_LEVELUP_SCHOOL_FEAT(ch, subfeat, j))
            SET_SCHOOL_FEAT(ch, subfeat, j);
      }

      /* Handle specific feats here: */
      switch (i) {
        case FEAT_MUMMY_DUST:
          send_to_char(ch, "\tMYou gained Epic Spell:  Mummy Dust!\tn\r\n");
          SET_SKILL(ch, SPELL_MUMMY_DUST, 99);
          break;
        case FEAT_DRAGON_KNIGHT:
          send_to_char(ch, "\tMYou gained Epic Spell:  Dragon Knight!\tn\r\n");
          SET_SKILL(ch, SPELL_DRAGON_KNIGHT, 99);
          break;
        case FEAT_GREATER_RUIN:
          send_to_char(ch, "\tMYou gained Epic Spell:  Greater Ruin!\tn\r\n");
          SET_SKILL(ch, SPELL_GREATER_RUIN, 99);
          break;
        case FEAT_HELLBALL:
          send_to_char(ch, "\tMYou gained Epic Spell:  Hellball!\tn\r\n");
          SET_SKILL(ch, SPELL_HELLBALL, 99);
          break;
        case FEAT_EPIC_MAGE_ARMOR:
          send_to_char(ch, "\tMYou gained Epic Spell:  Epic Mage Armor!\tn\r\n");
          SET_SKILL(ch, SPELL_EPIC_MAGE_ARMOR, 99);
          break;
        case FEAT_EPIC_WARDING:
          send_to_char(ch, "\tMYou gained Epic Spell:  Epic Warding!\tn\r\n");
          SET_SKILL(ch, SPELL_EPIC_WARDING, 99);
          break;
        case FEAT_GREAT_CHARISMA:
          GET_REAL_CHA(ch) += LEVELUP(ch)->feats[i];
          break;
        case FEAT_GREAT_CONSTITUTION:
          GET_REAL_CON(ch) += LEVELUP(ch)->feats[i];
          if (!(GET_REAL_CON(ch) % 2) && LEVELUP(ch)->feats[i]) {
            GET_REAL_MAX_HIT(ch) += GET_LEVEL(ch);
          }
          break;
        case FEAT_GREAT_DEXTERITY:
          GET_REAL_DEX(ch) += LEVELUP(ch)->feats[i];
          break;
        case FEAT_GREAT_INTELLIGENCE:
          GET_REAL_INT(ch) += LEVELUP(ch)->feats[i];
          break;
        case FEAT_GREAT_STRENGTH:
          GET_REAL_STR(ch) += LEVELUP(ch)->feats[i];
          break;
        case FEAT_GREAT_WISDOM:
          GET_REAL_WIS(ch) += LEVELUP(ch)->feats[i];
          break;
        case FEAT_TOUGHNESS:
          for (j = 0; j < GET_LEVEL(ch); j++)
            GET_REAL_MAX_HIT(ch) += LEVELUP(ch)->feats[i];
          break;
        case FEAT_EPIC_TOUGHNESS:
          for (j = 0; j < GET_LEVEL(ch); j++)
            GET_REAL_MAX_HIT(ch) += LEVELUP(ch)->feats[i];
          break;
        case FEAT_DAMAGE_REDUCTION:

          /* Create the DR structure and attach it to the player. */
          for (dr = GET_DR(ch); dr != NULL; dr = dr->next) {
            if (dr->feat == FEAT_DAMAGE_REDUCTION) {
              struct damage_reduction_type *temp;
              REMOVE_FROM_LIST(dr, GET_DR(ch), next);
            }
          }

          CREATE(dr, struct damage_reduction_type, 1);

          dr->spell = 0;
          dr->feat = FEAT_DAMAGE_REDUCTION;
          dr->amount = HAS_FEAT(ch, FEAT_DAMAGE_REDUCTION) + 1;
          dr->max_damage = -1;

          dr->bypass_cat[0] = DR_BYPASS_CAT_NONE;
          dr->bypass_val[0] = 0;
          dr->bypass_cat[1] = DR_BYPASS_CAT_UNUSED;
          dr->bypass_val[1] = 0; /* Unused. */
          dr->bypass_cat[2] = DR_BYPASS_CAT_UNUSED;
          dr->bypass_val[2] = 0; /* Unused. */

          dr->next = GET_DR(ch);
          GET_DR(ch) = dr;
          break;
      }
    }
  } /* for loop running through feats */

  /* set spells learned for domain */
  assign_domain_spells(ch);

  /* make sure to disable restricted school spells */
  disable_restricted_school_spells(ch);

  /* in case adding or changing clear domains, clean up and re-assign */
  clear_domain_feats(ch);
  add_domain_feats(ch);

  /* Set to learned. */
}

ACMD(do_study) {
  struct descriptor_data *d = ch->desc;

  init_study(d, GET_CLASS(ch));

  act("$n starts adjusting $s skill-set.",
          TRUE, d->character, 0, 0, TO_ROOM);
  SET_BIT_AR(PLR_FLAGS(ch), PLR_WRITING);

  display_main_menu(d);
}

#define FEAT_TYPE_NORMAL                1
#define FEAT_TYPE_NORMAL_CLASS          2
#define FEAT_TYPE_EPIC                  3
#define FEAT_TYPE_EPIC_CLASS            4

bool add_levelup_feat(struct descriptor_data *d, int feat) {

  struct char_data *ch = d->character;
  int feat_type = 0;

  if (has_feat(ch, feat) && !feat_list[feat].can_stack) {
    write_to_output(d, "You already have this feat.\r\n");
    return FALSE;
  }

  if (feat_list[feat].epic == TRUE) { /* This is an epic feat! */
    if (is_class_feat(feat, LEVELUP(ch)->class))
      feat_type = FEAT_TYPE_EPIC_CLASS;
    else
      feat_type = FEAT_TYPE_EPIC;
  } else {
    if (is_class_feat(feat, LEVELUP(ch)->class))
      feat_type = FEAT_TYPE_NORMAL_CLASS;
    else
      feat_type = FEAT_TYPE_NORMAL;
  }

  if ((feat_type == FEAT_TYPE_EPIC) && (LEVELUP(ch)->epic_feat_points < 1)) {
    write_to_output(d, "You do not have enough epic feat points to gain that feat.\r\n");
    return FALSE;
  }
  if ((feat_type == FEAT_TYPE_EPIC_CLASS) &&
      ((LEVELUP(ch)->epic_feat_points < 1) &&
       (LEVELUP(ch)->epic_class_feat_points < 1))) {
    write_to_output(d, "You do not have enough epic class feat points to gain that feat.\r\n");
    return FALSE;
  }
  if ((feat_type == FEAT_TYPE_NORMAL_CLASS) &&
      ( (LEVELUP(ch)->class_feat_points < 1) &&
        (LEVELUP(ch)->feat_points < 1) &&
        (LEVELUP(ch)->epic_class_feat_points < 1) &&
        (LEVELUP(ch)->epic_feat_points < 1) )) {
    write_to_output(d, "You do not have enough class feat points to gain that feat.\r\n");
    return FALSE;
  }
  if ((feat_type == FEAT_TYPE_NORMAL) && (LEVELUP(ch)->feat_points < 1) &&
      (LEVELUP(ch)->epic_feat_points < 1)) {
    write_to_output(d, "You do not have enough feat points to gain that feat.\r\n");
    return FALSE;
  }

  /* If we are here, then we can add the feat! */
  switch(feat_type) {
    case FEAT_TYPE_EPIC:
      LEVELUP(ch)->epic_feat_points--;
      break;
    case FEAT_TYPE_EPIC_CLASS:
      if (LEVELUP(ch)->epic_class_feat_points > 0)
        LEVELUP(ch)->epic_class_feat_points--;
      else
        LEVELUP(ch)->epic_feat_points--;
      break;
    case FEAT_TYPE_NORMAL_CLASS:
      if (LEVELUP(ch)->class_feat_points > 0)
        LEVELUP(ch)->class_feat_points--;
      else if (LEVELUP(ch)->feat_points > 0)
        LEVELUP(ch)->feat_points--;
      else if (LEVELUP(ch)->epic_class_feat_points > 0) {
        LEVELUP(ch)->epic_class_feat_points--;
        write_to_output(d, "You have used an epic class feat point to acquire a normal "
            "class feat, if you do not want to do this, exit out of the study menu "
                "without saving.\r\n");
      } else {
        LEVELUP(ch)->epic_feat_points--;
        write_to_output(d, "You have used an epic feat point to acquire a normal "
            "class feat, if you do not want to do this, exit out of the study menu "
                "without saving.\r\n");
      }
      break;
    case FEAT_TYPE_NORMAL:
      if (LEVELUP(ch)->feat_points > 0)
        LEVELUP(ch)->feat_points--;
      else {
        LEVELUP(ch)->epic_feat_points--;
        write_to_output(d, "You have used an epic feat point to acquire a normal "
            "feat, if you do not want to do this, exit out of the study menu "
                "without saving.\r\n");
      }
      break;
  }

  /* zusuk debug */
  //LEVELUP(ch)->feats[feat] += HAS_FEAT(ch, feat);
  /* zusuk debug */

  LEVELUP(ch)->feats[feat]++;
  return TRUE;
}

/*-------------------------------------------------------------------*/

/**************************************************************************
 Menu functions
 **************************************************************************/

/*-------------------------------------------------------------------*/

static void display_main_menu(struct descriptor_data *d) {
      generic_main_disp_menu(d);
}

static void sorc_known_spells_disp_menu(struct descriptor_data *d) {
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
                  "\r\n-- %sSpells Known Menu\r\n"
                  "\r\n"
                  "%s 1%s) 1st Circle     : %s%d\r\n"
                  "%s 2%s) 2nd Circle     : %s%d\r\n"
                  "%s 3%s) 3rd Circle     : %s%d\r\n"
                  "%s 4%s) 4th Circle     : %s%d\r\n"
                  "%s 5%s) 5th Circle     : %s%d\r\n"
                  "%s 6%s) 6th Circle     : %s%d\r\n"
                  "%s 7%s) 7th Circle     : %s%d\r\n"
                  "%s 8%s) 8th Circle     : %s%d\r\n"
                  "%s 9%s) 9th Circle     : %s%d\r\n"
                  "\r\n"
                  "%s Q%s) Quit\r\n"
                  "\r\n"
                  "Enter Choice : ",

                  mgn,
                  grn, nrm, yel, sorcererKnown[CLASS_LEVEL(d->character, CLASS_SORCERER)][0] -
                  count_sorc_known(d->character, 1, CLASS_SORCERER),
                  grn, nrm, yel, sorcererKnown[CLASS_LEVEL(d->character, CLASS_SORCERER)][1] -
                  count_sorc_known(d->character, 2, CLASS_SORCERER),
                  grn, nrm, yel, sorcererKnown[CLASS_LEVEL(d->character, CLASS_SORCERER)][2] -
                  count_sorc_known(d->character, 3, CLASS_SORCERER),
                  grn, nrm, yel, sorcererKnown[CLASS_LEVEL(d->character, CLASS_SORCERER)][3] -
                  count_sorc_known(d->character, 4, CLASS_SORCERER),
                  grn, nrm, yel, sorcererKnown[CLASS_LEVEL(d->character, CLASS_SORCERER)][4] -
                  count_sorc_known(d->character, 5, CLASS_SORCERER),
                  grn, nrm, yel, sorcererKnown[CLASS_LEVEL(d->character, CLASS_SORCERER)][5] -
                  count_sorc_known(d->character, 6, CLASS_SORCERER),
                  grn, nrm, yel, sorcererKnown[CLASS_LEVEL(d->character, CLASS_SORCERER)][6] -
                  count_sorc_known(d->character, 7, CLASS_SORCERER),
                  grn, nrm, yel, sorcererKnown[CLASS_LEVEL(d->character, CLASS_SORCERER)][7] -
                  count_sorc_known(d->character, 8, CLASS_SORCERER),
                  grn, nrm, yel, sorcererKnown[CLASS_LEVEL(d->character, CLASS_SORCERER)][8] -
                  count_sorc_known(d->character, 9, CLASS_SORCERER),
                  grn, nrm
                  );

  OLC_MODE(d) = STUDY_SORC_KNOWN_SPELLS_MENU;
}

void sorc_study_menu(struct descriptor_data *d, int circle) {
  int counter, columns = 0;

  LEVELUP(d->character)->spell_circle = circle;

  get_char_colors(d->character);
  clear_screen(d);

  for (counter = 1; counter < NUM_SPELLS; counter++) {
    if (spellCircle(CLASS_SORCERER, counter, DOMAIN_UNDEFINED) == circle) {
      if (sorcKnown(d->character, counter, CLASS_SORCERER))
        write_to_output(d, "%s%2d%s) %s%-20.20s %s", grn, counter, nrm, mgn,
              spell_info[counter].name, !(++columns % 3) ? "\r\n" : "");
      else
        write_to_output(d, "%s%2d%s) %s%-20.20s %s", grn, counter, nrm, yel,
              spell_info[counter].name, !(++columns % 3) ? "\r\n" : "");
    }
  }
  write_to_output(d, "\r\n");
  write_to_output(d, "%sNumber of slots availble:%s %d.\r\n", grn, nrm,
          sorcererKnown[CLASS_LEVEL(d->character, CLASS_SORCERER)][circle - 1] -
          count_sorc_known(d->character, circle, CLASS_SORCERER));
  write_to_output(d, "%sEnter spell choice, to add or remove "
          "(Q to exit to main menu) : ", nrm);

  OLC_MODE(d) = STUDY_SPELLS;
}

static void bard_known_spells_disp_menu(struct descriptor_data *d) {
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
          "\r\n-- %sSpells Known Menu\r\n"
          "\r\n"
          "%s 1%s) 1st Circle     : %s%d\r\n"
          "%s 2%s) 2nd Circle     : %s%d\r\n"
          "%s 3%s) 3rd Circle     : %s%d\r\n"
          "%s 4%s) 4th Circle     : %s%d\r\n"
          "%s 5%s) 5th Circle     : %s%d\r\n"
          "%s 6%s) 6th Circle     : %s%d\r\n"
          "\r\n"
          "%s Q%s) Quit\r\n"
          "\r\n"
          "Enter Choice : ",

          mgn,
          grn, nrm, yel, bardKnown[CLASS_LEVEL(d->character, CLASS_BARD)][0] -
          count_sorc_known(d->character, 1, CLASS_BARD),
          grn, nrm, yel, bardKnown[CLASS_LEVEL(d->character, CLASS_BARD)][1] -
          count_sorc_known(d->character, 2, CLASS_BARD),
          grn, nrm, yel, bardKnown[CLASS_LEVEL(d->character, CLASS_BARD)][2] -
          count_sorc_known(d->character, 3, CLASS_BARD),
          grn, nrm, yel, bardKnown[CLASS_LEVEL(d->character, CLASS_BARD)][3] -
          count_sorc_known(d->character, 4, CLASS_BARD),
          grn, nrm, yel, bardKnown[CLASS_LEVEL(d->character, CLASS_BARD)][4] -
          count_sorc_known(d->character, 5, CLASS_BARD),
          grn, nrm, yel, bardKnown[CLASS_LEVEL(d->character, CLASS_BARD)][5] -
          count_sorc_known(d->character, 6, CLASS_BARD),
          grn, nrm
          );

  OLC_MODE(d) = STUDY_BARD_KNOWN_SPELLS_MENU;
}

/* the menu for each circle, sorcerer */


void bard_study_menu(struct descriptor_data *d, int circle) {
  int counter, columns = 0;

  LEVELUP(d->character)->spell_circle = circle;

  get_char_colors(d->character);
  clear_screen(d);

  for (counter = 1; counter < NUM_SPELLS; counter++) {
    if (spellCircle(CLASS_BARD, counter, DOMAIN_UNDEFINED) == circle) {
      if (sorcKnown(d->character, counter, CLASS_BARD))
        write_to_output(d, "%s%2d%s) %s%-20.20s %s", grn, counter, nrm, mgn,
spell_info[counter].name, !(++columns % 3) ? "\r\n" : "");
      else
        write_to_output(d, "%s%2d%s) %s%-20.20s %s", grn, counter, nrm, yel,
              spell_info[counter].name, !(++columns % 3) ? "\r\n" : "");
    }
  }
  write_to_output(d, "\r\n");
  write_to_output(d, "%sNumber of slots availble:%s %d.\r\n", grn, nrm,
          bardKnown[CLASS_LEVEL(d->character, CLASS_BARD)][circle - 1] -
          count_sorc_known(d->character, circle, CLASS_BARD));
  write_to_output(d, "%sEnter spell choice, to add or remove "
          "(Q to exit to main menu) : ", nrm);

  OLC_MODE(d) = BARD_STUDY_SPELLS;
}

/***********************end bard******************************************/

static void favored_enemy_submenu(struct descriptor_data *d, int favored) {
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
          "\r\n-- %sRanger Favored Enemy Sub-Menu%s\r\n"
          "Slot:  %d\r\n"
          "\r\n"
          "%s"
          "\r\n"
          "Current Favored Enemy:  %s\r\n"
          "You can select 0 (Zero) to deselect an enemy for this slot.\r\n"
          "\r\n"
          "%s Q%s) Quit\r\n"
          "\r\n"
          "Enter Choice : ",

          mgn, nrm,
          favored,
          /* empty line */
          npc_race_menu,
          /* empty line */
          npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, favored)],
          grn, nrm
          /* empty line */
          );

  OLC_MODE(d) = FAVORED_ENEMY_SUB;
}

#define TOTAL_STAT_POINTS 30
#define MAX_POINTS_IN_A_STAT 10
#define BASE_STAT 8
int stat_cost_chart[11] = { /* cost for total points */
/*0  1  2  3  4  5  6  7  8   9   10 */
  0, 1, 2, 3, 4, 5, 6, 8, 10, 13, 16
};
int compute_base_dex(struct char_data *ch) {
  int base_dex = BASE_STAT;
  switch (GET_RACE(ch)) {
    case RACE_ELF:      base_dex += 2; break;
    case RACE_HALFLING: base_dex += 2; break;
    case RACE_HALF_TROLL:    base_dex += 2; break;
    case RACE_TRELUX:   base_dex += 8; break;
  }
  return base_dex;
}
int compute_dex_cost(struct char_data *ch, int number) {
  int base_dex = compute_base_dex(ch), current_dex = LEVELUP(ch)->dex + number;
  return stat_cost_chart[current_dex - base_dex];
}
int compute_base_str(struct char_data *ch) {
  int base_str = BASE_STAT;
  switch (GET_RACE(ch)) {
    case RACE_HALFLING:        base_str -= 2; break;
    case RACE_GNOME:           base_str -= 2; break;
    case RACE_HALF_TROLL:           base_str += 2; break;
    case RACE_CRYSTAL_DWARF:   base_str += 2; break;
    case RACE_TRELUX:          base_str += 2; break;
    case RACE_ARCANA_GOLEM:    base_str -= 2; break;
  }
  return base_str;
}
int compute_str_cost(struct char_data *ch, int number) {
  int base_str = compute_base_str(ch),
          current_str = LEVELUP(ch)->str + number;
  return stat_cost_chart[current_str - base_str];
}
int compute_base_con(struct char_data *ch) {
  int base_con = BASE_STAT;
  switch (GET_RACE(ch)) {
    case RACE_ELF:             base_con -= 2; break;
    case RACE_DWARF:           base_con += 2; break;
    case RACE_GNOME:           base_con += 2; break;
    case RACE_HALF_TROLL:           base_con += 2; break;
    case RACE_CRYSTAL_DWARF:   base_con += 8; break;
    case RACE_TRELUX:          base_con += 4; break;
    case RACE_ARCANA_GOLEM:    base_con -= 2; break;
  }
  return base_con;
}
int compute_con_cost(struct char_data *ch, int number) {
  int base_con = compute_base_con(ch), current_con = LEVELUP(ch)->con+number;
  return stat_cost_chart[current_con - base_con];
}
int compute_base_inte(struct char_data *ch) {
  int base_inte = BASE_STAT;
  switch (GET_RACE(ch)) {
    case RACE_HALF_TROLL:          base_inte -= 4; break;
    case RACE_ARCANA_GOLEM:   base_inte += 2; break;
  }
  return base_inte;
}
int compute_inte_cost(struct char_data *ch, int number) {
  int base_inte = compute_base_inte(ch), current_inte = LEVELUP(ch)->inte+number;
  return stat_cost_chart[current_inte - base_inte];
}
int compute_base_wis(struct char_data *ch) {
  int base_wis = BASE_STAT;
  switch (GET_RACE(ch)) {
    case RACE_HALF_TROLL:           base_wis -= 4; break;
    case RACE_CRYSTAL_DWARF:   base_wis += 2; break;
    case RACE_ARCANA_GOLEM:    base_wis += 2; break;
  }
  return base_wis;
}
int compute_wis_cost(struct char_data *ch, int number) {
  int base_wis = compute_base_wis(ch), current_wis = LEVELUP(ch)->wis+number;
  return stat_cost_chart[current_wis - base_wis];
}
int compute_base_cha(struct char_data *ch) {
  int base_cha = BASE_STAT;
  switch (GET_RACE(ch)) {
    case RACE_DWARF:            base_cha -= 2; break;
    case RACE_HALF_TROLL:            base_cha -= 4; break;
    case RACE_CRYSTAL_DWARF:    base_cha += 2; break;
    case RACE_ARCANA_GOLEM:     base_cha += 2; break;
  }
  return base_cha;
}
int compute_cha_cost(struct char_data *ch, int number) {
  int base_cha = compute_base_cha(ch), current_cha = LEVELUP(ch)->cha+number;
  return stat_cost_chart[current_cha - base_cha];
}

int compute_total_stat_points(struct char_data *ch) {
  return (compute_cha_cost(ch,0)+compute_wis_cost(ch,0)+compute_inte_cost(ch,0)+
          compute_str_cost(ch,0)+compute_dex_cost(ch,0)+compute_con_cost(ch,0));
}
int stat_points_left(struct char_data *ch) {
  return (TOTAL_STAT_POINTS-compute_total_stat_points(ch));
}

static void set_stats_menu(struct descriptor_data *d) {
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
          "\r\n-- %sSet Character Stats%s\r\n"
          "From this menu you can set your starting stats.  If you are completely\r\n"
          "unfamiliar with what type of stats you want, it is highly recommended\r\n"
          "to keep all values at or above 10, and spread out your points.  This choice\r\n"
          "will give you access to the most feats possible regardless of class.\r\n"
          "You will be required to use all your points before advancing your char.\r\n"
          "As you get more familiar with the feat system, there are opportunities\r\n"
          "to reset your characters stats to try a different configuration.\r\n"
          "Change made to base stat:   1  2  3  4  5  6  7  8   9   10\r\n"
          "Point cost              :   1  2  3  4  5  6  8  10  13  16\r\n"
          "\r\n"
          "%s 0%s) Strength:      %d%s\r\n"
          "%s 1%s) Dexterity:     %d%s\r\n"
          "%s 2%s) Constitution:  %d%s\r\n"
          "%s 3%s) Intelligence:  %d%s\r\n"
          "%s 4%s) Wisdom:        %d%s\r\n"
          "%s 5%s) Charisma:      %d%s\r\n"
          "%sPoints Left:         %d%s\r\n"
          "\r\n"
          "%s Q%s) Quit\r\n"
          "\r\n"
          "Enter Choice : ",

          mgn, nrm,
          /* empty line */
          grn, nrm, LEVELUP(d->character)->str, nrm,
          grn, nrm, LEVELUP(d->character)->dex, nrm,
          grn, nrm, LEVELUP(d->character)->con, nrm,
          grn, nrm, LEVELUP(d->character)->inte, nrm,
          grn, nrm, LEVELUP(d->character)->wis, nrm,
          grn, nrm, LEVELUP(d->character)->cha, nrm,
          grn, stat_points_left(d->character), nrm,
          /* empty line */
          grn, nrm
          /* empty line */
          );

  OLC_MODE(d) = STUDY_SET_STATS;
}

static void set_school_submenu(struct descriptor_data *d) {
  int i;

  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d, "\r\n");
  for (i = 0; i < NUM_SCHOOLS; i++) {
    write_to_output(d, "%d) %s\r\n", i, school_names[i]);
  }
  write_to_output(d, "\r\n");

  write_to_output(d, "\r\n%sEnter magical art to specialize in : ", nrm);
}
static void set_school_menu(struct descriptor_data *d) {
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
          "\r\n-- %sSet School of Magic Specialty%s\r\n"
          "\r\n"
          "%s 0%s) Specialty School:      %s%s\r\n"
          "%s    Restricted School:     %s%s\r\n"
          "\r\n"
          "%s Q%s) Quit\r\n"
          "\r\n"
          "Enter Choice : ",

          mgn, nrm,
          /* empty line */
          grn, nrm, school_names[GET_SPECIALTY_SCHOOL(d->character)], nrm,
          nrm, school_names[restricted_school_reference[GET_SPECIALTY_SCHOOL(d->character)]], nrm,
          /* empty line */
          grn, nrm
          /* empty line */
          );

  OLC_MODE(d) = STUDY_SET_SCHOOL;
}

static void set_domain_submenu(struct descriptor_data *d) {
  const char *domain_names[NUM_DOMAINS];
  int i;

  get_char_colors(d->character);
  clear_screen(d);

  for (i = 0; i < NUM_DOMAINS - 1; i++) {
    domain_names[i] = domain_list[i + 1].name;
  }

  column_list(d->character, 0, domain_names, NUM_DOMAINS - 1, TRUE);
  write_to_output(d, "\r\n%sEnter domain name selection : ", nrm);
}
static void set_domain_menu(struct descriptor_data *d) {
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
          "\r\n-- %sSet Domains%s\r\n"
          "\r\n"
          "%s 0%s) 1st Domain:      %s%s\r\n"
          "%s 1%s) 2nd Domain:      %s%s\r\n"
          "\r\n"
          "%s Q%s) Quit\r\n"
          "\r\n"
          "Enter Choice : ",

          mgn, nrm,
          /* empty line */
          grn, nrm, domain_list[GET_1ST_DOMAIN(d->character)].name, nrm,
          grn, nrm, domain_list[GET_2ND_DOMAIN(d->character)].name, nrm,
          /* empty line */
          grn, nrm
          /* empty line */
          );

  OLC_MODE(d) = STUDY_SET_DOMAINS;
}

void print_school_info(struct descriptor_data *d, int school_number) {

  write_to_output(d, "\r\n");
  write_to_output(d, "%s\r\n", school_benefits[school_number]);
  write_to_output(d, "\r\n");

}

void print_domain_info(struct descriptor_data *d, int domain_number) {
  int j = 0;

  write_to_output(d, "%sDomain:%s %-20s %sFavored Weapon:%s %-22s\r\n%sDescription:%s %s\r\n",
                  cyn, nrm, domain_list[domain_number].name,
                  cyn, nrm, weapon_list[domain_list[domain_number].favored_weapon].name,
                  cyn, nrm, domain_list[domain_number].description
                  );

  write_to_output(d, "%sGranted powers: |%s", cyn, nrm);
  for (j = 0; j < MAX_GRANTED_POWERS; j++) {
    if (domain_list[domain_number].granted_powers[j] != DOMAIN_POWER_UNDEFINED) {
      write_to_output(d, "%s%s|%s", domainpower_names[domain_list[domain_number].granted_powers[j]], cyn, nrm);
    }
  }
  write_to_output(d, "\r\n");

  write_to_output(d, "%sGranted spells: |%s", cyn, nrm);
  for (j = 0; j < MAX_DOMAIN_SPELLS; j++) {
    if (domain_list[domain_number].domain_spells[j] != SPELL_RESERVED_DBC) {
      write_to_output(d, "%s%s|%s", spell_info[domain_list[domain_number].domain_spells[j]].name, cyn, nrm);
    }
  }

}


static void favored_enemy_menu(struct descriptor_data *d) {
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
          "\r\n-- %sRanger Favored Enemy Menu%s\r\n"
          "\r\n"
          "%s 0%s) Favored Enemy #1  (Min. Level  1):  %s%s\r\n"
          "%s 1%s) Favored Enemy #2  (Min. Level  5):  %s%s\r\n"
          "%s 2%s) Favored Enemy #3  (Min. Level 10):  %s%s\r\n"
          "%s 3%s) Favored Enemy #4  (Min. Level 15):  %s%s\r\n"
          "%s 4%s) Favored Enemy #5  (Min. Level 20):  %s%s\r\n"
          "%s 5%s) Favored Enemy #6  (Min. Level 25):  %s%s\r\n"
          "%s 6%s) Favored Enemy #7  (Min. Level 30):  %s%s\r\n"
          "%s 7%s) Favored Enemy #8  (Min. Level xx):  %s%s\r\n"
          "%s 8%s) Favored Enemy #9  (Min. Level xx):  %s%s\r\n"
          "%s 9%s) Favored Enemy #10 (Min. Level xx):  %s%s\r\n"
          "\r\n"
          "%s Q%s) Quit\r\n"
          "\r\n"
          "Enter Choice : ",

          mgn, nrm,
          /* empty line */
          grn, nrm, npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, 0)], nrm,
          grn, nrm, npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, 1)], nrm,
          grn, nrm, npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, 2)], nrm,
          grn, nrm, npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, 3)], nrm,
          grn, nrm, npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, 4)], nrm,
          grn, nrm, npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, 5)], nrm,
          grn, nrm, npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, 6)], nrm,
          grn, nrm, npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, 7)], nrm,
          grn, nrm, npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, 8)], nrm,
          grn, nrm, npc_race_abbrevs[GET_FAVORED_ENEMY(d->character, 9)], nrm,
          /* empty line */
          grn, nrm
          /* empty line */
          );

  OLC_MODE(d) = FAVORED_ENEMY;
}

static void animal_companion_menu(struct descriptor_data *d) {
  int i = 1, found = 0;

  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
          "\r\n-- %sDruid/Ranger Animal Companion Menu%s\r\n"
          "\r\n", mgn, nrm);

  for (i = 1; i <= TOP_OF_C; i++) {
    write_to_output(d, "%s\r\n", animal_names[i]);
  }

  write_to_output(d, "\r\n");
  /* find current animal */
  for (i = 1; i <= TOP_OF_C; i++) {
    if (GET_ANIMAL_COMPANION(d->character) == animal_vnums[i]) {
      write_to_output(d, "Current Companion:  %s\r\n", animal_names[i]);
      found = 1;
      break;
    }
  }

  if (!found)
    write_to_output(d, "Currently No Companion Selected\r\n");

  write_to_output(d, "You can select 0 (Zero) to deselect the current "
          "companion.\r\n");
  write_to_output(d, "\r\n"
          "%s Q%s) Quit\r\n"
          "\r\n"
          "Enter Choice : ",
          grn, nrm
          );

  OLC_MODE(d) = ANIMAL_COMPANION;
}

static void familiar_menu(struct descriptor_data *d) {
  int i = 1, found = 0;

  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
          "\r\n-- %sFamiliar Menu%s\r\n"
          "\r\n", mgn, nrm);

  for (i = 1; i <= TOP_OF_C; i++) {
    write_to_output(d, "%s\r\n", familiar_names[i]);
  }

  write_to_output(d, "\r\n");

  /* find current familiar */
  for (i = 1; i <= TOP_OF_C; i++) {
    if (GET_FAMILIAR(d->character) == familiar_vnums[i]) {
      write_to_output(d, "Current Familiar:  %s\r\n", familiar_names[i]);
      found = 1;
      break;
    }
  }

  if (!found)
    write_to_output(d, "Currently No Familiar Selected\r\n");

  write_to_output(d, "You can select 0 (Zero) to deselect the current "
          "familiar.\r\n");
  write_to_output(d, "\r\n"
          "%s Q%s) Quit\r\n"
          "\r\n"
          "Enter Choice : ",
          grn, nrm
          );

  OLC_MODE(d) = FAMILIAR_MENU;
}

/* Helper function for the below menu */
bool can_study_feat_type(struct char_data *ch, int feat_type) {
  int i = 0;
  bool result = FALSE;

  if(!CAN_STUDY_FEATS(ch))
    return FALSE;

  /*  Feat types divide the huge mess of feats into categories for the purpose
   *  of making the learning process easier. */

  for (i = 0; i < NUM_FEATS; i++) {
    if(feat_list[i].in_game &&
       (feat_list[i].feat_type == feat_type)) {
      if(feat_is_available(ch, i, 0, NULL))
        result = TRUE;
    }
  }
  return result;
}

static void main_feat_disp_menu(struct descriptor_data *d) {
  int i;
  bool can_study = FALSE;
  struct char_data *ch = d->character;

  get_char_colors(ch);
  clear_screen(d);

  LEVELUP(ch)->feat_type = -1;

  write_to_output(d,
          "\r\n-- %sFeat Menu\r\n"
          "\r\n", mgn);
  for (i = 1; i < NUM_LEARNABLE_FEAT_TYPES; i++) {
    can_study = can_study_feat_type(ch, i);
    write_to_output(d,
          "%s %d%s) %s Feats\r\n",
          (can_study ? grn : "\tD"), i, (can_study ? nrm : "\tD"), feat_types[i]);
  }
  write_to_output(d,
          "\r\n"
          "%s Q%s) Quit\r\n"
          "\r\n"
          "Enter Choice : ",
          grn, nrm
          );


  OLC_MODE(d) = STUDY_MAIN_FEAT_MENU;
}

static void display_study_feats(struct descriptor_data *d) {

  struct char_data *ch = d->character;
  int i = 0, j = 0, feat_marker = 0, feat_counter = 0, sortpos = 0;
  int count = 0;
  bool class_feat = FALSE;

  for (sortpos = 1; sortpos < NUM_FEATS; sortpos++) {

    i = feat_sort_info[sortpos];
    feat_marker = 1;
    feat_counter = 0;
    class_feat = FALSE;

    while (feat_marker != 0) {
      feat_marker = class_bonus_feats[LEVELUP(ch)->class][feat_counter];
      if (feat_marker == i) {
        class_feat = TRUE;
      }
      feat_counter++;
    }

    j = 0;

    if (((feat_list[i].feat_type == LEVELUP(ch)->feat_type) &&
        feat_is_available(ch, i, 0, NULL) &&
        feat_list[i].in_game &&
        feat_list[i].can_learn &&
        (!has_feat(ch, i) || feat_list[i].can_stack))) {

      write_to_output(d,
                      "%s%s%3d%s) %-30s%s",
                      (class_feat ? (feat_list[i].epic ? "\tM(EC)" : "\tC (C)") : (feat_list[i].epic ? "\tM (E)" : "    ")),
                      grn, i, nrm, feat_list[i].name, nrm);
      count++;

      if (count % 2 == 0)
        write_to_output(d, "\r\n");
    }
  }

  if (count %2 != 0)
    write_to_output(d, "\r\n");

  write_to_output(d, "\r\n");

  write_to_output(d,
                  "To select a feat, type the number beside it.  Class feats are in \tCcyan\tn and marked with a (C).\r\n"
                  "Epic feats, both class and regular, are in \tMMagenta\tn and are marked with (EC) or (E).\r\n");
  write_to_output(d, "Feat Points: General (%s%d%s) Class (%s%d%s) Epic (%s%d%s) Epic Class (%s%d%s)\r\n",
                        (LEVELUP(ch)->feat_points > 0 ? grn : red), LEVELUP(ch)->feat_points, nrm,
                        (LEVELUP(ch)->class_feat_points > 0 ? grn : red), LEVELUP(ch)->class_feat_points, nrm,
                        (LEVELUP(ch)->epic_feat_points > 0 ? grn : red), LEVELUP(ch)->epic_feat_points, nrm,
                        (LEVELUP(ch)->epic_class_feat_points > 0 ? grn : red), LEVELUP(ch)->epic_class_feat_points, nrm);

  write_to_output(d, "Your choice? (type -1 to exit) : ");

}

static void gen_feat_disp_menu(struct descriptor_data *d) {
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
          "\r\n-- %sFeats%s\r\n"
          "\r\n",
          mgn, nrm);

  display_study_feats(d);


  OLC_MODE(d) = STUDY_GEN_FEAT_MENU;
}

static void generic_main_disp_menu(struct descriptor_data *d) {
  struct char_data *ch = d->character;
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
          "\r\n-- %sStudy Menu\r\n"
          "\r\n"
          "%s 1%s) Feats\r\n"
          "%s 2%s) Known Spells\r\n"
          "%s 3%s) Choose Familiar\r\n"
          "%s 4%s) Animal Companion\r\n"
          "%s 5%s) Ranger Favored Enemy\r\n"
          "%s 6%s) Set Stats\r\n"
          "%s 7%s) Cleric Domain Selection\r\n"
          "%s 8%s) Wizard School Selection\r\n"
          "\r\n"
          "%s Q%s) Quit\r\n"
          "\r\n"
          "Enter Choice : ",

          mgn,
          MENU_OPT(CAN_STUDY_FEATS(ch)),
          MENU_OPT(CAN_STUDY_KNOWN_SPELLS(ch)),
          MENU_OPT(CAN_STUDY_FAMILIAR(ch)),
          MENU_OPT(CAN_STUDY_COMPANION(ch)),
          MENU_OPT(CAN_STUDY_FAVORED_ENEMY(ch)),
          MENU_OPT(CAN_SET_STATS(ch)),
          MENU_OPT(CAN_SET_DOMAIN(ch)),
          MENU_OPT(CAN_SET_SCHOOL(ch)),
          grn, nrm
          );

  OLC_MODE(d) = STUDY_GEN_MAIN_MENU;
}

/*  This does not work for all cfeats -exotc weapon proficiency is a special
 *  case and needs special handling. */
static void cfeat_disp_menu(struct descriptor_data *d) {
  const char *feat_weapons[NUM_WEAPON_TYPES - 1];
  int i = 0;

  get_char_colors(d->character);
  clear_screen(d);

  /* we want to use column_list here, but we don't have a pre made list
   * of string values (without undefined).  Make one, and make sure it is in order. */
  for (i = 0; i < NUM_WEAPON_TYPES - 1 ; i++) {
    feat_weapons[i] = weapon_list[i + 1].name;
  }

  column_list(d->character, 3, feat_weapons, NUM_WEAPON_TYPES - 1, TRUE);

  write_to_output(d, "\r\n%sChoose weapon type for the %s feat : ", nrm, feat_list[LEVELUP(d->character)->tempFeat].name);

  OLC_MODE(d) = STUDY_CFEAT_MENU;
}

static void sfeat_disp_menu(struct descriptor_data *d) {
  int i = 0;

  get_char_colors(d->character);
  clear_screen(d);

  for(i = 1; i < NUM_SCHOOLS; i++)
    write_to_output(d, "%d) %s\r\n", i, spell_schools[i]);

  write_to_output(d, "\r\n%sChoose a spell school for the %s feat : ", nrm, feat_list[LEVELUP(d->character)->tempFeat].name);

  OLC_MODE(d) = STUDY_SFEAT_MENU;
}

static void skfeat_disp_menu(struct descriptor_data *d) {
  int i = 0;

  get_char_colors(d->character);
  clear_screen(d);

  for(i = 1; i < END_GENERAL_ABILITIES + 1; i++)
    write_to_output(d, "%d) %s\r\n", i, ability_names[i]);

  write_to_output(d, "\r\n%sChoose a skill for the %s feat : ", nrm,
                  feat_list[LEVELUP(d->character)->tempFeat].name);

  OLC_MODE(d) = STUDY_SKFEAT_MENU;
}

/**************************************************************************
  The handler
 **************************************************************************/

/* we add to add this function to make sure training points are properly
 calculated based on stat changes */
void reset_training_points(struct char_data *ch) {
  int i = 0, trains = 0, int_bonus = 0;

  /* first reset all trained abilities */  
  for (i = 1; i <= NUM_ABILITIES; i++)
    SET_ABILITY(ch, i, 0);
    
  /* determine intelligence bonus */
  int_bonus = (int) ((LEVELUP(ch)->inte - 10) / 2);

  /* use class to establish base skill points */
  switch (GET_CLASS(ch)) {
    case CLASS_DRUID:
    case CLASS_RANGER:
    case CLASS_BERSERKER:
    case CLASS_MONK:
      trains = (4 + int_bonus) * 4;
      break;
    case CLASS_BARD:
      trains = (6 + int_bonus) * 4;
      break;
    case CLASS_ROGUE:
      trains = (8 + int_bonus) * 4;
      break;    
    case CLASS_WARRIOR:
    case CLASS_WEAPON_MASTER:
    case CLASS_WIZARD:
    case CLASS_CLERIC:
    case CLASS_UNDEFINED:
    case CLASS_SORCERER:
    case CLASS_PALADIN:
    default:
      trains = (2 + int_bonus) * 4;
      break;
  }
  
  /* minimum value for trains */
  if (trains < 4)
    trains = 4;
  
  /* human bonus */
  trains += 4;
  
  /* finalize */
  LEVELUP(ch)->trains = trains;  
}

void study_parse(struct descriptor_data *d, char *arg) {
  struct char_data *ch = d->character;
  int number = -1;
  int counter;
  int points_left = 0, cost_for_number = 0, new_stat = 0;

  switch (OLC_MODE(d)) {
    case STUDY_CONFIRM_SAVE:
      switch (*arg) {
        case 'y':
        case 'Y':
          /* Save the temporary values in LEVELUP(d->character) to the
           * character, print a message, free the structures and exit. */
          write_to_output(d, "Your choices have been finalized!\r\n\r\n");
          finalize_study(d);
          save_char(d->character, 0);
          cleanup_olc(d, CLEANUP_ALL);
          free(LEVELUP(d->character));
          LEVELUP(d->character) = NULL;
          break;
        case 'n':
        case 'N':
          /* Discard the changes and exit. */
          write_to_output(d, "Your choices were NOT finalized.\r\n\r\n");
          cleanup_olc(d, CLEANUP_ALL);
          free(LEVELUP(d->character));
          LEVELUP(d->character) = NULL;
          break;
        default:
          write_to_output(d, "Invalid choice!\r\nDo you wish to save your changes ? : ");
          break;
      }
      return;

    case STUDY_GEN_MAIN_MENU:
      switch (*arg) {
        case 'q':
        case 'Q':
          if (GET_LEVEL(ch) == 1) {            
            write_to_output(d, "Your training points will be reset upon exit to "
                "account for any changes made to stats. (This will only occur at "
                "level 1)\r\n");
            reset_training_points(ch);
          }
          write_to_output(d, "You can currently study as much as you want per level.\r\n");
          /* in the future we will probably change study to be limited to 1/ level ?*/
          write_to_output(d, "Do you wish to save your changes? : ");
          OLC_MODE(d) = STUDY_CONFIRM_SAVE;
          break;
        case '1':
          main_feat_disp_menu(d);
          break;
        case '2':
          if (CAN_STUDY_KNOWN_SPELLS(ch)) {
            if (LEVELUP(ch)->class == CLASS_SORCERER)
              sorc_known_spells_disp_menu(d);
            if (LEVELUP(ch)->class == CLASS_BARD)
              bard_known_spells_disp_menu(d);
          }    else {
            write_to_output(d, "That is an invalid choice!\r\n");
            generic_main_disp_menu(d);
          }
          break;
        case '3':
          if (CAN_STUDY_FAMILIAR(ch))
            familiar_menu(d);
          else {
            write_to_output(d, "That is an invalid choice!\r\n");
            generic_main_disp_menu(d);
          }
          break;
        case '4':
          if (CAN_STUDY_COMPANION(ch))
            animal_companion_menu(d);
          else {
            write_to_output(d, "That is an invalid choice!\r\n");
            generic_main_disp_menu(d);
          }
          break;
        case '5':
          if (CAN_STUDY_FAVORED_ENEMY(ch))
            favored_enemy_menu(d);
          else {
            write_to_output(d, "That is an invalid choice!\r\n");
            generic_main_disp_menu(d);
          }
          break;
        case '6':
          if (CAN_SET_STATS(ch))
            set_stats_menu(d);
          else {
            write_to_output(d, "That is an invalid choice!\r\n");
            generic_main_disp_menu(d);
          }
          break;
        case '7':
          if (CAN_SET_DOMAIN(ch))
            set_domain_menu(d);
          else {
            write_to_output(d, "That is an invalid choice!\r\n");
            generic_main_disp_menu(d);
          }
          break;
        case '8':
          if (CAN_SET_SCHOOL(ch))
            set_school_menu(d);
          else {
            write_to_output(d, "That is an invalid choice!\r\n");
            generic_main_disp_menu(d);
          }
          break;
        default:
          write_to_output(d, "That is an invalid choice!\r\n");
          display_main_menu(d);
          break;

      }
      break;
    case STUDY_MAIN_FEAT_MENU:
      /* This is the menu where the player chooses feats - This menu is actually a
       * 'master menu' that drives the process. */
      switch (*arg) {
        case 'q':
        case 'Q':
          display_main_menu(d);
          break;
        default: /* Choose Feats */
         number = atoi(arg);
          if(!CAN_STUDY_FEATS(ch) || (number < 1) || (number >= NUM_LEARNABLE_FEAT_TYPES) || !can_study_feat_type(ch, number)) {
           write_to_output(d, "That is an invalid choice!\r\n");
            main_feat_disp_menu(d);
            break;
          }
          LEVELUP(d->character)->feat_type = number;
          gen_feat_disp_menu(d);
          break;
      }
      break;

    case STUDY_GEN_FEAT_MENU:
      number = atoi(arg);
      if (number == -1) {
        main_feat_disp_menu(d);
        break;
      }

      /* Check if the feat is available. */
      if ((number < 1) ||
          (number >= NUM_FEATS) ||
          (!feat_is_available(d->character, number, 0, NULL))) {
        write_to_output(d, "Invalid feat, try again.\r\n");
        gen_feat_disp_menu(d);
        break;
      }

      /* Store the feat number in the work area in the data structure. */
      LEVELUP(d->character)->tempFeat = number;

     /* Display the description of the feat, and give the player an option. */
      write_to_output(d, "%s%s%s: %s\r\n\r\n"
                         "Choose this feat? (y/n) : ",
                         nrm, feat_list[LEVELUP(ch)->tempFeat].name, nrm, feat_list[LEVELUP(ch)->tempFeat].description);

      OLC_MODE(d) = STUDY_CONFIRM_ADD_FEAT;
      break;

    case STUDY_CONFIRM_ADD_FEAT:
      switch(*arg) {
        case 'n':
        case 'N':
          gen_feat_disp_menu(d);
          break;
        case 'y':
        case 'Y':
          /* Check to see if this feat has a subfeat - If so, then display the
           * approptiate menus. */
          if(feat_to_cfeat(LEVELUP(ch)->tempFeat) != -1) {
            /* Combat feat - Need to choose weapon type. */
            cfeat_disp_menu(d);
            break;
          }
          if(feat_to_sfeat(LEVELUP(ch)->tempFeat) != -1) {
            /* Spell school feat - Need to choose spell school. */
            sfeat_disp_menu(d);
            break;
          }
          if(feat_to_skfeat(LEVELUP(ch)->tempFeat) != -1) {
            /* Skill feat - Need to choose a skill (ability) */
            skfeat_disp_menu(d);
            break;
          }

          if (add_levelup_feat(d, LEVELUP(ch)->tempFeat))
            write_to_output(d, "Feat %s chosen!\r\n", feat_list[LEVELUP(ch)->tempFeat].name);
          gen_feat_disp_menu(d);
          break;
      }
      break;
    /* Combat feats require the selection of a weapon type. */
    case STUDY_CFEAT_MENU:
      number = atoi(arg);
      if (number == -1) {
        LEVELUP(d->character)->tempFeat = -1;
        gen_feat_disp_menu(d);
        break;
      }
      if ((number < 1) || (number >= NUM_WEAPON_TYPES)) {
        write_to_output(d, "That is an invalid choice!\r\n");
        cfeat_disp_menu(d);
        break;
      }
      if(HAS_COMBAT_FEAT(ch, feat_to_cfeat(LEVELUP(d->character)->tempFeat), number) ||
         HAS_LEVELUP_COMBAT_FEAT(ch, feat_to_cfeat(LEVELUP(ch)->tempFeat), number)) {
        write_to_output(d, "You already have that weapon type selected for this feat!\r\n\r\n");
        cfeat_disp_menu(d);
        break;
      }
      /* Now we have the weapon type - set it in the structure. */
      if (add_levelup_feat(d, LEVELUP(d->character)->tempFeat)) {
        SET_LEVELUP_COMBAT_FEAT(d->character, feat_to_cfeat(LEVELUP(d->character)->tempFeat), number);

        write_to_output(d, "Feat %s (%s) chosen!\r\n", feat_list[LEVELUP(d->character)->tempFeat].name, weapon_list[number].name);

      } else {
        LEVELUP(d->character)->tempFeat = -1;
      }
      gen_feat_disp_menu(d);

      break;

    /* School feats require the selection of a spell school. */
    case STUDY_SFEAT_MENU:
      number = atoi(arg);
      if (number == -1) {
        LEVELUP(d->character)->tempFeat = -1;
        gen_feat_disp_menu(d);
        break;
      }
      if ((number < 1) || (number >= NUM_SCHOOLS)) {
        write_to_output(d, "That is an invalid choice!\r\n");
        sfeat_disp_menu(d);
        break;
      }
      if(HAS_SCHOOL_FEAT(ch, feat_to_sfeat(LEVELUP(d->character)->tempFeat), number) ||
         HAS_LEVELUP_SCHOOL_FEAT(ch, feat_to_sfeat(LEVELUP(d->character)->tempFeat), number)) {
        write_to_output(d, "You already have that school selected for this feat!\r\n\r\n");
        sfeat_disp_menu(d);
        break;
      }

      /* Now we have the spell school. */
      if (add_levelup_feat(d, LEVELUP(d->character)->tempFeat)) {
        SET_LEVELUP_SCHOOL_FEAT(d->character, feat_to_sfeat(LEVELUP(d->character)->tempFeat), number);

        write_to_output(d, "Feat %s (%s) chosen!\r\n", feat_list[LEVELUP(d->character)->tempFeat].name, spell_schools[number]);
      } else {
        LEVELUP(d->character)->tempFeat = -1;
      }
      gen_feat_disp_menu(d);
      break;

    /* Skill feats require the selection of a skill. */
    case STUDY_SKFEAT_MENU:
      number = atoi(arg);
      if (number == -1) {
        LEVELUP(d->character)->tempFeat = -1;
        gen_feat_disp_menu(d);
        break;
      }
      if ((number < 1) || (number > END_GENERAL_ABILITIES)) {
        write_to_output(d, "That is an invalid choice!\r\n");
        skfeat_disp_menu(d);
        break;
      }
      if(HAS_SKILL_FEAT(ch, number, feat_to_skfeat(LEVELUP(d->character)->tempFeat)) ||
         HAS_LEVELUP_SKILL_FEAT(ch, number, feat_to_skfeat(LEVELUP(ch)->tempFeat))) {
        write_to_output(d, "You already have that skill selected for this feat!\r\n\r\n");
        skfeat_disp_menu(d);
        break;
      }
      /* Now we have the skill - set it in the structure. */
      if (add_levelup_feat(d, LEVELUP(d->character)->tempFeat)) {
        SET_LEVELUP_SKILL_FEAT(d->character, number, feat_to_skfeat(LEVELUP(d->character)->tempFeat));

        write_to_output(d, "Feat %s (%s) chosen!\r\n", feat_list[LEVELUP(d->character)->tempFeat].name, ability_names[number]);

      } else {
        LEVELUP(d->character)->tempFeat = -1;
      }
      gen_feat_disp_menu(d);
      break;

    /******* start sorcerer **********/
    case STUDY_SORC_KNOWN_SPELLS_MENU:
      switch (*arg) {
        case 'q':
        case 'Q':
          display_main_menu(d);
          break;
          /* here are our spell levels for 'spells known' */
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          sorc_study_menu(d, atoi(arg));
          OLC_MODE(d) = STUDY_SPELLS;
          break;
        default:
          write_to_output(d, "That is an invalid choice!\r\n");
          sorc_known_spells_disp_menu(d);
          break;
      }
      break;

    case STUDY_SPELLS:
      switch (*arg) {
        case 'q':
        case 'Q':
          sorc_known_spells_disp_menu(d);
          break;

        default:
          number = atoi(arg);

          for (counter = 1; counter < NUM_SPELLS; counter++) {
            if (counter == number) {
              if (spellCircle(CLASS_SORCERER, counter, DOMAIN_UNDEFINED) == LEVELUP(d->character)->spell_circle) {
                if (sorcKnown(d->character, counter, CLASS_SORCERER))
                  sorc_extract_known(d->character, counter, CLASS_SORCERER);
                else if (!sorc_add_known(d->character, counter, CLASS_SORCERER))
                  write_to_output(d, "You are all FULL for spells!\r\n");
              }
            }
          }
          sorc_study_menu(d, LEVELUP(d->character)->spell_circle);
          break;
      }
      break;
      /******* end sorcerer **********/


      /******* start bard **********/

    case STUDY_BARD_KNOWN_SPELLS_MENU:
      switch (*arg) {
        case 'q':
        case 'Q':
          display_main_menu(d);
          break;

          /* here are our spell levels for 'spells known' */
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
          bard_study_menu(d, atoi(arg));
          break;
        default:
          write_to_output(d, "That is an invalid choice!\r\n");
          bard_known_spells_disp_menu(d);
          break;
      }
      break;

    case BARD_STUDY_SPELLS:
      switch (*arg) {
        case 'q':
        case 'Q':
          bard_known_spells_disp_menu(d);
          break;

        default:
          number = atoi(arg);

          for (counter = 1; counter < NUM_SPELLS; counter++) {
            if (counter == number) {
              if (spellCircle(CLASS_BARD, counter, DOMAIN_UNDEFINED) == LEVELUP(d->character)->spell_circle) {
                if (sorcKnown(d->character, counter, CLASS_BARD))
                  sorc_extract_known(d->character, counter, CLASS_BARD);
                else if (!sorc_add_known(d->character, counter, CLASS_BARD))
                  write_to_output(d, "You are all FULL for spells!\r\n");
              }
            }
          }
          bard_study_menu(d, LEVELUP(d->character)->spell_circle);
          break;
      }
      break;
    /******* end bard **********/

    case SET_1ST_DOMAIN:
      number = atoi(arg);
      if (number < 0) {
        write_to_output(d, "Invalid value!  Try again.\r\n");
        OLC_MODE(d) = SET_1ST_DOMAIN;
        set_domain_submenu(d);
        return;
      }
      if (number >= NUM_DOMAINS) {
        write_to_output(d, "Invalid value!  Try again.\r\n");
        OLC_MODE(d) = SET_1ST_DOMAIN;
        set_domain_submenu(d);
        return;
      }
      if (GET_2ND_DOMAIN(ch) == number) {
        write_to_output(d, "You already have that domain!  Try again.\r\n");
        OLC_MODE(d) = SET_1ST_DOMAIN;
        set_domain_submenu(d);
        return;
      }
      GET_1ST_DOMAIN(ch) = number;
      write_to_output(d, "Choice selected.\r\n");
      print_domain_info(d, number);
      OLC_MODE(d) = STUDY_SET_DOMAINS;
      set_domain_menu(d);
      break;
    case SET_2ND_DOMAIN:
      number = atoi(arg);
      if (number < 0) {
        write_to_output(d, "Invalid value!  Try again.\r\n");
        OLC_MODE(d) = SET_2ND_DOMAIN;
        set_domain_submenu(d);
        return;
      }
      if (number >= NUM_DOMAINS) {
        write_to_output(d, "Invalid value!  Try again.\r\n");
        OLC_MODE(d) = SET_2ND_DOMAIN;
        set_domain_submenu(d);
        return;
      }
      if (GET_1ST_DOMAIN(ch) == number) {
        write_to_output(d, "You already have that domain!  Try again.\r\n");
        OLC_MODE(d) = SET_2ND_DOMAIN;
        set_domain_submenu(d);
        return;
      }
      GET_2ND_DOMAIN(ch) = number;
      write_to_output(d, "Choice selected.\r\n");
      print_domain_info(d, number);
      OLC_MODE(d) = STUDY_SET_DOMAINS;
      set_domain_menu(d);
      break;
    case STUDY_SET_DOMAINS:
      switch (*arg) {
        case 'q':
        case 'Q':
          display_main_menu(d);
          break;

        default:
          number = atoi(arg);
          switch (number) {
            case 0:
              set_domain_submenu(d);
              OLC_MODE(d) = SET_1ST_DOMAIN;
              return;
            case 1:
              set_domain_submenu(d);
              OLC_MODE(d) = SET_2ND_DOMAIN;
              return;
            default: break;
          }
          OLC_MODE(d) = STUDY_SET_DOMAINS;
          set_domain_menu(d);
          break;
      }
      break;

    case SET_SCHOOL:
      number = atoi(arg);
      if (number < 0) {
        write_to_output(d, "Invalid value!  Try again.\r\n");
        OLC_MODE(d) = SET_SCHOOL;
        set_school_submenu(d);
        return;
      }
      if (number >= NUM_SCHOOLS) {
        write_to_output(d, "Invalid value!  Try again.\r\n");
        OLC_MODE(d) = SET_SCHOOL;
        set_school_submenu(d);
        return;
      }
      GET_SPECIALTY_SCHOOL(ch) = number;
      write_to_output(d, "Choice selected.\r\n");
      OLC_MODE(d) = STUDY_SET_SCHOOL;
      print_school_info(d, number);
      set_school_menu(d);
      break;
    case STUDY_SET_SCHOOL:
      switch (*arg) {
        case 'q':
        case 'Q':
          display_main_menu(d);
          break;

        default:
          number = atoi(arg);
          switch (number) {
            case 0:
              set_school_submenu(d);
              OLC_MODE(d) = SET_SCHOOL;
              return;
            default: break;
          }
          OLC_MODE(d) = STUDY_SET_SCHOOL;
          set_school_menu(d);
          break;
      }
      break;

      /*****/

    case STUDY_SET_STATS:
      switch (*arg) {
        case 'q':
        case 'Q':
          display_main_menu(d);
          break;

        default:
          number = atoi(arg);
          write_to_output(d, "Please enter the value to modify your current stat by."
              "  Example:  If you want to change your stat from 10 to 14, you would enter "
              "'4' now.  If you wanted to change your stat from 10 to 8, you would enter '-2' now.\r\n");
          switch (number) {
            case 0: /* str */
              OLC_MODE(d) = SET_STAT_STR;
              return;
            case 1: /* dex */
              OLC_MODE(d) = SET_STAT_DEX;
              return;
            case 2: /* con */
              OLC_MODE(d) = SET_STAT_CON;
              return;
            case 3: /* inte */
              OLC_MODE(d) = SET_STAT_INTE;
              return;
            case 4: /* wis */
              OLC_MODE(d) = SET_STAT_WIS;
              return;
            case 5: /* cha */
              OLC_MODE(d) = SET_STAT_CHA;
              return;
            default: break;
          }
          OLC_MODE(d) = STUDY_SET_STATS;
          set_stats_menu(d);
          break;
      }
      break;
    /***** end study set stats */
    case SET_STAT_STR:
      number = MAX(-10, MIN(atoi(arg), 10));
      /*debug*/
      //write_to_output(d, "Number: %d\r\n", number);
      points_left = stat_points_left(d->character);
      /*debug*/
      //write_to_output(d, "Points Left: %d\r\n", points_left);
      new_stat = LEVELUP(d->character)->str + number;
      /*debug*/
      //write_to_output(d, "New Stat: %d\r\n", new_stat);
      if (new_stat < compute_base_str(d->character) ||
          new_stat > compute_base_str(d->character) + MAX_POINTS_IN_A_STAT) {
        write_to_output(d, "This would put you below/above the stat-cap!\r\n");
        break;
      }
      cost_for_number = compute_str_cost(d->character, number) -
              compute_str_cost(d->character, 0); /*total cost*/
      /*debug*/
      //write_to_output(d, "Cost for 'Number': %d\r\n", cost_for_number);
      if ((points_left - cost_for_number) >= 0) {
        if (new_stat >= compute_base_str(d->character) &&
            new_stat <= (compute_base_str(d->character)+MAX_POINTS_IN_A_STAT)) {
          /* success! */
          LEVELUP(d->character)->str = new_stat;
          OLC_MODE(d) = STUDY_SET_STATS;
          set_stats_menu(d);
          return;
        } else {
          write_to_output(d, "That would put you below/above the stat-cap!\r\n");
          break;
        }
      } else {
        write_to_output(d, "You do not have enough points!\r\n");
        break;
      }
      break;
    case SET_STAT_DEX:
      number = MAX(-10, MIN(atoi(arg), 10));
      /*debug*/
      //write_to_output(d, "Number: %d\r\n", number);
      points_left = stat_points_left(d->character);
      /*debug*/
      //write_to_output(d, "Points Left: %d\r\n", points_left);
      new_stat = LEVELUP(d->character)->dex + number;
      /*debug*/
      //write_to_output(d, "New Stat: %d\r\n", new_stat);
      if (new_stat < compute_base_dex(d->character) ||
          new_stat > compute_base_dex(d->character) + MAX_POINTS_IN_A_STAT) {
        write_to_output(d, "This would put you below/above the stat-cap!\r\n");
        break;
      }
      cost_for_number = compute_dex_cost(d->character, number) -
              compute_dex_cost(d->character, 0);
      /*debug*/
      //write_to_output(d, "Cost for 'Number': %d\r\n", cost_for_number);
      if ((points_left - cost_for_number) >= 0) {
        if (new_stat >= compute_base_dex(d->character) &&
            new_stat <= (compute_base_dex(d->character)+MAX_POINTS_IN_A_STAT)) {
          /* success! */
          LEVELUP(d->character)->dex = new_stat;
          OLC_MODE(d) = STUDY_SET_STATS;
          set_stats_menu(d);
          return;
        } else {
          write_to_output(d, "That would put you below/above the stat-cap!\r\n");
          break;
        }
      } else {
        write_to_output(d, "You do not have enough points!\r\n");
        break;
      }
      break;
    case SET_STAT_CON:
      number = MAX(-10, MIN(atoi(arg), 10));
      /*debug*/
      //write_to_output(d, "Number: %d\r\n", number);
      points_left = stat_points_left(d->character);
      /*debug*/
      //write_to_output(d, "Points Left: %d\r\n", points_left);
      new_stat = LEVELUP(d->character)->con + number;
      /*debug*/
      //write_to_output(d, "New Stat: %d\r\n", new_stat);
      if (new_stat < compute_base_con(d->character) ||
          new_stat > compute_base_con(d->character) + MAX_POINTS_IN_A_STAT) {
        write_to_output(d, "This would put you below/above the stat-cap!\r\n");
        break;
      }
      cost_for_number = compute_con_cost(d->character, number) -
              compute_con_cost(d->character, 0);
      /*debug*/
      //write_to_output(d, "Cost for 'Number': %d\r\n", cost_for_number);
      if ((points_left - cost_for_number) >= 0) {
        if (new_stat >= compute_base_con(d->character) &&
            new_stat <= (compute_base_con(d->character)+MAX_POINTS_IN_A_STAT)) {
          /* success! */
          LEVELUP(d->character)->con = new_stat;
          OLC_MODE(d) = STUDY_SET_STATS;
          set_stats_menu(d);
          return;
        } else {
          write_to_output(d, "That would put you below/above the stat-cap!\r\n");
          break;
        }
      } else {
        write_to_output(d, "You do not have enough points!\r\n");
        break;
      }
      break;
    case SET_STAT_INTE:
      number = MAX(-10, MIN(atoi(arg), 10));
      /*debug*/
      //write_to_output(d, "Number: %d\r\n", number);
      points_left = stat_points_left(d->character);
      /*debug*/
      //write_to_output(d, "Points Left: %d\r\n", points_left);
      new_stat = LEVELUP(d->character)->inte + number;
      /*debug*/
      //write_to_output(d, "New Stat: %d\r\n", new_stat);
      if (new_stat < compute_base_inte(d->character) ||
          new_stat > compute_base_inte(d->character) + MAX_POINTS_IN_A_STAT) {
        write_to_output(d, "This would put you below/above the stat-cap!\r\n");
        break;
      }
      cost_for_number = compute_inte_cost(d->character, number) -
              compute_inte_cost(d->character, 0);
      /*debug*/
      //write_to_output(d, "Cost for 'Number': %d\r\n", cost_for_number);
      if ((points_left - cost_for_number) >= 0) {
        if (new_stat >= compute_base_inte(d->character) &&
            new_stat <= (compute_base_inte(d->character)+MAX_POINTS_IN_A_STAT)) {
          /* success! */
          LEVELUP(d->character)->inte = new_stat;
          OLC_MODE(d) = STUDY_SET_STATS;
          set_stats_menu(d);
          return;
        } else {
          write_to_output(d, "That would put you below/above the stat-cap!\r\n");
          break;
        }
      } else {
        write_to_output(d, "You do not have enough points!\r\n");
        break;
      }
      break;
    case SET_STAT_WIS:
      number = MAX(-10, MIN(atoi(arg), 10));
      /*debug*/
      //write_to_output(d, "Number: %d\r\n", number);
      points_left = stat_points_left(d->character);
      /*debug*/
      //write_to_output(d, "Points Left: %d\r\n", points_left);
      new_stat = LEVELUP(d->character)->wis + number;
      /*debug*/
      //write_to_output(d, "New Stat: %d\r\n", new_stat);
      if (new_stat < compute_base_wis(d->character) ||
          new_stat > compute_base_wis(d->character) + MAX_POINTS_IN_A_STAT) {
        write_to_output(d, "This would put you below/above the stat-cap!\r\n");
        break;
      }
      cost_for_number = compute_wis_cost(d->character, number) -
              compute_wis_cost(d->character, 0);
      /*debug*/
      //write_to_output(d, "Cost for 'Number': %d\r\n", cost_for_number);
      if ((points_left - cost_for_number) >= 0) {
        if (new_stat >= compute_base_wis(d->character) &&
            new_stat <= (compute_base_wis(d->character)+MAX_POINTS_IN_A_STAT)) {
          /* success! */
          LEVELUP(d->character)->wis = new_stat;
          OLC_MODE(d) = STUDY_SET_STATS;
          set_stats_menu(d);
          return;
        } else {
          write_to_output(d, "That would put you below/above the stat-cap!\r\n");
          break;
        }
      } else {
        write_to_output(d, "You do not have enough points!\r\n");
        break;
      }
      break;
    case SET_STAT_CHA:
      number = MAX(-10, MIN(atoi(arg), 10));
      /*debug*/
      //write_to_output(d, "Number: %d\r\n", number);
      points_left = stat_points_left(d->character);
      /*debug*/
      //write_to_output(d, "Points Left: %d\r\n", points_left);
      new_stat = LEVELUP(d->character)->cha + number;
      /*debug*/
      //write_to_output(d, "New Stat: %d\r\n", new_stat);
      if (new_stat < compute_base_cha(d->character) ||
          new_stat > compute_base_cha(d->character) + MAX_POINTS_IN_A_STAT) {
        write_to_output(d, "This would put you below/above the stat-cap!\r\n");
        break;
      }
      cost_for_number = compute_cha_cost(d->character, number) -
              compute_cha_cost(d->character, 0);
      /*debug*/
      write_to_output(d, "Cost for 'Number': %d\r\n", cost_for_number);
      if ((points_left - cost_for_number) >= 0) {
        if (new_stat >= compute_base_cha(d->character) &&
            new_stat <= (compute_base_cha(d->character)+MAX_POINTS_IN_A_STAT)) {
          /* success! */
          LEVELUP(d->character)->cha = new_stat;
          OLC_MODE(d) = STUDY_SET_STATS;
          set_stats_menu(d);
          return;
        } else {
          write_to_output(d, "That would put you below/above the stat-cap!\r\n");
          break;
        }
      } else {
        write_to_output(d, "You do not have enough points!\r\n");
        break;
      }
      break;

    case FAVORED_ENEMY:
      switch (*arg) {
        case 'q':
        case 'Q':
          display_main_menu(d);
          break;

        default:
          number = atoi(arg);
          int ranger_level = CLASS_LEVEL(d->character, CLASS_RANGER);
          switch (number) {
            case 0:
              if (ranger_level) {
                LEVELUP(d->character)->favored_slot = number;
                favored_enemy_submenu(d, number);
                OLC_MODE(d) = FAVORED_ENEMY_SUB;
                return;
              }
              break;
            case 1:
              if (ranger_level >= 5) {
                LEVELUP(d->character)->favored_slot = number;
                favored_enemy_submenu(d, number);
                OLC_MODE(d) = FAVORED_ENEMY_SUB;
                return;
              } else {
                write_to_output(d, "You are not a high enough level ranger to"
                        " modify this slot!\r\n");
              }
              break;
            case 2:
              if (ranger_level >= 10) {
                LEVELUP(d->character)->favored_slot = number;
                favored_enemy_submenu(d, number);
                OLC_MODE(d) = FAVORED_ENEMY_SUB;
                return;
              } else {
                write_to_output(d, "You are not a high enough level ranger to"
                        " modify this slot!\r\n");
              }
              break;
            case 3:
              if (ranger_level >= 15) {
                LEVELUP(d->character)->favored_slot = number;
                favored_enemy_submenu(d, number);
                OLC_MODE(d) = FAVORED_ENEMY_SUB;
                return;
              } else {
                write_to_output(d, "You are not a high enough level ranger to"
                        " modify this slot!\r\n");
              }
              break;
            case 4:
              if (ranger_level >= 20) {
                LEVELUP(d->character)->favored_slot = number;
                favored_enemy_submenu(d, number);
                OLC_MODE(d) = FAVORED_ENEMY_SUB;
                return;;
              } else {
                write_to_output(d, "You are not a high enough level ranger to"
                        " modify this slot!\r\n");
              }
              break;
            case 5:
              if (ranger_level >= 25) {
                LEVELUP(d->character)->favored_slot = number;
                favored_enemy_submenu(d, number);
                OLC_MODE(d) = FAVORED_ENEMY_SUB;
                return;
              } else {
                write_to_output(d, "You are not a high enough level ranger to"
                        " modify this slot!\r\n");
              }
              break;
            case 6:
              if (ranger_level >= 30) {
                LEVELUP(d->character)->favored_slot = number;
                favored_enemy_submenu(d, number);
                OLC_MODE(d) = FAVORED_ENEMY_SUB;
                return;
              } else {
                write_to_output(d, "You are not a high enough level ranger to"
                        " modify this slot!\r\n");
              }
              break;
            case 7:
            case 8:
            case 9:
              write_to_output(d, "This slot is not currently modifyable.\r\n");
              break;

          }

          OLC_MODE(d) = FAVORED_ENEMY;
          favored_enemy_menu(d);
          break;
      }
      break;

    case FAVORED_ENEMY_SUB:
      switch (*arg) {
        case 'q':
        case 'Q':
          favored_enemy_menu(d);
          OLC_MODE(d) = FAVORED_ENEMY;
          break;

        default:
          number = atoi(arg);

          if (number < 0 || number >= NUM_NPC_RACES)
            write_to_output(d, "Invalid race!\r\n");
          else {
            GET_FAVORED_ENEMY(d->character, LEVELUP(d->character)->favored_slot) =
                    number;
            favored_enemy_menu(d);
            OLC_MODE(d) = FAVORED_ENEMY;
            return;
          }

          favored_enemy_submenu(d, LEVELUP(d->character)->favored_slot);
          break;
      }
      break;

    case ANIMAL_COMPANION:
      switch (*arg) {
        case 'q':
        case 'Q':
          display_main_menu(d);
          break;
        default:
          number = atoi(arg);

          if (number == 0) {
            GET_ANIMAL_COMPANION(d->character) = number;
            write_to_output(d, "Your companion has been set to OFF.\r\n");
          } else if (number < 0 || number >= NUM_ANIMALS) {
            write_to_output(d, "Not a valid choice!\r\n");
          } else {
            GET_ANIMAL_COMPANION(d->character) = animal_vnums[number];
            write_to_output(d, "You have selected %s.\r\n",
                    animal_names[number]);
          }

          animal_companion_menu(d);
          break;
      }
      break;

    case FAMILIAR_MENU:
      switch (*arg) {
        case 'q':
        case 'Q':
          display_main_menu(d);
          break;
        default:
          number = atoi(arg);

          if (!number) {
            GET_FAMILIAR(d->character) = number;
            write_to_output(d, "Your familiar has been set to OFF.\r\n");
          } else if (number < 0 || number >= NUM_FAMILIARS) {
            write_to_output(d, "Not a valid choice!\r\n");
          } else {
            GET_FAMILIAR(d->character) = familiar_vnums[number];
            write_to_output(d, "You have selected %s.\r\n",
                    familiar_names[number]);
          }

          familiar_menu(d);
          break;
      }
      break;

      /* We should never get here, but just in case... */
    default:
      cleanup_olc(d, CLEANUP_CONFIG);
      free(LEVELUP(d->character));
      LEVELUP(d->character) = NULL;
      mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: study_parse(): Reached default case!");
      write_to_output(d, "Oops...\r\n");
      break;
  }
  /*-------------------------------------------------------------------*/
  /*. END OF CASE */
}

/* some undefines from top of file */
#undef C_BEAR
#undef C_BOAR
#undef C_LION
#undef C_CROCODILE
#undef C_HYENA
#undef C_SNOW_LEOPARD
#undef C_SKULL_SPIDER
#undef C_FIRE_BEETLE
#undef C_CAYHOUND
#undef C_DRACAVES
/*--- paladin mount(s) -----*/
#undef C_W_WARHORSE
#undef C_B_DESTRIER
#undef C_STALLION
#undef C_A_DESTRIER
#undef C_G_WARHORSE
#undef C_P_WARHORSE
#undef C_C_DESTRIER
#undef C_WARDOG
#undef C_WARPONY
#undef C_GRIFFON
/*--- familiars -----*/
#undef F_HUNTER
#undef F_PANTHER
#undef F_MOUSE
#undef F_EAGLE
#undef F_RAVEN
#undef F_IMP
#undef F_PIXIE
#undef F_FAERIE_DRAGON
#undef F_PSEUDO_DRAGON
#undef F_HELLHOUND

#undef MENU_OPT

