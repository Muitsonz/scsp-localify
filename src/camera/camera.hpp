#pragma once
#include "camera/baseCamera.hpp"

namespace SCCamera {
	extern BaseCamera::Camera baseCamera;
	extern Vector2Int_t currRenderResolution;
	extern std::map<int, int> CameraControlKeyMapping;

	void onKillFocus();
	void initCameraSettings();
	void mouseMove(LONG moveX, LONG moveY, int mouseEventType);
}
