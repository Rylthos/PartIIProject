#include "shader_manager.hpp"

#include "logger.hpp"
#include <algorithm>

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

    slang::SessionDesc sessionDesc {};

    slang::createGlobalSession(m_GlobalSession.writeRef());

    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = m_GlobalSession->findProfile("spirv_1_5");

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    std::array<slang::CompilerOptionEntry, 1> options = {
        { slang::CompilerOptionName::EmitSpirvDirectly,
         { slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr } }
    };
    sessionDesc.compilerOptionEntries = options.data();
    sessionDesc.compilerOptionEntryCount = options.size();

    const char* searchPaths[] = { "res/shaders/" };
    sessionDesc.searchPaths = searchPaths;
    sessionDesc.searchPathCount = 1;

    m_GlobalSession->createSession(sessionDesc, m_Session.writeRef());

    LOG_INFO("Setup slang session");
}

void ShaderManager::cleanup()
{
    for (const auto module : m_ShaderModules) {
        vkDestroyShaderModule(m_VkDevice, module.second, nullptr);
    }
    m_ShaderModules.clear();
}

void ShaderManager::addShader(std::initializer_list<const char*> files,
    std::function<void()>&& createPipeline, std::function<void()>&& destroyPipeline)
{
    PipelineFunction function {
        .creator = createPipeline,
        .destructor = destroyPipeline,
    };
    size_t index = m_PipelineFunctions.size();
    m_PipelineFunctions.push_back(function);

    for (auto file : files) {
        generateShaderModule(file, index);
    }
}

VkShaderModule ShaderManager::getShaderModule(const char* filename)
{
    return m_ShaderModules[filename];
}

void ShaderManager::generateShaderModule(const char* filename, size_t pipelineIndex)
{
    Slang::ComPtr<slang::IModule> slangModule;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        slangModule = m_Session->loadModule(filename, diagnostics.writeRef());
        SLANG_DIAG(diagnostics);
    }

    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    // FIX: Doesn't work if not compute shader
    slangModule->findEntryPointByName("computeMain", entryPoint.writeRef());

    uint32_t count = slangModule->getDependencyFileCount();
    for (uint32_t i = 0; i < count; i++) {
        // FIX: Add file hooks for changing
        const char* path = slangModule->getDependencyFilePath(i);
    }

    if (!entryPoint)
        LOG_ERROR("Error getting entry point");

    std::array<slang::IComponentType*, 2> componentTypes = { slangModule, entryPoint };

    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result = m_Session->createCompositeComponentType(componentTypes.data(),
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
            LOG_ERROR("Fail composing shader");
        }
    }

    Slang::ComPtr<slang::IBlob> spirvCode;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result
            = linkedProgram->getTargetCode(0, spirvCode.writeRef(), diagnostics.writeRef());

        SLANG_DIAG(diagnostics);
        if (SLANG_FAILED(result)) {
            LOG_ERROR("Fail linking shader");
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

    m_ShaderModules[filename] = module;

    LOG_INFO("Compiled shader module: {}", filename);
}
