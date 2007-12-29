// Some portions Copyright (C) 1999-2000 Id Software, Inc.
// All other portions Copyright (C) 2002-2007 Ted Vessenes

/*****************************************************************************
 * ai_accuracy.c
 *
 * Functions the bot uses to estimate its combat accuracy
 *****************************************************************************/

#include "ai_main.h"
#include "ai_vars.h"
#include "ai_accuracy.h"

#include "ai_weapon.h"


// Mapping from zone ids to distances and pitches
float  dist_zone_center[ZCD_NUM_IDS] = { ZCD_NEAR, ZCD_MID, ZCD_FAR, ZCD_VERYFAR };
float pitch_zone_center[ZCP_NUM_IDS] = { -ZCP_LOW, 0, ZCP_LOW };

// Default accuracy statistics for each weapon
bot_accuracy_t acc_default_weapon[WP_NUM_WEAPONS];


/*
==============
AccuracyCreate

Fills out the inputted accuracy record with the
inputted data.  If this code were written in C++,
this would be a constructor.
==============
*/
void AccuracyCreate(bot_accuracy_t *acc, int weapon, float shots,
					float direct_hits, float splash_hits, float total_splash_damage)
{
	weapon_stats_t *ws;

	// Initialize shot data
	ws = &weapon_stats[weapon];
	acc->shots = shots;

	// Some weapons create multiple shots each time the weapon is
	// fired, so the fire time must be divided between each shot
	//
	// NOTE: This intentionally ignores the potential of hasted reload times.
	acc->time  = shots * ws->reload / ws->shots;

	// Initialize hits and damage as appropriate
	//
	// NOTE: Direct damage is computed from the weapon stats, but splash damage
	// must be supplied by the caller.  If a mod creates a weapon which has variable
	// damage (often but not always a bad game design idea), then this function will
	// need to be reworked to accept a total direct damage argument as well.
	acc->direct.hits   = direct_hits;
	acc->direct.damage = direct_hits * ws->damage;
	acc->splash.hits   = splash_hits;
	acc->splash.damage = total_splash_damage;
}

/*
=============
AccuracyTally

Add the data from the input accuracy record to
the total record.
=============
*/
void AccuracyTally(bot_accuracy_t *total, bot_accuracy_t *acc)
{
	total->shots         += acc->shots;
	total->time          += acc->time;
	total->direct.hits   += acc->direct.hits;
	total->direct.damage += acc->direct.damage;
	total->splash.hits   += acc->splash.hits;
	total->splash.damage += acc->splash.damage;
}

/*
=============
AccuracyScale

Scale the data in one accuracy record by a floating
point value and the result in result.  Result may point
to the input accuracy record.  Returns a pointer
to the result record.
=============
*/
bot_accuracy_t *AccuracyScale(bot_accuracy_t *acc, float scale, bot_accuracy_t *result)
{
	result->shots         = scale * acc->shots;
	result->time          = scale * acc->time;
	result->direct.hits   = scale * acc->direct.hits;
	result->direct.damage = scale * acc->direct.damage;
	result->splash.hits   = scale * acc->splash.hits;
	result->splash.damage = scale * acc->splash.damage;

	return result;
}

/*
================
ZoneCenterWeight

Given an input distance to a target and a sorted list of
zone centers (either distances or pitch angles, whose list
indicies equal their zone ids), this function determines
which two centers the input distance lies between.  It
also computes how closely the input distance is weighted
towards the first center (so the weighting for the second
center will be 1.0 minus this weight).  If the input
distance is less than the first list value or greater than
the last list value, the weight value is 1.0 and the
second center id will be -1 (no center).
================
*/
void ZoneCenterWeight(float value, float *centers, int num_centers,
					  int *first_id, int *second_id, float *weight)
{
	int index;
	qboolean found;
	float *match;

	// Check where this distance would fall in the sorted distance list--
	// The value lies between index-1 and index.
	//
	// NOTE: Index will equal num_centers if the value is greater than
	// the last distance in the center list.
	found = bsearch_addr(&value, centers, num_centers, sizeof(float),
						 CompareEntryFloat, (void *) &match);
	index = match - centers;

	// If an exact match wasn't found and the index is internal to the list's
	// interval, compute the weighting between the two nearest centers
	if (index > 0 && index < num_centers && !found)
	{
		*first_id  = index-1;
		*second_id = index;
		*weight    = (centers[index] - value) /
					 (centers[index] - centers[index-1]);
	}

	// Otherwise just use the nearest (or matching) interval
	else
	{
		// For indicies off the end the list, use the last valid list index
		if (index >= num_centers)
			index = num_centers-1;

		// Just use one center id with a weight of 1.0
		*first_id = index;
		*second_id = -1;
		*weight = 1.0;
	}
}

/*
================
CombatZoneCreate

Given an input distance and pitch, creates a combat zone
description, interpolated from nearby combat zone centers.
================
*/
void CombatZoneCreate(combat_zone_t *zone, float dist, float pitch)
{
	int i, j, centers;
	int dist_id_start, dist_id_end, pitch_id_start, pitch_id_end;
	float dist_weight, pitch_weight;

	// Set the input distance and pitch
	zone->dist = dist;
	zone->pitch = pitch;

	// Determine which distance and pitch zone centers to
	// average between and their weights
	ZoneCenterWeight(dist,  dist_zone_center,  ZCD_NUM_IDS,
					 &dist_id_start,  &dist_id_end,  &dist_weight);
	ZoneCenterWeight(pitch, pitch_zone_center, ZCP_NUM_IDS,
					 &pitch_id_start, &pitch_id_end, &pitch_weight);

	// Create entry for the first center
	centers = 0;
	zone->center[centers].dist = dist_id_start;
	zone->center[centers].pitch = pitch_id_start;
	zone->weight[centers++] = dist_weight * pitch_weight;

	// Create a second dist entry if necessary
	if (dist_id_end != ZCD_ID_NONE) {
		zone->center[centers].dist = dist_id_end;
		zone->center[centers].pitch = pitch_id_start;
		zone->weight[centers++] = (1.0 - dist_weight) * pitch_weight;
	}

	// Create a second pitch entry if necessary
	if (pitch_id_end != ZCP_ID_NONE) {
		zone->center[centers].dist = dist_id_start;
		zone->center[centers].pitch = pitch_id_end;
		zone->weight[centers++] = dist_weight * (1.0 - pitch_weight);
	}

	// Add the fourth dist end / pitch end entry if necessary
	if (dist_id_end   != ZCD_ID_NONE &&
		pitch_id_end != ZCP_ID_NONE)
	{
		zone->center[centers].dist = dist_id_end;
		zone->center[centers].pitch = pitch_id_end;
		zone->weight[centers++] = (1.0 - dist_weight) * (1.0 - pitch_weight);
	}

	// Save the actual number of centers in the zone average
	zone->num_centers = centers;
}

/*
================
CombatZoneInvert

Think of a combat zone as a description of a target
relative to a player's position.  This function
inverts that description so that it describes the
player's position relative to the target.  It's
permitted for "source" and "inverted" to point to
the same structure.
================
*/
void CombatZoneInvert(combat_zone_t *source, combat_zone_t *inverted)
{
	int i;
	zone_center_pitch *pitch;

	// Most of the data remains unchanged
	if (inverted != source)
		memcpy(inverted, source, sizeof(combat_zone_t));

	// Invert the pitch value and zones
	inverted->pitch = -inverted->pitch;
	for (i = 0; i < inverted->num_centers; i++)
	{
		pitch = &inverted->center[i].pitch;
		if (*pitch == ZCP_ID_LOW)
			*pitch = ZCP_ID_HIGH;
		else if (*pitch == ZCP_ID_HIGH)
			*pitch = ZCP_ID_LOW;
	}
}

/*
===============
BotAccuracyRead

Reads accuracy data for the weapon and zone pair.  The
weapon argument must be specified (ie. not WP_NONE), but
the zone argument may be ommited (ie. NULL).  The read data
is stored in the inputted accuracy record, and the data
is padded with extra default data if not enough real
information has been collected.
===============
*/
void BotAccuracyRead(bot_state_t *bs, bot_accuracy_t *acc, int weapon, combat_zone_t *zone)
{
	int i;
	float time;
	zone_center_t *center;
	bot_accuracy_t scaled;

	// Average over the specified zones if zone data was supplied ...
	if (zone)
	{
		// Reset the contents of the output accuracy before tallying data from zone centers
		memset(acc, 0, sizeof(bot_accuracy_t));

		// Use a portion from each zone center accuracy record
		for (i = 0; i < zone->num_centers; i++)
		{
			// Look up the accuracy record associated with the current combat zone center
			center = &zone->center[i];

			// Add a fraction of this accuracy record to the output total
			AccuracyScale(&bs->acc_weap_zone[weapon][center->dist][center->pitch],
						  zone->weight[i], &scaled);
			AccuracyTally(acc, &scaled);
		}
	}

	// ... Otherwise just read the appropriate weapon data
	else
	{
		memcpy(acc, &bs->acc_weapon[weapon], sizeof(bot_accuracy_t));
	}

	// Check if default data must get added
	time = ACCURACY_DEFAULT_TIME - acc->time;
	if (time <= 0.0)
		return;

	// The default data only applies for weapons in range;
	// Add time but no extra hits or damage for out of range weapons
	if (WeaponInRange(weapon, zone->dist))
	{
		// Add that many seconds of default data to the input record
		AccuracyScale(&acc_default_weapon[weapon], time, &scaled);
		AccuracyTally(acc, &scaled);
	}
	else
	{
		acc->time = time;
	}
}

#ifdef DEBUG_AI
/*
==================
PrintWeaponAccInfo
==================
*/
void PrintWeaponAccInfo(bot_state_t *bs, int weapon)
{
	int pitch, dist;
	float actual, potential, hit_damage;
	float actual_error, potential_error;
	bot_accuracy_t *acc;
	char *level_name;

	// Print a nice header explaining the table layout
	G_Printf("%.2f %s %s:  Near,  Mid,  Far, Very Far\n",
			 server_time, EntityNameFast(bs->ent), WeaponName(weapon));

	// Compute and print out the actual percentage of potential damage dealt
	// for each pitch and distance zone center
	hit_damage = weapon_stats[weapon].damage;
	actual_error = potential_error = 0.0;
	for (pitch = 0; pitch < ZCP_NUM_IDS; pitch++)
	{
		// Get a name for this level
		switch (pitch)
		{
			default:
			case ZCP_ID_LOW:	level_name = "  Low";	break;
			case ZCP_ID_LEVEL:	level_name = "Level";	break;
			case ZCP_ID_HIGH:	level_name = " High";	break;
		}

		// Print the level name and each distance accuracy for that level
		G_Printf(" %s:", level_name);
		for (dist = 0; dist < ZCD_NUM_IDS; dist++)
		{
			// Determine the actual damage done and the maximum potential damage
			acc = &bs->acc_weap_zone[weapon][dist][pitch];
			actual = acc->direct.damage + acc->splash.damage;
			potential = acc->shots * hit_damage;

			// Check for accuracy errors
			if ( (potential > 0) &&
				 (actual/potential > 1.0 + 1e-5) )
			{
				actual_error = actual;
				potential_error = potential;
			}

			// Print accuracy data, avoiding divides by zero
			if (potential > 0)
				G_Printf(" %2.f%%", 100 * actual / potential);
			else
				G_Printf(" ??%%");

			// Also print the time spent acquiring the data and a seaparator
			G_Printf(" (%3.2f)", acc->time);
			if (dist < ZCD_NUM_IDS-1)
				G_Printf(", ");
		}
		G_Printf("\n");
	}

	// Display an error message if appropriate
	if (actual_error > potential_error)
		G_Printf("  ^1WARNING: Actual damage ^2(%f)^1 exceeds potential damage ^2(%f)^7\n",
				 actual_error, potential_error);

	G_Printf("\n");
}
#endif

/*
=================
BotAccuracyRecord

Record whether or not the bot hit an enemy when
it took a shot from the specified location.
=================
*/
void BotAccuracyRecord(bot_state_t *bs, bot_accuracy_t *acc, int weapon, combat_zone_t *zone)
{
	int i;
	zone_center_t *center;
	bot_accuracy_t zone_acc;
#ifdef DEBUG_AI
	float direct_acc, splash_acc, splash_damage, misses;
	char *direct_name, *separate;
#endif

	// Ignore non-updates
	// NOTE: Technically this function shouldn't even be called in this
	// case, but it's good to check anyway.
	if (!acc->shots)
		return;

	// Add this to the total damage the bot has dealt
	bs->damage_dealt += acc->direct.damage + acc->splash.damage;

	// Add the record to the weapon aggregate total and the complete aggregate
	AccuracyTally(&bs->acc_weapon[weapon], acc);

	// Divide the record into a portion for each center in the combat zone
	for (i = 0; i < zone->num_centers; i++)
	{
		// Compute the portion allocated for this center
		center = &zone->center[i];
		AccuracyScale(acc, zone->weight[i], &zone_acc);

		// Add this portion to the specific instance total
		AccuracyTally(&bs->acc_weap_zone[weapon][center->dist][center->pitch], &zone_acc);
	}

#ifdef DEBUG_AI
	// Print accuracy statistics when requested
	if (bs->debug_flags & BOT_DEBUG_INFO_ACCSTATS)
		PrintWeaponAccInfo(bs, weapon);

	// Only give accuracy debug messages when requested
	if ( !(bs->debug_flags & BOT_DEBUG_INFO_ACCURACY) )
		return;

	// Print a description of the zone and weapon
	BotAI_Print(PRT_MESSAGE, "%s: Accuracy with %s (at %.f away, %.f %s): ",
				EntityNameFast(bs->ent), WeaponName(weapon),
				zone->dist, fabs(zone->pitch), (zone->pitch < 0.0 ? "below" : "above"));

	// Compute direct hit accuracy with weapon
	BotAccuracyRead(bs, &zone_acc, weapon, zone);
	if (zone_acc.shots)
		direct_acc = 100 * zone_acc.direct.hits / zone_acc.shots;
	else
		direct_acc = 0.0;

	// Print direct accuracy data
	G_Printf("%0.2f%% ", direct_acc);

	// Compute splash accuracy and average splash damage
	if (zone_acc.splash.hits) {
		splash_acc    = 100 * zone_acc.splash.hits / zone_acc.shots;
		splash_damage = zone_acc.splash.damage / zone_acc.splash.hits;
	} else {
		splash_acc    = 0;
		splash_damage = 0;
	}

	// Add splash accuracy data if necessary; qualify direct hits as "direct"
	// if splash hits are possible
	if (weapon_stats[weapon].radius || splash_acc)
	{
		G_Printf("direct, %0.2f%% splash (%.f avg damage) ",
				 splash_acc, splash_damage);
		direct_name = "direct hit";
	}
	else
	{
		direct_name = "hit";
	}

	// Print a description of the input accuracy record
	G_Printf("(");
	separate = "";

	// Print the direct hits
	if (acc->direct.hits > 0)
	{
		G_Printf("%.f %s%s",
				 acc->direct.hits, direct_name, (acc->direct.hits == 1.0 ? "" : "s"));
		separate = ", ";
	}

	// Print the splash hits
	if (acc->splash.hits > 0)
	{
		G_Printf("%s%.f splash hit%s",
				 separate, acc->splash.hits, (acc->splash.hits == 1.0 ? "" : "s"));
		separate = ", ";
	}

	// Print the misses
	misses = acc->shots - (acc->direct.hits + acc->splash.hits);
	if (misses > 0)
		G_Printf("%s%.f miss%s", separate, misses, (misses == 1.0 ? "" : "es"));

	// Finish the line of printing
	G_Printf(")\n");
#endif
}

/*
================
BotAccuracyReset

Reset the bot's accuracy tracking.  This should probably only be done
when a bot is loaded, or else bots will lose otherwise good statistical
information.  But if the statistics somehow become meaningless, it might
be worth resetting them or toning them down somehow.
================
*/
void BotAccuracyReset(bot_state_t *bs)
{
	// Reset all of the bot's accuracy tracking data
	bs->attack_rate_update_time = server_time;
	memset(&bs->attack_rate,   0, sizeof(bs->attack_rate));
	memset(&bs->acc_weap_zone, 0, sizeof(bs->acc_weap_zone));
	memset(&bs->acc_weapon,    0, sizeof(bs->acc_weapon));
}

/*
===================
BotAttackTimeUpdate

This function updates the bot's internal records
of what percent of the time it spent attacking its
target among all opportunities for attacking.
===================
*/
void BotAttackTimeUpdate(bot_state_t *bs, float extra_reload_time)
{
	float elapsed;
	history_t *attack_rate;

	// Determine how much time has elapsed since the last update
	elapsed = server_time - bs->attack_rate_update_time;
	bs->attack_rate_update_time = server_time;

	// Update nothing if the bot had no enemy to shoot at last frame
	if (!bs->aim_enemy)
		return;

	// Don't count update time from switching weapons
	//
	// NOTE: This doesn't necessarily mean the weapon is in the firing animation
	// state.  The server code applies extra reload time to a "ready to attack"
	// gauntlet while the player holds down the fire button.  That time needs
	// to be counted here as attack time too.
	if (BotWeaponChanging(bs))
		return;

	// Look up the history record for this weapon's attack rate
	attack_rate = &bs->attack_rate[bs->ps->weapon];

	// If the bot shot last frame, accrue potential and actual fire time
	if (extra_reload_time > 0)
	{
		attack_rate->actual    += extra_reload_time;
		attack_rate->potential += extra_reload_time;
		return;
	}

	// Accrue no potential attack time if the weapon was not reloaded
	if (bs->ps->weaponTime > 0)
		return;

	// Account for the potential attack time since the last update
	attack_rate->potential += elapsed;
}

/*
========================
BotAccuracyUpdateMissile

Returns the number of hit counter ticks
attributable to missile fire
========================
*/
int BotAccuracyUpdateMissile(bot_state_t *bs)
{
	int i, exploded, event, hits;
	float damage;
	bot_missile_shot_t *shot;
	gentity_t *bolt, *target;
	bot_accuracy_t acc;
	damage_multi_t blast;
	qboolean enemy_target, team_target;

	// Number of missiles that exploded and won't be tracked after this frame
	exploded = 0;

	// The number of hit counter ticks attributable to missiles this frame
	hits = 0;

	// Loop through list looking for any exploded missiles
	for (i = 0; i < bs->num_own_missiles; i++)
	{
		// If this missile is not valid, remove it from the list
		//
		// NOTE: This occurs when the missile contacts a sky plane.  It doesn't
		// blow up; it's just immediately deleted.
		shot = &bs->own_missiles[i];
		bolt = shot->bolt;
		if (!bolt->inuse || bolt->r.ownerNum != bs->client)
		{

			// Record this shot as a complete miss
			AccuracyCreate(&acc, shot->weapon, 1, 0, 0, 0);
			BotAccuracyRecord(bs, &acc, shot->weapon, &shot->zone);

			// Remove it from the list
			exploded++;
			continue;
		}

		// If the missile hasn't exploded yet, continue tracking it for later
		event = bolt->s.event & ~EV_EVENT_BITS;
		if (event != EV_MISSILE_HIT &&
			event != EV_MISSILE_MISS &&
			event != EV_MISSILE_MISS_METAL)
		{
			// Move this valid record to the proper list position
			if (exploded)
				memcpy(&bs->own_missiles[i - exploded], shot, sizeof(bot_missile_shot_t));
			continue;
		}

		// Check whom, if anyone, this missile directly hit
		//
		// NOTE: The server code does not provide enough information to determine
		// when the missile directly hits a non-player target, like an Obelisk in
		// Overload.  This means the blast damage code will not know to ignore such
		// a target, so direct hits on the object will be tracked as splash damage,
		// which could cause issues with weapon selection if the bot's enemy has
		// something that prevents splash damage (like the battle suit).  It also
		// causes issues with missiles whose blast damage doesn't equal their
		// direct damage (like Plasma).  See G_MissileImpact() in g_missile.c for
		// more information.
		if (event == EV_MISSILE_HIT)
			target = &g_entities[bolt->s.otherEntityNum];
		else
			target = NULL;

		// Determine if this target is an enemy or a teammate
		enemy_target = BotEnemyTeam(bs, target);
		team_target  = BotSameTeam(bs, target);

		// Estimate the amount of blast damage this missile dealt (and blast hits scored)
		BotBlastDamage(bs, shot->weapon, bolt->r.currentOrigin, &blast, target);

		// Adjust the hit counter for direct hits...
		if (enemy_target)
			hits++;
		else if (g_friendlyFire.integer && team_target)
			hits--;

		// ... and for blast hits
		hits += blast.enemy.hits;
		hits -= blast.team.hits;


		// Determine the most damage this missile dealt to a single enemy
		//
		// NOTE: Even though multiple hits are tracked by the server's hit
		// tally counter (see blast.enemy.hits), this code only tracks the
		// most damaging shot for the purpose of accuracy records.  This
		// avoids any potential issues that could occur if the total damage
		// dealt is greater than 100% of potential damage against a single
		// target.

		// First check for direct hits on an enemy
		if (BotEnemyTeam(bs, target))
			AccuracyCreate(&acc, shot->weapon, 1, 1, 0, 0);

		// Check for enemy blast damage as well
		else if (blast.enemy.hits)
			AccuracyCreate(&acc, shot->weapon, 1, 0, 1, blast.enemy.max);

		// The shot completely missed enemies
		else
			AccuracyCreate(&acc, shot->weapon, 1, 0, 0, 0);

		// Record this shot and remove it from the list
		BotAccuracyRecord(bs, &acc, shot->weapon, &shot->zone);
		exploded++;
	}

	// Record the new (possibly lower) number of tracked missiles
	bs->num_own_missiles -= exploded;

	// Return the number of hits actually caused by missiles
	return hits;
}

/*
=========================
BotAccuracyFireCountMelee

Returns the number of times the bot's
melee weapon fired this frame.
=========================
*/
int BotAccuracyFireCountMelee(bot_state_t *bs, float extra_reload_time, float reload_rate)
{
	float update_time, fire_time, fires;

	// Check when the server last processed commands from the bot
	update_time = EntityTimestamp(bs->ent);

	// Compute the amount of time spent firing since a melee shot was last recorded
	//
	// NOTE: Technically melee_time is the time at which the the weapon has reloaded
	// or will reload for another shot.  If the bot recently got a hit with this
	// melee weapon, the melee time will refer to a time in the future.
	if (bs->melee_time > 0.0)
	{
		fire_time = update_time - bs->melee_time;
		if (fire_time < 0.0)
			fire_time = 0.0;
	}
	else
	{
		fire_time = 0.0;
	}

	// Compute the number of fires or fractions thereof
	fires = fire_time / reload_rate;

	// If the bot didn't attack with this melee weapon last frame, just do some cleanup
	//
	// NOTE: bs->ps->weaponstate only equals WEAPON_FIRING for melee
	// weapons when the bot actually scores a hit, so this code must
	// instead check if the server received an attack signal.
	if ( !(bs->ent->client->pers.cmd.buttons & BUTTON_ATTACK) )
	{
		// If the bot wasn't previously firing, obviously no partial shots were taken
		if (bs->melee_time <= 0.0)
			return 0;

		// The bot is no longer firing with its melee weapon
		bs->melee_time = 0.0;

		// Round off the amount of time spent firing to the nearest integer number of fires
		return floor(fires + 0.5);
	}

	// If the bot's weapon required reloading and the weapon is in the firing state,
	// immediately detect another weapon fire
	if ( (extra_reload_time > 0.0) &&
		 (bs->ps->weaponstate == WEAPON_FIRING) )
	{
		// The melee weapon will reload for another fire by this time
		bs->melee_time = update_time + (bs->ps->weaponTime * 0.001);

		// No matter how small the last time slice was, count it as a full fire
		return ceil(fires);
	}

	// If the bot wasn't firing before, start tracking time spent firing from now
	if (bs->melee_time <= 0.0)
	{
		bs->melee_time = update_time;
		return 0;
	}

	// Check how many full frames of firing occured, if any
	fires = floor(fires);
	bs->melee_time += fires * reload_rate;
	return fires;
}

/*
===========================
BotAccuracyFireCountHitscan

Returns the number of times the bot's
hitscan weapon fired this frame.
===========================
*/
int BotAccuracyFireCountHitscan(bot_state_t *bs, float extra_reload_time, float reload_rate)
{
	// If the weapon isn't shooting, even if there is an increased in weapon
	// reload time, the delay can't have come from a shot.  (It probably
	// came from changing weapons.)
	if (bs->ps->weaponstate != WEAPON_FIRING)
		return 0;

	// If no additional reload time was detected, the weapon could not have shot
	if (extra_reload_time <= 0.0)
		return 0;

	// If the weapon is currently reloaded, no shots could have been taken
	//
	// NOTE: It *IS* possible for ps->weaponTime to be less than zero.  Some
	// bugs in the game server can cause the weapon time to be negative (usually
	// -34).  This in turn becomes "reload time credit" when the bot next fires
	// its weapon, so rounding up is also important for this situation.  In
	// theory this could cause inaccurate data for weapons that reload faster
	// than 34 ms, like the Chaingun.
	//
	// Here is how the server might get a player with -34 ms weapon time.
	// Normally bot server frames execute every 50 ms, so 50 ms of weapon
	// time are deducted from bs->ps->weaponTime every frame.  But when a bot
	// respawns, the server sends two 66 ms frames followed by one 18 ms
	// frame (for a total of 150 ms over three frames).  It's possible for
	// the bot to switch weapons after the first 66 ms frame but before the
	// second.  Dropping the weapon takes 200 ms, so the weapon time, after
	// the 18 ms frame, will be 200 - 66 - 18 = 116.  From then on, the bot's
	// frames execute every 50 ms, so the weapon time decays to 66, 16, and
	// finally -34 (where it stops dropping because the value is not positive).
	// See PM_Weapon() in bg_pmove.c for more information.  This is not the
	// first such bug in PM_Weapon(), incidently.
	//
	// Anyway, this function is carefully written to handle weird cases where
	// the server added a fire frame to a negative reload time.  For example,
	// if the reload time is -34 and the bot shoots the railgun, its reload time
	// will be 1466, not 1500.  That's why the preceding code checks the
	// extra reload time, not the official weapon time.
	if (bs->ps->weaponTime <= 0)
		return 0;

	// Compute the number of fires taken in this time period
	//
	// NOTE: Technically this rounding shouldn't be necessary, but it's good to be careful.
	return floor(extra_reload_time / reload_rate + 0.5);
}

/*
=======================
BotAccuracyUpdateWeapon

Returns the number of hit counter ticks
attributable to instant hit weapon fire.

"extra_reload_time" is the number of
additional seconds of weapon reload the bot
detected since last time it processed
accuracies.

"hits" is the number of unaccounted hits
the bot detected last frame.
=======================
*/
void BotAccuracyUpdateWeapon(bot_state_t *bs, float extra_reload_time, int hits)
{
	int fires, shots, weapon;
	float reload_rate;
	bot_accuracy_t acc;
	weapon_stats_t *ws;
	qboolean melee;

	// Never update when dead
	//
	// NOTE: It's theoretically possible to record accuracy data when the
	// bot is dead.  If the bot dies the same frame it tried to shoot,
	// it will be dead know, but still might have gotten a shot off (and
	// maybe hit).  Unfortunately, a bug in the Quake 3 server code prevents
	// this from being easily done.
	//
	// This is because Quake 3 updates each player in order, first moving
	// the player and then shooting.  So if a bot with client number 1 is
	// killed by client 0, the bot won't get to shoot-- it will be dead
	// before it's ClientThink() executes.  (And because of another bug in
	// PM_Weapon() in bg_pmove.c, the bot's ps->weaponTime value will not
	// decrease, so it appears as if another shot has been taken by the
	// dead bot.)  But if the bot is killed by client 2, the bot will still
	// have gotten the shot off, and in theory the accuracy data could be
	// tracked.  Some extra effort is needed, however, because the bot's
	// current weapon (bs->ps->weapon) will be set to WP_NONE, even though
	// the shot would have been made with the old weapon, presumably
	// bs->weapon.  But most of the time bs->ps->weapon should be used
	// (see below).
	//
	// Unfortunately, the only way to track this is to check who killed
	// the bot and compare client numbers.  And there's still the issue
	// of ps->weaponTime not being effectively updated.  (Usually the bug
	// causes a 50ms accrual difference, but this value could be larger
	// if the server was processor lagged, so the value is unpredictable.)
	//
	// NOTE: These issues only apply to instant hit weapons, which can only
	// be detected by their reload times.  The bot tracks accuracy data for
	// missiles in BotTrackMissileShot() in ai_scan.c.
	//
	// FIXME: The game server should really reset ent->ps.weaponTime when a
	// player dies.
	if (BotIsDead(bs))
		return;

	// Never update non-weapons
	//
	// NOTE: Accuracy data must check the bot's current weapon (bs->ps->weapon),
	// not the bot's selected weapon (bs->weapon).
	weapon = bs->ps->weapon;
	if (weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS)
		return;

	// Never update missile weapons here-- their accuracies can only
	// be updated once the missile explodes
	ws = &weapon_stats[weapon];
	if (ws->speed)
		return;

	// FIXME: If any instant-hit weapons dealt blast damage, code should be
	// inserted here to estimate that using BotBlastDamage().  Unfortunately,
	// there's currently no way to determine where an instant hit blast shot
	// exploded.  (You can't use traces because it's possible it exploded on
	// an entity that was killed by the blast.  It gets even worse when
	// shooting a weapon with spread.)  So if anyone adds code for instant
	// hit blast damage weapons, its their responsibility to define the
	// interface that communicates the blast location to the client.  Once that
	// interface is defined, code can be written to estimate blast damage.


	// Look up the weapon's reload rate relative to the bot
	//
	// NOTE: The division by zero check shouldn't be necessary, but why take chances?
	reload_rate = ws->reload;
	if (bs->weapon_rate > 0.0)
		reload_rate /= bs->weapon_rate;

	// Check how many more times the bot's weapon shot
	//
	// NOTE: Melee weapons (eg. WP_GAUNTLET) only need to reload if they
	// contact an enemy directly in front of them, so checking for total number
	// of attacks is different for them.
	if (ws->flags & WSF_MELEE)
		fires = BotAccuracyFireCountMelee(bs, extra_reload_time, reload_rate);
	else
		fires = BotAccuracyFireCountHitscan(bs, extra_reload_time, reload_rate);
	shots = fires * ws->shots;

	// Don't update accuracy data if no additional shots were detected
	if (shots <= 0)
		return;

	// Ignore perceived shots when the bot just used an item
	//
	// NOTE: A bug in PM_Weapon() in bg_pmove.c causes the code to not decay
	// bs->ps->weaponTime if the player uses an item.  This is perceived by the
	// AI code as an time increase, which could be inappropriately be read as a
	// shot.  As such, all shots read when the bot uses an item are ignored.
	// Incidently, players are not allowed to use a holdable item and shoot at
	// the same time
	//
	// FIXME: Someone at id Software should fix this server bug.
	if (bs->ps->pm_flags & PMF_USE_ITEM_HELD)
		return;

	// Sanity-bound the number of hits the bot detected to the number of shots taken
	// Any extra hits must be from other sources, like telefrags.
	//
	// NOTE: It's possible for a railgun to damage two players in one shot,
	// but for the purposes of accuracy data, the bot never expects a shot to
	// deal more than 100% of the total damage possible against one target.
	if (hits > shots)
		hits = shots;

	// Record enemy information in the accuracy data structure
	AccuracyCreate(&acc, weapon, shots, hits, 0, 0);
	BotAccuracyRecord(bs, &acc, weapon, &bs->aim_zone);
}

/*
=================
BotAccuracyUpdate

This function processes the bot's missile and hitscan
fire data to track the bot's attack hits and misses.

The fundamental theme of this function (and the functions
it calls) is that correctly determining hits and misses
is almost impossible.  The server infrastructure simply
doesn't allow for it.  In fact, even determining whether
or not the bot's weapon fired is difficult.

The server uses the bs->ps->persistant[PERS_HITS] tally
counter to send damage ticks to the client.  When this
value is incremented, the client plays a *DING*.  It's
also decremented when a teammate is hurt.  So for example
if a bot uses the Kamikaze and damages one teammate and
one enemy, the tally counter will get -1 for the teammate
and +1 for the enemy, which will read as zero change.  So
there's no possible way for the bot to determine how many
hits were actually scored.  Similar problems can occur
when friendly fire is on, with a missile blast damaging
an enemy and an opponent.

There are other issues with missile fire.  Suppose the bot
fires a grenade and then switches to the machinegun.  It
shot last frame and hears a *DING*-- is that from the grenade
exploding or the machinegun shot?  It's very difficult to
determine.

And there are other issues as well, not described here.  If
you see anything in these functions that might not give
accurate data, rest assured that it bothers me too.  I do
the best I can, but I cannot modify the client/server
infrastructure.
=================
*/
void BotAccuracyUpdate(bot_state_t *bs)
{
	int hits;
	float extra_reload_time;

	// Compute how much additional reload time the bot's weapon accrued since the last update
	extra_reload_time = BotWeaponExtraReloadTime(bs);

	// Account the potential and actual time spent attack the enemy
	BotAttackTimeUpdate(bs, extra_reload_time);

	// Check if the bot hit anything this frame
	hits = bs->ps->persistant[PERS_HITS] - bs->last_hit_count;

	// Process accuracy data from missiles, accounting for each hit caused
	// by missiles
	hits -= BotAccuracyUpdateMissile(bs);

	// Process instant hit weapon accuracy data, given the estimated number
	// of hits this turn from instant hit weapons.
	//
	// NOTE: Technically this is just all hits that did not come from
	// missiles.  This hit count could also include things like kamikaze
	// and telefrag damage.
	BotAccuracyUpdateWeapon(bs, extra_reload_time, hits);

	// Update hit counter
	bs->last_hit_count = bs->ps->persistant[PERS_HITS];

	// Deduce what the weapon reload time should be when the next command is processed
	if (bs->ps->weaponTime <= 0)
		bs->last_reload_delay_ms = bs->ps->weaponTime;
	else
		bs->last_reload_delay_ms = bs->ps->weaponTime -
								   (bs->cmd.serverTime - server_time_ms);

	// Determine how fast the bot's weapon will reload for next frame's shots
	//
	// NOTE: This is done after the accuracy updates because the server code
	// makes the players shoot before picking up items.  So if a player
	// picks up haste and shoots in the same frame, the shot made during that
	// frame will have the increased haste reload rate.  Conversely, for the
	// last frame of haste, the shots made during that frame will reload
	// faster, even though the haste will have worn off before this code
	// executes.  That's why it's important to cache the bot's weapon reload
	// rate for next frame
	//
	// NOTE: See PM_Weapon() in bg_pmove.c for more information
#ifdef MISSIONPACK
	if (bs->ps->powerups[PW_SCOUT])
		bs->weapon_rate = 1.5;
	else
	if (bs->ps->powerups[PW_AMMOREGEN])
		bs->weapon_rate = 1.3;
	else
#endif
	if (bs->ps->powerups[PW_HASTE])
		bs->weapon_rate = 1.3;
	else
		bs->weapon_rate = 1.0;
}

/*
=============
AccuracySetup

Resets default data for accuracy statistics
=============
*/
void AccuracySetup(void)
{
	int weapon;
	float fires, shots, direct_hits, splash_hits;
	float direct_accuracy, splash_accuracy, splash_damage;
	weapon_stats_t *ws;

	// Initialize each weapon's accuracy
	for (weapon = WP_NONE+1; weapon < WP_NUM_WEAPONS; weapon++)
	{
		// Estimate one second of weapon fire
		ws = &weapon_stats[weapon];
		shots = ws->shots / ws->reload;

		// Determine the weapon's accuracy for direct hits and splash
		if (ws->radius >= 100) {
			// NOTE: This averages to 1.0 when splash hits deal 50% damage
			direct_accuracy = ws->accuracy *  .5;
			splash_accuracy = ws->accuracy * 1.0;
		} else {
			direct_accuracy = ws->accuracy;
			splash_accuracy = 0.0;
		}

		// Estimate how many hits would be scored in the specified period of time
		direct_hits = shots * direct_accuracy;
		splash_hits = shots * splash_accuracy;
		if (shots < direct_hits + splash_hits)
			splash_hits = shots - direct_hits;

		// Compute splash damage
		splash_damage = splash_hits * ws->splash_damage * .5;

		// Create a default accuracy record using this data
		AccuracyCreate(&acc_default_weapon[weapon],
					   weapon, shots, direct_hits, splash_hits, splash_damage);
	}
}
