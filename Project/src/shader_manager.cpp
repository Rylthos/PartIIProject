#include "shader_manager.hpp"

#include "file_watcher.hpp"

#include "logger.hpp"
#include "slang.h"
#include <optional>
#include <vulkan/vulkan_core.h>

#define SLANG_DIAG(diagnostics)                                                                    \
    do {                                                                                           \
        if ((diagnostics) != nullptr) {                                                            \
            LOG_ERROR((const char*)(diagnostics)->getBufferPointer());                             \
        }                                                                                          \
    } while (0)

ShaderManager* ShaderManager::getInstance()
{
    static ShaderManager manager;
    return &manager;
}

void ShaderManager::init(VkDevice device)
{
    m_VkDevice = device;

    slang::createGlobalSession(m_GlobalSession.writeRef());

    static slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = m_GlobalSession->findProfile("spirv_1_5");

    m_SessionDesc.targets = &targetDesc;
    m_SessionDesc.targetCount = 1;

    static std::vector<slang::CompilerOptionEntry> options = {
        { slang::CompilerOptionName::EmitSpirvDirectly,
         { slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr }                    },
        { slang::CompilerOptionName::DebugInformation,
         { slang::CompilerOptionValueKind::Int,
                SlangDebugInfoLevel::SLANG_DEBUG_INFO_LEVEL_STANDARD, 0, nullptr, nullptr } },
        { slang::CompilerOptionName::Capability,
         { slang::CompilerOptionValueKind::Int,
                m_GlobalSession->findCapability("SPV_KHR_non_semantic_info"), 0, nullptr,
                nullptr }                                                                   },
        { slang::CompilerOptionName::Capability,
         { slang::CompilerOptionValueKind::Int,
                m_GlobalSession->findCapability("spvShaderClockKHR"), 0, nullptr, nullptr } },
    };

    m_SessionDesc.compilerOptionEntries = options.data();
    m_SessionDesc.compilerOptionEntryCount = options.size();

    static const char* searchPaths[] = { "res/shaders/" };
    m_SessionDesc.searchPaths = searchPaths;
    m_SessionDesc.searchPathCount = 1;

    FileWatcher::getInstance()->init();

    LOG_DEBUG("Initialised Shader manager");
}

void ShaderManager::cleanup()
{
    LOG_DEBUG("Shader manager cleanup");
    {
        const auto moduleCopy = m_Modules;
        for (const auto& module : moduleCopy) {
            removeModule(module.first);
        }
    }
    m_Modules.clear();

    assert(m_FileHandlers.size() == 0 && "File handlers should be 0");

    FileWatcher::getInstance()->stop();
}

void ShaderManager::addModule(const ModuleName& module, std::function<void()>&& createPipeline,
    std::function<void()>&& destroyPipeline)
{
    PipelineFunction function {
        .creator = createPipeline,
        .destructor = destroyPipeline,
    };

    LOG_DEBUG("Add module: {}", module);

    if (m_Modules.contains(module)) {
        m_Modules[module].pipelineFunctions.push_back(function);
    } else {
        ModuleHandler handler = {
            .moduleName = module,
            .pipelineFunctions = { function },
        };

        generateShaderModule(handler);
        addDependencies(handler);

        m_Modules.insert({ module, handler });
    }
}

void ShaderManager::removeModule(const ModuleName& module)
{
    assert(m_Modules.contains(module) && "Cannot remove module that doesn't exist");
    ModuleHandler& handler = m_Modules[module];

    LOG_DEBUG("Remove module: {}", module);

    auto fileDependencies = handler.fileDependencies;
    for (const auto& dependent : fileDependencies) {
        removeFileHandler(handler, dependent);
    }

    vkDestroyShaderModule(m_VkDevice, handler.shaderModule, nullptr);
    m_Modules.erase(module);
}

VkShaderModule ShaderManager::getShaderModule(const ModuleName& module)
{
    return m_Modules[module].shaderModule;
}

void ShaderManager::fileUpdated(const FileName& file)
{
    std::lock_guard<std::mutex> _lock(m_UpdateMutex);
    m_Updates.insert(file);
}

void ShaderManager::moduleUpdated(const ModuleName& module)
{
    std::lock_guard<std::mutex> _lock(m_UpdateMutex);
    if (!m_Modules[module].fileDependencies.empty())
        m_Updates.insert(*m_Modules[module].fileDependencies.begin());
}

void ShaderManager::updateShaders()
{
    if (m_Updates.empty()) {
        return;
    }

    updateSessionMacros();

    vkDeviceWaitIdle(m_VkDevice);
    std::lock_guard<std::mutex> _lock(m_UpdateMutex);

    for (FileName file : m_Updates) {
        std::set<ModuleName> modules = m_FileHandlers[file].dependents;

        for (const ModuleName& module : modules) {
            ModuleHandler& handler = m_Modules[module];
            LOG_DEBUG("Reloading {}", module);
            if (!generateShaderModule(handler)) {
                LOG_INFO("Failed to reload {}", module);
                continue;
            }
            addDependencies(handler);

            const std::vector<PipelineFunction>& functions = handler.pipelineFunctions;
            for (const auto& function : functions) {
                function.destructor();
                function.creator();
            }
        }
    }
    m_Updates.clear();
}

std::optional<std::string> ShaderManager::getMacro(std::string name)
{
    if (m_Macros.find(name) != m_Macros.end()) {
        return std::optional<std::string>(m_Macros.at(name));
    }
    return std::nullopt;
}

void ShaderManager::setMacro(std::string name, std::string value) { m_Macros[name] = value; }

void ShaderManager::defineMacro(std::string name) { m_Macros.insert({ name.c_str(), "" }); }
void ShaderManager::removeMacro(std::string name) { m_Macros.erase(name); }

bool ShaderManager::generateShaderModule(ModuleHandler& module)
{
    updateSessionMacros();

    Slang::ComPtr<slang::ISession> session;
    m_GlobalSession->createSession(m_SessionDesc, session.writeRef());

    Slang::ComPtr<slang::IModule> slangModule;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        slangModule = session->loadModule(module.moduleName.c_str(), diagnostics.writeRef());
        SLANG_DIAG(diagnostics);
        if (diagnostics != nullptr) {
            return false;
        }
    }

    Slang::ComPtr<slang::IEntryPoint> entryPoint;

    // FIX: Doesn't work if not compute shader
    slangModule->findEntryPointByName("computeMain", entryPoint.writeRef());

    if (!entryPoint)
        LOG_ERROR("Error getting entry point");

    std::array<slang::IComponentType*, 2> componentTypes = { slangModule, entryPoint };

    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result = session->createCompositeComponentType(componentTypes.data(),
            componentTypes.size(), composedProgram.writeRef(), diagnostics.writeRef());

        SLANG_DIAG(diagnostics);
        if (SLANG_FAILED(result)) {
            LOG_ERROR("Fail composing shader");
            return false;
        }
    }

    Slang::ComPtr<slang::IComponentType> linkedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result
            = composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef());

        SLANG_DIAG(diagnostics);
        if (SLANG_FAILED(result)) {
            LOG_ERROR("Fail linking shader");
            return false;
        }
    }

    Slang::ComPtr<slang::IBlob> spirvCode;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result
            = linkedProgram->getTargetCode(0, spirvCode.writeRef(), diagnostics.writeRef());

        SLANG_DIAG(diagnostics);
        if (SLANG_FAILED(result)) {
            LOG_ERROR("Fail getting target code");
            return false;
        }
    }

    VkShaderModuleCreateInfo moduleCI {};
    moduleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCI.pNext = nullptr;
    moduleCI.flags = 0;
    moduleCI.codeSize = spirvCode->getBufferSize();
    moduleCI.pCode = (uint32_t*)spirvCode->getBufferPointer();

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(m_VkDevice, &moduleCI, nullptr, &shaderModule),
        "Failed to create shader module");

    if (module.shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_VkDevice, module.shaderModule, nullptr);
    }
    module.shaderModule = shaderModule;
    module.slangSession = session;

    LOG_INFO("Compiled shader module: {}", module.moduleName);

    return true;
}

void ShaderManager::addDependencies(ModuleHandler& module)
{
    Slang::ComPtr<slang::IModule> slangModule;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        slangModule
            = module.slangSession->loadModule(module.moduleName.c_str(), diagnostics.writeRef());
        SLANG_DIAG(diagnostics);
    }

    LOG_DEBUG("Removing dependencies");
    const std::set<FileName> fileDependencies = module.fileDependencies;
    for (const auto& file : fileDependencies) {
        removeFileHandler(module, file);
    }

    uint32_t count = slangModule->getDependencyFileCount();
    for (uint32_t i = 0; i < count; i++) {
        const char* path = slangModule->getDependencyFilePath(i);

        addFileHandler(module, path);
    }
}

void ShaderManager::updateSessionMacros()
{
    static std::vector<slang::PreprocessorMacroDesc> preprocessor;
    preprocessor.resize(m_Macros.size());
    int count = 0;
    for (const auto& macro : m_Macros) {
        preprocessor[count++] = {
            .name = macro.first.c_str(),
            .value = macro.second.c_str(),
        };
    }
    m_SessionDesc.preprocessorMacroCount = preprocessor.size();
    m_SessionDesc.preprocessorMacros = preprocessor.data();
}

void ShaderManager::addFileHandler(ModuleHandler& module, const FileName& file)
{
    if (module.fileDependencies.contains(file)) {
        return;
    }

    if (m_FileHandlers.contains(file)) {
        module.fileDependencies.insert(file);
        m_FileHandlers[file].refCount += 1;
    } else {
        FileHandler handler = {
            .filePath = file,
            .refCount = 1,
            .dependents = {
                module.moduleName,
            },
        };
        m_FileHandlers[file] = handler;
        module.fileDependencies.insert(file);

        FileWatcher::getInstance()->addWatcher(
            file, std::bind(&ShaderManager::fileUpdated, this, std::placeholders::_1));
    }
}

void ShaderManager::removeFileHandler(ModuleHandler& module, const FileName& file)
{
    assert(module.fileDependencies.contains(file)
        && "Cannot remove file dependency that doesn't exist");
    LOG_DEBUG("Remove file handler: {} - {}", module.moduleName, file);

    module.fileDependencies.erase(file);
    m_FileHandlers[file].refCount -= 1;

    if (m_FileHandlers[file].refCount == 0) {
        FileWatcher::getInstance()->removeWatcher(file);
        m_FileHandlers.erase(file);
    }
}
