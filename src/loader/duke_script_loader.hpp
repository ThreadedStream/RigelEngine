/* Copyright (C) 2016, Nikolai Wuttke. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "data/duke_script.hpp"
#include "data/level_hints.hpp"

#include <string>
#include <unordered_map>


namespace rigel::loader
{

// enum class Command{
//   FADE_IN,
//   FADE_OUT,
//   DELAY,
//   BABBLE_ON,
//   BABBLE_OFF,
//   NO_SOUNDS,
//   KEYS,
//   GET_NAMES,
//   PAK,
//   LOAD_RAW,
//   Z,
//   GET_PAL,
//   WAIT,
//   SHIFT_WIN,
//   EXIT_TO_DEMO,
//   TOGGS,
//   CENTER_WINDOW,
//   MENU,
//   XY_TEXT,
// };


using namespace data::script;


#define LAMBDA_DEF(action, additional_body)                                    \
  [](                                                                          \
    const std::string& command,                                                \
    std::istream& lineTextStream) -> std::optional<Action>                     \
  {                                                                            \
    additional_body return action;                                             \
  }

// nice hack to escape commas in macro expansions
// https://stackoverflow.com/questions/13842468/comma-in-c-c-macro
#define COMMA ,


using ScriptBundle = std::unordered_map<std::string, data::script::Script>;


ScriptBundle loadScripts(const std::string& scriptSource);


data::LevelHints loadHintMessages(const std::string& scriptSource);

} // namespace rigel::loader
