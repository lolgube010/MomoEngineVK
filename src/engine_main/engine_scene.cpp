#include <engine_main/engine_scene.h>
#include <engine_main/engine.h>
#include <vk/loader.h>

#include <input/Input.h>
#include <cvars/cvars.h>
#include <api/MomoTracy.h>

#include <chrono>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
// ---------------------------------------------------------------------------
// Test CVars
// ---------------------------------------------------------------------------

momo_cvars::AutoCVar_Int    CVAR_TestCheckbox("test.checkbox", "just a checkbox",           0,     momo_cvars::CVarFlags::EditCheckbox);
momo_cvars::AutoCVar_Int    CVAR_TestInt(     "test.int",      "just a configurable int",   42);
momo_cvars::AutoCVar_Int    CVAR_TestFloat(   "test.float",    "just a configurable float", 13.37f);
momo_cvars::AutoCVar_String CVAR_TestString(  "test.string",   "just a configurable string", "just a configurable string");

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void EngineScene::Init()
{
    const std::string structurePath = {R"(..\..\assets\structure.glb)"};
    const auto structureFile = momo_vkGLTF::load_gltf(structurePath);
    assert(structureFile.has_value());

    const std::string sponzaPath = {R"(..\..\assets\sponza\sponza-png.glb)"};
    const auto sponzaFile = momo_vkGLTF::load_gltf(sponzaPath);
    assert(sponzaFile.has_value());

    Momo_Model structure;
    structure._name      = "structure";
    structure._scene     = *structureFile;
    structure._transform = glm::mat4x4(glm::translate(glm::vec3(0.f, -50.f, 0.f)));
    _loadedModels.push_back(structure);

    Momo_Model sponza;
    sponza._name      = "sponza";
    sponza._scene     = *sponzaFile;
    sponza._transform = glm::mat4x4(1.f);
    _loadedModels.push_back(sponza);
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------

void EngineScene::Update(const double aDt, const VkExtent2D aWindowExtent, const Camera& aCamera)
{
    PROFILE_SCOPE_N("Update_Scene")
    const auto start = std::chrono::system_clock::now();

    _mainDrawContext._opaqueSurfaces.clear();
    _mainDrawContext._transparentSurfaces.clear();

    for (const auto& model : _loadedModels)
    {
        model._scene->Draw(model._transform, _mainDrawContext); // this runs MeshNode::Draw
    }

    const glm::mat4 view = GetViewMatrix(aCamera);
    const glm::mat4 projection = GetProjectionMatrix(aCamera,
        static_cast<float>(aWindowExtent.width),
        static_cast<float>(aWindowExtent.height));

    _sceneData._view     = view;
    _sceneData._proj     = projection;
    _sceneData._viewProj = projection * view;

    _sceneData._ambientColor      = _tempAmbientColor;
    _sceneData._sunlightColor     = _tempSunColor;
    _sceneData._sunlightDirection = _tempSunDir;

    const auto end     = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    VulkanEngine::Get()._stats._sceneUpdateTime = elapsed.count() / 1000.f;
}

glm::mat4 EngineScene::GetProjectionMatrix(const Camera& aCamera, const float aWidth, const float aHeight)
{
    auto matrix = glm::perspective(glm::radians(aCamera._cameraFOV), aWidth / aHeight, 10000.f, 0.1f);
    matrix[1][1] *= -1; // invert the Y direction on projection matrix so that we are more similar to opengl and gltf axis
    return matrix;
}

glm::mat4 EngineScene::GetViewMatrix(const Camera& aCamera)
{
    const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), aCamera._position);
    const glm::mat4 cameraRotation = CameraUtil::get_rotation_matrix(aCamera);
    return glm::inverse(cameraTranslation * cameraRotation);
}

