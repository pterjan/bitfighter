//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "SymbolShape.h"

#include "FontManager.h"
#include "InputCode.h"
#include "JoystickRender.h"

#include "GameObjectRender.h"
#include "Colors.h"

#include "RenderUtils.h"
#include "stringUtils.h"
#include "UIAbstractInstructions.h"
#include "GameManager.h"
#include "ClientGame.h"

#include <algorithm>

using namespace TNL;


namespace Zap { namespace UI {


void SymbolStringSetCollection::clear()
{
   mSymbolSet.clear();
   mAlignment.clear();
   mXPos.clear();
}


void SymbolStringSetCollection::addSymbolStringSet(const SymbolStringSet &set, Alignment alignment, S32 xpos)
{
   mSymbolSet.push_back(set);
   mAlignment.push_back(alignment);
   mXPos.push_back(xpos);
}


S32 SymbolStringSetCollection::render(S32 yPos) const
{
   S32 lines = -1;

   // Figure out how many lines in our tallest SymbolStringSet
   for(S32 i = 0; i < mSymbolSet.size(); i++)
      lines = max(lines, mSymbolSet[i].getItemCount());

   // Render the SymbolStringSets line-by-line, keeping all lines aligned with one another.
   // Make a tally of the total height along the way (using the height of the tallest item rendered).
   S32 totalHeight = 0;

   for(S32 i = 0; i < lines; i++)
   {
      S32 height = 0;

      for(S32 j = 0; j < mSymbolSet.size(); j++)
      {
         S32 h = mSymbolSet[j].renderLine(i, mXPos[j], yPos + totalHeight, mAlignment[j]);
         height = max(h, height);   // Find tallest
      }

      totalHeight += height;
   }

   return totalHeight;
}


////////////////////////////////////////
////////////////////////////////////////


SymbolStringSet::SymbolStringSet(S32 gap)
{
   mGap = gap;
}


void SymbolStringSet::clear()
{
   mSymbolStrings.clear();
}


void SymbolStringSet::add(const SymbolString &symbolString)
{
   mSymbolStrings.push_back(symbolString);
}


S32 SymbolStringSet::getHeight() const
{
   S32 height = 0;
   for(S32 i = 0; i < mSymbolStrings.size(); i++)
      height += mSymbolStrings[i].getHeight() + (mSymbolStrings[i].getHasGap() ? mGap : 0);

   return height;
}


S32 SymbolStringSet::getWidth() const
{
   S32 width = 0;
   for(S32 i = 0; i < mSymbolStrings.size(); i++)
      width = max(mSymbolStrings[i].getWidth(), width);

   return width;
}


S32 SymbolStringSet::getItemCount() const
{
   return mSymbolStrings.size();
}


S32 SymbolStringSet::render(S32 x, S32 y, Alignment alignment, S32 blockWidth) const
{
   return render(F32(x), F32(y), alignment, blockWidth);
}


S32 SymbolStringSet::render(F32 x, F32 yStart, Alignment alignment, S32 blockWidth) const
{
   S32 width = getWidth();
   S32 y = 0;

   for(S32 i = 0; i < mSymbolStrings.size(); i++)
   {
      mSymbolStrings[i].render(x, yStart + y, alignment, width);
      y += mSymbolStrings[i].getHeight() + mGap;
   }

   return y;
}


S32 SymbolStringSet::renderLine(S32 line, S32 x, S32 y, Alignment alignment) const
{
   // Make sure we're in bounds
   if(line >= mSymbolStrings.size())
      return 0;

   mSymbolStrings[line].render(x, y, alignment);
   return mSymbolStrings[line].getHeight() + (mSymbolStrings[line].getHasGap() ? mGap : 0);
}


////////////////////////////////////////
////////////////////////////////////////

// Width is the sum of the widths of all elements in the symbol list
static S32 computeWidth(const Vector<SymbolShapePtr > &symbols)
{
   S32 width = 0;

   for(S32 i = 0; i < symbols.size(); i++)
      width += symbols[i]->getWidth();

   return width;
}


// Width of a layered item is the widest of the widths of all elements in the symbol list
static S32 computeLayeredWidth(const Vector<SymbolShapePtr> &symbols)
{
   S32 width = 0;

   for(S32 i = 0; i < symbols.size(); i++)
   {
      S32 w = symbols[i]->getWidth();

      width = max(w, width);
   }

   return width;
}


// Height is the height of the tallest element in the symbol list
static S32 computeHeight(const Vector<SymbolShapePtr> &symbols)
{
   S32 height = 0;

   for(S32 i = 0; i < symbols.size(); i++)
   {
      S32 h = symbols[i]->getHeight();
      height = max(h, height);
   }

   return height;
}


// Constructor with symbols
SymbolString::SymbolString(const Vector<SymbolShapePtr> &symbols, Alignment alignment) : mSymbols(symbols)
{
   mWidth = computeWidth(symbols);
   mHeight = computeHeight(symbols);
   mAlignment = alignment;
}


SymbolString::SymbolString(const SymbolShapePtr &symbol, Alignment alignment)
{
   mSymbols.push_back(symbol);

   mWidth = symbol->getWidth();
   mHeight = symbol->getHeight();
   mAlignment = alignment;
}


// Convenience constructor, just pass in a string, we'll do the rest!
SymbolString::SymbolString(const string &str, const InputCodeManager *inputCodeManager, FontContext context, 
                           S32 textSize, const Color &color, bool blockMode, Alignment alignment)
{
   symbolParse(inputCodeManager, str, mSymbols, context, textSize, blockMode, color, color);
   mWidth = computeWidth(mSymbols);
   mHeight = computeHeight(mSymbols);
   mColor = color;
   mAlpha = 1.0f;
   mAlignment = alignment;
}


// Convenience constructor, just pass in a string, we'll do the rest!
SymbolString::SymbolString(const string &str, const InputCodeManager *inputCodeManager, FontContext context, 
                           S32 textSize, const Color &color, F32 alpha, bool blockMode, Alignment alignment)
{
   symbolParse(inputCodeManager, str, mSymbols, context, textSize, blockMode, color, color);
   mWidth = computeWidth(mSymbols);
   mHeight = computeHeight(mSymbols);
   mColor = color;
   mAlpha = alpha;
   mAlignment = alignment;
}


// Constructor -- symbols will be provided later
SymbolString::SymbolString()
{
   mWidth = 0;
   mHeight = 0;
   mAlignment = AlignmentNone;
}


// Destructor
SymbolString::~SymbolString()
{
  // Do nothing
}


void SymbolString::setSymbols(const Vector<SymbolShapePtr> &symbols)
{
   mSymbols = symbols;

   mWidth = computeWidth(symbols);
}


// Only used in editor for showing message related to updating levels in pleiades
void SymbolString::setSymbolsFromString(const string &string, const InputCodeManager *inputCodeManager, 
                                        FontContext fontContext, S32 textSize, const Color &color)
{
   Vector<SymbolShapePtr> symbols;
   symbolParse(inputCodeManager, string, symbols, fontContext, textSize, false, color, color);
   setSymbols(symbols);
}


void SymbolString::clear()
{
   mSymbols.clear();
   mWidth = 0;
   mHeight = 0;
}


S32 SymbolString::getWidth() const
{ 
   return mWidth;
}


S32 SymbolString::getHeight() const
{ 
   S32 height = 0;
   for(S32 i = 0; i < mSymbols.size(); i++)
      height = max(height, mSymbols[i]->getHeight());

   return height;
}


// Here to make class non-virtual
void SymbolString::render(const Point &pos) const
{
   render(pos, AlignmentCenter);
}


void SymbolString::render(S32 x, S32 y) const
{
   render(x, y, AlignmentCenter);
}


void SymbolString::render(const Point &center, Alignment alignment) const
{
   render((S32)center.x, (S32)center.y, alignment);
}


S32 SymbolString::render(S32 x, S32 y, Alignment blockAlignment, S32 blockWidth) const
{
   //FontManager::setFontColor(mColor);
   return render((F32)x, (F32)y, blockAlignment, blockWidth);
}


S32 SymbolString::render(F32 x, F32 y, Alignment blockAlignment, S32 blockWidth) const
{
   if(mSymbols.size() == 0)   // Nothing to render!
      return mHeight;

   // Alignment of overall symbol string
   if(blockAlignment == AlignmentCenter)
      x -= mWidth / 2;        // x is now at the left edge of the render area

   if(blockWidth > -1)
   {
      // Individual line alignment
      Alignment lineAlignment;
      if(mAlignment == AlignmentNone)
         lineAlignment = blockAlignment;
      else
         lineAlignment = mAlignment;

      // TODO: Need to handle more cases here...
      if(lineAlignment == AlignmentLeft && blockAlignment == AlignmentCenter)
         x -= (blockWidth - mWidth) / 2;
   }

   for(S32 i = 0; i < mSymbols.size(); i++)
   {
      S32 w = mSymbols[i]->getWidth();
      mSymbols[i]->render(Point(x + w / 2, y));
      x += w;
   }

   return mHeight;
}


bool SymbolString::getHasGap() const
{
   for(S32 i = 0; i < mSymbols.size(); i++)
      if(mSymbols[i]->getHasGap())
         return true;

   return false;
}


static const S32 buttonHalfHeight = 9;   // This is the default half-height of a button
static const S32 rectButtonWidth = 24;
static const S32 rectButtonHeight = 18;
static const S32 smallRectButtonWidth = 19;
static const S32 smallRectButtonHeight = 15;
static const S32 horizEllipseButtonDiameterX = 28;
static const S32 horizEllipseButtonDiameterY = 16;
static const S32 rightTriangleWidth = 28;
//static const S32 rightTriangleHeight = 18;
static const S32 RectRadius = 3;
static const S32 RoundedRectRadius = 5;


static SymbolShapePtr getSymbol(InputCode inputCode, const Color &color);

static SymbolShapePtr getSymbol(Joystick::ButtonShape shape, const Color &color)
{
   switch(shape)
   {
      case Joystick::ButtonShapeRound:
         return SymbolShapePtr(new SymbolCircle(buttonHalfHeight, color));

      case Joystick::ButtonShapeRect:
         return SymbolShapePtr(new SymbolRoundedRect(rectButtonWidth, 
                                                     rectButtonHeight, 
                                                     RectRadius,
                                                     color));

      case Joystick::ButtonShapeSmallRect:
         return SymbolShapePtr(new SymbolSmallRoundedRect(smallRectButtonWidth, 
                                                          smallRectButtonHeight, 
                                                          RectRadius,
                                                          color));

      case Joystick::ButtonShapeRoundedRect:
         return SymbolShapePtr(new SymbolRoundedRect(rectButtonWidth, 
                                                     rectButtonHeight, 
                                                     RoundedRectRadius,
                                                     color));

      case Joystick::ButtonShapeSmallRoundedRect:
         return SymbolShapePtr(new SymbolSmallRoundedRect(smallRectButtonWidth, 
                                                          smallRectButtonHeight, 
                                                          RoundedRectRadius,
                                                          color));
                                                     
      case Joystick::ButtonShapeHorizEllipse:
         return SymbolShapePtr(new SymbolHorizEllipse(horizEllipseButtonDiameterX, 
                                                      horizEllipseButtonDiameterY,
                                                      color));

      case Joystick::ButtonShapeRightTriangle:
         return SymbolShapePtr(new SymbolRightTriangle(rightTriangleWidth, color));

      // DPad
      case Joystick::ButtonShapeDPadUp:
      case Joystick::ButtonShapeDPadDown:
      case Joystick::ButtonShapeDPadLeft:
      case Joystick::ButtonShapeDPadRight:
         return SymbolShapePtr(new SymbolDPadArrow(shape, color));

      default:
         //TNLAssert(false, "Unknown button shape!");
         return getSymbol(KEY_UNKNOWN, Colors::red);
   }
}


static SymbolShapePtr getSymbol(Joystick::ButtonShape shape, const string &label, const Color &color)
{
   static const S32 LabelSize = 13;
   Vector<SymbolShapePtr> symbols;
   
   // Get the button outline
   SymbolShapePtr shapePtr = getSymbol(shape, color);

   symbols.push_back(shapePtr);

   // Handle some special cases -- there are some button labels that refer to special glyphs
   Joystick::ButtonSymbol buttonSymbol = Joystick::stringToButtonSymbol(label);

   // Point(0,-1) below is a font-dependent rendering factor chosen by trial-and-error
   if(buttonSymbol == Joystick::ButtonSymbolNone)
      symbols.push_back(SymbolShapePtr(new SymbolText(label, LabelSize + shapePtr->getLabelSizeAdjustor(label, LabelSize), 
                                                      KeyContext, shapePtr->getLabelOffset(label, LabelSize) + Point(0,-1), color)));
   else
      symbols.push_back(SymbolShapePtr(new SymbolButtonSymbol(buttonSymbol)));

   return SymbolShapePtr(new LayeredSymbolString(symbols));
}


static S32 KeyFontSize = 13;     // Size of characters used for rendering key bindings

// Color is ignored for controller buttons
static SymbolShapePtr getSymbol(InputCode inputCode, const Color &color)
{
   if(InputCodeManager::isKeyboardKey(inputCode))
      return SymbolShapePtr(new SymbolKey(InputCodeManager::inputCodeToString(inputCode), color));

   else if(InputCodeManager::isCtrlKey(inputCode) || InputCodeManager::isAltKey(inputCode))
   {
      Vector<string> modifiers(1);
      if(InputCodeManager::isCtrlKey(inputCode))
         modifiers.push_back(InputCodeManager::inputCodeToString(KEY_CTRL));
      else // if(isAltKey(inputCode))
         modifiers.push_back(InputCodeManager::inputCodeToString(KEY_ALT));
      
      return SymbolString::getModifiedKeySymbol(InputCodeManager::getBaseKeySpecialSequence(inputCode), modifiers, color);
   }
   else if(InputCodeManager::isControllerButton(inputCode))
   {
      // This gives us the logical SDL button that inputCode represents...
      SDL_GameControllerButton button = (SDL_GameControllerButton) InputCodeManager::inputCodeToControllerButton(inputCode);

      if(button == SDL_CONTROLLER_BUTTON_INVALID)
         return getSymbol(KEY_UNKNOWN, color);

      // Now we need to figure out which symbol to use for this button, depending on controller make/model
      Joystick::ButtonInfo buttonInfo = Joystick::getButtonInfo(button);

      // This gets us the button shape index, which will tell us what to draw... something like ButtonShapeRound
      Joystick::ButtonShape buttonShape = buttonInfo.buttonShape;

      SymbolShapePtr symbol = getSymbol(buttonShape, buttonInfo.label, buttonInfo.color);

      return symbol;
   }

   else if(strcmp(InputCodeManager::inputCodeToString(inputCode), "") != 0)
      return SymbolShapePtr(new SymbolKey(InputCodeManager::inputCodeToString(inputCode), color)); 

   else if(inputCode == KEY_UNKNOWN)
      return SymbolShapePtr(new SymbolUnknown(color));

   else
      return getSymbol(KEY_UNKNOWN, color);
}


SymbolShapePtr SymbolString::getModifiedKeySymbol(const string &symbolName, const Color &color)
{
   const Vector<string> *mods = InputCodeManager::getModifierNames();
   Vector<string> foundMods;

   bool found = true;
   string sym = symbolName;      // Make working copy

   while(found)
   {
      for(S32 i = 0; i < mods->size(); i++)
      {
         string mod = mods->get(i) + "+";
         found = false;
         if(sym.compare(0, mod.size(), mod) == 0)     // Read as: sym.startsWith(mod)
         {
            foundMods.push_back(mods->get(i));
            sym = sym.substr(mod.size());
            found = true;
            break;
         }
      }
   }

   InputCode inputCode = InputCodeManager::stringToInputCode(sym.c_str());     // Get the base inputCode

   if(inputCode == KEY_UNKNOWN) 
       return SymbolShapePtr();

   return getModifiedKeySymbol(inputCode, foundMods, color);
}


SymbolShapePtr SymbolString::getModifiedKeySymbol(InputCode inputCode, const Vector<string> &modifiers, const Color &color)
{
   // Returns the SymbolUnknown symbol
   if(inputCode == KEY_UNKNOWN || modifiers.size() == 0)
      return getSymbol(inputCode, color);

   Vector<SymbolShapePtr> symbols;
   for(S32 i = 0; i < modifiers.size(); i++)
   {
      symbols.push_back(SymbolShapePtr(new SymbolKey(modifiers[i], color)));
      symbols.push_back(SymbolShapePtr(new SymbolText("+", 13, KeyContext, Point(0, -3), color))); // Use offset to vertically center "+"
   }

   symbols.push_back(SymbolShapePtr(new SymbolKey(InputCodeManager::inputCodeToString(inputCode), color)));

   SymbolShape *s = new SymbolString(symbols);

   return SymbolShapePtr(s);
}


// Static method
SymbolShapePtr SymbolString::getControlSymbol(InputCode inputCode, const Color &color)
{
   return getSymbol(inputCode, color);
}


// Static method
SymbolShapePtr SymbolString::getSymbolGear(S32 fontSize, const Color &color)
{
   return SymbolShapePtr(new SymbolGear(fontSize, color));
}


// Static method
SymbolShapePtr SymbolString::getSymbolGoal(S32 fontSize, const Color &color)
{
   return SymbolShapePtr(new SymbolGoal(fontSize, color));
}


// Static method
SymbolShapePtr SymbolString::getSymbolNexus(S32 fontSize, const Color &color)
{
   return SymbolShapePtr(new SymbolNexus(fontSize, color));
}


// Static method
SymbolShapePtr SymbolString::getSymbolSpinner(S32 fontSize, const Color &color)
{
   return SymbolShapePtr(new SymbolSpinner(fontSize, color));
}


// Static method
SymbolShapePtr SymbolString::getBullet(const Color &color)
{
   return SymbolShapePtr(new SymbolBullet(color));
}


// Static method
SymbolShapePtr SymbolString::getSymbolText(const string &text, S32 fontSize, FontContext context, const Color &color)
{
   return SymbolShapePtr(new SymbolText(text, fontSize, context, color));
}


// Static method
SymbolShapePtr SymbolString::getBlankSymbol(S32 width, S32 height)
{
   return SymbolShapePtr(new SymbolBlank(width, height));
}


// Static method
SymbolShapePtr SymbolString::getHorizLine(S32 length, S32 height, const Color &color)
{
   return SymbolShapePtr(new SymbolHorizLine(length, height, color));
}


SymbolShapePtr SymbolString::getHorizLine(S32 length, S32 vertOffset, S32 height, const Color &color)
{
   return SymbolShapePtr(new SymbolHorizLine(length, vertOffset, height, color));
}


static InputCode convertStringToInputCode(const InputCodeManager *inputCodeManager, const string &symbol)
{
   // We might pass in a NULL inputCodeManager, in which case we won't process any inputCode related items
   if(!inputCodeManager)
      return KEY_UNKNOWN;

   // The following will return KEY_UNKNOWN if symbolName is not recognized as a known binding
   InputCode inputCode = inputCodeManager->getKeyBoundToBindingCodeName(symbol);

   // Second chance -- maybe it's a key name instead of a control binding (like "K")
   if(inputCode == KEY_UNKNOWN)
      inputCode = inputCodeManager->stringToInputCode(symbol.c_str());

   return inputCode;
}


static SymbolShapePtr convertStringToControlSymbol(const InputCodeManager *inputCodeManager, const string &symbol, const Color &color)
{
   string controlString = "";

   // We might pass in a NULL inputCodeManager, in which case we can't go further
   if(inputCodeManager)
   {
      // The following will return KEY_UNKNOWN if symbolName is not recognized as a known binding
      controlString = inputCodeManager->getEditorKeyBoundToBindingCodeName(symbol);

      if(controlString == "")
         controlString = inputCodeManager->getSpecialKeyBoundToBindingCodeName(symbol);
   }

   if(controlString != "")
      return SymbolString::getModifiedKeySymbol(controlString, color);

   return SymbolString::getModifiedKeySymbol(symbol, color);     // Is symbol something like "Ctrl+J"?
}


// Parse special symbols enclosed inside [[ ]] in strings.  The passed symbolName is the bit inside the brackets.
// Pass in NULL for inputCodeManager only if you double-pinky-promise that the string your parsing doesn't need it.
static void getSymbolShape(const InputCodeManager *inputCodeManager, const string &symbol, S32 fontSize,
                           const Color &color, Vector<SymbolShapePtr> &symbols)
{
   // The first thing we'll do is see if we can convert symbolName into an inputCode, which we can then convert into its 
   // string representation.  If conversion doesn't work, convertStringToInputCode will return KEY_UNKNOWN.
   InputCode inputCode = convertStringToInputCode(inputCodeManager, symbol);

   if(inputCode != KEY_UNKNOWN)     // We got a usable inputCode!!
   {
      symbols.push_back(SymbolString::getControlSymbol(inputCode, color));
      return;
   }

   // See if maybe this is an editor binding
   string editorBinding = inputCodeManager->getEditorKeyBoundToBindingCodeName(symbol);
   if(editorBinding != "")
   { 
      symbols.push_back(SymbolString::getModifiedKeySymbol(editorBinding, color));
      return;
   }

   // See if we can get something that looks like a string containing the name of an inputString, something like "Shift+O",
   // or perhaps a binding that can be converted into such.
   SymbolShapePtr modifiedKey = convertStringToControlSymbol(inputCodeManager, symbol, color);

   if(modifiedKey.get() != NULL)    // Got something!!
   {
      symbols.push_back(modifiedKey);
      return;
   }


   // Now for some standard parsing...
   Vector<string> words;
   parseString(symbol, words, ':');

   string symbolName = words[0];

   if(symbol == "LOADOUT_ICON")
      symbols.push_back(SymbolString::getSymbolGear(14, Colors::green)); // This is definitely wrong, but fixing the problem will be hard
   else if(symbol == "GOAL_ICON")
      symbols.push_back(SymbolString::getSymbolGoal(14, color));
   else if(symbol == "NEXUS_ICON")
      symbols.push_back(SymbolString::getSymbolNexus(14, color));
   else if(symbol == "SPINNER")
      symbols.push_back(SymbolString::getSymbolSpinner(fontSize, color));
   else if(symbol == "CHANGEWEP")
   {
      TNLAssert(inputCodeManager, "Need an inputCodeManager to parse this!");
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_SELWEAP1), color));
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_SELWEAP2), color));
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_SELWEAP3), color));
   }

   else if(symbol == "MOVEMENT")
   {
      TNLAssert(inputCodeManager, "Need an inputCodeManager to parse this!");
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_UP), color));
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_DOWN), color));
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_LEFT), color));
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_RIGHT), color));
   }

   else if(symbol == "MOVEMENT_LDR")
   {
      TNLAssert(inputCodeManager, "Need an inputCodeManager to parse this!");
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_LEFT), color));
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_DOWN), color));
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_RIGHT), color));
   }

   else if(symbol == "MODULE_CTRL1")
   {
      TNLAssert(inputCodeManager, "Need an inputCodeManager to parse this!");
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_MOD1), color));
   }

   else if(symbol == "MODULE_CTRL2")
   {
      TNLAssert(inputCodeManager, "Need an inputCodeManager to parse this!");
      symbols.push_back(SymbolString::getControlSymbol(inputCodeManager->getBinding(BINDING_MOD2), color));
   }

   else if(symbol == "BULLET")                        // Square bullet point 
      symbols.push_back(SymbolString::getBullet(AbstractInstructionsUserInterface::txtColor));    
      // Note that the above color will be wrong if this is used in any other context.  At the moment, we have no mechanism
      // for specifying the color of a symbol.  We need one.  But until then, this will work.

   else if(symbolName == "TAB_STOP")                  // Adds whitespace until width is equal to n
   {
      TNLAssert(words.size() == 2, "TAB_STOP:n has the wrong number of components!");
      S32 width = atoi(words[1].c_str());

      S32 w = 0;
      for(S32 i = 0; i < symbols.size(); i++)
         w += symbols[i]->getWidth();

      symbols.push_back(SymbolShapePtr(new SymbolBlank(width - w)));
   }

   else if(symbolName == "FOLDER_NAME")
   {
      TNLAssert(words.size() == 2, "FOLDER_NAME:xxx has the wrong number of components!");

      FolderManager *folderManager = GameManager::getClientGames()->get(0)->getSettings()->getFolderManager();
      const string dirName = folderManager->getDir(words[1]);

      symbols.push_back(SymbolShapePtr(new SymbolText(dirName, 18, HelpContext, Colors::red)));
   }

   else
   {
      // Note that we might get here with an otherwise usable symbol if we passed NULL for the inputCodeManager
      symbols.push_back(SymbolShapePtr(new SymbolText("Unknown Symbol: " + symbolName, fontSize, HelpItemContext, Colors::red)));
   }
}


// Pass true for block if this is part of a block of text, and empty lines should be accorded their full height.
// Pass false if this is a standalone string where and empty line should have zero height.
void SymbolString::symbolParse(const InputCodeManager *inputCodeManager, const string &str, Vector<SymbolShapePtr> &symbols,
                              FontContext fontContext, S32 fontSize, bool block, const Color &color)
{
   symbolParse(inputCodeManager, str, symbols, fontContext, fontSize, block, color, color);
}


void SymbolString::symbolParse(const InputCodeManager *inputCodeManager, const string &str, Vector<SymbolShapePtr> &symbols,
                              FontContext fontContext, S32 fontSize, bool block, const Color &textColor, const Color &symbolColor)
{
   if(!block && str == "")
      return;

   size_t offset = 0;

   while(true)
   {
      size_t startPos = str.find("[[", offset);      // If this isn't here, no further searching is necessary
      size_t endPos   = str.find("]]", offset + 2);

      if(startPos == string::npos || endPos == string::npos)
      {
         // No further symbols herein, convert the rest to text symbol and exit
         symbols.push_back(SymbolShapePtr(new SymbolText(str.substr(offset), fontSize, fontContext, textColor)));
         return;
      }

      // Check for the exception of the ']' key, which would create a symbol ending in ']]]'
      if(str[endPos+2] == ']')
         endPos++;

      symbols.push_back(SymbolShapePtr(new SymbolText(str.substr(offset, startPos - offset), fontSize, fontContext, textColor)));

      // Use + 2 to advance past the opening "[["
      getSymbolShape(inputCodeManager, str.substr(startPos + 2, endPos - startPos - 2), fontSize, symbolColor, symbols); 

      offset = endPos + 2;
   }
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
LayeredSymbolString::LayeredSymbolString(const Vector<shared_ptr<SymbolShape> > &symbols) :
                  Parent(symbols)
{
   mWidth = computeLayeredWidth(symbols);
}


// Destructor
LayeredSymbolString::~LayeredSymbolString()
{
   // Do nothing
}


// Each layer is rendered atop the previous, creating a layered effect
S32 LayeredSymbolString::render(F32 x, F32 y, Alignment alignment, S32 blockWidth) const
{
   for(S32 i = 0; i < mSymbols.size(); i++)
      mSymbols[i]->render(Point(x, y));

   return mHeight;
}


////////////////////////////////////////
////////////////////////////////////////

SymbolShape::SymbolShape(S32 width, S32 height, const Color &color) : 
   mColor(color)
{
   mWidth = width;
   mHeight = height;
   mLabelSizeAdjustor = 0;
   mAlpha = 1;
}


// Destructor
SymbolShape::~SymbolShape()
{
   // Do nothing
}


void SymbolShape::render(F32 x, F32 y) const
{
   render(Point(x,y));
}


void SymbolShape::render(S32 x, S32 y, Alignment alignment) const
{
   render((F32)x, (F32)y, alignment);
}


// y is the bottom of the glyph to be rendered
void SymbolShape::render(F32 x, F32 y, Alignment alignment) const
{
   if(alignment == AlignmentLeft)
      x += mWidth / 2;

   render(Point(x, y));    
}


S32 SymbolShape::getWidth() const
{
   return mWidth;
}


S32 SymbolShape::getHeight() const
{
   return mHeight;
}


bool SymbolShape::getHasGap() const
{
   return false;
}


Point SymbolShape::getLabelOffset(const string &label, S32 labelSize) const
{
   return mLabelOffset;
}


S32 SymbolShape::getLabelSizeAdjustor(const string &label, S32 labelSize) const
{
   return mLabelSizeAdjustor;
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
SymbolBlank::SymbolBlank(S32 width, S32 height) : Parent(width, height, Colors::black)
{
   // Do nothing
}


// Destructor
SymbolBlank::~SymbolBlank()
{
   // Do nothing
}


void SymbolBlank::render(const Point &center) const
{
   // Do nothing -- it's blank, remember?
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
SymbolHorizLine::SymbolHorizLine(S32 length, S32 height, const Color &color) : Parent(length, height, color)
{
   mVertOffset = 0;
}


// Constructor
SymbolHorizLine::SymbolHorizLine(S32 length, S32 vertOffset, S32 height, const Color &color) : Parent(length, height, color)
{
   mVertOffset = vertOffset;
}


// Destructor
SymbolHorizLine::~SymbolHorizLine()
{
   // Do nothing
}


void SymbolHorizLine::render(const Point &center) const
{
   RenderUtils::drawHorizLine(center.x - mWidth / 2, center.x + mWidth / 2, center.y - mHeight / 2 + mVertOffset, mColor, mAlpha);
}


////////////////////////////////////////
////////////////////////////////////////

static const S32 BorderDecorationVertCenteringOffset = 2;   // Offset the border of keys and buttons to better center them in the flow of text
static const S32 SpacingAdjustor = 2;


// Constructor
SymbolRoundedRect::SymbolRoundedRect(S32 width, S32 height, S32 radius, const Color &color) : 
                                                                  Parent(width + SpacingAdjustor, height + SpacingAdjustor, color)
{
   mRadius = radius;
}


// Destructor
SymbolRoundedRect::~SymbolRoundedRect()
{
   // Do nothing
}


void SymbolRoundedRect::render(const Point &center) const
{
   RenderUtils::drawRoundedRect(center - Point(0, (mHeight - SpacingAdjustor) / 2 - BorderDecorationVertCenteringOffset - 1),
                                mWidth - SpacingAdjustor, mHeight - SpacingAdjustor, mRadius, mColor, mAlpha);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
SymbolSmallRoundedRect::SymbolSmallRoundedRect(S32 width, S32 height, S32 radius, const Color &color) : 
                                                               Parent(width + SpacingAdjustor, height + SpacingAdjustor, radius, color)
{
   mLabelOffset.set(0, -1);
}


// Destructor
SymbolSmallRoundedRect::~SymbolSmallRoundedRect()
{
   // Do nothing
}


void SymbolSmallRoundedRect::render(const Point &center) const
{
   RenderUtils::drawRoundedRect(center - Point(0, mHeight / 2 - BorderDecorationVertCenteringOffset - SpacingAdjustor + 2),
                                mWidth - SpacingAdjustor, mHeight - SpacingAdjustor, mRadius, mColor, mAlpha);
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
SymbolHorizEllipse::SymbolHorizEllipse(S32 width, S32 height, const Color &color) : Parent(width + 2, height, color)
{
   mLabelOffset.set(0, -1);
}


// Destructor
SymbolHorizEllipse::~SymbolHorizEllipse()
{
   // Do nothing
}


void SymbolHorizEllipse::render(const Point &center) const
{
   S32 w = mWidth / 2;
   S32 h = mHeight / 2;

   Point cen = center - Point(0, h - 1);

   RenderUtils::drawEllipse(cen, w, h, mColor, mAlpha);
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
SymbolRightTriangle::SymbolRightTriangle(S32 width, const Color &color) : Parent(width, 19, color)
{
   mLabelOffset.set(-5, -1);
   mLabelSizeAdjustor = -3;
}


// Destructor
SymbolRightTriangle::~SymbolRightTriangle()
{
   // Do nothing
}


void SymbolRightTriangle::render(const Point &center) const
{
   Point cen((center.x -mWidth / 4) - 9, center.y + 6);  // Need to off-center the label slightly for this button
   JoystickRender::drawButtonRightTriangle(cen, mColor);
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
SymbolDPadArrow::SymbolDPadArrow(Joystick::ButtonShape buttonShape, const Color &color) : Parent(18, 18, color)
{
   mButtonShape = buttonShape;
}


// Destructor
SymbolDPadArrow::~SymbolDPadArrow()
{
   // Do nothing
}


void SymbolDPadArrow::render(const Point &center) const
{
   // Off set to match text rendering methods
   Point pos = center + Point(0, -6);

   // DPad
   switch(mButtonShape)
   {
      case Joystick::ButtonShapeDPadUp:
         JoystickRender::drawDPadUp(pos, mColor);
         break;

      case Joystick::ButtonShapeDPadDown:
         JoystickRender::drawDPadDown(pos, mColor);
         break;

      case Joystick::ButtonShapeDPadLeft:
         JoystickRender::drawDPadLeft(pos, mColor);
         break;

      case Joystick::ButtonShapeDPadRight:
         JoystickRender::drawDPadRight(pos, mColor);
         break;

      default:  // Should never get here
         break;
   }
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
SymbolCircle::SymbolCircle(S32 radius, const Color &color) : 
      Parent(radius * 2 + SpacingAdjustor, radius * 2 + SpacingAdjustor, color)
{
   // Do nothing
}


// Destructor
SymbolCircle::~SymbolCircle()
{
   // Do nothing
}


void SymbolCircle::render(const Point &pos) const
{
   // Adjust our position's y coordinate to be the center of the circle
   RenderUtils::drawCircle(pos - Point(0, (mHeight) / 2 - BorderDecorationVertCenteringOffset - SpacingAdjustor), 
                           F32(mWidth - SpacingAdjustor) / 2, 
                           mColor);
}


static const S32 LabelAutoShrinkThreshold = 15;

S32 SymbolCircle::getLabelSizeAdjustor(const string &label, S32 labelSize) const
{
   // Shrink labels a little when the text is uncomfortably big for the button
   if(RenderUtils::getStringWidth(labelSize, label.c_str()) > LabelAutoShrinkThreshold)
      return mLabelSizeAdjustor - 2;
   
   return mLabelSizeAdjustor;
}


Point SymbolCircle::getLabelOffset(const string &label, S32 labelSize) const
{
   if(RenderUtils::getStringWidth(labelSize, label.c_str()) > LabelAutoShrinkThreshold)
      return mLabelOffset + Point(0, -1);

   return mLabelOffset;
}


////////////////////////////////////////
////////////////////////////////////////


SymbolButtonSymbol::SymbolButtonSymbol(Joystick::ButtonSymbol glyph)
{
   mGlyph = glyph;
}


SymbolButtonSymbol::~SymbolButtonSymbol()
{
   // Do nothing
}


void SymbolButtonSymbol::render(const Point &pos) const
{
   // Get symbol in the proper position for rendering -- it's either this or change all the render methods
   Point renderPos = pos + Point(0, -6);   

   switch(mGlyph)
   {
      case Joystick::ButtonSymbolPsCircle:
         JoystickRender::drawPlaystationCircle(renderPos);
         break;
      case Joystick::ButtonSymbolPsCross:
         JoystickRender::drawPlaystationCross(renderPos);
         break;
      case Joystick::ButtonSymbolPsSquare:
         JoystickRender::drawPlaystationSquare(renderPos);
         break;
      case Joystick::ButtonSymbolPsTriangle:
         JoystickRender::drawPlaystationTriangle(renderPos, mColor);
         break;
      case Joystick::ButtonSymbolSmallLeftTriangle:
         JoystickRender::drawSmallLeftTriangle(renderPos + Point(0, -1), mColor);
         break;
      case Joystick::ButtonSymbolSmallRightTriangle:
         JoystickRender::drawSmallRightTriangle(renderPos + Point(0, -1), mColor);
         break;

      case Joystick::ButtonSymbolNone:
      default:
         TNLAssert(false, "Shouldn't be here!");
   }
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
SymbolGear::SymbolGear(S32 fontSize, const Color &color) : Parent(0, color)
{
   mWidth = S32(1.333f * fontSize);    // mWidth is effectively a diameter; we'll use mWidth / 2 for our rendering radius
   mHeight = mWidth;
}


// Destructor
SymbolGear::~SymbolGear()
{
   // Do nothing
}


void SymbolGear::render(const Point &pos) const
{
   // We are given the bottom y position of the line, but the icon expects the center
   Point center(pos.x, (pos.y - mHeight/2) + 2); // Slight downward adjustment to position to better align with text
   GameObjectRender::renderLoadoutZoneIcon(center, mWidth / 2, mColor);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
SymbolGoal::SymbolGoal(S32 fontSize, const Color &color) : Parent(fontSize, color)
{
   // Do nothing
}


// Destructor
SymbolGoal::~SymbolGoal()
{
   // Do nothing
}


void SymbolGoal::render(const Point &pos) const
{
   // We are given the bottom y position of the line, but the icon expects the center
   Point center(pos.x, (pos.y - mHeight/2) + 2); // Slight downward adjustment to position to better align with text
   GameObjectRender::renderGoalZoneIcon(center, mWidth/2, mColor);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
SymbolNexus::SymbolNexus(S32 fontSize, const Color &color) : Parent(fontSize, color)
{
   // Do nothing
}


// Destructor
SymbolNexus::~SymbolNexus()
{
   // Do nothing
}


void SymbolNexus::render(const Point &pos) const
{
   // We are given the bottom y position of the line, but the icon expects the center
   // so we need a slight downward adjustment to position to better align with text
   Point center(pos.x, (pos.y - mHeight/2) + 2);               
   GameObjectRender::renderNexusIcon(center, mWidth/2, 0, mColor);
}


////////////////////////////////////////
////////////////////////////////////////

// Constructor
SymbolSpinner::SymbolSpinner(S32 fontSize, const Color &color) : Parent(fontSize / 2, color)
{
   // Do nothing
}


// Destructor
SymbolSpinner::~SymbolSpinner()
{
   // Do nothing
}


void SymbolSpinner::render(const Point &pos) const
{
   S32 charindx = Platform::getRealMilliseconds() / 200 % 4;

   const char *charstr = NULL;

   switch(charindx)
   {
      case 0:
         charstr = "|";
         break;
      case 1:
         charstr = "/";
         break;
      case 2:
         charstr = "--";
         break;
      case 3:
         charstr = "\\";
         break;
      default: 
         TNLAssert(false, "Unexpected value of charindx");
         break;
   }

   RenderUtils::drawStringc(pos.x, pos.y - SpacingAdjustor / 2, mHeight / 2.0f, mColor, charstr);
}


////////////////////////////////////////
////////////////////////////////////////

static const S32 BulletRad = 2;

// Constructor
SymbolBullet::SymbolBullet(const Color &color) : Parent(BulletRad * 2, BulletRad * 2, color)
{
   // Do nothing
}


// Destructor
SymbolBullet::~SymbolBullet()
{
   // Do nothing
}


void SymbolBullet::render(const Point &pos) const
{
   // We are given the bottom y position of the line, but the icon expects the center
   Point center(pos.x, (pos.y - 7));
   RenderUtils::drawFilledSquare(center, (F32)BulletRad, mColor, mAlpha);
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor with no vertical offset
SymbolText::SymbolText(const string &text, S32 fontSize, FontContext context, const Color &color) : 
                              Parent(RenderUtils::getStringWidth(context, fontSize, text.c_str()), fontSize, color)
{
   mText = text;
   mFontContext = context;
   mFontSize = fontSize;
}


// Constructor with vertical offset
SymbolText::SymbolText(const string &text, S32 fontSize, FontContext context, const Point &labelOffset, const Color &color) : 
                                       Parent(RenderUtils::getStringWidth(context, fontSize, text.c_str()), fontSize, color)
{
   mText = text;
   mFontContext = context;
   mFontSize = fontSize;
   mLabelOffset = labelOffset;

   mHeight = fontSize;
}


// Destructor
SymbolText::~SymbolText()
{
   // Do nothing
}


void SymbolText::render(const Point &center) const
{
   FontManager::pushFontContext(mFontContext);
   
      RenderUtils::drawStringc(center + mLabelOffset, mFontSize, mColor, mAlpha, mText.c_str());

   FontManager::popFontContext();
}


S32 SymbolText::getHeight() const
{
   return Parent::getHeight() + (S32)mLabelOffset.y;
}


bool SymbolText::getHasGap() const
{
   return true;
}


////////////////////////////////////////
////////////////////////////////////////


static S32 Margin = 3;              // Buffer within key around text
static S32 Gap = 3;                 // Distance between keys
static S32 TotalHeight = KeyFontSize + 2 * Margin;
static S32 SymbolPadding = 6;       // Just some padding we throw around our symbols to make them look hot


static S32 getKeyWidth(const string &text, S32 height)
{
   S32 width;
   if(text == "Up Arrow" || text == "Down Arrow" || text == "Left Arrow" || text == "Right Arrow")
      width = 0;     // Make a square button; will return height below (and since it's a square...)
   else
      width = RenderUtils::getStringWidth(KeyContext, KeyFontSize, text.c_str()) + Margin * 2;

   return max(width, height) + SymbolPadding;
}


SymbolKey::SymbolKey(const string &text, const Color &color) : Parent(text, KeyFontSize, KeyContext, color)
{
   mHeight = TotalHeight;
   mWidth = getRenderWidth(text);
}


// Destructor
SymbolKey::~SymbolKey()
{
   // Do nothing
}


S32 SymbolKey::getRenderWidth(const string &text)
{
   return getKeyWidth(text, TotalHeight);
}


// Note: passed font size and context will be ignored
// center represents a point a the bottom-center of the glyph; so it's not exactly a center
void SymbolKey::render(const Point &center) const
{
   // Compensate for the fact that boxes draw from center
   const Point boxVertAdj  = mLabelOffset + Point(0, BorderDecorationVertCenteringOffset - KeyFontSize / 2 - 3); 

   // The -4 is a font-dependent aesthetic value determined by trial and error while looking at the help screens
   const Point textVertAdj = mLabelOffset + Point(0, BorderDecorationVertCenteringOffset - 4);

   // Handle some special cases:
   if(mText == "Up Arrow")
      GameObjectRender::renderUpArrow(center + textVertAdj + Point(0, -5.5), KeyFontSize, mColor);
   else if(mText == "Down Arrow")
      GameObjectRender::renderDownArrow(center + textVertAdj + Point(0, -6), KeyFontSize, mColor);
   else if(mText == "Left Arrow")
      GameObjectRender::renderLeftArrow(center + textVertAdj + Point(0, -6), KeyFontSize, mColor);
   else if(mText == "Right Arrow")
      GameObjectRender::renderRightArrow(center + textVertAdj + Point(0, -6), KeyFontSize, mColor);
   else
      Parent::render(center + textVertAdj);

   S32 width =  max(mWidth - 2 * Gap, mHeight);

   RenderUtils::drawHollowRect(center + boxVertAdj, width, mHeight, mColor);
}


////////////////////////////////////////
////////////////////////////////////////


// Symbol to be used when we don't know what symbol to use

// Constructor
SymbolUnknown::SymbolUnknown(const Color &color) : Parent("~?~", Colors::red)
{
   // Do nothing
}


// Destructor
SymbolUnknown::~SymbolUnknown()
{
   // Do nothing
}


} } // Nested namespace


