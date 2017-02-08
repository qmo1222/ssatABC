/**CFile****************************************************************

  FileName    [ssatBranchBound.cc]

  SystemName  [ssatQesto]

  Synopsis    [Branch and bound method]

  Author      [Nian-Ze Lee]
  
  Affiliation [NTU]

  Date        [8, Feb., 2017]

***********************************************************************/

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <stdio.h>
#include "ssat/utils/ParseUtils.h"
#include "ssat/core/SolverTypes.h"
#include "ssat/core/Solver.h"
#include "ssat/core/Dimacs.h"
#include "ssat/core/SsatSolver.h"

using namespace Minisat;
using namespace std;

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [All-sat model counting entrance point.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void
SsatSolver::solveBranchBound( Abc_Ntk_t * pNtk )
{
   vec<Lit> eLits;
   Abc_Obj_t * pObj;
   double tempValue = 0.0;
   int i;

   _satPb = 0.0;
   _s1 = ntkBuildSolver( pNtk , true ); // assert Po to be false
   ntkBuildPrefix( pNtk );
   // TODO: initialize eLits to 1111..1111
   do {
      tempValue = allSatModelCount( _s1 , eLits , _satPb );
      if ( _satPb < tempValue ) {
         printf( "  > find better solution! (%f)\n" , tempValue );
         _satPb = tempValue;
         eLits.copyTo( _erModel );
      }
   } while ( binaryDecrement( eLits ) );
   printf( "\n  > optimized value: %f\n" , _satPb );
   printf( "  > optimizing assignment to exist vars:\n" );
   dumpCla( _erModel );
}

/**Function*************************************************************

  Synopsis    [Build SAT solver from pNtk.]

  Description [Assume negative output for branch and bound.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

Solver*
SsatSolver::ntkBuildSolver( Abc_Ntk_t * pNtk , bool negative )
{
   Abc_Obj_t * pObj;
   int i;
   Solver * s = new Solver;

   Abc_NtkForEachObj( pNtk , pObj , i )
      while ( s->nVars() <= Abc_ObjId(pObj) ) s->newVar();
   Abc_NtkForEachNode( pNtk , pObj , i )
   {
      // (c v -a v -b)
      s->addClause( mkLit( Abc_ObjId(pObj)       , false ) , 
                    mkLit( Abc_ObjFaninId0(pObj) , !Abc_ObjFaninC0(pObj) ) ,
                    mkLit( Abc_ObjFaninId1(pObj) , !Abc_ObjFaninC1(pObj) ) );
      // (-c v a)
      s->addClause( mkLit( Abc_ObjId(pObj)       , true ) , 
                    mkLit( Abc_ObjFaninId0(pObj) , Abc_ObjFaninC0(pObj) ) );
      // (-c v b)
      s->addClause( mkLit( Abc_ObjId(pObj)       , true ) , 
                    mkLit( Abc_ObjFaninId1(pObj) , Abc_ObjFaninC1(pObj) ) );
   }
   s->addClause( mkLit( Abc_ObjFaninId0(Abc_NtkPo(pNtk,0)) , negative ^ Abc_ObjFaninC0(Abc_NtkPo(pNtk,0)) ) );
   return s;
}

/**Function*************************************************************

  Synopsis    [Build prefix structure from pNtk.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void
SsatSolver::ntkBuildPrefix( Abc_Ntk_t * pNtk )
{
   Abc_Obj_t * pObj;
   int i;

   for ( int i = 0 ; i < 3 ; ++i ) _rootVars.push();
   _rootVars[0].capacity( Abc_NtkPiNum(pNtk)   );
   _rootVars[1].capacity( Abc_NtkPiNum(pNtk)   );
   _rootVars[2].capacity( Abc_NtkNodeNum(pNtk) );

   _quan.growTo ( Abc_NtkObjNumMax(pNtk) );
   _level.growTo( Abc_NtkObjNumMax(pNtk) );
   Abc_NtkForEachPi( pNtk , pObj , i )
   {
      (pObj->dTemp == -1) ? _rootVars[0].push(Abc_ObjId(pObj)): 
                            _rootVars[1].push(Abc_ObjId(pObj));
      _quan[Abc_ObjId(pObj)]  = pObj->dTemp;
      _level[Abc_ObjId(pObj)] = (pObj->dTemp == -1) ? 0 : 1;
   }
   Abc_NtkForEachNode( pNtk , pObj , i )
   {
      _rootVars[2].push(Abc_ObjId(pObj));
      _quan[Abc_ObjId(pObj)]  = -1;
      _level[Abc_ObjId(pObj)] = 2;
   }
}

/**Function*************************************************************

  Synopsis    [Iterate all exist assignments.]

  Description [Decrement 1, return false if eLits = 00...00.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

bool
SsatSolver::binaryDecrement( vec<Lit> & eLits ) const
{
   // TODO: decrement exist assignment
   return false;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
