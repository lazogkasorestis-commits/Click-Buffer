#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// True if a click would do something *right now* for this player.
// Ship, ufo, wave and swing always react to clicks, so nothing is buffered there.
static bool canAct(PlayerObject* p) {
	if (!p) return true;
	if (p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing) return true;
	if (p->m_isOnGround) return true;
	if (p->m_touchingRings && p->m_touchingRings->count() > 0) return true; // touching an orb
	return false;
}

class $modify(BufferGJBGL, GJBaseGameLayer) {
	struct Fields {
		bool physicalDown[2] = {false, false}; // what your finger/key is really doing
		bool gameDown[2]     = {false, false}; // what the game has been told
		bool pending[2]      = {false, false}; // an early click waiting for its moment
		int  framesLeft[2]   = {0, 0};
		bool releaseNext[2]  = {false, false}; // release to send on the next frame
	};

	void handleButton(bool down, int button, bool isPlayer1) {
		auto f = m_fields.self();
		int i = isPlayer1 ? 0 : 1;

		if (button != 1 || !Mod::get()->getSettingValue<bool>("enabled")) {
			GJBaseGameLayer::handleButton(down, button, isPlayer1);
			return;
		}

		if (down) {
			// decide BEFORE the game sees the click (a normal jump would change the answer)
			auto player = isPlayer1 ? m_player1 : m_player2;
			int frames = (int)Mod::get()->getSettingValue<int64_t>("buffer-frames");
			bool early = frames > 0 && !canAct(player);

			f->physicalDown[i] = true;
			f->gameDown[i] = true;
			f->pending[i] = early;
			f->framesLeft[i] = frames;
		} else {
			f->physicalDown[i] = false;
			f->gameDown[i] = false;
		}

		// real clicks and releases ALWAYS reach the game, so a release can never get lost
		GJBaseGameLayer::handleButton(down, button, isPlayer1);
	}

	void update(float dt) {
		GJBaseGameLayer::update(dt);
		if (!Mod::get()->getSettingValue<bool>("enabled")) return;

		auto f = m_fields.self();

		for (int i = 0; i < 2; i++) {
			bool isP1 = (i == 0);
			auto player = isP1 ? m_player1 : m_player2;

			// finish a tap we started last frame
			if (f->releaseNext[i]) {
				f->releaseNext[i] = false;
				if (!f->physicalDown[i]) {
					GJBaseGameLayer::handleButton(false, 1, isP1);
					f->gameDown[i] = false;
				}
			}

			if (f->pending[i]) {
				if (canAct(player)) {
					// the right moment: send a fresh click now
					f->pending[i] = false;
					if (f->physicalDown[i]) GJBaseGameLayer::handleButton(false, 1, isP1); // orbs need a new press
					GJBaseGameLayer::handleButton(true, 1, isP1);
					f->gameDown[i] = true;
					if (!f->physicalDown[i]) f->releaseNext[i] = true;
				} else if (--f->framesLeft[i] <= 0) {
					f->pending[i] = false; // waited too long
				}
			}

			// safety net: the game thinks you're holding but you're not -> release
			if (!f->physicalDown[i] && f->gameDown[i] && !f->releaseNext[i]) {
				GJBaseGameLayer::handleButton(false, 1, isP1);
				f->gameDown[i] = false;
			}
		}
	}
};
