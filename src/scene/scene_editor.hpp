#pragma once
#include "rdr/camera.hpp"
#include "update_queue.hpp"

#include <entt/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <map>
#include <string>

struct sSceneEditCtx {
	cCamera::sView camView;
};

struct sSceneSnapshot;
class cScene;

namespace nSceneEdit {
	struct iComponentReg {
		std::string name;
		entt::id_type id;
		int32_t order = 0;
		bool openByDefault = false;

		const char* ui_name() const { return name.c_str(); }
		virtual void ui(entt::registry& reg, entt::entity en, sSceneEditCtx& ctx) = 0;
		virtual ~iComponentReg() {}
	};

	template <typename T>
	struct sComponentReg : public iComponentReg {
		sComponentReg(const char* szName) {
			name = szName;
			id = entt::type_info<T>::id();
		}

		virtual void ui(entt::registry& reg, entt::entity en, sSceneEditCtx& ctx) override {
			if (T* p = reg.try_get<T>(en)) {
				p->dbg_ui(ctx);
			}
		}
	};

	struct iParamReg {
		std::string name;
		entt::id_type id;
		int32_t order = 0;
		bool openByDefault = false;

		const char* ui_name() const { return name.c_str(); }
		virtual void ui(entt::registry& reg, sSceneSnapshot& snapshot, entt::entity en, sSceneEditCtx& ctx) = 0;
		virtual void add(entt::registry& reg, sSceneSnapshot& snapshot, entt::entity en) = 0;
		virtual void remove(entt::registry& reg, sSceneSnapshot& snapshot, entt::entity en) = 0;
		virtual ~iParamReg() {}
	};

	template <typename T>
	struct sParamReg : public iParamReg {
		sParamReg(const char* szName) {
			name = szName;
			id = entt::type_info<T>::id();
		}

		virtual void ui(entt::registry& reg, sSceneSnapshot& snapshot, entt::entity en, sSceneEditCtx& ctx) override {
			auto pit = snapshot.params.find(id);
			if (pit != snapshot.params.end() && pit->second) {
				iParamList* pList = pit->second.get();
				auto eit = pList->entityList.find(en);
				if (eit != pList->entityList.end()) {
					auto& params = static_cast<sParamList<T>*>(pList)->paramList;
					auto epit = params.find(eit->second);
					if (epit != params.end()) {
						if (epit->second.dbg_ui(ctx)) {
							epit->second.edit_component(reg, en);
						}
					}
				}
			}
		}

		virtual void add(entt::registry& reg, sSceneSnapshot& snapshot, entt::entity en) override {
			auto pit = snapshot.params.find(id);
			if (pit == snapshot.params.end()) {
				pit = snapshot.ensure_list_iter<T>(pit, id);
				snapshot.paramsOrder.push_back(id);
			}

			if (pit != snapshot.params.end() && pit->second) {
				iParamList* pList = pit->second.get();
				auto eit = pList->entityList.find(en);
				if (eit == pList->entityList.end()) {
					auto& params = static_cast<sParamList<T>*>(pList)->paramList;
					const uint32_t paramId = (uint32_t)params.size();
					auto epit = params.emplace(paramId, typename T::init_ui()).first;
					pList->entityList.emplace(en, paramId);

					epit->second.edit_component(reg, en);
				}
			}
		}

		virtual void remove(entt::registry& reg, sSceneSnapshot& snapshot, entt::entity en) override {
			auto pit = snapshot.params.find(id);
			if (pit != snapshot.params.end() && pit->second) {
				iParamList* pList = pit->second.get();
				auto eit = pList->entityList.find(en);
				if (eit != pList->entityList.end()) {
					auto& params = static_cast<sParamList<T>*>(pList)->paramList;
					const uint32_t paramId = eit->second;
					auto epit = params.find(paramId);
					if (epit != params.end()) {
						epit->second.remove_component(reg, en);
						params.erase(epit);
					}
					pList->entityList.erase(eit);
				}
			}
		}
	};
}

class cSceneCompMetaReg {
protected:
	std::map<entt::id_type, std::unique_ptr<nSceneEdit::iComponentReg>> mComponentTypes;
	std::map<entt::id_type, std::unique_ptr<nSceneEdit::iParamReg>> mParamTypes;

public:
	template <typename T, typename TMap>
	static T& register_type(const char* szName, TMap& map) {
		std::unique_ptr<T> r = std::make_unique<T>(szName);
		entt::id_type id = r->id;
		r->order = (int32_t)map.size();
		return static_cast<T&>(*(map[id] = std::move(r)).get());
	}

	template <typename TComp>
	nSceneEdit::sComponentReg<TComp>& register_component(const char* szName) {
		return register_type<nSceneEdit::sComponentReg<TComp>>(szName, mComponentTypes);
	}

	template <typename TComp>
	nSceneEdit::sParamReg<TComp>& register_param(const char* szName) {
		return register_type<nSceneEdit::sParamReg<TComp>>(szName, mParamTypes);
	}
};


class cSceneEditor : public cSceneCompMetaReg {
	cUpdateSubscriberScope mDbgUpdate;
	cScene* mpScene = nullptr;
	entt::entity mSelected = entt::null;
public:
	cSceneEditor();
	void init(cScene* pScene);
	void dbg_ui();
	void show_entity_list(sSceneEditCtx& ctx);
	void show_entity_components(entt::entity en, sSceneEditCtx& ctx);
	void show_entity_params(entt::entity en, sSceneEditCtx& ctx);
	void gather_params_in_snapshot(const sSceneSnapshot& snapshot, entt::entity en, std::vector<entt::id_type>& paramTypes) const;
	bool select_param_type_to_add(const std::vector<entt::id_type>& paramTypes, entt::id_type& typeToAdd) const;
	void sort_components_by_order(std::vector<entt::id_type>& comp) const;
	void sort_params_by_order(std::vector<entt::id_type>& comp) const;
};