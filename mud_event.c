/**************************************************************************
 *  File: mud_event.c                                       Part of tbaMUD *
 *  Usage: Handling of the mud event system                                *
 *                                                                         *
 *  By Vatiken. Copyright 2012 by Joseph Arnusch                           *
 **************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "db.h"
#include "dg_event.h"
#include "constants.h"
#include "comm.h"  /* For access to the game pulse */
#include "lists.h"
#include "mud_event.h"
#include "handler.h"
#include "wilderness.h"


/* Global List */
struct list_data * world_events = NULL;

/* The mud_event_index[] is merely a tool for organizing events, and giving
 * them a "const char *" name to help in potential debugging */
struct mud_event_list mud_event_index[] = {
  { "Null", NULL, -1}, /* eNULL */
  { "Protocol", get_protocols, EVENT_DESC}, /* ePROTOCOLS */
  { "Whirlwind", event_whirlwind, EVENT_CHAR}, /* eWHIRLWIND */
  { "Casting", event_casting, EVENT_CHAR}, /* eCASTING */
  { "Lay on hands", event_countdown, EVENT_CHAR}, // eLAYONHANDS
  { "Treat injury", event_countdown, EVENT_CHAR}, // eTREATINJURY
  { "Taunt", event_countdown, EVENT_CHAR}, // eTAUNT
  { "Taunted", event_countdown, EVENT_CHAR}, // eTAUNTED
  { "Mummy dust", event_countdown, EVENT_CHAR}, // eMUMMYDUST
  { "Dragon knight", event_countdown, EVENT_CHAR}, //  eDRAGONKNIGHT
  { "Greater ruin", event_countdown, EVENT_CHAR}, // eGREATERRUIN
  { "Hellball", event_countdown, EVENT_CHAR}, // eHELLBALL
  { "Epic mage armor", event_countdown, EVENT_CHAR}, // eEPICMAGEARMOR
  { "Epic warding", event_countdown, EVENT_CHAR}, // eEPICWARDING
  { "Memorizing", event_memorizing, EVENT_CHAR}, //eMEMORIZING
  { "Stunned", event_countdown, EVENT_CHAR}, //eSTUNNED
  { "Stunning fist", event_daily_use_cooldown, EVENT_CHAR}, //eSTUNNINGFIST
  { "Crafting", event_crafting, EVENT_CHAR}, //eCRAFTING
  { "Crystal fist", event_daily_use_cooldown, EVENT_CHAR}, //eCRYSTALFIST
  { "Crystal body", event_daily_use_cooldown, EVENT_CHAR}, //eCRYRSTALBODY
  { "Rage", event_countdown, EVENT_CHAR}, //eRAGE
  { "Acid arrow", event_acid_arrow, EVENT_CHAR}, //eACIDARROW
  { "Defensive Roll", event_countdown, EVENT_CHAR}, // eD_ROLL
  { "Purify", event_countdown, EVENT_CHAR}, // ePURIFY
  { "Call Animal Companion", event_countdown, EVENT_CHAR}, // eC_ANIMAL
  { "Call Familiar", event_countdown, EVENT_CHAR}, // eC_FAMILIAR
  { "Call Mount", event_countdown, EVENT_CHAR}, // eC_MOUNT
  { "Implode", event_implode, EVENT_CHAR}, //eIMPLODE
  { "Smite Evil", event_countdown, EVENT_CHAR}, // eSMITE
  { "Perform", event_countdown, EVENT_CHAR}, // ePERFORM
  { "Mob Purge", event_countdown, EVENT_CHAR}, // ePURGEMOB
  { "SoV Ice Storm", event_ice_storm, EVENT_CHAR}, // eICE_STORM
  { "SoV Chain Lightning", event_chain_lightning, EVENT_CHAR}, // eCHAIN_LIGHTNING
  { "Darkness", event_countdown, EVENT_ROOM}, /* eDARKNESS */
  { "Magic Food", event_countdown, EVENT_CHAR}, /* eMAGIC_FOOD */
  { "Fisted", event_countdown, EVENT_CHAR}, /* eFISTED */
  { "Wait", event_countdown, EVENT_CHAR}, /* eWAIT */
  { "Turn Undead", event_countdown, EVENT_CHAR}, /* eTURN_UNDEAD */
  { "SpellBattle", event_countdown, EVENT_CHAR}, /* eSPELLBATTLE */
  { "Falling", event_falling, EVENT_CHAR}, /* eFALLING */
  { "Check Occupied", event_check_occupied, EVENT_ROOM}, /* eCHECK_OCCUPIED */
  { "Tracks", event_tracks, EVENT_ROOM}, /* eTRACKS */
  { "Wild Shape", event_daily_use_cooldown, EVENT_CHAR}, /* eWILD_SHAPE */
  { "Shield Recovery", event_countdown, EVENT_CHAR}, /* eSHIELD_RECOVERY */
  { "Combat Round", event_combat_round, EVENT_CHAR}, /* eCOMBAT_ROUND */
  { "Standard Action Cooldown", event_action_cooldown, EVENT_CHAR}, /* eSTANDARDACTION */
  { "Move Action Cooldown", event_action_cooldown, EVENT_CHAR}, /* eMOVEACTION */
  { "Wholeness of Body", event_countdown, EVENT_CHAR}, // eWHOLENESSOFBODY
  { "Empty Body", event_countdown, EVENT_CHAR}, // eEMPTYBODY
  { "Quivering Palm", event_daily_use_cooldown, EVENT_CHAR}, //eQUIVERINGPALM
  { "Swift Action Cooldown", event_action_cooldown, EVENT_CHAR}, // eSWIFTACTION
  { "Trap Triggered", event_trap_triggered, EVENT_CHAR}, // eTRAPTRIGGERED */
  { "Suprise Accuracy", event_countdown, EVENT_CHAR}, //eSUPRISE_ACCURACY
  { "Powerful Blow", event_countdown, EVENT_CHAR}, //ePOWERFUL_BLOW
  { "Renewed Vigor", event_countdown, EVENT_CHAR}, // eRENEWEDVIGOR
  { "Come and Get Me!", event_countdown, EVENT_CHAR}, //eCOME_AND_GET_ME
  { "Animate Dead", event_daily_use_cooldown, EVENT_CHAR}, //eANIMATEDEAD
};

/* init_events() is the ideal function for starting global events. This
 * might be the case if you were to move the contents of heartbeat() into
 * the event system */
void init_events(void) {
  /* Allocate Event List */
  world_events = create_list();
}

/* The bottom switch() is for any post-event actions, like telling the character they can
 * now access their skill again.
 */
EVENTFUNC(event_countdown) {
  struct mud_event_data *pMudEvent = NULL;
  struct char_data *ch = NULL;
  struct room_data *room = NULL;
  room_vnum *rvnum;
  room_rnum rnum = NOWHERE;

  pMudEvent = (struct mud_event_data *) event_obj;

  if (!pMudEvent)
    return 0;

  if (!pMudEvent->iId)
    return 0;

  switch (mud_event_index[pMudEvent->iId].iEvent_Type) {
    case EVENT_CHAR:
      ch = (struct char_data *) pMudEvent->pStruct;
      break;
    case EVENT_ROOM:
      rvnum = (room_vnum *) pMudEvent->pStruct;
      rnum = real_room(*rvnum);
      room = &world[real_room(rnum)];
      break;
    default:
      break;
  }

  switch (pMudEvent->iId) {
    case eC_ANIMAL:
      send_to_char(ch, "You are now able to 'call companion' again.\r\n");
      break;
    case eC_FAMILIAR:
      send_to_char(ch, "You are now able to 'call familiar' again.\r\n");
      break;
    case eC_MOUNT:
      send_to_char(ch, "You are now able to 'call mount' again.\r\n");
      break;
    case eCRYSTALBODY:
      send_to_char(ch, "You are now able to use crystal body again.\r\n");
      break;
    case eCRYSTALFIST:
      send_to_char(ch, "You are now able to use crystal fist again.\r\n");
      break;
    case eDARKNESS:
      REMOVE_BIT_AR(ROOM_FLAGS(rnum), ROOM_DARK);
      send_to_room(rnum, "The dark shroud disappates.\r\n");
      break;
    case eD_ROLL:
      send_to_char(ch, "You are now able to 'defensive roll' again.\r\n");
      break;
    case eDRAGONKNIGHT:
      send_to_char(ch, "You are now able to cast Dragon Knight again.\r\n");
      break;
    case eEPICMAGEARMOR:
      send_to_char(ch, "You are now able to cast Epic Mage Armor again.\r\n");
      break;
    case eEPICWARDING:
      send_to_char(ch, "You are now able to cast Epic Warding again.\r\n");
      break;
    case eFISTED:
      send_to_char(ch, "The magic fist holding you in place dissolves into nothingness.\r\n");
      break;
    case eGREATERRUIN:
      send_to_char(ch, "You are now able to cast Greater Ruin again.\r\n");
      break;
    case eHELLBALL:
      send_to_char(ch, "You are now able to cast Hellball again.\r\n");
      break;
    case eLAYONHANDS:
      send_to_char(ch, "You are now able to lay on hands again.\r\n");
      break;
    case eMAGIC_FOOD:
      send_to_char(ch, "You feel able to eat magical food again.\r\n");
      break;
    case eMUMMYDUST:
      send_to_char(ch, "You are now able to cast Mummy Dust again.\r\n");
      break;
    case ePERFORM:
      send_to_char(ch, "You are once again prepared to perform.\r\n");
      break;
    case ePURGEMOB:
      send_to_char(ch, "You must return to your home plane!\r\n");
      act("With a sigh of relief $n fades out of this plane!",
              FALSE, ch, NULL, NULL, TO_ROOM);
      extract_char(ch);
      break;
    case ePURIFY:
      send_to_char(ch, "You are now able to 'purify' again.\r\n");
      break;
    case eRAGE:
      send_to_char(ch, "You are now able to Rage again.\r\n");
      break;
    case eSMITE:
      send_to_char(ch, "You are once again prepared to smite your foe.\r\n");
      break;
    case eSTUNNED:
      send_to_char(ch, "You are now free from the stunning affect.\r\n");
      break;
    case eSUPRISE_ACCURACY:
      send_to_char(ch, "You are now able to use suprise accuracy again.\r\n");
      break;
    case eCOME_AND_GET_ME:
      send_to_char(ch, "You are now able to use 'come and get me' again.\r\n");
      break;
    case ePOWERFUL_BLOW:
      send_to_char(ch, "You are now able to use powerful blow again.\r\n");
      break;
    case eANIMATEDEAD:
      send_to_char(ch, "You are now able to animate dead again.\r\n");
      break;
    case eSTUNNINGFIST:
      send_to_char(ch, "You are now able to strike with your stunning fist again.\r\n");
      break;
    case eQUIVERINGPALM:
      send_to_char(ch, "You are now able to strike with your quivering palm again.\r\n");
      break;
    case eTAUNT:
      send_to_char(ch, "You are now able to taunt again.\r\n");
      break;
    case eTAUNTED:
      send_to_char(ch, "You feel the effects of the taunt wear off.\r\n");
      break;
   case eEMPTYBODY:
      send_to_char(ch, "You are now able to use Empty Body again.\r\n");
      break;
   case eWHOLENESSOFBODY:
      send_to_char(ch, "You are now able to use Wholeness of Body again.\r\n");
      break;
   case eRENEWEDVIGOR:
      send_to_char(ch, "You are now able to use Renewed Vigor again.\r\n");
      break;
    case eTREATINJURY:
      send_to_char(ch, "You are now able to treat injuries again.\r\n");
      break;
    case eWAIT:
      send_to_char(ch, "You are able to act again.\r\n");
      break;
    case eTURN_UNDEAD:
      send_to_char(ch, "You are able to turn undead again.\r\n");
      break;
    case eSPELLBATTLE:
      send_to_char(ch, "You are able to use spellbattle again.\r\n");
      SPELLBATTLE(ch) = 0;
      break;
    default:
      break;
  }

  return 0;
}

EVENTFUNC(event_daily_use_cooldown) {
  struct mud_event_data *pMudEvent = NULL;
  struct char_data *ch = NULL;
  int cooldown = 0;
  int uses = 0;
  int featnum = 0;
  char buf[128];

  pMudEvent = (struct mud_event_data *) event_obj;

  if (!pMudEvent)
    return 0;

  if (!pMudEvent->iId)
    return 0;

  switch (mud_event_index[pMudEvent->iId].iEvent_Type) {
    case EVENT_CHAR:
      ch = (struct char_data *) pMudEvent->pStruct;
      break;
    default:
      return 0;
  }

  if (pMudEvent->sVariables == NULL) {
    /* This is odd - This field should always be populated for daily-use abilities,
     * maybe some legacy code or bad id. */
    log("SYSERR: sVariables field is NULL for daily-use-cooldown-event: %d", pMudEvent->iId);
  } else {
    if(sscanf(pMudEvent->sVariables, "uses:%d", &uses) != 1) {
      log("SYSERR: In daily_uses_remaining, bad sVariables for dauly-use-cooldown-event: %d", pMudEvent->iId);
      uses = 0;
    }
  }

  switch (pMudEvent->iId) {
    case eQUIVERINGPALM:
      featnum = FEAT_QUIVERING_PALM;
      send_to_char(ch, "You are now able to strike with your quivering palm again.\r\n");
      break;
    case eSTUNNINGFIST:
      featnum = FEAT_STUNNING_FIST;
      send_to_char(ch, "You are now able to strike with your stunning fist again.\r\n");
      break;
    case eANIMATEDEAD:
      featnum = FEAT_ANIMATE_DEAD;
      send_to_char(ch, "You are now able to animate dead again.\r\n");
      break;
    case eWILD_SHAPE:
      featnum = FEAT_WILD_SHAPE;
      send_to_char(ch, "You may assume your wild shape again.\r\n");
      break;
    case eCRYSTALBODY:
      featnum = FEAT_CRYSTAL_BODY;
      send_to_char(ch, "You may harden your crystalline body again.\r\n");
      break;
    case eCRYSTALFIST:
      featnum = FEAT_CRYSTAL_FIST;
      send_to_char(ch, "You may enhance your unarmed attacks again.\r\n");
      break;
    default:
      break;
  }

  uses -= 1;
  if (uses > 0) {
    if(pMudEvent->sVariables != NULL)
      free(pMudEvent->sVariables);

    sprintf(buf, "uses:%d", uses);
    pMudEvent->sVariables = strdup(buf);
    cooldown = (SECS_PER_MUD_DAY/get_daily_uses(ch, featnum)) RL_SEC;
  }


  return cooldown;

}

/* As of 3.63, there are only global, descriptor, and character events. This
 * is due to the potential scope of the necessary debugging if events were
 * included with rooms, objects, spells or any other structure type. Adding
 * events to these other systems should be just as easy as adding the current
 * library was, and should be available in a future release. - Vat */
void attach_mud_event(struct mud_event_data *pMudEvent, long time) {
  struct event * pEvent = NULL;
  struct descriptor_data * d = NULL;
  struct char_data * ch = NULL;
  struct room_data * room = NULL;
  room_vnum *rvnum = NULL;

  pEvent = event_create(mud_event_index[pMudEvent->iId].func, pMudEvent, time);
  pEvent->isMudEvent = TRUE;
  pMudEvent->pEvent = pEvent;

  switch (mud_event_index[pMudEvent->iId].iEvent_Type) {
    case EVENT_WORLD:
      add_to_list(pEvent, world_events);
      break;
    case EVENT_DESC:
      d = (struct descriptor_data *) pMudEvent->pStruct;
      add_to_list(pEvent, d->events);
      break;
    case EVENT_CHAR:
      ch = (struct char_data *) pMudEvent->pStruct;

      if (ch->events == NULL)
        ch->events = create_list();

      add_to_list(pEvent, ch->events);
      break;
    case EVENT_ROOM:

      CREATE(rvnum, room_vnum, 1);
      *rvnum = *((room_vnum *) pMudEvent->pStruct);
      pMudEvent->pStruct = rvnum;
      room = &world[real_room(*rvnum)];

//      log("[DEBUG] Adding Event %s to room %d",mud_event_index[pMudEvent->iId].event_name, room->number);

      if (room->events == NULL)
        room->events = create_list();

      add_to_list(pEvent, room->events);
      break;
  }
}

struct mud_event_data *new_mud_event(event_id iId, void *pStruct, char *sVariables) {
  struct mud_event_data *pMudEvent = NULL;
  char *varString = NULL;

  CREATE(pMudEvent, struct mud_event_data, 1);
  varString = (sVariables != NULL) ? strdup(sVariables) : NULL;

  pMudEvent->iId = iId;
  pMudEvent->pStruct = pStruct;
  pMudEvent->sVariables = varString;
  pMudEvent->pEvent = NULL;

  return (pMudEvent);
}

void free_mud_event(struct mud_event_data *pMudEvent) {
  struct descriptor_data * d = NULL;
  struct char_data * ch = NULL;
  struct room_data * room = NULL;
  room_vnum *rvnum = NULL;

  switch (mud_event_index[pMudEvent->iId].iEvent_Type) {
    case EVENT_WORLD:
      remove_from_list(pMudEvent->pEvent, world_events);
      break;
    case EVENT_DESC:
      d = (struct descriptor_data *) pMudEvent->pStruct;
      remove_from_list(pMudEvent->pEvent, d->events);
      break;
    case EVENT_CHAR:
      ch = (struct char_data *) pMudEvent->pStruct;
      remove_from_list(pMudEvent->pEvent, ch->events);

      if (ch->events && ch->events->iSize == 0) {
        free_list(ch->events);
        ch->events = NULL;
      }
      break;
    case EVENT_ROOM:
      /* Due to OLC changes, if rooms were deleted then the room we have in the event might be
       * invalid.  This entire system needs to be re-evaluated!  We should really use RNUM
       * and just get the room data ourselves.  Storing the room_data struct is asking for bad
       * news. */
      rvnum = (room_vnum *) pMudEvent->pStruct;

      room = &world[real_room(*rvnum)];

//      log("[DEBUG] Removing Event %s from room %d, which has %d events.",mud_event_index[pMudEvent->iId].event_name, room->number, (room->events == NULL ? 0 : room->events->iSize));

      free(pMudEvent->pStruct);

      remove_from_list(pMudEvent->pEvent, room->events);

      if (room->events && room->events->iSize == 0) {  /* Added the null check here. - Ornir*/
        free_list(room->events);
        room->events = NULL;
      }
      break;
  }

  if (pMudEvent->sVariables != NULL)
    free(pMudEvent->sVariables);

  pMudEvent->pEvent->event_obj = NULL;
  free(pMudEvent);
}

struct mud_event_data * char_has_mud_event(struct char_data * ch, event_id iId) {
  struct event * pEvent = NULL;
  struct mud_event_data * pMudEvent = NULL;
  bool found = FALSE;
  struct iterator_data it;

  if (ch->events == NULL)
    return NULL;

  if (ch->events->iSize == 0)
    return NULL;

  /*
  simple_list(NULL);
  while ((pEvent = (struct event *) simple_list(ch->events)) != NULL) {
    if (!pEvent->isMudEvent)
      continue;
    pMudEvent = (struct mud_event_data *) pEvent->event_obj;
    if (pMudEvent->iId == iId) {
      found = TRUE;
      break;
    }
  }
  simple_list(NULL);
  */
  
  for( pEvent = (struct event *) merge_iterator(&it, ch->events);
       pEvent != NULL;
       pEvent = next_in_list(&it)) {
    if (!pEvent->isMudEvent)
      continue;
    pMudEvent = (struct mud_event_data *) pEvent->event_obj;
    if (pMudEvent->iId == iId) {
      found = TRUE;
      break;
    }
  }
  remove_iterator(&it);

  if (found)
    return (pMudEvent);

  return NULL;
}

struct mud_event_data *room_has_mud_event(struct room_data *rm, event_id iId) {
  struct event * pEvent = NULL;
  struct mud_event_data * pMudEvent = NULL;
  bool found = FALSE;

  if (rm->events == NULL)
    return NULL;

  if (rm->events->iSize == 0)
    return NULL;

  simple_list(NULL);

  while ((pEvent = (struct event *) simple_list(rm->events)) != NULL) {
    if (!pEvent->isMudEvent)
      continue;
    pMudEvent = (struct mud_event_data *) pEvent->event_obj;
    if (pMudEvent->iId == iId) {
      found = TRUE;
      break;
    }
  }

  simple_list(NULL);

  if (found)
    return (pMudEvent);

  return NULL;
}

void event_cancel_specific(struct char_data *ch, event_id iId) {
  struct event * pEvent;
  struct mud_event_data * pMudEvent = NULL;
  bool found = FALSE;
  struct iterator_data it;


  if (ch->events == NULL) {
    act("ch->events == NULL, for $n.", FALSE, ch, NULL, NULL, TO_ROOM);
    send_to_char(ch, "ch->events == NULL.\r\n");
    return;
  }

  if (ch->events->iSize == 0) {
    act("ch->events->iSize == 0, for $n.", FALSE, ch, NULL, NULL, TO_ROOM);
    send_to_char(ch, "ch->events->iSize == 0.\r\n");
    return;
  }

  /* need to clear simple lists */
  /*
  simple_list(NULL);
  act("Clearing simple list for $n.", FALSE, ch, NULL, NULL, TO_ROOM);
  send_to_char(ch, "Clearing simple list.\r\n");
  */
  for( pEvent = (struct event *) merge_iterator(&it, ch->events);
       pEvent != NULL;
       pEvent = next_in_list(&it)) {
    if (!pEvent->isMudEvent)
      continue;
    pMudEvent = (struct mud_event_data *) pEvent->event_obj;
    if (pMudEvent->iId == iId) {
      found = TRUE;
      break;
    }
  }
  remove_iterator(&it);

  /* fill simple_list with ch's events, use it to try and find event ID */
  /*
  while ((pEvent = (struct event *) simple_list(ch->events)) != NULL) {
    if (!pEvent->isMudEvent)
      continue;
    pMudEvent = (struct mud_event_data *) pEvent->event_obj;
    if (pMudEvent->iId == iId) {
      found = TRUE;
      break;
    }
  }
  */

  /* need to clear simple lists */
  /*
  simple_list(NULL);
  act("Clearing simple list for $n, 2nd time.", FALSE, ch, NULL, NULL, TO_ROOM);
  send_to_char(ch, "Clearing simple list, 2nd time.\r\n");
  */

  if (found) {
    act("event found for $n, attempting to cancel", FALSE, ch, NULL, NULL, TO_ROOM);
    send_to_char(ch, "Event found: %d.\r\n", iId);
    event_cancel(pEvent);
  } else {
    act("event_cancel_specific did not find an event for $n.", FALSE, ch, NULL, NULL, TO_ROOM);
    send_to_char(ch, "event_cancel_specific did not find an event.\r\n");
  }

  return;
}

void clear_char_event_list(struct char_data * ch) {
  struct event * pEvent = NULL;
  struct iterator_data it;


  if (ch->events == NULL)
    return;

  if (ch->events->iSize == 0)
    return;

  /* This uses iterators because we might be in the middle of another
   * function using simple_list, and that method requires that we do not use simple_list again
   * on another list -> It generates unpredictable results.  Iterators are safe. */
  for( pEvent = (struct event *) merge_iterator(&it, ch->events);
       pEvent != NULL;
       pEvent = next_in_list(&it)) {
    /* Here we have an issue - If we are currently executing an event, and it results in a char
     * having their events cleared (death) then we must be sure that we don't clear the executing
     * event!  Doing so will crash the event system. */

    if(event_is_queued(pEvent))
      event_cancel(pEvent);
    else if (ch->events->iSize == 1)
      break;
  }
  remove_iterator(&it);
}

void clear_room_event_list(struct room_data *rm) {
  struct event * pEvent = NULL;

  if (rm->events == NULL)
    return;

  if (rm->events->iSize == 0)
    return;

  simple_list(NULL);

  while ((pEvent = (struct event *) simple_list(rm->events)) != NULL) {
    if (event_is_queued(pEvent))
      event_cancel(pEvent);
  }

  simple_list(NULL);

}

/* ripley's version of change_event_duration
 * a function to adjust the event time of a given event
 */
void change_event_duration(struct char_data * ch, event_id iId, long time) {
  struct event *pEvent = NULL;
  struct mud_event_data *pMudEvent = NULL;
  bool found = FALSE;

  if (ch->events->iSize == 0)
    return;

  simple_list(NULL);

  while ((pEvent = (struct event *) simple_list(ch->events)) != NULL) {

    if (!pEvent->isMudEvent)
      continue;

    pMudEvent = (struct mud_event_data *) pEvent->event_obj;

    if (pMudEvent->iId == iId) {
      found = TRUE;
      break;
    }
  }

  simple_list(NULL);

  if (found) {
    /* So we found the offending event, now build a new one, with the new time */
    attach_mud_event(new_mud_event(iId, pMudEvent->pStruct, pMudEvent->sVariables), time);
    if (event_is_queued(pEvent))
      event_cancel(pEvent);
  }

}

/*  this is vatiken's version, above is ripley's version */
/*
 void change_event_duration(struct char_data * ch, event_id iId, long time) {
  struct event *pEvent = NULL;
  struct mud_event_data *pMudEvent = NULL;
  bool found = FALSE;

  if (ch->events == NULL);
    return;

  if (ch->events->iSize == 0)
    return;

  clear_simple_list();

  while ((pEvent = (struct event *) simple_list(ch->events)) != NULL) {

    if (!pEvent->isMudEvent)
      continue;

    pMudEvent = (struct mud_event_data *) pEvent->event_obj;

    if (pMudEvent->iId == iId) {
      found = TRUE;
      break;
    }
  }

  if (found) {
    // So we found the offending event, now build a new one, with the new time
    attach_mud_event(new_mud_event(iId, pMudEvent->pStruct, pMudEvent->sVariables), time);
    event_cancel(pEvent);
  }

}
 */

