//
// Created by swinston on 8/19/20.
//

#include "OpenXRUtil.h"
#include "OpenXR/CommonHelper.h"
#include "VulkanUtil.h"

namespace Util {
    namespace Renderer {
        OpenXRUtil::OpenXRUtil(VulkanUtil * _app, const XrInstanceCreateInfo& ici)
        : app(_app)
        , instanceCreateInfo(ici)
        , instance(nullptr)
        , session(nullptr)
        , view_count(0)
        , local_space(nullptr)
        , system_id(0)
        , view_config_type()
        , configuration_views(nullptr)
        , graphics_binding()
        { }

        OpenXRUtil::~OpenXRUtil() {
            for(auto swap : xrSwapChains) {
                xrDestroySwapchain(swap);
            }

            if(local_space != nullptr)
                xrDestroySpace(local_space);
            if(session != nullptr)
                xrDestroySession(session);
            if(instance != nullptr)
                xrDestroyInstance(instance);
        }

        bool OpenXRUtil::initOpenXR(bool useLegacy) {

            XrGraphicsRequirementsVulkan2KHR graphicsRequirements2{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
            XrGraphicsRequirementsVulkanKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
            PFN_xrGetVulkanGraphicsRequirements2KHR pfnXrGetVulkanGraphicsRequirements2Khr = nullptr;
            PFN_xrGetVulkanGraphicsRequirementsKHR pfnXrGetVulkanGraphicsRequirementsKhr = nullptr;
            if(!useLegacy) {
                if (XR_FAILED(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR",
                                                    reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetVulkanGraphicsRequirements2Khr)))) {
                    return false;
                }

                //NB: Vulkan enable 2 (or non legacy Vulkan enable) the concept of gathering graphics requirements is moot.
                //This is because the runtime takes responsibility for vulkan instance creation.  This is here for symmetry.
                pfnXrGetVulkanGraphicsRequirements2Khr(instance, system_id, &graphicsRequirements2);
            } else {
                if (XR_FAILED(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR",
                                                    reinterpret_cast<PFN_xrVoidFunction *>(&pfnXrGetVulkanGraphicsRequirementsKhr)))) {
                    return false;
                }

                pfnXrGetVulkanGraphicsRequirementsKhr(instance, system_id, &graphicsRequirements);
            }

//            pfnGetVulkanGraphicsRequirements2KHR(instance, systemId, graphicsRequirements);
            graphics_binding = {
                    .type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
                    .instance = app->getInstance(),
                    .physicalDevice = app->vulkanDevice->getPhysicalDevice(),
                    .device = app->vulkanDevice->getLogicalDevice(),
                    .queueFamilyIndex = app->vulkanDevice->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT),
                    .queueIndex = 0,
            };

            XrSessionCreateInfo session_create_info = {
                    .type = XR_TYPE_SESSION_CREATE_INFO,
                    .next = &graphics_binding,
                    .systemId = system_id,
            };

            if(xrCreateSession(instance, &session_create_info, &session) != XR_SUCCESS) {
                return false;
            }

            if (!initSpaces())
                return false;

            XrSessionBeginInfo sessionBeginInfo = {
                    .type = XR_TYPE_SESSION_BEGIN_INFO,
                    .primaryViewConfigurationType = view_config_type,
            };
            if(xrBeginSession(session, &sessionBeginInfo) != XR_SUCCESS) {
                return false;
            }

            return true;
        }

        bool OpenXRUtil::initSpaces() {
            uint32_t referenceSpacesCount;

            if(XR_FAILED(xrEnumerateReferenceSpaces(session, 0, &referenceSpacesCount, NULL))) {
                fprintf(stderr, "Getting number of reference spaces failed!\n");
                assert(false);
                return false;
            }

            XrReferenceSpaceType referenceSpaces[referenceSpacesCount];
            if(XR_FAILED(xrEnumerateReferenceSpaces(session, referenceSpacesCount,
                                                &referenceSpacesCount, referenceSpaces))) {
                fprintf(stderr, "Enumerating reference spaces failed!\n");
                assert(false);
                return false;
            }

            bool localSpaceSupported = false;
            printf("Enumerated %d reference spaces.", referenceSpacesCount);
            for (uint32_t i = 0; i < referenceSpacesCount; i++) {
                if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_LOCAL) {
                    localSpaceSupported = true;
                }
            }

            if (!localSpaceSupported) {
                fprintf(stderr, "XR_REFERENCE_SPACE_TYPE_LOCAL unsupported.\n");
                assert(false);
                return false;
            }

            XrPosef identity = {
                    .orientation = { .x = 0, .y = 0, .z = 0, .w = 1.0 },
                    .position = { .x = 0, .y = 0, .z = 0 },
            };

            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = {
                    .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                    .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
                    .poseInReferenceSpace = identity,
            };

            if(XR_FAILED(xrCreateReferenceSpace(session, &referenceSpaceCreateInfo,
                                            &local_space))) {
                fprintf(stderr, "Failed to create local space!\n");
                assert(false);
                return false;
            }

            return true;
        }

        bool OpenXRUtil::checkExtensions() {
            uint32_t instanceExtensionCount = 0;
            XR_CHECK_RESULT(xrEnumerateInstanceExtensionProperties(
                    nullptr, 0, &instanceExtensionCount, nullptr))

            XrExtensionProperties instanceExtensionProperties[instanceExtensionCount];
            for(auto &&props : instanceExtensionProperties)
                props.type = XR_TYPE_EXTENSION_PROPERTIES;

            XR_CHECK_RESULT(xrEnumerateInstanceExtensionProperties(nullptr, instanceExtensionCount,
                                                            &instanceExtensionCount,
                                                            instanceExtensionProperties))

            for(auto props : instanceExtensionProperties) {
                if(!strcmp(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME, props.extensionName))
                    return true;
            }
            fprintf(stderr,  "Runtime does not support required instance extension %s\n",
                   XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
            return false;
        }

        bool OpenXRUtil::createOpenXRInstance() {
            return !XR_FAILED(xrCreateInstance(&instanceCreateInfo, &instance));
        }

        bool OpenXRUtil::setupSwapChain(SwapChains & swapChains) {
            if(app->useLegacyOpenXR) {
                XrResult result;
                uint32_t swapchainFormatCount;
                if(XR_FAILED(xrEnumerateSwapchainFormats(session, 0, &swapchainFormatCount, nullptr))) {
                    fprintf(stderr, "Failed to get number of supported swapchain formats\n");
                    assert(false);
                    return false;
                }

                int64_t swapchainFormats[swapchainFormatCount];
                if(XR_FAILED(xrEnumerateSwapchainFormats(session, swapchainFormatCount,
                                                     &swapchainFormatCount, swapchainFormats))) {
                    fprintf(stderr, "failed to enumerate swapchain formats\n");
                    assert(false);
                    return false;
                }

                /* First create swapchains and query the length for each swapchain. */
                xrSwapChains.resize(view_count);
                xrSwapChainLengths.resize(view_count);
                for( auto &len : xrSwapChainLengths) {
                    len = 0;
                }

                // If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
                // there is no preferered format, so we assume VK_FORMAT_B8G8R8A8_UNORM
                if ((swapchainFormatCount == 1) && (swapchainFormats[0] == VK_FORMAT_UNDEFINED))
                {
                    swapChains.colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
                }
                else
                {
                    // iterate over the list of available surface format and
                    // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
                    bool found_B8G8R8A8_UNORM = false;
                    for (auto&& swapFormat : swapchainFormats)
                    {
                        if (swapFormat == VK_FORMAT_B8G8R8A8_UNORM)
                        {
                            swapChains.colorFormat = static_cast<VkFormat>(swapFormat);
                            found_B8G8R8A8_UNORM = true;
                            break;
                        }
                    }

                    // in case VK_FORMAT_B8G8R8A8_UNORM is not available
                    // select the first available color format
                    if (!found_B8G8R8A8_UNORM)
                    {
                        swapChains.colorFormat = static_cast<VkFormat>(swapchainFormats[0]);
                    }
                }


                for (uint32_t i = 0; i < view_count; i++) {
                    XrSwapchainCreateInfo swapchainCreateInfo = {
                            .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
                            .createFlags = 0,
                            .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                                          XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
                            // just use the first enumerated format
                            .format = swapChains.colorFormat,
                            .sampleCount = 1,
                            .width = configuration_views[i].recommendedImageRectWidth,
                            .height = configuration_views[i].recommendedImageRectHeight,
                            .faceCount = 1,
                            .arraySize = 1,
                            .mipCount = 1,
                    };

                    printf("Swapchain %d dimensions: %dx%d", i,
                              configuration_views[i].recommendedImageRectWidth,
                              configuration_views[i].recommendedImageRectHeight);
                    swapChains.setExtent({configuration_views[i].recommendedImageRectWidth, configuration_views[i].recommendedImageRectHeight});

                    if(XR_FAILED(xrCreateSwapchain(session, &swapchainCreateInfo,&xrSwapChains[i]))) {
                        fprintf(stderr, "Failed to create swapchain %d!\n", i);
                        assert(false);
                        return false;
                    }

                    if(XR_FAILED(xrEnumerateSwapchainImages(xrSwapChains[i], 0,
                                                        &xrSwapChainLengths[i], nullptr))) {
                        fprintf(stderr, "Failed to enumerate swapchains\n");
                        assert(false);
                        return false;
                    }
                }

                // most likely all swapchains have the same length, but let's not fail
                // if they are not
                uint32_t maxSwapchainLength = 0;
                for (uint32_t i = 0; i < view_count; i++) {
                    if (xrSwapChainLengths[i] > maxSwapchainLength) {
                        maxSwapchainLength = xrSwapChainLengths[i];
                    }
                }

                images.resize(view_count);

                for (uint32_t i = 0; i < view_count; i++) {
                    if(XR_FAILED(xrEnumerateSwapchainImages(
                            xrSwapChains[i], xrSwapChainLengths[i],
                            &xrSwapChainLengths[i], (XrSwapchainImageBaseHeader *) &images[i]))) {
                        fprintf(stderr, "Failed to enumerate swapchains\n");
                        assert(false);
                        return false;
                    }
                    printf("xrEnumerateSwapchainImages: swapchain_length[%d] %d", i,
                              xrSwapChainLengths[i]);
                }

                return true;
            }
            ///@todo non legacy path
            return false;
        }

        bool OpenXRUtil::createOpenXRSystem() {
            XrSystemGetInfo systemGetInfo = {
                    .type = XR_TYPE_SYSTEM_GET_INFO,
                    .formFactor = formFactor,
            };

            if(XR_FAILED(xrGetSystem(instance, &systemGetInfo, &system_id))) {
                fprintf(stderr, "failed to getSystemInfo\n");
                assert(false);
                return false;
            }

            XrSystemProperties systemProperties = {
                    .type = XR_TYPE_SYSTEM_PROPERTIES,
                    .graphicsProperties = { 0 },
                    .trackingProperties = { 0 },
            };
            if(XR_FAILED(xrGetSystemProperties(instance, system_id, &systemProperties))) {
                fprintf(stderr, "failed to get system_id\n");
                assert(false);
                return false;
            }
            return setupViews();
        }

        bool OpenXRUtil::setupViews() {
            uint32_t viewConfigurationCount;
            if(XR_FAILED(xrEnumerateViewConfigurations(instance, system_id, 0,
                                                   &viewConfigurationCount, nullptr))) {
                fprintf(stderr, "Failed to get view configuration count\n");
                assert(false);
                return false;
            }

            XrViewConfigurationType viewConfigurations[viewConfigurationCount];
            if(XR_FAILED(xrEnumerateViewConfigurations(
                    instance, system_id, viewConfigurationCount,
                    &viewConfigurationCount, viewConfigurations))) {
                fprintf(stderr, "Failed to enumerate view configurations!\n");
                assert(false);
                return false;
            }

            view_config_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            XrViewConfigurationType optionalSecondaryViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;

            /* if struct (more specifically .type) is still 0 after searching, then
             we have not found the config. This way we don't need to set a bool
             found to true. */
            XrViewConfigurationProperties requiredViewConfigProperties = { };
            XrViewConfigurationProperties secondaryViewConfigProperties = { };

            for (uint32_t i = 0; i < viewConfigurationCount; ++i) {
                XrViewConfigurationProperties properties = {
                        .type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
                };

                if(XR_FAILED(xrGetViewConfigurationProperties(
                        instance, system_id, viewConfigurations[i], &properties))) {
                    fprintf(stderr, "Failed to get view configuration info %d!", i);
                    assert(false);
                    return false;
                }

                if (viewConfigurations[i] == view_config_type &&
                    properties.viewConfigurationType == view_config_type) {
                    requiredViewConfigProperties = properties;
                } else if (viewConfigurations[i] == optionalSecondaryViewConfigType &&
                           properties.viewConfigurationType ==
                           optionalSecondaryViewConfigType) {
                    secondaryViewConfigProperties = properties;
                }
            }
            if (requiredViewConfigProperties.type !=
                XR_TYPE_VIEW_CONFIGURATION_PROPERTIES) {
                fprintf(stderr,"Couldn't get required VR View Configuration from Runtime!\n");
                assert(false);
                return false;
            }

            if(XR_FAILED(xrEnumerateViewConfigurationViews(instance, system_id,
                                                       view_config_type, 0,
                                                       &view_count, nullptr))) {
                fprintf(stderr, "Failed to get view configuration view count!\n");
                assert(false);
                return false;
            }

            configuration_views = (XrViewConfigurationView*)malloc(
                    sizeof(XrViewConfigurationView) * view_count);
            for (uint32_t i = 0; i < view_count; ++i) {
                configuration_views[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                configuration_views[i].next = nullptr;
            }

            if(XR_FAILED(xrEnumerateViewConfigurationViews(
                    instance, system_id, view_config_type, view_count,
                    &view_count, configuration_views))) {
                fprintf(stderr, "Failed to enumerate view configuration views!\n");
                assert(false);
                return false;
            }

            uint32_t secondaryViewConfigurationViewCount = 0;
            if (secondaryViewConfigProperties.type == XR_TYPE_VIEW_CONFIGURATION_PROPERTIES) {
                if(XR_FAILED(xrEnumerateViewConfigurationViews(
                        instance, system_id, optionalSecondaryViewConfigType, 0,
                        &secondaryViewConfigurationViewCount, nullptr))) {
                    fprintf(stderr, "Failed to get view configuration view count!\n");
                    assert(false);
                    return false;
                }
            }

            if (secondaryViewConfigProperties.type == XR_TYPE_VIEW_CONFIGURATION_PROPERTIES) {
                if(XR_FAILED(xrEnumerateViewConfigurationViews(
                        instance, system_id, optionalSecondaryViewConfigType,
                        secondaryViewConfigurationViewCount, &secondaryViewConfigurationViewCount,
                        configuration_views))) {
                    fprintf(stderr, "Failed to enumerate view configuration views!\n");
                    assert(false);
                    return false;
                }
            }

            return true;
        }

        std::vector<VkImage> OpenXRUtil::getImages() {
            std::vector<VkImage> ReturnMe;
            for( auto im : images ) {
                ReturnMe.push_back(im.image);
            }
            return ReturnMe;
        }

        void OpenXRUtil::beginFrame() {

            XrEventDataBuffer runtimeEvent = {
                    .type = XR_TYPE_EVENT_DATA_BUFFER,
                    .next = NULL,
            };

            frameState = { XR_TYPE_FRAME_STATE };
            XrFrameWaitInfo frameWaitInfo = { XR_TYPE_FRAME_WAIT_INFO };
            if(XR_FAILED(xrWaitFrame(session, &frameWaitInfo, &frameState))) {
                fprintf(stderr, "xrWaitframe() was not successful, exiting...\n");
                assert(false);
                return;
            }

            XrResult pollResult = xrPollEvent(instance, &runtimeEvent);
            if (pollResult == XR_SUCCESS) {
                switch (runtimeEvent.type) {
                    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                        XrEventDataSessionStateChanged* event =
                                (XrEventDataSessionStateChanged*)&runtimeEvent;
                        XrSessionState state = event->state;
                        is_visible = event->state <= XR_SESSION_STATE_FOCUSED;
                        printf("EVENT: session state changed to %d. Visible: %d", state,
                                  is_visible);
                        if (event->state >= XR_SESSION_STATE_STOPPING) {
                            is_running = false;
                        }
                        break;
                    }
                    default: break;
                }
            } else if (pollResult == XR_EVENT_UNAVAILABLE) {
                // this is the usual case
            } else {
                fprintf(stderr, "Failed to poll events!\n");
                assert(false);
                return;
            }
            if (!is_visible)
                return;

            // --- Create projection matrices and view matrices for each eye
            XrViewLocateInfo viewLocateInfo = {
                    .type = XR_TYPE_VIEW_LOCATE_INFO,
                    .viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                    .displayTime = frameState.predictedDisplayTime,
                    .space = local_space,
            };

            views = (XrView*)malloc(sizeof(XrView) * view_count);
            for (uint32_t i = 0; i < view_count; i++) {

                views[i] = (XrView){ .type = XR_TYPE_VIEW };
            };

            XrViewState viewState = {
                    .type = XR_TYPE_VIEW_STATE,
            };
            uint32_t viewCountOutput;
            if(XR_FAILED(xrLocateViews(session, &viewLocateInfo, &viewState,
                                   view_count, &viewCountOutput, views))) {
                fprintf(stderr, "Could not locate views\n");
                assert(false);
                return;
            }

            // --- Begin frame
            XrFrameBeginInfo frameBeginInfo = { XR_TYPE_FRAME_BEGIN_INFO };

            if(XR_FAILED(xrBeginFrame(session, &frameBeginInfo))) {
                assert(false);
            }
        }

        bool OpenXRUtil::endFrame() {
            XrFrameEndInfo frameEndInfo = {
                    .type = XR_TYPE_FRAME_END_INFO,
                    .displayTime = frameState.predictedDisplayTime,
                    .environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
                    .layerCount = 0,
                    .layers = nullptr,
            };

            if(XR_FAILED(xrEndFrame(session, &frameEndInfo))) {
                fprintf(stderr, "failed to end frame!\n");
                assert(false);
                return false;
            }

            free(views);
            return true;
        }
    }
}