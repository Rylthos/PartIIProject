#include "shader_manager.hpp"

#include "file_watcher.hpp"

#include "logger.hpp"

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

    static std::array<slang::CompilerOptionEntry, 1> options = {
        { slang::CompilerOptionName::EmitSpirvDirectly,
         { slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr } }
    };
    m_SessionDesc.compilerOptionEntries = options.data();
    m_SessionDesc.compilerOptionEntryCount = options.size();

    static const char* searchPaths[] = { "res/shaders/" };
    m_SessionDesc.searchPaths = searchPaths;
    m_SessionDesc.searchPathCount = 1;

    FileWatcher::getInstance()->init();

    LOG_INFO("Setup slang session");
}

void ShaderManager::cleanup()
{
    for (const auto module : m_ShaderModules) {
        vkDestroyShaderModule(m_VkDevice, module.second, nullptr);
    }
    m_ShaderModules.clear();

    FileWatcher::getInstance()->stop();
}

void ShaderManager::addModule(const ModuleName& module, std::function<void()>&& createPipeline,
    std::function<void()>&& destroyPipeline)
{
    PipelineFunction function {
        .creator = createPipeline,
        .destructor = destroyPipeline,
    };

    m_FunctionMap[module].push_back(function);

    generateShaderModule(module);
    addDependencies(module);
}

VkShaderModule ShaderManager::getShaderModule(const ModuleName& module)
{
    return m_ShaderModules[module];
}

void ShaderManager::updated(const FileName& file)
{
    std::lock_guard<std::mutex> _lock(m_UpdateMutex);
    m_Updates.insert(file);
}

void ShaderManager::updateAll()
{
    if (m_Updates.empty()) {
        return;
    }

    vkDeviceWaitIdle(m_VkDevice);
    std::lock_guard<std::mutex> _lock(m_UpdateMutex);

    for (FileName file : m_Updates) {
        std::vector<ModuleName> modules = m_FileMapping[file];

        for (const ModuleName& module : modules) {
            LOG_INFO("Reloading {}", module);
            generateShaderModule(module);

            std::vector<PipelineFunction> functions = m_FunctionMap[module];
            for (const auto& function : functions) {
                function.destructor();
                function.creator();
            }
        }
    }
    m_Updates.clear();
}

void ShaderManager::generateShaderModule(const ModuleName& moduleName)
{
    Slang::ComPtr<slang::ISession> session;
    m_GlobalSession->createSession(m_SessionDesc, session.writeRef());

    Slang::ComPtr<slang::IModule> slangModule;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        slangModule = session->loadModule(moduleName.c_str(), diagnostics.writeRef());
        SLANG_DIAG(diagnostics);
        if (diagnostics != nullptr) {
            return;
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
        }
    }

    VkShaderModuleCreateInfo moduleCI {};
    moduleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCI.pNext = nullptr;
    moduleCI.flags = 0;
    moduleCI.codeSize = spirvCode->getBufferSize();
    moduleCI.pCode = (uint32_t*)spirvCode->getBufferPointer();

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(m_VkDevice, &moduleCI, nullptr, &module),
        "Failed to create shader module");

    if (m_ShaderModules.find(moduleName) != m_ShaderModules.end()) {
        vkDestroyShaderModule(m_VkDevice, m_ShaderModules[moduleName], nullptr);
    }
    m_ShaderModules[moduleName] = module;

    m_Sessions[moduleName] = session;

    LOG_INFO("Compiled shader module: {}", moduleName);
}

void ShaderManager::addDependencies(const ModuleName& module)
{
    Slang::ComPtr<slang::IModule> slangModule;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        slangModule = m_Sessions[module]->loadModule(module.c_str(), diagnostics.writeRef());
        SLANG_DIAG(diagnostics);
    }

    uint32_t count = slangModule->getDependencyFileCount();
    for (uint32_t i = 0; i < count; i++) {
        // FIX: Add file hooks for changing
        const char* path = slangModule->getDependencyFilePath(i);

        m_FileMapping[path].push_back(module);

        FileWatcher::getInstance()->addWatcher(
            path, std::bind(&ShaderManager::updated, this, std::placeholders::_1));
    }
    m_FileMapping[module].push_back(module);
}
