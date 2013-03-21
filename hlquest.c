/* *************************************************************************
 *   File: hlquest.c                                   Part of LuminariMUD *
 *  Usage: alternate quest system                                          *
 *  Author: Homeland, ported to tba/luminari by Zusuk                      *
 ************************************************************************* */

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "hlquest.h"
#include "spells.h"
#include "race.h"
#include "class.h"
#include "fight.h"

extern int has_race_kit(int race, int c);

void add_follower(struct char_data * ch, struct char_data *leader);
extern void clear_quest(struct quest_entry *quest);
extern struct char_data *mob_proto;
void erase_spell_memory(struct char_data *ch);
extern int isname(const char *str, const char *namelist);
extern bool perform_give(struct char_data * ch, struct char_data * vict, struct obj_data * obj);
extern void quest_open_door(int room, int door);
//extern int real_zone(int number);

//extern long top_of_zone_table;
extern struct zone_data *zone_table;
//extern long top_of_mobt;
//extern char *pc_class_types[];
extern struct obj_data *obj_proto;
extern struct index_data *obj_index;
extern struct index_data *mob_index;
extern struct room_data *world;
//extern long top_of_world;
extern const char *spells[];
extern struct spell_info_type spell_info[];
extern const char *dirs[];
int level_exp(struct char_data *ch, int level);

void show_quest_to_player(struct char_data *ch, struct quest_entry *quest) {
  struct quest_command *qcom = NULL;
  char buf[MAX_INPUT_LENGTH] = { '\0' };
  
  if (quest->approved)
    send_to_char(ch, "&cc*** &cCQuest is Approved&cc ***&c0\r\n");
  else
    send_to_char(ch, "&cr*** &cRQuest is NOT APPROVED!&cr ***&c0\r\n");
  if (quest->type == QUEST_ASK) {
    sprintf(buf, "&cCASK&c0 %s\r\n", quest->keywords);
    send_to_char(ch, buf);
  } else {
    if (quest->type == QUEST_ROOM) {
      sprintf(buf, "&cCROOM&c0 %d\r\n", quest->room);
      send_to_char(ch, buf);
    } else {

      for (qcom = quest->in; qcom; qcom = qcom->next) {
        switch (qcom->type) {
          case QUEST_COMMAND_ITEM:
            sprintf(buf, "&cCGIVE&c0 %s (%d)\r\n",
                    obj_proto[real_object(qcom->value)].short_description, qcom->value);
            send_to_char(ch, buf);
            break;
          case QUEST_COMMAND_COINS:
            sprintf(buf, "&cCGIVE&c0 %d coins\r\n", qcom->value);
            send_to_char(ch, buf);
            break;
        }
      }
    }
    for (qcom = quest->out; qcom; qcom = qcom->next) {
      switch (qcom->type) {
        case QUEST_COMMAND_ITEM:
          sprintf(buf, "&ccRECIEVE&c0 %s (%d)\r\n",
                  obj_proto[real_object(qcom->value)].short_description
                  , qcom->value);
          send_to_char(ch, buf);
          break;
        case QUEST_COMMAND_COINS:
          sprintf(buf, "&ccRECIEVE&c0 %d coins\r\n", qcom->value);
          send_to_char(ch, buf);
          break;
        case QUEST_COMMAND_LOAD_OBJECT_INROOM:
          sprintf(buf, "&ccLOADOBJECT&c0 %s in %s\r\n",
                  obj_proto[ real_object(qcom->value)].short_description,
                  (qcom->location == -1 ? "CurrentRoom" : world[real_room(qcom->location)].name));
          send_to_char(ch, buf);
          break;
        case QUEST_COMMAND_OPEN_DOOR:
          sprintf(buf, "&ccOPEN_DOOR&c0 %s in %s(%d)\r\n", dirs[qcom->value], world[real_room(qcom->location)].name, qcom->location);
          send_to_char(ch, buf);
          break;
        case QUEST_COMMAND_FOLLOW:
          send_to_char(ch, "&ccFOLLOW&c0 playeStart following player\r\n");
          break;
        case QUEST_COMMAND_CHURCH:
          sprintf(buf, "&ccSET_CHURCH&c0 of player to of %s.\r\n", church_types[qcom->value]);
          send_to_char(ch, buf);
          break;
        case QUEST_COMMAND_KIT:
          sprintf(buf, "&ccSET_KIT&c0 of a %s to become %s.\r\n",
                  pc_class_types[ qcom->location ], pc_class_types[ qcom->value ]);
          send_to_char(ch, buf);
          break;
        case QUEST_COMMAND_LOAD_MOB_INROOM:
          sprintf(buf, "&ccLOADMOB&c0 %s in %s\r\n",
                  mob_proto[ real_mobile(qcom->value)].player.short_descr,
                  (qcom->location == -1 ? "CurrentRoom" : world[real_room(qcom->location)].name));
          send_to_char(ch, buf);
          break;
        case QUEST_COMMAND_ATTACK_QUESTOR:
          send_to_char(ch, "&ccMOB_ATTACK&c0 player!\r\n");
          break;
        case QUEST_COMMAND_DISAPPEAR:
          send_to_char(ch, "&ccREMOVE_MOB&c0\r\n");
          break;
        case QUEST_COMMAND_TEACH_SPELL:
          sprintf(buf, "&ccTEACH_SPELL&c0 %s\r\n", spells[qcom->value]);
          send_to_char(ch, buf);
          break;
        case QUEST_COMMAND_CAST_SPELL:
          sprintf(buf, "&ccCAST_SPELL&c0 %s\r\n", spells[qcom->value]);
          send_to_char(ch, buf);
          break;
      }
    }
  }
  send_to_char(ch, quest->reply_msg);
}

ACMD(do_qinfo) {
  int i = 0, j = 0, start_num = 0, end_num = 0, number = 0, found = 0;
  int realnum = -1;
  struct quest_entry *quest = NULL;
  struct quest_command *qcmd = NULL;
  char arg[MAX_INPUT_LENGTH] = { '\0' };
  char buf[MAX_INPUT_LENGTH] = { '\0' };
  char buf2[MAX_INPUT_LENGTH] = { '\0' };

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "qinfo what object?\r\n");
    return;
  }
  if ((number = atoi(arg)) < 0) {
    send_to_char(ch, "No such object.\r\n");
    return;
  }

  for (j = 0; j <= top_of_zone_table; j++) {
    start_num = zone_table[j].number * 100;
    end_num = zone_table[real_zone(start_num)].top;
    for (i = start_num; i <= end_num; i++) {
      if ((realnum = real_mobile(i)) >= 0) {
        if (mob_proto[realnum].mob_specials.quest) {
          for (quest = mob_proto[realnum].mob_specials.quest; quest; quest =
                  quest->next) {
            for (qcmd = quest->in; qcmd && !found; qcmd = qcmd->next) {
              if (qcmd->type == QUEST_COMMAND_ITEM && number == qcmd->value) {
                found = 1;
              }
            }
            for (qcmd = quest->out; qcmd && !found; qcmd = qcmd->next) {
              if (qcmd->type == QUEST_GIVE && number == qcmd->value) {
                found = 1;
              }
            }
            if (found) {
              sprintf(buf, "You");
              for (qcmd = quest->in; qcmd; qcmd = qcmd->next) {
                if (qcmd->type == QUEST_GIVE) {
                  sprintf(buf2, " give %s (%d)",
                          obj_proto[ real_object(qcmd->value)].short_description,
                          qcmd->value);
                  strcat(buf, buf2);
                } else if (qcmd->type == QUEST_COMMAND_COINS) {
                  sprintf(buf2, " give %d copper coins", qcmd->value);
                  strcat(buf, buf2);
                }
                if (qcmd->next) {
                  strcat(buf, " and");
                }
              }
              sprintf(buf2, "\r\nTo %s (%d)\r\n", mob_proto[realnum].player.
                      short_descr, i);
              strcat(buf, buf2);
              for (qcmd = quest->out; qcmd; qcmd = qcmd->next) {
                if (qcmd->type == QUEST_GIVE) {
                  sprintf(buf2, " and you receive %s (%d)",
                          obj_proto[ real_object(qcmd->value)].short_description,
                          qcmd->value);
                  strcat(buf, buf2);
                } else if (qcmd->type == QUEST_COMMAND_DISAPPEAR) {
                  strcat(buf, " and the mob disappears");
                } else if (qcmd->type == QUEST_COMMAND_ATTACK_QUESTOR) {
                  strcat(buf, " and the mob Attacks!");
                } else if (qcmd->type == QUEST_COMMAND_LOAD_OBJECT_INROOM) {
                  sprintf(buf2, " and the mob loads %s in %s(%d)",
                          obj_proto[ real_object(qcmd->value)].short_description,
                          (qcmd->location == -1 ? "CurrentRoom" :
                          world[real_room(qcmd->location)].name), qcmd->location);
                  strcat(buf, buf2);
                } else if (qcmd->type == QUEST_COMMAND_TEACH_SPELL) {
                  sprintf(buf2, " and teaches you %s", spells[qcmd->value]);
                  strcat(buf, buf2);
                } else if (qcmd->type == QUEST_COMMAND_OPEN_DOOR) {
                  sprintf(buf2, "and opens a door %s in %s(%d)", dirs[qcmd->value],
                          world[real_room(qcmd->location)].name, qcmd->location);
                  strcat(buf, buf2);
                } else if (qcmd->type == QUEST_COMMAND_LOAD_MOB_INROOM) {
                  sprintf(buf2, " and loads %s in %s (%d)",
                          mob_proto[ real_mobile(qcmd->value)].player.short_descr,
                          qcmd->location == -1 ? "CurrentRoom" :
                          world[real_room(qcmd->location)].name, qcmd->location);
                  strcat(buf, buf2);
                } else if (qcmd->type == QUEST_COMMAND_FOLLOW) {
                  strcat(buf, " and follows you");
                } else if (qcmd->type == QUEST_COMMAND_KIT) {
                  sprintf(buf, "and changes your kit to %s", pc_class_types[qcmd->value]);
                  strcat(buf, buf2);
                } else if (qcmd->type == QUEST_COMMAND_CHURCH) {
                  sprintf(buf2, " and changes your religious affiliation to %s",
                          church_types[qcmd->value]);
                  strcat(buf, buf2);
                } else {
                  strcat(buf, " tell azuth get off his lazy @$$ and fix this");
                }
              } // end quest-> out loop 

              strcat(buf, ".\r\n\r\n");
              send_to_char(ch, buf);
              found = 0;
            } // end of if (found)
          } // end quest loop
        } // End if (mob has quest)
      } //do we have a mob?
    } // mobs in zone walk
  } // zone table walk
}

bool has_spell_a_quest(int spell) {
  int i;
  struct quest_entry *quest;
  struct quest_command *qcom;
  for (i = 0; i < top_of_mobt; i++) {
    if (mob_proto[i].mob_specials.quest) {
      for (quest = mob_proto[i].mob_specials.quest; quest; quest = quest->next) {
        for (qcom = quest->out; qcom; qcom = qcom->next) {
          if (qcom->type == QUEST_COMMAND_TEACH_SPELL && qcom->value == spell)
            return TRUE;
        }
      }
    }
  }
  return FALSE;
}

ACMD(do_checkapproved) {
  int i;
  int count;
  int total;
  struct quest_entry *quest;
  char buf[MAX_INPUT_LENGTH] = { '\0' };

  for (i = 0; i < top_of_mobt; i++) {
    if (mob_proto[i].mob_specials.quest) {
      count = 0;
      total = 0;
      for (quest = mob_proto[i].mob_specials.quest; quest; quest = quest->next) {
        if (quest->approved == FALSE)
          count++;
        total++;
      }
      if (count > 0) {
        sprintf(buf, "[%5d] %-40s  %d/%d\r\n"
                , mob_index[i].vnum
                , mob_proto[i].player.short_descr
                , total - count
                , total
                );
        send_to_char(ch, buf);
      }
    }
  }
}

ACMD(do_kitquests) {
  char buf[MAX_INPUT_LENGTH] = { '\0' };
  
  if (GET_LEVEL(ch) < LVL_IMMORT) {
    sprintf(buf, "(GC) %s looked at kitquest list.", GET_NAME(ch));
    log(buf);
  }

  int i;
  struct quest_entry *quest;
  struct quest_command *qcom;

  for (i = 0; i < top_of_mobt; i++) {
    if (mob_proto[i].mob_specials.quest) {
      for (quest = mob_proto[i].mob_specials.quest; quest; quest = quest->next) {
        // check in.
        for (qcom = quest->out; qcom; qcom = qcom->next) {
          if (qcom->type == QUEST_COMMAND_KIT) {
            sprintf(buf, "&cc%-32s&c0 - %s(&cW%d&c0)\r\n"
                    , pc_class_types[ qcom->value ]
                    , mob_proto[i].player.short_descr
                    , mob_index[i].vnum
                    );
            send_to_char(ch, buf);
          }
        }
      }
    }
  }
}

ACMD(do_spellquests) {
  char buf[MAX_INPUT_LENGTH] = { '\0' };

  if (GET_LEVEL(ch) < LVL_IMMORT) {
    sprintf(buf, "(GC) %s looked at spellquest list.", GET_NAME(ch));
    log(buf);
  }

  int i;
  struct quest_entry *quest;
  struct quest_command *qcom;
  send_to_char(ch, "&ccSpells requiring quests:&c0\r\n&cc-------------------&c0\r\n");
  for (i = 0; i < MAX_SPELLS; i++) {
    if (spell_info[i].quest) {
      sprintf(buf, "&cc%-32s&c0  %s\r\n", spells[i], (has_spell_a_quest(i) ? "(&cCQuest&c0)" : ""));
      send_to_char(ch, buf);
    }
  }

  send_to_char(ch, "\r\n&cCCurrent quests:&c0\r\n&cc-------------------&c0\r\n");
  for (i = 0; i < top_of_mobt; i++) {
    if (mob_proto[i].mob_specials.quest) {
      for (quest = mob_proto[i].mob_specials.quest; quest; quest = quest->next) {
        // check in.
        for (qcom = quest->out; qcom; qcom = qcom->next) {
          if (qcom->type == QUEST_COMMAND_TEACH_SPELL) {
            sprintf(buf, "&cc%-32s&c0 - %s(&cW%d&c0)\r\n"
                    , spells[qcom->value]
                    , mob_proto[i].player.short_descr
                    , mob_index[i].vnum
                    );
            send_to_char(ch, buf);
          }
        }
      }
    }
  }
}

ACMD(do_qref) {
  int i;
  int count = 0;
  int vnum = 0;
  int real_num = 0;
  struct quest_entry *quest;
  struct quest_command *qcom;
  char buf[MAX_INPUT_LENGTH] = { '\0' };

  one_argument(argument, buf);
  
  if (!*buf) {
    send_to_char(ch, "qref what object?\r\n");
    return;
  }
  
  vnum = atoi(buf);
  real_num = real_object(vnum);
  
  if (real_num < 0) {
    send_to_char(ch, "&cRNo such object!&c0\r\n");
    return;
  }

  if (GET_LEVEL(ch) < LVL_IMMORT) {
    sprintf(buf, "(GC) %s did a reference check for (%d).", GET_NAME(ch), vnum);
    log(buf);
  }

  for (i = 0; i < top_of_mobt; i++) {
    if (mob_proto[i].mob_specials.quest) {
      for (quest = mob_proto[i].mob_specials.quest; quest; quest = quest->next) {
        // check in.
        for (qcom = quest->in; qcom; qcom = qcom->next) {
          if (qcom->value == vnum && qcom->type == QUEST_COMMAND_ITEM) {
            sprintf(buf, "&cCGIVE&c0 %s to %s(&cW%d&c0)\r\n"
                    , obj_proto[real_num].short_description
                    , mob_proto[i].player.short_descr
                    , mob_index[i].vnum
                    );
            send_to_char(ch, buf);
            count++;
          }
        }

        // check out.
        for (qcom = quest->out; qcom; qcom = qcom->next) {
          if (qcom->value == vnum) {
            switch (qcom->type) {
              case QUEST_COMMAND_ITEM:
                sprintf(buf, "&cCRECIEVE&c0 %s from %s(&cW%d&c0)\r\n"
                        , obj_proto[real_num].short_description
                        , mob_proto[i].player.short_descr
                        , mob_index[i].vnum
                        );
                send_to_char(ch, buf);
                count++;
                break;
              case QUEST_COMMAND_LOAD_OBJECT_INROOM:
                sprintf(buf, "&ccLOADOBJECT&c0 %s in quest for %s (&cW%d&c0)\r\n",
                        obj_proto[ real_num].short_description
                        , mob_proto[i].player.short_descr
                        , mob_index[i].vnum);
                send_to_char(ch, buf);
                count++;
                break;

            }
          }
        }


      }
    }
  }
  if (count == 0) {
    send_to_char(ch, "&cRThat object is not used in any quests!&c0\r\n");
    return;
  }
}

ACMD(do_qview) {
  struct quest_entry *quest;
  int num;
  char buf[MAX_INPUT_LENGTH] = { '\0' };
  
  one_argument(argument, buf);
  
  if (!*buf) {
    send_to_char(ch, "Qview what mob?\r\n");
    return;
  }

  num = real_mobile(atoi(buf));
  if (num < 0) {
    send_to_char(ch, "&cRNo such mobile!&c0\r\n");
    return;
  }

  if (mob_proto[num].mob_specials.quest == 0) {
    send_to_char(ch, "&cRThat mob has no quests.&c0\r\n");
    return;
  }

  if (GET_LEVEL(ch) < 60) {
    sprintf(buf, "(GC) %s has peeked at quest for (%d).", GET_NAME(ch), atoi(buf));
    log(buf);
  }

  for (quest = mob_proto[num].mob_specials.quest; quest; quest = quest->next) {
    show_quest_to_player(ch, quest);
    if (quest->next)
      send_to_char(ch, "\r\n&cW-------------------------------------&c0\r\n\r\n");
  }

}

void give_back_items(struct char_data *questor, struct char_data *player, struct quest_entry *quest) {
  struct quest_command *qcom;
  struct obj_data * obj;

  for (qcom = quest->in; qcom; qcom = qcom->next) {
    switch (qcom->type) {
      case QUEST_COMMAND_ITEM:
        obj = get_obj_in_list_num(real_object(qcom->value),
                questor->carrying);
        if (obj) {
          obj_from_char(obj);
          obj_to_char(obj, player);
        }
        break;
    }
  }

}

bool is_object_in_a_quest(struct obj_data *obj) {
  int i;
  int vnum = 0;
  struct quest_entry *quest;
  struct quest_command *qcom;
  vnum = GET_OBJ_VNUM(obj);

  for (i = 0; i < top_of_mobt; i++) {
    if (mob_proto[i].mob_specials.quest) {
      for (quest = mob_proto[i].mob_specials.quest; quest; quest = quest->next) {
        // check in.           
        for (qcom = quest->in; qcom; qcom = qcom->next) {
          if (qcom->value == vnum && qcom->type == QUEST_COMMAND_ITEM)
            return TRUE;
        }
        // check out.
        for (qcom = quest->out; qcom; qcom = qcom->next) {
          if (qcom->value == vnum) {
            switch (qcom->type) {
              case QUEST_COMMAND_ITEM:
                return TRUE;
              case QUEST_COMMAND_LOAD_OBJECT_INROOM:
                return TRUE;
            }
          }
        }
      }
    }
  }
  return FALSE;
}

/* temporary definition for initial compile by zusuk */
#define CLASS_LICH 0
#define RACE_LICH 0
void perform_out_chain(struct char_data *ch, struct char_data *victim, struct quest_entry *quest, char *name) {
  struct char_data *mob;
  struct quest_command *qcom;
  struct char_data *homie = NULL, *nexth = NULL;
  struct obj_data *obj;
  char buf[MAX_INPUT_LENGTH] = { '\0' };
  int i = 0;
  
  // heh.. give stuff..
  act(quest->reply_msg, FALSE, ch, 0, victim, TO_CHAR);
  
  for (qcom = quest->out; qcom; qcom = qcom->next) {
    switch (qcom->type) {
      case QUEST_COMMAND_COINS:
        GET_GOLD(victim) += qcom->value;
        break;
      case QUEST_COMMAND_ITEM:
        obj = read_object(real_object(qcom->value), REAL);

        obj_to_char(obj, victim);
        if (FALSE == perform_give(victim, ch, obj)) {
          act("$n drops $p at the ground.", TRUE, victim, obj, 0, TO_ROOM);
          obj_from_char(obj);
          obj_to_room(obj, ch->in_room);
        }
        break;
      case QUEST_COMMAND_LOAD_OBJECT_INROOM:
        obj = read_object(real_object(qcom->value), REAL);
        if (qcom->location == -1)
          obj_to_room(obj, victim->in_room);
        else
          obj_to_room(obj, real_room(qcom->location));
        break;
      case QUEST_COMMAND_LOAD_MOB_INROOM:
        mob = read_mobile(real_mobile(qcom->value), REAL);
        if (qcom->location == -1)
          char_to_room(mob, victim->in_room);
        else
          char_to_room(mob, real_room(qcom->location));
        break;
      case QUEST_COMMAND_ATTACK_QUESTOR:
        hit(victim, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
        WAIT_STATE(victim, PULSE_VIOLENCE + 2);
        break;
      case QUEST_COMMAND_OPEN_DOOR:
        quest_open_door(real_room(qcom->location), qcom->value);
        break;
      case QUEST_COMMAND_DISAPPEAR:
        char_from_room(victim);
        char_to_room(victim, real_room(1));
        if (victim->master)
          stop_follower(victim);
        for (homie = world[victim->in_room].people; homie; homie = nexth) {
          nexth = homie->next_in_room;
          if (IS_NPC(homie) && homie->master == victim) {
            char_from_room(homie);
            char_to_room(homie, real_room(1));
          }
        }
        break;
      case QUEST_COMMAND_FOLLOW:
        if (circle_follow(victim, ch)) {
          send_to_char(ch, "Sorry, following in circles can not be allowed.\r\n");
          return;
        }
        if (victim->master) {
          send_to_char(ch, "Sorry, I am already following someone else.\r\n");
          return;
        }

        add_follower(victim, ch);
        SET_BIT_AR(AFF_FLAGS(victim), AFF_CHARM);

        break;
      case QUEST_COMMAND_CHURCH:
        GET_CHURCH(ch) = qcom->value;
        sprintf(buf, "You are now a servant of %s.\r\n", church_types[GET_CHURCH(ch)]);
        send_to_char(ch, buf);
        break;
      case QUEST_COMMAND_KIT:
        if (GET_CLASS(ch) != qcom->location) {
          sprintf(buf, "You need to be a %s to learn how to be a %s.\r\n"
                  , pc_class_types[ qcom->location ]
                  , pc_class_types[ qcom->value ]
                  );
          give_back_items(victim, ch, quest);
          send_to_char(ch, buf);
        } else if (0 == has_race_kit(GET_RACE(ch), qcom->value)) {
          sprintf(buf, "Your race can NEVER learn how to become a %s.\r\n"
                  , pc_class_types[ qcom->value ]
                  );
          send_to_char(ch, buf);
          give_back_items(victim, ch, quest);
          log("quest_log : %s failed to do a kitquest.(Not right race)", GET_NAME(ch));
        } else if (GET_LEVEL(ch) < 10) {
          send_to_char(ch, "You are to low level to do this now.\r\n");
          log("quest_log : %s failed to do a kitquest.(too low level)", GET_NAME(ch));
          give_back_items(victim, ch, quest);

        } else if (GET_LEVEL(ch) < (LVL_IMMORT-1) && qcom->value == CLASS_LICH) {
          send_to_char(ch, "You are to low level to do this now.\r\n");
          log("quest_log : %s failed to do a kitquest.(too low level)", GET_NAME(ch));
          give_back_items(victim, ch, quest);
        } else {
          ch->player.chclass = qcom->value;
          erase_spell_memory(ch);

          if (qcom->value == CLASS_LICH) {
            //hack for lich remort..

            GET_RACE(ch) = RACE_LICH;
            GET_LEVEL(ch) -= 9;
            //GET_HOMETOWN(ch) = 3; /*Zhentil Keep*/s

            if (GET_EXP(ch) > level_exp(ch, GET_LEVEL(ch)))
              GET_EXP(ch) = level_exp(ch, GET_LEVEL(ch));

            send_to_char(ch, "&cLYour &cWlifeforce&cL is ripped apart of you, and you realize&c0\r\n"
                    "&cLthat you are dieing. Your body is now merely a vessel for your power.&c0\r\n");

          }

          for (i = 0; i < MAX_SKILLS; i++) {
            if (GET_SKILL(ch, i) && spell_info[i].min_level[(int) GET_CLASS(ch)] > GET_LEVEL(ch)) {
              GET_SKILL(ch, i) = 0;
            }
          }

          sprintf(buf, "You are now a %s.\r\n", pc_class_types[ GET_CLASS(ch) ]);
          send_to_char(ch, buf);
          log("quest_log : %s have changed to %s", GET_NAME(ch), pc_class_types[ GET_CLASS(ch) ]);
        }
        break;
      case QUEST_COMMAND_TEACH_SPELL:
        if (GET_LEVEL(ch) < spell_info[qcom->value].min_level[(int) GET_CLASS(ch) ])
          send_to_char(ch, "The teaching is way beyond your comprehension!\r\n");
        else if (GET_SKILL(ch, qcom->value) > 0)
          send_to_char(ch, "You realize that you already know this way too well.\r\n");
        else {
          sprintf(buf, "$N teaches you '&cL%s&c0'", spells[qcom->value ]);
          act(buf, FALSE, ch, 0, victim, TO_CHAR);
          GET_SKILL(ch, qcom->value) = 100;
          sprintf(buf, "quest_log: %s have quested %s", GET_NAME(ch), spells[qcom->value]);
          log(buf);
        }
        break;

      case QUEST_COMMAND_CAST_SPELL:
        call_magic(victim, ch, 0, qcom->value, GET_LEVEL(victim), CAST_SPELL);
        break;
    }
  }
}
#undef CLASS_LICH
#undef RACE_LICH

void quest_room(struct char_data * ch) {
  struct quest_entry *quest;

  if (!IS_NPC(ch))
    return;
  if (!ch->master)
    return;

  for (quest = ch->mob_specials.quest; quest; quest = quest->next) {
    /* Mortals can only quest on approved quests */
    if (quest->type == QUEST_ROOM && ch->in_room == real_room(quest->room) && ch->in_room == ch->master->in_room) {
      perform_out_chain(ch->master, ch, quest, GET_NAME(ch->master));
      return;
    }
  }
}

void quest_ask(struct char_data * ch, struct char_data * victim, char *keyword) {
  struct quest_entry *quest;
  char buf[MAX_INPUT_LENGTH] = { '\0' };

  if (IS_NPC(ch))
    return;
  if (!IS_NPC(victim))
    return;

  if (GET_LEVEL(ch) > 50 && GET_LEVEL(ch) < 60) {
    sprintf(buf, "(GC) %s asked '%s' on %s (%d).", GET_NAME(ch), keyword, GET_NAME(victim), GET_MOB_VNUM(victim));
    log(buf);
  }

  for (quest = victim->mob_specials.quest; quest; quest = quest->next) {
    /* Mortals can only quest on approved quests */
    if (quest->approved == FALSE && GET_LEVEL(ch) < LVL_IMMORT)
      continue;

    if (quest->type == QUEST_ASK && isname(keyword, quest->keywords)) {
      act(quest->reply_msg, FALSE, ch, 0, victim, TO_CHAR);
      return;
    }
  }
}

void quest_give(struct char_data * ch, struct char_data * victim) {
  struct quest_entry *quest;
  struct quest_command *qcom;
  bool fullfilled;
  struct obj_data *obj;

  if (!ch || !victim)
    return;
  if (IS_NPC(ch))
    return;
  if (!IS_NPC(victim))
    return;

  for (quest = victim->mob_specials.quest; quest; quest = quest->next) {
    /* Mortals can only quest on approved quests */
    if (quest->approved == FALSE && GET_LEVEL(ch) < LVL_IMMORT)
      continue;

    if (quest->type == QUEST_GIVE) {
      fullfilled = TRUE;
      for (qcom = quest->in; qcom && fullfilled == TRUE; qcom = qcom->next) {
        switch (qcom->type) {
          case QUEST_COMMAND_COINS:
            if (GET_GOLD(victim) < qcom->value)
              fullfilled = FALSE;
            break;
          case QUEST_COMMAND_ITEM:
            if (!get_obj_in_list_num(real_object(qcom->value),
                    victim->carrying))
              fullfilled = FALSE;
            break;
        }
      }
      if (fullfilled) {
        //remove given items from inventory...
        for (qcom = quest->in; qcom; qcom = qcom->next) {
          switch (qcom->type) {
            case QUEST_COMMAND_COINS:
              GET_GOLD(victim) = 0;
              break;
            case QUEST_COMMAND_ITEM:
              obj = get_obj_in_list_num(real_object(qcom->value),
                      victim->carrying);
              obj_from_char(obj);
              obj_to_room(obj, real_room(1));
              break;
          }
        }

        perform_out_chain(ch, victim, quest, GET_NAME(ch));
      }
    }
  }
}

void boot_the_quests(FILE * quest_f, char *filename, int rec_count) {
  char str[256];
  char line[256];
  char inner[256];
  int temp;
  bool done = FALSE;
  bool approved = FALSE;
  struct char_data *mob = 0;
  struct quest_command *qcom = 0;
  struct quest_command *qlast = 0;
  struct quest_entry *quest = 0;
  char buf2[MAX_INPUT_LENGTH] = { '\0' };

  while (done == FALSE) {
    get_line(quest_f, line);
    approved = FALSE;
    if (strlen(line) > 1)
      approved = TRUE;

    switch (line[0]) { /* New quest */
      case '#':
        sscanf(line, "#%d", &temp);
        mob = &mob_proto[real_mobile(temp)];
        break;
      case 'A':
        if (mob == 0) {
          log("ERROR: No mob defined in quest in %s", filename);
          return;
        }

        CREATE(quest, struct quest_entry, 1);
        clear_quest(quest);
        quest->type = QUEST_ASK;
        quest->approved = approved;
        quest->keywords = fread_string(quest_f, buf2);
        quest->reply_msg = fread_string(quest_f, buf2);
        quest->next = mob->mob_specials.quest;
        mob->mob_specials.quest = quest;
        break;
      case 'R':
        get_line(quest_f, inner);
        sscanf(inner, "%d", &temp);
      case 'Q':
        CREATE(quest, struct quest_entry, 1);
        clear_quest(quest);
        quest->room = temp;
        quest->type = line[0] == 'Q' ? QUEST_GIVE : QUEST_ROOM;
        quest->approved = approved;
        quest->keywords = 0;
        quest->reply_msg = fread_string(quest_f, buf2);
        quest->next = mob->mob_specials.quest;
        mob->mob_specials.quest = quest;
        do {
          get_line(quest_f, inner);
          CREATE(qcom, struct quest_command, 1);
          if (3 == sscanf(inner + 1, "%s%d%d", str, &qcom->value, &qcom->location
                  )) {
          } else
            qcom->location = -1;
          switch (str[0]) {
            case 'C': qcom->type = QUEST_COMMAND_COINS;
              break;
            case 'I': qcom->type = QUEST_COMMAND_ITEM;
              break;
            case 'O': qcom->type = QUEST_COMMAND_LOAD_OBJECT_INROOM;
              break;
            case 'M': qcom->type = QUEST_COMMAND_LOAD_MOB_INROOM;
              break;
            case 'A': qcom->type = QUEST_COMMAND_ATTACK_QUESTOR;
              break;
            case 'D': qcom->type = QUEST_COMMAND_DISAPPEAR;
              break;
            case 'T': qcom->type = QUEST_COMMAND_TEACH_SPELL;
              break;
            case 'X': qcom->type = QUEST_COMMAND_OPEN_DOOR;
              break;
            case 'F': qcom->type = QUEST_COMMAND_FOLLOW;
              break;
            case 'U': qcom->type = QUEST_COMMAND_CHURCH;
              break;
            case 'K': qcom->type = QUEST_COMMAND_KIT;
              break;
            case 'S': qcom->type = QUEST_COMMAND_CAST_SPELL;
              break;

          }
          switch (inner[0]) {
            case 'I':
              qcom->next = quest->in;
              quest->in = qcom;
              break;
            case 'O':
              if (quest->out == 0)
                quest->out = qcom;
              else {
                qlast = quest->out;
                while (qlast->next != 0)
                  qlast = qlast->next;
                qlast->next = qcom;
              }
              break;

          }
        } while (*inner != 'S');
        break;
      case '$':
        done = TRUE;
        break;
      default:
        return;
        break;
    }
  }
}

void clear_quest(struct quest_entry *quest) {
  quest->approved = FALSE;
  quest->next = NULL;
  quest->in = 0;
  quest->out = 0;
  quest->reply_msg = 0;
  quest->keywords = 0;
  quest->room = 1;
}

void free_quests(struct quest_entry *quest) {
  struct quest_entry *next;
  struct quest_command *qcom;
  while (quest) {
    next = quest->next;
    free(quest->keywords);
    free(quest->reply_msg);
    while (quest->in) {
      qcom = quest->in;
      quest->in = qcom->next;
      free(qcom);
    }
    while (quest->out) {
      qcom = quest->out;
      quest->out = qcom->next;
      free(qcom);
    }
    free(quest);
    quest = next;
  }
}

void free_quest(struct char_data * ch) {
  if (!IS_NPC(ch))
    return;
  free_quests(ch->mob_specials.quest);
  ch->mob_specials.quest = 0;
}

int quest_value_vnum(struct quest_command * qcom) {
  switch (qcom->type) {
    case QUEST_COMMAND_TEACH_SPELL:
    case QUEST_COMMAND_COINS:
      return qcom->value;
    case QUEST_COMMAND_ITEM:
    case QUEST_COMMAND_LOAD_OBJECT_INROOM:
      return GET_OBJ_VNUM(&obj_proto[qcom->value]);
      break;
    case QUEST_COMMAND_LOAD_MOB_INROOM:
      return GET_MOB_VNUM(&mob_proto[qcom->value]);
      break;
    case QUEST_COMMAND_ATTACK_QUESTOR:
    case QUEST_COMMAND_DISAPPEAR:
      return 0;
  }
  return 0;
}

int quest_location_vnum(struct quest_command *qcom) {
  if (qcom->location == -1)
    return -1;

  switch (qcom->type) {
    case QUEST_COMMAND_LOAD_OBJECT_INROOM:
    case QUEST_COMMAND_LOAD_MOB_INROOM:
      return GET_ROOM_VNUM(qcom->value);
  }
  return -1;
}

