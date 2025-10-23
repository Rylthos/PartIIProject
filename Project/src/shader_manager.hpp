#pragma once

#include <functional>
#include <mutex>
#include <optional>
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
    void regenerateModule(const ModuleName& module);
    void updateAll();

    std::optional<std::string> getMacro(std::string name);
    void setMacro(std::string name, std::string value);
    void defineMacro(std::string name);
    void removeMacro(std::string name);

  private:
    ShaderManager() { }

    bool generateShaderModule(const ModuleName& moduleName);
    void setDependencies(const ModuleName& moduleName);

    void updateSessionDesc();

  private:
    struct PipelineFunction {
        std::function<void()> creator;
        std::function<void()> destructor;
    };

    VkDevice m_VkDevice;

    std::map<std::string, std::string> m_Macros;

    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    slang::SessionDesc m_SessionDesc;

    std::map<FileName, std::set<ModuleName>> m_FileMapping;
    std::map<ModuleName, std::set<FileName>> m_InverseFileMapping;
    std::map<ModuleName, std::vector<PipelineFunction>> m_FunctionMap;

    std::map<ModuleName, VkShaderModule> m_ShaderModules;
    std::map<ModuleName, Slang::ComPtr<slang::ISession>> m_Sessions;

    std::set<FileName> m_Updates;
    std::mutex m_UpdateMutex;
};
