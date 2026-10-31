#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// True if a click would do something *right now* for this player.
// Ship, ufo, wave and swing always react to clicks, so nothing gets held back there.
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
		bool pending[2]      = {false, false}; // an early click being held back
		int  framesLeft[2]   = {0, 0};
		bool releaseNext[2]  = {false, false}; // release to send on the next frame
		bool swallowUp[2]    = {false, false}; // click expired while still held: ignore its release
	};

	void handleButton(bool down, int button, bool isPlayer1) {
		auto f = m_fields.self();
		int i = isPlayer1 ? 0 : 1;

		if (button != 1 || !Mod::get()->getSettingValue<bool>("enabled")) {
			GJBaseGameLayer::handleButton(down, button, isPlayer1);
			return;
		}

		if (down) {
			f->physicalDown[i] = true;
			f->swallowUp[i] = false;

			int frames = (int)Mod::get()->getSettingValue<int64_t>("buffer-frames");
			auto player = isPlayer1 ? m_player1 : m_player2;

			if (frames > 0 && !canAct(player)) {
				// too early: do NOT pass the click to the game yet, just remember it
				f->pending[i] = true;
				f->framesLeft[i] = frames;
				return;
			}
			f->pending[i] = false;
			GJBaseGameLayer::handleButton(true, button, isPlayer1);
		} else {
			f->physicalDown[i] = false;
			if (f->pending[i]) return; // click not delivered yet, its release is handled when it fires
			if (f->swallowUp[i]) { f->swallowUp[i] = false; return; }
			GJBaseGameLayer::handleButton(false, button, isPlayer1);
		}
	}

	void update(float dt) {
		GJBaseGameLayer::update(dt);
		if (!Mod::get()->getSettingValue<bool>("enabled")) return;

		auto f = m_fields.self();

		for (int i = 0; i < 2; i++) {
			bool isP1 = (i == 0);
			auto player = isP1 ? m_player1 : m_player2;

			// release of a click that was already let go while it was being held back
			if (f->releaseNext[i]) {
				f->releaseNext[i] = false;
				if (!f->physicalDown[i]) GJBaseGameLayer::handleButton(false, 1, isP1);
			}

			if (!f->pending[i]) continue;

			if (canAct(player)) {
				// the right moment: deliver the held-back click now
				f->pending[i] = false;
				GJBaseGameLayer::handleButton(true, 1, isP1);
				if (!f->physicalDown[i]) f->releaseNext[i] = true;
			} else if (--f->framesLeft[i] <= 0) {
				// waited too long: drop the click
				f->pending[i] = false;
				if (f->physicalDown[i]) f->swallowUp[i] = true;
			}
		}
	}
};
