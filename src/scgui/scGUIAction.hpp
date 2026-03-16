#include "stdinclude.hpp"

namespace SCGUILoop {
	std::string SerializeIdolPose(const Il2CppObject* gameObject);
	void DeserializeIdolPose(const std::string& json, Il2CppObject* gameObject, bool registerToTransformOverriding);
	void ClearIdolPose(Il2CppObject* gameObject);
}