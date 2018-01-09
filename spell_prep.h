/*/ \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \
\                                                             
/    Luminari Spell Prep System
/  Created By: Zusuk                                                           
\    File:     spell_prep.h                                                           
/    Handling spell preparation for all casting classes, memorization                                                           
\    system, queue, related commands, etc
/  Created on January 8, 2018, 3:27 PM                                                                                                                                                                                     
\ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ / \ /*/

#pragma once

#ifdef	__cplusplus
extern "C" {
#endif
    
    /** START structs **/
    
    /* the structure for spells related to the prep system
       is in structs.h: prep_collection_spell_data */
    
    /** END structs **/
    
    /** START defines **/
    
    /* assuming wizard as our standard, this is the base mem time for a 1st
     * circle spell without any bonuses*/
    #define BASE_PREP_TIME  7
    /* this is the value added to a circle's prep time to calculate next circle's
     * prep time, example: 7 second for 1st circle + this-interval = 2nd circle
     * preparation time. */
    #define PREP_TIME_INTERVALS 2
    /* preparation times are modified by this factor, control knobs we will call
       them to easily adjust preparation time for spell */
    #define RANGER_PREP_TIME_FACTOR   7
    #define PALADIN_PREP_TIME_FACTOR  7
    #define DRUID_PREP_TIME_FACTOR    4
    #define WIZ_PREP_TIME_FACTOR      2
    #define CLERIC_PREP_TIME_FACTOR   4
    #define SORC_PREP_TIME_FACTOR     5
    #define BARD_PREP_TIME_FACTOR     6
    
    /* macros */
    
    /* char's pointer to their spell prep queue (head) */
    #define SPELL_PREP_QUEUE(ch, ch_class) (ch->player_specials->saved.preparation_queue[ch_class])
    /* spellnum of a prep-queue top item (head) */
    #define PREP_QUEUE_ITEM_SPELLNUM(ch, ch_class) (ch->player_specials->saved.preparation_queue[ch_class]->spell)
    
    /* char's pointer to their spell collection */
    #define SPELL_COLLECTION(ch, ch_class) (ch->player_specials->saved.spell_collection[ch_class])
    /* spellnum of a collection top item (head) */
    #define COLLECTIONE_ITEM_SPELLNUM(ch, ch_class) (ch->player_specials->saved.spell_collection[ch_class]->spell)
    
    /** END defines **/
    
    /** START functions **/
    
    void init_ch_spell_prep_queue(struct char_data *ch);
    void destroy_ch_spell_prep_queue(struct char_data *ch);
    void load_ch_spell_prep_queue();
    void save_ch_spell_prep_queue();  
    void print_prep_queue(struct char_data *ch, int ch_class);
    int is_spell_in_prep_queue(struct char_data *ch, int spell_num);
    struct prep_collection_spell_data *create_prep_queue_entry(int spell,
            int ch_class, int metamagic, int prep_time);
    struct prep_collection_spell_data *spell_to_prep_queue(struct char_data *ch,
            int spell, int ch_class, int metamagic,  int prep_time);
    struct prep_collection_spell_data *spell_from_prep_queue(struct char_data *ch,
            int spell, int ch_class);
    void init_ch_spell_collection(struct char_data *ch);
    void destroy_ch_spell_collection(struct char_data *ch);
    void load_ch_spell_collection(struct char_data *ch);
    void save_ch_spell_collection(struct char_data *ch);  
    int is_spell_in_collection(struct char_data *ch, int spell_num);
    struct prep_collection_spell_data *create_collection_entry(int spell,
            int ch_class, int metamagic, int prep_time);
    struct prep_collection_spell_data *spell_to_collection(struct char_data *ch,
        int spell, int ch_class, int metamagic,  int prep_time);
    struct prep_collection_spell_data *spell_from_collection(struct char_data *ch,
            int spell, int ch_class);
    bool item_from_queue_to_collection(struct char_data *ch, int spell);  
    int compute_spells_prep_time(struct char_data *ch, int spellnum, int class,
            int circle);
    int compute_spells_circle(int spellnum, int class, int metamagic);

    /** END functions **/
    
    /** Start ACMD **/
    /** End ACMD **/
    
    /* work space / references */
    /*
// spell preparation queue and collection (prepared spells))
// this refers to items in the list of spells the ch is trying to prepare 
#define PREPARATION_QUEUE(ch, slot, cc)	(ch->player_specials->saved.prep_queue[slot][cc])
//this refers to preparation-time in a list that parallels the preparation_queue 
//    OLD system, this can be phased out  
#define PREP_TIME(ch, slot, cc)	(ch->player_specials->saved.prep_queue[slot][cc].prep_time)
// this refers to items in the list of spells the ch already has prepared (collection) 
#define PREPARED_SPELLS(ch, slot, cc)	(ch->player_specials->saved.collection[slot][cc])
#define SPELL_COLLECTION(ch, slot, cc)  PREPARED_SPELLS(ch, slot, cc)
// given struct entry, this is the appropriate class for this spell in relation to queue/collection  
#define PREP_CLASS(ch, slot, cc)	(ch->player_specials->saved.prep_queue[slot][cc].ch_class)
// bitvector of metamagic affecting this spell  
#define PREP_METAMAGIC(ch, slot, cc)	(ch->player_specials->saved.prep_queue[slot][cc].metamagic)
     */



#ifdef	__cplusplus
}
#endif


/*EOF*/
