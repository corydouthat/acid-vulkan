//// Acid Game Engine - Vulkan
//// Testbench
//
//#include "phVkEngine.hpp"
//
//int main()
//{
//	phVkEngine<float> engine;
//
//	//engine.init();
//
//	//engine.run();
//
//	//engine.cleanup();
//
//	return 0;
//}




//#include <acid_physics.hpp>
#include <phVkEngine.hpp>
#include <phVkCamera.hpp>

#define float_type float

phVkEngine<float_type> engine;
//phSystem<float_type> physics;

bool SceneSetup(/*std::string scene_path*/);
void HandleUserInputs();
void UpdateObjectTranforms();

int main()
{
	// -- Initialize Graphis Engine --
	// Initialize engine
	// Initialize window + Vulkan
	engine.init(1920, 1080, "Untitled Game");

	// -- Initialize GUI --
	// Initialize
	//engine.initGUI();
	// TODO: configure GUI

	// -- Initialize User Inputs --
	//engine.setUserInputCallback(HandleUserInputs());

	// -- Load Scene File --
	// Load scene definition
	// Load assets
	// Scene setup in engine
	// Link with physics
	// Configure Vulkan for scene
	SceneSetup(/*"scene/test_scene.xml"*/);

	// -- Run Loop --
	// Physics and render time management (threads?)
	std::chrono::time_point<std::chrono::steady_clock> t_start;
	std::chrono::duration<float> t_interval;
	float frame_interval = 0.0f;
	float second_interval = 0.0f;
	unsigned int frame_count = 0;

	while (engine.isRunning())
	{
		// Update timer
		t_interval = std::chrono::high_resolution_clock::now() - t_start;
		t_start = std::chrono::high_resolution_clock::now();

		frame_interval += float(t_interval.count());
		second_interval += float(t_interval.count());

		// Display FPS
		if (second_interval >= 1.0)
		{
			std::cout << "FPS: " << frame_count / second_interval << "\n";
			second_interval = 0.0;
			frame_count = 0;
		}

		// Once per frame:
		if (frame_interval >= 0.01666667)	//60fps
		{
			// Game logic
			// TODO

			// Physics update
			// TODO: using constant frame interval for testing
			//physics.update(0.0167 /*frame_interval*/);

			// Transfer physics transforms to Vulkan engine
			UpdateObjectTranforms();

			// Render
			engine.run();

			// TODO: sleep if engine.stopRendering();

			frame_interval = 0.0f;
			frame_count++;	//FPS
		}
	}

	// -- Cleanup --
	engine.cleanup();
	//physics.cleanup();

	return 0;
}


bool SceneSetup(/*std::string scene_path*/)
{
	// TODO: load scene definition xml and use to define scene data

	// Load scene graphics file(s)
	engine.loadScene("../../../../scene/test_scene.glb");

	// Set up camera
	engine.active_camera = engine.cameras.push(phVkCamera<float_type>(
		Vec3<float_type>(0, 0, 1000),
		Vec3<float_type>(0, 0, 0)));

	// TODO: Set up lighting
	// TODO: Set up object initial positions
	// TODO: Set up physics

	return true;
}


void HandleUserInputs()
{

}


void UpdateObjectTranforms()
{

}
