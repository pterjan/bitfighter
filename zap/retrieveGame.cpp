//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "retrieveGame.h"

#include "game.h"
#include "goalZone.h"
#include "HelpItemManager.h"
#include "Level.h"               // Resharper thinks this is not needed, but it is
#include "ship.h"

namespace Zap
{


// Constructor
RetrieveGameType::RetrieveGameType()
{
   // Do nothing
}

// Destructor
RetrieveGameType::~RetrieveGameType()
{
   // Do nothing
}


bool RetrieveGameType::isFlagGame() const { return true; }


// Note -- neutral or enemy-to-all robots can't pick up the flag!!!
void RetrieveGameType::shipTouchFlag(Ship *theShip, FlagItem *theFlag)
{
   // See if the ship is already carrying a flag - can only carry one at a time
   if(theShip->isCarryingItem(FlagTypeNumber))
      return;

   // Can only pick up flags on your team or neutral
   if(theFlag->getTeam() != TEAM_NEUTRAL && theShip->getTeam() != theFlag->getTeam())
      return;

   // See if this flag is already in a capture zone owned by the ship's team
   if(theFlag->getZone() != NULL && theFlag->getZone()->getTeam() == theShip->getTeam())
      return;

   static StringTableEntry stealString("%e0 stole a flag from team %e1!");
   static StringTableEntry takeString("%e0 of team %e1 took a flag!");
   static StringTableEntry oneFlagTakeString("%e0 of team %e1 took the flag!");

   StringTableEntry r = takeString;

   if(getGame()->getLevel()->getObjectCount(FlagTypeNumber) == 1)
      r = oneFlagTakeString;

   ClientInfo *clientInfo = theShip->getClientInfo();
   if(!clientInfo)
      return;

   S32 team;
   if(theFlag->getZone() == NULL)      // Picked up flag just sitting around
      team = theShip->getTeam();
   else                                // Grabbed flag from enemy zone
   {
      r = stealString;
      team = theFlag->getZone()->getTeam();
      updateScore(team, LostFlag);

      clientInfo->getStatistics()->mFlagReturn++;  // used as flag steal
   }

   clientInfo->getStatistics()->mFlagPickup++;

   Vector<StringTableEntry> e;
   e.push_back(clientInfo->getName());
   e.push_back(getGame()->getTeamName(team));

   broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagSnatch, r, e);

   theFlag->mountToShip(theShip);
   updateScore(theShip, RemoveFlagFromEnemyZone);
   theFlag->setZone(NULL);
}


void RetrieveGameType::itemDropped(Ship *ship, MoveItem *item, DismountMode dismountMode)
{
   Parent::itemDropped(ship, item, dismountMode);

   if(item->getObjectTypeNumber() == FlagTypeNumber)
   {
      if(dismountMode != DISMOUNT_SILENT)
      {
         if(ship->getClientInfo())
         {
            static StringTableEntry dropString("%e0 dropped a flag!");
            Vector<StringTableEntry> e;

            e.push_back(ship->getClientInfo()->getName());

            broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagDrop, dropString, e);
         }
      }
   }
}


// The ship has entered a drop zone, either friend or foe
void RetrieveGameType::shipTouchZone(Ship *s, GoalZone *z)
{
   // See if this is an opposing team's zone.  If so, do nothing.
   if(s->getTeam() != z->getTeam())
      return;

   // See if this zone already has a flag in it.  If so, do nothing.
   const Vector<DatabaseObject *> *flags = getGame()->getLevel()->findObjects_fast(FlagTypeNumber);

   for(S32 i = 0; i < flags->size(); i++)
      if(static_cast<FlagItem *>(flags->get(i))->getZone() == z)
         return;

   // Ok, it's an empty zone on our team: See if this ship is carrying a flag...
   S32 flagIndex = s->getFlagIndex();

   if(flagIndex == NO_FLAG)
      return;

   // Ok, the ship has a flag and it's on the ship and we're in an empty zone
   MoveItem *item = s->getMountedItem(flagIndex);

   if(item->getObjectTypeNumber() == FlagTypeNumber)
   {
      FlagItem *mountedFlag = static_cast<FlagItem *>(item);

      static StringTableEntry capString("%e0 retrieved a flag!");
      static StringTableEntry oneFlagCapString("%e0 retrieved the flag!");

      Vector<StringTableEntry> e;
      e.push_back(s->getClientInfo()->getName());
      broadcastMessage(GameConnection::ColorNuclearGreen, SFXFlagCapture, 
                       (getGame()->getLevel()->getObjectCount(FlagTypeNumber) == 1) ? oneFlagCapString : capString, e);

      // Drop the flag into the zone
      mountedFlag->dismount(DISMOUNT_SILENT);

      const Vector<DatabaseObject *> *flags = getGame()->getLevel()->findObjects_fast(FlagTypeNumber);
      S32 flagIndex = flags->getIndex(mountedFlag);

      static_cast<FlagItem *>(flags->get(flagIndex))->setZone(z);
      mountedFlag->setActualPos(z->getExtent().getCenter());

      // Score the flag...
      updateScore(s, ReturnFlagToZone);

      s->getClientInfo()->getStatistics()->mFlagScore++;

      // See if all the flags are owned by one team...
      for(S32 i = 0; i < flags->size(); i++)
      {
         FlagItem *flag = static_cast<FlagItem *>(flags->get(i));

         bool ourFlag = (flag->getTeam() == s->getTeam()) || (flag->getTeam() == TEAM_NEUTRAL);
         if(ourFlag && (!flag->getZone() || flag->getZone()->getTeam() != s->getTeam()))
            return;     // ...if not, we're done
      }

      // One team has all the flags
      if(flags->size() != 1)
      {
         static StringTableEntry capAllString("Team %e0 retrieved all the flags!");
         e[0] = getGame()->getTeamName(s->getTeam());

         for(S32 i = 0; i < getGame()->getClientCount(); i++)
            if(!getGame()->getClientInfo(i)->isRobot())
            {
               if(isGameOver())  // Avoid flooding messages on game over. (empty formatString)
                  getGame()->getClientInfo(i)->getConnection()->s2cTouchdownScored(SFXNone, s->getTeam(), StringTableEntry(), e, s->getPos());
               else
                  getGame()->getClientInfo(i)->getConnection()->s2cTouchdownScored(SFXFlagCapture, s->getTeam(), capAllString, e, s->getPos());
            }
      }

      // Return all the flags to their starting locations if need be
      for(S32 i = 0; i < flags->size(); i++)
      {
         FlagItem *flag = static_cast<FlagItem *>(flags->get(i));

         if(flag->getTeam() == s->getTeam() || flag->getTeam() == TEAM_NEUTRAL) 
         {
            flag->setZone(NULL);
            if(!flag->isAtHome())
               flag->sendHome();
         }
      }
   }
}


// A major scoring event has ocurred -- in this case, it's all flags being collected by one team
void RetrieveGameType::majorScoringEventOcurred(S32 team)
{
   mZoneGlowTimer.reset();
   mGlowingZoneTeam = team;
}


void RetrieveGameType::onOvertimeStarted()
{
   startSuddenDeath();
}


// Same code as in HTF, CTF
void RetrieveGameType::performProxyScopeQuery(BfObject *scopeObject, ClientInfo *clientInfo)
{
   Parent::performProxyScopeQuery(scopeObject, clientInfo);

   GameConnection *connection = clientInfo->getConnection();

   S32 uTeam = scopeObject->getTeam();

   const Vector<DatabaseObject *> *flags = getGame()->getLevel()->findObjects_fast(FlagTypeNumber);
   for(S32 i = 0; i < flags->size(); i++)
   {
      FlagItem *flag = static_cast<FlagItem *>(flags->get(i));

      if(flag->isAtHome() || flag->getZone())
         connection->objectInScope(flag);
      else
      {
         Ship *mount = flag->getMount();
         if(mount && mount->getTeam() == uTeam)
         {
            connection->objectInScope(mount);
            connection->objectInScope(flag);
         }
      }
   }
}


// Runs on client
void RetrieveGameType::renderInterfaceOverlay(S32 canvasWidth, S32 canvasHeight) const
{
#ifndef ZAP_DEDICATED

   Parent::renderInterfaceOverlay(canvasWidth, canvasHeight);

   Ship *ship = getGame()->getLocalPlayerShip();

   if(!ship)
      return;

   bool uFlag = false;   // What does this mean?
   S32 team = ship->getTeam();

   const Vector<DatabaseObject *> *goalZones = getGame()->getLevel()->findObjects_fast(GoalZoneTypeNumber);
   const Vector<DatabaseObject *> *flags = getGame()->getLevel()->findObjects_fast(FlagTypeNumber);

   for(S32 i = 0; i < flags->size(); i++)
   {
      if(static_cast<FlagItem *>(flags->get(i))->getMount() == ship)
      {
         for(S32 j = 0; j < goalZones->size(); j++)
         {
            GoalZone *goalZone = static_cast<GoalZone *>(goalZones->get(j));

            // See if this is one of our zones and that it doesn't have a flag in it.
            if(goalZone->getTeam() != team)
               continue;

            bool found = false;
            for(S32 k = 0; k < flags->size(); k++)
               if(static_cast<FlagItem *>(flags->get(k))->getZone() == goalZone)
               {
                  found = true;
                  break;
               }

            if(!found)
               renderObjectiveArrow(goalZone, canvasWidth, canvasHeight);
         }
         uFlag = true;
         break;
      }
   }

   for(S32 i = 0; i < flags->size(); i++)
   {
      FlagItem *flag = static_cast<FlagItem *>(flags->get(i));
      if(!flag->isMounted() && !uFlag)
      {
         GoalZone *gz = flag->getZone();

         // If flag is on a team's zone, render the objective arrow as the team's color
         if(gz && gz->getTeam() != team)
            renderObjectiveArrow(flag, gz->getColor(), canvasWidth, canvasHeight);

         // Else if flag is not on a zone, render objective arrow as flag team's color (but only
         // flags you can pick up, non-enemies')
         else if(!gz && (flag->getTeam() == TEAM_NEUTRAL || flag->getTeam() == team))
            renderObjectiveArrow(flag, getTeamColor(flag->getTeam()), canvasWidth, canvasHeight);
      }
      else
      {
         // Arrow to ship carrying flag
         Ship *mount = flag->getMount();

         if(mount && mount != ship)
            renderObjectiveArrow(mount, canvasWidth, canvasHeight);
      }
   }
#endif
}


// What does a particular scoring event score?
S32 RetrieveGameType::getEventScore(ScoringGroup scoreGroup, ScoringEvent scoreEvent, S32 data)
{
   if(scoreGroup == TeamScore)
   {
      switch(scoreEvent)
      {
         case KillEnemy:
            return 0;
         case KilledByAsteroid:  // Fall through OK
         case KilledByTurret:    // Fall through OK
         case KillSelf:
            return 0;
         case KillTeammate:
            return 0;
         case KillEnemyTurret:
            return 0;
         case KillOwnTurret:
            return 0;
         case ReturnFlagToZone:
            return 1;
         case RemoveFlagFromEnemyZone:
            return 0;
         case LostFlag:    // Not really an individual scoring event!
            return -1;
         case ScoreSetByScript:
            return data;
         default:
            return naScore;
      }
   }
   else  // scoreGroup == IndividualScore
   {
      switch(scoreEvent)
      {
         case KillEnemy:
            return 1;
         case KilledByAsteroid:  // Fall through OK
         case KilledByTurret:    // Fall through OK
         case KillSelf:
            return -1;
         case KillTeammate:
            return 0;
         case KillEnemyTurret:
            return 1;
         case KillOwnTurret:
            return -1;
         case ReturnFlagToZone:
            return 2;
         case RemoveFlagFromEnemyZone:
            return 1;
            // case LostFlag:    // Not really an individual scoring event!
            //    return 0;
         default:
            return naScore;
      }
   }
}


GameTypeId RetrieveGameType::getGameTypeId() const { return RetrieveGame; }
const char *RetrieveGameType::getShortName() const { return "Ret"; }

static const char *instructions[] = { "Find all the flags, and bring",  "them to your capture zones!" };
const char **RetrieveGameType::getInstructionString() const { return instructions; }
HelpItem RetrieveGameType::getGameStartInlineHelpItem() const { return RetGameStartItem; }

bool RetrieveGameType::isTeamGame()          const { return true;  }
bool RetrieveGameType::canBeTeamGame()       const { return true;  }
bool RetrieveGameType::canBeIndividualGame() const { return false; }


TNL_IMPLEMENT_NETOBJECT(RetrieveGameType);


};


