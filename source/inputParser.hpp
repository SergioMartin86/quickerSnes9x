#pragma once

// Base controller class
// by eien86

#include <cstdint>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/json.hpp>
#include <string>
#include <sstream>
#include <snes9x.h>

namespace jaffar
{
  
typedef uint16_t port_t;

struct input_t
{
  bool power = false;
  bool reset = false;
  port_t port1 = 0;
  port_t port2 = 0;
};

class InputParser
{
public:

  enum controller_t { none, joypad };

  InputParser(const nlohmann::json &config)
  {

    // Parsing controller 1 type
    {
      bool isTypeRecognized = false;
      const auto controller1Type = jaffarCommon::json::getString(config, "Controller 1 Type");

      if (controller1Type == "None")   { _controller1Type = controller_t::none;   isTypeRecognized = true; }
      if (controller1Type == "Joypad") { _controller1Type = controller_t::joypad; isTypeRecognized = true; }
      
      if (isTypeRecognized == false) JAFFAR_THROW_LOGIC("Controller 1 type not recognized: '%s'\n", controller1Type.c_str()); 
   }

    // Parsing controller 2 type
    {
      bool isTypeRecognized = false;
      const auto controller2Type = jaffarCommon::json::getString(config, "Controller 2 Type");

      if (controller2Type == "None")   { _controller2Type = controller_t::none;    isTypeRecognized = true; }
      if (controller2Type == "Joypad") { _controller2Type = controller_t::joypad;  isTypeRecognized = true; }
      
      if (isTypeRecognized == false) JAFFAR_THROW_LOGIC("Controller 2 type not recognized: '%s'\n", controller2Type.c_str()); 
    }
  }

  inline input_t parseInputString(const std::string &inputString) const
  {
    // Storage for the input
    input_t input;

    // Converting input into a stream for parsing
    std::istringstream ss(inputString);

    // Start separator
    if (ss.get() != '|') reportBadInputString(inputString);

    // Parsing console inputs
    parseConsoleInputs(input, ss, inputString);
    
    // Parsing controller 1 inputs
    parseControllerInputs(_controller1Type, input.port1, ss, inputString);

    // Parsing controller 2 inputs
    parseControllerInputs(_controller2Type, input.port2, ss, inputString);

    // End separator
    if (ss.get() != '|') reportBadInputString(inputString);

    // If its not the end of the stream, then extra values remain and its invalid
    ss.get();
    if (ss.eof() == false) reportBadInputString(inputString);
    
    // Returning input
    return input;
  };

  private:

  static inline void reportBadInputString(const std::string &inputString)
  {
    JAFFAR_THROW_LOGIC("Could not decode input string: '%s'\n", inputString.c_str());
  }

  static bool parseJoyPadInput(uint16_t& code, std::istringstream& ss, const std::string& inputString)
  {
    // Currently read character
    char c;

    // Cleaning code
    code = 0;

    // Up
    c = ss.get();
    if (c != '.' && c != 'U') return false;
    if (c == 'U') code |= SNES_UP_MASK;

    // Down
    c = ss.get();
    if (c != '.' && c != 'D') return false;
    if (c == 'D') code |= SNES_DOWN_MASK;

    // Left
    c = ss.get();
    if (c != '.' && c != 'L') return false;
    if (c == 'L') code |= SNES_LEFT_MASK;

    // Right
    c = ss.get();
    if (c != '.' && c != 'R') return false;
    if (c == 'R') code |= SNES_RIGHT_MASK;

    // Select
    c = ss.get();
    if (c != '.' && c != 's') return false;
    if (c == 's') code |= SNES_SELECT_MASK;

    // Start
    c = ss.get();
    if (c != '.' && c != 'S') return false;
    if (c == 'S') code |= SNES_START_MASK;

    // Y
    c = ss.get();
    if (c != '.' && c != 'Y') return false;
    if (c == 'Y') code |= SNES_Y_MASK;

    // B
    c = ss.get();
    if (c != '.' && c != 'B') return false;
    if (c == 'B') code |= SNES_B_MASK;

    // X
    c = ss.get();
    if (c != '.' && c != 'X') return false;
    if (c == 'X') code |= SNES_X_MASK;

    // A
    c = ss.get();
    if (c != '.' && c != 'A') return false;
    if (c == 'A') code |= SNES_A_MASK;

    // l
    c = ss.get();
    if (c != '.' && c != 'l') return false;
    if (c == 'l') code |= SNES_TL_MASK;

    // r
    c = ss.get();
    if (c != '.' && c != 'r') return false;
    if (c == 'r') code |= SNES_TR_MASK;

    return true;
  }

  static void parseControllerInputs(const controller_t type, port_t& port, std::istringstream& ss, const std::string& inputString)
  {
    // If no controller assigned then, its port is all zeroes.
    if (type == controller_t::none) { port = 0; return; }

    // Controller separator
    if (ss.get() != '|') reportBadInputString(inputString);

    // If game gear input, parse 2 buttons and start
    if (type == controller_t::joypad) 
    {
      // Storage for gamepad's code
      uint16_t code = 0;

      // Parsing gamepad code
      parseJoyPadInput(code, ss, inputString);

      // Pushing input code into the port
      port = code;
    }
  }

  static void parseConsoleInputs(input_t& input, std::istringstream& ss, const std::string& inputString)
  {
    // Currently read character
    char c;

    c = ss.get();
    if (c != '.' && c != 'P') reportBadInputString(inputString);
    if (c == 'P') input.power = true;
    if (c == '.') input.power = false;

    c = ss.get();
    if (c != '.' && c != 'r') reportBadInputString(inputString);
    if (c == 'r') input.reset = true;
    if (c == '.') input.reset = false;
  }

  input_t _input;
  controller_t _controller1Type;
  controller_t _controller2Type;
  
}; // class InputParser

} // namespace jaffar