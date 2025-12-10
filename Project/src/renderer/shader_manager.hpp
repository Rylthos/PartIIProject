#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string>

#include <map>

#include "vulkan/vulkan_core.h"

#include "slang-com-ptr.h"
#include "slang.h"

class ShaderManager {
    using ModuleName = std::string;
    using FileName = std::string;

    struct PipelineFunction {
        std::function<void()> creator;
        std::function<void()> destructor;
    };

    struct FileHandler {
        std::string filePath;
        size_t refCount;
        std::set<ModuleName> dependents;
    };

    struct ModuleHandler {
        std::string moduleName;
        Slang::ComPtr<slang::ISession> slangSession;
        VkShaderModule shaderModule;
        std::set<FileName> fileDependencies;
        std::vector<PipelineFunction> pipelineFunctions;
    };

  public:
    static ShaderManager* getInstance();
    void init(VkDevice device);
    void cleanup();

    void addModule(const ModuleName& module, std::function<void()>&& createPipeline,
        std::function<void()>&& destroyPipeline);
    void removeModule(const ModuleName& module);

    VkShaderModule getShaderModule(const ModuleName& moduleName);

    void fileUpdated(const FileName& file);
    void moduleUpdated(const ModuleName& module);
    void updateShaders();

    std::optional<std::string> getMacro(std::string name);
    void setMacro(std::string name, std::string value);
    void defineMacro(std::string name);
    void removeMacro(std::string name);

  private:
    ShaderManager() { }

    bool generateShaderModule(ModuleHandler& module);
    void addDependencies(ModuleHandler& module);

    void updateSessionMacros();

    void addFileHandler(ModuleHandler& module, const FileName& file);
    void removeFileHandler(ModuleHandler& module, const FileName& file);

  private:
    VkDevice m_VkDevice;

    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    slang::SessionDesc m_SessionDesc;

    std::map<std::string, std::string> m_Macros;

    std::map<ModuleName, ModuleHandler> m_Modules;

    std::map<FileName, FileHandler> m_FileHandlers;

    std::set<FileName> m_Updates;
    std::mutex m_UpdateMutex;
};
