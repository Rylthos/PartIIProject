#pragma once

#include <functional>
#include <mutex>
#include <set>
#include <thread>

#include <map>

#include "vulkan/vulkan.h"

#include "slang-com-ptr.h"
#include "slang.h"

class ShaderManager {
    using ModuleName = std::string;
    using FileName = std::string;

  public:
    static ShaderManager* getInstance();
    void init(VkDevice device);
    void cleanup();

    void addModule(const ModuleName& module, std::function<void()>&& createPipeline,
        std::function<void()>&& destroyPipeline);

    VkShaderModule getShaderModule(const ModuleName& moduleName);

    void updated(const FileName& file);
    void updateAll();

  private:
    ShaderManager() { }

    void generateShaderModule(const ModuleName& moduleName);
    void addDependencies(const ModuleName& moduleName);

  private:
    struct PipelineFunction {
        std::function<void()> creator;
        std::function<void()> destructor;
    };

    VkDevice m_VkDevice;

    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    slang::SessionDesc m_SessionDesc;

    std::map<FileName, std::vector<ModuleName>> m_FileMapping;
    std::map<ModuleName, std::vector<PipelineFunction>> m_FunctionMap;

    std::map<ModuleName, VkShaderModule> m_ShaderModules;
    std::map<ModuleName, Slang::ComPtr<slang::ISession>> m_Sessions;

    std::set<FileName> m_Updates;
    std::mutex m_UpdateMutex;
};
