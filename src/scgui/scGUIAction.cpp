#include "stdinclude.hpp"
#include <rapidjson/error/en.h>

namespace SCGUILoop {

#define WRITE_POS_FLOAT(_str_name_, _value_, _default_) \
	if (std::abs((_value_) - (_default_)) > 1e-5) \
        value.AddMember(_str_name_, rapidjson::Value((_value_)), allocator)

	static void SerializeIdolPoseSubNode(
		bool isRoot, const Il2CppObject* gameObject, const Il2CppObject* transform,
		rapidjson::Value& value, rapidjson::Document::AllocatorType& allocator
	) {
		if (!isRoot) {
			auto name = reflection::UnityObject_get_name(gameObject)->ToUtf8String();
			value.AddMember("name", rapidjson::Value(name.c_str(), allocator), allocator);

			LocalTransform localTransform(transform, true);
			WRITE_POS_FLOAT("px", localTransform.localPosition.x, 0);
			WRITE_POS_FLOAT("py", localTransform.localPosition.y, 0);
			WRITE_POS_FLOAT("pz", localTransform.localPosition.z, 0);

			WRITE_POS_FLOAT("rx", localTransform.localRotation.x, 0);
			WRITE_POS_FLOAT("ry", localTransform.localRotation.y, 0);
			WRITE_POS_FLOAT("rz", localTransform.localRotation.z, 0);
			WRITE_POS_FLOAT("rw", localTransform.localRotation.w, 1);

			WRITE_POS_FLOAT("sx", localTransform.localScale.x, 1);
			WRITE_POS_FLOAT("sy", localTransform.localScale.y, 1);
			WRITE_POS_FLOAT("sz", localTransform.localScale.z, 1);
		}

		rapidjson::Value childNode(rapidjson::kArrayType);
		auto children = il2cpp_symbols::GetChildrenGameObjects((Il2CppObject*)gameObject);
		for (int i = 0; i < children.size(); ++i) {
			rapidjson::Value childValue(rapidjson::kObjectType);
			SerializeIdolPoseSubNode(false, children[i].first, children[i].second, childValue, allocator);
			childNode.PushBack(childValue, allocator);
		}
		value.AddMember("children", childNode, allocator);
	}

	std::string SerializeIdolPose(const Il2CppObject* gameObject) {
		rapidjson::Document doc;
		auto& value = doc.SetObject();
		auto& allocator = doc.GetAllocator();

		auto transform = reflection::GameObject_get_transform(gameObject);
		SerializeIdolPoseSubNode(true, gameObject, transform, value, allocator);

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
		std::string serialized(buffer.GetString());
		return serialized;
	}

#define READ_POS_FLOAT(_str_name_, _value_, _default_) \
    _value_ = value.HasMember(_str_name_) ? value[_str_name_].GetFloat() : (_default_)

	static void DeserializeIdolPoseSubNode(
		bool isRoot, Il2CppObject* gameObject, const Il2CppObject* transform,
		const rapidjson::Value& value, bool registerToTransformOverriding
	) {
		if (!isRoot) {
			LocalTransform localTransform(transform, false);

			READ_POS_FLOAT("px", localTransform.localPosition.x, 0);
			READ_POS_FLOAT("py", localTransform.localPosition.y, 0);
			READ_POS_FLOAT("pz", localTransform.localPosition.z, 0);
			localTransform.WriteLocalPosition((Il2CppObject*)transform);

			READ_POS_FLOAT("rx", localTransform.localRotation.x, 0);
			READ_POS_FLOAT("ry", localTransform.localRotation.y, 0);
			READ_POS_FLOAT("rz", localTransform.localRotation.z, 0);
			READ_POS_FLOAT("rw", localTransform.localRotation.w, 1);
			localTransform.WriteLocalRotation((Il2CppObject*)transform);

			READ_POS_FLOAT("sx", localTransform.localScale.x, 1);
			READ_POS_FLOAT("sy", localTransform.localScale.y, 1);
			READ_POS_FLOAT("sz", localTransform.localScale.z, 1);
			localTransform.WriteLocalScale((Il2CppObject*)transform);

			if (registerToTransformOverriding) {
				transformOverriding.emplace((Il2CppObject*)transform, std::make_unique<LocalTransform>(std::move(localTransform)));
			}
		}

		if (!value.HasMember("children") || !value["children"].IsArray()) {
			return;
		}

		const auto& childrenNodes = value["children"].GetArray();
		auto childrenObjects = il2cpp_symbols::GetChildrenGameObjects((Il2CppObject*)gameObject);
		for (const auto& childObject : childrenObjects) {
			for (const auto& childNode : childrenNodes) {
				if (!childNode.HasMember("name")) continue;
				auto name = reflection::UnityObject_get_name(childObject.first)->ToUtf8String();
				if (std::string(childNode["name"].GetString()) == name) {
					DeserializeIdolPoseSubNode(false, childObject.first, childObject.second, childNode, registerToTransformOverriding);
					break;
				}
			}
		}
	}

	void DeserializeIdolPose(const std::string& json, Il2CppObject* gameObject, bool registerToTransformOverriding) {
		rapidjson::Document doc;
		doc.Parse(json.c_str(), json.size());
		if (doc.HasParseError()) {
			std::cerr
				<< "[DeserializeIdolPose] Parse error: " << rapidjson::GetParseError_En(doc.GetParseError())
				<< "(at " << doc.GetErrorOffset() << ")"
				<< std::endl;
			return;
		}
		auto transform = reflection::GameObject_get_transform(gameObject);
		DeserializeIdolPoseSubNode(true, gameObject, transform, doc, registerToTransformOverriding);
	}

	void ClearIdolPose(Il2CppObject* gameObject) {
		il2cpp_symbols::EnumerateAllChildrenGameObjects(gameObject,
			[](auto* go, auto* tf) {
				transformOverriding.erase(tf);
				return 1;
			});
	}
}