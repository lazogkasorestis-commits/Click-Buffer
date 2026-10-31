#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

// Returns true if a click would do something *right now* for this player.
// Modes where a click always works (ship, ufo, wave, swing) are treated as "can act",
// so nothing gets buffered there.
static bool canAct(PlayerObject* p, bool isP1) {
	if (!p) return true;
	if (p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing) return true;
	if (p->m_isOnGround) return true;

	// touching an orb that hasn't been used yet
	if (p->m_touchingRings) {
		for (auto obj : CCArrayExt<GameObject*>(p->m_touchingRings)) {
			bool used = isP1 ? obj->m_hasBeenActivated : obj->m_hasBeenActivatedP2;
			if (!used) return true;
		}
	}
	return false;
}

class $modify(BufferGJBGL, GJBaseGameLayer) {
	struct Fields {
		bool physicalDown[2] = {false, false}; // what the user's finger/key is really doing
		bool pending[2]      = {false, false}; // an early click waiting to be fired
		int framesLeft[2]    = {0, 0};
		bool releaseNext[2]  = {false, false}; // synthetic release scheduled for next frame
		bool internal        = false;          // guard so our own calls aren't re-recorded
	};

	void handleButton(bool down, int button, bool isPlayer1) {
		auto f = m_fields.self();
		int i = isPlayer1 ? 0 : 1;

		if (f->internal || button != 1 || !Mod::get()->getSettingValue<bool>("enabled")) {
			GJBaseGameLayer::handleButton(down, button, isPlayer1);
			return;
		}

		f->physicalDown[i] = down;
		GJBaseGameLayer::handleButton(down, button, isPlayer1);

		if (down) {
			auto player = isPlayer1 ? m_player1 : m_player2;
			if (!canAct(player, isPlayer1)) {
				// click came too early -> remember it
				f->framesLeft[i] = (int)Mod::get()->getSettingValue<int64_t>("buffer-frames");
				f->pending[i]    = f->framesLeft[i] > 0;
			} else {
				f->pending[i] = false;
			}
		}
	}

	void update(float dt) {
		GJBaseGameLayer::update(dt);
		if (!Mod::get()->getSettingValue<bool>("enabled")) return;

		auto f = m_fields.self();

		for (int i = 0; i < 2; i++) {
			bool isP1 = (i == 0);
			auto player = isP1 ? m_player1 : m_player2;

			// finish a synthetic tap from last frame
			if (f->releaseNext[i]) {
				f->releaseNext[i] = false;
				if (!f->physicalDown[i]) {
					f->internal = true;
					GJBaseGameLayer::handleButton(false, 1, isP1);
					f->internal = false;
				}
			}

			if (!f->pending[i]) continue;

			if (canAct(player, isP1)) {
				f->pending[i] = false;
				f->internal = true;
				// fresh press (release first if the real button is still held,
				// because orbs need a new press, not a held one)
				if (f->physicalDown[i]) GJBaseGameLayer::handleButton(false, 1, isP1);
				GJBaseGameLayer::handleButton(true, 1, isP1);
				f->internal = false;
				if (!f->physicalDown[i]) f->releaseNext[i] = true;
				continue;
			}

			// still can't act: use up one frame of the buffer
			f->framesLeft[i]--;
			if (f->framesLeft[i] <= 0) f->pending[i] = false;
		}
	}
};
