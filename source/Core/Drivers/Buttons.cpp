/*
 * Buttons.c
 *
 *  Created on: 29 May 2020
 *      Author: Ralim
 */
#include "FreeRTOS.h"
#include "settingsGUI.hpp"
#include "task.h"
#include <Buttons.hpp>
uint32_t lastButtonTime = 0;

ButtonState getButtonState() {
  /*
   * Read in the buttons and then determine if a state change needs to occur
   */

  /*
   * If the previous state was  00 Then we want to latch the new state if
   * different & update time
   * If the previous state was !00 Then we want to search if we trigger long
   * press (buttons still down), or if release we trigger press
   * (downtime>filter)
   */
  constexpr uint16_t LONG_PRESS_MIN = TICKS_100MS * 4;

  struct State {
    /// Bitmask for currently held buttons. Used to identify buttons being
    /// pressed or released.
    uint8_t state = 0;

    /// Set on every call (aside from consecutive no-button-press calls) to
    /// (state.state && now - state.initialPressTime < LONG_PRESS_MIN). Only
    /// read when releasing all buttons.
    bool wasShortPress = false;

    /// Bitmask for any buttons pressed, cleared when releasing all buttons.
    /// Only read when releasing all buttons in a short-press. Zero if
    /// previousState is 0.
    uint8_t totalState = 0;

    /// Only set when transitioning from no buttons pressed to buttons pressed.
    /// Used to calculate wasShortPress.
    TickType_t initialPressTime = 0;

    /// Set whenever any buttons are pressed or released (state is changed).
    /// Used to delay sending long-press events.
    TickType_t stateChangeTime = 0;
  };
  static State state;

  const uint8_t currentState = (getButtonA() << 0) | (getButtonB() << 1);
  const TickType_t now = xTaskGetTickCount();
  if (currentState)
    lastButtonTime = now;

  // If a button is newly pressed, assign state.initialPressTime here.
  // state.initialPressTime is used to compute isShortPress ->
  // state.wasShortPress.
  if (currentState && !state.state) {
    state.initialPressTime = now;
  }

  // If any button is held >= LONG_PRESS_MIN, don't generate a short press event
  // on release.
  const bool isShortPress =
    currentState && (now - state.initialPressTime < LONG_PRESS_MIN);

  if (currentState != state.state) {
    // A button is pressed or released.
    state.stateChangeTime = now;

    if (currentState) {
      // Buttons are held. Return BUTTON_NONE (nothing is done until all buttons
      // are released), but log all buttons pressed to totalState. If both
      // buttons were pressed and one is released, return BUTTON_BOTH once both
      // are released.
      state.state = currentState;
      state.wasShortPress = isShortPress;
      state.totalState = static_cast<uint8_t>(state.totalState | currentState);
      return BUTTON_NONE;
    } else {
      // User has released all buttons. If we have (loosely speaking) not
      // returned a long-press event, return a short-press event.
      //
      // Checking state.wasShortPress (not isShortPress) ensures that every
      // single-button press returns either a short event or 1+ long events.

      ButtonState retVal = BUTTON_NONE;
      if (state.wasShortPress) {
        // The user didn't hold the button for long
        // So we send button press

        if (state.totalState == 0x01)
          retVal = BUTTON_F_SHORT;
        else if (state.totalState == 0x02)
          retVal = BUTTON_B_SHORT;
        else
          retVal = BUTTON_BOTH; // Both being held case
      }
      state.state = currentState;
      state.wasShortPress = isShortPress;
      state.totalState = 0;
      return retVal;
    }
  } else {
    // Button state unchanged.
    // If no buttons are pressed, exit.
    if (currentState == 0)
      return BUTTON_NONE;

    // Buttons are pressed.
    state.wasShortPress = isShortPress;

    // If the current exact state is held >= LONG_PRESS_MIN, generate a long
    // press event.
    //
    // Checking state.stateChangeTime (not state.initialPressTime) ensures that
    // releasing one of two held buttons doesn't immediately generate an
    // unwanted BUTTON_F_LONG or BUTTON_B_LONG event.
    if (now - state.stateChangeTime >= LONG_PRESS_MIN) {
      if (currentState == 0x01)
        return BUTTON_F_LONG;
      else if (currentState == 0x02)
        return BUTTON_B_LONG;
      else
        return BUTTON_BOTH_LONG; // Both being held case
    } else
      return BUTTON_NONE;
  }
}

void waitForButtonPress() {
  // we are just lazy and sleep until user confirms button press
  // This also eats the button press event!
  ButtonState buttons = getButtonState();
  while (buttons) {
    buttons = getButtonState();
    GUIDelay();
  }
  while (!buttons) {
    buttons = getButtonState();
    GUIDelay();
  }
}

void waitForButtonPressOrTimeout(uint32_t timeout) {
  timeout += xTaskGetTickCount();
  // calculate the exit point

  ButtonState buttons = getButtonState();
  while (buttons) {
    buttons = getButtonState();
    GUIDelay();
    if (xTaskGetTickCount() > timeout)
      return;
  }
  while (!buttons) {
    buttons = getButtonState();
    GUIDelay();
    if (xTaskGetTickCount() > timeout)
      return;
  }
}
