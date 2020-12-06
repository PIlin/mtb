#pragma once

#include "path_helpers.hpp"
#include <entt/fwd.hpp>
#include <map>
#include <vector>
#include <unordered_map>

struct sModelCompParams {
	fs::path modelPath;
	fs::path materialPath;

	bool create(entt::registry& reg, entt::entity en) const;

	template <class Archive>
	void serialize(Archive& arc);
};

template <typename Params>
struct sParamList
{
	using ParamId = uint32_t;
	using ParamList = std::map<ParamId, Params>;
	using EntityList = std::unordered_map<entt::entity, ParamId>;

	ParamList paramList;
	EntityList entityList;

	template <class Archive>
	void serialize(Archive& arc);
};


struct sSceneSnapshot {
	std::vector<entt::entity> entityIds;
	sParamList<sModelCompParams> modelParams;

	template <class Archive>
	void serialize(Archive& arc);

	bool load(const fs::path& filepath);
	bool save(const fs::path& filepath);
};

