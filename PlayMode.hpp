#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <unordered_map>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	Scene::Transform *airplane = nullptr;
	float bias = 0.0f;
	float timer = 0.0f;
	uint8_t hits = 0;

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > music;
	std::shared_ptr< Sound::PlayingSample > sound;
	
	//camera:
	Scene::Camera *camera = nullptr;

	std::vector<Scene::Transform*> notes;
	std::unordered_map<Scene::Transform*, bool> claps;

};
