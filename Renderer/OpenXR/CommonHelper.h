//
// Created by swinston on 8/19/20.
//

#ifndef OPENXR_COMMONHELPER_H
#define OPENXR_COMMONHELPER_H


#include <string>
#include <cassert>
#include <openxr/openxr.h>

#define XR_CHECK_RESULT(f)																				\
{																										\
	XrResult res = (f);																					\
	if (res != XR_SUCCESS)																				\
	{																									\
		fprintf(stderr, "Fatal : XrResult is \"%s\" file \"%s\" line %d", tools::errorString(res).c_str(), __FILE__, __LINE__);                 \
		assert(res == XR_SUCCESS);																		\
	}																									\
}

namespace tools {
    /** @brief Disable message boxes on fatal errors */
    extern bool errorModeSilent;

    /** @brief Returns an error code as a string */
    std::string errorString(XrResult errorCode);
    std::string GetXrVersionString(XrVersion ver);
//    XrFormFactor GetXrFormFactor(const std::string& formFactorStr="Hmd") {
////        if(EqualsIgnoreCase)
//    }
}

#endif //OPENXR_COMMONHELPER_H
