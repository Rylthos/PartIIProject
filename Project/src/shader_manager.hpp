#pragma once

#include <functional>
#include <initializer_list>
#include <unordered_map>

#include "vulkan/vulkan.h"

#include "slang-com-ptr.h"
#include "slang.h"

class ShaderManager {

  public:
    static ShaderManager* getInstance();
    void init(VkDevice device);
    void cleanup();

    void addShader(std::initializer_list<const char*> files, std::function<void()>&& createPipeline,
        std::function<void()>&& destroyPipeline);

    VkShaderModule getShaderModule(const char* filename);

  private:
    ShaderManager() { }

    void generateShaderModule(const char* filename, size_t pipelineIndex);

  private:
    struct PipelineFunction {
        std::function<void()> creator;
        std::function<void()> destructor;
    };

    VkDevice m_VkDevice;

    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    Slang::ComPtr<slang::ISession> m_Session;

    std::unordered_map<const char*, size_t> m_FunctionMap;
    std::vector<PipelineFunction> m_PipelineFunctions;

    std::unordered_map<const char*, VkShaderModule> m_ShaderModules;
};
