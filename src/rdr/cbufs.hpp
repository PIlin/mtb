#pragma once

#include "math.hpp"
#include "rdr_buf.hpp"

struct sCameraCBuf {
	DirectX::XMMATRIX viewProj;
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX proj;
	DirectX::XMVECTOR camPos;
};

struct sMeshCBuf {
	DirectX::XMMATRIX wmtx;
};

struct sImguiCameraCBuf {
	DirectX::XMMATRIX proj;
};

struct sLightCBuf {
	struct Bool {
		uint32_t val;
		uint32_t pad[3];

		Bool() = default;
		Bool(bool v) : val(v) {}
		operator bool() { return !!val; }
	};
	enum { MAX_LIGHTS = 8 };

	DirectX::XMVECTOR pos[MAX_LIGHTS];
	DirectX::XMVECTOR clr[MAX_LIGHTS];
	DirectX::XMVECTOR dir[MAX_LIGHTS];
	Bool isEnabled[MAX_LIGHTS];

	DirectX::XMVECTOR sh[7];
};

struct sTestMtlCBuf {
	float fresnel[3];
	float shin;
	float nmap0Power;
	float nmap1Power;

	float pad[2];

	template <class Archive>
	void serialize(Archive& arc);
};

struct sSkinCBuf {
	enum { MAX_SKIN_MTX = 64 };
	DirectX::XMMATRIX skin[MAX_SKIN_MTX];
};

////

class cConstBufferBase : public cBufferBase {
public:
	void set_VS(ID3D11DeviceContext* pCtx, UINT slot) {
		ID3D11Buffer* bufs[1] = { mpBuf };
		pCtx->VSSetConstantBuffers(slot, 1, bufs);
	}

	void set_PS(ID3D11DeviceContext* pCtx, UINT slot) {
		ID3D11Buffer* bufs[1] = { mpBuf };
		pCtx->PSSetConstantBuffers(slot, 1, bufs);
	}
protected:
	void init(ID3D11Device* pDev, size_t size);
	void update(ID3D11DeviceContext* pCtx, void const* pData, size_t size);
};

template <typename T>
class cConstBuffer : public cConstBufferBase {
public:
	T mData;

	void init(ID3D11Device* pDev) {
		cConstBufferBase::init(pDev, sizeof(T));
	}

	void update(ID3D11DeviceContext* pCtx) {
		cConstBufferBase::update(pCtx, &mData, sizeof(T));
	}
};

template <typename T, int slot>
class cConstBufferSlotted : public cConstBuffer<T> {
public:
	void set_VS(ID3D11DeviceContext* pCtx) {
		cConstBuffer<T>::set_VS(pCtx, slot);
	}
	void set_PS(ID3D11DeviceContext* pCtx) {
		cConstBuffer<T>::set_PS(pCtx, slot);
	}
};

class cConstBufStorage {
public:
	cConstBufferSlotted<sCameraCBuf, 0> mCameraCBuf;
	cConstBufferSlotted<sMeshCBuf, 1> mMeshCBuf;
	cConstBufferSlotted<sImguiCameraCBuf, 0> mImguiCameraCBuf;
	cConstBufferSlotted<sTestMtlCBuf, 2> mTestMtlCBuf;
	cConstBufferSlotted<sLightCBuf, 3> mLightCBuf;
	cConstBufferSlotted<sSkinCBuf, 4> mSkinCBuf;

	cConstBufStorage() = default;
	cConstBufStorage(ID3D11Device* pDev) { init(pDev); }

	void init(ID3D11Device* pDev) {
		mCameraCBuf.init(pDev);
		mMeshCBuf.init(pDev);
		mImguiCameraCBuf.init(pDev);
		mTestMtlCBuf.init(pDev);
		mLightCBuf.init(pDev);
		mSkinCBuf.init(pDev);
	}

	static cConstBufStorage& get_global();
};
