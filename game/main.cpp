#include "lib/Arduboy2.h"
#include "lib/EEPROM.h"
#include "lib/runtime.h"

#undef INPUT_LEFT
#undef INPUT_RIGHT
#undef INPUT_UP
#undef INPUT_DOWN
#undef INPUT_A
#undef INPUT_B

#include "Draw.h"
#include "Interface.h"
#include "Game.h"
#include "Simulation.h"

static constexpr uint16_t EEPROM_STORAGE_SPACE_START = 0;
static constexpr uint32_t RETURN_HOLD_TIME_MS = 500;

static uint32_t return_hold_start_ms = 0;
static bool return_was_pressed = false;
static bool return_hold_handled = false;
static bool pending_return_press = false;

static bool IsInMainMenuState()
{
  return UIState.state == StartScreen || UIState.state == NewCityMenu;
}

static void HandleReturnHold()
{
  if(IsInMainMenuState())
  {
    arduboy.exitToBootloader();
  }
  else
  {
    UIState.state = StartScreen;
    UIState.selection = 0;
  }
}

static void UpdateReturnButton()
{
  const bool is_pressed = arduboy.pressed(A_BUTTON);

  if(is_pressed)
  {
    if(!return_was_pressed)
    {
      return_was_pressed = true;
      return_hold_handled = false;
      return_hold_start_ms = millis();
    }
    else if(!return_hold_handled && (uint32_t)(millis() - return_hold_start_ms) >= RETURN_HOLD_TIME_MS)
    {
      return_hold_handled = true;
      pending_return_press = false;
      HandleReturnHold();
    }
  }
  else if(return_was_pressed)
  {
    if(!return_hold_handled)
    {
      pending_return_press = true;
    }

    return_was_pressed = false;
    return_hold_handled = false;
    return_hold_start_ms = 0;
  }
}

uint8_t GetInput()
{
  uint8_t result = 0;

  if(pending_return_press)
  {
    result |= INPUT_A;
    pending_return_press = false;
  }
  
  if(arduboy.pressed(B_BUTTON))
  {
    result |= INPUT_B;  
  }
  if(arduboy.pressed(UP_BUTTON))
  {
    result |= INPUT_UP;  
  }
  if(arduboy.pressed(DOWN_BUTTON))
  {
    result |= INPUT_DOWN;  
  }
  if(arduboy.pressed(LEFT_BUTTON))
  {
    result |= INPUT_LEFT;  
  }
  if(arduboy.pressed(RIGHT_BUTTON))
  {
    result |= INPUT_RIGHT;  
  }

  return result;
}

void PutPixel(uint8_t x, uint8_t y, uint8_t colour)
{
  arduboy.drawPixel(x, y, colour);
}

/*
void DrawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour)
{
  arduboy.fillRect(x, y, w, h, colour);
}

void DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour)
{
  arduboy.drawRect(x, y, w, h, colour);
}
*/

void DrawBitmap(const uint8_t* bmp, uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
  arduboy.drawBitmap(x, y, bmp, w, h, BLACK);
}

void SaveCity()
{
  uint16_t address = EEPROM_STORAGE_SPACE_START;

  // Add a header so we know that the EEPROM contains a saved city
  EEPROM.update(address++, 'C'); 
  EEPROM.update(address++, 'T'); 
  EEPROM.update(address++, 'Y'); 
  EEPROM.update(address++, '1'); 

  uint8_t* ptr = (uint8_t*) &State;
  for(size_t n = 0; n < sizeof(GameState); n++)
  {
    EEPROM.update(address++, *ptr);
    ptr++;
  }
  EEPROM.commit();
}

bool LoadCity()
{
  uint16_t address = EEPROM_STORAGE_SPACE_START;

  if(EEPROM.read(address++) != 'C') return false;
  if(EEPROM.read(address++) != 'T') return false;
  if(EEPROM.read(address++) != 'Y') return false;
  if(EEPROM.read(address++) != '1') return false;

  uint8_t* ptr = (uint8_t*) &State;
  for(size_t n = 0; n < sizeof(GameState); n++)
  {
    *ptr = EEPROM.read(address++);
    ptr++;
  }

  return true;
}

uint8_t* GetPowerGrid()
{
  return arduboy.getBuffer();
}

void setup()
{
  arduboy.boot();
  arduboy.bootLogo();
  arduboy.setFrameRate(25);

  InitGame();
}

void loop()
{
  if(arduboy.nextFrame())
  {
    arduboy.pollButtons();
    UpdateReturnButton();
    TickGame();
    arduboy.display(false);
  }
}
