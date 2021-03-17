
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
#include <natus/device/layouts/three_mouse.hpp>
#include <natus/device/mapping.hpp>
#include <natus/device/global.h>

#include <natus/gfx/camera/pinhole_camera.h>
#include <natus/gfx/sprite/sprite_render_2d.h>
#include <natus/gfx/primitive/primitive_render_2d.h>
#include <natus/gfx/font/text_render_2d.h>
#include <natus/gfx/util/quad.h>

#include <natus/graphics/variable/variable_set.hpp>
#include <natus/profiling/macros.h>

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

    struct sprite_sheet
    {
        struct animation
        {
            struct sprite
            {
                size_t idx ;
                size_t begin ;
                size_t end ;
            };
            size_t duration ;
            natus::ntd::string_t name ;
            natus::ntd::vector< sprite > sprites ;

        };
        natus::ntd::vector< animation > animations ;

        struct sprite
        {
            natus::math::vec4f_t rect ;
            natus::math::vec2f_t pivot ;
        };
        natus::ntd::vector< sprite > rects ;

        sprite determine_sprite( size_t const ani_id, size_t const milli_time ) const noexcept
        {
            for( auto const & s : animations[ani_id].sprites )
            {
                if( milli_time >= s.begin && milli_time < s.end )
                {
                    return rects[s.idx] ;
                }
            }

            return sprite() ;
        }
    };
    natus_res_typedefs( natus::ntd::vector< sprite_sheet >, sheets ) ;

    //
    //
    //
    //////////////////////////////////////////////////////////////////////////
    class field
    {
        natus_this_typedefs( field ) ;

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

    private: // intruders

        struct intruder
        {
            bool_t life = true ;

            natus::math::vec2f_t pos ;
            natus::math::vec4f_t rect ;

            size_t ani_id = size_t( -1 ) ;
            size_t anim_time = 0 ;
            size_t max_ani_time = 1 ;

            // the currently animated sprite
            space_intruders::sprite_sheet::sprite cur_sprite ;
            float_t scale = 3000.0f ;

            bounding_box_2d_t get_bb( void_t ) const noexcept
            {
                auto const rdims = cur_sprite.rect.zw() - cur_sprite.rect.xy() ;
                auto const p0 =  natus::math::vec2f_t(-0.5f, -0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p1 =  natus::math::vec2f_t(-0.5f, +0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p2 =  natus::math::vec2f_t(+0.5f, +0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p3 =  natus::math::vec2f_t(+0.5f, -0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;

                return { p0, p1, p2, p3 } ;
            }
        };
        natus_typedef( intruder ) ;

        size_t _intruders_w = 10 ;
        size_t _intruders_h = 6 ;

        natus::ntd::vector< intruder_t > _intruders ;
        natus::math::vec2f_t _intruders_offset = natus::math::vec2f_t(0.0f, 0.0f) ;
        natus::math::vec2f_t _intruders_speed = natus::math::vec2f_t( 100.0f, 100.0f ) ;

        natus::math::vec2f_t _intruders_dir = natus::math::vec2f_t( 1.0f, -1.0f ) ;

        std::chrono::milliseconds _intruders_physics_dur = std::chrono::milliseconds( 1000 ) ;
        clock_t::time_point _intruders_physics_tp ;

        std::chrono::milliseconds _intruders_shoot_dur = std::chrono::milliseconds( 1642 ) ;
        clock_t::time_point _intruders_shoot_tp ;

    private: // projectile

        struct projectile
        {
            natus::math::vec2f_t pos ;

            // direction
            natus::math::vec2f_t adv ;

            size_t ani_id = size_t( -1 ) ;
            size_t anim_time = 0 ;
            size_t max_ani_time = 1 ;

            // 0: none
            // 1: player
            // 2: intruder
            size_t from = 0 ;

            // the currently animated sprite
            space_intruders::sprite_sheet::sprite cur_sprite ;
            float_t scale = 2500.0f ;

            bounding_box_2d_t get_bb( void_t ) const noexcept
            {
                auto const rdims = cur_sprite.rect.zw() - cur_sprite.rect.xy() ;
                auto const p0 =  natus::math::vec2f_t(-0.5f, -0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p1 =  natus::math::vec2f_t(-0.5f, +0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p2 =  natus::math::vec2f_t(+0.5f, +0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p3 =  natus::math::vec2f_t(+0.5f, -0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;

                return { p0, p1, p2, p3 } ;
            }
        };
        // templates
        natus::ntd::vector< projectile > _projectiles ;

        // the actual currently shot projectiles
        natus::ntd::vector< projectile > _shots ;

    private: // ufo

        struct ufo
        {
            clock_t::time_point tp ;
            std::chrono::milliseconds dur = std::chrono::milliseconds(10000) ;
            bool_t do_appear = false ;

            natus::math::vec2f_t pos ;
            
            size_t ani_id = size_t(-1) ;
            size_t anim_time = 0 ;
            size_t max_ani_time = 1 ;

            // the currently animated sprite
            space_intruders::sprite_sheet::sprite cur_sprite ;

            float_t scale = 3000.0f ;

            bounding_box_2d_t get_bb( void_t ) const noexcept
            {
                auto const rdims = cur_sprite.rect.zw() - cur_sprite.rect.xy() ;
                auto const p0 =  natus::math::vec2f_t(-0.5f, -0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p1 =  natus::math::vec2f_t(-0.5f, +0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p2 =  natus::math::vec2f_t(+0.5f, +0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p3 =  natus::math::vec2f_t(+0.5f, -0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;

                return { p0, p1, p2, p3 } ;
            }
        } ;

        ufo _ufo ;

        std::chrono::milliseconds _ufo_physics_dur = std::chrono::milliseconds( 5000 ) ;
        clock_t::time_point _ufo_physics_tp ;
        natus::math::vec2f_t _ufo_dir = natus::math::vec2f_t( 1.0f, 0.0f ) ;
        bool_t _ufo_spawned = false ;

    private: // player

        struct player
        {
            size_t num_lifes = 3 ;
            natus::math::vec2f_t pos ;

            // direction
            natus::math::vec2f_t adv ;

            size_t ani_id = size_t( -1 ) ;
            size_t anim_time = 0 ;
            size_t max_ani_time = 1 ;

            // the currently animated sprite
            space_intruders::sprite_sheet::sprite cur_sprite ;

            float_t scale = 1500.0f ;

            bounding_box_2d_t get_bb( void_t ) const noexcept
            {
                auto const rdims = cur_sprite.rect.zw() - cur_sprite.rect.xy() ;
                auto const p0 =  natus::math::vec2f_t(-0.5f, -0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p1 =  natus::math::vec2f_t(-0.5f, +0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p2 =  natus::math::vec2f_t(+0.5f, +0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p3 =  natus::math::vec2f_t(+0.5f, -0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;

                return { p0, p1, p2, p3 } ;
            }
        } ;
        player _player ;

    private: // defense

        struct defense
        {
            size_t ani_id = size_t(-1) ;
            size_t hits = 0 ;

            natus::math::vec2f_t pos ;

            // the currently animated sprite
            space_intruders::sprite_sheet::sprite cur_sprite ;

            float_t scale = 2500.0f ;

            bounding_box_2d_t get_bb( void_t ) const noexcept
            {
                auto const rdims = cur_sprite.rect.zw() - cur_sprite.rect.xy() ;
                auto const p0 =  natus::math::vec2f_t(-0.5f, -0.5f) * rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p1 =  natus::math::vec2f_t(-0.5f, +0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p2 =  natus::math::vec2f_t(+0.5f, +0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;
                auto const p3 =  natus::math::vec2f_t(+0.5f, -0.5f) *rdims * natus::math::vec2f_t(scale) + pos ;

                return { p0, p1, p2, p3 } ;
            }
        } ;

        natus::ntd::vector< defense > _defenses ;

    private: // graphics

        size_t _anim = 0 ;
        

    public:

        struct init_data
        {
            sheets_res_t sheets ;
        };
        natus_typedef( init_data ) ;

    public: // ctors

        field( void_t ) noexcept
        {
        }

        field( this_cref_t ) = delete ;
        field( this_rref_t rhv ) noexcept
        {
            _anim = rhv._anim ;
            _intruders_w = rhv._intruders_w ;
            _intruders_h = rhv._intruders_h ;
            _intruders = std::move( rhv._intruders ) ;
        }

        this_ref_t operator = ( this_rref_t rhv ) noexcept
        {
            _anim = rhv._anim ;
            _intruders_w = rhv._intruders_w ;
            _intruders_h = rhv._intruders_h ;
            _intruders = std::move( rhv._intruders ) ;
            return *this ;
        }

    public: // functions

        bool_t on_init( init_data_rref_t d ) noexcept
        {
            auto const & sheets = *d.sheets ;
            if( sheets.size() == 0 ) return false ;

            auto const & sheet = sheets[0] ;

            // intruders
            {
                natus::ntd::vector< natus::ntd::string_t > names = 
                { "intr_5_move", "intr_4_move", "intr_3_move", "intr_2_move", "intr_1_move", "intr_0_move" } ;
                
                for( auto const & name : names )
                {
                    this_t::intruder_t intr ;

                    auto const iter = std::find_if( sheet.animations.begin(), sheet.animations.end(), 
                        [&]( space_intruders::sprite_sheet::animation const & a )
                    {
                        return a.name == name ;
                    } ) ;
                    if( iter == sheet.animations.end() )
                    {
                        natus::log::global_t::error( "Can not find animation [" + name + "]. Taking 0" ) ;
                        for( size_t i=0; i<_intruders_w; ++i )
                            _intruders.emplace_back( intr ) ;
                        continue ;
                    }
                    intr.ani_id = std::distance( sheet.animations.begin(), iter ) ;
                    intr.max_ani_time = iter->duration ;
                    for( size_t i=0; i<_intruders_w; ++i )
                        _intruders.emplace_back( intr) ;
                }

                // init positions
                {
                    auto const start = natus::math::vec2f_t( -350.0f, 200.0f ) ;
                    for( size_t i=0; i<_intruders.size(); ++i )
                    {
                        size_t const y = i / _intruders_w ;
                        size_t const x = i % _intruders_w ;

                        auto & intr = _intruders[i] ;
                        intr.pos = start + natus::math::vec2f_t( 
                            float_t(x) * (800.0f/20.0f), 
                            -float_t(y) * (600.0f/10.0f) ) ;
                    }
                }
            }

            // projectiles
            {
                natus::ntd::vector< natus::ntd::string_t > names = 
                { "projectile_0", "projectile_1", "projectile_2", "projectile_3"} ;
                
                for( auto const & name : names )
                {
                    this_t::projectile proj ;

                    auto const iter = std::find_if( sheet.animations.begin(), sheet.animations.end(), 
                        [&]( space_intruders::sprite_sheet::animation const & a )
                    {
                        return a.name == name ;
                    } ) ;
                    if( iter == sheet.animations.end() )
                    {
                        natus::log::global_t::error( "Can not find animation [" + name + "]. Taking 0" ) ;
                        for( size_t i=0; i<_intruders_w; ++i )
                            _projectiles.emplace_back( proj ) ;
                        continue ;
                    }
                    proj.ani_id = std::distance( sheet.animations.begin(), iter ) ;
                    proj.max_ani_time = iter->duration ;
                    for( size_t i=0; i<_intruders_w; ++i )
                        _projectiles.emplace_back( proj) ;
                }
            }

            // ufo
            {
                natus::ntd::string_t name = "ufo_move" ;
                auto const iter = std::find_if( sheet.animations.begin(), sheet.animations.end(), 
                    [&]( space_intruders::sprite_sheet::animation const & a )
                {
                    return a.name == name ;
                } ) ;
                if( iter == sheet.animations.end() )
                {
                    natus::log::global_t::error( "Can not find animation [" + name + "]. Taking 0" ) ;
                }
                else
                {
                    _ufo.ani_id = std::distance( sheet.animations.begin(), iter ) ;
                    _ufo.max_ani_time = iter->duration ;
                }
                _ufo.tp = clock_t::now() ;
            }

            // player
            {
                natus::ntd::string_t name = "player" ;
                auto const iter = std::find_if( sheet.animations.begin(), sheet.animations.end(), 
                    [&]( space_intruders::sprite_sheet::animation const & a )
                {
                    return a.name == name ;
                } ) ;
                if( iter == sheet.animations.end() )
                {
                    natus::log::global_t::error( "Can not find animation [" + name + "]. Taking 0" ) ;
                }
                else
                {
                    _player.ani_id = std::distance( sheet.animations.begin(), iter ) ;
                    _player.max_ani_time = iter->duration ;
                }

                {
                    _player.pos = natus::math::vec2f_t( -400.0f, -250.0f ) ;
                }
            }

            // defense
            {
                natus::ntd::string_t name = "defense" ;
                auto const iter = std::find_if( sheet.animations.begin(), sheet.animations.end(), 
                    [&]( space_intruders::sprite_sheet::animation const & a )
                {
                    return a.name == name ;
                } ) ;
                if( iter == sheet.animations.end() )
                {
                    natus::log::global_t::error( "Can not find animation [" + name + "]. Taking 0" ) ;
                }
                else
                {
                    defense d ;
                    d.ani_id = std::distance( sheet.animations.begin(), iter ) ;
                    
                    natus::math::vec2f_t pos( -300.0f, -200.0f ) ;

                    for( size_t i=0; i<6; ++i )
                    {
                        d.pos = pos ;
                        pos += natus::math::vec2f_t( 800.0f/7.0f, 0.0f ) ;
                        _defenses.emplace_back( d ) ;
                    }
                }
            }

            {
                _intruders_physics_tp = clock_t::now() ;
                _ufo_physics_tp = clock_t::now() ;
                _intruders_shoot_tp = clock_t::now() ;
            }
            return true ;
        }

        void_t on_update( void_t ) noexcept
        {
            //auto const dt = std::chrono::milliseconds( milli_dt ) ;
        }

        void_t on_device( natus::device::game_device_res_t dev ) noexcept
        {
            using ctrl_t = natus::device::layouts::game_controller_t ;
            ctrl_t ctrl( dev ) ;

            natus::math::vec2f_t value ;
            if( ctrl.is( ctrl_t::directional::movement, natus::device::components::stick_state::tilting, value ) )
            {
                _player.adv = value ;
            }
            if( ctrl.is( ctrl_t::directional::movement, natus::device::components::stick_state::untilted, value ) )
            {
                _player.adv = natus::math::vec2f_t() ;
            }

            {
                float_t bnt_value = 0.0f ;
                if( ctrl.is( ctrl_t::button::shoot, natus::device::components::button_state::pressed, bnt_value ) )
                {
                    auto s = _projectiles[0] ;
                    s.adv = natus::math::vec2f_t( 0.0f, 1.0f ) ;
                    s.pos = _player.pos ;
                    s.from = 1 ;
                    _shots.emplace_back( s ) ;
                }
            }
        }

        void_t on_logic( sheets_cref_t sheets, size_t const milli_dt ) noexcept 
        {
            size_t const sheet = 0 ;

            // projectiles
            {
                for( auto & shot : _shots )
                {
                    shot.cur_sprite = sheets[sheet].determine_sprite( shot.ani_id, shot.anim_time ) ;
                    shot.anim_time += milli_dt ;
                    shot.anim_time = shot.anim_time % shot.max_ani_time ;
                }
            }

            // intruders
            {
                for( size_t y=0; y<_intruders_h; ++y )
                {
                    for( size_t x=0; x<_intruders_w; ++x )
                    {
                        size_t const idx = y * _intruders_w + x ;
                        auto & intr = _intruders[ idx ] ;

                        intr.cur_sprite = sheets[sheet].determine_sprite( intr.ani_id, intr.anim_time ) ;
                        intr.anim_time += milli_dt ;
                        intr.anim_time = intr.anim_time % intr.max_ani_time ;
                    }
                }
            }

            // ufo
            if( _ufo.ani_id != size_t(-1) )
            {
                _ufo.cur_sprite = sheets[sheet].determine_sprite( _ufo.ani_id, _ufo.anim_time ) ;
                _ufo.anim_time += milli_dt ;
                _ufo.anim_time = _ufo.anim_time % _ufo.max_ani_time ;
            }

            // player
            if( _player.ani_id != size_t(-1) )
            {
                _player.cur_sprite = sheets[sheet].determine_sprite( _player.ani_id, _player.anim_time ) ;
                _player.anim_time += milli_dt ;
                _player.anim_time = _player.anim_time % _player.max_ani_time ;
            }

            // defense
            for( auto & d : _defenses )
            {
                d.cur_sprite = sheets[sheet].determine_sprite( d.ani_id, 0 ) ;
            }

            // test intruder shoot time
            {
                if( (clock_t::now() - _intruders_shoot_tp) > _intruders_shoot_dur )
                {
                    static size_t sx = 0 ;

                    _intruders_shoot_tp = clock_t::now() ;
                    for( size_t y = _intruders_h-size_t(1); y >= 0; --y )
                    {
                        size_t const x = sx++ % _intruders_w ;
                        size_t const idx = y * _intruders_w + x ;
                        
                        if( _intruders[ idx ].life )
                        {
                            auto s = _projectiles[sx%_projectiles.size()] ;
                            s.adv = natus::math::vec2f_t( 0.0f, -1.0f ) ;
                            s.pos = _intruders[ idx ].pos ;
                            s.from = 2 ;
                            _shots.emplace_back( s ) ;
                            break ;
                        }
                    }
                }
            }

            _anim += milli_dt ;
            _anim = _anim > 5000 ? 0 : _anim ;
        }

        void_t on_physics( size_t const milli_dt ) noexcept
        {
            float_t const dt = (float_t(milli_dt) / 1000.0f) ;

            if( (clock_t::now() - _intruders_physics_tp) > _intruders_physics_dur )
            {
                _intruders_physics_tp = clock_t::now() ;

                for( auto & intr : _intruders ) 
                {
                    intr.pos +=_intruders_dir * natus::math::vec2f_t( 800.0f/20.0f, 0.0f ) ;
                }

                for( auto & intr : _intruders ) 
                {
                    if( (intr.pos.x() > (400.0f - (800.0f/10.0f))) ||
                        (intr.pos.x() < (-400.0f + (800.0f/10.0f))) )
                    {
                        _intruders_dir *= natus::math::vec2f_t( -1.0f, 1.0f ) ;
                        break ;
                    }
                }
            }

            // ufo
            {
                if( !_ufo_spawned && (clock_t::now() - _ufo_physics_tp) > _ufo_physics_dur )
                {
                    _ufo_spawned = true ;
                    if( _ufo_dir.x() < 0.0f ) _ufo.pos = natus::math::vec2f_t( 450.0f, 250.0f ) ; 
                    else _ufo.pos = natus::math::vec2f_t( -450.0f, 250.0f ) ; 
                }
            
                if( _ufo_spawned )
                {
                    _ufo.pos += _ufo_dir * natus::math::vec2f_t( 150.0f, 0.0f ) * dt ;                    
                    if( _ufo.pos.x() > 500.0f || _ufo.pos.x() < -500.0f )
                    {
                        _ufo_spawned = false ;
                        _ufo_physics_tp = clock_t::now() ;
                        _ufo_dir *= natus::math::vec2f_t( -1.0f, 1.0f ) ;
                    }
                }
            }

            // player
            {
                _player.pos += natus::math::vec2f_t( 300.0f, 0.0f ) * 
                    natus::math::vec2f_t( _player.adv.x() * dt ) ;

                if( _player.pos.x() > 390.0f || _player.pos.x() < -390.0f )
                {
                    _player.pos = natus::math::vec2f_t( 390.0f * natus::math::fn<float_t>::sign(_player.pos.x()), 
                        _player.pos.y() ) ;
                }
            }

            // projectiles
            {
                size_t end = _shots.size() ;
                for( size_t i=0; i<end; ++i )
                {
                    if( end == size_t(-1) ) break ;

                    auto & proj = _shots[i] ;

                    proj.pos += natus::math::vec2f_t( 0.0f, 400.0f ) * 
                        natus::math::vec2f_t( proj.adv ) * natus::math::vec2f_t(dt) ;

                    if( proj.pos.y() > 320.0f || _player.pos.y() < -320.0f )
                    {
                        _shots[i--] = _shots[--end] ;
                    }
                }
                _shots.resize( end ) ;
            }

            // collision testing
            {
                size_t end = _shots.size() ;
                for( size_t i=0; i<end; ++i )
                {
                    auto const p_bb = _shots[i].get_bb() ;

                    // test against defense
                    for( auto & d : _defenses )
                    {

                    }
                }
                _shots.resize( end ) ;
            }
        }

        void_t on_graphics( natus::gfx::sprite_render_2d_res_t sr, sheets_cref_t sheets, size_t const milli_dt ) noexcept
        {
            size_t const sheet = 0 ;
            
            // ufo
            for( auto & s : _shots )
            {
                sr->draw( 0, 
                    s.pos, 
                    natus::math::mat2f_t().identity(),
                    natus::math::vec2f_t(s.scale),
                    s.cur_sprite.rect,  
                    sheet, s.cur_sprite.pivot, 
                    natus::math::vec4f_t(1.0f) ) ;
            }

            // intruders
            {
                natus::math::vec4f_t colors[6] = {
                    natus::math::vec4f_t( 0.0f, 1.0f, 0.0f, 1.0f ), 
                    natus::math::vec4f_t( 0.0f, 1.0f, 0.0f, 1.0f ), 
                    natus::math::vec4f_t( 0.0f, 0.0f, 1.0f, 1.0f ),
                    natus::math::vec4f_t( 0.0f, 0.0f, 1.0f, 1.0f ), 
                    natus::math::vec4f_t( 1.0f, 0.0f, 0.0f, 1.0f ), 
                    natus::math::vec4f_t( 1.0f, 0.0f, 0.0f, 1.0f ) 
                } ;

                for( size_t y=0; y<_intruders_h; ++y )
                {
                    for( size_t x=0; x<_intruders_w; ++x )
                    {
                        size_t const idx = y * _intruders_w + x ;
                        auto const & intr = _intruders[ idx ] ;

                        sr->draw( 0, 
                            intr.pos, 
                            natus::math::mat2f_t().identity(),
                            natus::math::vec2f_t(intr.scale),
                            intr.cur_sprite.rect,  
                            sheet, intr.cur_sprite.pivot, 
                            colors[5 - (intr.ani_id % 6)] ) ;
                    }
                }
            }

            // ufo
            if( _ufo_spawned && _ufo.ani_id != size_t(-1) )
            {
                sr->draw( 0, 
                    _ufo.pos, 
                    natus::math::mat2f_t().identity(),
                    natus::math::vec2f_t(_ufo.scale),
                    _ufo.cur_sprite.rect,  
                    sheet, _ufo.cur_sprite.pivot, 
                    natus::math::vec4f_t(1.0f) ) ;
            }

            // player
            if( _player.ani_id != size_t(-1) )
            {
                sr->draw( 0, 
                    _player.pos, 
                    natus::math::mat2f_t().identity(),
                    natus::math::vec2f_t(_player.scale),
                    _player.cur_sprite.rect,  
                    sheet, _player.cur_sprite.pivot, 
                    natus::math::vec4f_t(1.0f) ) ;
            }

            // defense
            for( auto & d : _defenses )
            {
                sr->draw( 0, 
                    d.pos, 
                    natus::math::mat2f_t().identity(),
                    natus::math::vec2f_t(d.scale),
                    d.cur_sprite.rect,  
                    sheet, d.cur_sprite.pivot, 
                    natus::math::vec4f_t(1.0f) ) ;
            }
        }

        void_t on_debug_graphics( natus::gfx::primitive_render_2d_res_t pr, sheets_cref_t sheets, size_t const milli_dt ) noexcept
        {
            size_t const sheet = 0 ;

            // player box
            if( _player.ani_id != size_t(-1) )
            {
                auto const bb = _player.get_bb() ;

                natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.5f ) ;
                natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;

                pr->draw_rect( 50, bb.box[0], bb.box[1], bb.box[2], bb.box[3], color0, color1 ) ;
            }

            // ufo box
            if( _ufo_spawned && _ufo.ani_id != size_t(-1) )
            {
                auto const bb = _ufo.get_bb() ;

                natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.5f ) ;
                natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;

                pr->draw_rect( 50, bb.box[0], bb.box[1], bb.box[2], bb.box[3], color0, color1 ) ;
            }
            
            for( auto & intr : _intruders )
            {
                auto const bb = intr.get_bb() ;

                natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.5f ) ;
                natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;

                pr->draw_rect( 50, bb.box[0], bb.box[1], bb.box[2], bb.box[3], color0, color1 ) ;
            }

            for( auto & d : _defenses )
            {
                auto const bb = d.get_bb() ;

                natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.5f ) ;
                natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;

                pr->draw_rect( 50, bb.box[0], bb.box[1], bb.box[2], bb.box[3], color0, color1 ) ;
            }

            for( auto & p : _shots )
            {
                auto const bb = p.get_bb() ;

                natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.5f ) ;
                natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;

                pr->draw_rect( 50, bb.box[0], bb.box[1], bb.box[2], bb.box[3], color0, color1 ) ;
            }
        }
    };
    natus_typedef( field ) ;

    //
    //
    //
    ////////////////////////////////////////////////////////////////////////////////////
    class the_game : public natus::application::app
    {
        natus_this_typedefs( the_game ) ;

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

        natus::math::vec2f_t _screen_target = natus::math::vec2f_t( 800, 600 ) ;
        natus::math::vec2f_t _screen_current = natus::math::vec2f_t( 100, 100 ) ;
        natus::math::vec2f_t _ratio ;

        natus::gfx::quad_res_t _quad ;

    private: // tool/debug

        bool_t _do_tool = false ;
        bool_t _draw_debug = true ;

    private: // sprite tool

        natus::tool::sprite_editor_res_t _se ;
        sheets_res_t _sheets ;

    private: // game

        field_t _field ;

    public:

        the_game( void_t ) 
        {
            natus::application::app::window_info_t wi ;
            #if 1
            auto view1 = this_t::create_window( "Space Intruders", wi ) ;
            auto view2 = this_t::create_window( "Space Intruders", wi,
                { natus::graphics::backend_type::gl3, natus::graphics::backend_type::d3d11}) ;

            view1.window().position( 50, 50 ) ;
            view1.window().resize( 800, 600 ) ;
            view2.window().position( 50 + 800, 50 ) ;
            view2.window().resize( 800, 600 ) ;

            _graphics = natus::graphics::async_views_t( { view1.async(), view2.async() } ) ;
            #elif 0
            auto view1 = this_t::create_window( "Space Intruders", wi, 
                { natus::graphics::backend_type::gl3, natus::graphics::backend_type::d3d11 } ) ;
            _graphics = natus::graphics::async_views_t( { view1.async() } ) ;
            #else
            auto view1 = this_t::create_window( "Space Intruders", wi ) ;
            //view1.window().vsync( false ) ;
            _graphics = natus::graphics::async_views_t( { view1.async() } ) ;
            #endif

            _db = natus::io::database_t( natus::io::path_t( DATAPATH ), "./working", "data" ) ;

            _se = natus::tool::sprite_editor_res_t( natus::tool::sprite_editor_t( _db )  ) ;
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

            _se = std::move( rhv._se ) ;

            _field = std::move( rhv._field ) ;
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
            natus::device::xbc_device_res_t xbc_dev ;
            natus::device::ascii_device_res_t ascii_dev ;
            natus::device::three_device_res_t three_dev ;

            natus::device::global_t::system()->search( [&] ( natus::device::idevice_res_t dev_in )
            {
                if( natus::device::three_device_res_t::castable( dev_in ) )
                {
                    three_dev = dev_in ;
                    _dev_mouse = dev_in ;
                }
                else if( natus::device::ascii_device_res_t::castable( dev_in ) )
                {
                    ascii_dev = dev_in ;
                    _dev_ascii = dev_in ;
                }
            } ) ;

            if( !three_dev.is_valid() )
            {
                natus::log::global_t::status( "no three mosue found" ) ;
            }

            if( !ascii_dev.is_valid() )
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

            // framebuffer render states
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
                    rss.clear_s.ss.clear_color = natus::math::vec4f_t(0.0f,0.1f,0.1f,1.0f) ;
                   
                    rss.view_s.do_change = true ;
                    rss.view_s.ss.do_activate = true ;
                    rss.view_s.ss.vp = natus::math::vec4ui_t( 0, 0, uint_t(_screen_target.x()), 
                        uint_t(_screen_target.y()) ) ;

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
                _sheets = space_intruders::sheets_t() ;

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

                            sprite_sheet ss ;
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

                                sprite_sheet::sprite s_ ;
                                s_.rect = rect ;
                                s_.pivot = pivot ;

                                sheet.rects.emplace_back( s_ ) ;
                            }

                            for( auto const & a : ss.animations )
                            {
                                sprite_sheet::animation a_ ;

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
                                    sprite_sheet::animation::sprite s_ ;
                                    s_.begin = tp ;
                                    s_.end = tp + f.duration ;
                                    s_.idx = d ;
                                    a_.sprites.emplace_back( s_ ) ;

                                    tp = s_.end ;
                                }
                                a_.duration = tp ;
                                a_.name = a.name ;
                                sheet.animations.emplace_back( std::move( a_ ) ) ;
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

            // tool sprite
            {
                _se->add_sprite_sheet( "sprites", natus::io::location_t( "images.space_intruders.png" ) ) ;
                //_se->add_sprite_sheet( "enemies", natus::io::location_t( "images.Paper-Pixels-8x8.Enemies.png" ) ) ;
                //_se->add_sprite_sheet( "player", natus::io::location_t( "images.Paper-Pixels-8x8.Player.png" ) ) ;
                //_se->add_sprite_sheet( "tiles", natus::io::location_t( "images.Paper-Pixels-8x8.Tiles.png" ) ) ;
                
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

                #if 0
                // do mappings for xbox
                {
                    using a_t = natus::device::game_device_t ;
                    using b_t = natus::device::xbc_device_t ;

                    using ica_t = a_t::layout_t::input_component ;
                    using icb_t = b_t::layout_t::input_component ;

                    using mapping_t = natus::device::mapping< a_t, b_t > ;
                    mapping_t m( "xbc->gc", _game_dev, xbc_dev ) ;

                    {
                        auto const res = m.insert( ica_t::jump, icb_t::button_a ) ;
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
                #endif

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

            // should come last
            // field data
            {
                space_intruders::field_t::init_data_t field_init_data ;
                field_init_data.sheets = _sheets  ;
                _field.on_init( std::move( field_init_data ) ) ;
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

            _field.on_device( _game_dev ) ;

            //NATUS_PROFILING_COUNTER_HERE( "Device Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_logic( logic_data_in_t d ) noexcept 
        { 
            _field.on_logic( *_sheets, d.milli_dt ) ;

            NATUS_PROFILING_COUNTER_HERE( "Logic Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_audio( natus::application::app_t::audio_data_in_t ) 
        { 
            NATUS_PROFILING_COUNTER_HERE( "Audio Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_physics( natus::application::app_t::physics_data_in_t pd ) noexcept
        { 
            _field.on_physics( pd.micro_dt / 1000 ) ;
            NATUS_PROFILING_COUNTER_HERE( "Physics Clock" ) ;
            return natus::application::result::ok ; 
        }

        virtual natus::application::result on_graphics( natus::application::app_t::render_data_in_t rdi ) 
        { 
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

            #if 0 // test rect
            {
                auto const p0 =  natus::math::vec2f_t(-0.5f,-0.5f) * natus::math::vec2f_t(10, 90) + natus::math::vec2f_t(-400,0) ;
                auto const p1 =  natus::math::vec2f_t(-0.5f,+0.5f) * natus::math::vec2f_t(10, 90) + natus::math::vec2f_t(-400,0) ;
                auto const p2 =  natus::math::vec2f_t(+0.5f,+0.5f) * natus::math::vec2f_t(10, 90) + natus::math::vec2f_t(-400,0) ;
                auto const p3 =  natus::math::vec2f_t(+0.5f,-0.5f) * natus::math::vec2f_t(10, 90) + natus::math::vec2f_t(-400,0) ;
                natus::math::vec4f_t color0( 1.0f, 1.0f, 1.0f, 0.0f ) ;
                natus::math::vec4f_t color1( 1.0f, 1.0f, 1.0f, 1.0f ) ;
                _pr->draw_rect( 50, p0, p1, p2, p3, color0, color1 ) ;
            }
            #endif

            {
                _tr->draw_text( 0, 0, 10, natus::math::vec2f_t(-.2f, 0.7f), 
                    natus::math::vec4f_t(1.0f), "space intruders" ) ;
            }

            #if 0 // test animation
            // animation
            {
                size_t const sheet = 0 ;
                size_t const ani = 0 ;
                static size_t ani_time = 0 ;
                if( ani_time > _sheets[sheet].animations[ani].duration ) ani_time = 0 ;

                for( auto const & s : _sheets[sheet].animations[ani].sprites )
                {
                    if( ani_time >= s.begin && ani_time < s.end )
                    {
                        auto const & rect = _sheets[sheet].rects[s.idx] ;
                        natus::math::vec2f_t pos( -0.0f, 0.0f ) ;
                        _sr->draw( 0, 
                            pos, 
                            natus::math::mat2f_t().identity(),
                            natus::math::vec2f_t(7000.0f),
                            rect.rect,  
                            sheet, rect.pivot, 
                            natus::math::vec4f_t(1.0f) ) ;
                        break ;
                    }
                }

                ani_time += rdi.milli_dt ;
            }
            #endif

            {
                _field.on_graphics( _sr, *_sheets, rdi.milli_dt ) ;
            }

            if( _draw_debug )
            {
                _field.on_debug_graphics( _pr, *_sheets, rdi.milli_dt ) ;
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

                _pr->draw_rect( 10, p0, p1, p2, p3, color0, color1 ) ;
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

            _se->render( imgui ) ;

            ImGui::Begin( "do something" ) ;
            ImGui::End() ;

            bool_t tmp = true ;
            ImGui::ShowDemoWindow( &tmp ) ;
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
