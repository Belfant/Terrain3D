// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>

#include "generated_texture.h"
#include "logger.h"
#include "terrain_3d.h"

///////////////////////////
// Private Functions
///////////////////////////

// Encode a FORMAT_RF image as R16_UNORM bytes, normalized over [range.x, range.y].
// Out-of-range heights clamp (render-only; the CPU image stays the f32 truth).
static PackedByteArray _encode_rf_to_u16(const Ref<Image> &p_image, const Vector2 &p_range) {
	PackedByteArray src = p_image->get_data(); // FORMAT_RF: 4 bytes/texel, little-endian f32
	const int64_t texels = src.size() / 4;
	const float *f = (const float *)src.ptr();
	const float lo = p_range.x;
	const float inv_span = 1.f / MAX(p_range.y - p_range.x, 1e-6f);
	PackedByteArray out;
	out.resize(texels * 2);
	uint16_t *o = (uint16_t *)out.ptrw();
	for (int64_t i = 0; i < texels; i++) {
		float n = CLAMP((f[i] - lo) * inv_span, 0.f, 1.f);
		o[i] = (uint16_t)(n * 65535.f + 0.5f);
	}
	return out;
}

///////////////////////////
// Public Functions
///////////////////////////

void GeneratedTexture::clear() {
	if (_rid.is_valid()) {
		LOG(EXTREME, "GeneratedTexture freeing ", _rid);
		RS->free_rid(_rid); // the RS-facing RID (RD path: the wrapper) first
	}
	if (_rd_rid.is_valid()) {
		// The texture_rd_create wrap does not own the RD texture; free it ourselves
		// after the wrapper so nothing references a freed RD texture.
		RenderingDevice *rd = RS->get_rendering_device();
		if (rd != nullptr) {
			LOG(EXTREME, "GeneratedTexture freeing RD texture ", _rd_rid);
			rd->free_rid(_rd_rid);
		}
		_rd_rid = RID();
	}
	if (_image.is_valid()) {
		LOG(EXTREME, "GeneratedTexture unref image", _image);
		_image.unref();
	}
	_rid = RID();
	_dirty = true;
}

RID GeneratedTexture::create(const TypedArray<Image> &p_layers, const bool p_r16, const Vector2 &p_encode_range) {
	if (p_layers.is_empty()) {
		clear();
		return _rid;
	}
	if (Terrain3D::debug_level >= DEBUG) {
		LOG(EXTREME, "RenderingServer creating Texture2DArray, layers size: ", p_layers.size());
		for (int i = 0; i < p_layers.size(); i++) {
			Ref<Image> img = p_layers[i];
			LOG(EXTREME, i, ": ", img, ", empty: ", img->is_empty(), ", size: ", img->get_size(), ", format: ", img->get_format());
		}
	}
	RenderingDevice *rd = p_r16 ? RS->get_rendering_device() : nullptr;
	if (rd == nullptr) {
		// Upstream path (and the R16 fallback when no RenderingDevice exists, e.g.
		// --headless dummy or GL compatibility): RS-owned array in the images' format.
		_rid = RS->texture_2d_layered_create(p_layers, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
		_dirty = false;
		return _rid;
	}
	// R16 path: an RD-owned R16_UNORM array (2 bytes/texel vs FORMAT_RF's 4),
	// normalized over p_encode_range, wrapped into an RS texture so the material
	// binds it like any other RID. Consumer shaders must remap (sample*span+min).
	LOG(INFO, "Creating R16_UNORM height array via RenderingDevice, layers: ", p_layers.size(), ", range: ", p_encode_range);
	_encode_range = p_encode_range;
	Ref<Image> first = p_layers[0];
	Ref<RDTextureFormat> fmt;
	fmt.instantiate();
	fmt->set_format(RenderingDevice::DATA_FORMAT_R16_UNORM);
	fmt->set_width(first->get_width());
	fmt->set_height(first->get_height());
	fmt->set_depth(1);
	fmt->set_array_layers(p_layers.size());
	fmt->set_mipmaps(1); // height is texelFetched at mip 0 only
	fmt->set_texture_type(RenderingDevice::TEXTURE_TYPE_2D_ARRAY);
	fmt->set_usage_bits(
			RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
			RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT |
			RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT); // COPY_TO: future sub-rect uploads
	Ref<RDTextureView> view;
	view.instantiate();
	TypedArray<PackedByteArray> data;
	data.resize(p_layers.size());
	for (int i = 0; i < p_layers.size(); i++) {
		data[i] = _encode_rf_to_u16(p_layers[i], _encode_range);
	}
	_rd_rid = rd->texture_create(fmt, view, data);
	_rid = RS->texture_rd_create(_rd_rid, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	_dirty = false;
	return _rid;
}

void GeneratedTexture::update(const Ref<Image> &p_image, const int p_layer) {
	if (_rd_rid.is_valid()) {
		LOG(EXTREME, "RenderingDevice updating R16 Texture2DArray at index: ", p_layer);
		RenderingDevice *rd = RS->get_rendering_device(); // non-null: we created via it
		rd->texture_update(_rd_rid, p_layer, _encode_rf_to_u16(p_image, _encode_range));
		return;
	}
	LOG(EXTREME, "RenderingServer updating Texture2DArray at index: ", p_layer);
	RS->texture_2d_update(_rid, p_image, p_layer);
}

RID GeneratedTexture::create(const Ref<Image> &p_image) {
	LOG(EXTREME, "RenderingServer creating Texture2D");
	_image = p_image;
	_rid = RS->texture_2d_create(_image);
	_dirty = false;
	return _rid;
}
