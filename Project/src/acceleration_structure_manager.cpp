#include "acceleration_structure_manager.hpp"
#include "accelerationStructures/accelerationStructure.hpp"
#include "accelerationStructures/contree.hpp"
#include "accelerationStructures/grid.hpp"
#include "accelerationStructures/octree.hpp"

#include "events.hpp"
#include "glm/common.hpp"
#include "logger.hpp"

#include "loaders/equationLoader.hpp"

#include "imgui.h"
#include "shader_manager.hpp"

#include <cassert>
#include <format>
#include <memory>
#include <vulkan/vulkan_core.h>

static std::map<ASType, const char*> typeToStringMap {
    { ASType::GRID,    "Grid"    },
    { ASType::OCTREE,  "Octree"  },
    { ASType::CONTREE, "Contree" },
};

static std::map<RenderStyle, const char*> styleToStringMap {
    { RenderStyle::NORMAL, "Normal"  },
    { RenderStyle::HEAT,   "Heatmap" },
    { RenderStyle::CYCLES, "Cycles"  },
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

void ASManager::update(float dt)
{
    assert(m_CurrentAS);

    m_CurrentAS->update(dt);
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
    case ASType::CONTREE:
        m_CurrentAS = std::make_unique<ContreeAS>();
        break;
    default:
        assert(false && "Invalid Type provided");
    }
    m_CurrentAS->init(m_InitInfo);
    const uint32_t sideLength = 1 << 8;
    // std::unique_ptr<Loader> loader = std::make_unique<EquationLoader>(
    //     glm::uvec3(sideLength), std::function([](glm::uvec3 dimensions, glm::uvec3 index) {
    //         return Voxel { .colour = glm::vec3(index) / glm::vec3(dimensions - 1u) };
    //     }));
    std::unique_ptr<Loader> loader = std::make_unique<EquationLoader>(
        glm::uvec3(sideLength), std::function([](glm::uvec3 dimensions, glm::uvec3 index) {
            const float radius = sideLength / 2.2f;
            glm::vec3 center = glm::vec3(dimensions) / 2.f;
            const glm::vec3 position = glm::vec3(index) - center;

            if (glm::length(position) < radius) {
                glm::vec3 normal = glm::normalize(glm::abs(position - center));
                // return std::make_optional(Voxel { .colour = normal });
                return std::make_optional(Voxel { .colour = glm::vec3(1) });
            }

            return std::optional<Voxel>();
        }));

    m_CurrentAS->fromLoader(std::move(loader));
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

            uint64_t storedVoxels = m_CurrentAS->getStoredVoxels();
            ImGui::Text("Stored Voxels");
            ImGui::Text(" %lu", storedVoxels);

            ImGui::Text("Bytes / Voxel");
            ImGui::Text(" %5.2f", (float)bytes / (float)voxels);
        }
        ImGui::End();

        if (ImGui::Begin("AS Generation")) {
            ImGui::Text("Status: %s", m_CurrentAS->isGenerating() ? "Generating" : "Finished");
            float time = m_CurrentAS->getGenerationTime();
            float percent = m_CurrentAS->getGenerationCompletion();
            float timeRemaining = (time / percent) - time;
            ImGui::Text("  Time       : %6.2f", time);
            ImGui::Text("  Completion : %6.5f", percent);
            ImGui::Text("  Remaining  : %6.5f", timeRemaining);
        }
        ImGui::End();
    }
}
