#include "oneshot.h"
#include "main_data.h"
#include "game_strings.h"
#include "game_switches.h"
#include "multiplayer/game_multiplayer.h"
#include "web_api.h"

static Game_Oneshot _instance;

Game_Oneshot& Game_Oneshot::Instance() {
  return _instance;
}

void Game_Oneshot::SwitchSet(int switch_id, bool value) {
  if (!value) return;
  switch (switch_id) {
    case Switches::EVENT_SETNAME:
      Game_Strings::Str_Params str_params {Variables::PLAYERNAME, 0, 0};
      Main_Data::game_strings->Asg(str_params, Web_API::GetPlayerName());
      Main_Data::game_switches->Set(Switches::HAS_ACCOUNT, !GMI().session_token.empty());
      break;
  }
}
