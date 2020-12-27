#pragma once

class cSceneCompMetaReg;
struct sSceneEditCtx;

struct sPositionComp {
	DirectX::XMMATRIX wmtx;

	sPositionComp();

	sPositionComp(DirectX::FXMMATRIX wmtx);

	void dbg_ui(sSceneEditCtx& ctx);

	static void register_to_editor(cSceneCompMetaReg& metaRegistry);
};

