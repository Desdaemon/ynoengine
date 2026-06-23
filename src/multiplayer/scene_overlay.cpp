#include "scene_overlay.h"

Scene_Overlay::Scene_Overlay(UpdateFn fn) : OnUpdate(fn) {
	type = SceneType::ChatOverlay;
}

void Scene_Overlay::vUpdate() {
	if (OnUpdate)
		OnUpdate();
}
