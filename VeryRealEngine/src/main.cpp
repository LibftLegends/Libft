/**
 * @file main.cpp
 * @brief Entry point. All engine setup and the main loop live in
 * Application (src/app/application.hpp); this stays a free function only
 * because the language requires it as the process entry point.
 */
#include "app/application.hpp"

int main(int argc, char **argv)
{
	// The steam emitter and pendulum lamp are hand-placed at world
	// coordinates that only make sense inside house_scene.json's own room
	// layout, so demo_scene.json (the step 1-5 tech demo: physics, PBR
	// materials, multi-light shadows) is still selectable via argv[1].
	const char *scene_path = (argc > 1) ? argv[1] : "assets/scenes/house_scene.json";
	vre::Application application;

	return (application.run(scene_path));
}
