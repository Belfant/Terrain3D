// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef GENERATEDTEXTURE_CLASS_H
#define GENERATEDTEXTURE_CLASS_H

#include <godot_cpp/classes/image.hpp>

#include "constants.h"

using namespace godot;

class GeneratedTexture {
	CLASS_NAME_STATIC("Terrain3DGenTex");

private:
	RID _rid = RID(); // RS-facing RID; what get_rid()/the material bind (RD path: the texture_rd_create wrap)
	// R16 height mode: the underlying RenderingDevice texture (R16_UNORM, normalized
	// over _encode_range). Invalid in the default RS/RF path.
	RID _rd_rid = RID();
	Vector2 _encode_range = Vector2(0.f, 1.f);
	Ref<Image> _image;
	bool _dirty = false;

public:
	void clear();
	bool is_dirty() const { return _dirty; }
	bool is_rd() const { return _rd_rid.is_valid(); }
	// p_r16 + an available RenderingDevice -> R16_UNORM array normalized over
	// p_encode_range (min, max); otherwise the upstream RS path (FORMAT as given).
	RID create(const TypedArray<Image> &p_layers, const bool p_r16 = false, const Vector2 &p_encode_range = Vector2(0.f, 1.f));
	void update(const Ref<Image> &p_image, const int p_layer);
	RID create(const Ref<Image> &p_image);
	Ref<Image> get_image() const { return _image; }
	RID get_rid() const { return _rid; }
};

#endif // GENERATEDTEXTURE_CLASS_H
