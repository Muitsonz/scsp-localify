#include <stdinclude.hpp>
#include "camera/baseCamera.hpp"
#include <mhotkey.hpp>


namespace SCCamera {
	BaseCamera::Camera baseCamera{};
	Vector2Int_t currRenderResolution{ -1, -1 };
	std::map<int, int> CameraControlKeyMapping{};

	bool rMousePressFlg = false;

	void reset_camera() {
		baseCamera.reset();
	}

	void camera_forward() {  // 向前
		baseCamera.set_lon_move(0, LonMoveHState::LonMoveForward);
	}
	void camera_back() {  // 后退
		baseCamera.set_lon_move(180, LonMoveHState::LonMoveBack);
	}
	void camera_left() {  // 向左
		baseCamera.set_lon_move(90);
	}
	void camera_right() {  // 向右
		baseCamera.set_lon_move(-90);
	}
	void camera_down() {  // 向下
		float preStep = BaseCamera::moveStep / BaseCamera::smoothLevel;

		for (int i = 0; i < BaseCamera::smoothLevel; i++) {
			baseCamera.pos.y -= preStep;
			baseCamera.lookAt.y -= preStep;
			Sleep(BaseCamera::sleepTime);
		}
	}
	void camera_up() {  // 向上
		float preStep = BaseCamera::moveStep / BaseCamera::smoothLevel;

		for (int i = 0; i < BaseCamera::smoothLevel; i++) {
			baseCamera.pos.y += preStep;
			baseCamera.lookAt.y += preStep;
			Sleep(BaseCamera::sleepTime);
		}
	}
	void cameraLookat_up(float mAngel, bool mouse = false) {
		baseCamera.horizontalAngle += mAngel;
		if (baseCamera.horizontalAngle >= 90) baseCamera.horizontalAngle = 89.99;
		baseCamera.updateVertLook();
	}
	void cameraLookat_down(float mAngel, bool mouse = false) {
		baseCamera.horizontalAngle -= mAngel;
		if (baseCamera.horizontalAngle <= -90) baseCamera.horizontalAngle = -89.99;
		baseCamera.updateVertLook();
	}
	void cameraLookat_left(float mAngel) {
		baseCamera.verticalAngle += mAngel;
		if (baseCamera.verticalAngle >= 360) baseCamera.verticalAngle = -360;
		baseCamera.setHoriLook(baseCamera.verticalAngle);
	}
	void cameraLookat_right(float mAngel) {
		baseCamera.verticalAngle -= mAngel;
		if (baseCamera.verticalAngle <= -360) baseCamera.verticalAngle = 360;
		baseCamera.setHoriLook(baseCamera.verticalAngle);
	}
	void changeCameraFOV(float value) {
		baseCamera.fov += value;
	}

	void onMouseScroll(LONG value) {
		changeCameraFOV(-value);
	}

	void mouseMove(LONG moveX, LONG moveY, int mouseEventType) {
		if (mouseEventType == 1) {  // down
			rMousePressFlg = true;
			int fCount = 0;
			while (ShowCursor(false) >= 0) {
				if (fCount >= 5) break;
				fCount++;
			}
		}
		else if (mouseEventType == 2) {  // up
			rMousePressFlg = false;
			int fCount = 0;
			while (ShowCursor(true) < 0) {
				if (fCount >= 5) break;
				fCount++;
			}
		}
		else if (mouseEventType == 3) {  // move
			std::thread([moveX, moveY]() {
				if (!rMousePressFlg) return;
				if (moveX > 0) {
					cameraLookat_right(moveX * g_free_camera_mouse_speed / 100.0);
				}
				else if (moveX < 0) {
					cameraLookat_left(-moveX * g_free_camera_mouse_speed / 100.0);
				}
				if (moveY > 0) {
					cameraLookat_down(moveY * g_free_camera_mouse_speed / 100.0, true);
				}
				else if (moveY < 0) {
					cameraLookat_up(-moveY * g_free_camera_mouse_speed / 100.0, true);
				}
				// printf("move x: %d, y: %d\n", moveX, moveY);
				}).detach();
		}
		else if (mouseEventType == 4) {  // scroll
			onMouseScroll(moveY);
		}
	}

	struct CameraMoveState {
		bool w = false;
		bool s = false;
		bool a = false;
		bool d = false;
		bool ctrl = false;
		bool space = false;
		bool up = false;
		bool down = false;
		bool left = false;
		bool right = false;
		bool q = false;
		bool e = false;
		bool i = false;
		bool k = false;
		bool j = false;
		bool l = false;
		bool threadRunning = false;

		void resetAll() {
			auto p = reinterpret_cast<bool*>(this);
			const auto numMembers = sizeof(*this) / sizeof(bool);
			for (size_t i = 0; i < numMembers; ++i) {
				p[i] = false;
			}
		}
	} cameraMoveState;

	void onKillFocus() {
		mouseMove(0, 0, 2);
		cameraMoveState.resetAll();

		std::thread([]() {
			Sleep(50);

			}).detach();
	}

	void cameraRawInputThread() {
		using namespace BaseCamera;

		std::thread([]() {
			if (cameraMoveState.threadRunning) return;
			cameraMoveState.threadRunning = true;
			while (true) {
				if (cameraMoveState.w) camera_forward();
				if (cameraMoveState.s) camera_back();
				if (cameraMoveState.a) camera_left();
				if (cameraMoveState.d) camera_right();
				if (cameraMoveState.ctrl) camera_down();
				if (cameraMoveState.space) camera_up();
				if (cameraMoveState.up) cameraLookat_up(moveAngel);
				if (cameraMoveState.down) cameraLookat_down(moveAngel);
				if (cameraMoveState.left) cameraLookat_left(moveAngel);
				if (cameraMoveState.right) cameraLookat_right(moveAngel);
				if (cameraMoveState.q) changeCameraFOV(0.5f);
				if (cameraMoveState.e) changeCameraFOV(-0.5f);
				// if (cameraMoveState.i) changeLiveFollowCameraOffsetY(moveStep / 3);
				// if (cameraMoveState.k) changeLiveFollowCameraOffsetY(-moveStep / 3);
				// if (cameraMoveState.j) changeLiveFollowCameraOffsetX(moveStep * 10);
				// if (cameraMoveState.l) changeLiveFollowCameraOffsetX(-moveStep * 10);
				Sleep(10);
			}
			}).detach();
	}

	void on_cam_rawinput_keyboard(int message, int key) {
		// printf("key %d - %d\n", message, key);
		if (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) {
			auto mapped = CameraControlKeyMapping.find(key);
			if (mapped == CameraControlKeyMapping.end()) {
				return;
			}
			bool isKeyDown = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
			switch (mapped->second) {
			case KEY_W:
				cameraMoveState.w = isKeyDown; break;
			case KEY_S:
				cameraMoveState.s = isKeyDown; break;
			case KEY_A:
				cameraMoveState.a = isKeyDown; break;
			case KEY_D:
				cameraMoveState.d = isKeyDown; break;
			case KEY_CTRL:
				cameraMoveState.ctrl = isKeyDown; break;
			case KEY_SPACE:
				cameraMoveState.space = isKeyDown; break;
			case KEY_UP:
				cameraMoveState.up = isKeyDown; break;
			case KEY_DOWN:
				cameraMoveState.down = isKeyDown; break;
			case KEY_LEFT:
				cameraMoveState.left = isKeyDown; break;
			case KEY_RIGHT:
				cameraMoveState.right = isKeyDown; break;
			case KEY_Q:
				cameraMoveState.q = isKeyDown; break;
			case KEY_E:
				cameraMoveState.e = isKeyDown; break;
				//case 'I':
				//	cameraMoveState.i = isKeyDown; break;
				//case 'K':
				//	cameraMoveState.k = isKeyDown; break;
				//case 'J':
				//	cameraMoveState.j = isKeyDown; break;
				//case 'L':
				//	cameraMoveState.l = isKeyDown; break;
			case KEY_R: {
				if (message == WM_KEYDOWN) reset_camera();
			}; break;
			case KEY_192: {
				if (message == WM_KEYDOWN) mouseMove(0, 0, rMousePressFlg ? 2 : 1);
			}; break;
			default: break;
			}
		}
	}

	void initCameraSettings() {
		reset_camera();
		cameraRawInputThread();
		MHotkey::setMKeyBoardRawCallBack(on_cam_rawinput_keyboard);
		// MHotkey::SetKeyCallBack(on_keyboard_down);
	}

}
