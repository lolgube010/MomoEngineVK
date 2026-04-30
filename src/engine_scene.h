#pragma once
#include <vk/render_types.h>
#include <camera.h>
#include <vk/loader.h>

struct Momo_Model
{
    std::string _name;
    glm::mat4x4 _transform;
    std::shared_ptr<LoadedGLTF> _scene;
};

class EngineScene
{
public:
    void Init();
    void Update(float aDt, VkExtent2D aWindowExtent);

    const DrawContext&  GetDrawContext()  const { return _mainDrawContext; }
    const GPUSceneData& GetSceneData()    const { return _sceneData; }

    // Public so EngineRenderer::ImGui_Run can read them directly
    Camera      _mainCamera;
    glm::vec4   _tempAmbientColor = glm::vec4(1.f);
    glm::vec4   _tempSunColor     = glm::vec4(1.f);
    glm::vec4   _tempSunDir       = glm::vec4(0, 1, 0.5, 1.f);

    std::vector<Momo_Model> _loadedModels;

private:
    DrawContext  _mainDrawContext;
    GPUSceneData _sceneData = {};
};
