//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "TimeLeftRenderer.h"

#include "ClientGame.h"
#include "gameType.h"
#include "DisplayManager.h"
#include "FontManager.h"
#include "Colors.h"
#include "GameObjectRender.h"    // For renderFlag()

#include "stringUtils.h"
#include "RenderUtils.h"


namespace Zap { 

namespace UI {

static const S32 BigScoreTextSize = 28;
static const S32 BigScoreTextGap  =  5;

// Constructor
TimeLeftRenderer::TimeLeftRenderer()
{
   mScreenInfo = DisplayManager::getScreenInfo();
   TNLAssert(mScreenInfo != NULL, "ScreenInfo is NULL!");

   mLeadingPlayer = NONE;
   mSecondLeadingPlayer = NONE;
   mLeadingPlayerScore = 0;
   mSecondLeadingPlayerScore = 0;
}


// Sets mLeadingTeamScore and mLeadingTeam; runs on client only
void TimeLeftRenderer::updateLeadingPlayerAndScore(const Game *game)
{
   mLeadingPlayerScore = S32_MIN;
   mLeadingPlayer = NONE;
   mSecondLeadingPlayerScore = S32_MIN;
   mSecondLeadingPlayer = NONE;

   // Find the leading player
   for(S32 i = 0; i < game->getClientCount(); i++)
   {
      // Check to make sure client hasn't disappeared somehow
      if(!game->getClientInfo(i))  
         continue;

      S32 score = game->getPlayerScore(i);

      if(score > mLeadingPlayerScore)
      {
         // Demote leading player to 2nd place
         mSecondLeadingPlayerScore = mLeadingPlayerScore;
         mSecondLeadingPlayer = mLeadingPlayer;

         mLeadingPlayerScore = score;
         mLeadingPlayer = i;

         continue;
      }

      if(score > mSecondLeadingPlayerScore)
      {
         mSecondLeadingPlayerScore = score;
         mSecondLeadingPlayer = i;
      }
   }
}


S32 TimeLeftRenderer::renderLock(S32 xPos, bool render)
{
   static const S32 lockSize = 14;
   static const S32 renderWidth = GameObjectRender::getLockWidth(lockSize);
   static const S32 margin = 4;
   static const S32 totalWidth = renderWidth + margin;

   if(render)
   {
      S32 y = DisplayManager::getScreenInfo()->getGameCanvasHeight() - TimeLeftIndicatorMargin - lockSize - 2;
    
      GameObjectRender::renderLock(Point(xPos - totalWidth / 2, y + lockSize), lockSize, Colors::white);
   }

   return totalWidth;
}


// When render param is true, will render as expected; when false, will simply return dimensions
Point TimeLeftRenderer::render(const GameType *gameType, bool scoreboardVisible, bool teamsLocked, bool render) const
{
   FontManager::pushFontContext(TimeLeftHeadlineContext);

   Point corner = renderTimeLeft(gameType, teamsLocked, render);
   S32 timeTop  = (S32)corner.y;

   // Convert the coordinates we got above into dimensions
   corner.x = mScreenInfo->getGameCanvasWidth()  - corner.x - TimeLeftIndicatorMargin;    // Width
   corner.y = mScreenInfo->getGameCanvasHeight() - corner.y - TimeLeftIndicatorMargin;    // Height

   // Some game types *ahem* Nexus *ahem* require an extra line for the scoreboard... a "special" if you will
   const S32 timeLeftSpecialHeight = gameType->renderTimeLeftSpecial(
                  (mScreenInfo->getGameCanvasWidth() - TimeLeftIndicatorMargin), timeTop, render);

   timeTop  -= timeLeftSpecialHeight;
   corner.y += timeLeftSpecialHeight;

   if(!scoreboardVisible)
   {
      Game *game = gameType->getGame();
      S32 teamCount = game->getTeamCount();

      if(teamCount > 1 && gameType->isTeamGame())
         corner.y += renderTeamScores(gameType, timeTop, render);

      else if(teamCount > 0 && !gameType->isTeamGame())     // For single team games like rabbit and bitmatch
         corner.y += renderIndividualScores(gameType, timeTop, render);
   }

   FontManager::popFontContext();

   return corner;
}


// Draw the scores for each team, with an adjacent flag
S32 TimeLeftRenderer::renderTeamScores(const GameType *gameType, S32 bottom, bool render) const
{
   Game *game = gameType->getGame();

   S32 maxWidth = render ? renderHeadlineScores(game, bottom) : 0;   // Use max score width to vertically align symbols
   S32 xpos = (mScreenInfo->getGameCanvasWidth() - TimeLeftIndicatorMargin) - maxWidth - 18;

   S32 ypos = bottom - BigScoreTextSize;      

   S32 teamCount = game->getTeamCount();

   for(S32 i = teamCount - 1; i >= 0; i--)
   {
      if(render)
         gameType->renderScoreboardOrnament(i, xpos, ypos);

      ypos -= (BigScoreTextSize + BigScoreTextGap);
   }

   return bottom - ypos - (BigScoreTextSize + BigScoreTextGap);
}


// ypos is the coordinate of the top of the bottom most score; because the position of the scores is anchored to the bottom,
// we'll render bottom to top
S32 TimeLeftRenderer::renderHeadlineScores(const Game *game, S32 ypos) const
{
   S32 teamCount = game->getTeamCount();
   S32 maxWidth = S32_MIN;

   // Draw bottom to top
   for(S32 i = teamCount - 1; i >= 0; i--)
   {
      TNLAssert(dynamic_cast<Team *>(game->getTeam(i)), "Bad team pointer or bad type");
      S32 score = static_cast<Team *>(game->getTeam(i))->getScore();
      //score = Platform::getRealMilliseconds() / 1000 % 20 * i;  // Uncomment to test display of various scores

      // This is a total hack based on visual inspection trying to get scores ending in 1 to align with others
      // in a way that is nice.  This is totally font dependent, sadly...

      S32 width = RenderUtils::drawStringfr_fixed((mScreenInfo->getGameCanvasWidth() - TimeLeftIndicatorMargin), ypos, BigScoreTextSize, Colors::white, "%d", score);
      maxWidth = max(maxWidth, width);

      ypos -= BigScoreTextSize + BigScoreTextGap;
   }

   return maxWidth;
}


// Try to mitigate some of the weirdness that comes from TTF hinting when trying to right-align text
static void drawStringDigitByDigit(S32 x, S32 y, S32 textsize, const Color &color, const string &s)
{
   // Note for the well-intentioned cast-killer... if you try to make i an unsigned int to avoid the cast,
   // this loop will crash, as i will be decremented below 0, and, well... bring on the velocoraptor.  
   // See http://xkcd.com/292/ for clarification.
   for(S32 i = (S32)s.length() - 1; i >= 0; i--)
      x -= RenderUtils::drawStringr(x, y, textsize, color, s.substr(i, 1).c_str());
}


// Render 1 or 2 scores: Either render the current client on the bottom (if only one player); 
// or renders player on top and the 2nd player on the bottom (if player is winning);
// or leader on top and player second (if player is losing)
S32 TimeLeftRenderer::renderIndividualScores(const GameType *gameType, S32 bottom, bool render) const
{
   Game *game = gameType->getGame();
   ClientGame *clientGame = static_cast<ClientGame *>(game);

   // We can get here before we get the first unpackUpdate packet arrives -- if so, return
   if(clientGame->getLocalRemoteClientInfo() == NULL)
      return 0;
   
   if(mLeadingPlayer == NONE)
      return 0;

   const S32 textsize = 12;
   const S32 textgap = 4;
   const S32 oneAdjFact = 2;

   S32 ypos = bottom - textsize; 

   /// Render player score
   bool hasSecondLeader = mSecondLeadingPlayer != NONE;

   const StringTableEntry localClientName = clientGame->getClientInfo()->getName();

   // The player is the leader if a leader is detected and it matches his name
   bool localClientIsLeader = (localClientName == game->getClientInfo(mLeadingPlayer)->getName());

   S32 botOneFixFactor = 0, botScoreLen = 0;

   bool renderTwoNames = hasSecondLeader || !localClientIsLeader;

   // Slide the first entry up if there will be a second entry
   S32 firstNameOffset = renderTwoNames ? (textsize + textgap) : 0;    

   const char *topName  = game->getClientInfo(mLeadingPlayer)->getName().getString();
   S32 topScore = mLeadingPlayerScore;

   // This is a total hack based on visual inspection trying to get scores ending in 1 to align with others
   // in a way that is nice.  This is totally font dependent, sadly...
   S32 topOneFixFactor = topScore % 10 == 1 ? oneAdjFact : 0;

   string topScoreStr = itos(topScore);
   S32 topScoreLen = RenderUtils::getStringWidth(textsize, topScoreStr.c_str()) + topOneFixFactor;

   const char *botName;
   string botScoreStr;
   S32 botScore;

   if(renderTwoNames)
   {
      // Should test if leader first
      if(!localClientIsLeader)
      {
         botName  = clientGame->getLocalRemoteClientInfo()->getName().getString();
         botScore = clientGame->getLocalRemoteClientInfo()->getScore();
      }
      // hasSecondLeader
      else
      {
         botName  = game->getClientInfo(mSecondLeadingPlayer)->getName().getString();
         botScore = mSecondLeadingPlayerScore;
      }

      //botScore = (Platform::getRealMilliseconds() / 500 % 10 )* 2;
      
      botOneFixFactor = botScore % 10 == 1 ? oneAdjFact : 0;
      botScoreStr = itos(botScore);
      botScoreLen = RenderUtils::getStringWidth(textsize, botScoreStr.c_str()) + botOneFixFactor;
   }

   // 5 here is the gap between the names and the scores
   S32 maxWidth = max(topScoreLen, botScoreLen) + 5;

   if(render)
   {
      drawStringDigitByDigit  ((mScreenInfo->getGameCanvasWidth() - TimeLeftIndicatorMargin) - topOneFixFactor, 
                               ypos - firstNameOffset, textsize, Colors::white, topScoreStr);
      RenderUtils::drawStringr((mScreenInfo->getGameCanvasWidth() - TimeLeftIndicatorMargin) - maxWidth,
                               ypos - firstNameOffset, textsize, Colors::white, topName);

      // Render bottom score if we have one
      if(renderTwoNames)
      {
         //                                                winner color : loser color
         const Color &color = (topScore == botScore) ? Colors::red : Colors::red60;

         drawStringDigitByDigit  ((mScreenInfo->getGameCanvasWidth() - TimeLeftIndicatorMargin) - botOneFixFactor, 
                                    ypos, textsize, color, botScoreStr);
         RenderUtils::drawStringr((mScreenInfo->getGameCanvasWidth() - TimeLeftIndicatorMargin) - maxWidth,
                                    ypos, textsize, color, botName);
      }
   }

   return firstNameOffset + textsize;
}


// Returns y-coord of top of display, and width of display, which we can use to position other elements 
// If render is true, will draw display, if false, will only calculate dimensions
Point TimeLeftRenderer::renderTimeLeft(const GameType *gameType, bool includeLock, bool render) const
{
   const S32 siSize = 12;                 // Size of stacked indicators
   const S32 grayLineHorizPadding = 4;
   const S32 grayLineVertPadding = -1;

   static const char *SuddenDeathMsg = "SUDDEN DEATH";
   static const char *UnlimMsg       = "Unlim.";

   // Precalc some widths we'll need from time to time -- these will never change
   static const U32 w0        = RenderUtils::getStringWidth(TimeTextSize, "0");
   static const U32 wUnlim    = RenderUtils::getStringWidth(TimeTextSize, UnlimMsg);
   static const U32 wSudDeath = RenderUtils::getStringWidth(TimeTextSize, SuddenDeathMsg);

   static const S32 gameAlmostOverMs = 10 * 1000;  // Ten seconds

   U32 timeWidth;
   if(gameType->isTimeUnlimited())
      timeWidth = wUnlim;
   else if(gameType->isSuddenDeath())
      timeWidth = wSudDeath;
   else
   {
      // Get the width of the minutes and 10 seconds digit(s)
      // By using the actual minutes, we get a better fit while not changing the positioning too often
      const U32 minsRemaining = gameType->getRemainingGameTimeInMs() / (60 * 1000);
      const U32 tenSecsRemaining = gameType->getRemainingGameTimeInMs() / 1000 % 60 / 10;
      const string timestr = itos(minsRemaining) + ":" + itos(tenSecsRemaining);
      timeWidth = RenderUtils::getStringWidth(TimeTextSize, timestr.c_str()) + w0;

      // Add a little extra for the leading 0 that's drawn for one digit times
      if(minsRemaining < 10)
         timeWidth += w0;
   }

   S32 canvasWidth = mScreenInfo->getGameCanvasWidth();

   // grayLinePos --> Where the vertical gray line is drawn
   const S32 grayLinePos = (canvasWidth - TimeLeftIndicatorMargin) - timeWidth - grayLineHorizPadding;
   const S32 smallTextRPos = grayLinePos - grayLineHorizPadding;                // Right-align the stacked text here
   
   // Left and bottom coordinates of the time display
   const S32 timeLeft = (canvasWidth - TimeLeftIndicatorMargin) - timeWidth;
   const S32 timeBottom = mScreenInfo->getGameCanvasHeight() - TimeLeftIndicatorMargin;

   S32 wt, wb;    // Width of top and bottom items respectively

   S32 stwSizeBonus = 1;

   if(render)
   {
      // Align with top of time, + 6 is a font-dependent fudge factor
      wt = RenderUtils::drawStringfr_fixed(smallTextRPos, timeBottom - TimeTextSize + 6 + siSize, siSize, Colors::cyan, gameType->getShortName());

      // Align with bottom of time
      wb = RenderUtils::drawStringfr_fixed(smallTextRPos, timeBottom, siSize + stwSizeBonus,
                                           Colors::red, itos(gameType->getWinningScore()).c_str());

      // Draw the time itself
      const Color &color = (gameType->isOvertime() || gameType->getRemainingGameTimeInMs() < gameAlmostOverMs) ? Colors::red : Colors::white;

      if(gameType->isTimeUnlimited())  
         RenderUtils::drawString_fixed(timeLeft, timeBottom, TimeTextSize, color, UnlimMsg);
      else if(gameType->isSuddenDeath())
         RenderUtils::drawString_fixed(timeLeft, timeBottom, TimeTextSize, color, SuddenDeathMsg);
      else
         RenderUtils::drawTime(timeLeft, timeBottom, TimeTextSize, color, gameType->getRemainingGameTimeInMs());
   }
   else     // Not rendering
   {
      wt = RenderUtils::getStringWidth(siSize, gameType->getShortName());
      wb = RenderUtils::getStringWidth(siSize + stwSizeBonus, itos(gameType->getWinningScore()).c_str());
   }

   const S32 leftLineOverhangAmount = 4;
   S32 farLeftCoord = smallTextRPos - max(wt, wb) - leftLineOverhangAmount;

   if(render)
   {
      const S32 visualVerticalTextAlignmentHackyFacty = 8;     // This is that little gap btwn the gray vert and horiz lines
      const S32 timeTop = timeBottom - TimeTextSize;
      const Color &gray = Colors::gray40;

      RenderUtils::drawHorizLine(farLeftCoord, (canvasWidth - TimeLeftIndicatorMargin), timeTop - grayLineVertPadding, gray);
      RenderUtils::drawVertLine(grayLinePos, timeBottom, timeTop + visualVerticalTextAlignmentHackyFacty, gray);
   }

   if(includeLock)
      farLeftCoord -= renderLock(farLeftCoord, render);

   // Adjusting this topCord will control how much space above the horiz gray line there is before the flags or other junk is drawn
   const S32 topCoord = timeBottom - 2 * grayLineVertPadding - TimeTextSize - (S32)RenderUtils::DEFAULT_LINE_WIDTH - 8;

   return Point(farLeftCoord, topCoord);
}


} }      // Nested namespaces
