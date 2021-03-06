/*
History
=======

2009/08/06 MuadDib:   Added gotten_by code for items.
2009/08/25 Shinigami: STLport-5.2.1 fix: init order changed of gotten_by, layer and slot_index_
2009/09/01 MuadDib:   Removed layer setting on item creation for mounts. Pointless.

Notes
=======

*/

#include "item.h"
#include "itemdesc.h"
#include "../gameclck.h"
#include "../layers.h"
#include "../resource.h"
#include "../objtype.h"
#include "../uofile.h"
#include "../ustruct.h"
#include "../globals/state.h"

#include <climits>

namespace Pol {
  namespace Items {
	Item::Item( const ItemDesc& id, UOBJ_CLASS uobj_class ) :
	  UObject( id.objtype, uobj_class ),
	  container( NULL ),
	  gotten_by( NULL ),
	  decayat_gameclock_( 0 ),
	  sellprice_( UINT_MAX ), //dave changed 1/15/3 so 0 means 0, not default to itemdesc value
	  buyprice_( UINT_MAX ),  //dave changed 1/15/3 so 0 means 0, not default to itemdesc value
	  amount_( 1 ),
	  newbie_( id.newbie ),
	  movable_( id.default_movable() ),
	  inuse_( false ),
	  is_gotten_( 0 ),
	  invisible_( id.invisible ),
	  slot_index_( 0 ),
	  _itemdesc( nullptr ),
	  layer( 0 ),
	  hp_( id.maxhp ),
	  quality_( id.quality )
	{
	  graphic = id.graphic;
	  color = id.color;
	  setfacing( id.facing );
	  equip_script_ = id.equip_script;
	  unequip_script_ = id.unequip_script;

	  ++Core::stateManager.uobjcount.uitem_count;

	  // hmm, doesn't quite work right with items created on startup..
      decayat_gameclock_ = Core::read_gameclock( ) + id.decay_time * 60;

	  // FIXME : Need to change this to it's own function like Character Class does.
	  // Let's build the resistances defaults.
      setBaseResistance( Core::ELEMENTAL_FIRE, 0 );
      setBaseResistance( Core::ELEMENTAL_COLD, 0 );
      setBaseResistance( Core::ELEMENTAL_ENERGY, 0 );
      setBaseResistance( Core::ELEMENTAL_POISON, 0 );
      setBaseResistance( Core::ELEMENTAL_PHYSICAL, 0 );
      setResistanceMod( Core::ELEMENTAL_FIRE, 0 );
      setResistanceMod( Core::ELEMENTAL_COLD, 0 );
      setResistanceMod( Core::ELEMENTAL_ENERGY, 0 );
      setResistanceMod( Core::ELEMENTAL_POISON, 0 );
      setResistanceMod( Core::ELEMENTAL_PHYSICAL, 0 );

      setBaseElementDamage( Core::ELEMENTAL_FIRE, 0 );
      setBaseElementDamage( Core::ELEMENTAL_COLD, 0 );
      setBaseElementDamage( Core::ELEMENTAL_ENERGY, 0 );
      setBaseElementDamage( Core::ELEMENTAL_POISON, 0 );
      setBaseElementDamage( Core::ELEMENTAL_PHYSICAL, 0 );
      setElementDamageMod( Core::ELEMENTAL_FIRE, 0 );
      setElementDamageMod( Core::ELEMENTAL_COLD, 0 );
      setElementDamageMod( Core::ELEMENTAL_ENERGY, 0 );
      setElementDamageMod( Core::ELEMENTAL_POISON, 0 );
      setElementDamageMod( Core::ELEMENTAL_PHYSICAL, 0 );
	}

	Item::~Item()
	{
      --Core::stateManager.uobjcount.uitem_count;
      return_resources( objtype_, amount_ );
	}

    size_t Item::estimatedSize() const
    {
      return base::estimatedSize()
        + sizeof( Core::UContainer* )/* container*/
        + sizeof( Mobile::Character* )/* gotten_by*/
        + sizeof(int)/* decayat_gameclock_*/
        +sizeof(int)/* sellprice_*/
        +sizeof(int)/* buyprice_*/
        +sizeof(u16)/* amount_*/
        +sizeof(bool)/* newbie_*/
        +sizeof(bool)/* movable_*/
        +sizeof(bool)/* inuse_*/
        +sizeof(bool)/* is_gotten_*/
        +sizeof(bool)/* invisible_*/
        +sizeof(u8)/* slot_index_*/
        +sizeof(const ItemDesc *)/* _itemdesc*/
        +sizeof(u8)/* layer*/
        +sizeof(u8)/* tile_layer*/
        +sizeof(unsigned short)/* hp_*/
        +sizeof(double)/* quality_*/
        +sizeof( boost_utils::script_name_flystring ) /*on_use_script_*/
        +sizeof( boost_utils::script_name_flystring ) /*equip_script_*/
        +sizeof( boost_utils::script_name_flystring ); /*unequip_script_*/
    }
  }
}