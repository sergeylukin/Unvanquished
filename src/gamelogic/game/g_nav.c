/*
===========================================================================
This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "g_bot.h"
#include "../../engine/botlib/bot_types.h"

//tells if all navmeshes loaded successfully
qboolean navMeshLoaded = qfalse;

/*
========================
Navigation Mesh Loading
========================
*/

void G_BotNavInit()
{
	qhandle_t navHandle;
	int i;

	Com_Printf( "==== Bot Navigation Initialization ==== \n" );

	for ( i = PCL_NONE + 1; i < PCL_NUM_CLASSES; i++ )
	{
		botClass_t bot;
		bot.polyFlagsInclude = POLYFLAGS_WALK;
		bot.polyFlagsExclude = POLYFLAGS_DISABLED;
		
		Q_strncpyz( bot.name, BG_Class( i )->name, sizeof( bot.name ) );

		if ( !trap_BotSetupNav( &bot, &navHandle ) )
		{
			return;
		}
	}
	navMeshLoaded = qtrue;
}

void G_BotNavCleanup( void )
{
	trap_BotShutdownNav();
	navMeshLoaded = qfalse;
}

void G_BotDisableArea( vec3_t origin, vec3_t mins, vec3_t maxs )
{
	trap_BotDisableArea( origin, mins, maxs );
}

void G_BotEnableArea( vec3_t origin, vec3_t mins, vec3_t maxs )
{
	trap_BotEnableArea( origin, mins, maxs );
}

void BotSetNavmesh( gentity_t  *self, class_t newClass )
{
	if ( newClass == PCL_NONE )
	{
		return;
	}

	trap_BotSetNavMesh( self->s.number, newClass - 1 );
}

/*
========================
Bot Navigation Querys
========================
*/

int DistanceToGoal( gentity_t *self )
{
	vec3_t targetPos;
	vec3_t selfPos;
	//safety check for morons who use this incorrectly
	if ( !( self->botMind ) )
	{
		return -1;
	}
	BotGetTargetPos( self->botMind->goal, targetPos );
	VectorCopy( self->s.origin, selfPos );
	return Distance( selfPos, targetPos );
}

int DistanceToGoalSquared( gentity_t *self )
{
	vec3_t targetPos;
	vec3_t selfPos;
	//safety check for morons who use this incorrectly
	if ( !( self->botMind ) )
	{
		return -1;
	}
	BotGetTargetPos( self->botMind->goal, targetPos );
	VectorCopy( self->s.origin, selfPos );
	return DistanceSquared( selfPos, targetPos );
}

qboolean BotPathIsWalkable( gentity_t *self, botTarget_t target )
{
	vec3_t selfPos, targetPos;
	vec3_t viewNormal;
	botTrace_t trace;

	BG_GetClientNormal( &self->client->ps, viewNormal );
	VectorMA( self->s.origin, self->r.mins[2], viewNormal, selfPos );
	BotGetTargetPos( target, targetPos );

	if ( !trap_BotNavTrace( self->s.number, &trace, selfPos, targetPos ) )
	{
		return qfalse;
	}

	if ( trace.frac >= 1.0f )
	{
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}

void BotFindRandomPointOnMesh( gentity_t *self, vec3_t point )
{
	trap_BotFindRandomPoint( self->s.number, point );
}

/*
========================
Local Bot Navigation
========================
*/

void BotStrafeDodge( gentity_t *self )
{
	usercmd_t *botCmdBuffer = &self->botMind->cmdBuffer;

	if ( self->client->time1000 >= 500 )
	{
		botCmdBuffer->rightmove = 127;
	}
	else
	{
		botCmdBuffer->rightmove = -127;
	}

	if ( ( self->client->time10000 % 2000 ) < 1000 )
	{
		botCmdBuffer->rightmove *= -1;
	}

	if ( ( self->client->time1000 % 300 ) >= 100 && ( self->client->time10000 % 3000 ) > 2000 )
	{
		botCmdBuffer->rightmove = 0;
	}
}

void BotMoveInDir( gentity_t *self, uint32_t dir )
{
	usercmd_t *botCmdBuffer = &self->botMind->cmdBuffer;
	if ( dir & MOVE_FORWARD )
	{
		botCmdBuffer->forwardmove = 127;
	}
	else if ( dir & MOVE_BACKWARD )
	{
		botCmdBuffer->forwardmove = -127;
	}

	if ( dir & MOVE_RIGHT )
	{
		botCmdBuffer->rightmove = 127;
	}
	else if ( dir & MOVE_LEFT )
	{
		botCmdBuffer->rightmove = -127;
	}
}

void BotAlternateStrafe( gentity_t *self )
{
	usercmd_t *botCmdBuffer = &self->botMind->cmdBuffer;

	if ( level.time % 8000 < 4000 )
	{
		botCmdBuffer->rightmove = 127;
	}
	else
	{
		botCmdBuffer->rightmove = -127;
	}
}

void BotStandStill( gentity_t *self )
{
	usercmd_t *botCmdBuffer = &self->botMind->cmdBuffer;

	botCmdBuffer->forwardmove = 0;
	botCmdBuffer->rightmove = 0;
	botCmdBuffer->upmove = 0;
}

qboolean BotJump( gentity_t *self )
{
	if ( self->client->ps.stats[STAT_TEAM] == TEAM_HUMANS && self->client->ps.stats[STAT_STAMINA] < STAMINA_SLOW_LEVEL + STAMINA_JUMP_TAKE )
	{
		return qfalse;
	}

	self->botMind->cmdBuffer.upmove = 127;
	return qtrue;
}

qboolean BotSprint( gentity_t *self, qboolean enable )
{

	usercmd_t *botCmdBuffer = &self->botMind->cmdBuffer;

	if ( !enable )
	{
		usercmdReleaseButton( botCmdBuffer->buttons, BUTTON_SPRINT );
		return qfalse;
	}

	if ( self->client->ps.stats[STAT_TEAM] == TEAM_HUMANS && self->client->ps.stats[STAT_STAMINA] > STAMINA_SLOW_LEVEL + STAMINA_JUMP_TAKE && self->botMind->botSkill.level >= 5 )
	{
		usercmdPressButton( botCmdBuffer->buttons, BUTTON_SPRINT );
		usercmdReleaseButton( botCmdBuffer->buttons, BUTTON_WALKING );
		return qtrue;
	}
	else
	{
		usercmdReleaseButton( botCmdBuffer->buttons, BUTTON_SPRINT );
		return qfalse;
	}
}


qboolean BotDodge( gentity_t *self )
{
	vec3_t backward, right, left;
	vec3_t end;
	float jumpMag;
	botTrace_t tback, tright, tleft;

	//see: bg_pmove.c, these conditions prevent use from using dodge
	if ( self->client->ps.stats[STAT_TEAM] != TEAM_HUMANS )
	{
		usercmdReleaseButton( self->botMind->cmdBuffer.buttons, BUTTON_DODGE );
		return qfalse;
	}

	if ( self->client->ps.pm_type != PM_NORMAL || self->client->ps.stats[STAT_STAMINA] > STAMINA_SLOW_LEVEL + STAMINA_DODGE_TAKE ||
	        ( self->client->ps.pm_flags & PMF_DUCKED ) )
	{
		usercmdReleaseButton( self->botMind->cmdBuffer.buttons, BUTTON_DODGE );
		return qfalse;
	}

	if ( !( self->client->ps.pm_flags & ( PMF_TIME_LAND | PMF_CHARGE ) ) && self->client->ps.groundEntityNum != ENTITYNUM_NONE )
	{
		usercmdReleaseButton( self->botMind->cmdBuffer.buttons, BUTTON_DODGE );
		return qfalse;
	}

	//skill level required to use dodge
	if ( self->botMind->botSkill.level < 7 )
	{
		usercmdReleaseButton( self->botMind->cmdBuffer.buttons, BUTTON_DODGE );
		return qfalse;
	}

	//find the best direction to dodge in
	AngleVectors( self->client->ps.viewangles, backward, right, NULL );
	VectorInverse( backward );
	backward[2] = 0;
	VectorNormalize( backward );
	right[2] = 0;
	VectorNormalize( right );
	VectorNegate( right, left );

	jumpMag = BG_Class( ( class_t ) self->client->ps.stats[STAT_CLASS] )->jumpMagnitude;

	//test each direction for navigation mesh collisions
	//FIXME: this code does not guarentee that we will land safely and on the navigation mesh!
	VectorMA( self->s.origin, jumpMag, backward, end );
	trap_BotNavTrace( self->s.number, &tback, self->s.origin, end );
	VectorMA( self->s.origin, jumpMag, right, end );
	trap_BotNavTrace( self->s.number, &tright, self->s.origin, end );
	VectorMA( self->s.origin, jumpMag, left, end );
	trap_BotNavTrace( self->s.number, &tleft, self->s.origin, end );

	//set the direction to dodge
	BotStandStill( self );

	if ( tback.frac > tleft.frac && tback.frac > tright.frac )
	{
		self->botMind->cmdBuffer.forwardmove = -127;
	}
	else if ( tleft.frac > tright.frac && tleft.frac > tback.frac )
	{
		self->botMind->cmdBuffer.rightmove = -127;
	}
	else
	{
		self->botMind->cmdBuffer.rightmove = 127;
	}

	// dodge
	usercmdPressButton( self->botMind->cmdBuffer.buttons, BUTTON_DODGE );
	return qtrue;
}

#define STEPSIZE 18.0f
gentity_t* BotGetPathBlocker( gentity_t *self )
{
	vec3_t playerMins, playerMaxs;
	vec3_t end;
	vec3_t forward;
	trace_t trace;
	vec3_t moveDir;
	const int TRACE_LENGTH = BOT_OBSTACLE_AVOID_RANGE;

	if ( !( self && self->client ) )
	{
		return NULL;
	}

	//get the forward vector, ignoring pitch
	forward[0] = cos( DEG2RAD( self->client->ps.viewangles[YAW] ) );
	forward[1] = sin( DEG2RAD( self->client->ps.viewangles[YAW] ) );
	forward[2] = 0;
	//already normalized
	VectorCopy( forward, moveDir );

	BG_ClassBoundingBox( ( class_t ) self->client->ps.stats[STAT_CLASS], playerMins, playerMaxs, NULL, NULL, NULL );

	//account for how large we can step
	playerMins[2] += STEPSIZE;
	playerMaxs[2] += STEPSIZE;

	VectorMA( self->s.origin, TRACE_LENGTH, moveDir, end );

	trap_Trace( &trace, self->s.origin, playerMins, playerMaxs, end, self->s.number, MASK_SHOT );
	if ( trace.fraction < 1.0f && trace.plane.normal[ 2 ] < 0.7f )
	{
		return &g_entities[trace.entityNum];
	}
	else
	{
		return NULL;
	}
}

qboolean BotShouldJump( gentity_t *self, gentity_t *blocker )
{
	vec3_t playerMins;
	vec3_t playerMaxs;
	vec3_t moveDir;
	float jumpMagnitude;
	trace_t trace;
	const int TRACE_LENGTH = BOT_OBSTACLE_AVOID_RANGE;
	vec3_t forward;
	vec3_t end;

	//blocker is not on our team, so ignore
	if ( BotGetEntityTeam( self ) != BotGetEntityTeam( blocker ) )
	{
		return qfalse;
	}

	VectorSubtract( self->botMind->route[0], self->s.origin, forward );
	//get the forward vector, ignoring z axis
	/*forward[0] = cos(DEG2RAD(self->client->ps.viewangles[YAW]));
	forward[1] = sin(DEG2RAD(self->client->ps.viewangles[YAW]));
	forward[2] = 0;*/
	forward[2] = 0;
	VectorNormalize( forward );

	//already normalized
	VectorCopy( forward, moveDir );
	BG_ClassBoundingBox( ( class_t ) self->client->ps.stats[STAT_CLASS], playerMins, playerMaxs, NULL, NULL, NULL );

	playerMins[2] += STEPSIZE;
	playerMaxs[2] += STEPSIZE;


	//trap_Print(vtos(self->movedir));
	VectorMA( self->s.origin, TRACE_LENGTH, moveDir, end );

	//make sure we are moving into a block
	trap_Trace( &trace, self->s.origin, playerMins, playerMaxs, end, self->s.number, MASK_SHOT );
	if ( trace.fraction >= 1.0f || blocker != &g_entities[trace.entityNum] )
	{
		return qfalse;
	}

	jumpMagnitude = BG_Class( ( class_t )self->client->ps.stats[STAT_CLASS] )->jumpMagnitude;

	//find the actual height of our jump
	jumpMagnitude = Square( jumpMagnitude ) / ( self->client->ps.gravity * 2 );

	//prepare for trace
	playerMins[2] += jumpMagnitude;
	playerMaxs[2] += jumpMagnitude;

	//check if jumping will clear us of entity
	trap_Trace( &trace, self->s.origin, playerMins, playerMaxs, end, self->s.number, MASK_SHOT );

	//if we can jump over it, then jump
	//note that we also test for a blocking barricade because barricades will collapse to let us through
	if ( blocker->s.modelindex == BA_A_BARRICADE || trace.fraction == 1.0f )
	{
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}

qboolean BotFindSteerTarget( gentity_t *self, vec3_t aimPos )
{
	vec3_t forward;
	vec3_t testPoint1, testPoint2;
	vec3_t playerMins, playerMaxs;
	float yaw1, yaw2;
	trace_t trace1, trace2;
	int i;

	if ( !( self && self->client ) )
	{
		return qfalse;
	}

	//get bbox
	BG_ClassBoundingBox( ( class_t ) self->client->ps.stats[STAT_CLASS], playerMins, playerMaxs, NULL, NULL, NULL );

	//account for stepsize
	playerMins[2] += STEPSIZE;
	playerMaxs[2] += STEPSIZE;

	//get the yaw (left/right) we dont care about up/down
	yaw1 = self->client->ps.viewangles[YAW];
	yaw2 = self->client->ps.viewangles[YAW];

	//directly infront of us is blocked, so dont test it
	yaw1 -= 15;
	yaw2 += 15;

	//forward vector is 2D
	forward[2] = 0;

	//find an unobstructed position
	//we check the full 180 degrees in front of us
	for ( i = 0; i < 5; i++, yaw1 -= 15 , yaw2 += 15 )
	{
		//compute forward for right
		forward[0] = cos( DEG2RAD( yaw1 ) );
		forward[1] = sin( DEG2RAD( yaw1 ) );
		//forward is already normalized
		//try the right
		VectorMA( self->s.origin, BOT_OBSTACLE_AVOID_RANGE, forward, testPoint1 );

		//test it
		trap_Trace( &trace1, self->s.origin, playerMins, playerMaxs, testPoint1, self->s.number, MASK_SHOT );

		//check if unobstructed
		if ( trace1.fraction >= 1.0f )
		{
			VectorCopy( testPoint1, aimPos );
			aimPos[2] += self->client->ps.viewheight;
			return qtrue;
		}

		//compute forward for left
		forward[0] = cos( DEG2RAD( yaw2 ) );
		forward[1] = sin( DEG2RAD( yaw2 ) );
		//forward is already normalized
		//try the left
		VectorMA( self->s.origin, BOT_OBSTACLE_AVOID_RANGE, forward, testPoint2 );

		//test it
		trap_Trace( &trace2, self->s.origin, playerMins, playerMaxs, testPoint2, self->s.number, MASK_SHOT );

		//check if unobstructed
		if ( trace2.fraction >= 1.0f )
		{
			VectorCopy( testPoint2, aimPos );
			aimPos[2] += self->client->ps.viewheight;
			return qtrue;
		}
	}

	//we couldnt find a new position
	return qfalse;
}
qboolean BotAvoidObstacles( gentity_t *self, vec3_t rVec )
{
	usercmd_t *botCmdBuffer = &self->botMind->cmdBuffer;
	gentity_t *blocker;

	if ( !( self && self->client ) )
	{
		return qfalse;
	}
	if ( BotGetEntityTeam( self ) == TEAM_NONE )
	{
		return qfalse;
	}

	blocker = BotGetPathBlocker( self );
	if ( blocker )
	{
		vec3_t newAimPos;
		if ( BotShouldJump( self, blocker ) )
		{
			BotJump( self );

			return qfalse;
		}
		else if ( BotFindSteerTarget( self, newAimPos ) )
		{
			//found a steer target
			VectorCopy( newAimPos, rVec );
			return qfalse;
		}
		else
		{
			BotAlternateStrafe( self );
			BotMoveInDir( self, MOVE_BACKWARD );

			return qtrue;
		}
	}
	return qfalse;
}

//copy of PM_CheckLadder in bg_pmove.c
qboolean BotOnLadder( gentity_t *self )
{
	vec3_t forward, end;
	vec3_t mins, maxs;
	trace_t trace;

	if ( !BG_ClassHasAbility( ( class_t ) self->client->ps.stats[ STAT_CLASS ], SCA_CANUSELADDERS ) )
	{
		return qfalse;
	}

	AngleVectors( self->client->ps.viewangles, forward, NULL, NULL );

	forward[ 2 ] = 0.0f;
	BG_ClassBoundingBox( ( class_t ) self->client->ps.stats[ STAT_CLASS ], mins, maxs, NULL, NULL, NULL );
	VectorMA( self->s.origin, 1.0f, forward, end );

	trap_Trace( &trace, self->s.origin, mins, maxs, end, self->s.number, MASK_PLAYERSOLID );

	if ( trace.fraction < 1.0f && trace.surfaceFlags & SURF_LADDER )
	{
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}

void BotDirectionToUsercmd( gentity_t *self, vec3_t dir, usercmd_t *cmd )
{
	vec3_t forward;
	vec3_t right;

	float forwardmove;
	float rightmove;
	AngleVectors( self->client->ps.viewangles, forward, right, NULL );
	forward[2] = 0;
	VectorNormalize( forward );
	right[2] = 0;
	VectorNormalize( right );

	// get direction and non-optimal magnitude
	forwardmove = 127 * DotProduct( forward, dir );
	rightmove = 127 * DotProduct( right, dir );

	// find optimal magnitude to make speed as high as possible
	if ( Q_fabs( forwardmove ) > Q_fabs( rightmove ) )
	{
		float highestforward = forwardmove < 0 ? -127 : 127;

		float highestright = highestforward * rightmove / forwardmove;

		cmd->forwardmove = ClampChar( highestforward );
		cmd->rightmove = ClampChar( highestright );
	}
	else
	{
		float highestright = rightmove < 0 ? -127 : 127;

		float highestforward = highestright * forwardmove / rightmove;

		cmd->forwardmove = ClampChar( highestforward );
		cmd->rightmove = ClampChar( highestright );
	}
}

void BotSeek( gentity_t *self, vec3_t seekPos )
{
	vec3_t direction;
	vec3_t viewOrigin;

	BG_GetClientViewOrigin( &self->client->ps, viewOrigin );
	VectorSubtract( seekPos, viewOrigin, direction );

	VectorNormalize( direction );

	// move directly toward the target
	BotDirectionToUsercmd( self, direction, &self->botMind->cmdBuffer );

	// slowly change our aim to point to the target
	BotSlowAim( self, seekPos, 0.6 );
	BotAimAtLocation( self, seekPos );
}

/**BotGoto
* Used to make the bot travel between waypoints or to the target from the last waypoint
* Also can be used to make the bot travel other short distances
*/
void BotGoto( gentity_t *self, botTarget_t target )
{

	vec3_t tmpVec;

	usercmd_t *botCmdBuffer = &self->botMind->cmdBuffer;

	BotGetIdealAimLocation( self, target, tmpVec );

	if ( !BotAvoidObstacles( self, tmpVec ) )
	{
		BotSeek( self, tmpVec );
	}
	else
	{
		BotSlowAim( self, tmpVec, 0.6 );
		BotAimAtLocation( self, tmpVec );
	}

	//dont sprint or dodge if we dont have enough stamina and are about to slow
	if ( self->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS && self->client->ps.stats[STAT_STAMINA] < STAMINA_SLOW_LEVEL + STAMINA_JUMP_TAKE )
	{
		usercmdReleaseButton( botCmdBuffer->buttons, BUTTON_SPRINT );
		usercmdReleaseButton( botCmdBuffer->buttons, BUTTON_DODGE );
	}
}

/*
=========================
Global Bot Navigation
=========================
*/

qboolean BotMoveToGoal( gentity_t *self )
{
	botTarget_t target;
	vec3_t pos;
	if ( !( self && self->client ) )
	{
		return qfalse;
	}

	BotGetTargetPos( self->botMind->goal, pos );
	trap_BotUpdatePath( self->s.number, self->botMind->route, &self->botMind->numCorners, MAX_ROUTE_NODES, pos );

	if ( self->botMind->numCorners > 0 )
	{
		BotSetTarget( &target, NULL, &self->botMind->route[0] );
		BotGoto( self, target );
		return qfalse;
	}
	return qtrue;
}

unsigned int FindRouteToTarget( gentity_t *self, botTarget_t target )
{
	vec3_t targetPos;

	if ( BotTargetIsEntity( target ) )
	{
		PlantEntityOnGround( target.ent, targetPos );
	}
	else
	{
		BotGetTargetPos( target, targetPos );
	}

	return trap_BotFindRoute( self->s.number, targetPos );
}

/*
========================
Misc Bot Nav
========================
*/

void PlantEntityOnGround( gentity_t *ent, vec3_t groundPos )
{
	vec3_t mins, maxs;
	trace_t trace;
	vec3_t realPos;
	const int traceLength = 1000;
	vec3_t endPos;
	vec3_t traceDir;
	if ( ent->client )
	{
		BG_ClassBoundingBox( ( class_t )ent->client->ps.stats[STAT_CLASS], mins, maxs, NULL, NULL, NULL );
	}
	else if ( ent->s.eType == ET_BUILDABLE )
	{
		BG_BuildableBoundingBox( ( buildable_t )ent->s.modelindex, mins, maxs );
	}
	else
	{
		VectorCopy( ent->r.mins, mins );
		VectorCopy( ent->r.maxs, maxs );
	}

	VectorSet( traceDir, 0, 0, -1 );
	VectorCopy( ent->s.origin, realPos );
	VectorMA( realPos, traceLength, traceDir, endPos );
	trap_Trace( &trace, ent->s.origin, mins, maxs, endPos, ent->s.number, CONTENTS_SOLID );
	if ( trace.fraction != 1.0f )
	{
		VectorCopy( trace.endpos, groundPos );
	}
	else
	{
		VectorCopy( realPos, groundPos );
		groundPos[2] += mins[2];

	}
}
