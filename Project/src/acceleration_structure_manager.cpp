#include "acceleration_structure_manager.hpp"
#include "accelerationStructures/accelerationStructure.hpp"
#include "accelerationStructures/grid.hpp"
#include "accelerationStructures/octree.hpp"

#include "events.hpp"
#include "glm/common.hpp"
#include "logger.hpp"

#include "imgui.h"
#include "shader_manager.hpp"

#include <cassert>
#include <format>
#include <vulkan/vulkan_core.h>

static std::map<ASType, const char*> typeToStringMap {
    { ASType::GRID,   "Grid"   },
    { ASType::OCTREE, "Octree" },
};

static std::map<RenderStyle, const char*> styleToStringMap {
    { RenderStyle::NORMAL, "Normal"  },
    { RenderStyle::HEAT,   "Heatmap" },
};

void ASManager::init(ASStructInfo initInfo)
{
    m_InitInfo = initInfo;
    setAS(m_CurrentType);
}

void ASManager::cleanup() { delete m_CurrentAS.release(); }

void ASManager::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet, VkExtent2D imageSize)
{
    assert(m_CurrentAS);

    m_CurrentAS->render(cmd, camera, drawImageSet, imageSize);
}

void ASManager::setAS(ASType type)
{
    vkDeviceWaitIdle(m_InitInfo.device);

    if (m_CurrentAS)
        delete m_CurrentAS.release();

    switch (type) {
    case ASType::GRID:
        m_CurrentAS = std::make_unique<GridAS>();
        break;
    case ASType::OCTREE:
        m_CurrentAS = std::make_unique<OctreeAS>();
        break;
    default:
        assert(false && "Invalid Type provided");
    }
    m_CurrentAS->init(m_InitInfo);
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
                    default:
                        assert(false && "Cases not handled");
                    }

                    switch (m_CurrentRenderStyle) {
                    case RenderStyle::NORMAL:
                        break;
                    case RenderStyle::HEAT:
                        ShaderManager::getInstance()->defineMacro("HEATMAP");
                        break;
                    default:
                        assert(false && "Cases not handled");
                    }
                }
            }

            {
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
    }
}
