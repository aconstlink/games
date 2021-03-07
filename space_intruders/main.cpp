

#include <natus/application/global.h>
#include <natus/application/app.h>

#include <natus/format/global.h>
#include <natus/format/future_items.hpp>
#include <natus/io/database.h>

#include <natus/device/global.h>
#include <natus/gfx/camera/pinhole_camera.h>
#include <natus/gfx/sprite/sprite_render_2d.h>
#include <natus/gfx/primitive/primitive_render_2d.h>
#include <natus/gfx/font/text_render_2d.h>
#include <natus/gfx/util/quad.h>

#include <natus/graphics/variable/variable_set.hpp>
#include <natus/profiling/macros.h>

#include <natus/geometry/mesh/polygon_mesh.h>
#include <natus/geometry/mesh/tri_mesh.h>
#include <natus/geometry/mesh/flat_tri_mesh.h>
#include <natus/geometry/3d/cube.h>
#include <natus/geometry/3d/tetra.h>
#include <natus/math/vector/vector3.hpp>
#include <natus/math/vector/vector4.hpp>
#include <natus/math/matrix/matrix4.hpp>
#include <natus/math/utility/angle.hpp>
#include <natus/math/utility/3d/transformation.hpp>

#include <thread>

namespace space_intruders
{
    static size_t const NUM_LAYERS = 100 ;

    using namespace natus::core::types ;

    class the_game : public natus::application::app
    {
        natus_this_typedefs( the_game ) ;

    private: // device

        natus::device::three_device_res_t _dev_mouse ;
        natus::device::ascii_device_res_t _dev_ascii ;

    private: // io

        natus::io::database_res_t _db ;

    private: // graphics

        natus::graphics::async_views_t _graphics ;
        natus::gfx::pinhole_camera_t _camera_0 ;
        natus::gfx::pinhole_camera_t _camera_1 ;

        natus::gfx::sprite_render_2d_res_t _sr ;
        natus::gfx::primitive_render_2d_res_t _pr ;
        natus::gfx::text_render_2d_res_t _tr ;

        natus::graphics::state_object_res_t _root_render_states ;
        natus::graphics::state_object_res_t _fb_render_states ;
        natus::graphics::framebuffer_object_res_t _fb  ;

        natus::math::vec2f_t _screen_target = natus::math::vec2f_t(800, 600) ;
        natus::math::vec2f_t _screen_current = natus::math::vec2f_t( 100, 100 ) ;
        natus::math::vec2f_t _ratio ;

        natus::gfx::quad_res_t _quad ;

    private: // tool/debug

        bool_t _do_tool = false ;
        bool_t _draw_debug = false ;

    public:

        the_game( void_t ) 
        {
            natus::application::app::window_info_t wi ;
            #if 1
            auto view1 = this_t::create_window( "A Render Window", wi ) ;
            auto view2 = this_t::create_window( "A Render Window", wi,
                { natus::graphics::backend_type::gl3, natus::graphics::backend_type::d3d11}) ;

            view1.window().position( 50, 50 ) ;
            view1.window().resize( 800, 600 ) ;
            view2.window().position( 50 + 800, 50 ) ;
            view2.window().resize( 800, 600 ) ;

            _graphics = natus::graphics::async_views_t( { view1.async(), view2.async() } ) ;
            #elif 0
            auto view1 = this_t::create_window( "A Render Window", wi, 
                { natus::graphics::backend_type::gl3, natus::graphics::backend_type::d3d11 } ) ;
            _graphics = natus::graphics::async_views_t( { view1.async() } ) ;
            #else
            auto view1 = this_t::create_window( "A Render Window", wi ) ;
            _graphics = natus::graphics::async_views_t( { view1.async() } ) ;
            #endif

            _db = natus::io::database_t( natus::io::path_t( DATAPATH ), "./working", "data" ) ;
        }
        the_game( this_cref_t ) = delete ;
        the_game( this_rref_t rhv ) : app( ::std::move( rhv ) ) 
        {
            _camera_0 = std::move( rhv._camera_0 ) ;
            _camera_1 = std::move( rhv._camera_1 ) ;
            _graphics = std::move( rhv._graphics ) ;
            _db = std::move( rhv._db ) ; 

            _sr = std::move( rhv._sr ) ;
            _pr = std::move( rhv._pr ) ;
            _tr = std::move( rhv._tr ) ;

            _fb = std::move( rhv._fb ) ;
        }
        virtual ~the_game( void_t ) 
        {}

        virtual natus::application::result on_event( window_id_t const, this_t::window_event_info_in_t wei )
        {
            natus::math::vec2f_t const target = _screen_target ; 
            natus::math::vec2f_t const window = natus::math::vec2f_t( float_t(wei.w), float_t(wei.h) ) ;
            natus::math::vec2f_t const ratio = window / target ;

            _ratio = ratio ;
            _screen_current = target * (ratio.x() < ratio.y() ? ratio.xx() : ratio.yy()) ;

            _camera_0.orthographic( target.x(), target.y(), 0.1f, 1000.0f ) ;
            _camera_1.orthographic( window.x(), window.y(), 0.1f, 1000.0f ) ;

            return natus::application::result::ok ;
        }

    private:

        virtual natus::application::result on_init( void_t )
        { 
            natus::device::global_t::system()->search( [&] ( natus::device::idevice_res_t dev_in )
            {
                if( natus::device::three_device_res_t::castable( dev_in ) )
                {
                    _dev_mouse = dev_in ;
                }
                else if( natus::device::ascii_device_res_t::castable( dev_in ) )
                {
                    _dev_ascii = dev_in ;
                }
            } ) ;

            if( !_dev_mouse.is_valid() )
            {
                natus::log::global_t::status( "no three mosue found" ) ;
            }

            if( !_dev_ascii.is_valid() )
            {
                natus::log::global_t::status( "no ascii keyboard found" ) ;
            }

            {
                _camera_0.look_at( natus::math::vec3f_t( 0.0f, 0.0f, -10.0f ),
                        natus::math::vec3f_t( 0.0f, 1.0f, 0.0f ), natus::math::vec3f_t( 0.0f, 0.0f, 0.0f )) ;
            }

            // root render states
            {
                natus::graphics::state_object_t so = natus::graphics::state_object_t(
                    "root_render_states" ) ;

                {
                    natus::graphics::render_state_sets_t rss ;

                    rss.depth_s.do_change = true ;
                    rss.depth_s.ss.do_activate = false ;
                    rss.depth_s.ss.do_depth_write = false ;

                    rss.polygon_s.do_change = true ;
                    rss.polygon_s.ss.do_activate = true ;
                    rss.polygon_s.ss.ff = natus::graphics::front_face::clock_wise ;
                    rss.polygon_s.ss.cm = natus::graphics::cull_mode::back ;
                    rss.polygon_s.ss.fm = natus::graphics::fill_mode::fill ;

                    rss.clear_s.do_change = true ;
                    rss.clear_s.ss.do_activate = true ;
                    rss.clear_s.ss.do_color_clear = true ;
                    rss.clear_s.ss.clear_color = natus::math::vec4f_t(0.2f,0.2f,0.2f,1.0f) ;
                   
                    so.add_render_state_set( rss ) ;
                }

                _root_render_states = std::move( so ) ;
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.configure( _root_render_states ) ;
                } ) ;
            }

            // root render states
            {
                natus::graphics::state_object_t so = natus::graphics::state_object_t(
                    "fb_render_states" ) ;

                {
                    natus::graphics::render_state_sets_t rss ;

                    rss.depth_s.do_change = true ;
                    rss.depth_s.ss.do_activate = false ;
                    rss.depth_s.ss.do_depth_write = false ;

                    rss.polygon_s.do_change = true ;
                    rss.polygon_s.ss.do_activate = true ;
                    rss.polygon_s.ss.ff = natus::graphics::front_face::clock_wise ;
                    rss.polygon_s.ss.cm = natus::graphics::cull_mode::back ;
                    rss.polygon_s.ss.fm = natus::graphics::fill_mode::fill ;

                    rss.clear_s.do_change = true ;
                    rss.clear_s.ss.do_activate = true ;
                    rss.clear_s.ss.do_color_clear = true ;
                    rss.clear_s.ss.clear_color = natus::math::vec4f_t(0.0f,0.5f,1.0f,1.0f) ;
                   
                    rss.view_s.do_change = true ;
                    rss.view_s.ss.do_activate = true ;
                    rss.view_s.ss.vp = natus::math::vec4ui_t( 0, 0, _screen_target.x(), _screen_target.y() ) ;

                    so.add_render_state_set( rss ) ;
                }

                _fb_render_states = std::move( so ) ;
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.configure( _fb_render_states ) ;
                } ) ;
            }
            
            // image configuration
            {
                size_t const num_images = 1 ;
                natus::format::module_registry_res_t mod_reg = natus::format::global_t::registry() ;
                natus::format::future_item_t items[num_images] = 
                {
                    mod_reg->import_from( natus::io::location_t( "images.space_intruders.png" ), _db )
                } ;

                // taking all slices
                natus::graphics::image_t img ;

                // load each slice into the image
                for( size_t i=0; i<num_images; ++i )
                {
                    natus::format::image_item_res_t ii = items[i].get() ;
                    if( ii.is_valid() )
                    {
                        img.append( *ii->img ) ;
                    }
                }

                natus::graphics::image_object_res_t ires = natus::graphics::image_object_t( 
                    "image_array", std::move( img ) )
                    .set_type( natus::graphics::texture_type::texture_2d_array )
                    .set_wrap( natus::graphics::texture_wrap_mode::wrap_s, natus::graphics::texture_wrap_type::repeat )
                    .set_wrap( natus::graphics::texture_wrap_mode::wrap_t, natus::graphics::texture_wrap_type::repeat )
                    .set_filter( natus::graphics::texture_filter_mode::min_filter, natus::graphics::texture_filter_type::nearest )
                    .set_filter( natus::graphics::texture_filter_mode::mag_filter, natus::graphics::texture_filter_type::nearest );

                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.configure( ires ) ;
                } ) ;
            }

            // prepare sprite render
            {
                _sr = natus::gfx::sprite_render_2d_res_t( natus::gfx::sprite_render_2d_t() ) ;
                _sr->init( "sprite_render", "image_array", _graphics ) ;
            }
            
            // prepare primitive
            {
                _pr = natus::gfx::primitive_render_2d_res_t( natus::gfx::primitive_render_2d_t() ) ;
                _pr->init( "prim_render", _graphics ) ;
            }

            // import fonts and create text render
            {
                natus::property::property_sheet_res_t ps = natus::property::property_sheet_t() ;

                {
                    natus::font::code_points_t pts ;
                    for( uint32_t i = 33; i <= 126; ++i ) pts.emplace_back( i ) ;
                    for( uint32_t i : {uint32_t( 0x00003041 )} ) pts.emplace_back( i ) ;
                    ps->set_value< natus::font::code_points_t >( "code_points", pts ) ;
                }

                #if 0
                {
                    natus::ntd::vector< natus::io::location_t > locations = 
                    {
                        natus::io::location_t("fonts.LCD_Solid.ttf"),
                        //natus::io::location_t("")
                    } ;
                    ps->set_value( "additional_locations", locations ) ;
                }
                #endif 

                {
                    ps->set_value<size_t>( "atlas_width", 512 ) ;
                    ps->set_value<size_t>( "atlas_height", 512 ) ;
                    ps->set_value<size_t>( "point_size", 90 ) ;
                }

                natus::format::module_registry_res_t mod_reg = natus::format::global_t::registry() ;
                auto fitem = mod_reg->import_from( natus::io::location_t( "fonts.LCD_Solid.ttf" ), _db, ps ) ;
                natus::format::glyph_atlas_item_res_t ii = fitem.get() ;
                if( ii.is_valid() )
                {
                    _tr = natus::gfx::text_render_2d_res_t( natus::gfx::text_render_2d_t( "text_render", _graphics ) ) ;
                    _tr->init( std::move( *ii->obj ) ) ;
                }
            }

            // framebuffer
            {
                _fb = natus::graphics::framebuffer_object_t( "the_scene" ) ;
                _fb->set_target( natus::graphics::color_target_type::rgba_uint_8, 2 )
                    .resize( _screen_target.x(), _screen_target.y() ) ;

                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.configure( _fb ) ;
                } ) ;
            }

            // prepare quad
            {
                _quad = natus::gfx::quad_res_t( natus::gfx::quad_t("post_map") ) ;
                _quad->set_texture("the_scene.0") ;
                _quad->init( _graphics ) ;
            }

            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_device( natus::application::app_t::device_data_in_t ) 
        {
            {
                natus::device::layouts::ascii_keyboard_t ascii( _dev_ascii ) ;
                if( ascii.get_state( natus::device::layouts::ascii_keyboard_t::ascii_key::f8 ) ==
                    natus::device::components::key_state::released )
                {
                }
                else if( ascii.get_state( natus::device::layouts::ascii_keyboard_t::ascii_key::f9 ) ==
                    natus::device::components::key_state::released )
                {
                }
                else if( ascii.get_state( natus::device::layouts::ascii_keyboard_t::ascii_key::f2 ) ==
                    natus::device::components::key_state::released )
                {
                    _do_tool = !_do_tool ;
                }
            }

            NATUS_PROFILING_COUNTER_HERE( "Device Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_update( natus::application::app_t::update_data_in_t ) 
        { 
            NATUS_PROFILING_COUNTER_HERE( "Update Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_audio( natus::application::app_t::audio_data_in_t ) 
        { 
            NATUS_PROFILING_COUNTER_HERE( "Audio Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_physics( natus::application::app_t::physics_data_in_t ) noexcept
        { 
            NATUS_PROFILING_COUNTER_HERE( "Physics Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_graphics( natus::application::app_t::render_data_in_t ) 
        { 
            {
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.use( _root_render_states ) ;
                } ) ;
            }

            // BEGIN : Framebuffer
            {
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.use( _fb ) ;
                    a.use( _fb_render_states ) ;
                } ) ;
            }

            {
                _tr->draw_text( 0, 0, 10, natus::math::vec2f_t(0.0f, -0.6f), 
                    natus::math::vec4f_t(1.0f), "Template" ) ;
            }

            // draw extend of aspect
            if( _draw_debug )
            {
                natus::math::vec2f_t p0 = natus::math::vec2f_t() + _screen_current * natus::math::vec2f_t(-0.5f,-0.5f) ;
                natus::math::vec2f_t p1 = natus::math::vec2f_t() + _screen_current * natus::math::vec2f_t(-0.5f,+0.5f) ;
                natus::math::vec2f_t p2 = natus::math::vec2f_t() + _screen_current * natus::math::vec2f_t(+0.5f,+0.5f) ;
                natus::math::vec2f_t p3 = natus::math::vec2f_t() + _screen_current * natus::math::vec2f_t(+0.5f,-0.5f) ;

                natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.0f ) ;
                natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;

                _pr->draw_rect( 50, p0, p1, p2, p3, color0, color1 ) ;
            }

            // render renderer
            {
                //_sr->set_view_proj( _camera_0.mat_view(), _camera_0.mat_proj() ) ;
                _pr->set_view_proj( _camera_0.mat_view(), _camera_0.mat_proj() ) ;
                //_tr->set_view_projection( _camera_0.mat_view(), _camera_0.mat_proj() ) ;

                _sr->prepare_for_rendering() ;
                _pr->prepare_for_rendering() ;
                _tr->prepare_for_rendering() ;

                for( size_t i=0; i<NUM_LAYERS; ++i )
                {
                    _sr->render( i ) ;
                    _pr->render( i ) ;
                    _tr->render( i ) ;
                }
            }
            
            // END Framebuffer
            {
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.use( natus::graphics::state_object_t() ) ;
                    a.use( natus::graphics::framebuffer_object_t() ) ;
                } ) ;
            }
            
            {
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.use( natus::graphics::state_object_t() ) ;
                } ) ;
            }

            {
                _quad->set_view_proj( natus::math::mat4f_t().identity(),  _camera_1.mat_proj() ) ;
                _quad->set_scale( _screen_current*0.5f ) ;
                
                _quad->render( _graphics ) ;
            }

            NATUS_PROFILING_COUNTER_HERE( "Graphics Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_tool( natus::tool::imgui_view_t imgui )
        {
            if( !_do_tool ) return natus::application::result::no_imgui ;

            ImGui::Begin( "Controlls" ) ;
            {
                ImGui::Checkbox( "Draw Debug", &_draw_debug ) ;
                ImGui::Text( "Display Resolution : %.2f, %.2f", _screen_current.x(), _screen_current.y() ) ;
            }
            ImGui::End() ;

            ImGui::Begin( "do something" ) ;
            ImGui::End() ;
            return natus::application::result::ok ;
        }

        virtual natus::application::result on_shutdown( void_t ) 
        { return natus::application::result::ok ; }
    };
    natus_res_typedef( the_game ) ;
}

int main( int argc, char ** argv )
{
    return natus::application::global_t::create_application( 
        space_intruders::the_game_res_t( space_intruders::the_game_t() ) )->exec() ;
}
