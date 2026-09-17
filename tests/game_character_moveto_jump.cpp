#include "doctest.h"
#include "options.h"
#include "game_map.h"
#include "game_player.h"
#include "main_data.h"
#include <lcf/rpg/movecommand.h>

#include "mock_game.h"

TEST_SUITE_BEGIN("Game_Character_MoveTo_Jump");

using Code = lcf::rpg::MoveCommand::Code;

static constexpr auto map_id = MockMap::ePassBlock20x15;

static lcf::rpg::MoveRoute MakeRoute(std::initializer_list<Code> codes) {
	lcf::rpg::MoveRoute mr;
	for (auto code : codes) {
		lcf::rpg::MoveCommand mc;
		mc.command_id = static_cast<int>(code);
		mr.move_commands.push_back(mc);
	}
	return mr;
}

static void Step(Game_Player& ch) {
	ch.SetProcessed(false);
	ch.Update();
}

static Game_Player& SetupJumped(const MockGame& mg) {
	auto& ch = *mg.GetPlayer();
	ch.SetMapId(static_cast<int>(map_id));
	ch.MoveTo(static_cast<int>(map_id), 6, 6);
	ch.ForceMoveRoute(MakeRoute({ Code::begin_jump, Code::move_right, Code::end_jump }), 8);
	Step(ch);
	return ch;
}

TEST_CASE("MoveToDuringJumpIsStopping") {
	const MockGame mg(map_id);
	auto& ch = SetupJumped(mg);

	REQUIRE_EQ(ch.GetX(), 7);
	REQUIRE(ch.IsJumping());
	REQUIRE_GT(ch.GetRemainingStep(), 0);

	ch.MoveTo(static_cast<int>(map_id), 6, 6);

	REQUIRE_EQ(ch.GetRemainingStep(), 0);
	REQUIRE(ch.IsJumping());
	REQUIRE(ch.IsStopping());
}

TEST_CASE("MoveToDuringJumpAcceptsRouteNextFrame") {
	const MockGame mg(map_id);
	auto& ch = SetupJumped(mg);

	ch.MoveTo(static_cast<int>(map_id), 6, 6);
	ch.ForceMoveRoute(MakeRoute({ Code::move_down }), 8);
	Step(ch);

	REQUIRE_EQ(ch.GetY(), 7);
	REQUIRE_GT(ch.GetRemainingStep(), 0);
}

TEST_CASE("MoveToDuringJumpHopsOnNextStepOnly") {
	const MockGame mg(map_id);
	auto& ch = SetupJumped(mg);

	ch.MoveTo(static_cast<int>(map_id), 6, 6);
	ch.ForceMoveRoute(MakeRoute({ Code::move_down }), 8);
	Step(ch);
	REQUIRE(ch.IsJumping());

	Step(ch);
	REQUIRE_GT(ch.GetJumpHeight(), 0);

	while (!ch.IsStopping()) {
		Step(ch);
	}
	REQUIRE(!ch.IsJumping());

	ch.ForceMoveRoute(MakeRoute({ Code::move_down }), 8);
	Step(ch);
	Step(ch);
	REQUIRE_EQ(ch.GetJumpHeight(), 0);
}

TEST_SUITE_END();
