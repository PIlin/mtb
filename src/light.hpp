struct sLightPoint {
	vec4 pos;
	vec4 clr;
	bool enabled;

	template <class Archive>
	void serialize(Archive& arc);
};


class cLightMgr : noncopyable {
	sSHCoef mSH;
	sLightPoint mPointLights[4];
public:
	cLightMgr();

	void update();
	void dbg_ui();
	
	void set_default();

	bool load(const fs::path& filepath);
	bool save(const fs::path& filepath);

	template <class Archive>
	void serialize(Archive& arc);
};