#pragma once

#include <functional>
#include <initializer_list>
#include <unordered_map>

#include "vulkan/vulkan.h"

#include "slang-com-ptr.h"
#include "slang.h"

class ShaderManager {
    using ModuleName = const char*;
    using FileName = const char*;

  public:
    static ShaderManager* getInstance();
    void init(VkDevice device);
    void cleanup();

    void addModule(ModuleName module, std::function<void()>&& createPipeline,
        std::function<void()>&& destroyPipeline);

    VkShaderModule getShaderModule(ModuleName moduleName);

    void updated(FileName file);

  private:
    ShaderManager() { }

    void generateShaderModule(ModuleName moduleName);
    void addDependencies(ModuleName moduleName);

  private:
    struct PipelineFunction {
        std::function<void()> creator;
        std::function<void()> destructor;
    };

    VkDevice m_VkDevice;

    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    slang::SessionDesc m_SessionDesc;

    std::unordered_map<FileName, std::vector<ModuleName>> m_FileMapping;

    std::unordered_map<ModuleName, std::vector<PipelineFunction>> m_FunctionMap;

    std::unordered_map<ModuleName, VkShaderModule> m_ShaderModules;
    std::unordered_map<ModuleName, Slang::ComPtr<slang::ISession>> m_Sessions;
};
