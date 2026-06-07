// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef GENERATEDTEXTURE_CLASS_H
#define GENERATEDTEXTURE_CLASS_H

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/vector2i.hpp>

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
	// Sub-rect upload (edits): a small reused staging texture, texture_copy'd into a
	// layer of _rd_rid so an edit uploads only its changed rect, not the whole layer.
	RID _stage_rid = RID();
	Vector2i _stage_size;
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
	// R16 path: upload only p_rect of p_image into p_layer (via a staging texture +
	// texture_copy). Falls back to a whole-layer update() when there's no RD texture.
	void update_rect(const Ref<Image> &p_image, const int p_layer, const Rect2i &p_rect);
	RID create(const Ref<Image> &p_image);
	Ref<Image> get_image() const { return _image; }
	RID get_rid() const { return _rid; }
};

#endif // GENERATEDTEXTURE_CLASS_H
