//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------


#include "UI.h"

#include "UIChat.h"
#include "UIDiagnostics.h"
#include "UIMenus.h"
#include "UIManager.h"

#include "GameManager.h"

#include "ClientGame.h"
#include "Console.h"             // For console rendering
#include "Colors.h"
#include "DisplayManager.h"
#include "Joystick.h"
#include "masterConnection.h"    // For MasterServerConnection def
#include "VideoSystem.h"
#include "SoundSystem.h"
#include "LoadoutIndicator.h"    // For LoadoutIndicatorHeight
#include "ScreenShooter.h"

#include "FontManager.h"

#include "MathUtils.h"           // For RADIANS_TO_DEGREES def
#include "RenderUtils.h"

#include <string>

using namespace std;
using namespace TNL;

namespace Zap
{

using namespace UI;

// Define statics
S32 UserInterface::messageMargin = UserInterface::vertMargin + UI::LoadoutIndicator::LoadoutIndicatorHeight + 5;


////////////////////////////////////////
////////////////////////////////////////

// Constructor
UserInterface::UserInterface(ClientGame *clientGame, UIManager *uiManager)
{
   TNLAssert(clientGame, "Need a ClientGame to get settings!");

   mClientGame = clientGame;
   mGameSettings = mClientGame->getSettings();
   mTimeSinceLastInput = 0;
   mDisableShipKeyboardInput = true;
   mUiManager = uiManager;
}


UserInterface::~UserInterface()
{
   // Do nothing
}


void UserInterface::setUiManager(UIManager *uiManager)
{
   mUiManager = uiManager;
}


ClientGame *UserInterface::getGame() const
{
   return mClientGame;
}


UIManager *UserInterface::getUIManager() const 
{ 
   return mUiManager;
}


bool UserInterface::usesEditorScreenMode() const
{
   return false;
}


void UserInterface::activate()
{
   onActivate(); 
}


void UserInterface::reactivate()
{
   onReactivate();
}


void UserInterface::onActivate()          { /* Do nothing */ }
void UserInterface::onReactivate()        { /* Do nothing */ }
void UserInterface::onDisplayModeChange() { /* Do nothing */ }


void UserInterface::onDeactivate(bool nextUIUsesEditorScreenMode)
{
   if(nextUIUsesEditorScreenMode != usesEditorScreenMode())
      VideoSystem::actualizeScreenMode(mGameSettings, true, nextUIUsesEditorScreenMode);
}


U32 UserInterface::getTimeSinceLastInput()
{
   return mTimeSinceLastInput;
}


void UserInterface::playBoop()
{
   SoundSystem::playSoundEffect(SFXUIBoop, 1);
}


// Render master connection state if we're not connected
void UserInterface::renderMasterStatus(const MasterServerConnection *connectionToMaster) const
{
   if(connectionToMaster && connectionToMaster->getConnectionState() != NetConnection::Connected)
      RenderUtils::drawStringf_fixed(10, 565, 15, Colors::white, "Master Server - %s",
                     GameConnection::getConnectionStateString(connectionToMaster->getConnectionState()));
}


void UserInterface::renderConsole() const
{
#ifndef BF_NO_CONSOLE
   // Temporarily disable scissors mode so we can use the full width of the screen
   // to show our console text, black bars be damned!
//   bool scissorMode = mGL->isEnabled(GLOPT::ScissorTest);
//
//   if(scissorMode)
//      mGL->disable(GLOPT::ScissorTest);

   GameManager::gameConsole->render();

//   if(scissorMode)
//      mGL->enable(GLOPT::ScissorTest);
#endif
}


static const S32 TitleSize = 30;


void UserInterface::renderMessageBox(const string &titleStr, const string &instrStr, 
                                     const string &messageStr, const Color &color, S32 vertOffset, S32 style) const
{
   static const FontContext Context = ErrorMsgContext;
   static const S32 TextSize = 18;


   InputCodeManager *inputCodeManager = mGameSettings->getInputCodeManager();

   SymbolShapePtr title = SymbolShapePtr(new SymbolString(titleStr, inputCodeManager, Context, TitleSize, color, false));
   SymbolShapePtr instr = SymbolShapePtr(new SymbolString(instrStr, inputCodeManager, Context, TextSize, color, false));

   Vector<string> wrappedLines;
   RenderUtils::wrapString(messageStr, UIManager::MessageBoxWrapWidth, TextSize, Context, wrappedLines);

   Vector<SymbolShapePtr> message(wrappedLines.size());

   for(S32 i = 0; i < wrappedLines.size(); i++)
      message.push_back(SymbolShapePtr(new SymbolString(wrappedLines[i], inputCodeManager, Context, TextSize, color, true)));

   renderMessageBox(title, instr, message.address(), message.size(), vertOffset, style);
}


void UserInterface::renderCenteredFancyBox(S32 boxTop, S32 boxHeight, S32 inset, S32 cornerInset, const Color &fillColor, 
                                           F32 fillAlpha, const Color &borderColor)
{
   RenderUtils::drawFilledFancyBox(inset, boxTop, DisplayManager::getScreenInfo()->getGameCanvasWidth() - inset, boxTop + boxHeight, cornerInset, fillColor, fillAlpha, borderColor);
}


// Note that title and instr can be NULL
void UserInterface::renderMessageBox(const SymbolShapePtr &title, const SymbolShapePtr &instr, 
                                     const SymbolShapePtr *message, S32 msgLines, S32 vertOffset, S32 style) const
{
   const S32 canvasWidth  = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   const S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   const S32 instrGap = 20;            // Gap between last line of text and instruction line
   const S32 instrGapBottom = 5;       // A bit of extra gap below the instr. line

   const F32 LinespacingFactor = 1.333f;

   S32 titleHeight = title ? title->getHeight() : 0;
   if(titleHeight > 0)
      titleHeight += 30;      // 30 is the gap between the title and the message

   S32 instrHeight = instr ? instr->getHeight() : 0;
   if(instrHeight > 0)
      instrHeight += instrGap + instrGapBottom;

   S32 boxHeight = vertMargin + titleHeight + instrHeight + vertMargin;

   // Don't include a linespacing gap after the final line
   for(S32 i = 0; i < msgLines; i++ )
      boxHeight += S32(message[i]->getHeight() * LinespacingFactor);

   S32 boxTop = (canvasHeight - boxHeight) / 2 + vertOffset;

   static const S32 HorizBoxPadding = 20;

   S32 maxLen = title ? title->getWidth() + HorizBoxPadding * 2 : 0;

   for(S32 i = 0; i < msgLines; i++)
   {
      S32 len = message[i]->getWidth() + HorizBoxPadding * 2;
      if(len > maxLen)
         maxLen = len;
   }

   S32 boxwidth = max(UIManager::MessageBoxWrapWidth, maxLen);
   S32 inset = (canvasWidth - boxwidth) / 2;   // Inset for left and right edges of box

   if(style == 1)       
      renderCenteredFancyBox(boxTop, boxHeight, inset, 15, Colors::red30, 1.0f, Colors::white);
   else if(style == 2)
      renderCenteredFancyBox(boxTop, boxHeight, inset, 15, Colors::black, 0.70f, Colors::blue);

   // Draw title
   if(titleHeight > 0)
      title->render(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2, boxTop + vertMargin + TitleSize, AlignmentCenter);

   // Draw messages -- since symbolstrings are positioned by specifying the baseline, we need to advance y by the height of
   // the first message so that it will be drawn in the correct location
   S32 y = boxTop + vertMargin + titleHeight + message[0]->getHeight();

   for(S32 i = 0; i < msgLines; i++)
   {
      message[i]->render(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2, y, AlignmentCenter);
      y += S32(message[i]->getHeight() * LinespacingFactor);
   }

   // And footer
   if(instrHeight > 0)
      instr->render(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2, boxTop + boxHeight - vertMargin - instrGapBottom, AlignmentCenter);
}


// Static method
void UserInterface::dimUnderlyingUI(F32 alpha)
{
   RenderUtils::drawFilledRect(0, 0, DisplayManager::getScreenInfo()->getGameCanvasWidth(), DisplayManager::getScreenInfo()->getGameCanvasHeight(), Colors::black, alpha);
}


// Draw blue rectangle around selected menu item
void UserInterface::drawMenuItemHighlight(S32 x1, S32 y1, S32 x2, S32 y2, bool disabled)
{
   if(disabled)
      RenderUtils::drawFilledRect(x1, y1, x2, y2, Colors::gray40, Colors::gray80);
   else
      RenderUtils::drawFilledRect(x1, y1, x2, y2, Colors::blue40, Colors::blue);
}


// These will be overridden in child classes if needed
void UserInterface::render() const
{ 
   // Do nothing -- probably never even gets called
}


void UserInterface::idle(U32 timeDelta)
{ 
   mTimeSinceLastInput += timeDelta;
}


void UserInterface::onMouseMoved()                         
{ 
   mTimeSinceLastInput = 0;
}


void UserInterface::onMouseDragged()  { /* Do nothing */ }


// Static method
InputCode UserInterface::getInputCode(GameSettings *settings, BindingNameEnum binding)
{
   return settings->getInputCodeManager()->getBinding(binding);
}


string UserInterface::getEditorBindingString(EditorBindingNameEnum binding) const
{
   return mGameSettings->getInputCodeManager()->getEditorBinding(binding);
}


string UserInterface::getSpecialBindingString(SpecialBindingNameEnum binding) const
{
   return mGameSettings->getInputCodeManager()->getSpecialBinding(binding);
}


InputCode UserInterface::getEditorBindingInputCode(EditorBindingCodeEnum binding) const
{
   return mGameSettings->getInputCodeManager()->getEditorBinding(binding);
}


void UserInterface::setInputCode(BindingNameEnum binding, InputCode inputCode)
{
   mGameSettings->getInputCodeManager()->setBinding(binding, inputCode);
}


bool UserInterface::checkInputCode(BindingNameEnum binding, InputCode inputCode) const
{
   InputCode bindingCode = getInputCode(mGameSettings, binding);

   // Handle modified keys
   if(InputCodeManager::isModified(bindingCode))
      return inputCode == InputCodeManager::getBaseKeySpecialSequence(bindingCode) &&
             InputCodeManager::checkModifier(InputCodeManager::getModifier(bindingCode));

   // Otherwise, just do a simple key check.  filterInputCode deals with the numeric keypad.
   return bindingCode == mGameSettings->getInputCodeManager()->filterInputCode(inputCode);
}


bool UserInterface::checkInputCode(SpecialBindingNameEnum binding, const string &inputString) const
{
   return inputString == getSpecialBindingString(binding);
}


const char *UserInterface::getInputCodeString(BindingNameEnum binding) const
{
   return InputCodeManager::inputCodeToString(getInputCode(mGameSettings, binding));
}


class ChatUserInterface;
class NameEntryUserInterface;
class DiagnosticUserInterface;
 
bool UserInterface::onKeyDown(InputCode inputCode)
{ 
   mTimeSinceLastInput = 0;

   bool handled = false;

   UIManager *uiManager = getUIManager();
   string inputString = InputCodeManager::getCurrentInputString(inputCode);

   if(checkInputCode(BINDING_DIAG, inputString))               // Turn on diagnostic overlay
   { 
      if(uiManager->isCurrentUI<DiagnosticUserInterface>())
         return false;

      uiManager->activate<DiagnosticUserInterface>();

      playBoop();
      
      handled = true;
   }
   else if(checkInputCode(BINDING_LOBBYCHAT, inputString))     // Turn on Lobby Chat overlay
   {
      // Don't activate if we're already in chat or if we're on the Name Entry
      // screen (since we don't have a nick yet)
      if(uiManager->isCurrentUI<ChatUserInterface>() || uiManager->isCurrentUI<NameEntryUserInterface>())
         return false;

      getUIManager()->activate<ChatUserInterface>();
      playBoop();

      handled = true;
   }
   
#ifndef BF_NO_SCREENSHOTS
   // Screenshot!
   else if(checkInputCode(BINDING_SCREENSHOT_1, inputString) || checkInputCode(BINDING_SCREENSHOT_2, inputString))
   {
      ScreenShooter::saveScreenshot(getUIManager(), mGameSettings);
      handled = true;
   }
#endif

   return handled;
}


void UserInterface::onKeyUp(InputCode inputCode) { /* Do nothing */ }
void UserInterface::onTextInput(char ascii)      { /* Do nothing */ }


// Dumps any keys and raw stick button inputs depressed to the screen when in diagnostic mode.
// This should make it easier to see what happens when users press joystick buttons.
void UserInterface::renderDiagnosticKeysOverlay()
{
   // This setting can't be changed from in-game, so we can just grab the value at the outset and use that to save the lookup
   static const bool dumpKeys = 
         GameManager::getClientGames()->get(0)->getSettings()->getSetting<YesNo>(IniKey::DumpKeys);

   if(dumpKeys)
   {
     static const S32 margin = 5;
     static const S32 fontSize = 18;
     S32 vpos = DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2 + fontSize;
     S32 hpos = horizMargin;

     // Key states
     for (U32 i = 0; i < MAX_INPUT_CODES; i++)
        if(InputCodeManager::getState((InputCode) i))
           hpos += RenderUtils::drawStringAndGetWidth_fixed(hpos, vpos, fontSize, Colors::white, InputCodeManager::inputCodeToString((InputCode)i));

      vpos += 41;
      hpos = horizMargin;

      for(U32 i = 0; i < CHAR_BIT * sizeof(Joystick::ButtonMask); i++)
         if(Joystick::ButtonMask & (1 << i))
            hpos += RenderUtils::drawStringAndGetWidthf_fixed(hpos, vpos, fontSize, Colors::magenta, "RawBut [%d]", i) + margin;
   }
}   


void UserInterface::onColorPicked(const Color &color)
{
   // Do nothing, expect this function should be overridden by other classes that use UIColorPicker
}


////////////////////////////////////////
////////////////////////////////////////


//void UserInterfaceData::get()
//{
//   vertMargin  = UserInterface::vertMargin;
//   horizMargin = UserInterface::horizMargin;
//   chatMargin  = UserInterface::messageMargin;
//}
//
//
//void UserInterfaceData::set()
//{
//   UserInterface::vertMargin  = vertMargin;
//   UserInterface::horizMargin = horizMargin;
//   UserInterface::messageMargin = chatMargin;
//}


};

