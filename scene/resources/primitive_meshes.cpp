/*************************************************************************/
/*  primitive_meshes.cpp                                                 */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2017 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2017 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "primitive_meshes.h"
#include "servers/visual_server.h"

/**
  PrimitiveMesh
*/
void PrimitiveMesh::_update() const {

	Array arr;
	arr.resize(VS::ARRAY_MAX);
	_create_mesh_array(arr);

	PoolVector<Vector3> points = arr[VS::ARRAY_VERTEX];

	aabb = Rect3();

	int pc = points.size();
	ERR_FAIL_COND(pc == 0);
	{

		PoolVector<Vector3>::Read r = points.read();
		for (int i = 0; i < pc; i++) {
			if (i == 0)
				aabb.position = r[i];
			else
				aabb.expand_to(r[i]);
		}
	}

	// in with the new
	VisualServer::get_singleton()->mesh_clear(mesh);
	VisualServer::get_singleton()->mesh_add_surface_from_arrays(mesh, (VisualServer::PrimitiveType)primitive_type, arr);
	VisualServer::get_singleton()->mesh_surface_set_material(mesh, 0, material.is_null() ? RID() : material->get_rid());

	pending_request = false;

	_clear_triangle_mesh();
}

void PrimitiveMesh::_request_update() {

	if (pending_request)
		return;
	_update();
}

int PrimitiveMesh::get_surface_count() const {
	return 1;
}

int PrimitiveMesh::surface_get_array_len(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, 1, -1);
	if (pending_request) {
		_update();
	}

	return VisualServer::get_singleton()->mesh_surface_get_array_len(mesh, 0);
}

int PrimitiveMesh::surface_get_array_index_len(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, 1, -1);
	if (pending_request) {
		_update();
	}

	return VisualServer::get_singleton()->mesh_surface_get_array_index_len(mesh, 0);
}

Array PrimitiveMesh::surface_get_arrays(int p_surface) const {
	ERR_FAIL_INDEX_V(p_surface, 1, Array());
	if (pending_request) {
		_update();
	}

	return VisualServer::get_singleton()->mesh_surface_get_arrays(mesh, 0);
}

uint32_t PrimitiveMesh::surface_get_format(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, 1, 0);
	if (pending_request) {
		_update();
	}

	return VisualServer::get_singleton()->mesh_surface_get_format(mesh, 0);
}

Mesh::PrimitiveType PrimitiveMesh::surface_get_primitive_type(int p_idx) const {
	return primitive_type;
}

Ref<Material> PrimitiveMesh::surface_get_material(int p_idx) const {
	return material;
}

int PrimitiveMesh::get_blend_shape_count() const {
	return 0;
}

StringName PrimitiveMesh::get_blend_shape_name(int p_index) const {
	return StringName();
}

Rect3 PrimitiveMesh::get_aabb() const {
	if (pending_request) {
		_update();
	}

	return aabb;
}

RID PrimitiveMesh::get_rid() const {
	if (pending_request) {
		_update();
	}
	return mesh;
}

void PrimitiveMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update"), &PrimitiveMesh::_update);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &PrimitiveMesh::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &PrimitiveMesh::get_material);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material", "get_material");
}

void PrimitiveMesh::set_material(const Ref<Material> &p_material) {
	material = p_material;
	if (!pending_request) {
		// just apply it, else it'll happen when _update is called.
		VisualServer::get_singleton()->mesh_surface_set_material(mesh, 0, material.is_null() ? RID() : material->get_rid());
		_change_notify();
		emit_changed();
	};
}

Ref<Material> PrimitiveMesh::get_material() const {
	return material;
}

PrimitiveMesh::PrimitiveMesh() {
	// defaults
	mesh = VisualServer::get_singleton()->mesh_create();

	// assume primitive triangles as the type, correct for all but one and it will change this :)
	primitive_type = Mesh::PRIMITIVE_TRIANGLES;

	// make sure we do an update after we've finished constructing our object
	pending_request = true;
}

PrimitiveMesh::~PrimitiveMesh() {
	VisualServer::get_singleton()->free(mesh);
}

/**
	CapsuleMesh
*/

void CapsuleMesh::_create_mesh_array(Array &p_arr) const {
	int i, j, prevrow, thisrow, point;
	float x, y, z, u, v, w;
	float onethird = 1.0 / 3.0;
	float twothirds = 2.0 / 3.0;

	// note, this has been aligned with our collision shape but I've left the descriptions as top/middle/bottom

	PoolVector<Vector3> points;
	PoolVector<Vector3> normals;
	PoolVector<float> tangents;
	PoolVector<Vector2> uvs;
	PoolVector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	/* top hemisphere */
	thisrow = 0;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		v = j;
		w;

		v /= (rings + 1);
		w = sin(0.5 * Math_PI * v);
		z = radius * cos(0.5 * Math_PI * v);

		for (i = 0; i <= radial_segments; i++) {
			u = i;
			u /= radial_segments;

			x = sin(u * (Math_PI * 2.0));
			y = -cos(u * (Math_PI * 2.0));

			Vector3 p = Vector3(x * radius * w, y * radius * w, z);
			points.push_back(p + Vector3(0.0, 0.0, 0.5 * mid_height));
			normals.push_back(p.normalized());
			ADD_TANGENT(y, -x, 0.0, -1.0)
			uvs.push_back(Vector2(u, v * onethird));
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			};
		};

		prevrow = thisrow;
		thisrow = point;
	};

	/* cylinder */
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		v = j;
		v /= (rings + 1);

		z = mid_height * v;
		z = (mid_height * 0.5) - z;

		for (i = 0; i <= radial_segments; i++) {
			u = i;
			u /= radial_segments;

			x = sin(u * (Math_PI * 2.0));
			y = -cos(u * (Math_PI * 2.0));

			Vector3 p = Vector3(x * radius, y * radius, z);
			points.push_back(p);
			normals.push_back(Vector3(x, y, 0.0));
			ADD_TANGENT(y, -x, 0.0, -1.0)
			uvs.push_back(Vector2(u, onethird + (v * onethird)));
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			};
		};

		prevrow = thisrow;
		thisrow = point;
	};

	/* bottom hemisphere */
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		v = j;
		w;

		v /= (rings + 1);
		v += 1.0;
		w = sin(0.5 * Math_PI * v);
		z = radius * cos(0.5 * Math_PI * v);

		for (i = 0; i <= radial_segments; i++) {
			float u = i;
			u /= radial_segments;

			x = sin(u * (Math_PI * 2.0));
			y = -cos(u * (Math_PI * 2.0));

			Vector3 p = Vector3(x * radius * w, y * radius * w, z);
			points.push_back(p + Vector3(0.0, 0.0, -0.5 * mid_height));
			normals.push_back(p.normalized());
			ADD_TANGENT(y, -x, 0.0, -1.0)
			uvs.push_back(Vector2(u, twothirds + ((v - 1.0) * onethird)));
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			};
		};

		prevrow = thisrow;
		thisrow = point;
	};

	p_arr[VS::ARRAY_VERTEX] = points;
	p_arr[VS::ARRAY_NORMAL] = normals;
	p_arr[VS::ARRAY_TANGENT] = tangents;
	p_arr[VS::ARRAY_TEX_UV] = uvs;
	p_arr[VS::ARRAY_INDEX] = indices;
}

void CapsuleMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &CapsuleMesh::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &CapsuleMesh::get_radius);
	ClassDB::bind_method(D_METHOD("set_mid_height", "mid_height"), &CapsuleMesh::set_mid_height);
	ClassDB::bind_method(D_METHOD("get_mid_height"), &CapsuleMesh::get_mid_height);

	ClassDB::bind_method(D_METHOD("set_radial_segments", "segments"), &CapsuleMesh::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &CapsuleMesh::get_radial_segments);
	ClassDB::bind_method(D_METHOD("set_rings", "rings"), &CapsuleMesh::set_rings);
	ClassDB::bind_method(D_METHOD("get_rings"), &CapsuleMesh::get_rings);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "radius", PROPERTY_HINT_RANGE, "0.1,100.0,0.1"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "mid_height", PROPERTY_HINT_RANGE, "0.1,100.0,0.1"), "set_mid_height", "get_mid_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "1,100,1"), "set_radial_segments", "get_radial_segments");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rings", PROPERTY_HINT_RANGE, "1,100,1"), "set_rings", "get_rings");
}

void CapsuleMesh::set_radius(const float p_radius) {
	radius = p_radius;
	_request_update();
}

float CapsuleMesh::get_radius() const {
	return radius;
}

void CapsuleMesh::set_mid_height(const float p_mid_height) {
	mid_height = p_mid_height;
	_request_update();
}

float CapsuleMesh::get_mid_height() const {
	return mid_height;
}

void CapsuleMesh::set_radial_segments(const int p_segments) {
	radial_segments = p_segments > 4 ? p_segments : 4;
	_request_update();
}

int CapsuleMesh::get_radial_segments() const {
	return radial_segments;
}

void CapsuleMesh::set_rings(const int p_rings) {
	rings = p_rings > 1 ? p_rings : 1;
	_request_update();
}

int CapsuleMesh::get_rings() const {
	return rings;
}

CapsuleMesh::CapsuleMesh() {
	// defaults
	radius = 1.0;
	mid_height = 1.0;
	radial_segments = 64;
	rings = 8;
}

/**
  CubeMesh
*/

void CubeMesh::_create_mesh_array(Array &p_arr) const {
	int i, j, prevrow, thisrow, point;
	float x, y, z;
	float onethird = 1.0 / 3.0;
	float twothirds = 2.0 / 3.0;

	Vector3 start_pos = size * -0.5;

	// set our bounding box

	PoolVector<Vector3> points;
	PoolVector<Vector3> normals;
	PoolVector<float> tangents;
	PoolVector<Vector2> uvs;
	PoolVector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	// front + back
	y = start_pos.y;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= subdivide_h + 1; j++) {
		x = start_pos.x;
		for (i = 0; i <= subdivide_w + 1; i++) {
			float u = i;
			float v = j;
			u /= (3.0 * (subdivide_w + 1.0));
			v /= (2.0 * (subdivide_h + 1.0));

			// front
			points.push_back(Vector3(x, -y, -start_pos.z)); // double negative on the Z!
			normals.push_back(Vector3(0.0, 0.0, 1.0));
			ADD_TANGENT(-1.0, 0.0, 0.0, -1.0);
			uvs.push_back(Vector2(u, v));
			point++;

			// back
			points.push_back(Vector3(-x, -y, start_pos.z));
			normals.push_back(Vector3(0.0, 0.0, -1.0));
			ADD_TANGENT(1.0, 0.0, 0.0, -1.0);
			uvs.push_back(Vector2(twothirds + u, v));
			point++;

			if (i > 0 && j > 0) {
				int i2 = i * 2;

				// front
				indices.push_back(prevrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				// back
				indices.push_back(prevrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			};

			x += size.x / (subdivide_w + 1.0);
		};

		y += size.y / (subdivide_h + 1.0);
		prevrow = thisrow;
		thisrow = point;
	};

	// left + right
	y = start_pos.y;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_h + 1); j++) {
		z = start_pos.z;
		for (i = 0; i <= (subdivide_d + 1); i++) {
			float u = i;
			float v = j;
			u /= (3.0 * (subdivide_d + 1.0));
			v /= (2.0 * (subdivide_h + 1.0));

			// right
			points.push_back(Vector3(-start_pos.x, -y, -z));
			normals.push_back(Vector3(1.0, 0.0, 0.0));
			ADD_TANGENT(0.0, 0.0, 1.0, -1.0);
			uvs.push_back(Vector2(onethird + u, v));
			point++;

			// left
			points.push_back(Vector3(start_pos.x, -y, z));
			normals.push_back(Vector3(-1.0, 0.0, 0.0));
			ADD_TANGENT(0.0, 0.0, -1.0, -1.0);
			uvs.push_back(Vector2(u, 0.5 + v));
			point++;

			if (i > 0 && j > 0) {
				int i2 = i * 2;

				// right
				indices.push_back(prevrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				// left
				indices.push_back(prevrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			};

			z += size.z / (subdivide_d + 1.0);
		};

		y += size.y / (subdivide_h + 1.0);
		prevrow = thisrow;
		thisrow = point;
	};

	// top + bottom
	z = start_pos.z;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_d + 1); j++) {
		x = start_pos.x;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			float u = i;
			float v = j;
			u /= (3.0 * (subdivide_w + 1.0));
			v /= (2.0 * (subdivide_d + 1.0));

			// top
			points.push_back(Vector3(-x, -start_pos.y, -z));
			normals.push_back(Vector3(0.0, 1.0, 0.0));
			ADD_TANGENT(1.0, 0.0, 0.0, -1.0);
			uvs.push_back(Vector2(onethird + u, 0.5 + v));
			point++;

			// bottom
			points.push_back(Vector3(x, start_pos.y, -z));
			normals.push_back(Vector3(0.0, -1.0, 0.0));
			ADD_TANGENT(-1.0, 0.0, 0.0, -1.0);
			uvs.push_back(Vector2(twothirds + u, 0.5 + v));
			point++;

			if (i > 0 && j > 0) {
				int i2 = i * 2;

				// top
				indices.push_back(prevrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				// bottom
				indices.push_back(prevrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			};

			x += size.x / (subdivide_w + 1.0);
		};

		z += size.z / (subdivide_d + 1.0);
		prevrow = thisrow;
		thisrow = point;
	};

	p_arr[VS::ARRAY_VERTEX] = points;
	p_arr[VS::ARRAY_NORMAL] = normals;
	p_arr[VS::ARRAY_TANGENT] = tangents;
	p_arr[VS::ARRAY_TEX_UV] = uvs;
	p_arr[VS::ARRAY_INDEX] = indices;
}

void CubeMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &CubeMesh::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &CubeMesh::get_size);

	ClassDB::bind_method(D_METHOD("set_subdivide_width", "subdivide"), &CubeMesh::set_subdivide_width);
	ClassDB::bind_method(D_METHOD("get_subdivide_width"), &CubeMesh::get_subdivide_width);
	ClassDB::bind_method(D_METHOD("set_subdivide_height", "divisions"), &CubeMesh::set_subdivide_height);
	ClassDB::bind_method(D_METHOD("get_subdivide_height"), &CubeMesh::get_subdivide_height);
	ClassDB::bind_method(D_METHOD("set_subdivide_depth", "divisions"), &CubeMesh::set_subdivide_depth);
	ClassDB::bind_method(D_METHOD("get_subdivide_depth"), &CubeMesh::get_subdivide_depth);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_width", PROPERTY_HINT_RANGE, "0,100,1"), "set_subdivide_width", "get_subdivide_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_height", PROPERTY_HINT_RANGE, "0,100,1"), "set_subdivide_height", "get_subdivide_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_depth", PROPERTY_HINT_RANGE, "0,100,1"), "set_subdivide_depth", "get_subdivide_depth");
}

void CubeMesh::set_size(const Vector3 &p_size) {
	size = p_size;
	_request_update();
}

Vector3 CubeMesh::get_size() const {
	return size;
}

void CubeMesh::set_subdivide_width(const int p_divisions) {
	subdivide_w = p_divisions > 0 ? p_divisions : 0;
	_request_update();
}

int CubeMesh::get_subdivide_width() const {
	return subdivide_w;
}

void CubeMesh::set_subdivide_height(const int p_divisions) {
	subdivide_h = p_divisions > 0 ? p_divisions : 0;
	_request_update();
}

int CubeMesh::get_subdivide_height() const {
	return subdivide_h;
}

void CubeMesh::set_subdivide_depth(const int p_divisions) {
	subdivide_d = p_divisions > 0 ? p_divisions : 0;
	_request_update();
}

int CubeMesh::get_subdivide_depth() const {
	return subdivide_d;
}

CubeMesh::CubeMesh() {
	// defaults
	size = Vector3(2.0, 2.0, 2.0);
	subdivide_w = 0;
	subdivide_h = 0;
	subdivide_d = 0;
}

/**
  CylinderMesh
*/

void CylinderMesh::_create_mesh_array(Array &p_arr) const {
	int i, j, prevrow, thisrow, point;
	float x, y, z, u, v, radius;

	radius = bottom_radius > top_radius ? bottom_radius : top_radius;

	PoolVector<Vector3> points;
	PoolVector<Vector3> normals;
	PoolVector<float> tangents;
	PoolVector<Vector2> uvs;
	PoolVector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	thisrow = 0;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		v = j;
		v /= (rings + 1);

		radius = top_radius + ((bottom_radius - top_radius) * v);

		y = height * v;
		y = (height * 0.5) - y;

		for (i = 0; i <= radial_segments; i++) {
			u = i;
			u /= radial_segments;

			x = sin(u * (Math_PI * 2.0));
			z = cos(u * (Math_PI * 2.0));

			Vector3 p = Vector3(x * radius, y, z * radius);
			points.push_back(p);
			normals.push_back(Vector3(x, 0.0, z));
			ADD_TANGENT(-z, 0.0, x, -1.0)
			uvs.push_back(Vector2(u, v * 0.5));
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			};
		};

		prevrow = thisrow;
		thisrow = point;
	};

	// add top
	if (top_radius > 0.0) {
		y = height * 0.5;

		thisrow = point;
		points.push_back(Vector3(0.0, y, 0.0));
		normals.push_back(Vector3(0.0, 1.0, 0.0));
		ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
		uvs.push_back(Vector2(0.25, 0.75));
		point++;

		for (i = 0; i <= radial_segments; i++) {
			float r = i;
			r /= radial_segments;

			x = sin(r * (Math_PI * 2.0));
			z = cos(r * (Math_PI * 2.0));

			u = ((x + 1.0) * 0.25);
			v = 0.5 + ((z + 1.0) * 0.25);

			Vector3 p = Vector3(x * top_radius, y, z * top_radius);
			points.push_back(p);
			normals.push_back(Vector3(0.0, 1.0, 0.0));
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
			uvs.push_back(Vector2(u, v));
			point++;

			if (i > 0) {
				indices.push_back(thisrow);
				indices.push_back(point - 1);
				indices.push_back(point - 2);
			};
		};
	};

	// add bottom
	if (bottom_radius > 0.0) {
		y = height * -0.5;

		thisrow = point;
		points.push_back(Vector3(0.0, y, 0.0));
		normals.push_back(Vector3(0.0, -1.0, 0.0));
		ADD_TANGENT(-1.0, 0.0, 0.0, -1.0)
		uvs.push_back(Vector2(0.75, 0.75));
		point++;

		for (i = 0; i <= radial_segments; i++) {
			float r = i;
			r /= radial_segments;

			x = sin(r * (Math_PI * 2.0));
			z = cos(r * (Math_PI * 2.0));

			u = 0.5 + ((x + 1.0) * 0.25);
			v = 1.0 - ((z + 1.0) * 0.25);

			Vector3 p = Vector3(x * bottom_radius, y, z * bottom_radius);
			points.push_back(p);
			normals.push_back(Vector3(0.0, -1.0, 0.0));
			ADD_TANGENT(-1.0, 0.0, 0.0, -1.0)
			uvs.push_back(Vector2(u, v));
			point++;

			if (i > 0) {
				indices.push_back(thisrow);
				indices.push_back(point - 2);
				indices.push_back(point - 1);
			};
		};
	};

	p_arr[VS::ARRAY_VERTEX] = points;
	p_arr[VS::ARRAY_NORMAL] = normals;
	p_arr[VS::ARRAY_TANGENT] = tangents;
	p_arr[VS::ARRAY_TEX_UV] = uvs;
	p_arr[VS::ARRAY_INDEX] = indices;
}

void CylinderMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_top_radius", "radius"), &CylinderMesh::set_top_radius);
	ClassDB::bind_method(D_METHOD("get_top_radius"), &CylinderMesh::get_top_radius);
	ClassDB::bind_method(D_METHOD("set_bottom_radius", "radius"), &CylinderMesh::set_bottom_radius);
	ClassDB::bind_method(D_METHOD("get_bottom_radius"), &CylinderMesh::get_bottom_radius);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &CylinderMesh::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &CylinderMesh::get_height);

	ClassDB::bind_method(D_METHOD("set_radial_segments", "segments"), &CylinderMesh::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &CylinderMesh::get_radial_segments);
	ClassDB::bind_method(D_METHOD("set_rings", "rings"), &CylinderMesh::set_rings);
	ClassDB::bind_method(D_METHOD("get_rings"), &CylinderMesh::get_rings);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "top_radius", PROPERTY_HINT_RANGE, "0.1,100.0,0.1"), "set_top_radius", "get_top_radius");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "bottom_radius", PROPERTY_HINT_RANGE, "0.1,100.0,0.1"), "set_bottom_radius", "get_bottom_radius");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height", PROPERTY_HINT_RANGE, "0.1,100.0,0.1"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "1,100,1"), "set_radial_segments", "get_radial_segments");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rings", PROPERTY_HINT_RANGE, "1,100,1"), "set_rings", "get_rings");
}

void CylinderMesh::set_top_radius(const float p_radius) {
	top_radius = p_radius;
	_request_update();
}

float CylinderMesh::get_top_radius() const {
	return top_radius;
}

void CylinderMesh::set_bottom_radius(const float p_radius) {
	bottom_radius = p_radius;
	_request_update();
}

float CylinderMesh::get_bottom_radius() const {
	return bottom_radius;
}

void CylinderMesh::set_height(const float p_height) {
	height = p_height;
	_request_update();
}

float CylinderMesh::get_height() const {
	return height;
}

void CylinderMesh::set_radial_segments(const int p_segments) {
	radial_segments = p_segments > 4 ? p_segments : 4;
	_request_update();
}

int CylinderMesh::get_radial_segments() const {
	return radial_segments;
}

void CylinderMesh::set_rings(const int p_rings) {
	rings = p_rings > 0 ? p_rings : 0;
	_request_update();
}

int CylinderMesh::get_rings() const {
	return rings;
}

CylinderMesh::CylinderMesh() {
	// defaults
	top_radius = 1.0;
	bottom_radius = 1.0;
	height = 2.0;
	radial_segments = 64;
	rings = 4;
}

/**
  PlaneMesh
*/

void PlaneMesh::_create_mesh_array(Array &p_arr) const {
	int i, j, prevrow, thisrow, point;
	float x, z;

	Size2 start_pos = size * -0.5;

	PoolVector<Vector3> points;
	PoolVector<Vector3> normals;
	PoolVector<float> tangents;
	PoolVector<Vector2> uvs;
	PoolVector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	/* top + bottom */
	z = start_pos.y;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_d + 1); j++) {
		x = start_pos.x;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			float u = i;
			float v = j;
			u /= (subdivide_w + 1.0);
			v /= (subdivide_d + 1.0);

			points.push_back(Vector3(-x, 0.0, -z));
			normals.push_back(Vector3(0.0, 1.0, 0.0));
			ADD_TANGENT(1.0, 0.0, 0.0, -1.0);
			uvs.push_back(Vector2(u, v));
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			};

			x += size.x / (subdivide_w + 1.0);
		};

		z += size.y / (subdivide_d + 1.0);
		prevrow = thisrow;
		thisrow = point;
	};

	p_arr[VS::ARRAY_VERTEX] = points;
	p_arr[VS::ARRAY_NORMAL] = normals;
	p_arr[VS::ARRAY_TANGENT] = tangents;
	p_arr[VS::ARRAY_TEX_UV] = uvs;
	p_arr[VS::ARRAY_INDEX] = indices;
}

void PlaneMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &PlaneMesh::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &PlaneMesh::get_size);

	ClassDB::bind_method(D_METHOD("set_subdivide_width", "subdivide"), &PlaneMesh::set_subdivide_width);
	ClassDB::bind_method(D_METHOD("get_subdivide_width"), &PlaneMesh::get_subdivide_width);
	ClassDB::bind_method(D_METHOD("set_subdivide_depth", "subdivide"), &PlaneMesh::set_subdivide_depth);
	ClassDB::bind_method(D_METHOD("get_subdivide_depth"), &PlaneMesh::get_subdivide_depth);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_width", PROPERTY_HINT_RANGE, "0,100,1"), "set_subdivide_width", "get_subdivide_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_depth", PROPERTY_HINT_RANGE, "0,100,1"), "set_subdivide_depth", "get_subdivide_depth");
}

void PlaneMesh::set_size(const Size2 &p_size) {
	size = p_size;
	_request_update();
}

Size2 PlaneMesh::get_size() const {
	return size;
}

void PlaneMesh::set_subdivide_width(const int p_divisions) {
	subdivide_w = p_divisions > 0 ? p_divisions : 0;
	_request_update();
}

int PlaneMesh::get_subdivide_width() const {
	return subdivide_w;
}

void PlaneMesh::set_subdivide_depth(const int p_divisions) {
	subdivide_d = p_divisions > 0 ? p_divisions : 0;
	_request_update();
}

int PlaneMesh::get_subdivide_depth() const {
	return subdivide_d;
}

PlaneMesh::PlaneMesh() {
	// defaults
	size = Size2(2.0, 2.0);
	subdivide_w = 0;
	subdivide_d = 0;
}

/**
  PrismMesh
*/

void PrismMesh::_create_mesh_array(Array &p_arr) const {
	int i, j, prevrow, thisrow, point;
	float x, y, z;
	float onethird = 1.0 / 3.0;
	float twothirds = 2.0 / 3.0;

	Vector3 start_pos = size * -0.5;

	// set our bounding box

	PoolVector<Vector3> points;
	PoolVector<Vector3> normals;
	PoolVector<float> tangents;
	PoolVector<Vector2> uvs;
	PoolVector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	/* front + back */
	y = start_pos.y;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_h + 1); j++) {
		float scale = (y - start_pos.y) / size.y;
		float scaled_size_x = size.x * scale;
		float start_x = start_pos.x + (1.0 - scale) * size.x * left_to_right;
		float offset_front = (1.0 - scale) * onethird * left_to_right;
		float offset_back = (1.0 - scale) * onethird * (1.0 - left_to_right);

		x = 0.0;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			float u = i;
			float v = j;
			u /= (3.0 * (subdivide_w + 1.0));
			v /= (2.0 * (subdivide_h + 1.0));

			u *= scale;

			/* front */
			points.push_back(Vector3(start_x + x, -y, -start_pos.z)); // double negative on the Z!
			normals.push_back(Vector3(0.0, 0.0, 1.0));
			ADD_TANGENT(-1.0, 0.0, 0.0, -1.0);
			uvs.push_back(Vector2(offset_front + u, v));
			point++;

			/* back */
			points.push_back(Vector3(start_x + scaled_size_x - x, -y, start_pos.z));
			normals.push_back(Vector3(0.0, 0.0, -1.0));
			ADD_TANGENT(1.0, 0.0, 0.0, -1.0);
			uvs.push_back(Vector2(twothirds + offset_back + u, v));
			point++;

			if (i > 0 && j == 1) {
				int i2 = i * 2;

				/* front */
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				/* back */
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			} else if (i > 0 && j > 0) {
				int i2 = i * 2;

				/* front */
				indices.push_back(prevrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				/* back */
				indices.push_back(prevrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			};

			x += scale * size.x / (subdivide_w + 1.0);
		};

		y += size.y / (subdivide_h + 1.0);
		prevrow = thisrow;
		thisrow = point;
	};

	/* left + right */
	Vector3 normal_left, normal_right;

	normal_left = Vector3(-size.y, size.x * left_to_right, 0.0);
	normal_right = Vector3(size.y, size.x * left_to_right, 0.0);
	normal_left.normalize();
	normal_right.normalize();

	y = start_pos.y;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_h + 1); j++) {
		float left, right;
		float scale = (y - start_pos.y) / size.y;

		left = start_pos.x + (size.x * (1.0 - scale) * left_to_right);
		right = left + (size.x * scale);

		z = start_pos.z;
		for (i = 0; i <= (subdivide_d + 1); i++) {
			float u = i;
			float v = j;
			u /= (3.0 * (subdivide_d + 1.0));
			v /= (2.0 * (subdivide_h + 1.0));

			/* right */
			points.push_back(Vector3(right, -y, -z));
			normals.push_back(normal_right);
			ADD_TANGENT(0.0, 0.0, 1.0, -1.0);
			uvs.push_back(Vector2(onethird + u, v));
			point++;

			/* left */
			points.push_back(Vector3(left, -y, z));
			normals.push_back(normal_left);
			ADD_TANGENT(0.0, 0.0, -1.0, -1.0);
			uvs.push_back(Vector2(u, 0.5 + v));
			point++;

			if (i > 0 && j > 0) {
				int i2 = i * 2;

				/* right */
				indices.push_back(prevrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				/* left */
				indices.push_back(prevrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			};

			z += size.z / (subdivide_d + 1.0);
		};

		y += size.y / (subdivide_h + 1.0);
		prevrow = thisrow;
		thisrow = point;
	};

	/* bottom */
	z = start_pos.z;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_d + 1); j++) {
		x = start_pos.x;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			float u = i;
			float v = j;
			u /= (3.0 * (subdivide_w + 1.0));
			v /= (2.0 * (subdivide_d + 1.0));

			/* bottom */
			points.push_back(Vector3(x, start_pos.y, -z));
			normals.push_back(Vector3(0.0, -1.0, 0.0));
			ADD_TANGENT(-1.0, 0.0, 0.0, -1.0);
			uvs.push_back(Vector2(twothirds + u, 0.5 + v));
			point++;

			if (i > 0 && j > 0) {
				/* bottom */
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			};

			x += size.x / (subdivide_w + 1.0);
		};

		z += size.z / (subdivide_d + 1.0);
		prevrow = thisrow;
		thisrow = point;
	};

	p_arr[VS::ARRAY_VERTEX] = points;
	p_arr[VS::ARRAY_NORMAL] = normals;
	p_arr[VS::ARRAY_TANGENT] = tangents;
	p_arr[VS::ARRAY_TEX_UV] = uvs;
	p_arr[VS::ARRAY_INDEX] = indices;
}

void PrismMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_left_to_right", "left_to_right"), &PrismMesh::set_left_to_right);
	ClassDB::bind_method(D_METHOD("get_left_to_right"), &PrismMesh::get_left_to_right);

	ClassDB::bind_method(D_METHOD("set_size", "size"), &PrismMesh::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &PrismMesh::get_size);

	ClassDB::bind_method(D_METHOD("set_subdivide_width", "segments"), &PrismMesh::set_subdivide_width);
	ClassDB::bind_method(D_METHOD("get_subdivide_width"), &PrismMesh::get_subdivide_width);
	ClassDB::bind_method(D_METHOD("set_subdivide_height", "segments"), &PrismMesh::set_subdivide_height);
	ClassDB::bind_method(D_METHOD("get_subdivide_height"), &PrismMesh::get_subdivide_height);
	ClassDB::bind_method(D_METHOD("set_subdivide_depth", "segments"), &PrismMesh::set_subdivide_depth);
	ClassDB::bind_method(D_METHOD("get_subdivide_depth"), &PrismMesh::get_subdivide_depth);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "left_to_right", PROPERTY_HINT_RANGE, "-2.0,2.0,0.1"), "set_left_to_right", "get_left_to_right");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size", PROPERTY_HINT_RANGE, "0.1,100.0,0.1"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_width", PROPERTY_HINT_RANGE, "0,100,1"), "set_subdivide_width", "get_subdivide_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_height", PROPERTY_HINT_RANGE, "0,100,1"), "set_subdivide_height", "get_subdivide_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_depth", PROPERTY_HINT_RANGE, "0,100,1"), "set_subdivide_depth", "get_subdivide_depth");
}

void PrismMesh::set_left_to_right(const float p_left_to_right) {
	left_to_right = p_left_to_right;
	_request_update();
}

float PrismMesh::get_left_to_right() const {
	return left_to_right;
}

void PrismMesh::set_size(const Vector3 &p_size) {
	size = p_size;
	_request_update();
}

Vector3 PrismMesh::get_size() const {
	return size;
}

void PrismMesh::set_subdivide_width(const int p_divisions) {
	subdivide_w = p_divisions > 0 ? p_divisions : 0;
	_request_update();
}

int PrismMesh::get_subdivide_width() const {
	return subdivide_w;
}

void PrismMesh::set_subdivide_height(const int p_divisions) {
	subdivide_h = p_divisions > 0 ? p_divisions : 0;
	_request_update();
}

int PrismMesh::get_subdivide_height() const {
	return subdivide_h;
}

void PrismMesh::set_subdivide_depth(const int p_divisions) {
	subdivide_d = p_divisions > 0 ? p_divisions : 0;
	_request_update();
}

int PrismMesh::get_subdivide_depth() const {
	return subdivide_d;
}

PrismMesh::PrismMesh() {
	// defaults
	left_to_right = 0.5;
	size = Vector3(2.0, 2.0, 2.0);
	subdivide_w = 0;
	subdivide_h = 0;
	subdivide_d = 0;
}

/**
  QuadMesh
*/

void QuadMesh::_create_mesh_array(Array &p_arr) const {
	PoolVector<Vector3> faces;
	PoolVector<Vector3> normals;
	PoolVector<float> tangents;
	PoolVector<Vector2> uvs;

	faces.resize(4);
	normals.resize(4);
	tangents.resize(4 * 4);
	uvs.resize(4);

	for (int i = 0; i < 4; i++) {

		static const Vector3 quad_faces[4] = {
			Vector3(-1, -1, 0),
			Vector3(-1, 1, 0),
			Vector3(1, 1, 0),
			Vector3(1, -1, 0),
		};

		faces.set(i, quad_faces[i]);
		normals.set(i, Vector3(0, 0, 1));
		tangents.set(i * 4 + 0, 1.0);
		tangents.set(i * 4 + 1, 0.0);
		tangents.set(i * 4 + 2, 0.0);
		tangents.set(i * 4 + 3, 1.0);

		static const Vector2 quad_uv[4] = {
			Vector2(0, 1),
			Vector2(0, 0),
			Vector2(1, 0),
			Vector2(1, 1),
		};

		uvs.set(i, quad_uv[i]);
	}

	p_arr[ARRAY_VERTEX] = faces;
	p_arr[ARRAY_NORMAL] = normals;
	p_arr[ARRAY_TANGENT] = tangents;
	p_arr[ARRAY_TEX_UV] = uvs;
};

void QuadMesh::_bind_methods() {
	// nothing here yet...
}

QuadMesh::QuadMesh() {
	primitive_type = PRIMITIVE_TRIANGLE_FAN;
}

/**
  SphereMesh
*/

void SphereMesh::_create_mesh_array(Array &p_arr) const {
	int i, j, prevrow, thisrow, point;
	float x, y, z;

	// set our bounding box

	PoolVector<Vector3> points;
	PoolVector<Vector3> normals;
	PoolVector<float> tangents;
	PoolVector<Vector2> uvs;
	PoolVector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	thisrow = 0;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		float v = j;
		float w;

		v /= (rings + 1);
		w = sin(Math_PI * v);
		y = height * (is_hemisphere ? 1.0 : 0.5) * cos(Math_PI * v);

		for (i = 0; i <= radial_segments; i++) {
			float u = i;
			u /= radial_segments;

			x = sin(u * (Math_PI * 2.0));
			z = cos(u * (Math_PI * 2.0));

			if (is_hemisphere && y < 0.0) {
				points.push_back(Vector3(x * radius * w, 0.0, z * radius * w));
				normals.push_back(Vector3(0.0, -1.0, 0.0));
			} else {
				Vector3 p = Vector3(x * radius * w, y, z * radius * w);
				points.push_back(p);
				normals.push_back(p.normalized());
			};
			ADD_TANGENT(-z, 0.0, x, -1.0)
			uvs.push_back(Vector2(u, v));
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			};
		};

		prevrow = thisrow;
		thisrow = point;
	};

	p_arr[VS::ARRAY_VERTEX] = points;
	p_arr[VS::ARRAY_NORMAL] = normals;
	p_arr[VS::ARRAY_TANGENT] = tangents;
	p_arr[VS::ARRAY_TEX_UV] = uvs;
	p_arr[VS::ARRAY_INDEX] = indices;
}

void SphereMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &SphereMesh::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &SphereMesh::get_radius);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &SphereMesh::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &SphereMesh::get_height);

	ClassDB::bind_method(D_METHOD("set_radial_segments", "radial_segments"), &SphereMesh::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &SphereMesh::get_radial_segments);
	ClassDB::bind_method(D_METHOD("set_rings", "rings"), &SphereMesh::set_rings);
	ClassDB::bind_method(D_METHOD("get_rings"), &SphereMesh::get_rings);

	ClassDB::bind_method(D_METHOD("set_is_hemisphere", "is_hemisphere"), &SphereMesh::set_is_hemisphere);
	ClassDB::bind_method(D_METHOD("get_is_hemisphere"), &SphereMesh::get_is_hemisphere);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "radius", PROPERTY_HINT_RANGE, "0.1,100.0,0.1"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height", PROPERTY_HINT_RANGE, "0.1,100.0,0.1"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "1,100,1"), "set_radial_segments", "get_radial_segments");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rings", PROPERTY_HINT_RANGE, "1,100,1"), "set_rings", "get_rings");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_hemisphere"), "set_is_hemisphere", "get_is_hemisphere");
}

void SphereMesh::set_radius(const float p_radius) {
	radius = p_radius;
	_request_update();
}

float SphereMesh::get_radius() const {
	return radius;
}

void SphereMesh::set_height(const float p_height) {
	height = p_height;
	_request_update();
}

float SphereMesh::get_height() const {
	return height;
}

void SphereMesh::set_radial_segments(const int p_radial_segments) {
	radial_segments = p_radial_segments > 4 ? p_radial_segments : 4;
	_request_update();
}

int SphereMesh::get_radial_segments() const {
	return radial_segments;
}

void SphereMesh::set_rings(const int p_rings) {
	rings = p_rings > 1 ? p_rings : 1;
	_request_update();
}

int SphereMesh::get_rings() const {
	return rings;
}

void SphereMesh::set_is_hemisphere(const bool p_is_hemisphere) {
	is_hemisphere = p_is_hemisphere;
	_request_update();
}

bool SphereMesh::get_is_hemisphere() const {
	return is_hemisphere;
}

SphereMesh::SphereMesh() {
	// defaults
	radius = 1.0;
	height = 2.0;
	radial_segments = 64;
	rings = 32;
	is_hemisphere = false;
}