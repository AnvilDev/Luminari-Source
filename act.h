/***
**
* @file act.h
* Header file for the core act* c files.
*
* Part of the core tbaMUD source code distribution, which is a derivative
* of, and continuation of, CircleMUD.
*
* All rights reserved.  See license for complete information.
* Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University
* CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.
*
* @todo Utility functions that could easily be moved elsewhere have been
* marked. Suggest a review of all utility functions (aka. non ACMDs) and
* determine if the utility functions should be placed into a lower level
* (non-ACMD focused) shared module.
*
*/
#ifndef _ACT_H_
#define _ACT_H_

#include "utils.h" /* for the ACMD macro */

/*****************************************************************************
 * Begin Functions and defines for act.comm.c
 ****************************************************************************/
/* functions with subcommands */
/* do_gen_comm */
ACMD(do_gen_comm);
#define SCMD_HOLLER   0
#define SCMD_SHOUT    1
#define SCMD_GOSSIP   2
#define SCMD_AUCTION  3
#define SCMD_GRATZ    4
#define SCMD_GEMOTE   5
/* do_qcomm */
ACMD(do_qcomm);
#define SCMD_QSAY     0
#define SCMD_QECHO    1
/* do_spec_com */
ACMD(do_spec_comm);
#define SCMD_WHISPER  0
#define SCMD_ASK      1
/* functions without subcommands */
ACMD(do_say);
ACMD(do_gsay);
ACMD(do_page);
ACMD(do_reply);
ACMD(do_tell);
ACMD(do_write);
/*****************************************************************************
 * Begin Functions and defines for act.informative.c
 ****************************************************************************/
/* Utility Functions */

// char creation help files
void perform_help(struct descriptor_data *d, char *argument);
void perform_affects(struct char_data *ch, struct char_data *k);

// displaying more info -zusuk
void show_obj_to_char(struct obj_data *obj, struct char_data *ch, int mode, int mxp_type);
#define SHOW_OBJ_SHORT	1

/** @todo Move to a utility library */
char *find_exdesc(char *word, struct extra_descr_data *list);
/** @todo Move to a mud centric string utility library */
void space_to_minus(char *str);
/** @todo Move to a help module? */
void game_info(const char *format, ...);
void free_history(struct char_data *ch, int type);
void free_recent_players(void);
/* functions with subcommands */
/* do_commands */
ACMD(do_commands);
#define SCMD_COMMANDS 0
#define SCMD_SOCIALS  1
#define SCMD_WIZHELP  2
/* do_gen_ps */
ACMD(do_gen_ps);
#define SCMD_INFO      0
#define SCMD_HANDBOOK  1
#define SCMD_CREDITS   2
#define SCMD_NEWS      3
#define SCMD_WIZLIST   4
#define SCMD_POLICIES  5
#define SCMD_VERSION   6
#define SCMD_IMMLIST   7
#define SCMD_MOTD      8
#define SCMD_IMOTD     9
#define SCMD_CLEAR     10
#define SCMD_WHOAMI    11
/* do_look */
ACMD(do_look);
#define SCMD_LOOK 0
#define SCMD_READ 1
ACMD(do_affects);
#define SCMD_AFFECTS     0
#define SCMD_COOLDOWNS   1
#define SCMD_RESISTANCES 2

/* functions without subcommands */
ACMD(do_innates);
ACMD(do_abilities);
ACMD(do_masterlist);
ACMD(do_areas);
ACMD(do_attacks);
ACMD(do_consider);
ACMD(do_diagnose);
ACMD(do_disengage);
ACMD(do_equipment);
ACMD(do_examine);
ACMD(do_exits);
ACMD(do_gold);
ACMD(do_help);
ACMD(do_history);
ACMD(do_inventory);
ACMD(do_levels);
ACMD(do_scan);
ACMD(do_score);
ACMD(do_spot);
ACMD(do_listen);
ACMD(do_time);
ACMD(do_toggle);
ACMD(do_users);
ACMD(do_weather);
ACMD(do_where);
ACMD(do_who);
ACMD(do_whois);
ACMD(do_track);

/*****************************************************************************
 * Begin Functions and defines for act.item.c
 ****************************************************************************/
/* defines */
#define EXITN(room, door)		(world[room].dir_option[door])
#define OPEN_DOOR(room, obj, door)	((obj) ?\
		(REMOVE_BIT(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) :\
		(REMOVE_BIT(EXITN(room, door)->exit_info, EX_CLOSED)))
#define CLOSE_DOOR(room, obj, door)	((obj) ?\
		(SET_BIT(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) :\
		(SET_BIT(EXITN(room, door)->exit_info, EX_CLOSED)))
#define LOCK_DOOR(room, obj, door)	((obj) ?\
		(SET_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) :\
		(SET_BIT(EXITN(room, door)->exit_info, EX_LOCKED_EASY)))
#define UNLOCK_DOOR(room, obj, door)	((obj) ?\
		(REMOVE_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) :\
		(REMOVE_BIT(EXITN(room, door)->exit_info, EX_LOCKED)))
#define IS_CLOSED(x, y) (EXIT_FLAGGED(world[(x)].dir_option[(y)], EX_CLOSED))

/* HACK: Had to change this with the new lock strengths from homeland... */

#define TOGGLE_LOCK(room, obj, door)	((obj) ?\
		(TOGGLE_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) :\
		(IS_SET(EXITN(room, door)->exit_info, EX_LOCKED) ?\
                  UNLOCK_DOOR(room, obj, door) :\
                  LOCK_DOOR(room, obj, door)))

/*(TOGGLE_BIT(EXITN(room, door)->exit_info, EX_LOCKED)))*/

/* Utility Functions */
/** @todo Compare with needs of find_eq_pos_script. */
int find_eq_pos(struct char_data *ch, struct obj_data *obj, char *arg);
void name_from_drinkcon(struct obj_data *obj);
void name_to_drinkcon(struct obj_data *obj, int type);
void weight_change_object(struct obj_data *obj, int weight);
void perform_remove(struct char_data *ch, int pos, bool forced);
bool perform_give(struct char_data *ch, struct char_data *vict, struct obj_data *obj);
void perform_wear(struct char_data *ch, struct obj_data *obj, int where);
bool obj_should_fall(struct obj_data *obj);
bool char_should_fall(struct char_data *ch, bool silent);
bool perform_wield(struct char_data *ch, struct obj_data *obj, bool not_silent);

/* functions with subcommands */
/* do_drop */
ACMD(do_drop);
#define SCMD_DROP   0
#define SCMD_JUNK   1
#define SCMD_DONATE 2
/* do_eat */
ACMD(do_eat);
#define SCMD_EAT    0
#define SCMD_TASTE  1
#define SCMD_DRINK  2
#define SCMD_SIP    3
/* do_pour */
ACMD(do_pour);
#define SCMD_POUR  0
#define SCMD_FILL  1
/* functions without subcommands */
ACMD(do_drink);
ACMD(do_get);
ACMD(do_give);
ACMD(do_grab);
ACMD(do_put);
ACMD(do_remove);
ACMD(do_sac);
ACMD(do_wear);
ACMD(do_wield);


/*****************************************************************************
 * Begin Functions and defines for act.movement.c
 ****************************************************************************/

int has_boat(struct char_data *ch);
int has_flight(struct char_data *ch);

/* Functions with subcommands */
/* do_gen_door */
ACMD(do_gen_door);
#define SCMD_OPEN       0
#define SCMD_CLOSE      1
#define SCMD_UNLOCK     2
#define SCMD_LOCK       3
#define SCMD_PICK       4
/* Functions without subcommands */
ACMD(do_disembark);
ACMD(do_enter);
ACMD(do_follow);
ACMD(do_leave);
ACMD(do_move);
ACMD(do_rest);
ACMD(do_sit);
ACMD(do_recline);
ACMD(do_sleep);
ACMD(do_stand);
ACMD(do_wake);
/* Global variables from act.movement.c */
#ifndef __ACT_MOVEMENT_C__
extern const char *cmd_door[];
#endif /* __ACT_MOVEMENT_C__ */


/*****************************************************************************
 * Begin Functions and defines for act.offensive.c
 ****************************************************************************/
/* functions */
void clear_rage(struct char_data *ch);
void perform_stunningfist(struct char_data *ch);
void perform_quiveringpalm(struct char_data *ch);
void perform_rescue(struct char_data *ch, struct char_data *vict);
void perform_smite(struct char_data *ch);
void perform_rage(struct char_data *ch);
void perform_layonhands(struct char_data *ch, struct char_data *vict);
bool perform_knockdown(struct char_data *ch, struct char_data *vict,
        int skill);
bool perform_shieldpunch(struct char_data *ch, struct char_data *vict);
void perform_headbutt(struct char_data *ch, struct char_data *vict);
void perform_sap(struct char_data *ch, struct char_data *vict);
void perform_kick(struct char_data *ch, struct char_data *vict);
bool perform_dirtkick(struct char_data *ch, struct char_data *vict);
void perform_assist(struct char_data *ch, struct char_data *helpee);
void perform_springleap(struct char_data *ch, struct char_data *vict);
bool perform_backstab(struct char_data *ch, struct char_data *vict);
bool has_missile_in_ammo_pouch(struct char_data *ch, struct obj_data *obj,
        bool silent);
bool can_fire_arrow(struct char_data *ch, bool silent);
/* Functions with subcommands */
/* do_hit */
ACMD(do_hit);
#define SCMD_HIT    0
ACMD(do_process_attack);

/* Functions without subcommands */
ACMD(do_fire);
ACMD(do_autofire);
ACMD(do_collect);
ACMD(do_hitall);
ACMD(do_guard);
ACMD(do_charge);
ACMD(do_circle);
ACMD(do_bodyslam);
ACMD(do_springleap);
ACMD(do_headbutt);
ACMD(do_shieldpunch);
ACMD(do_disarm);
ACMD(do_shieldcharge);
ACMD(do_shieldslam);
ACMD(do_dirtkick);
ACMD(do_sap);
ACMD(do_assist);
ACMD(do_rage);
ACMD(do_turnundead);
ACMD(do_bash);
ACMD(do_call);
ACMD(do_fly);
ACMD(do_land);
ACMD(do_frightful);
ACMD(do_breathe);
ACMD(do_tailsweep);
ACMD(do_backstab);
ACMD(do_flee);
ACMD(do_stunningfist);
ACMD(do_quiveringpalm);
ACMD(do_kick);
ACMD(do_smite);
ACMD(do_kill);
ACMD(do_layonhands);
ACMD(do_order);
ACMD(do_perform);
ACMD(do_applypoison);
ACMD(do_abundantstep);
ACMD(do_animatedead);
ACMD(do_rescue);
ACMD(do_taunt);
ACMD(do_treatinjury);
ACMD(do_emptybody);
ACMD(do_wholenessofbody);
ACMD(do_trip);
ACMD(do_whirlwind);
ACMD(do_crystalfist);
ACMD(do_crystalbody);
ACMD(do_supriseaccuracy);
ACMD(do_powerfulblow);
ACMD(do_renewedvigor);
ACMD(do_comeandgetme);

/*****************************************************************************
 * Begin Functions and defines for act.other.c
 ****************************************************************************/
/* Functions with subcommands */
void list_forms(struct char_data *ch);
void perform_shapechange(struct char_data *ch, char *arg, int mode);
int valid_align_by_class(int alignment, int class);
void perform_wildshape(struct char_data *ch, int form_num, int spellnum);
void perform_perform(struct char_data *ch);
void perform_call(struct char_data *ch, int call_type, int level);
/* do_gen_tog */
ACMD(do_gen_tog);
#define SCMD_NOSUMMON    0
#define SCMD_NOHASSLE    1
#define SCMD_BRIEF       2
#define SCMD_COMPACT     3
#define SCMD_NOTELL      4
#define SCMD_NOAUCTION   5
#define SCMD_NOSHOUT     6
#define SCMD_NOGOSSIP    7
#define SCMD_NOGRATZ     8
#define SCMD_NOWIZ       9
#define SCMD_QUEST       10
#define SCMD_SHOWVNUMS   11
#define SCMD_NOREPEAT    12
#define SCMD_HOLYLIGHT   13
#define SCMD_SLOWNS      14
#define SCMD_AUTOEXIT    15
#define SCMD_TRACK       16
#define SCMD_CLS         17
#define SCMD_BUILDWALK   18
#define SCMD_AFK         19
#define SCMD_AUTOLOOT    20
#define SCMD_AUTOGOLD    21
#define SCMD_AUTOSPLIT   22
#define SCMD_AUTOSAC     23
#define SCMD_AUTOASSIST  24
#define SCMD_AUTOMAP     25
#define SCMD_AUTOKEY     26
#define SCMD_AUTODOOR    27
#define SCMD_NOCLANTALK  28
#define SCMD_COLOR       29
#define SCMD_SYSLOG      30
#define SCMD_WIMPY       31
#define SCMD_PAGELENGTH  32
#define SCMD_SCREENWIDTH 33
#define SCMD_AUTOSCAN    34

/* do_quit */
ACMD(do_quit);
#define SCMD_QUI  0
#define SCMD_QUIT 1
/* do_use */
ACMD(do_use);
#define SCMD_USE  0
#define SCMD_QUAFF  1
#define SCMD_RECITE 2
/* do_utter */
ACMD(do_utter);
/* do_diplomacy */
ACMD(do_diplomacy);
#define SCMD_MURMUR     0
#define SCMD_PROPAGANDA 1
#define SCMD_LOBBY      2
/* used by diplomacy skills */
#define DIP_SKILL (diplomacy_types[dip_num].skill)
#define DIP_INCR  (diplomacy_types[dip_num].increase)
#define DIP_WAIT  (diplomacy_types[dip_num].wait)
/* Functions without subcommands */
ACMD(do_recharge);
ACMD(do_buck);
ACMD(do_dismount);
ACMD(do_mount);
ACMD(do_dismiss);
ACMD(do_tame);
ACMD(do_boosts);
ACMD(do_respec);
ACMD(do_gain);
ACMD(do_display);
ACMD(do_shapechange);
ACMD(do_group);
ACMD(do_greport);
ACMD(do_purify);
ACMD(do_happyhour);
ACMD(do_hide);
ACMD(do_lore);
ACMD(do_not_here);
ACMD(do_practice);
ACMD(do_report);
ACMD(do_save);
ACMD(do_search);
ACMD(do_sneak);
ACMD(do_spelllist);
ACMD(do_spells);
ACMD(do_split);
ACMD(do_steal);
ACMD(do_title);
ACMD(do_train);
ACMD(do_visible);
ACMD(do_wildshape);


/*****************************************************************************
 * Begin Functions and defines for act.social.c
 ****************************************************************************/
/* Utility Functions */
void free_social_messages(void);
/** @todo free_action should be moved to a utility function module. */
void free_action(struct social_messg *mess);
/** @todo command list functions probably belong in interpreter */
void free_command_list(void);
/** @todo command list functions probably belong in interpreter */
void create_command_list(void);
/* Functions without subcommands */
ACMD(do_action);
ACMD(do_gmote);

/******************
* memorize
*******************/
ACMD(do_gen_forget);

#define SCMD_FORGET      1
#define SCMD_BLANK       2
#define SCMD_UNCOMMUNE   3
#define SCMD_OMIT        4
#define SCMD_UNADJURE        5

ACMD(do_gen_memorize);

#define SCMD_MEMORIZE   1
#define SCMD_PRAY       2
#define SCMD_COMMUNE    3
#define SCMD_MEDITATE   4
#define SCMD_CHANT      5
#define SCMD_ADJURE     6
#define SCMD_COMPOSE    7


/*****************************************************************************
 * Begin Functions and defines for act.wizard.c
 ****************************************************************************/
/* Utility Functions */
/** @todo should probably be moved to a more general file handler module */
void clean_llog_entries(void);
/** @todo This should be moved to a more general utility file */
int script_command_interpreter(struct char_data *ch, char *arg);
room_rnum find_target_room(struct char_data *ch, char *rawroomstr);
void perform_immort_vis(struct char_data *ch);
void snoop_check(struct char_data *ch);
bool change_player_name(struct char_data *ch, struct char_data *vict, char *new_name);
bool AddRecentPlayer(char *chname, char *chhost, bool newplr, bool cpyplr);
/* Functions with subcommands */
/* do_date */
ACMD(do_date);
#define SCMD_DATE   0
#define SCMD_UPTIME 1
/* do_echo */
ACMD(do_echo);
#define SCMD_ECHO   0
#define SCMD_EMOTE  1
/* do_last */
ACMD(do_last);
#define SCMD_LIST_ALL 1
/* do_shutdown */
ACMD(do_shutdown);
#define SCMD_SHUTDOW   0
#define SCMD_SHUTDOWN  1
/* do_wizutil */
ACMD(do_wizutil);
#define SCMD_REROLL   0
#define SCMD_PARDON   1
#define SCMD_NOTITLE  2
#define SCMD_MUTE     3
#define SCMD_FREEZE   4
#define SCMD_THAW     5
#define SCMD_UNAFFECT 6
/* Functions without subcommands */
ACMD(do_hlqlist);
ACMD(do_advance);
ACMD(do_objlist);
ACMD(do_singlefile);
ACMD(do_at);
ACMD(do_checkloadstatus);
ACMD(do_copyover);
ACMD(do_dc);
ACMD(do_changelog);
ACMD(do_file);
ACMD(do_force);
ACMD(do_gecho);
ACMD(do_goto);
ACMD(do_invis);
ACMD(do_links);
ACMD(do_load);
ACMD(do_oset);
ACMD(do_peace);
ACMD(do_plist);
ACMD(do_purge);
ACMD(do_recent);
ACMD(do_restore);
ACMD(do_return);
ACMD(do_saveall);
ACMD(do_savemobs);
ACMD(do_send);
ACMD(do_set);
ACMD(do_show);
ACMD(do_snoop);
ACMD(do_stat);
ACMD(do_switch);
ACMD(do_teleport);
ACMD(do_trans);
ACMD(do_vnum);
ACMD(do_vstat);
ACMD(do_wizlock);
ACMD(do_wiznet);
ACMD(do_wizupdate);
ACMD(do_zcheck);
ACMD(do_zlock);
ACMD(do_zpurge);
ACMD(do_zreset);
ACMD(do_zunlock);
ACMD(do_afflist);
ACMD(do_typelist);
ACMD(do_eqrating);

ACMD(do_genmap);
ACMD(do_oconvert);
ACMD(do_acconvert);
#endif /* _ACT_H_ */
