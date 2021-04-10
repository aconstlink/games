

#include <natus/application/global.h>
#include <natus/application/app.h>

#include <natus/tool/imgui/sprite_editor.h>

#include <natus/format/global.h>
#include <natus/format/future_items.hpp>
#include <natus/format/natus/natus_module.h>
#include <natus/io/database.h>

#include <natus/device/layouts/xbox_controller.hpp>
#include <natus/device/layouts/game_controller.hpp>
#include <natus/device/layouts/ascii_keyboard.hpp>
#include <natus/device/mapping.hpp>
#include <natus/device/global.h>

#include <natus/gfx/camera/pinhole_camera.h>
#include <natus/gfx/sprite/sprite_render_2d.h>
#include <natus/gfx/primitive/primitive_render_2d.h>
#include <natus/gfx/font/text_render_2d.h>
#include <natus/gfx/util/quad.h>

#include <natus/graphics/variable/variable_set.hpp>
#include <natus/profiling/macros.h>

#include <natus/concurrent/global.h>

#include <natus/collide/2d/bounds/aabb.hpp>

#include <natus/math/interpolation/interpolate.hpp>
#include <natus/math/utility/angle.hpp>
#include <natus/math/vector/vector3.hpp>
#include <natus/math/vector/vector4.hpp>
#include <natus/math/matrix/matrix4.hpp>
#include <natus/math/utility/angle.hpp>
#include <natus/math/utility/3d/transformation.hpp>

#include <thread>

namespace paddle_n_ball
{
    static size_t const NUM_LAYERS = 100 ;

    using namespace natus::core::types ;

    class the_game
    {
        natus_this_typedefs( the_game ) ;

        typedef std::chrono::high_resolution_clock clock_t ;

    private:

        struct bounding_box_2d
        {
            // bottom left
            // top left
            // top right
            // bottom right 
            natus::math::vec2f_t box[4] ;
        };
        natus_typedef( bounding_box_2d ) ;

    private: // entity

        template< typename T >
        struct entity
        {
            natus_this_typedefs( entity< T > ) ;

            T comp ;

            bool_t hit = false ;

            natus::math::vec2f_t pos ;

            size_t obj_id = size_t( -1 ) ;
            size_t ani_id = size_t( -1 ) ;
            size_t anim_time = 0 ;
            size_t max_ani_time = 1 ;

            // the currently animated sprite
            natus::gfx::sprite_sheet::sprite cur_sprite ;
            float_t scale = 500.0f ;

            bounding_box_2d_t get_bb( void_t ) const noexcept
            {
                auto const rdims = cur_sprite.rect.zw() - cur_sprite.rect.xy() ;
                auto const p0 =  natus::math::vec2f_t(-0.5f, -0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p1 =  natus::math::vec2f_t(-0.5f, +0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p2 =  natus::math::vec2f_t(+0.5f, +0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p3 =  natus::math::vec2f_t(+0.5f, -0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;

                return { p0, p1, p2, p3 } ;
            }

            natus::collide::n2d::aabbf_t get_aabb( void_t ) const noexcept
            {
                auto const rdims = cur_sprite.rect.zw() - cur_sprite.rect.xy() ;
                auto const p0 =  natus::math::vec2f_t(-0.5f, -0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p2 =  natus::math::vec2f_t(+0.5f, +0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;

                return natus::collide::n2d::aabbf_t( p0, p2 ) ;
            }

            static natus::ntd::vector< this_t > load_from( natus::gfx::sprite_sheet_cref_t sheet, 
                natus::ntd::vector< natus::ntd::string_t > names,
                natus::ntd::vector< natus::ntd::string_t > animations ) noexcept
            {
                natus::ntd::vector< this_t > ret ;

                for( auto const & name : names )
                {
                    this_t intr ;

                    {
                        auto const iter = std::find_if( sheet.objects.begin(), sheet.objects.end(), 
                        [&]( natus::gfx::sprite_sheet::object const & o )
                        {
                            return o.name == name ;
                        } ) ;
                        if( iter == sheet.objects.end() )
                        {
                            natus::log::global_t::error( "Can not find object [" + name + "]. Taking 0" ) ;
                            ret.emplace_back( intr ) ;
                            continue ;
                        }
                        intr.obj_id = std::distance( sheet.objects.begin(), iter ) ;
                    }

                    auto const & obj = sheet.objects[intr.obj_id] ;
                    
                    for( auto const & ani : animations )
                    {
                        auto const iter = std::find_if( obj.animations.begin(), obj.animations.end(), 
                        [&]( natus::gfx::sprite_sheet::animation const & a )
                        {
                            return a.name == ani ;
                        } ) ;

                        intr.ani_id = std::distance( obj.animations.begin(), iter ) ;
                        intr.max_ani_time = iter->duration ;
                        break ;
                    }
                    
                    ret.emplace_back( intr ) ;
                }
                return ret ;
            }
        };

        private: // bricks

            struct level
            {
                size_t w = 0 ;
                size_t h = 0 ;

                natus::ntd::vector< size_t > layout ;
            };
            natus_typedef( level ) ;
            level_t _level ;

            struct brick
            {
                bool_t is_visible = true ;
                natus::audio::buffer_object_res_t hit_sound ;
                natus::audio::buffer_object_res_t destruction_sound ;
            };
            natus_typedefs( entity< brick >, brick ) ;
            natus_typedefs( natus::ntd::vector< brick_t >, bricks ) ;

            bricks_t _bricks ;

        private: // paddle

            struct paddle
            {
                natus::audio::buffer_object_res_t hit_sound ;

                // direction
                natus::math::vec2f_t adv ;

                size_t num_lifes = 3 ;
            };
            natus_typedefs( entity< paddle >, paddle ) ;
            paddle_t _paddle ;

        private: // ball

            struct ball
            {
                natus::audio::buffer_object_res_t hit_sound ;
                natus::math::vec2f_t adv ;
            };
            natus_typedefs( entity< ball >, ball ) ;
            natus_typedefs( natus::ntd::vector< ball_t >, balls ) ;
            ball_t _ball ;

        private: // audio

            struct audio_queue_item
            {
                natus::audio::buffer_object_res_t buffer ;
                natus::audio::execution_options eo ;
                bool_t loop = false ;
            } ;
            natus::ntd::vector< audio_queue_item > _audio_play_queue ;

        private: // score

            size_t _score = 0 ;

        private:

            bool_t _is_init = false ;
            size_t _level_cur = 0 ;
            size_t _level_max = 4 ;

        public: 

            size_t get_score( void_t ) const noexcept { return _score ; }

        public:

            struct init_data
            {
                natus::gfx::sprite_sheets_res_t sheets ;
                natus::io::database_res_t db ;
            };
            natus_typedef( init_data ) ;
            init_data _init_data ;


            //********************************************************************************
            natus::concurrent::task_res_t load_and_prepare_level_task( size_t const level_no, natus::concurrent::task_res_t tin ) 
            {
                auto const & sheets = *_init_data.sheets ;
                if( sheets.size() == 0 ) return natus::concurrent::task_res_t() ;

                auto const & sheet = sheets[0] ;

                natus::concurrent::task_res_t load_level = natus::concurrent::task_t([&, level_no]( natus::concurrent::task_res_t )
                {
                    auto const res = _init_data.db->load( natus::io::location_t("layouts.level_"+std::to_string(level_no)+".txt"), true ).
                        wait_for_operation( [&]( char_cptr_t data_ptr, size_t const sib )
                    {
                        natus::ntd::string_t const data( data_ptr, sib ) ;

                        if( data.size() == 0 ) return ;

                        size_t const p = data.find_first_of('\n') ;
                        if( p == std::string::npos ) 
                        {
                            natus::log::global_t::error("file requires \\n char.") ;
                            return ;
                        }

                        auto const w = p - 1 ;
                        auto h = 0 ;

                        this_t::level_t l ;
                        l.layout.reserve( data.size() ) ;
                        for( size_t i=0; i<data.size(); ++i ) 
                        {
                            auto const & c = data[i] ;
                            if( c == '\n' ) ++h ;
                        }
                        if( data[data.size()-1] != '\n' ||
                            data[data.size()-2] != '\n' ) ++h ;

                        for( size_t i=0; i<data.size(); ++i ) 
                        {
                            auto const & c = data[i] ;
                            if( c == '\r' || c == '\n' ) 
                            {
                                continue ;
                            }
                            l.layout.emplace_back( size_t(c) ) ;
                        }
                        l.w = w ;
                        l.h = h ;
                        
                        _level = std::move( l ) ;
                        _bricks.resize( w * h ) ;
                    } ) ;
                    natus::log::global_t::error( !res, "can not find level file for " + std::to_string(level_no) ) ;
                }) ;

                natus::concurrent::task_res_t prepare_level = natus::concurrent::task_t( [&]( natus::concurrent::task_res_t )
                {
                    natus::ntd::vector< natus::ntd::string_t > names = { "brick" } ;
                    natus::ntd::vector< natus::ntd::string_t > animations = { "idle" } ;

                    natus::math::vec2f_t dims( 1.0f ) ;

                    auto const brs = brick_t::load_from( sheet, names, animations ) ;
                    if( brs.size() > 0 )
                    {
                        for( size_t i=0; i<_bricks.size() ; ++i )
                            _bricks[i] = brs[0] ;

                        auto const s = sheets[0].determine_sprite( brs[0].obj_id, brs[0].ani_id, brs[0].anim_time ) ;
                        dims = (s.rect.zw()-s.rect.xy()) * natus::math::vec2f_t( brs[0].scale ) ;
                    }

                    auto const field = natus::math::vec2f_t( float_t(_level.w), float_t( _level.h ) ) ;
                    auto const req_off = natus::math::vec2f_t( 10.0f, 5.0f ) ;
                    auto const req_dims =  field * dims + req_off * (field-natus::math::vec2f_t(1.0f)) ;

                    natus::math::vec2f_t const start = req_dims * natus::math::vec2f_t( -0.5f, 0.0f ) + natus::math::vec2f_t( 0.0, 250.0f ) ;

                    // init positions
                    {
                        for( size_t i=0; i<_level.layout.size(); ++i )
                        {
                            size_t const y = i / _level.w ;
                            size_t const x = i % _level.w ;

                            auto & intr = _bricks[i] ;
                            intr.comp.is_visible = _level.layout[i] != '.' ;
                            intr.pos = start + natus::math::vec2f_t( float_t(x), -float_t(y) ) * (dims+req_off) ;
                        }
                    }
                }) ;

                return tin->then( load_level )->then( prepare_level ) ;
            }

            //********************************************************************************
            natus::concurrent::task_res_t on_init( init_data_rref_t d ) noexcept
            {
                _init_data = std::move( d ) ;

                auto const & sheets = *_init_data.sheets ;
                if( sheets.size() == 0 ) return natus::concurrent::task_res_t() ;

                auto const & sheet = sheets[0] ;

                natus::concurrent::task_res_t root = natus::concurrent::task_t( [&]( natus::concurrent::task_res_t )
                {
                }) ;

                natus::concurrent::task_res_t finish = natus::concurrent::task_t([&]( natus::concurrent::task_res_t )
                {
                    _is_init = true ;
                } ) ;

                this_t::load_and_prepare_level_task( _level_cur, root )->then( finish ) ;

                natus::concurrent::task_res_t prepare_player = natus::concurrent::task_t( [&]( natus::concurrent::task_res_t )
                {
                    natus::ntd::vector< natus::ntd::string_t > names = { "paddle" } ;
                    natus::ntd::vector< natus::ntd::string_t > animations = { "move" } ;
                    natus::math::vec2f_t dims( 1.0f ) ;

                    auto const ents = paddle_t::load_from( sheet, names, animations ) ;
                    if( ents.size() > 0 )
                    {
                        _paddle = ents[0] ;

                        auto const s = sheets[0].determine_sprite( ents[0].obj_id, ents[0].ani_id, ents[0].anim_time ) ;
                        dims = (s.rect.zw()-s.rect.xy()) * natus::math::vec2f_t( ents[0].scale ) ;
                    }

                    // start position
                    {
                        _paddle.scale = 500.0f ;
                        _paddle.pos = natus::math::vec2f_t( -400.0f, -250.0f ) ;
                    }
                } ) ;
                root->then( prepare_player )->then( finish ) ;

                natus::concurrent::task_res_t prepare_ball = natus::concurrent::task_t( [&]( natus::concurrent::task_res_t )
                {
                    natus::ntd::vector< natus::ntd::string_t > names = { "ball" } ;
                    natus::ntd::vector< natus::ntd::string_t > animations = { "move" } ;
                    natus::math::vec2f_t dims( 1.0f ) ;

                    auto const ents = ball_t::load_from( sheet, names, animations ) ;
                    if( ents.size() > 0 )
                    {
                        _ball = ents[0] ;

                        auto const s = sheets[0].determine_sprite( ents[0].obj_id, ents[0].ani_id, ents[0].anim_time ) ;
                        dims = (s.rect.zw()-s.rect.xy()) * natus::math::vec2f_t( ents[0].scale ) ;
                    }

                    // start position
                    {
                        _ball.scale = 150.0f ;
                        _ball.pos = natus::math::vec2f_t( 0.0f, -200.0f ) ;
                        _ball.comp.adv = natus::math::vec2f_t( 1.0f, 1.0f ) ;
                    }
                } ) ;
                root->then( prepare_ball )->then( finish ) ;

                return root ;
            }

            //********************************************************************************
            void_t on_update( void_t ) noexcept
            {
                if( !_is_init ) return ;
                //auto const dt = std::chrono::milliseconds( milli_dt ) ;
            }

            //********************************************************************************
            void_t on_device( natus::device::ascii_device_res_t keyboard, natus::device::game_device_res_t dev ) noexcept
            {
                if( !_is_init ) return ;

                // move paddle
                {
                    using ctrl_t = natus::device::layouts::game_controller_t ;
                    ctrl_t ctrl( dev ) ;

                    natus::math::vec2f_t value ;
                    if( ctrl.is( ctrl_t::directional::movement, natus::device::components::stick_state::tilting, value ) )
                    {
                        _paddle.comp.adv = value ;
                    }
                    if( ctrl.is( ctrl_t::directional::movement, natus::device::components::stick_state::untilted, value ) )
                    {
                        _paddle.comp.adv = natus::math::vec2f_t() ;
                    }
                }

                // reload level
                {
                    size_t level = size_t(-1) ;

                    natus::device::layouts::ascii_keyboard_t ascii( keyboard ) ;
                    if( ascii.get_state( natus::device::layouts::ascii_keyboard_t::ascii_key::k_1 ) ==
                        natus::device::components::key_state::released )
                    {
                        _is_init = false ;
                        level = 1 ;
                    }
                    else if( ascii.get_state( natus::device::layouts::ascii_keyboard_t::ascii_key::k_2 ) ==
                        natus::device::components::key_state::released )
                    {
                        _is_init = false ;
                        level = 2 ;
                    }
                    else if( ascii.get_state( natus::device::layouts::ascii_keyboard_t::ascii_key::k_3 ) ==
                        natus::device::components::key_state::released )
                    {
                        _is_init = false ;
                        level = 3 ;
                    }
                    else if( ascii.get_state( natus::device::layouts::ascii_keyboard_t::ascii_key::k_4 ) ==
                        natus::device::components::key_state::released )
                    {
                        _is_init = false ;
                        level = 4 ;
                    }

                    if( level != size_t(-1) )
                    {
                        _level_cur = level ;
                        natus::concurrent::task_res_t root = natus::concurrent::task_t( [&]( natus::concurrent::task_res_t ){}) ;
                        natus::concurrent::task_res_t finish = natus::concurrent::task_t([&]( natus::concurrent::task_res_t ){ _is_init = true ; } ) ;
                        this_t::load_and_prepare_level_task( level, root )->then( finish ) ;
                        natus::concurrent::global_t::schedule( root, natus::concurrent::schedule_type::loose ) ;
                    }
                }
                
            }

            //********************************************************************************
            void_t on_logic( natus::gfx::sprite_sheets_cref_t sheets, size_t const milli_dt ) noexcept 
            {
                if( !_is_init ) return ;

                size_t const sheet = 0 ;

                // paddle
                {
                    paddle_ref_t entity = _paddle ;

                    if( entity.ani_id != size_t(-1) )
                    {
                        entity.cur_sprite = sheets[sheet].determine_sprite( entity.obj_id, entity.ani_id, entity.anim_time ) ;
                        entity.anim_time += milli_dt ;
                        entity.anim_time = entity.anim_time % entity.max_ani_time ;
                    }

                    if( _paddle.comp.num_lifes == 0 )
                    {
                        _paddle.comp.num_lifes = 3 ;
                        _score = 0 ;
                    }
                }

                // ball
                {
                    natus::math::vec2f_t const thres = natus::math::vec2f_t( 400.0f, 300.0f ) ;

                    ball_ref_t entity = _ball ;

                    if( entity.ani_id != size_t(-1) )
                    {
                        entity.cur_sprite = sheets[sheet].determine_sprite( entity.obj_id, entity.ani_id, entity.anim_time ) ;
                        entity.anim_time += milli_dt ;
                        entity.anim_time = entity.anim_time % entity.max_ani_time ;
                    }
                }

                // bricks
                {
                    for( size_t y=0; y<_level.h; ++y )
                    {
                        for( size_t x=0; x<_level.w; ++x )
                        {
                            size_t const idx = y * _level.w + x ;
                            auto & intr = _bricks[ idx ] ;

                            intr.cur_sprite = sheets[sheet].determine_sprite( intr.obj_id, intr.ani_id, intr.anim_time ) ;
                            intr.anim_time += milli_dt ;
                            intr.anim_time = intr.anim_time % intr.max_ani_time ;
                        }
                    }
                }

                // test new level
                {
                    size_t elems = _bricks.size() ;
                    for( auto const & b : _bricks ) if( !b.comp.is_visible ) --elems ;

                    if( elems == 0 )
                    {
                        _is_init = false ;
                        natus::concurrent::task_res_t root = natus::concurrent::task_t( [&]( natus::concurrent::task_res_t ){}) ;
                        natus::concurrent::task_res_t finish = natus::concurrent::task_t([&]( natus::concurrent::task_res_t )
                        {
                            _is_init = true ;
                        } ) ;

                        this_t::load_and_prepare_level_task( ((++_level_cur)%_level_max)+1, root )->then( finish ) ;
                        natus::concurrent::global_t::schedule( root, natus::concurrent::schedule_type::loose ) ;
                    }

                }
            }

            //********************************************************************************
            void_t on_physics( size_t const milli_dt ) noexcept
            {
                if( !_is_init ) return ;

                float_t const dt = (float_t(milli_dt) / 1000.0f) ;

                // player
                {
                    _paddle.pos += natus::math::vec2f_t( 600.0f, 0.0f ) * 
                        natus::math::vec2f_t( _paddle.comp.adv.x() * dt ) ;

                    float_t const thres = 345.0f ;
                    if( _paddle.pos.x() > thres || _paddle.pos.x() < -thres )
                    {
                        _paddle.pos = natus::math::vec2f_t( thres * natus::math::fn<float_t>::sign(_paddle.pos.x()), 
                            _paddle.pos.y() ) ;
                    }
                }

                // ball
                {
                    _ball.pos += natus::math::vec2f_t( 500.0f ) * 
                        natus::math::vec2f_t( _ball.comp.adv * dt ) ;

                    natus::math::vec2f_t const thres = natus::math::vec2f_t( 400.0f, 300.0f ) ;

                    // test y bottom hit
                    if( _ball.pos.y() < -thres.y() )
                    {
                        _ball.pos = natus::math::vec2f_t(0.0f) ;
                        _ball.comp.adv = natus::math::vec2f_t(1.0f,1.0f) ;
                        --_paddle.comp.num_lifes ;
                    }

                    if( _ball.pos.x() > thres.x() || _ball.pos.x() < -thres.x() )
                    {
                        _ball.pos.x( natus::math::fn<float_t>::sign( _ball.pos.x() ) * thres.x() ) ;
                        _ball.comp.adv.x( _ball.comp.adv.x() * -1.0f ) ; 
                    }

                    if( _ball.pos.y() > thres.y() || _ball.pos.y() < -thres.y() )
                    {
                        _ball.pos.y( natus::math::fn<float_t>::sign( _ball.pos.y() ) * thres.y() ) ;
                        _ball.comp.adv.y( _ball.comp.adv.y() * -1.0f ) ; 
                    }
                }

                // ball paddle collision
                {
                    if( _paddle.get_aabb().is_overlapping( _ball.get_aabb() ) )
                    {
                        natus::math::vec3f_t const plane_l( -1.0f, +0.0f, -(_paddle.get_aabb().get_max() - _paddle.pos).x() ) ;
                        natus::math::vec3f_t const plane_t( +0.0f, +1.0f, -(_paddle.get_aabb().get_max() - _paddle.pos).y() ) ;
                        natus::math::vec3f_t const plane_r( +1.0f, +0.0f, -(_paddle.get_aabb().get_max() - _paddle.pos).x() ) ;
                        natus::math::vec3f_t const plane_b( +0.0f, -1.0f, -(_paddle.get_aabb().get_max() - _paddle.pos).y() ) ;

                        float_t const dist_l = plane_l.dot( natus::math::vec3f_t( _ball.pos - _paddle.pos, 1.0f ) ) ;
                        float_t const dist_t = plane_t.dot( natus::math::vec3f_t( _ball.pos - _paddle.pos, 1.0f ) ) ;
                        float_t const dist_r = plane_r.dot( natus::math::vec3f_t( _ball.pos - _paddle.pos, 1.0f ) ) ;
                        float_t const dist_b = plane_b.dot( natus::math::vec3f_t( _ball.pos - _paddle.pos, 1.0f ) ) ;

                        auto const b = natus::math::vec4f_t( dist_l, dist_r, dist_t, dist_b ).less_equal_than( natus::math::vec4f_t( 0.0f ) ) ;

                        if( b.x() && b.y() ) 
                        {
                            float_t const thres = 100.0f ;
                            _ball.comp.adv = _ball.comp.adv * natus::math::vec2f_t( 1.0f, -1.0f ) ;
                            if( std::abs( dist_r ) < thres )
                            {
                                auto const f = std::min( std::abs( dist_r ), thres ) / thres ;
                                _ball.comp.adv.x( _paddle.comp.adv.x() * (1.0f - f) ) ;
                            }

                            else if( std::abs( dist_l ) < thres )
                            {
                                auto const f = std::min( std::abs( dist_l ), thres ) / thres ;
                                _ball.comp.adv.x( _paddle.comp.adv.x() * (1.0f - f) ) ;
                            }
                        }
                        else _ball.comp.adv = _ball.comp.adv * natus::math::vec2f_t( -1.0f, 1.0f ) ;
                    }
                }

                // ball brick collision
                {
                    for( auto & b : _bricks )
                    {
                        if( !b.comp.is_visible ) continue ;

                        if( b.get_aabb().is_overlapping( _ball.get_aabb() ) )
                        {
                            natus::math::vec3f_t const plane_l( -1.0f, +0.0f, -(b.get_aabb().get_max() - b.pos).x() ) ;
                            natus::math::vec3f_t const plane_t( +0.0f, +1.0f, -(b.get_aabb().get_max() - b.pos).y() ) ;
                            natus::math::vec3f_t const plane_r( +1.0f, +0.0f, -(b.get_aabb().get_max() - b.pos).x() ) ;
                            natus::math::vec3f_t const plane_b( +0.0f, -1.0f, -(b.get_aabb().get_max() - b.pos).y() ) ;

                            auto const to_ball = _ball.pos - b.pos ;
                            float_t const dist_l = plane_l.dot( natus::math::vec3f_t( to_ball, 1.0f ) ) ;
                            float_t const dist_t = plane_t.dot( natus::math::vec3f_t( to_ball, 1.0f ) ) ;
                            float_t const dist_r = plane_r.dot( natus::math::vec3f_t( to_ball, 1.0f ) ) ;
                            float_t const dist_b = plane_b.dot( natus::math::vec3f_t( to_ball, 1.0f ) ) ;

                            auto const bin = natus::math::vec4f_t( dist_l, dist_r, dist_t, dist_b ).less_equal_than( natus::math::vec4f_t( 0.0f ) ) ;

                            if( bin.x() && bin.y() ) _ball.comp.adv = _ball.comp.adv * natus::math::vec2f_t( 1.0f, -1.0f ) ;
                            else _ball.comp.adv = _ball.comp.adv * natus::math::vec2f_t( -1.0f, 1.0f ) ;

                            b.comp.is_visible = false ;

                            _score += 100 ;

                            break ;
                        }
                    }
                }
            }

            //********************************************************************************
            void_t on_audio( natus::audio::async_access_t audio ) noexcept
            {
                if( !_is_init ) return ;
            }

            //********************************************************************************
            void_t on_graphics( natus::gfx::sprite_render_2d_res_t sr, natus::gfx::sprite_sheets_cref_t sheets, size_t const milli_dt ) noexcept
            {
                if( !_is_init ) return ;

                size_t const sheet = 0 ;

                // bricks
                {
                    natus::math::vec4f_t colors[6] = {
                        natus::math::vec4f_t( 0.0f, 1.0f, 0.0f, 1.0f ), 
                        natus::math::vec4f_t( 0.0f, 1.0f, 0.0f, 1.0f ), 
                        natus::math::vec4f_t( 0.0f, 0.0f, 1.0f, 1.0f ),
                        natus::math::vec4f_t( 0.0f, 0.0f, 1.0f, 1.0f ), 
                        natus::math::vec4f_t( 1.0f, 0.0f, 0.0f, 1.0f ), 
                        natus::math::vec4f_t( 1.0f, 0.0f, 0.0f, 1.0f ) 
                    } ;

                    for( size_t y=0; y<_level.h; ++y )
                    {
                        for( size_t x=0; x<_level.w; ++x )
                        {
                            size_t const idx = y * _level.w + x ;
                            auto const & ent = _bricks[ idx ] ;

                            if( !ent.hit && ent.comp.is_visible )
                            {
                                sr->draw( 0, 
                                    ent.pos, 
                                    natus::math::mat2f_t().identity(),
                                    natus::math::vec2f_t(ent.scale),
                                    ent.cur_sprite.rect,  
                                    sheet, ent.cur_sprite.pivot, 
                                    colors[5 - (ent.obj_id % 6)] ) ;
                            }
                        }
                    }
                }

                // player
                if( _paddle.ani_id != size_t(-1) )
                {
                    sr->draw( 0, 
                        _paddle.pos, 
                        natus::math::mat2f_t().identity(),
                        natus::math::vec2f_t(_paddle.scale),
                        _paddle.cur_sprite.rect,  
                        sheet, _paddle.cur_sprite.pivot, 
                        natus::math::vec4f_t(1.0f) ) ;
                }

                // ball
                if( _ball.ani_id != size_t(-1) )
                {
                    sr->draw( 0, 
                        _ball.pos, 
                        natus::math::mat2f_t().identity(),
                        natus::math::vec2f_t(_ball.scale),
                        _ball.cur_sprite.rect,  
                        sheet, _ball.cur_sprite.pivot, 
                        natus::math::vec4f_t(1.0f) ) ;

                    for( size_t i=0; i<_paddle.comp.num_lifes; ++i )
                    {
                        sr->draw( 0, 
                            natus::math::vec2f_t(-390.0f, 283.0f ) + natus::math::vec2f_t( float_t(i)*10.0f, 0.0f ), 
                            natus::math::mat2f_t().rotation( natus::math::angle<float_t>::degree_to_radian(90.0f) ),
                            natus::math::vec2f_t(_ball.scale*0.75f),
                            _ball.cur_sprite.rect,  
                            sheet, _paddle.cur_sprite.pivot, 
                            natus::math::vec4f_t(1.0f) ) ;
                    }

                    #if 1
                    {
                        static float_t inter = 0.0f ;
                        float_t v = natus::math::interpolation<float_t>::linear( 0.0f, 1.0f, inter ) *2.0f-1.0f ;
                        v *= 400 ;

                        sr->draw( 0, 
                            natus::math::vec2f_t( v, -200.0f ), 
                            natus::math::mat2f_t().rotation( natus::math::angle<float_t>::degree_to_radian(90.0f) ),
                            natus::math::vec2f_t(_ball.scale*0.75f),
                            _ball.cur_sprite.rect,  
                            sheet, _paddle.cur_sprite.pivot, 
                            natus::math::vec4f_t(1.0f) ) ;

                        inter += (float_t( milli_dt ) / 1000.0f)*0.5f ;
                        //inter += 0.016f*0.5f ;
                        if( inter > 1.0f ) inter = 0.0f ;
                    }
                    #endif
                }
            }

            //********************************************************************************
            void_t on_debug_graphics( natus::gfx::primitive_render_2d_res_t pr, natus::gfx::sprite_sheets_cref_t sheets, size_t const milli_dt ) noexcept
            {
                if( !_is_init ) return ;
               
                // player box
                {
                    auto const & entity = _paddle ;
                    if( entity.ani_id != size_t(-1) )
                    {
                        auto const bb = entity.get_bb() ;

                        natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.5f ) ;
                        natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;

                        pr->draw_rect( 50, bb.box[0], bb.box[1], bb.box[2], bb.box[3], color0, color1 ) ;
                    }
                }

                // ball box
                {
                    auto const & entity = _ball ;
                    if( entity.ani_id != size_t(-1) )
                    {
                        auto const bb = entity.get_bb() ;

                        natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.5f ) ;
                        natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;

                        pr->draw_rect( 50, bb.box[0], bb.box[1], bb.box[2], bb.box[3], color0, color1 ) ;
                    }
                }

                for( auto & entity : _bricks )
                {
                    auto const bb = entity.get_bb() ;

                    natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.5f ) ;
                    natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;

                    pr->draw_rect( 50, bb.box[0], bb.box[1], bb.box[2], bb.box[3], color0, color1 ) ;
                }
            }
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    class game_app : public natus::application::app
    {
        natus_this_typedefs( game_app ) ;

    private: // device

        natus::device::three_device_res_t _dev_mouse ;
        natus::device::ascii_device_res_t _dev_ascii ;

        natus::device::game_device_res_t _game_dev = natus::device::game_device_t() ;
        natus::ntd::vector< natus::device::imapping_res_t > _mappings ;

    private: // io

        natus::io::database_res_t _db ;

    private: // graphics

        natus::graphics::async_views_t _graphics ;
        natus::gfx::pinhole_camera_t _camera_0 ; // target
        natus::gfx::pinhole_camera_t _camera_1 ; // window

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

    private: // sprite tool

        natus::tool::sprite_editor_res_t _se ;
        natus::gfx::sprite_sheets_res_t _sheets ;

    private: // private

        the_game _game ;

    public:

        game_app( void_t ) 
        {
            natus::application::app::window_info_t wi ;
            #if 1
            auto view1 = this_t::create_window( "Template Application", wi ) ;
            auto view2 = this_t::create_window( "Template Application", wi,
                { natus::graphics::backend_type::gl3, natus::graphics::backend_type::d3d11}) ;

            view1.window().position( 50, 50 ) ;
            view1.window().resize( 800, 600 ) ;
            view2.window().position( 50 + 800, 50 ) ;
            view2.window().resize( 800, 600 ) ;

            _graphics = natus::graphics::async_views_t( { view1.async(), view2.async() } ) ;

            //view1.window().vsync( false ) ;
            //view2.window().vsync( false ) ;
            #elif 0
            auto view1 = this_t::create_window( "Template Application", wi, 
                { natus::graphics::backend_type::gl3, natus::graphics::backend_type::d3d11 } ) ;
            _graphics = natus::graphics::async_views_t( { view1.async() } ) ;
            #else
            auto view1 = this_t::create_window( "Template Application", wi ) ;
            _graphics = natus::graphics::async_views_t( { view1.async() } ) ;
            view1.window().vsync( true ) ;
            #endif

            _db = natus::io::database_t( natus::io::path_t( DATAPATH ), "./working", "data" ) ;
            _se = natus::tool::sprite_editor_res_t( natus::tool::sprite_editor_t( _db )  ) ;
        }
        game_app( this_cref_t ) = delete ;
        game_app( this_rref_t rhv ) : app( ::std::move( rhv ) ) 
        {
            _camera_0 = std::move( rhv._camera_0 ) ;
            _camera_1 = std::move( rhv._camera_1 ) ;
            _graphics = std::move( rhv._graphics ) ;
            _db = std::move( rhv._db ) ; 

            _sr = std::move( rhv._sr ) ;
            _pr = std::move( rhv._pr ) ;
            _tr = std::move( rhv._tr ) ;

            _fb = std::move( rhv._fb ) ;

            _se = std::move( rhv._se ) ;

            _game = std::move( rhv._game ) ;
        }
        virtual ~game_app( void_t ) 
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

            #if 1
            _fb->set_target( natus::graphics::color_target_type::rgba_uint_8, 1 )
                .resize( size_t(_screen_current.x()), size_t(_screen_current.y()) ) ;

            _graphics.for_each( [&]( natus::graphics::async_view_t a )
            {
                a.configure( _fb ) ;
            } ) ;

            _fb_render_states->access_render_state( 0, [&]( natus::graphics::render_state_sets_ref_t rss)
            {
                rss.view_s.ss.vp = natus::math::vec4ui_t( 0, 0, size_t(_screen_current.x()), size_t(_screen_current.y()) ) ;
            } ) ;
            
            _graphics.for_each( [&]( natus::graphics::async_view_t a )
            {
                a.configure( _fb_render_states ) ;
            } ) ;
            #endif

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
                    rss.view_s.ss.vp = natus::math::vec4ui_t( 0, 0, uint_t(_screen_target.x()), uint_t(_screen_target.y()) ) ;

                    so.add_render_state_set( rss ) ;
                }

                _fb_render_states = std::move( so ) ;
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.configure( _fb_render_states ) ;
                } ) ;
            }

            // import natus file
            {
                _sheets = natus::gfx::sprite_sheets_t() ;

                natus::format::module_registry_res_t mod_reg = natus::format::global_t::registry() ;
                auto item = mod_reg->import_from( natus::io::location_t( "sprite_sheet.natus" ), _db ) ;

                natus::format::natus_item_res_t ni = item.get() ;
                if( ni.is_valid() )
                {
                    natus::format::natus_document_t doc = std::move( ni->doc ) ;

                    // taking all slices
                    natus::graphics::image_t imgs ;

                    // load images
                    {
                        natus::ntd::vector< natus::format::future_item_t > futures ;
                        for( auto const & ss : doc.sprite_sheets )
                        {
                            auto const l = natus::io::location_t::from_path( natus::io::path_t(ss.image.src) ) ;
                            futures.emplace_back( mod_reg->import_from( l, _db ) ) ;
                        }

                    
                        for( size_t i=0; i<doc.sprite_sheets.size(); ++i )
                        {
                            natus::format::image_item_res_t ii = futures[i].get() ;
                            if( ii.is_valid() )
                            {
                                imgs.append( *ii->img ) ;
                            }

                            natus::gfx::sprite_sheet ss ;
                            _sheets->emplace_back( ss ) ;
                        }
                    }

                    // make sprite animation infos
                    {
                        // as an image array is used, the max dims need to be
                        // used to compute the particular rect infos
                        auto dims = imgs.get_dims() ;
                    
                        size_t i=0 ;
                        for( auto const & ss : doc.sprite_sheets )
                        {
                            auto & sheet = (*_sheets)[i++] ;

                            for( auto const & s : ss.sprites )
                            {
                                natus::math::vec4f_t const rect = 
                                    (natus::math::vec4f_t( s.animation.rect ) + 
                                        natus::math::vec4f_t(0.0f,0.0f, 1.0f, 1.0f))/ 
                                    natus::math::vec4f_t( dims.xy(), dims.xy() )  ;

                                natus::math::vec2f_t const pivot =
                                    natus::math::vec2f_t( s.animation.pivot ) / 
                                    natus::math::vec2f_t( dims.xy() ) ;

                                natus::gfx::sprite_sheet::sprite s_ ;
                                s_.rect = rect ;
                                s_.pivot = pivot ;

                                sheet.rects.emplace_back( s_ ) ;
                            }

                            natus::ntd::map< natus::ntd::string_t, size_t > object_map ;

                            for( auto const & a : ss.animations )
                            {
                                size_t obj_id = 0 ;
                                {
                                    auto iter = object_map.find( a.object ) ;
                                    if( iter != object_map.end() ) obj_id = iter->second ;
                                    else 
                                    {
                                        obj_id = sheet.objects.size() ;
                                        sheet.objects.emplace_back( natus::gfx::sprite_sheet::object { a.object, {} } ) ;
                                    }
                                }

                                natus::gfx::sprite_sheet::animation a_ ;

                                size_t tp = 0 ;
                                for( auto const & f : a.frames )
                                {
                                    auto iter = std::find_if( ss.sprites.begin(), ss.sprites.end(), 
                                        [&]( natus::format::natus_document_t::sprite_sheet_t::sprite_cref_t s )
                                    {
                                        return s.name == f.sprite ;
                                    } ) ;
                                    if( iter == ss.sprites.end() )
                                    {
                                        natus::log::global_t::error("can not find sprite [" + f.sprite + "]" ) ;
                                        continue ;
                                    }
                                    size_t const d = std::distance( ss.sprites.begin(), iter ) ;
                                    natus::gfx::sprite_sheet::animation::sprite s_ ;
                                    s_.begin = tp ;
                                    s_.end = tp + f.duration ;
                                    s_.idx = d ;
                                    a_.sprites.emplace_back( s_ ) ;

                                    tp = s_.end ;
                                }
                                a_.duration = tp ;
                                a_.name = a.name ;

                                sheet.objects[obj_id].animations.emplace_back( std::move( a_ ) ) ;
                            }
                        }
                    }

                    natus::graphics::image_object_res_t ires = natus::graphics::image_object_t( 
                        "image_array", std::move( imgs ) )
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

            // tool sprite
            {
                _se->add_sprite_sheet( "sprites", natus::io::location_t( "images.paddle_n_ball.png" ) ) ;
                //_se->add_sprite_sheet( "enemies", natus::io::location_t( "images.Paper-Pixels-8x8.Enemies.png" ) ) ;
                //_se->add_sprite_sheet( "player", natus::io::location_t( "images.Paper-Pixels-8x8.Player.png" ) ) ;
                //_se->add_sprite_sheet( "tiles", natus::io::location_t( "images.Paper-Pixels-8x8.Tiles.png" ) ) ;
                
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
                    _tr->init( std::move( *ii->obj ), NUM_LAYERS ) ;
                }
            }

            // framebuffer
            {
                _fb = natus::graphics::framebuffer_object_t( "the_scene" ) ;
                _fb->set_target( natus::graphics::color_target_type::rgba_uint_8, 1 )
                    .resize( size_t(_screen_target.x()), size_t(_screen_target.y()) ) ;

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

            {
                the_game::init_data id ;
                id.db = _db ;
                id.sheets = _sheets ;

                natus::concurrent::global_t::schedule( _game.on_init( std::move( id ) ), 
                    natus::concurrent::schedule_type::loose ) ;
            }

            {
                natus::device::xbc_device_res_t xbc_dev ;
                natus::device::ascii_device_res_t ascii_dev ;
                natus::device::three_device_res_t three_dev ;

                natus::device::global_t::system()->search( [&] ( natus::device::idevice_res_t dev_in )
                {
                    if( natus::device::xbc_device_res_t::castable( dev_in ) )
                    {
                        if( xbc_dev.is_valid() ) return ;
                        xbc_dev = dev_in ;
                    }
                    if( natus::device::ascii_device_res_t::castable( dev_in ) )
                    {
                        if( ascii_dev.is_valid() ) return ;
                        ascii_dev = dev_in ;
                    }
                    if( natus::device::three_device_res_t::castable( dev_in ) )
                    {
                        if( three_dev.is_valid() ) return ;
                        three_dev = dev_in ;
                    }
                } ) ;

                // do mappings for xbox
                if( xbc_dev.is_valid() )
                {
                    using a_t = natus::device::game_device_t ;
                    using b_t = natus::device::xbc_device_t ;

                    using ica_t = a_t::layout_t::input_component ;
                    using icb_t = b_t::layout_t::input_component ;

                    using mapping_t = natus::device::mapping< a_t, b_t > ;
                    mapping_t m( "xbc->gc", _game_dev, xbc_dev ) ;

                    {
                        auto const res = m.insert( ica_t::shoot, icb_t::button_a ) ;
                        natus::log::global_t::warning( natus::core::is_not(res), "can not do mapping." ) ;
                    }
                    {
                        auto const res = m.insert( ica_t::shoot, icb_t::button_b ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }
                    {
                        auto const res = m.insert( ica_t::action_a, icb_t::button_x ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }
                    {
                        auto const res = m.insert( ica_t::action_b, icb_t::button_y ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }
                    {
                        auto const res = m.insert( ica_t::aim, icb_t::stick_right ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }
                    {
                        auto const res = m.insert( ica_t::movement, icb_t::stick_left ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }

                    _mappings.emplace_back( natus::memory::res<mapping_t>(m) ) ;
                }

                // do mappings for ascii
                {
                    using a_t = natus::device::game_device_t ;
                    using b_t = natus::device::ascii_device_t ;

                    using ica_t = a_t::layout_t::input_component ;
                    using icb_t = b_t::layout_t::input_component ;

                    using mapping_t = natus::device::mapping< a_t, b_t > ;
                    mapping_t m( "ascii->gc", _game_dev, ascii_dev ) ;

                    {
                        auto const res = m.insert( ica_t::shoot, 
                            b_t::layout_t::ascii_key_to_input_component(b_t::layout_t::ascii_key::space ) ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }

                    {
                        auto const res = m.insert( ica_t::movement,
                            b_t::layout_t::ascii_key_to_input_component( b_t::layout_t::ascii_key::a ),
                            natus::device::mapping_detail::negative_x ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }

                    {
                        auto const res = m.insert( ica_t::movement,
                            b_t::layout_t::ascii_key_to_input_component( b_t::layout_t::ascii_key::d ),
                            natus::device::mapping_detail::positive_x ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }

                    {
                        auto const res = m.insert( ica_t::movement,
                            b_t::layout_t::ascii_key_to_input_component( b_t::layout_t::ascii_key::w ),
                            natus::device::mapping_detail::positive_y ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }

                    {
                        auto const res = m.insert( ica_t::movement,
                            b_t::layout_t::ascii_key_to_input_component( b_t::layout_t::ascii_key::s ),
                            natus::device::mapping_detail::negative_y ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }
                

                    _mappings.emplace_back( natus::memory::res<mapping_t>( m ) ) ;
                }

                // do mappings for mouse
                {
                    using a_t = natus::device::game_device_t ;
                    using b_t = natus::device::three_device_t ;

                    using ica_t = a_t::layout_t::input_component ;
                    using icb_t = b_t::layout_t::input_component ;

                    using mapping_t = natus::device::mapping< a_t, b_t > ;
                    mapping_t m( "mouse->gc", _game_dev, three_dev ) ;

                    {
                        auto const res = m.insert( ica_t::aim, icb_t::local_coords ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }

                    _mappings.emplace_back( natus::memory::res<mapping_t>( m ) ) ;
                }
            }

            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_device( natus::application::app_t::device_data_in_t ) 
        {
            _game_dev->update() ;
            for( auto & m : _mappings )
            {
                m->update() ;
            }

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

            _game.on_device( _dev_ascii, _game_dev ) ;

            //NATUS_PROFILING_COUNTER_HERE( "Device Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_logic( logic_data_in_t d ) noexcept 
        { 
            _game.on_logic( *_sheets, d.micro_dt / 1000 ) ;

            //NATUS_PROFILING_COUNTER_HERE( "Logic Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_update( natus::application::app_t::update_data_in_t ) 
        { 
            //NATUS_PROFILING_COUNTER_HERE( "Update Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_audio( natus::application::app_t::audio_data_in_t ) 
        { 
            //NATUS_PROFILING_COUNTER_HERE( "Audio Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_physics( natus::application::app_t::physics_data_in_t pd ) noexcept
        { 
            //natus::log::global_t::status( "physics: " + std::to_string( pd.micro_dt ) ) ;
            _game.on_physics( pd.micro_dt / 1000 ) ;
            //NATUS_PROFILING_COUNTER_HERE( "Physics Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_graphics( natus::application::app_t::render_data_in_t rdi ) 
        { 
            //natus::log::global_t::status( "graphics: " + std::to_string( rdi.micro_dt ) ) ;

            {
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.push( _root_render_states ) ;
                } ) ;
            }

            // BEGIN : Framebuffer
            {
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.use( _fb ) ;
                    a.push( _fb_render_states ) ;
                } ) ;
            }

            #if 0
            {
                natus::math::vec2f_t p0 =  natus::math::vec2f_t(-0.5f,-0.5f) * natus::math::vec2f_t(10, 90) ;
                natus::math::vec2f_t p1 =  natus::math::vec2f_t(-0.5f,+0.5f) * natus::math::vec2f_t(10, 90);
                natus::math::vec2f_t p2 =  natus::math::vec2f_t(+0.5f,+0.5f) * natus::math::vec2f_t(10, 90);
                natus::math::vec2f_t p3 =  natus::math::vec2f_t(+0.5f,-0.5f) * natus::math::vec2f_t(10, 90);
                natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.0f ) ;
                natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;
                _pr->draw_rect( 50, p0, p1, p2, p3, color0, color1 ) ;
            }
            #endif

            {
                _tr->draw_text( 0, 0, 10, natus::math::vec2f_t(-0.1f, 0.7f), 
                    natus::math::vec4f_t(1.0f), "Paddle'n'Ball" ) ;
            }

            {
                _game.on_graphics( _sr, *_sheets, rdi.micro_dt / 1000) ;
                
                _tr->draw_text( 0, 0, 10, natus::math::vec2f_t(-.85f, 0.7f), 
                    natus::math::vec4f_t(1.0f), std::to_string( _game.get_score()) ) ;
            
            }

            if( _draw_debug )
            {
                _game.on_debug_graphics( _pr, *_sheets, rdi.milli_dt ) ;
            }

            // render renderer
            {
                _sr->set_view_proj( _camera_0.mat_view(), _camera_0.mat_proj() ) ;
                _pr->set_view_proj( _camera_0.mat_view(), _camera_0.mat_proj() ) ;
                _tr->set_view_proj( natus::math::mat4f_t().identity(), 
                    natus::math::mat4f_t().identity().scale_by( 
                        natus::math::vec4f_t(1.0f, _screen_current.x()/_screen_current.y(), 1.0f, 1.0f) ) ) ;

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
                    a.pop( natus::graphics::backend::pop_type::render_state ) ;
                    a.unuse( natus::graphics::backend::unuse_type::framebuffer ) ;
                } ) ;
            }
            
            {
                _graphics.for_each( [&]( natus::graphics::async_view_t a )
                {
                    a.pop( natus::graphics::backend::pop_type::render_state ) ;
                } ) ;
            }

            {
                _quad->set_view_proj( natus::math::mat4f_t().identity(),  _camera_1.mat_proj() ) ;
                _quad->set_scale( _screen_current*0.5f ) ;
                
                _quad->render( _graphics ) ;
            }

            //NATUS_PROFILING_COUNTER_HERE( "Graphics Clock" ) ;
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

            _se->render( imgui ) ;

            bool_t tmp = true ;
            ImGui::ShowDemoWindow( &tmp ) ;

            
            return natus::application::result::ok ;
        }

        virtual natus::application::result on_shutdown( void_t ) 
        { return natus::application::result::ok ; }
    };
    natus_res_typedef( game_app ) ;
}

int main( int argc, char ** argv )
{
    return natus::application::global_t::create_application( 
        paddle_n_ball::game_app_res_t( paddle_n_ball::game_app_t() ) )->exec() ;
}
 