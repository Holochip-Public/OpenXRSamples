//
// Created by swinston on 10/19/20.
//

#ifndef LIGHTFIELDFORWARDRENDERER_GLTFMODEL_H
#define LIGHTFIELDFORWARDRENDERER_GLTFMODEL_H

#include <string>
#include <fstream>
#include <vector>

#include "../VulkanUtil.h"

#include <ktx.h>
#include <ktxvulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

namespace Util {
    namespace Renderer {
        namespace vkglTF {
            enum DescriptorBindingFlags {
                ImageBaseColor = 0x00000001,
                ImageNormalMap = 0x00000002
            };

            extern VkDescriptorSetLayout descriptorSetLayoutImage;
            extern VkDescriptorSetLayout descriptorSetLayoutUbo;
            extern VkMemoryPropertyFlags memoryPropertyFlags;
            extern uint32_t descriptorBindingFlags;

            struct Node;

            /*
                glTF texture loading class
            */
            struct Texture {
                Device *device;
                VkImage image;
                VkImageLayout imageLayout;
                VkDeviceMemory deviceMemory;
                VkImageView view;
                uint32_t width, height;
                uint32_t mipLevels;
                uint32_t layerCount;
                VkDescriptorImageInfo descriptor;
                VkSampler sampler;

                void updateDescriptor();

                void destroy() const;

                void fromglTfImage(tinygltf::Image &gltfimage, const std::string& path, Device *pDevice,
                                   VkQueue copyQueue);
            };

            /*
                glTF material class
            */
            struct Material {
                Device *device;
                enum AlphaMode {
                    ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND
                };
                AlphaMode alphaMode = ALPHAMODE_OPAQUE;
                float alphaCutoff = 1.0f;
                float metallicFactor = 1.0f;
                float roughnessFactor = 1.0f;
                glm::vec4 baseColorFactor = glm::vec4(1.0f);
                vkglTF::Texture *baseColorTexture = nullptr;
                vkglTF::Texture *metallicRoughnessTexture = nullptr;
                vkglTF::Texture *normalTexture = nullptr;
                vkglTF::Texture *occlusionTexture = nullptr;
                vkglTF::Texture *emissiveTexture = nullptr;

                vkglTF::Texture *specularGlossinessTexture;
                vkglTF::Texture *diffuseTexture;

                VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

                explicit Material(Device *_device);

                void createDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout,
                                         uint32_t descriptorBindingFlags);
            };

            /*
                glTF primitive
            */
            struct Primitive {
                uint32_t firstIndex;
                uint32_t indexCount;
                uint32_t firstVertex;
                uint32_t vertexCount;
                Material &material;

                struct Dimensions {
                    glm::vec3 min = glm::vec3(FLT_MAX);
                    glm::vec3 max = glm::vec3(-FLT_MAX);
                    glm::vec3 size;
                    glm::vec3 center;
                    float radius;
                } dimensions;

                void setDimensions(glm::vec3 min, glm::vec3 max);

                Primitive(uint32_t firstIndex, uint32_t indexCount, Material &_material);
            };

            /*
                glTF mesh
            */
            struct Mesh {
                Device *device;

                std::vector<Primitive *> primitives;
                std::string name;

                struct UniformBuffer {
                    VkBuffer buffer;
                    VkDeviceMemory memory;
                    VkDescriptorBufferInfo descriptor;
                    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
                    void *mapped;
                } uniformBuffer;

                struct UniformBlock {
                    glm::mat4 matrix;
                    glm::mat4 jointMatrix[64]{};
                    float jointcount{0};
                } uniformBlock;

                Mesh(Device *device, glm::mat4 matrix);

                ~Mesh();
            };

            /*
                glTF skin
            */
            struct Skin {
                std::string name;
                Node *skeletonRoot = nullptr;
                std::vector<glm::mat4> inverseBindMatrices;
                std::vector<Node *> joints;
            };

            /*
                glTF node
            */
            struct Node {
                Node *parent;
                uint32_t index;
                std::vector<Node *> children;
                glm::mat4 matrix;
                std::string name;
                Mesh *mesh;
                Skin *skin;
                int32_t skinIndex = -1;
                glm::vec3 translation{};
                glm::vec3 scale{1.0f};
                glm::quat rotation{};

                [[nodiscard]] glm::mat4 localMatrix() const;

                [[nodiscard]] glm::mat4 getMatrix() const;

                void update();

                ~Node();
            };

            /*
                glTF animation channel
            */
            struct AnimationChannel {
                enum PathType {
                    TRANSLATION, ROTATION, SCALE
                };
                PathType path;
                Node *node;
                uint32_t samplerIndex;
            };

            /*
                glTF animation sampler
            */
            struct AnimationSampler {
                enum InterpolationType {
                    LINEAR, STEP, CUBICSPLINE
                };
                InterpolationType interpolation;
                std::vector<float> inputs;
                std::vector<glm::vec4> outputsVec4;
            };

            /*
                glTF animation
            */
            struct Animation {
                std::string name;
                std::vector<AnimationSampler> samplers;
                std::vector<AnimationChannel> channels;
                float start = std::numeric_limits<float>::max();
                float end = std::numeric_limits<float>::min();
            };

            /*
                glTF default vertex layout with easy Vulkan mapping functions
            */
            enum class VertexComponent {
                Position, Normal, UV, Color, Tangent, Joint0, Weight0
            };

            struct Vertex {
                glm::vec3 pos;
                glm::vec3 normal;
                glm::vec2 uv;
                glm::vec4 color;
                glm::vec4 joint0;
                glm::vec4 weight0;
                glm::vec4 tangent;
                static VkVertexInputBindingDescription vertexInputBindingDescription;
                static std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
                static VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;

                static VkVertexInputBindingDescription inputBindingDescription(uint32_t binding);

                static VkVertexInputAttributeDescription
                inputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component);

                static std::vector<VkVertexInputAttributeDescription>
                inputAttributeDescriptions(uint32_t binding, const std::vector<VertexComponent>& components);

                /** @brief Returns the default pipeline vertex input state create info structure for the requested vertex components */
                static VkPipelineVertexInputStateCreateInfo *
                getPipelineVertexInputState(const std::vector<VertexComponent>& components);
            };

            enum FileLoadingFlags {
                None = 0x00000000,
                PreTransformVertices = 0x00000001,
                PreMultiplyVertexColors = 0x00000002,
                FlipY = 0x00000004,
                DontLoadImages = 0x00000008
            };

            enum RenderFlags {
                BindImages = 0x00000001,
                RenderOpaqueNodes = 0x00000002,
                RenderAlphaMaskedNodes = 0x00000004,
                RenderAlphaBlendedNodes = 0x00000008
            };

            class GLTFModel {
            private:
                vkglTF::Texture *getTexture(uint32_t index);

                vkglTF::Texture emptyTexture;

                void createEmptyTexture(VkQueue transferQueue);

            public:
                Device *device;
                VkDescriptorPool descriptorPool;

                struct Vertices {
                    int count;
                    VkBuffer buffer;
                    VkDeviceMemory memory;
                } vertices;
                struct Indices {
                    int count;
                    VkBuffer buffer;
                    VkDeviceMemory memory;
                } indices;

                std::vector<Node *> nodes;
                std::vector<Node *> linearNodes;

                std::vector<Skin *> skins;

                std::vector<Texture> textures;
                std::vector<Material> materials;
                std::vector<Animation> animations;

                struct Dimensions {
                    glm::vec3 min = glm::vec3(FLT_MAX);
                    glm::vec3 max = glm::vec3(-FLT_MAX);
                    glm::vec3 size;
                    glm::vec3 center;
                    float radius;
                } dimensions;

                bool metallicRoughnessWorkflow = true;
                bool buffersBound = false;
                std::string path;

                GLTFModel();

                ~GLTFModel();

                void loadNode(vkglTF::Node *parent, const tinygltf::Node &node, uint32_t nodeIndex,
                              const tinygltf::Model &model, std::vector<uint32_t> &indexBuffer,
                              std::vector<Vertex> &vertexBuffer, float globalscale);

                void loadSkins(tinygltf::Model &gltfModel);

                void loadImages(tinygltf::Model &gltfModel, Device *device, VkQueue transferQueue);

                void loadMaterials(tinygltf::Model &gltfModel);

                void loadAnimations(tinygltf::Model &gltfModel);

                void loadFromFile(const std::string& filename, Device *device, VkQueue transferQueue,
                                  uint32_t fileLoadingFlags = vkglTF::FileLoadingFlags::None, float scale = 1.0f);

                void bindBuffers(VkCommandBuffer commandBuffer);

                void drawNode(Node *node, VkCommandBuffer commandBuffer, uint32_t renderFlags = 0,
                              VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);

                void draw(VkCommandBuffer commandBuffer, uint32_t renderFlags = 0,
                          VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);

                void getNodeDimensions(Node *node, glm::vec3 &min, glm::vec3 &max);

                void getSceneDimensions();

                void updateAnimation(uint32_t index, float time);

                Node *findNode(Node *parent, uint32_t index);

                Node *nodeFromIndex(uint32_t index);

                void prepareNodeDescriptor(vkglTF::Node *node, VkDescriptorSetLayout descriptorSetLayout);

            };
        }
    }
}


#endif //LIGHTFIELDFORWARDRENDERER_GLTFMODEL_H
