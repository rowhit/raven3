/**************************************************************************
*  File: medit.c                                           Part of tbaMUD *
*  Usage: Oasis OLC - Mobiles.                                            *
*                                                                         *
* Copyright 1996 Harvey Gilpin. 1997-2001 George Greer.                   *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "comm.h"
#include "spells.h"
#include "db.h"
#include "shop.h"
#include "genolc.h"
#include "genmob.h"
#include "genzon.h"
#include "genshp.h"
#include "oasis.h"
#include "handler.h"
#include "constants.h"
#include "improved-edit.h"
#include "dg_olc.h"
#include "screen.h"
#include "fight.h"
#include "modify.h"      /* for smash_tilde */
#include "class.h"
#include "race.h"

/* local functions */
static void medit_setup_new(struct descriptor_data *d);
static void init_mobile(struct char_data *mob);
static void medit_save_to_disk(zone_vnum zone_num);
static void medit_disp_positions(struct descriptor_data *d);
static void medit_disp_sex(struct descriptor_data *d);
static void medit_disp_attack_types(struct descriptor_data *d);
static void medit_disp_mob_flags(struct descriptor_data *d);
static void medit_disp_aff_flags(struct descriptor_data *d);
static void medit_disp_menu(struct descriptor_data *d);
static void medit_disp_race_selection(struct descriptor_data *d);
static void medit_disp_subrace_selection(struct descriptor_data *d);
static void medit_disp_class_selection(struct descriptor_data *d);

#define NUM_ILLEGAL_MOB_FLAGS 9
/* Illegal mob flags */
static const int illegal_mob_flags[NUM_ILLEGAL_MOB_FLAGS] = {
    MOB_ISNPC,
    MOB_CONJURED,
    MOB_CLONED,
    MOB_UNUSED1,
    MOB_UNUSED2,
    MOB_UNUSED3,
    MOB_NOTDEADYET,
    MOB_UNUSED4,
    MOB_UNUSED5,
};


#define NUM_ILLEGAL_AFF_FLAGS 13
/* Illegal mob affects */
static const int illegal_aff_flags[NUM_ILLEGAL_AFF_FLAGS] = {
    AFF_DONTUSE,
    AFF_GROUP,
    AFF_POISON,
    AFF_CHARM,
    AFF_PLAGUE,
    AFF_UNUSED1,
    AFF_UNUSED2,
    AFF_UNUSED3,
    AFF_PARALYZE,
    AFF_ASSISTANT,
    AFF_FORTIFY,
    AFF_UNUSED4,
    AFF_UNUSED5,
};


/*  utility functions */
ACMD(do_oasis_medit)
{
  int number = NOBODY, save = 0, real_num, all = 0;
  struct descriptor_data *d;
  char *buf3;
  char buf1[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];

  /* No building as a mob or while being forced. */
  if (IS_NPC(ch) || !ch->desc || STATE(ch->desc) != CON_PLAYING)
    return;

  /* Parse any arguments */
  buf3 = two_arguments(argument, buf1, buf2);

  if (!*buf1) {
    send_to_char(ch, "Specify a mobile VNUM to edit.\r\n");
    return;
  } else if (!isdigit(*buf1)) {
    if (str_cmp("save", buf1) != 0) {
      send_to_char(ch, "Yikes!  Stop that, someone will get hurt!\r\n");
      return;
    }

    save = TRUE;

    if (is_number(buf2))
      number = atoi(buf2);
    else if (GET_OLC_ZONE(ch) > 0) {
      zone_rnum zlok;

      if ((zlok = real_zone(GET_OLC_ZONE(ch))) == NOWHERE)
        number = NOWHERE;
      else
        number = genolc_zone_bottom(zlok);
    } else if(GET_LEVEL(ch) >= LVL_GRGOD && is_abbrev(buf2, "all")) {
      all = TRUE;
    }

    if (number == NOWHERE && !all) {
      send_to_char(ch, "Save which zone?\r\n");
      return;
    }
  }

  /* If a numeric argument was given (like a room number), get it. */
  if (number == NOBODY && !all)
    number = atoi(buf1);

  /* Check that whatever it is isn't already being edited. */
  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) == CON_MEDIT) {
      if (d->olc && OLC_NUM(d) == number) {
        send_to_char(ch, "That mobile is currently being edited by %s.\r\n",
          GET_NAME(d->character));
        return;
      }
    }
  }

  d = ch->desc;

  /* Give descriptor an OLC structure. */
  if (d->olc) {
    mudlog(BRF, LVL_IMMORT, TRUE,
      "SYSERR: do_oasis_medit: Player already had olc structure.");
    free(d->olc);
  }

  CREATE(d->olc, struct oasis_olc_data, 1);

  /* Find the zone. */
  OLC_ZNUM(d) = save ? real_zone(number) : real_zone_by_thing(number);
  if (OLC_ZNUM(d) == NOWHERE && !all) {
    send_to_char(ch, "Sorry, there is no zone for that number!\r\n");
    free(d->olc);
    d->olc = NULL;
    return;
  }

  /* Everyone but IMPLs can only edit zones they have been assigned. */
  if (!can_edit_zone(ch, OLC_ZNUM(d)) && !all) {
    send_cannot_edit(ch, zone_table[OLC_ZNUM(d)].number);
    /* Free the OLC structure. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  /* If save is TRUE, save the mobiles. */
  if (save) {
    zone_rnum iZone = 0, zStart = 0, zEnd = top_of_zone_table + 1;

    if(!all) {
      zStart = OLC_ZNUM(d);
      zEnd = OLC_ZNUM(d) + 1;
    }
    for(iZone = zStart; iZone < zEnd; iZone++) {
      OLC_ZNUM(d) = iZone;
      send_to_char(ch, "Saving all mobiles in zone %d.\r\n",
          zone_table[OLC_ZNUM(d)].number);
      mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE,
          "OLC: %s saves mobile info for zone %d.",
          GET_NAME(ch), zone_table[OLC_ZNUM(d)].number);

      /* Save the mobiles. */
      save_mobiles(OLC_ZNUM(d));
    }

    /* Free the olc structure stored in the descriptor. */
    free(d->olc);
    d->olc = NULL;
    return;
  }

  OLC_NUM(d) = number;

  /* If this is a new mobile, setup a new one, otherwise, setup the
     existing mobile. */
  if ((real_num = real_mobile(number)) == NOBODY)
    medit_setup_new(d);
  else
    medit_setup_existing(d, real_num);

  medit_disp_menu(d);
  STATE(d) = CON_MEDIT;

  /* Display the OLC messages to the players in the same room as the
     builder and also log it. */
  act("$n starts using OLC.", TRUE, d->character, 0, 0, TO_ROOM);
  SET_BIT_AR(PLR_FLAGS(ch), PLR_WRITING);

  mudlog(CMP, LVL_IMMORT, TRUE,"OLC: %s starts editing zone %d allowed zone %d",
    GET_NAME(ch), zone_table[OLC_ZNUM(d)].number, GET_OLC_ZONE(ch));
}

static void medit_save_to_disk(zone_vnum foo)
{
  save_mobiles(real_zone(foo));
}

static void medit_setup_new(struct descriptor_data *d)
{
  struct char_data *mob;

  /* Allocate a scratch mobile structure. */
  CREATE(mob, struct char_data, 1);

  init_mobile(mob);

  GET_MOB_RNUM(mob) = NOBODY;
  /* Set up some default strings. */
  GET_ALIAS(mob) = strdup("mob unfinished");
  GET_SDESC(mob) = strdup("the unfinished mob");
  GET_LDESC(mob) = strdup("An unfinished mob stands here.\r\n");
  GET_DDESC(mob) = strdup("It looks unfinished.\r\n");
  SCRIPT(mob) = NULL;
  mob->proto_script = OLC_SCRIPT(d) = NULL;

  OLC_MOB(d) = mob;
  /* Has changed flag. (It hasn't so far, we just made it.) */
  OLC_VAL(d) = FALSE;
  OLC_ITEM_TYPE(d) = MOB_TRIGGER;
}

void medit_setup_existing(struct descriptor_data *d, int rmob_num)
{
  struct char_data *mob;

  /* Allocate a scratch mobile structure. */
  CREATE(mob, struct char_data, 1);

  copy_mobile(mob, mob_proto + rmob_num);

  OLC_MOB(d) = mob;
  OLC_ITEM_TYPE(d) = MOB_TRIGGER;
  dg_olc_script_copy(d);
  /*
   * The edited mob must not have a script.
   * It will be assigned to the updated mob later, after editing.
   */
  SCRIPT(mob) = NULL;
  OLC_MOB(d)->proto_script = NULL;
}

/* Ideally, this function should be in db.c, but I'll put it here for portability. */
static void init_mobile(struct char_data *mob)
{
  clear_char(mob);

  GET_HIT(mob) = GET_MANA(mob) = 1;
  GET_MAX_MANA(mob) = GET_MAX_MOVE(mob) = 100;
  GET_NDD(mob) = GET_SDD(mob) = 1;
  GET_WEIGHT(mob) = 200;
  GET_HEIGHT(mob) = 198;

  mob->real_abils.str = mob->real_abils.intel = mob->real_abils.wis = 11;
  mob->real_abils.dex = mob->real_abils.con = mob->real_abils.cha = 11;
  mob->aff_abils = mob->real_abils;

  GET_SAVE(mob, SAVING_PARA)   = 0;
  GET_SAVE(mob, SAVING_ROD)    = 0;
  GET_SAVE(mob, SAVING_PETRI)  = 0;
  GET_SAVE(mob, SAVING_BREATH) = 0;
  GET_SAVE(mob, SAVING_SPELL)  = 0;

  SET_BIT_AR(MOB_FLAGS(mob), MOB_ISNPC);
  mob->player_specials = &dummy_mob;
}

/* Save new/edited mob to memory. */
void medit_save_internally(struct descriptor_data *d)
{
  int i;
  mob_rnum new_rnum;
  struct descriptor_data *dsc;
  struct char_data *mob;

  i = (real_mobile(OLC_NUM(d)) == NOBODY);

  if ((new_rnum = add_mobile(OLC_MOB(d), OLC_NUM(d))) == NOBODY) {
    log("medit_save_internally: add_mobile failed.");
    return;
  }

  /* Update triggers and free old proto list */
  if (mob_proto[new_rnum].proto_script &&
      mob_proto[new_rnum].proto_script != OLC_SCRIPT(d))
    free_proto_script(&mob_proto[new_rnum], MOB_TRIGGER);

  mob_proto[new_rnum].proto_script = OLC_SCRIPT(d);

  /* this takes care of the mobs currently in-game */
  for (mob = character_list; mob; mob = mob->next) {
    if (GET_MOB_RNUM(mob) != new_rnum)
      continue;

    /* remove any old scripts */
    if (SCRIPT(mob))
      extract_script(mob, MOB_TRIGGER);

    free_proto_script(mob, MOB_TRIGGER);
    copy_proto_script(&mob_proto[new_rnum], mob, MOB_TRIGGER);
    assign_triggers(mob, MOB_TRIGGER);
  }
  /* end trigger update */

  if (!i)	/* Only renumber on new mobiles. */
    return;

  /* Update keepers in shops being edited and other mobs being edited. */
  for (dsc = descriptor_list; dsc; dsc = dsc->next) {
    if (STATE(dsc) == CON_SEDIT)
      S_KEEPER(OLC_SHOP(dsc)) += (S_KEEPER(OLC_SHOP(dsc)) != NOTHING && S_KEEPER(OLC_SHOP(dsc)) >= new_rnum);
    else if (STATE(dsc) == CON_MEDIT)
      GET_MOB_RNUM(OLC_MOB(dsc)) += (GET_MOB_RNUM(OLC_MOB(dsc)) != NOTHING && GET_MOB_RNUM(OLC_MOB(dsc)) >= new_rnum);
  }

  /* Update other people in zedit too. From: C.Raehl 4/27/99 */
  for (dsc = descriptor_list; dsc; dsc = dsc->next)
    if (STATE(dsc) == CON_ZEDIT)
      for (i = 0; OLC_ZONE(dsc)->cmd[i].command != 'S'; i++)
        if (OLC_ZONE(dsc)->cmd[i].command == 'M')
          if (OLC_ZONE(dsc)->cmd[i].arg1 >= new_rnum)
            OLC_ZONE(dsc)->cmd[i].arg1++;
}

/* Menu functions
   Display positions. (sitting, standing, etc) */
static void medit_disp_positions(struct descriptor_data *d)
{
  get_char_colors(d->character);
  clear_screen(d);
  column_list(d->character, 0, position_types, NUM_POSITIONS, TRUE);
  write_to_output(d, "Enter position number : ");
}

/* Display the gender of the mobile. */
static void medit_disp_sex(struct descriptor_data *d)
{
  get_char_colors(d->character);
  clear_screen(d);
  column_list(d->character, 0, genders, NUM_GENDERS, TRUE);
  write_to_output(d, "Enter gender number : ");
}

/* Display attack types menu. */
static void medit_disp_attack_types(struct descriptor_data *d)
{
  int i;

  get_char_colors(d->character);
  clear_screen(d);

  for (i = 0; i < NUM_ATTACK_TYPES; i++) {
    write_to_output(d, "%s%2d%s) %s\r\n", grn, i, nrm, attack_hit_text[i].singular);
  }
  write_to_output(d, "Enter attack type : ");
}

/* Display mob-flags menu. */
static void medit_disp_mob_flags(struct descriptor_data *d)
{
  int i, count = 0;
  char flags[MAX_STRING_LENGTH];
  const char *allowed_act_flags[NUM_MOB_FLAGS];

  get_char_colors(d->character);
  clear_screen(d);

  /* Mob flags has special handling to remove illegal flags from the list */
  for (i = 0; i < NUM_MOB_FLAGS; i++) {
    if (is_illegal_flag(i, NUM_ILLEGAL_MOB_FLAGS, illegal_mob_flags)) continue;
    allowed_act_flags[count++] = strdup(action_bits[i]);
  }

  column_list(d->character, 4, allowed_act_flags, count, TRUE);
  sprintbitarray(MOB_FLAGS(OLC_MOB(d)), action_bits, AF_ARRAY_MAX, flags);
  write_to_output(d, "\r\nCurrent flags : %s%s%s\r\nEnter mob flags (0 to quit) : ", cyn, flags, nrm);
}

/* Display affection flags menu. */
static void medit_disp_aff_flags(struct descriptor_data *d)
{
  int i, count = 0;
  char flags[MAX_STRING_LENGTH];
  const char *allowed_mob_aff_flags[NUM_AFF_FLAGS];

  get_char_colors(d->character);
  clear_screen(d);

  for (i = 0; i < NUM_AFF_FLAGS; i++) {
    if (is_illegal_flag(i, NUM_ILLEGAL_AFF_FLAGS, illegal_aff_flags)) continue;
    allowed_mob_aff_flags[count++] = strdup(affected_bits[i]);
  }

  //pretty print to ch
  column_list(d->character, 4, allowed_mob_aff_flags, count, TRUE);
  sprintbitarray(AFF_FLAGS(OLC_MOB(d)), affected_bits, AF_ARRAY_MAX, flags);
  write_to_output(d, "\r\nCurrent flags   : %s%s%s\r\nEnter aff flags (0 to quit) : ",
                          cyn, flags, nrm);
}

/* Display main menu. */
static void medit_disp_menu(struct descriptor_data *d)
{
  struct char_data *mob;
  char flags[MAX_STRING_LENGTH], flag2[MAX_STRING_LENGTH];

  mob = OLC_MOB(d);
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
  "-- Mob Number:  [%s%d%s]\r\n"
  "%s1%s) Sex: %s%-7.7s%s	         %s2%s) Keywords: %s%s\r\n"
  "%s3%s) S-Desc: %s%s\r\n"
  "%s4%s) L-Desc:-\r\n%s%s\r\n"
  "%s5%s) D-Desc:-\r\n%s%s\r\n",

	  cyn, OLC_NUM(d), nrm,
	  grn, nrm, yel, genders[(int)GET_SEX(mob)], nrm,
	  grn, nrm, yel, GET_ALIAS(mob),
	  grn, nrm, yel, GET_SDESC(mob),
	  grn, nrm, yel, GET_LDESC(mob),
	  grn, nrm, yel, GET_DDESC(mob)
	  );

  sprintbitarray(MOB_FLAGS(mob), action_bits, AF_ARRAY_MAX, flags);
  sprintbitarray(AFF_FLAGS(mob), affected_bits, AF_ARRAY_MAX, flag2);
  write_to_output(d,
	  "%s6%s) Position  : %s%s\r\n"
	  "%s7%s) Default   : %s%s\r\n"
	  "%s8%s) Attack    : %s%s\r\n"
      "%s9%s) Stats Menu...\r\n"
	  "%sA%s) NPC Flags : %s%s\r\n"
	  "%sB%s) AFF Flags : %s%s\r\n"
          "%sS%s) Script    : %s%s\r\n"
          "%sW%s) Copy mob\r\n"
	  "%sX%s) Delete mob\r\n"
	  "%sQ%s) Quit\r\n"
	  "Enter choice : ",

	  grn, nrm, yel, position_types[(int)GET_POS(mob)],
	  grn, nrm, yel, position_types[(int)GET_DEFAULT_POS(mob)],
	  grn, nrm, yel, attack_hit_text[(int)GET_ATTACK(mob)].singular,
	  grn, nrm,
	  grn, nrm, cyn, flags,
	  grn, nrm, cyn, flag2,
          grn, nrm, cyn, OLC_SCRIPT(d) ?"Set.":"Not Set.",
          grn, nrm,
	  grn, nrm,
	  grn, nrm
	  );

  OLC_MODE(d) = MEDIT_MAIN_MENU;
}

/* Display main menu. */
static void medit_disp_stats_menu(struct descriptor_data *d)
{
  struct char_data *mob;
  char buf[MAX_STRING_LENGTH], subRaceBuf[MAX_STRING_LENGTH];

  mob = OLC_MOB(d);
  get_char_colors(d->character);
  clear_screen(d);
  
  /* set up subrace string */
  if(IS_ELEMENTAL(mob)) {
	  sprintf(subRaceBuf, "%s", ele_subrace_types[(int)GET_SUBRACE(mob)]);
  } else if(IS_DRACONIAN(mob) || IS_DRAGON(mob)) {
	  sprintf(subRaceBuf, "%s", drc_subrace_types[(int)GET_SUBRACE(mob)]);
  } else {
	  sprintf(subRaceBuf, "N/A");
  }
  
  /* Color codes have to be used here, for count_color_codes to work */
  sprintf(buf, "(range \ty%d\tn to \ty%d\tn)", GET_HIT(mob) + GET_MOVE(mob), (GET_HIT(mob) * GET_MANA(mob)) + GET_MOVE(mob));

  /* Top section - standard stats */
  write_to_output(d,
  "-- Mob Number:  %s[%s%d%s]%s\r\n"
  "(%s1%s) Level: %s[%s%15d%s]%s (%s2%s) Race:    %s[%s%15s%s]%s\r\n"
  "(%s4%s) Class: %s[%s%15s%s]%s (%s3%s) Subrace: %s[%s%15s%s]%s\r\n"
  "(%s5%s) %sAuto Set Stats (based on level)%s\r\n\r\n"
  "Hit Points  (xdy+z):        Bare Hand Damage (xdy+z): \r\n"
  "(%s6%s) HP NumDice:  %s[%s%5d%s]%s    (%s9%s) BHD NumDice:  %s[%s%5d%s]%s\r\n"
  "(%s7%s) HP SizeDice: %s[%s%5d%s]%s    (%sA%s) BHD SizeDice: %s[%s%5d%s]%s\r\n"
  "(%s8%s) HP Addition: %s[%s%5d%s]%s    (%sB%s) DamRoll:      %s[%s%5d%s]%s\r\n"
  "%-*s(range %s%d%s to %s%d%s)\r\n\r\n"

  "(%sC%s) Armor Class: %s[%s%4d%s]%s        (%sF%s) Hitroll:   %s[%s%5d%s]%s\r\n"
  "(%sD%s) Exp Points:  %s[%s%10d%s]%s  (%sG%s) Alignment: %s[%s%5d%s]%s\r\n"
  "(%sE%s) Gold:        %s[%s%10d%s]%s\r\n\r\n",
      cyn, yel, OLC_NUM(d), cyn, nrm,
      cyn, nrm, cyn, yel, GET_LEVEL(mob),  cyn, nrm, cyn, nrm, cyn, yel, pc_race_types[(int)GET_RACE(mob)],cyn, nrm,
      cyn, nrm, cyn, yel, pc_class_types[(int)GET_CLASS(mob)], cyn, nrm, 
      cyn, nrm, cyn, yel, subRaceBuf, cyn, nrm,
      cyn, nrm, cyn, nrm,
      cyn, nrm, cyn, yel, GET_HIT(mob), cyn, nrm,   cyn, nrm, cyn, yel, GET_NDD(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_MANA(mob), cyn, nrm,  cyn, nrm, cyn, yel, GET_SDD(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_MOVE(mob), cyn, nrm,  cyn, nrm, cyn, yel, GET_DAMROLL(mob), cyn, nrm,

      count_color_chars(buf)+28, buf,
      yel, GET_NDD(mob) + GET_DAMROLL(mob), nrm,
      yel, (GET_NDD(mob) * GET_SDD(mob)) + GET_DAMROLL(mob), nrm,

      cyn, nrm, cyn, yel, GET_AC(mob), cyn, nrm,   cyn, nrm, cyn, yel, GET_HITROLL(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_EXP(mob), cyn, nrm,  cyn, nrm, cyn, yel, GET_ALIGNMENT(mob), cyn, nrm,
      cyn, nrm, cyn, yel, GET_GOLD(mob), cyn, nrm
      );

  if (CONFIG_MEDIT_ADVANCED) {
    /* Bottom section - non-standard stats, togglable in cedit */
    write_to_output(d,
    "(%sH%s) Str: %s[%s%2d/%3d%s]%s   Saving Throws\r\n"
    "(%sI%s) Int: %s[%s%3d%s]%s      (%sN%s) Paralysis     %s[%s%3d%s]%s\r\n"
    "(%sJ%s) Wis: %s[%s%3d%s]%s      (%sO%s) Rods/Staves   %s[%s%3d%s]%s\r\n"
    "(%sK%s) Dex: %s[%s%3d%s]%s      (%sP%s) Petrification %s[%s%3d%s]%s\r\n"
    "(%sL%s) Con: %s[%s%3d%s]%s      (%sR%s) Breath        %s[%s%3d%s]%s\r\n"
    "(%sM%s) Cha: %s[%s%3d%s]%s      (%sS%s) Spells        %s[%s%3d%s]%s\r\n\r\n",
        cyn, nrm, cyn, yel, GET_STR(mob), GET_ADD(mob), cyn, nrm,
        cyn, nrm, cyn, yel, GET_INT(mob), cyn, nrm,   cyn, nrm, cyn, yel, GET_SAVE(mob, SAVING_PARA), cyn, nrm,
        cyn, nrm, cyn, yel, GET_WIS(mob), cyn, nrm,   cyn, nrm, cyn, yel, GET_SAVE(mob, SAVING_ROD), cyn, nrm,
        cyn, nrm, cyn, yel, GET_DEX(mob), cyn, nrm,   cyn, nrm, cyn, yel, GET_SAVE(mob, SAVING_PETRI), cyn, nrm,
        cyn, nrm, cyn, yel, GET_CON(mob), cyn, nrm,   cyn, nrm, cyn, yel, GET_SAVE(mob, SAVING_BREATH), cyn, nrm,
        cyn, nrm, cyn, yel, GET_CHA(mob), cyn, nrm,   cyn, nrm, cyn, yel, GET_SAVE(mob, SAVING_SPELL), cyn, nrm
        );
  }

  /* Quit to previous menu option */
  write_to_output(d, "(%sQ%s) Quit to main menu\r\nEnter choice : ", cyn, nrm);

  OLC_MODE(d) = MEDIT_STATS_MENU;
}

/*
 * Display all available races and prompt for an input.
 */
static void medit_disp_race_selection(struct descriptor_data *d) {
  clear_screen(d);
  column_list(d->character, 3, pc_race_types, NUM_RACES, TRUE);
  write_to_output(d,"Enter race number :");
  OLC_MODE(d) = MEDIT_RACE;
}

/*
 * Display all available subrace for the mob's race and prompt for an input.
 */
static void medit_disp_subrace_selection(struct descriptor_data *d) {
  int iSubRace, count = 0;
  const char *subrace_list[NUM_DRC_SUBRACE];

  clear_screen(d);
  //display only the subraces for the mob's race
  //start with 1 because it doesn't make sense to choose a subrace and
  //just select "None"(0).
  for(iSubRace = 1;;iSubRace++) {
    if((IS_ELEMENTAL(OLC_MOB(d)) && iSubRace >= NUM_ELE_SUBRACE)
        || (IS_DRACONIAN(OLC_MOB(d)) && iSubRace >= SUBRACE_CHROMATIC_DRAGON)
        || (IS_DRAGON(OLC_MOB(d)) && iSubRace >= NUM_DRC_SUBRACE))
      break;
    subrace_list[count++] = strdup((IS_ELEMENTAL(OLC_MOB(d)) ?
        ele_subrace_types[iSubRace] : drc_subrace_types[iSubRace]));
  }

  column_list(d->character, 3, subrace_list, count, TRUE);
  write_to_output(d,"Enter subrace number :");
  OLC_MODE(d) = MEDIT_SUBRACE;

}

/*
 * Display all available classes and prompt for input.
 */
static void medit_disp_class_selection(struct descriptor_data *d) {
  clear_screen(d);
  column_list(d->character, 3, pc_class_types, NUM_CLASSES, TRUE);
  write_to_output(d, "Enter class number :");
  OLC_MODE(d) = MEDIT_CLASS;
}

void medit_parse(struct descriptor_data *d, char *arg)
{
  int i = -1, j;
  char *oldtext = NULL;

  if (OLC_MODE(d) > MEDIT_NUMERICAL_RESPONSE) {
    i = atoi(arg);
    if (!*arg || (!isdigit(arg[0]) && ((*arg == '-') && !isdigit(arg[1])))) {
      write_to_output(d, "Try again : ");
      return;
    }
  } else {	/* String response. */
    if (!genolc_checkstring(d, arg))
      return;
  }
  switch (OLC_MODE(d)) {
  case MEDIT_CONFIRM_SAVESTRING:
    /* Ensure mob has MOB_ISNPC set. */
    SET_BIT_AR(MOB_FLAGS(OLC_MOB(d)), MOB_ISNPC);
    switch (*arg) {
    case 'y':
    case 'Y':
      /* Save the mob in memory and to disk. */
      medit_save_internally(d);
      mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(d->character)), TRUE, "OLC: %s edits mob %d", GET_NAME(d->character), OLC_NUM(d));
      if (CONFIG_OLC_SAVE) {
        medit_save_to_disk(zone_table[real_zone_by_thing(OLC_NUM(d))].number);
        write_to_output(d, "Mobile saved to disk.\r\n");
      } else
        write_to_output(d, "Mobile saved to memory.\r\n");
      cleanup_olc(d, CLEANUP_ALL);
      return;
    case 'n':
    case 'N':
      /* If not saving, we must free the script_proto list. We do so by
       * assigning it to the edited mob and letting free_mobile in
       * cleanup_olc handle it. */
      OLC_MOB(d)->proto_script = OLC_SCRIPT(d);
      cleanup_olc(d, CLEANUP_ALL);
      return;
    default:
      write_to_output(d, "Invalid choice!\r\n");
      write_to_output(d, "Do you wish to save your changes? : ");
      return;
    }
    break;

  case MEDIT_MAIN_MENU:
    i = 0;
    switch (*arg) {
    case 'q':
    case 'Q':
      if (OLC_VAL(d)) {	/* Anything been changed? */
	write_to_output(d, "Do you wish to save your changes? : ");
	OLC_MODE(d) = MEDIT_CONFIRM_SAVESTRING;
      } else
	cleanup_olc(d, CLEANUP_ALL);
      return;
    case '1':
      OLC_MODE(d) = MEDIT_SEX;
      medit_disp_sex(d);
      return;
    case '2':
      OLC_MODE(d) = MEDIT_KEYWORD;
      i--;
      break;
    case '3':
      OLC_MODE(d) = MEDIT_S_DESC;
      i--;
      break;
    case '4':
      OLC_MODE(d) = MEDIT_L_DESC;
      i--;
      break;
    case '5':
      OLC_MODE(d) = MEDIT_D_DESC;
      send_editor_help(d);
      write_to_output(d, "Enter mob description:\r\n\r\n");
      if (OLC_MOB(d)->player.description) {
	write_to_output(d, "%s", OLC_MOB(d)->player.description);
	oldtext = strdup(OLC_MOB(d)->player.description);
      }
      string_write(d, &OLC_MOB(d)->player.description, MAX_MOB_DESC, 0, oldtext);
      OLC_VAL(d) = 1;
      return;
    case '6':
      OLC_MODE(d) = MEDIT_POS;
      medit_disp_positions(d);
      return;
    case '7':
      OLC_MODE(d) = MEDIT_DEFAULT_POS;
      medit_disp_positions(d);
      return;
    case '8':
      OLC_MODE(d) = MEDIT_ATTACK;
      medit_disp_attack_types(d);
      return;
    case '9':
      OLC_MODE(d) = MEDIT_STATS_MENU;
      medit_disp_stats_menu(d);
      return;
    case 'a':
    case 'A':
      OLC_MODE(d) = MEDIT_NPC_FLAGS;
      medit_disp_mob_flags(d);
      return;
    case 'b':
    case 'B':
      OLC_MODE(d) = MEDIT_AFF_FLAGS;
      medit_disp_aff_flags(d);
      return;
    case 'w':
    case 'W':
      write_to_output(d, "Copy what mob? ");
      OLC_MODE(d) = MEDIT_COPY;
      return;
    case 'x':
    case 'X':
      write_to_output(d, "Are you sure you want to delete this mobile? ");
      OLC_MODE(d) = MEDIT_DELETE;
      return;
    case 's':
    case 'S':
      OLC_SCRIPT_EDIT_MODE(d) = SCRIPT_MAIN_MENU;
      dg_script_menu(d);
      return;
    default:
      medit_disp_menu(d);
      return;
    }
    if (i == 0)
      break;
    else if (i == 1)
      write_to_output(d, "\r\nEnter new value : ");
    else if (i == -1)
      write_to_output(d, "\r\nEnter new text :\r\n] ");
    else
      write_to_output(d, "Oops...\r\n");
    return;

  case MEDIT_STATS_MENU:
    i=0;
    switch(*arg) {
    case 'q':
    case 'Q':
      medit_disp_menu(d);
      return;
    case '1':  /* Edit level */
      OLC_MODE(d) = MEDIT_LEVEL;
      i++;
      break;
    /* race */
    case '2':
    	medit_disp_race_selection(d);
    	return;
    /* subrace */
    case '3':
    	if(IS_ELEMENTAL(OLC_MOB(d)) || IS_DRACONIAN(OLC_MOB(d)) || IS_DRAGON(OLC_MOB(d))) {
    		medit_disp_subrace_selection(d);
    		return;;
    	} 
    	write_to_output(d, "That option does not apply to this mob's race.\n\r");
    	medit_disp_stats_menu(d);
    	return;
    /* class */
    case '4':
    	medit_disp_class_selection(d);
    	return;
    case '5':  /* Autoroll stats */
      medit_autoroll_stats(d);
      medit_disp_stats_menu(d);
      OLC_VAL(d) = TRUE;
      return;
    case '6':
      OLC_MODE(d) = MEDIT_NUM_HP_DICE;
      i++;
      break;
    case '7':
      OLC_MODE(d) = MEDIT_SIZE_HP_DICE;
      i++;
      break;
    case '8':
      OLC_MODE(d) = MEDIT_ADD_HP;
      i++;
      break;
    case '9':
      OLC_MODE(d) = MEDIT_NDD;
      i++;
      break;
    case 'a':
    case 'A':
      OLC_MODE(d) = MEDIT_SDD;
      i++;
      break;
    case 'b':
    case 'B':
      OLC_MODE(d) = MEDIT_DAMROLL;
      i++;
      break;
    case 'c':
    case 'C':
      OLC_MODE(d) = MEDIT_AC;
      i++;
      break;
    case 'd':
    case 'D':
      OLC_MODE(d) = MEDIT_EXP;
      i++;
      break;
    case 'e':
    case 'E':
      OLC_MODE(d) = MEDIT_GOLD;
      i++;
      break;
    case 'f':
    case 'F':
      OLC_MODE(d) = MEDIT_HITROLL;
      i++;
      break;
    case 'g':
    case 'G':
      OLC_MODE(d) = MEDIT_ALIGNMENT;
      i++;
      break;
    case 'h':
    case 'H':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_STR;
      i++;
      break;
    case 'i':
    case 'I':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_INT;
      i++;
      break;
    case 'j':
    case 'J':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_WIS;
      i++;
      break;
    case 'k':
    case 'K':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_DEX;
      i++;
      break;
    case 'l':
    case 'L':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_CON;
      i++;
      break;
    case 'm':
    case 'M':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_CHA;
      i++;
      break;
    case 'n':
    case 'N':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_PARA;
      i++;
      break;
    case 'o':
    case 'O':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_ROD;
      i++;
      break;
    case 'p':
    case 'P':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_PETRI;
      i++;
      break;
    case 'r':
    case 'R':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_BREATH;
      i++;
      break;
    case 's':
    case 'S':
      if (!CONFIG_MEDIT_ADVANCED) {
        write_to_output(d, "Invalid Choice!\r\nEnter Choice : ");
        return;
	  }
      OLC_MODE(d) = MEDIT_SPELL;
      i++;
      break;
    default:
      medit_disp_stats_menu(d);
      return;
    }
    if (i == 0)
      break;
    else if (i == 1)
      write_to_output(d, "\r\nEnter new value : ");
    else if (i == -1)
      write_to_output(d, "\r\nEnter new text :\r\n] ");
    else
      write_to_output(d, "Oops...\r\n");
    return;

  case OLC_SCRIPT_EDIT:
    if (dg_script_edit_parse(d, arg)) return;
    break;

  case MEDIT_KEYWORD:
    smash_tilde(arg);
    if (GET_ALIAS(OLC_MOB(d)))
      free(GET_ALIAS(OLC_MOB(d)));
    GET_ALIAS(OLC_MOB(d)) = str_udup(arg);
    break;

  case MEDIT_S_DESC:
    smash_tilde(arg);
    if (GET_SDESC(OLC_MOB(d)))
      free(GET_SDESC(OLC_MOB(d)));
    GET_SDESC(OLC_MOB(d)) = str_udup(arg);
    break;

  case MEDIT_L_DESC:
    smash_tilde(arg);
    if (GET_LDESC(OLC_MOB(d)))
      free(GET_LDESC(OLC_MOB(d)));
    if (arg && *arg) {
      char buf[MAX_INPUT_LENGTH];
      snprintf(buf, sizeof(buf), "%s\r\n", arg);
      GET_LDESC(OLC_MOB(d)) = strdup(buf);
    } else
      GET_LDESC(OLC_MOB(d)) = strdup("undefined");

    break;

  case MEDIT_D_DESC:
    /*
     * We should never get here.
     */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: medit_parse(): Reached D_DESC case!");
    write_to_output(d, "Oops...\r\n");
    break;

  case MEDIT_NPC_FLAGS:
    if ((i = atoi(arg)) <= 0)
      break;
    else if ( (j = get_flag_by_number(i, NUM_MOB_FLAGS, NUM_ILLEGAL_MOB_FLAGS, illegal_mob_flags)) == -1) {
       write_to_output(d, "Invalid choice!\r\n");
       write_to_output(d, "Enter mob flags (0 to quit) :");
       return;
    } else if (j <= NUM_MOB_FLAGS) {
      TOGGLE_BIT_AR(MOB_FLAGS(OLC_MOB(d)), (j));
    }
    medit_disp_mob_flags(d);
    return;

  case MEDIT_AFF_FLAGS:
    if ((i = atoi(arg)) <= 0)
      break;
    else if((j = get_flag_by_number(i, NUM_AFF_FLAGS, NUM_ILLEGAL_AFF_FLAGS, illegal_aff_flags)) == -1) {
      write_to_output(d, "Invalid choice!\r\n");
      write_to_output(d, "Enter affected flags (0 to quit) :");
      return;
    }
    else if (j <= NUM_AFF_FLAGS)
      TOGGLE_BIT_AR(AFF_FLAGS(OLC_MOB(d)), j);

    medit_disp_aff_flags(d);
    return;

/* Numerical responses. */

  case MEDIT_SEX:
    GET_SEX(OLC_MOB(d)) = LIMIT(i - 1, 0, NUM_GENDERS - 1);
    break;

  case MEDIT_HITROLL:
    GET_HITROLL(OLC_MOB(d)) = LIMIT(i, 0, 50);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_DAMROLL:
    GET_DAMROLL(OLC_MOB(d)) = LIMIT(i, 0, 50);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_NDD:
    GET_NDD(OLC_MOB(d)) = LIMIT(i, 0, 30);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_SDD:
    GET_SDD(OLC_MOB(d)) = LIMIT(i, 0, 127);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_NUM_HP_DICE:
    GET_HIT(OLC_MOB(d)) = LIMIT(i, 0, 30);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_SIZE_HP_DICE:
    GET_MANA(OLC_MOB(d)) = LIMIT(i, 0, 1000);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_ADD_HP:
    GET_MOVE(OLC_MOB(d)) = LIMIT(i, 0, 30000);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_AC:
    GET_AC(OLC_MOB(d)) = LIMIT(i, -200, 200);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_EXP:
    GET_EXP(OLC_MOB(d)) = LIMIT(i, 0, MAX_MOB_EXP);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_GOLD:
    GET_GOLD(OLC_MOB(d)) = LIMIT(i, 0, MAX_MOB_GOLD);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_STR:
    GET_STR(OLC_MOB(d)) = LIMIT(i, 11, 25);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_INT:
    GET_INT(OLC_MOB(d)) = LIMIT(i, 11, 25);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_WIS:
    GET_WIS(OLC_MOB(d)) = LIMIT(i, 11, 25);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_DEX:
    GET_DEX(OLC_MOB(d)) = LIMIT(i, 11, 25);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_CON:
    GET_CON(OLC_MOB(d)) = LIMIT(i, 11, 25);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_CHA:
    GET_CHA(OLC_MOB(d)) = LIMIT(i, 11, 25);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_PARA:
    GET_SAVE(OLC_MOB(d), SAVING_PARA) = LIMIT(i, 0, 100);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_ROD:
    GET_SAVE(OLC_MOB(d), SAVING_ROD) = LIMIT(i, 0, 100);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_PETRI:
    GET_SAVE(OLC_MOB(d), SAVING_PETRI) = LIMIT(i, 0, 100);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_BREATH:
    GET_SAVE(OLC_MOB(d), SAVING_BREATH) = LIMIT(i, 0, 100);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_SPELL:
    GET_SAVE(OLC_MOB(d), SAVING_SPELL) = LIMIT(i, 0, 100);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_POS:
    GET_POS(OLC_MOB(d)) = LIMIT(i - 1, 0, NUM_POSITIONS - 1);
    break;

  case MEDIT_DEFAULT_POS:
    GET_DEFAULT_POS(OLC_MOB(d)) = LIMIT(i - 1, 0, NUM_POSITIONS - 1);
    break;

  case MEDIT_ATTACK:
    GET_ATTACK(OLC_MOB(d)) = LIMIT(i, 0, NUM_ATTACK_TYPES - 1);
    break;

  case MEDIT_LEVEL:
    GET_LEVEL(OLC_MOB(d)) = LIMIT(i, 1, LVL_IMPL);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_ALIGNMENT:
    GET_ALIGNMENT(OLC_MOB(d)) = LIMIT(i, -1000, 1000);
    OLC_VAL(d) = TRUE;
    medit_disp_stats_menu(d);
    return;

  case MEDIT_COPY:
    if ((i = real_mobile(atoi(arg))) != NOWHERE) {
      medit_setup_existing(d, i);
    } else
      write_to_output(d, "That mob does not exist.\r\n");
    break;

  case MEDIT_DELETE:
    if (*arg == 'y' || *arg == 'Y') {
      if (delete_mobile(GET_MOB_RNUM(OLC_MOB(d))) != NOBODY)
        write_to_output(d, "Mobile deleted.\r\n");
      else
        write_to_output(d, "Couldn't delete the mobile!\r\n");

      cleanup_olc(d, CLEANUP_ALL);
      return;
    } else if (*arg == 'n' || *arg == 'N') {
      medit_disp_menu(d);
      OLC_MODE(d) = MEDIT_MAIN_MENU;
      return;
    } else
      write_to_output(d, "Please answer 'Y' or 'N': ");
    break;
  case MEDIT_RACE:
    if((i = atoi(arg) - 1) > RACE_UNDEFINED && i < NUM_RACES) {
      /*
       * Reset the subrace to avoid some errors that will crash the game.
       */
      GET_SUBRACE(OLC_MOB(d)) = 0;
      GET_RACE(OLC_MOB(d)) = i;
      OLC_VAL(d) = TRUE;
      medit_disp_stats_menu(d);
      return;
    }
    write_to_output(d,"Please select a valid race below:\r\n");
    medit_disp_race_selection(d);
    return;

  case MEDIT_SUBRACE:
    if((i = atoi(arg)) > 0
        && ((IS_ELEMENTAL(OLC_MOB(d)) && i < NUM_ELE_SUBRACE)
            || (IS_DRACONIAN(OLC_MOB(d)) && i < SUBRACE_CHROMATIC_DRAGON)
            || (IS_DRAGON(OLC_MOB(d)) && i < NUM_DRC_SUBRACE))
    ) {
      GET_SUBRACE(OLC_MOB(d)) = i;
      OLC_VAL(d) = TRUE;
      medit_disp_stats_menu(d);
      return;
    }
    /*
     * The builder entered 0 or selected a wrong subrace for the current
     * mob's race. Alert the builder and display the selection again.
     */
    write_to_output(d, "Please select a valid subrace below:\r\n");
    medit_disp_subrace_selection(d);
    return;
	  
  case MEDIT_CLASS:
    if((i = atoi(arg) - 1) > CLASS_UNDEFINED && i < NUM_CLASSES) {
      GET_CLASS(OLC_MOB(d)) = i;
      OLC_VAL(d) = TRUE;
      medit_disp_stats_menu(d);
      return;
    }
    write_to_output(d, "Please select a valid class below:\r\n");
    medit_disp_class_selection(d);
    return;

  default:
    /* We should never get here. */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: medit_parse(): Reached default case!");
    write_to_output(d, "Oops...\r\n");
    break;
  }

/* END OF CASE If we get here, we have probably changed something, and now want
   to return to main menu.  Use OLC_VAL as a 'has changed' flag */

  OLC_VAL(d) = TRUE;
  medit_disp_menu(d);
}

void medit_string_cleanup(struct descriptor_data *d, int terminator)
{
  switch (OLC_MODE(d)) {

  case MEDIT_D_DESC:
  default:
     medit_disp_menu(d);
     break;
  }
}

void medit_autoroll_stats(struct descriptor_data *d)
{
  int mob_lev;

  mob_lev = GET_LEVEL(OLC_MOB(d));
  mob_lev = GET_LEVEL(OLC_MOB(d)) = LIMIT(mob_lev, 1, LVL_IMPL);

  GET_MOVE(OLC_MOB(d))    = mob_lev*10;          /* hit point bonus (mobs don't use movement points */
  GET_HIT(OLC_MOB(d))     = mob_lev/5;           /* number of hitpoint dice */
  GET_MANA(OLC_MOB(d))    = mob_lev/5;           /* size of hitpoint dice   */

  GET_NDD(OLC_MOB(d))     = MAX(1, mob_lev/6);   /* number damage dice 1-5  */
  GET_SDD(OLC_MOB(d))     = MAX(2, mob_lev/6);   /* size of damage dice 2-5 */
  GET_DAMROLL(OLC_MOB(d)) = mob_lev/6;           /* damroll (dam bonus) 0-5 */

  GET_HITROLL(OLC_MOB(d)) = mob_lev/3;           /* hitroll 0-10            */
  GET_EXP(OLC_MOB(d))     = (mob_lev*mob_lev*100);
  GET_GOLD(OLC_MOB(d))    = (mob_lev*10);
  GET_AC(OLC_MOB(d))      = (100-(mob_lev*6));   /* AC 94 to -80            */

  /* 'Advanced' stats are only rolled if advanced options are enabled */
  if (CONFIG_MEDIT_ADVANCED) {
    GET_STR(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18); /* 2/3 level in range 11 to 18 */
    GET_INT(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18);
    GET_WIS(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18);
    GET_DEX(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18);
    GET_CON(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18);
    GET_CHA(OLC_MOB(d))     = LIMIT((mob_lev*2)/3, 11, 18);

    GET_SAVE(OLC_MOB(d), SAVING_PARA)   = mob_lev / 4;  /* All Saving throws */
    GET_SAVE(OLC_MOB(d), SAVING_ROD)    = mob_lev / 4;  /* set to a quarter  */
    GET_SAVE(OLC_MOB(d), SAVING_PETRI)  = mob_lev / 4;  /* of the mobs level */
    GET_SAVE(OLC_MOB(d), SAVING_BREATH) = mob_lev / 4;
    GET_SAVE(OLC_MOB(d), SAVING_SPELL)  = mob_lev / 4;
  }

}
