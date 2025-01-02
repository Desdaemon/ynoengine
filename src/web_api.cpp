#include "web_api.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#  if defined(PLAYER_YNO)
#    include <webview/webview.h>
#    include <fmt/format.h>
#    include <nlohmann/json.hpp>

#    include "baseui.h"
#    include "platform/sdl/sdl2_ui.h"
#    include "output.h"
#    include "multiplayer/game_multiplayer.h"
#    include "player.h"
#    define JS_EVAL(...) (DisplayUi ? ((Sdl2Ui*)DisplayUi.get())->GetWebview().dispatch([=]{ ((Sdl2Ui*)DisplayUi.get())->GetWebview().eval(fmt::format(__VA_ARGS__)); }) : webview::noresult{})
     using json = nlohmann::json;
#  endif
#  define EM_ASM_INT(...) 0
#  define EM_ASM(...)
#endif

using namespace Web_API;

std::string Web_API::GetSocketURL() {
#ifdef PLAYER_YNO
	//return "wss://connect.ynoproject.net/2kki/";
	//return "wss://localhost:8028/backend/2kki/";
	std::string_view game = Player::emscripten_game_name;
	if (game.empty())
		game = "2kki";
	return fmt::format("wss://connect.ynoproject.net/{}/", game);
#else
	return reinterpret_cast<char*>(EM_ASM_INT({
	  var ws = Module.wsUrl;
	  var len = lengthBytesUTF8(ws)+1;
	  var wasm_str = _malloc(len);
	  stringToUTF8(ws, wasm_str, len);
	  return wasm_str;
	}));
#endif
}

void Web_API::OnLoadMap(std::string_view name) {
#ifdef PLAYER_YNO
	std::string name_(name);
	JS_EVAL(R"js(onLoadMap("{}"))js", name_);
	return;
#else
	EM_ASM({
		onLoadMap(UTF8ToString($0));
	}, name.data(), name.size());
#endif
}

void Web_API::OnRoomSwitch() {
#ifdef PLAYER_YNO
	JS_EVAL("onRoomSwitch()");
#else
	EM_ASM({
		onRoomSwitch();
	});
#endif
}

void Web_API::SyncPlayerData(std::string_view uuid, int rank, int account_bin, std::string_view badge, const int medals[5], int id) {
#ifdef PLAYER_YNO
	std::string uuid_(uuid), badge_(badge);
	JS_EVAL(R"js(syncPlayerData("{}", {}, {}, "{}", [{}, {}, {}, {}, {}], {}))js", uuid_, rank, account_bin, badge_, medals[0], medals[1], medals[2], medals[3], medals[4], id);
#else
	EM_ASM({
		syncPlayerData(UTF8ToString($0, $1), $2, $3, UTF8ToString($4, $5), [ $6, $7, $8, $9, $10 ], $11);
	}, uuid.data(), uuid.size(), rank, account_bin, badge.data(), badge.size(), medals[0], medals[1], medals[2], medals[3], medals[4], id);
#endif
}

void Web_API::OnPlayerDisconnect(int id) {
#ifdef PLAYER_YNO
	JS_EVAL("onPlayerDisconnected({})", id);
#else
	EM_ASM({
		onPlayerDisconnected($0);
	}, id);
#endif
}

void Web_API::OnPlayerNameUpdated(std::string_view name, int id) {
#ifdef PLAYER_YNO
	std::string name_(name);
	JS_EVAL(R"js(onPlayerConnectedOrUpdated("", "{}", {}))js", name_, id);
#else
	EM_ASM({
		onPlayerConnectedOrUpdated("", UTF8ToString($0, $1), $2);
	}, name.data(), name.size(), id);
#endif
}

void Web_API::OnPlayerSystemUpdated(std::string_view system, int id) {
#ifdef PLAYER_YNO
	std::string system_(system);
	JS_EVAL(R"js(onPlayerConnectedOrUpdated("{}", "", {}))js", system_, id);
#else
	EM_ASM({
		onPlayerConnectedOrUpdated(UTF8ToString($0, $1), "", $2);
	}, system.data(), system.size(), id);
#endif
}

void Web_API::UpdateConnectionStatus(int status) {
#ifdef PLAYER_YNO
	JS_EVAL("onUpdateConnectionStatus({})", status);
#else
	EM_ASM({
		onUpdateConnectionStatus($0);
	}, status);
#endif
}

void Web_API::ReceiveInputFeedback(int s) {
#ifdef PLAYER_YNO
	JS_EVAL("onReceiveInputFeedback({})", s);
#else
	EM_ASM({
		onReceiveInputFeedback($0);
	}, s);
#endif
}

void Web_API::NametagModeUpdated(int m) {
#ifdef PLAYER_YNO
	JS_EVAL("onNametagModeUpdated({})", m);
#else
	EM_ASM({
		onNametagModeUpdated($0);
	}, m);
#endif
}

void Web_API::OnPlayerSpriteUpdated(std::string_view name, int index, int id) {
#ifdef PLAYER_YNO
	std::string name_(name);
	JS_EVAL(R"js(onPlayerSpriteUpdated("{}", {}, {}))js", name_, index, id);
#else
	EM_ASM({
		onPlayerSpriteUpdated(UTF8ToString($0, $1), $2, $3);
	}, name.data(), name.size(), index, id);
#endif
}

void Web_API::OnPlayerTeleported(int map_id, int x, int y) {
#ifdef PLAYER_YNO
	JS_EVAL("onPlayerTeleported({}, {}, {})", map_id, x, y);
#else
	EM_ASM({
		onPlayerTeleported($0, $1, $2);
	}, map_id, x, y);
#endif
}

void Web_API::OnUpdateSystemGraphic(std::string_view sys) {
#ifdef PLAYER_YNO
	std::string sys_(sys);
	JS_EVAL(R"js( onUpdateSystemGraphic("{}") )js", sys_);
#else
	EM_ASM({
		onUpdateSystemGraphic(UTF8ToString($0, $1));
	}, sys.data(), sys.size());
#endif
}

void Web_API::OnRequestBadgeUpdate() {
#ifdef PLAYER_YNO
	JS_EVAL("onBadgeUpdateRequested()");
#else
	EM_ASM({
		onBadgeUpdateRequested();
	});
#endif
}

void Web_API::ShowToastMessage(std::string_view msg, std::string_view icon) {
#ifdef PLAYER_YNO
	std::string msg_(msg), icon_(icon);
	JS_EVAL(R"js( showClientToastMessage("{}", "{}") )js", msg_, icon_);
#else
	EM_ASM({
		showClientToastMessage(UTF8ToString($0, $1), UTF8ToString($2, $3));
	}, msg.data(), msg.size(), icon.data(), icon.size());
#endif
}

bool Web_API::ShouldConnectPlayer(std::string_view uuid) {
#ifdef PLAYER_YNO
	return true;
#else
	int result = EM_ASM_INT({
		return shouldConnectPlayer(UTF8ToString($0, $1)) ? 1 : 0;
	}, uuid.data(), uuid.size());
	return result == 1;
#endif
}

void Web_API::OnRequestFile(std::string_view path) {
#ifdef PLAYER_YNO
	std::string path_(path);
	JS_EVAL(R"js( onRequestFile("{}") )js", path_);
#else
	EM_ASM({
		onRequestFile(UTF8ToString($0, $1));
	}, path.data(), path.size());
#endif
}

void Web_API::InitializeBindings() {
#ifdef PLAYER_YNO
	auto& w = ((Sdl2Ui*)DisplayUi.get())->GetWebview();
	w.bind("webviewSendSession", [](const std::string& args) -> std::string {
		json args_ = json::parse(args);
		GMI().sessionConn.Send((std::string)args_[0]);
		return "";
	});
	w.bind("webviewSessionToken", [](const std::string&) -> std::string {
		if (GMI().session_token.empty())
			return "null";
		return fmt::format("\"{}\"", GMI().session_token);
	});
	GMI().sessionConn.RegisterRawHandler([](std::string_view name, std::string_view args) {
		JS_EVAL(R"js( receiveSessionMessage("{}", `{}`) )js", name, args);
	});
#endif
}
