#include "post_process_region.h"

#include <geomVertexWriter.h>
#include <geomTriangles.h>
#include <omniBoundingVolume.h>
#include <orthographicLens.h>

namespace rpcore {

PostProcessRegion* PostProcessRegion::make(GraphicsOutput* internal_buffer)
{
	return new PostProcessRegion(internal_buffer);
}

PostProcessRegion* PostProcessRegion::make(GraphicsOutput* internal_buffer, const LVecBase4f& dimensions)
{
	return new PostProcessRegion(internal_buffer, dimensions);
}

PostProcessRegion::PostProcessRegion(GraphicsOutput* internal_buffer)
{
    _buffer = internal_buffer;
	region = _buffer->make_display_region();
	node = NodePath("RTRoot");

	make_fullscreen_tri();
	make_fullscreen_cam();
	init_function_pointers();
}

PostProcessRegion::PostProcessRegion(GraphicsOutput* internal_buffer, const LVecBase4f& dimensions)
{
    _buffer = internal_buffer;
	region = _buffer->make_display_region(dimensions);
	node = NodePath("RTRoot");

	make_fullscreen_tri();
	make_fullscreen_cam();
	init_function_pointers();
}

void PostProcessRegion::init_function_pointers(void)
{
	using namespace std::placeholders;
	
	set_sort = std::bind(&DisplayRegion::set_sort, region, _1);
	disable_clears = std::bind(&DisplayRegion::disable_clears, region);
	set_active = std::bind(&DisplayRegion::set_active, region, _1);
	set_clear_depth_active = std::bind(&DisplayRegion::set_clear_depth_active, region, _1);
	set_clear_depth = std::bind(&DisplayRegion::set_clear_depth, region, _1);
	set_camera = std::bind(&DisplayRegion::set_camera, region, _1);
	set_clear_color_active = std::bind(&DisplayRegion::set_clear_color_active, region, _1);
	set_clear_color = std::bind(&DisplayRegion::set_clear_color, region, _1);
	set_draw_callback = std::bind(&DisplayRegion::set_draw_callback, region, _1);

	set_instance_count = std::bind(&NodePath::set_instance_count, tri, _1);
	set_shader = std::bind(&NodePath::set_shader, tri, _1, _2);
	set_attrib = std::bind(&NodePath::set_attrib, tri, _1, _2);
}

void PostProcessRegion::make_fullscreen_tri(void)
{
	const GeomVertexFormat* vformat =  GeomVertexFormat::get_v3();
	PT(GeomVertexData) vdata = new GeomVertexData("vertices", vformat, Geom::UH_static);
	vdata->set_num_rows(3);
	GeomVertexWriter vwriter(vdata, "vertex");
	vwriter.add_data3f(-1, 0, -1);
	vwriter.add_data3f(3, 0, -1);
	vwriter.add_data3f(-1, 0, 3);
	PT(GeomTriangles) gtris = new GeomTriangles(Geom::UH_static);
	gtris->add_next_vertices(3);
	PT(Geom) geom = new Geom(vdata);
	geom->add_primitive(gtris);
	PT(GeomNode) geom_node = new GeomNode("gn");
	geom_node->add_geom(geom);
	geom_node->set_final(true);
	PT(OmniBoundingVolume) obv = new OmniBoundingVolume();
	geom_node->set_bounds(obv);
	NodePath tri = NodePath(geom_node);
	tri.set_depth_test(false);
	tri.set_depth_write(false);
	tri.set_attrib(TransparencyAttrib::make(TransparencyAttrib::M_none), 10000);
	tri.set_color(LColor(1));
	tri.set_bin("unsorted", 10);
	tri.reparent_to(node);
	this->tri = tri;
}

void PostProcessRegion::make_fullscreen_cam(void)
{
	PT(Camera) buffer_cam = new Camera("BufferCamera");
	PT(OrthographicLens) lens = new OrthographicLens;
	lens->set_film_size(2, 2);
	lens->set_film_offset(0, 0);
	lens->set_near_far(-100, 100);
	buffer_cam->set_lens(lens);
	PT(OmniBoundingVolume) obv = new OmniBoundingVolume();
	buffer_cam->set_cull_bounds(obv);
	camera = node.attach_new_node(buffer_cam);
	region->set_camera(camera);
}

}
