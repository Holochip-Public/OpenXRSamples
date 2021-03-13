//
// Created by swinston on 8/19/20.
//

#include <array>
#include "OpenXRUtil.h"
#include "OpenXR/CommonHelper.h"
#include "VulkanUtil.h"
#include "OpenXR/XrMath.h"
#include "OpenXR/XRSwapChains.h"

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
        , formFactor()
        , viewConfigurationType()
        , environmentBlendMode()
        , frameState()
        , views(nullptr)
        , is_running(false)
        , is_visible(false)
        , projectionView(nullptr)
        , hand_paths()
        , layer()
        , actionSet()
        , exitAction()
        , fov(0.0f)
        { }

        OpenXRUtil::~OpenXRUtil() {
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

            if(!initActions())
                return false;

            XrSessionBeginInfo sessionBeginInfo = {
                    .type = XR_TYPE_SESSION_BEGIN_INFO,
                    .primaryViewConfigurationType = view_config_type,
            };
            return !(xrBeginSession(session, &sessionBeginInfo) != XR_SUCCESS);

        }

        void OpenXRUtil::createProjectionViews(XRSwapChains * xrSwapChains) {
            projectionView = (XrCompositionLayerProjectionView*)malloc(sizeof(XrCompositionLayerProjectionView) * view_count);

            for (uint32_t i = 0; i < view_count; i++)
                projectionView[i] = {
                        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
                        .subImage = {
                                .swapchain = xrSwapChains->swapChain[i],
                                .imageRect = {
                                        .extent = {
                                                .width = (int32_t) configuration_views[i].recommendedImageRectWidth,
                                                .height = (int32_t) configuration_views[i].recommendedImageRectHeight,
                                        },
                                },
                        },
                };

            layer = (XrCompositionLayerProjection){
                    .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
                    .layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
                    .space = local_space,
                    .viewCount = view_count,
                    .views = projectionView,
            };
        }

        bool OpenXRUtil::initSpaces() {
            uint32_t referenceSpacesCount;

            if(XR_FAILED(xrEnumerateReferenceSpaces(session, 0, &referenceSpacesCount, nullptr))) {
                fprintf(stderr, "Getting number of reference spaces failed!\n");
                return false;
            }

            std::vector<XrReferenceSpaceType> referenceSpaces(referenceSpacesCount);
            if(XR_FAILED(xrEnumerateReferenceSpaces(session, referenceSpacesCount,
                                                &referenceSpacesCount, referenceSpaces.data()))) {
                fprintf(stderr, "Enumerating reference spaces failed!\n");
                return false;
            }

            bool localSpaceSupported = false;
            printf("Enumerated %d reference spaces.\n", referenceSpacesCount);
            for (uint32_t i = 0; i < referenceSpacesCount; i++) {
                if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_LOCAL) {
                    localSpaceSupported = true;
                }
            }

            if (!localSpaceSupported) {
                fprintf(stderr, "XR_REFERENCE_SPACE_TYPE_LOCAL unsupported.\n");
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
                return false;
            }

            return true;
        }

        [[maybe_unused]] bool OpenXRUtil::checkExtensions() {
            uint32_t instanceExtensionCount = 0;
            XR_CHECK_RESULT(xrEnumerateInstanceExtensionProperties(
                    nullptr, 0, &instanceExtensionCount, nullptr))

            std::vector<XrExtensionProperties> instanceExtensionProperties(instanceExtensionCount);
            for(auto &&props : instanceExtensionProperties)
                props.type = XR_TYPE_EXTENSION_PROPERTIES;

            XR_CHECK_RESULT(xrEnumerateInstanceExtensionProperties(nullptr, instanceExtensionCount,
                                                            &instanceExtensionCount,
                                                            instanceExtensionProperties.data()))

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

        bool OpenXRUtil::createOpenXRSystem() {
            XrSystemGetInfo systemGetInfo = {
                    .type = XR_TYPE_SYSTEM_GET_INFO,
                    .formFactor = formFactor,
            };

            if(XR_FAILED(xrGetSystem(instance, &systemGetInfo, &system_id))) {
                fprintf(stderr, "failed to getSystemInfo\n");
                return false;
            }

            XrSystemProperties systemProperties = {
                    .type = XR_TYPE_SYSTEM_PROPERTIES,
                    .graphicsProperties = { 0 },
                    .trackingProperties = { 0 },
            };
            if(XR_FAILED(xrGetSystemProperties(instance, system_id, &systemProperties))) {
                fprintf(stderr, "failed to get system_id\n");
                return false;
            }
            return setupViews();
        }

        bool OpenXRUtil::setupViews() {
            uint32_t viewConfigurationCount;
            if(XR_FAILED(xrEnumerateViewConfigurations(instance, system_id, 0,
                                                   &viewConfigurationCount, nullptr))) {
                fprintf(stderr, "Failed to get view configuration count\n");
                return false;
            }

            std::vector<XrViewConfigurationType> viewConfigurations(viewConfigurationCount);
            if(XR_FAILED(xrEnumerateViewConfigurations(
                    instance, system_id, viewConfigurationCount,
                    &viewConfigurationCount, viewConfigurations.data()))) {
                fprintf(stderr, "Failed to enumerate view configurations!\n");
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
                return false;
            }

            if(XR_FAILED(xrEnumerateViewConfigurationViews(instance, system_id,
                                                       view_config_type, 0,
                                                       &view_count, nullptr))) {
                fprintf(stderr, "Failed to get view configuration view count!\n");
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
                return false;
            }

            uint32_t secondaryViewConfigurationViewCount = 0;
            if (secondaryViewConfigProperties.type == XR_TYPE_VIEW_CONFIGURATION_PROPERTIES) {
                if(XR_FAILED(xrEnumerateViewConfigurationViews(
                        instance, system_id, optionalSecondaryViewConfigType, 0,
                        &secondaryViewConfigurationViewCount, nullptr))) {
                    fprintf(stderr, "Failed to get view configuration view count!\n");
                    return false;
                }
            }

            if (secondaryViewConfigProperties.type == XR_TYPE_VIEW_CONFIGURATION_PROPERTIES) {
                if(XR_FAILED(xrEnumerateViewConfigurationViews(
                        instance, system_id, optionalSecondaryViewConfigType,
                        secondaryViewConfigurationViewCount, &secondaryViewConfigurationViewCount,
                        configuration_views))) {
                    fprintf(stderr, "Failed to enumerate view configuration views!\n");
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
                    .next = nullptr,
            };

            frameState = {XR_TYPE_FRAME_STATE};
            XrFrameWaitInfo frameWaitInfo = {XR_TYPE_FRAME_WAIT_INFO};
            if (XR_FAILED(xrWaitFrame(session, &frameWaitInfo, &frameState))) {
                fprintf(stderr, "xrWaitframe() was not successful, exiting...\n");
                return;
            }

            XrResult pollResult = xrPollEvent(instance, &runtimeEvent);
            if (pollResult == XR_SUCCESS) {
                switch (runtimeEvent.type) {
                    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                        auto *event = (XrEventDataSessionStateChanged *) &runtimeEvent;
                        XrSessionState state = event->state;
                        is_visible = event->state <= XR_SESSION_STATE_FOCUSED;
                        printf("EVENT: session state changed to %d. Visible: %d\n", state, is_visible);
                        switch (event->state) {
                            case XR_SESSION_STATE_STOPPING:
                            case XR_SESSION_STATE_EXITING:
                            case XR_SESSION_STATE_LOSS_PENDING:
                                is_running = false;
                                is_visible = false;
                                break;
                            default:
                                break;
                        }
                    }
                    default:
                        break;
                }
            } else if (pollResult == XR_EVENT_UNAVAILABLE) {
                // this is the usual case
            } else {
                fprintf(stderr, "Failed to poll events!\n");
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

            views = (XrView *) malloc(sizeof(XrView) * view_count);
            for (uint32_t i = 0; i < view_count; i++) {

                views[i] = (XrView) {.type = XR_TYPE_VIEW};
                views[i].next = nullptr;
            }

            XrViewState viewState = {
                    .type = XR_TYPE_VIEW_STATE,
            };
            uint32_t viewCountOutput;
            if (XR_FAILED(xrLocateViews(session, &viewLocateInfo, &viewState,
                                        view_count, &viewCountOutput, views))) {
                fprintf(stderr, "Could not locate views\n");
                return;
            }

            // --- Begin frame
            XrFrameBeginInfo frameBeginInfo = {XR_TYPE_FRAME_BEGIN_INFO};

            if (XR_FAILED(xrBeginFrame(session, &frameBeginInfo))) {
                assert(false);
            }

            // render each eye and fill projection_views with the result
            for (uint32_t i = 0; i < view_count; i++) {
                XrMatrix4x4f projection_matrix;
                XrMatrix4x4f_CreateProjectionFov(&projection_matrix, GRAPHICS_OPENGL, views[i].fov,
                                                 app->camera.getNearClip(), app->camera.getFarClip());

                XrMatrix4x4f view_matrix;
                XrMatrix4x4f_CreateViewMatrix(&view_matrix, &views[i].pose.position, &views[i].pose.orientation);

                app->camera.matrices.perspective = {
                        projection_matrix.m[0], projection_matrix.m[1], projection_matrix.m[2], projection_matrix.m[3],
                        projection_matrix.m[4], projection_matrix.m[5], projection_matrix.m[6], projection_matrix.m[7],
                        projection_matrix.m[8], projection_matrix.m[9], projection_matrix.m[10],
                        projection_matrix.m[11],
                        projection_matrix.m[12], projection_matrix.m[13], projection_matrix.m[14],
                        projection_matrix.m[15]
                };

                app->camera.matrices.view = {
                        view_matrix.m[0], view_matrix.m[1], view_matrix.m[2], view_matrix.m[3],
                        view_matrix.m[4], view_matrix.m[5], view_matrix.m[6], view_matrix.m[7],
                        view_matrix.m[8], view_matrix.m[9], view_matrix.m[10], view_matrix.m[11],
                        view_matrix.m[12], view_matrix.m[13], view_matrix.m[14], view_matrix.m[15]
                };
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
                return false;
            }

            free(views);
            return true;
        }

        bool OpenXRUtil::initActions() {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            std::strcpy(actionSetInfo.actionSetName, "mainactions");
            std::strcpy(actionSetInfo.localizedActionSetName, "Main Actions");
            if(XR_FAILED(xrCreateActionSet(instance, &actionSetInfo, &actionSet))) {
                fprintf(stderr, "failed to create the action set!\n");
                return false;
            }
            xrStringToPath(instance, "/user/hand/left", &hand_paths[HAND_LEFT]);
            xrStringToPath(instance, "/user/hand/right", &hand_paths[HAND_RIGHT]);

            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            std::strcpy(actionInfo.actionName, "exit_session");
            std::strcpy(actionInfo.localizedActionName, "Exit session");
            actionInfo.countSubactionPaths = static_cast<uint32_t>(HAND_COUNT);
            actionInfo.subactionPaths = hand_paths;
            if(XR_FAILED(xrCreateAction(actionSet, &actionInfo, &exitAction))) {
                fprintf(stderr, "failed to create exit action.\n");
                return false;
            }

            std::vector<XrActionSuggestedBinding> bindings;

            XrPath select_click_path[HAND_COUNT];
            xrStringToPath(instance, "/user/hand/left/input/menu/click", &select_click_path[HAND_LEFT]);
            xrStringToPath(instance, "/user/hand/right/input/menu/click", &select_click_path[HAND_RIGHT]);
            bindings.push_back({exitAction, select_click_path[HAND_LEFT]});
            bindings.push_back({exitAction, select_click_path[HAND_RIGHT]});

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            XrPath interactionPath;
            xrStringToPath(instance, "/interaction_profiles/khr/simple_controller", &interactionPath);
            suggestedBindings.interactionProfile = interactionPath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
            if(XR_FAILED(xrSuggestInteractionProfileBindings(instance, &suggestedBindings))) {
                fprintf(stderr, "couldn't create exit binding\n");
                return false;
            }

            return true;
        }


        XrPosef OpenXRUtil::getCurrentHeadPose() {
            return views->pose;
        }
    }
}