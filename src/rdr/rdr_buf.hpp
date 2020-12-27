#pragma once

#include "common.hpp"

#define NOMINMAX
#include <d3d11.h>

class cBufferBase : noncopyable {
protected:
	com_ptr<ID3D11Buffer> mpBuf;
public:
	class cMapHandle : noncopyable {
		ID3D11DeviceContext* mpCtx;
		ID3D11Buffer* mpBuf;
		D3D11_MAPPED_SUBRESOURCE mMSR;
	public:
		cMapHandle(ID3D11DeviceContext* pCtx, ID3D11Buffer* pBuf);
		~cMapHandle();
		cMapHandle(cMapHandle&& o) : mpCtx(o.mpCtx), mpBuf(o.mpBuf), mMSR(o.mMSR) {
			o.mMSR.pData = nullptr;
		}
		cMapHandle& operator=(cMapHandle&& o) {
			mpCtx = o.mpCtx;
			mpBuf = o.mpBuf;
			mMSR = o.mMSR;
			o.mMSR.pData = nullptr;
			return *this;
		}
		void* data() const { return mMSR.pData; }
		bool is_mapped() const { return !!data(); }
	};

	cBufferBase() = default;
	~cBufferBase() { deinit(); }
	cBufferBase(cBufferBase&& o) : mpBuf(std::move(o.mpBuf)) {}
	cBufferBase& operator=(cBufferBase&& o) {
		mpBuf = std::move(o.mpBuf);
		return *this;
	}

	void deinit() {
		mpBuf.reset();
	}

	cMapHandle map(ID3D11DeviceContext* pCtx);

protected:
	void init_immutable(ID3D11Device* pDev,
		void const* pData, uint32_t size,
		D3D11_BIND_FLAG bind);

	void init_write_only(ID3D11Device* pDev,
		uint32_t size, D3D11_BIND_FLAG bind);
};

