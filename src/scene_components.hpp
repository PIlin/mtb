#pragma once

#include "path_helpers.hpp"
#include <entt/fwd.hpp>
#include <map>
#include <vector>
#include <unordered_map>

struct sPositionCompParams {
	sXform xform = {};

	bool create(entt::registry& reg, entt::entity en) const;

	template <class Archive>
	void serialize(Archive& arc);
};

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
	using TParams = Params;
	using ParamId = uint32_t;
	using ParamList = std::map<ParamId, TParams>;
	using EntityList = std::unordered_map<entt::entity, ParamId>;

	ParamList paramList;
	EntityList entityList;

	bool create( entt::registry& reg) const {
		bool res = true;
		for (const auto& [en, paramId] : entityList) {
			ParamList::const_iterator it = paramList.find(paramId);
			if (it != paramList.cend()) {
				const auto& param = it->second;
				res &= param.create(reg, en);
			}
			else { dbg_break(); }
		}
		return res;
	}

	template <class Archive>
	void serialize(Archive& arc);
};


struct sSceneSnapshot {
	std::vector<entt::entity> entityIds;
	sParamList<sPositionCompParams> posParams;
	sParamList<sModelCompParams> modelParams;

	template <class Archive>
	void serialize(Archive& arc);

	bool load(const fs::path& filepath);
	bool save(const fs::path& filepath);
};

