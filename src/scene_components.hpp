#pragma once

#include "path_helpers.hpp"
#include <entt/fwd.hpp>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>

struct sSceneEditCtx;

struct sPositionCompParams {
	sXform xform = {};

	bool create(entt::registry& reg, entt::entity en) const;

	bool dbg_ui(sSceneEditCtx& ctx);
	static sPositionCompParams init_ui();
	bool edit_component(entt::registry& reg, entt::entity en) const;

	template <class Archive>
	void serialize(Archive& arc);
};

struct sModelCompParams {
	fs::path modelPath;
	fs::path materialPath;

	bool create(entt::registry& reg, entt::entity en) const;
	
	bool dbg_ui(sSceneEditCtx& ctx);
	static sModelCompParams init_ui();
	bool edit_component(entt::registry& reg, entt::entity en) const;

	template <class Archive>
	void serialize(Archive& arc);
};

struct iParamList {
	using ParamId = uint32_t;
	using EntityList = std::unordered_map<entt::entity, ParamId>;

	EntityList entityList;

	virtual bool create(entt::registry& reg) const = 0;
};

template <typename Params>
struct sParamList : iParamList {
	using TParams = Params;
	using ParamList = std::map<ParamId, TParams>;

	ParamList paramList;
	
	virtual bool create(entt::registry& reg) const override {
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
	using TParamsMap = std::map<entt::id_type, std::unique_ptr<iParamList>>;

	std::set<entt::entity> entityIds;
	std::vector<entt::id_type> paramsOrder;
	TParamsMap params;

	template <typename TFunc>
	void invoke_for_params(TFunc&& func) {
		for (entt::id_type t : paramsOrder) {
			auto it = params.find(t);
			switch (t) {
#define CASE(T) case entt::type_info<T>::id(): func(t, ensure_list<T>(it, t));  break
				CASE(sPositionCompParams);
				CASE(sModelCompParams);
#undef CASE
			}
		}
	}

	template <typename TParam>
	TParamsMap::iterator ensure_list_iter(TParamsMap::iterator it, entt::id_type t) {
		if (it == params.end()) {
			it = params.emplace(t, std::make_unique<sParamList<TParam>>()).first;
		}
		return it;
	}

	template <typename TParam>
	sParamList<TParam>* ensure_list(TParamsMap::iterator it, entt::id_type t) {
		return static_cast<sParamList<TParam>*>(ensure_list_iter<TParam>(it, t)->second.get());
	}

	template <class Archive>
	void serialize(Archive& arc);

	bool load(const fs::path& filepath);
	bool save(const fs::path& filepath);
private:


};

