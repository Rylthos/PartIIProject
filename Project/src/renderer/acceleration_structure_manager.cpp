#include "acceleration_structure_manager.hpp"

#include <GLFW/glfw3.h>
#include <cassert>
#include <format>
#include <memory>

#include "accelerationStructures/acceleration_structure.hpp"
#include "accelerationStructures/brickmap.hpp"
#include "accelerationStructures/contree.hpp"
#include "accelerationStructures/grid.hpp"
#include "accelerationStructures/octree.hpp"
#include "accelerationStructures/texture.hpp"

#include "buffer.hpp"
#include "modification_manager.hpp"
#include "shader_manager.hpp"

#include "events/events.hpp"

#include "logger/logger.hpp"

#include "loaders/equation_loader.hpp"

#include "glm/common.hpp"

#include "imgui.h"

#include <vulkan/vulkan_core.h>

static std::map<ASType, const char*> typeToStringMap {
    { ASType::GRID,     "Grid"     },
    { ASType::OCTREE,   "Octree"   },
    { ASType::CONTREE,  "Contree"  },
    { ASType::BRICKMAP, "Brickmap" },
    { ASType::TEXTURE,  "Texture"  },
};

static std::map<RenderStyle, const char*> styleToStringMap {
    { RenderStyle::NORMAL, "Normal"  },
    { RenderStyle::HEAT,   "Heatmap" },
    { RenderStyle::CYCLES, "Cycles"  },
};

void ASManager::init(ASStructInfo initInfo)
{
    m_InitInfo = initInfo;

    m_HitDataBuffer.init(initInfo.device, initInfo.allocator, sizeof(HitData),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VMA_MEMORY_USAGE_AUTO);

    m_InitInfo.hitDataAddress = m_HitDataBuffer.getBufferAddress();

    m_MappedHitData = (HitData*)m_HitDataBuffer.mapMemory();

    setAS(m_CurrentType);
}

void ASManager::cleanup()
{
    m_HitDataBuffer.unmapMemory();
    m_HitDataBuffer.cleanup();

    delete m_CurrentAS.release();
}

void ASManager::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
{
    assert(m_CurrentAS);

    m_CurrentAS->render(cmd, camera, renderSet, imageSize);
}

void ASManager::update(float dt)
{
    assert(m_CurrentAS);

    m_CurrentAS->update(dt);

    m_PreviousPlacement += dt;

    if (m_ShouldEdit && m_PreviousPlacement > ModificationManager::getManager()->getDelay()) {
        m_PreviousPlacement = 0.f;

        glm::ivec3 index = glm::ivec3(m_MappedHitData->voxelIndex);

        if (m_CurrentModification == ModificationType::PLACE) {
            index += glm::ivec3(m_MappedHitData->normal);
        }

        auto shape = ModificationManager::getManager()->getShape();
        auto colour = ModificationManager::getManager()->getSelectedColour();

        m_CurrentAS->addMod(
            { shape.shape, m_CurrentModification, glm::uvec3(index), colour, shape.additional });
    }
}

void ASManager::setAS(ASType type)
{
    m_CurrentType = type;
    vkDeviceWaitIdle(m_InitInfo.device);

    if (m_CurrentAS)
        delete m_CurrentAS.release();

    switch (m_CurrentType) {
    case ASType::GRID:
        m_CurrentAS = std::make_unique<GridAS>();
        break;
    case ASType::TEXTURE:
        m_CurrentAS = std::make_unique<TextureAS>();
        break;
    case ASType::OCTREE:
        m_CurrentAS = std::make_unique<OctreeAS>();
        break;
    case ASType::CONTREE:
        m_CurrentAS = std::make_unique<ContreeAS>();
        break;
    case ASType::BRICKMAP:
        m_CurrentAS = std::make_unique<BrickmapAS>();
        break;
    default:
        assert(false && "Invalid Type provided");
    }
    m_CurrentAS->init(m_InitInfo);
    LOG_INFO("Changed to {}", typeToStringMap[m_CurrentType]);
}

void ASManager::loadAS(
    std::filesystem::path path, bool validStructures[static_cast<uint8_t>(ASType::MAX_TYPE)])
{
    assert(m_CurrentAS);
    if (!validStructures[static_cast<uint8_t>(m_CurrentType)]) {
        LOG_ERROR("Model is not supported for this structure");
        return;
    }
    m_CurrentAS->fromFile(path);
}

void ASManager::updateShaders()
{
    assert(m_CurrentAS);
    m_CurrentAS->updateShaders();
}

void ASManager::UI(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);

    if (frameEvent.type() == FrameEventType::UI) {
        if (ImGui::Begin("AS Manager")) {
            int currentlySelectedID = static_cast<uint8_t>(m_CurrentType);
            const char* previewValue = typeToStringMap[m_CurrentType];

            ImGui::Text("Current AS");
            ImGui::PushItemWidth(-1.f);
            if (ImGui::BeginCombo("##CurrentAS", previewValue)) {
                for (uint8_t i = 0; i < static_cast<uint8_t>(ASType::MAX_TYPE); i++) {
                    const bool isSelected = (currentlySelectedID == i);
                    ASType currentType = static_cast<ASType>(i);
                    if (ImGui::Selectable(typeToStringMap[currentType], isSelected)) {
                        m_CurrentType = currentType;
                        setAS(m_CurrentType);
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
        }
        ImGui::End();

        if (ImGui::Begin("AS Settings")) {
            bool updateShader = false;
            ImGui::PushItemWidth(-1.f);

            {
                ImGui::Text("Step limit");

                auto currentStepLimitValue = ShaderManager::getInstance()->getMacro("STEP_LIMIT");
                int stepLimit = std::atoi(currentStepLimitValue.value_or("100").c_str());
                if (!currentStepLimitValue) {
                    updateShader = true;
                    ShaderManager::getInstance()->setMacro(
                        "STEP_LIMIT", std::format("{}", stepLimit));
                }

                if (ImGui::SliderInt("##StepLimit", &stepLimit, 1, 1000)) {
                    ShaderManager::getInstance()->setMacro(
                        "STEP_LIMIT", std::format("{}", stepLimit));
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    updateShader = true;
                }
            }

            {
                ImGui::Text("Voxel size");
                ImGui::PushItemWidth(-1.);

                auto shaderValue = ShaderManager::getInstance()->getMacro("VOXEL_SIZE");
                float voxelSize = std::atof(shaderValue.value_or("1.f").c_str());
                if (!shaderValue) {
                    updateShader = true;
                    ShaderManager::getInstance()->setMacro(
                        "VOXEL_SIZE", std::format("{}", voxelSize));
                }
                if (ImGui::SliderFloat("##VoxelSize", &voxelSize, 0.05f, 2.f)) {
                    ShaderManager::getInstance()->setMacro(
                        "VOXEL_SIZE", std::format("{}", voxelSize));
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    updateShader = true;
                }

                ImGui::PopItemWidth();
            }

            {
                int currentlySelectedID = static_cast<uint8_t>(m_CurrentRenderStyle);
                const char* previewValue = styleToStringMap[m_CurrentRenderStyle];

                bool change = false;
                RenderStyle previousStyle = m_CurrentRenderStyle;

                ImGui::Text("Current render style");
                if (ImGui::BeginCombo("##CurrentRenderStyle", previewValue)) {
                    for (uint8_t i = 0; i < static_cast<uint8_t>(RenderStyle::MAX_STYLE); i++) {
                        const bool isSelected = (currentlySelectedID == i);
                        RenderStyle currentType = static_cast<RenderStyle>(i);
                        if (ImGui::Selectable(styleToStringMap[currentType], isSelected)) {
                            m_CurrentRenderStyle = currentType;
                            change = true;
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }

                if (change) {
                    updateShader = true;
                    switch (previousStyle) {
                    case RenderStyle::NORMAL:
                        break;
                    case RenderStyle::HEAT:
                        ShaderManager::getInstance()->removeMacro("HEATMAP");
                        break;
                    case RenderStyle::CYCLES:
                        ShaderManager::getInstance()->removeMacro("CYCLES");
                        break;
                    default:
                        assert(false && "Cases not handled");
                    }

                    switch (m_CurrentRenderStyle) {
                    case RenderStyle::NORMAL:
                        break;
                    case RenderStyle::HEAT:
                        ShaderManager::getInstance()->defineMacro("HEATMAP");
                        break;
                    case RenderStyle::CYCLES:
                        ShaderManager::getInstance()->defineMacro("CYCLES");
                        break;
                    default:
                        assert(false && "Cases not handled");
                    }
                }
            }

            {
                switch (m_CurrentRenderStyle) {
                case RenderStyle::NORMAL:
                    break;
                case RenderStyle::HEAT: {
                    {
                        ImGui::Text("Intersection max");

                        auto currentIntersectionValue
                            = ShaderManager::getInstance()->getMacro("INTERSECTION_MAX");
                        int currentCount
                            = std::atoi(currentIntersectionValue.value_or("100").c_str());

                        if (!currentIntersectionValue) {
                            updateShader = true;
                            ShaderManager::getInstance()->setMacro(
                                "INTERSECTION_MAX", std::format("{}", currentCount));
                        }

                        if (ImGui::SliderInt("##IntersectionMax", &currentCount, 10, 1000)) {
                            ShaderManager::getInstance()->setMacro(
                                "INTERSECTION_MAX", std::format("{}", currentCount));
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            updateShader = true;
                        }
                    }
                    break;
                }
                case RenderStyle::CYCLES: {
                    {
                        ImGui::Text("Cycles max");

                        auto currentCycleValue
                            = ShaderManager::getInstance()->getMacro("CYCLE_MAX");
                        int currentCount = std::atoi(currentCycleValue.value_or("10").c_str());

                        if (!currentCycleValue) {
                            updateShader = true;
                            ShaderManager::getInstance()->setMacro(
                                "CYCLE_MAX", std::format("{}", currentCount));
                        }

                        if (ImGui::SliderInt("##CycleMax", &currentCount, 10, 1000)) {
                            ShaderManager::getInstance()->setMacro(
                                "CYCLE_MAX", std::format("{}", currentCount));
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            updateShader = true;
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }

            ImGui::PopItemWidth();

            if (updateShader) {
                updateShaders();
            }
        }
        ImGui::End();

        if (ImGui::Begin("AS Stats")) {
            uint64_t bytes = m_CurrentAS->getMemoryUsage();
            ImGui::Text("Total Memory");
            ImGui::Text(" %lu bytes", bytes);
            ImGui::Text(" %lu KiB", bytes / 1024);
            ImGui::Text(" %lu MiB", bytes / (1024 * 1024));
            ImGui::Text(" %lu GiB", bytes / (1024 * 1024 * 1024));

            uint64_t voxels = m_CurrentAS->getTotalVoxels();
            ImGui::Text("Voxels");
            ImGui::Text(" %lu", voxels);

            uint64_t nodes = m_CurrentAS->getNodes();
            ImGui::Text("Nodes");
            ImGui::Text(" %lu", nodes);

            ImGui::Text("Bytes / Voxel");
            ImGui::Text(" %5.2f", (float)bytes / (float)voxels);
        }
        ImGui::End();

        if (ImGui::Begin("AS Generation")) {
            const char* status = m_CurrentAS->isGenerating() ? "Generating"
                : m_CurrentAS->isLoading()                   ? "Loading"
                                                             : "Idle";
            ImGui::Text("Status: %s", status);
            float time = m_CurrentAS->getGenerationTime();
            float percent = m_CurrentAS->getGenerationCompletion();
            float timeRemaining = (time / percent) - time;
            ImGui::Text("  Time       : %6.2f", time);
            ImGui::Text("  Completion : %6.5f", percent);
            ImGui::Text("  Remaining  : %6.5f", timeRemaining);
        }
        ImGui::End();

        if (ImGui::Begin("Hit Data")) {
            ImGui::Text("Hit         : %b", m_MappedHitData->hit);
            glm::vec4 hitPos = m_MappedHitData->hitPosition;
            ImGui::Text("Hit position: %5.2f %5.2f %5.2f", hitPos.x, hitPos.y, hitPos.z);
            glm::ivec4 hitIndex = m_MappedHitData->voxelIndex;
            ImGui::Text("Voxel Index : %d %d %d", hitIndex.x, hitIndex.y, hitIndex.z);
            glm::vec4 hitNormal = m_MappedHitData->normal;
            ImGui::Text("Normal      : %5.2f %5.2f %5.2f", hitNormal.x, hitNormal.y, hitNormal.z);
        }
        ImGui::End();

        if (ImGui::Begin("Lighting")) {
            bool value = (ShaderManager::getInstance()->getMacro("SHADOW_RAY").value_or(" ") == "");
            bool prevValue = value;
            ImGui::Checkbox("Shadow ray", &value);

            if (prevValue != value) {
                if (value) {
                    ShaderManager::getInstance()->defineMacro("SHADOW_RAY");
                } else {
                    ShaderManager::getInstance()->removeMacro("SHADOW_RAY");
                }
                m_CurrentAS->updateShaders();
            }
        }
        ImGui::End();
    }
}

void ASManager::mouse(const Event& event)
{
    const MouseEvent& mouseEvent = static_cast<const MouseEvent&>(event);

    if (mouseEvent.type() == MouseEventType::CLICK) {
        const MouseClickEvent& clickEvent = static_cast<const MouseClickEvent&>(event);

        if (!m_MappedHitData->hit)
            return;

        if ((clickEvent.button == GLFW_MOUSE_BUTTON_1 || clickEvent.button == GLFW_MOUSE_BUTTON_2)
            && !m_PressedButton.has_value()) {
            m_ShouldEdit = true;
            m_PressedButton = clickEvent.button;

            m_CurrentModification = (clickEvent.button == GLFW_MOUSE_BUTTON_1)
                ? ModificationType::PLACE
                : ModificationType::ERASE;
        }
    }

    if (mouseEvent.type() == MouseEventType::LIFT && m_ShouldEdit && m_PressedButton.has_value()) {
        const MouseClickEvent& clickEvent = static_cast<const MouseClickEvent&>(event);
        if (clickEvent.button == m_PressedButton.value()) {
            m_PressedButton.reset();
            m_ShouldEdit = false;
        }
    }
}
