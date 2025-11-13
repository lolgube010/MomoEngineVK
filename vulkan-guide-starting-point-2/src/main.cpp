#include <vk_engine.h>


// In the future, this could be a good place to set some configuration parameters brought from the command line arguments at argc/argv or a settings file.
int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.init();

	engine.run();

	engine.cleanup();

	return 0;
}
