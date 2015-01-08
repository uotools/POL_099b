/*
History
=======

Notes
=======
Make sure to set the module up in scrsched.cpp too
add_common_exmods()

*/

#ifndef STORAGEEMOD_H
#define STORAGEEMOD_H

#include "../../bscript/execmodl.h"
namespace Pol {
  namespace Module {
	class StorageExecutorModule : public Bscript::TmplExecutorModule<StorageExecutorModule>
	{
	public:
      StorageExecutorModule( Bscript::Executor& exec ) :
        Bscript::TmplExecutorModule<StorageExecutorModule>( "Storage", exec ) {};

      Bscript::BObjectImp* mf_StorageAreas( );
      Bscript::BObjectImp* mf_DestroyRootItemInStorageArea( );
      Bscript::BObjectImp* mf_FindStorageArea( );
      Bscript::BObjectImp* mf_CreateStorageArea( );
      Bscript::BObjectImp* mf_FindRootItemInStorageArea( );
      Bscript::BObjectImp* mf_CreateRootItemInStorageArea( );
	};
  }
}
#endif
