class cGfx;
class cShader;
struct ImDrawData;
struct ID3D11InputLayout;

class cImgui : noncopyable {
	cShader* mpVS = nullptr;
	cShader* mpPS = nullptr;
	com_ptr<ID3D11InputLayout> mpIL;
	cTexture mFontTex;
	cVertexBuffer mVtx;
	cIndexBuffer mIdx;
	std::string mIniFilepath;
public:
	cImgui(cGfx& gfx);
	~cImgui();

	cImgui& operator=(cImgui&& o) {
		mFontTex = std::move(o.mFontTex);
		return *this;
	}

	void update();
	void disp();

	static cImgui& get();

protected:
	static void render_callback_st(ImDrawData* drawData);
	void render_callback(ImDrawData* drawData);

	void load_fonts();
};
