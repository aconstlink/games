

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

        private: // 

            struct level
            {
                size_t w = 40 ;
                size_t h = 60 ;

                natus::ntd::vector< size_t > layout ;
            };
            natus_typedef( level ) ;
            level_t _level ;

            natus::math::vec2f_t _game_space = natus::math::vec2f_t( 800.0f, 600.0f ) ;
            natus::math::vec2f_t _dims = natus::math::vec2f_t( _game_space.x() / float_t( _level.w ), _game_space.y() / float_t( _level.h ) ) ;

            struct brick
            {
                bool_t is_visible = true ;
                natus::audio::buffer_object_res_t hit_sound ;
                natus::audio::buffer_object_res_t destruction_sound ;
            };
            natus_typedefs( entity< brick >, brick ) ;
            natus_typedefs( natus::ntd::vector< brick_t >, bricks ) ;

            bricks_t _bricks ;

            struct player
            {
                natus::math::vec2f_t adv ;
                natus::math::vec2f_t adv2 ;

                size_t cur_shape = 4 ;
                size_t shape_elems = 0 ;
                std::array< natus::math::vec2f_t, 5 > shape ;

                // 0 : L 0 x cw
                // 1 : L 1 x cw
                // 2 : L 2 x cw
                // 3 : L 3 x cw
                // 4 : rect
                // 5 : |
                // 6 : -
                void_t set_shape( size_t const s, natus::math::vec2f_t pos, natus::math::vec2f_t dims ) noexcept
                {
                    cur_shape = s ;

                    if( s == 0 )
                    {
                        shape_elems = 5 ;
                        shape[0] = pos + dims * natus::math::vec2f_t( 0.0f, 0.0f ) ;
                        shape[1] = pos + dims * natus::math::vec2f_t( 1.0f, 0.0f ) ;
                        shape[2] = pos + dims * natus::math::vec2f_t( 0.0f, 1.0f ) ;
                        shape[3] = pos + dims * natus::math::vec2f_t( 0.0f, 2.0f ) ;
                        shape[4] = pos + dims * natus::math::vec2f_t( 0.0f, 3.0f ) ;
                    }
                    else if( s == 1 )
                    {
                        shape_elems = 5 ;
                        shape[0] = pos + dims * natus::math::vec2f_t( 0.0f, 0.0f ) ;
                        shape[1] = pos + dims * natus::math::vec2f_t( 0.0f, -1.0f ) ;
                        shape[2] = pos + dims * natus::math::vec2f_t( 1.0f, 0.0f ) ;
                        shape[3] = pos + dims * natus::math::vec2f_t( 2.0f, 0.0f ) ;
                        shape[4] = pos + dims * natus::math::vec2f_t( 3.0f, 0.0f ) ;
                    }
                    else if( s == 2 )
                    {
                        shape_elems = 5 ;
                        shape[0] = pos + dims * natus::math::vec2f_t( 0.0f, 0.0f ) ;
                        shape[1] = pos + dims * natus::math::vec2f_t( -1.0f, 0.0f ) ;
                        shape[2] = pos + dims * natus::math::vec2f_t( 0.0f, -1.0f ) ;
                        shape[3] = pos + dims * natus::math::vec2f_t( 0.0f, -2.0f ) ;
                        shape[4] = pos + dims * natus::math::vec2f_t( 0.0f, -3.0f ) ;
                    }
                    else if( s == 3 )
                    {
                        shape_elems = 5 ;
                        shape[0] = pos + dims * natus::math::vec2f_t( 0.0f, 0.0f ) ;
                        shape[1] = pos + dims * natus::math::vec2f_t( 0.0f, 1.0f ) ;
                        shape[2] = pos + dims * natus::math::vec2f_t( -1.0f, 0.0f ) ;
                        shape[3] = pos + dims * natus::math::vec2f_t( -2.0f, 0.0f ) ;
                        shape[4] = pos + dims * natus::math::vec2f_t( -3.0f, 0.0f ) ;
                    }
                    else if( s == 4 )
                    {
                        shape_elems = 4 ;
                        shape[0] = pos + dims * natus::math::vec2f_t( 0.0f, 0.0f ) ;
                        shape[1] = pos + dims * natus::math::vec2f_t( 0.0f, 1.0f ) ;
                        shape[2] = pos + dims * natus::math::vec2f_t( 1.0f, 0.0f ) ;
                        shape[3] = pos + dims * natus::math::vec2f_t( 1.0f, 1.0f ) ;
                    }
                    else if( s == 5 )
                    {
                        shape_elems = 4 ;
                        shape[0] = pos + dims * natus::math::vec2f_t( 0.0f, 0.0f ) ;
                        shape[1] = pos + dims * natus::math::vec2f_t( 0.0f, 1.0f ) ;
                        shape[2] = pos + dims * natus::math::vec2f_t( 0.0f, 2.0f ) ;
                        shape[3] = pos + dims * natus::math::vec2f_t( 0.0f, 3.0f ) ;
                    }
                    else if( s == 6 )
                    {
                        shape_elems = 4 ;
                        shape[0] = pos + dims * natus::math::vec2f_t( 0.0f, 0.0f ) ;
                        shape[1] = pos + dims * natus::math::vec2f_t( 1.0f, 0.0f ) ;
                        shape[2] = pos + dims * natus::math::vec2f_t( 2.0f, 0.0f ) ;
                        shape[3] = pos + dims * natus::math::vec2f_t( 3.0f, 0.0f ) ;
                    }
                }

                void_t rotate( size_t const lr ) noexcept
                {
                    if( cur_shape <= 3 )
                    {
                        cur_shape += lr == 0 ? -1 : 1 ;
                        cur_shape %= 7 ;
                    }
                }
            } ;
            natus_typedefs( entity< player >, player ) ;
            
            player_t _player ;

            
            size_t _next_shape = 0 ;

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
                natus::io::database_res_t db ;
                natus::audio::async_access_t audio ;
            };
            natus_typedef( init_data ) ;
            init_data _init_data ;

            //********************************************************************************
            natus::concurrent::task_res_t on_init( init_data_rref_t d ) noexcept
            {
                _init_data = std::move( d ) ;

                natus::concurrent::task_res_t root = natus::concurrent::task_t( [&]( natus::concurrent::task_res_t )
                {
                    _player.pos = natus::math::vec2f_t( 0.0f, 600.0f ) ;
                    _player.comp.adv = natus::math::vec2f_t( 0.0f, -200.0f ) ;
                    _player.comp.set_shape( _player.comp.cur_shape, _player.pos, _dims ) ;

                    _level.layout.resize( _level.w * _level.h ) ;
                    for( auto & i : _level.layout )
                    {
                        i = 0 ;
                    }
                }) ;

                natus::concurrent::task_res_t finish = natus::concurrent::task_t([&]( natus::concurrent::task_res_t )
                {
                    _is_init = true ;
                } ) ;

                root->then( finish ) ;

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
                        _player.comp.adv2 = natus::math::vec2f_t(200.0f) * value ;
                    }
                    else if( ctrl.is( ctrl_t::directional::movement, natus::device::components::stick_state::untilted, value ) )
                    {
                       _player.comp.adv2 = natus::math::vec2f_t( 0.0f, 0.0f ) ;
                    }

                    float_t button_value ;
                    if( ctrl.is( ctrl_t::button::action_a, natus::device::components::button_state::released, button_value ) )
                    {
                        _player.comp.rotate( 0 ) ;
                    }
                    else if( ctrl.is( ctrl_t::button::action_b, natus::device::components::button_state::released, button_value ) )
                    {
                        _player.comp.rotate( 1 ) ;
                    }
                }
            }

            //********************************************************************************
            void_t on_logic( size_t const milli_dt ) noexcept 
            {
                if( !_is_init ) return ;

                for( size_t y=0; y<_level.h; ++y )
                {
                    size_t c = 0 ;
                    for( size_t x=0; x<_level.w; ++x )
                    {
                        if( _level.layout[ y * _level.w + x ] == 0 ) break ;
                        ++c ;
                    }

                    // clear row
                    if( c == _level.w ) 
                    {
                        size_t y1 = y ;
                        for( size_t y2=y+1; y2<_level.h-1; ++y2, ++y1 )
                        {
                            for( size_t x=0; x<_level.w; ++x )
                            {
                                _level.layout[ y1 * _level.w + x ] = 
                                    _level.layout[ y2 * _level.w + x ] ;
                            }
                        }
                    }
                }
            }

            //********************************************************************************
            void_t on_physics( size_t const milli_dt ) noexcept
            {
                if( !_is_init ) return ;

                float_t const dt = (float_t(milli_dt) / 1000.0f) ;
                
                player_t next ;
                next.pos = _player.pos ;
                next.comp.adv = _player.comp.adv ;
                next.comp.adv2 = _player.comp.adv2 ;

                // advance player
                {
                    next.pos += (next.comp.adv + next.comp.adv2) * dt ;

                    natus::math::vec2f_t const pcell = (next.pos / _dims).floored() ;
                    natus::math::vec2f_t const pos = pcell * _dims ;
                    next.comp.set_shape( _player.comp.cur_shape, pos, _dims ) ; 
                }
                
                // collision left right bottom
                {
                    natus::math::vec2f_t side_ground( 1.0f ) ;

                    for( size_t i=0; i<next.comp.shape_elems; ++i )
                    {
                        auto const p = next.comp.shape[i] ;

                        natus::math::vec2f_t const cell = (p / _dims).floored() ;
                    
                        if( cell.x() < 0.0f || cell.x() >= float_t(_level.w) )
                        {
                            side_ground.x( 0.0f ) ;
                        }

                        if( cell.y() < 0.0f )
                        {
                            side_ground.y( 0.0f ) ;
                        }
                    }

                    // do correction
                    {
                        next.comp.adv *= side_ground ;
                        next.comp.adv2 *= side_ground ;
                        auto const adv = next.comp.adv + next.comp.adv2 ;
                        next.pos = _player.pos + adv * dt ;
                        next.comp.set_shape( _player.comp.cur_shape, next.pos, _dims ) ;
                    }

                    if( side_ground.y() < 0.5f )
                    {
                        for( size_t i=0; i<next.comp.shape_elems; ++i )
                        {
                            auto const p = next.comp.shape[i] ;

                            natus::math::vec2f_t const cell = (p / _dims).floored() ;
                            size_t const x = size_t( cell.x() ) ;
                            size_t const y = size_t( cell.y() ) ;
                            _level.layout[ y * _level.w + x ] = 1 ;
                        }
                        
                        _player.pos = natus::math::vec2f_t( 200.0f, 600.0f ) ;
                        _player.comp.set_shape( ++_player.comp.cur_shape%5, _player.pos, _dims ) ;
                        return ;
                    }
                }

                // test against the grid cells
                {
                    natus::math::vec2f_t side_ground( 1.0f ) ;

                    // test side in grid
                    {
                        for( size_t i=0; i<next.comp.shape_elems; ++i )
                        {
                            auto const p = next.comp.shape[i] ;

                            natus::math::vec2f_t const cell = (p/ _dims).floored() ;
                            size_t const x = size_t( cell.x() ) ;
                            size_t const y = size_t( cell.y() ) ;
                        
                            if( y >= _level.h ) continue ;

                            if( _level.layout[y * _level.w + x] != 0 )
                            {
                                side_ground.x( 0.0f ) ;
                                break ;
                            }
                        }

                        // do correction
                        {
                            next.comp.adv *= side_ground ;
                            next.comp.adv2 *= side_ground ;
                            auto const adv = next.comp.adv + next.comp.adv2 ;
                            next.pos = _player.pos + adv * dt ;
                            next.comp.set_shape( _player.comp.cur_shape, next.pos, _dims ) ;
                        }
                    }

                    // test hit in grid
                    {
                        for( size_t i=0; i<next.comp.shape_elems; ++i )
                        {
                            auto const p = next.comp.shape[i] ;

                            natus::math::vec2f_t const cell = (p/ _dims).floored() ;
                            size_t const x = size_t( cell.x() ) ;
                            size_t const y = size_t( cell.y() ) ;
                        
                            if( y >= _level.h ) continue ;

                            if( _level.layout[y * _level.w + x] != 0 )
                            {
                                side_ground.y( 0.0f ) ;
                                break ;
                            }
                        }

                        // do correction
                        {
                            next.comp.adv *= side_ground ;
                            next.comp.adv2 *= side_ground ;
                            auto const adv = next.comp.adv + next.comp.adv2 ;
                            next.pos = _player.pos + adv * dt ;
                            next.comp.set_shape( _player.comp.cur_shape, next.pos, _dims ) ;
                        }

                        if( side_ground.y() < 0.5f )
                        {
                            for( size_t i=0; i<next.comp.shape_elems; ++i )
                            {
                                auto const p = next.comp.shape[i] ;
                                natus::math::vec2f_t const cell = (p / _dims).floored() ;
                                size_t const x = size_t( cell.x() ) ;
                                size_t const y = size_t( cell.y() ) ;
                                _level.layout[ y * _level.w + x ] = 1 ;
                            }
                        
                            _player.pos = natus::math::vec2f_t( 200.0f, 600.0f ) ;
                            _player.comp.set_shape( ++_player.comp.cur_shape%5, _player.pos, _dims ) ;
                            return ;
                        }
                    }
                }

                {
                    _player.pos = next.pos ;
                    _player.comp.set_shape( _player.comp.cur_shape, _player.pos, _dims ) ;
                }
            }

            //********************************************************************************
            void_t on_audio( natus::audio::async_access_t audio ) noexcept
            {
                for( auto & b : _audio_play_queue )
                {
                    natus::audio::backend::execute_detail ed ;
                    ed.to = b.eo ;
                    ed.loop = b.loop ;
                    audio.execute( b.buffer, ed ) ;
                }
                _audio_play_queue.clear() ;
            }

            //********************************************************************************
            void_t on_graphics( natus::gfx::primitive_render_2d_res_t pr, size_t const milli_dt ) noexcept
            {
                if( !_is_init ) return ;
                
                
                natus::math::vec2f_t const menu_space( 800.0f-_game_space.x(), 600.0f ) ;

                // draw right
                {
                    natus::math::vec2f_t pos = natus::math::vec2f_t( 800.0f, 600.0f ) * natus::math::vec2f_t( -0.5f ) +
                        _game_space * natus::math::vec2f_t( 1.0f, 0.0f ) ;

                    pr->draw_rect( 6, 
                        pos + menu_space * natus::math::vec2f_t( -0.0f, -0.0f ),
                        pos + menu_space * natus::math::vec2f_t( -0.0f, +1.0f ),
                        pos + menu_space * natus::math::vec2f_t( +1.0f, +1.0f ),
                        pos + menu_space * natus::math::vec2f_t( +1.0f, -0.0f ),
                        natus::math::vec4f_t(0.0f, 0.0f, 0.0f, 1.0f),
                        natus::math::vec4f_t(0.0f, 0.0f, 0.0f, 1.0f)
                    ) ;
                }

                // draw field
                {
                    natus::math::vec2f_t pos = natus::math::vec2f_t( 800.0f, 600.0f ) * natus::math::vec2f_t( -0.5f ) ;

                    natus::math::vec4f_t const colors[] = {
                        natus::math::vec4f_t(0.9f, 0.9f, 0.9f, 1.0f),
                        natus::math::vec4f_t(1.0f, 0.0f, .00f, 1.0f)
                    } ;

                    natus::math::vec4f_t const borders[] = {
                        natus::math::vec4f_t(0.0f, 0.0f, 0.0f, 1.0f),
                        natus::math::vec4f_t(0.0f, 0.0f, 0.0f, 1.0f)
                    } ;

                    for( size_t y=0; y<_level.h; ++y )
                    {
                        for( size_t x=0; x<_level.w; ++x )
                        {
                            size_t const i = y * _level.w + x ;

                            pr->draw_rect( 5, 
                                pos + _dims * natus::math::vec2f_t( -0.0f, -0.0f ),
                                pos + _dims * natus::math::vec2f_t( -0.0f, +1.0f ),
                                pos + _dims * natus::math::vec2f_t( +1.0f, +1.0f ),
                                pos + _dims * natus::math::vec2f_t( +1.0f, -0.0f ),
                                colors[_level.layout[i]], borders[_level.layout[i]]
                            ) ;
                            pos += natus::math::vec2f_t( _dims.x(), 0.0f ) ;
                        }
                        pos = natus::math::vec2f_t( -800.0f * 0.5f, pos.y() ) ;
                        pos += natus::math::vec2f_t( 0.0f, _dims.y() ) ;
                    }
                }

                // draw player
                {
                    #if 0
                    natus::math::vec2f_t const cell = (_player.pos / _dims).floored() ;
                    natus::math::vec2f_t pos = natus::math::vec2f_t( 800.0f, 600.0f ) * natus::math::vec2f_t( -0.5f ) + cell * _dims ;
                    _player.comp.set_shape( _player.comp.cur_shape, pos, _dims ) ;
                    #endif

                    #if 1
                    for( size_t i=0; i<_player.comp.shape_elems; ++i )
                    {
                        natus::math::vec2f_t const pcell = (_player.comp.shape[i] / _dims).floored() ;
                        natus::math::vec2f_t pos = natus::math::vec2f_t( 800.0f, 600.0f ) * natus::math::vec2f_t( -0.5f ) + pcell * _dims ;

                        pr->draw_rect( 6, 
                        pos + _dims * natus::math::vec2f_t( -0.0f, -0.0f ),
                        pos + _dims * natus::math::vec2f_t( -0.0f, +1.0f ),
                        pos + _dims * natus::math::vec2f_t( +1.0f, +1.0f ),
                        pos + _dims * natus::math::vec2f_t( +1.0f, -0.0f ),
                        natus::math::vec4f_t(1.0f, 0.0f, 0.0f, 1.0f),
                        natus::math::vec4f_t(1.0f, 0.0f, 0.0f, 1.0f) ) ;
                    }
                    
                    #else

                    pr->draw_rect( 6, 
                        pos + _dims * natus::math::vec2f_t( -0.0f, -0.0f ),
                        pos + _dims * natus::math::vec2f_t( -0.0f, +1.0f ),
                        pos + _dims * natus::math::vec2f_t( +1.0f, +1.0f ),
                        pos + _dims * natus::math::vec2f_t( +1.0f, -0.0f ),
                        natus::math::vec4f_t(1.0f, 0.0f, 0.0f, 1.0f),
                        natus::math::vec4f_t(1.0f, 0.0f, 0.0f, 1.0f) ) ;

                    #endif
                    
                }
            }

            //********************************************************************************
            void_t on_debug_graphics( natus::gfx::primitive_render_2d_res_t pr, size_t const milli_dt ) noexcept
            {
                if( !_is_init ) return ;
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

    private: // private

        the_game _game ;

    private: // audio

        natus::audio::async_access_t _audio ;

    public:

        game_app( void_t ) 
        {
            natus::application::app::window_info_t wi ;
            #if 1
            auto view1 = this_t::create_window( "Tetrix", wi ) ;
            auto view2 = this_t::create_window( "Tetrix", wi,
                { natus::graphics::backend_type::gl3, natus::graphics::backend_type::d3d11}) ;

            view1.window().position( 50, 50 ) ;
            view1.window().resize( 800, 600 ) ;
            view2.window().position( 50 + 800, 50 ) ;
            view2.window().resize( 800, 600 ) ;

            _graphics = natus::graphics::async_views_t( { view1.async(), view2.async() } ) ;

            //view1.window().vsync( false ) ;
            //view2.window().vsync( false ) ;
            #elif 0
            auto view1 = this_t::create_window( "Tetrix", wi, 
                { natus::graphics::backend_type::gl3, natus::graphics::backend_type::d3d11 } ) ;
            _graphics = natus::graphics::async_views_t( { view1.async() } ) ;
            #else
            auto view1 = this_t::create_window( "Tetrix", wi ) ;
            _graphics = natus::graphics::async_views_t( { view1.async() } ) ;
            view1.window().vsync( true ) ;
            #endif

            _db = natus::io::database_t( natus::io::path_t( DATAPATH ), "./working", "data" ) ;
            _audio = this_t::create_audio_engine() ;
        }
        game_app( this_cref_t ) = delete ;
        game_app( this_rref_t rhv ) noexcept : app( ::std::move( rhv ) ) 
        {
            _camera_0 = std::move( rhv._camera_0 ) ;
            _camera_1 = std::move( rhv._camera_1 ) ;
            _graphics = std::move( rhv._graphics ) ;
            _db = std::move( rhv._db ) ; 

            _sr = std::move( rhv._sr ) ;
            _pr = std::move( rhv._pr ) ;
            _tr = std::move( rhv._tr ) ;

            _fb = std::move( rhv._fb ) ;

            _game = std::move( rhv._game ) ;

            _audio = std::move( rhv._audio ) ;
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
                id.audio = _audio ;

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
                    
                    {
                        auto const res = m.insert( ica_t::action_a,
                            b_t::layout_t::ascii_key_to_input_component( b_t::layout_t::ascii_key::q ) ) ;
                        natus::log::global_t::warning( natus::core::is_not( res ), "can not do mapping." ) ;
                    }

                    {
                        auto const res = m.insert( ica_t::action_b,
                            b_t::layout_t::ascii_key_to_input_component( b_t::layout_t::ascii_key::e ) ) ;
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
            _game.on_logic( d.micro_dt / 1000 ) ;

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
            _game.on_audio( _audio ) ;
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
                    natus::math::vec4f_t(1.0f), "Tetrix" ) ;
            }

            {
                _game.on_graphics( _pr, rdi.micro_dt / 1000) ;
                
                _tr->draw_text( 0, 0, 10, natus::math::vec2f_t(-.85f, 0.7f), 
                    natus::math::vec4f_t(1.0f), std::to_string( _game.get_score()) ) ;
            
            }

            if( _draw_debug )
            {
                _game.on_debug_graphics( _pr, rdi.milli_dt ) ;
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
 