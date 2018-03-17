#include <memory>
#include <string>
#include <vector>

#include "math.hpp"
#include "common.hpp"
#include "path_helpers.hpp"
#include "hou_geo.hpp"

#include "json_helpers.hpp"

using nJsonHelpers::Document;
using nJsonHelpers::Value;
using nJsonHelpers::Size;

class cLoaderImpl {
	cHouGeoLoader& mOwner;
public:
	cLoaderImpl(cHouGeoLoader& loader) : mOwner(loader) {}

	bool operator()(Value const& doc) {
		CHECK_SCHEMA(doc.IsArray(), "doc is not array\n");

		auto arrSize = doc.Size();
		for (Size i = 0; i < arrSize; i += 2) {
			cstr key;
			Value const* pVal = nullptr;
			bool gotKV = get_key_value(doc, i, key, pVal);
			CHECK_SCHEMA(gotKV, "unable to get key-value pair");

			if (cstr("fileversion").equals(key)) {
				CHECK_SCHEMA(pVal->IsString(), "version is not a string\n");
				dbg_msg("hou geo version: %s\n", pVal->GetString());
			}
			else if (cstr("pointcount").equals(key)) {
				CHECK_SCHEMA(pVal->IsNumber(), "pointcount is not a number\n");
				mOwner.mPointCount = pVal->GetInt();
			}
			else if (cstr("vertexcount").equals(key)) {
				CHECK_SCHEMA(pVal->IsNumber(), "vertexcount is not a number\n");
				mOwner.mVertexCount = pVal->GetInt();
			}
			else if (cstr("primitivecount").equals(key)) {
				CHECK_SCHEMA(pVal->IsNumber(), "primitivecount is not a number\n");
				mOwner.mPrimitiveCount = pVal->GetInt();
			}
			else if (cstr("attributes").equals(key)) {
				CHECK_SCHEMA(pVal->IsArray(), "attributes is not an array\n");
				if (!load_attribs(*pVal)) { return false; }
			}
			else if (cstr("primitivegroups").equals(key)) {
				CHECK_SCHEMA(pVal->IsArray(), "primitivegroups is not an array\n");
				if (!load_primitive_groups(*pVal)) { return false; }
			}
			else if (cstr("topology").equals(key)) {
				CHECK_SCHEMA(pVal->IsArray(), "topology is not an array\n");
				if (!load_topology(*pVal)) { return false; }
			}
			else if (cstr("primitives").equals(key)) {
				CHECK_SCHEMA(pVal->IsArray(), "primitives is not an array\n");
				if (!load_primitives(*pVal)) { return false; }
			}
			else {
				dbg_msg("hou geo: unknown key <%s>\n", key);
			}
		}

		return true;
	}

protected:

	bool load_attribs(Value const& doc) {
		auto arrSize = doc.Size();
		for (Size i = 0; i < arrSize; i += 2) {
			cstr key;
			Value const* pVal = nullptr;
			bool gotKV = get_key_value(doc, i, key, pVal);
			CHECK_SCHEMA(gotKV, "unable to get key-value pair");

			if (cstr("pointattributes").equals(key)) {
				CHECK_SCHEMA(pVal->IsArray(), "attributes is not an array\n");
				if (!load_point_attribs(*pVal)) { return false; }
			}
			else {
				dbg_msg("hou geo: unknown key <%s> in attributes array\n", key);
			}
		}
		return true;
	}

	bool load_primitive_groups(Value const& doc) {
		auto arrSize = doc.Size();

		mOwner.mpGroups = std::make_unique<cHouGeoGroup[]>(arrSize);
		mOwner.mGroupsCount = arrSize;

		for (Size i = 0; i < arrSize; ++i) {
			auto const& grp = doc[i];
			CHECK_SCHEMA(grp.IsArray(), "group is not an array\n");
			if (!load_primitive_group(grp, mOwner.mpGroups[i])) {
				dbg_msg("error loading primitive group %u\n", i);
			}
		}
		return true;
	}

	bool load_primitive_group(Value const& doc, cHouGeoGroup& group) {
		auto const& descr = doc[0u];
		CHECK_SCHEMA(descr.IsArray(), "group's description is not an array\n");

		bool nameFound = get_value_str(descr, "name", group.mName);
		CHECK_SCHEMA(nameFound, "unable to find groups's name\n");

		auto const& data = doc[1];
		CHECK_SCHEMA(descr.IsArray(), "group's data is not an array\n");

		auto const* pSel = get_value(data, "selection");
		CHECK_SCHEMA(pSel, "unable to find groups's data selection\n");

		auto const* pUnord = get_value(*pSel, "unordered");
		if (pUnord) {
			CHECK_SCHEMA(pUnord->IsArray(), "group's data  unordered is not an array\n");
			return load_primitive_group_unordered(*pUnord, group);
		}

		dbg_msg("unable to load unknown primitive group <%s>\n", group.mName.c_str());

		return false;
	}

	bool load_primitive_group_unordered(Value const& doc, cHouGeoGroup& group) {
		auto const* pRle = get_value(doc, "boolRLE");
		if (pRle) {
			auto const& rle = *pRle;
			CHECK_SCHEMA(rle.IsArray(), "boolRLE field is not an array\n");

			auto arrSize = rle.Size();

			std::vector<cHouGeoGroup::sInterval> intervals;
			intervals.reserve(arrSize / 2);

			int cur = 0;
			bool state = false;

			for (Size i = 0; i < arrSize;) {
				int n = rle[i++].GetInt();
				bool s = rle[i++].GetBool();

				if (s && s != state) {
					cHouGeoGroup::sInterval in = { cur, cur + n };
					intervals.push_back(in);
				}
				cur += n;
				state = s;
			}

			auto pIntervals = std::make_unique<cHouGeoGroup::sInterval[]>(intervals.size());
			for (size_t i = 0; i < intervals.size(); ++i) {
				pIntervals.get()[i] = intervals[i];
			}
			group.mpIntervals = std::move(pIntervals);
			group.mIntervalsCount = (int)intervals.size();

			return true;
		}

		dbg_msg("unable to load unordered primitive group with unknown encoding\n");
		return false;
	}

	bool load_point_attrib(Value const& doc, cHouGeoAttrib& attr) {
		auto const& descr = doc[0u];
		CHECK_SCHEMA(descr.IsArray(), "attribute's descr is not an array\n");
		auto const& data = doc[1];
		CHECK_SCHEMA(data.IsArray(), "attribute's data is not an array\n");

		cstr name;
		bool nameFound = get_value_str(descr, "name", name);
		CHECK_SCHEMA(nameFound, "unable to find attribute's name\n");

		cstr type;
		bool typeFound = get_value_str(descr, "type", type);
		CHECK_SCHEMA(typeFound, "unable to find attribute's type\n");

		if (!cstr("numeric").equals(type)) {
			dbg_msg("hou geo: unknown type <%s> for attribute <%s>\n", type, name);
			return false;
		}

		auto const* pValues = get_value(data, "values");
		CHECK_SCHEMA(pValues, "unable to find attribute's <%s> values\n", name);
		CHECK_SCHEMA(pValues->IsArray(), "attribute's values is not an array\n");

		auto valArrSize = pValues->Size();

		int attribSize = 0;
		cstr attribStorage;
		Value const* tuples = nullptr;

		for (Size i = 0; i < valArrSize; i += 2) {
			cstr key;
			Value const* pVal = nullptr;
			bool gotKV = get_key_value((*pValues), i, key, pVal);
			CHECK_SCHEMA(gotKV, "unable to get key-value pair");

			if (cstr("size").equals(key)) {
				CHECK_SCHEMA(pVal->IsNumber(), "size is not a number\n");
				attribSize = pVal->GetInt();
			}
			else if (cstr("storage").equals(key)) {
				CHECK_SCHEMA(pVal->IsString(), "storage is not a string\n");
				attribStorage = pVal->GetString();
			}
			else if (cstr("tuples").equals(key)) {
				CHECK_SCHEMA(pVal->IsArray(), "tuples is not an array\n");
				tuples = pVal;
			}
			else {
				dbg_msg("hou geo: unknown key <%s> in attribute's values array\n", key);
			}
		}

		if (!tuples)
			return false;
		
		int attribCount = tuples->Size();

		attr.init(name, attribStorage, attribSize, attribCount);

		auto const atype = attr.mType;
		if (atype == cHouGeoAttrib::E_TYPE_unknown) {
			dbg_msg("unknown attrib storage type <%s>\n", attribStorage);
			return false;
		}

		float* faval;
		int32_t* iaval;
		for (int i = 0; i < attribCount; ++i) {
			auto const& a = (*tuples)[i];
			CHECK_SCHEMA(a.IsArray(), "attrib value is not an array\n");
			CHECK_SCHEMA(a.Size() == attribSize, "attrib value size is wrong\n");

			switch (atype) {
			case cHouGeoAttrib::E_TYPE_fpreal32: 
				faval = attr.get_float_val(i);
				for (int j = 0; j < attribSize; ++j) {
					CHECK_SCHEMA(a[j].IsNumber(), "attrib value is not a number\n");
					faval[j] = (float)a[j].GetDouble();
				}
				break;
			case cHouGeoAttrib::E_TYPE_int32:
				iaval = attr.get_int32_val(i);
				for (int j = 0; j < attribSize; ++j) {
					CHECK_SCHEMA(a[j].IsNumber(), "attrib value is not a number\n");
					iaval[j] = a[j].GetInt();
				}
				break;
			default: break;
			}
		}

		return true;
	}

	bool load_point_attribs(Value const& doc) {
		auto arrSize = doc.Size();
		
		mOwner.mpPointAttribs = std::make_unique<cHouGeoAttrib[]>(arrSize);
		mOwner.mPointAttribCount = arrSize;

		for (Size i = 0; i < arrSize; ++i) {
			auto const& val = doc[i];
			CHECK_SCHEMA(doc.IsArray(), "attribute %d is not an array\n", i);
			load_point_attrib(val, mOwner.mpPointAttribs[i]);
		}

		return true;
	}


	bool load_topology(Value const& doc) {
		auto const* pPointref = get_value(doc, "pointref");
		CHECK_SCHEMA(pPointref, "missing pointref from topology\n");
		CHECK_SCHEMA(pPointref->IsArray(), "topology's pointref is not an array\n");

		auto const* pIndices = get_value(*pPointref, "indices");
		CHECK_SCHEMA(pIndices, "missing indices from topology's pointref\n");
		CHECK_SCHEMA(pIndices->IsArray(), "topology's indices is not an array\n");

		auto arrSize = pIndices->Size();
		CHECK_SCHEMA(arrSize == mOwner.mVertexCount, "vertex count and indices count differ\n");

		auto vmap = std::make_unique<int32_t[]>(arrSize);
		int32_t* pVmap = vmap.get();
		for (Size i = 0; i < arrSize; ++i) {
			pVmap[i] = (*pIndices)[i].GetInt();
		}

		mOwner.mpVertexMap = std::move(vmap);

		return true;
	}

	bool load_primitives(Value const& doc) {
		auto primTypes = doc.Size();

		mOwner.mPoly.reserve(mOwner.mPrimitiveCount);

		for (Size pt = 0; pt < primTypes; ++pt) {
			auto const& primType = doc[pt];
			CHECK_SCHEMA(primType.IsArray(), "prim type %u is not an array\n", pt);
			CHECK_SCHEMA(primType.Size() == 2, "wrong prim type size\n");

			load_primitive_type(primType);
		}

		CHECK_SCHEMA(mOwner.mPoly.size() == mOwner.mPrimitiveCount, "primitive number mismatch\n");

		return true;
	}


	bool load_primitive_type(Value const& doc) {
		auto const& decl = doc[0u];
		CHECK_SCHEMA(decl.IsArray(), "prim decl is not an array\n");

		auto const& data = doc[1];
		CHECK_SCHEMA(data.IsArray(), "prim data is not an array\n");
		auto dataArrSize = data.Size();

		cstr type;
		bool typeFound = get_value_str(decl, "type", type);
		CHECK_SCHEMA(typeFound, "unable to find prim type\n");

		if (!cstr("run").equals(type)) {
			dbg_msg("unknow prim type %s\n", type);
			load_primitives_invalid(dataArrSize);
			return false;
		}

		cstr runtype;
		bool runtypeFound = get_value_str(decl, "runtype", runtype);
		CHECK_SCHEMA(runtypeFound, "unable to find prim runtype\n");

		if (!cstr("Poly").equals(runtype)) {
			dbg_msg("unknow prim runtype %s\n", runtype);
			load_primitives_invalid(dataArrSize);
			return false;
		}

		auto const* pVarying = get_value(decl, "varyingfields");
		CHECK_SCHEMA(pVarying, "unable to find prim varyingfields description\n");
		CHECK_SCHEMA(pVarying->IsArray(), "prim varyingfields value is not an array\n");

		Size verVertex = 0;
		bool vertexFound = false;
		for (Size i = 0; i < pVarying->Size(); ++i) {
			auto const& val = (*pVarying)[i];
			if (val.IsString() && cstr("vertex").equals(val.GetString())) {
				verVertex = i;
				vertexFound = true;
				break;
			}
		}

		if (!vertexFound) {
			dbg_msg1("unable to find primitive vertex field position\n");
			load_primitives_invalid(dataArrSize);
			return false;
		}

		for (Size i = 0; i < dataArrSize; ++i) {
			auto const& val = data[i];
			if (val.IsArray() && val.Size() > verVertex) {
				auto const& vert = val[verVertex];
				if (vert.IsArray() && vert.Size() == 3) {
					sHouGeoPrimPoly poly;
					for (int j = 0; j < 3; ++j) {
						poly.v[j] = vert[j].GetInt();
					}
					poly.valid = true;
					mOwner.mPoly.push_back(poly);
				}
				else {
					dbg_msg("polygon with %u vertices\n", vert.Size());
					mOwner.mPoly.emplace_back();
					continue;
				}
			}
		}

		return true;
	}

	void load_primitives_invalid(Size size) {
		for (Size i = 0; i < size; ++i) {
			mOwner.mPoly.emplace_back();
		}
	}

protected:

	Size find_key(Value const& doc, cstr key) {
		auto arrSize = doc.Size();
		for (Size i = 0; i < arrSize;) {
			auto const& val = doc[i++];
			CHECK_SCHEMA(val.IsString(), "value is not key string\n");
			auto k = val.GetString();
			if (key.equals(k)) {
				return i;
			}
			else {
				i++;
			}
		}
		return -1;
	}

	Value const* get_value(Value const& doc, cstr key) {
		auto keyI = find_key(doc, key);
		if (keyI == -1) { return nullptr; }
		return &doc[keyI];
	}

	bool get_value_str(Value const& doc, cstr key, cstr& res) {
		auto val = get_value(doc, key);
		if (!val) { return false; }
		if (!val->IsString()) { return false; }
		res = val->GetString();
		return true;
	}

	bool get_value_str(Value const& doc, cstr key, std::string& res) {
		cstr str;
		if (get_value_str(doc, key, str)) {
			res = str;
			return true;
		}
		return false;
	}

	bool get_key_value(Value const& doc, Size idx, cstr& key, Value const*& pVal) {
		CHECK_SCHEMA(idx + 1 < doc.Size(), "wrong size of key-value array\n");
		auto const& k = doc[idx];
		CHECK_SCHEMA(k.IsString(), "key is not a string\n");
		key = k.GetString();
		pVal = &doc[idx + 1];
		return true;
	}
};


bool cHouGeoLoader::load(const fs::path& filepath) {
	cLoaderImpl loader(*this);
	if (!nJsonHelpers::load_file(filepath, loader)) {
		return false;
	}

	check_nonempty_groups();
	return true;
}

void cHouGeoLoader::check_nonempty_groups() {
	for (int i = 0; i < mGroupsCount; ++i) {
		auto& grp = mpGroups[i];
		if (grp.mIntervalsCount == 0) continue;

		bool nonempty = true;
		for (int j = 0; j < grp.mIntervalsCount; ++j) {
			if (grp.mpIntervals[j].size() == 0) {
				nonempty = false;
				break;
			}

			nonempty = false;
			for (int k = grp.mpIntervals[j].start; k < grp.mpIntervals[j].end; ++k) {
				if (mPoly[k].valid) {
					nonempty = true;
					break;
				}
			}
			if (nonempty) { break; }
		}
		if (nonempty) {
			grp.mEmpty = false;
			mNonemptyGroups++;
		}
	}
}


void cHouGeoAttrib::init(cstr name, cstr type, int attribSize, int count) {
	mName = name;

	mAttribSize = attribSize;
	mAttribCount = count;

	size_t elemSize = 1;

	if (type.equals("fpreal32")) { 
		mType = E_TYPE_fpreal32; 
		elemSize = sizeof(float);
	} else if (type.equals("int32")) { 
		mType = E_TYPE_int32; 
		elemSize = sizeof(int32_t);
	}

	mpData = std::make_unique<uint8_t[]>(elemSize * attribSize * count);
}
