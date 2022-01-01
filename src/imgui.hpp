#pragma once 

#include "imgui.h"
#include "math.hpp"

namespace std::filesystem { class path; }
namespace fs = std::filesystem;

struct sCameraView;

bool ImguiSlideFloat3_1(char const* label, float v[3], float v_min, float v_max, const char* display_format = "%.3f");
bool ImguiDragFloat3_1(char const* label, float v[3], float v_speed, const char* display_format = "%.3f");
bool ImguiDragXmVector3(char const* label, DirectX::XMVECTOR& vec, float v_speed = 1.0f, const char* display_format = "%.3f");
bool ImguiDragXmVector4(char const* label, DirectX::XMVECTOR& vec, float v_speed = 1.0f, const char* display_format = "%.3f");

bool ImguiEditTransform(DirectX::XMMATRIX* matrix);
bool ImguiEditTransform(DirectX::XMFLOAT4X4* matrix);
bool ImguiEditTransform(sXform& xform);
bool ImguiGizmoEditTransform(DirectX::XMMATRIX* matrix, const sCameraView& cam, bool editTransformDecomposition);
bool ImguiGizmoEditTransform(DirectX::XMFLOAT4X4* matrix, const sCameraView& cam, bool editTransformDecomposition);
bool ImguiGizmoEditTransform(sXform& xform, const sCameraView& cam, bool editTransformDecomposition);
bool ImguiInputTextPath(const char* szLabel, fs::path& path);
