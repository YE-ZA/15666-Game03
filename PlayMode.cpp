#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint rhythm_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > rhythm_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("rhythm.pnct"));
	rhythm_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > rhythm_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("rhythm.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = rhythm_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = rhythm_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > beep_box_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("BeepBox.wav"));
});

Load< Sound::Sample > clap_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("clap.wav"));
});

PlayMode::PlayMode() : scene(*rhythm_scene) {

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music playing:
	music = Sound::play_3D(*beep_box_sample, 1.0, glm::vec3(0.0f));

	//get scene transforms
	for (auto &transform : scene.transforms) {
		if (transform.name == "Airplane") airplane = &transform;
		for (uint32_t i = 0; i < 9; ++i)
			if (transform.name == "Note.00" + std::to_string(i+1)) 
			{
				notes.emplace_back(&transform);
				claps.emplace(&transform, true);
			}
		for (uint32_t i = 9; i < 92; ++i)
			if (transform.name == "Note.0" + std::to_string(i+1))
			{
				notes.emplace_back(&transform);
				claps.emplace(&transform, true);
			}
	}

}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_LEFT) {
			left.downs += 1;
			left.pressed = true;
			airplane->position.y -= 2.05f;
			if(airplane->position.y < -4.1f)
				airplane->position.y = -4.1f;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.downs += 1;
			right.pressed = true;
			airplane->position.y += 2.05f;
			if(airplane->position.y > 4.1f)
				airplane->position.y = 4.1f;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_LEFT) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	timer += elapsed;
	bias += 0.0005f * 60.0f * elapsed;

	for(auto note: notes)
	{
		note->position.x += (29.375f + bias) * elapsed;
	}

	for(auto it = claps.begin(); it != claps.end(); ++it)
	{
		if(glm::length((*it).first->position - airplane->position) < 0.75f)
		{
			if((*it).second)
			{
				sound = Sound::play_3D(*clap_sample, 10.0f, airplane->position);
				(*it).second = false;
				hits++;
			}
		}
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Use LEFT and RIGHT to clap your hands to the rhythm :)",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Use LEFT and RIGHT to clap your hands to the rhythm :)",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		lines.draw_text(std::to_string(hits) + " / 92",
		glm::vec3(-1.7, 0.8, 0.0),
		glm::vec3(1.5*H, 0.0f, 0.0f), glm::vec3(0.0f, 1.5*H, 0.0f),
		glm::u8vec4(0xff, 0x00, 0x00, 0x00));

		lines.draw_text(std::to_string(hits) + " / 92",
		glm::vec3(-1.7 + ofs, 0.8 + ofs, 0.0),
		glm::vec3(1.5*H, 0.0f, 0.0f), glm::vec3(0.0f, 1.5*H, 0.0f),
		glm::u8vec4(0xff, 0xff, 0x00, 0x00));
		
		if(timer > 44.0f)
		{
			if(hits > 80)
			{
				lines.draw_text("Good job! You're the master.",
				glm::vec3(-0.85, 0.0, 0.0),
				glm::vec3(2*H, 0.0f, 0.0f), glm::vec3(0.0f, 2*H, 0.0f),
				glm::u8vec4(0xff, 0x00, 0x00, 0x00));
				
				lines.draw_text("Good job! You're the master.",
				glm::vec3(-0.85 + ofs, 0.0 + ofs, 0.0),
				glm::vec3(2*H, 0.0f, 0.0f), glm::vec3(0.0f, 2*H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0x00, 0x00));
			}
			else if(hits > 60)
			{
				lines.draw_text("Not bad. Try it again",
				glm::vec3(-0.69, 0.0, 0.0),
				glm::vec3(2*H, 0.0f, 0.0f), glm::vec3(0.0f, 2*H, 0.0f),
				glm::u8vec4(0xff, 0x00, 0x00, 0x00));

				lines.draw_text("Not bad. Try it again",
				glm::vec3(-0.69 + ofs, 0.0 + ofs, 0.0),
				glm::vec3(2*H, 0.0f, 0.0f), glm::vec3(0.0f, 2*H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0x00, 0x00));
			}
			else
			{
				lines.draw_text("Seriously? Are you human or Doraemon?",
				glm::vec3(-1.35, 0.0, 0.0),
				glm::vec3(2*H, 0.0f, 0.0f), glm::vec3(0.0f, 2*H, 0.0f),
				glm::u8vec4(0xff, 0x00, 0x00, 0x00));

				lines.draw_text("Seriously? Are you human or Doraemon?",
				glm::vec3(-1.35 + ofs, 0.0 + ofs, 0.0),
				glm::vec3(2*H, 0.0f, 0.0f), glm::vec3(0.0f, 2*H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0x00, 0x00));
			}
		}
	}
	GL_ERRORS();
}
