/**CFile****************************************************************

  FileName    [ssatCubeToNtk.cc]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [2SSAT solving by Qesto and model counting.]

  Synopsis    [Member functions to construct circuits from cubes.]

  Author      [Nian-Ze Lee]
  
  Affiliation [NTU]

  Date        [28, Dec., 2016.]

***********************************************************************/

#include "ssat/core/SsatSolver.h"
using namespace Minisat;

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

// external functions from ABC
extern "C" {
   void Abc_NtkShow( Abc_Ntk_t * , int , int , int );
};

// helper functions
static Abc_Obj_t * Ssat_SopAnd2Obj   ( Abc_Obj_t * , Abc_Obj_t * );
static Abc_Obj_t * Ssat_SopOr2Obj    ( Abc_Obj_t * , Abc_Obj_t * );
static void        Ssat_DumpCubeNtk  ( Abc_Ntk_t * );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Construct circuits from cubes.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void
SsatSolver::cubeToNetwork() const
{
   char name[32];
   Abc_Ntk_t * pNtkCube;
   Vec_Ptr_t * vMapVars; // mapping Var to Abc_Obj_t
   Abc_Obj_t * pObjDef;  // definition of selection
   Abc_Obj_t * pObjCube; // gate of cubes
   
   pNtkCube = Abc_NtkAlloc( ABC_NTK_LOGIC , ABC_FUNC_SOP , 1 );
   sprintf( name , "qesto_cubes_network" );
   pNtkCube->pName = Extra_UtilStrsav( name );
   vMapVars = Vec_PtrStart( _s2->nVars() );

   ntkCreatePi     ( pNtkCube , vMapVars );
   pObjDef  = ntkCreateSelDef ( pNtkCube , vMapVars );
   pObjCube = ntkCreateNode   ( pNtkCube , vMapVars );
   ntkCreatePoCheck ( pNtkCube , pObjDef , pObjCube );
   ntkWriteWcnf    ();
   
   Abc_NtkDelete ( pNtkCube );
   Vec_PtrFree   ( vMapVars );
}

void
SsatSolver::ntkCreatePi( Abc_Ntk_t * pNtkCube , Vec_Ptr_t * vMapVars ) const
{
   Abc_Obj_t * pPi;
   char name[1024];
   for ( int i = 0 ; i < _rootVars[0].size() ; ++i ) {
      printf( "  > Create Pi for randome variable %d\n" , _rootVars[0][i]+1 );
      pPi = Abc_NtkCreatePi( pNtkCube );
      sprintf( name , "r%d" , _rootVars[0][i]+1 );
      Abc_ObjAssignName( pPi , name , "" );
      Vec_PtrWriteEntry( vMapVars , _rootVars[0][i] , pPi );
      printf( "  > Var %d map to Obj %s\n" , _rootVars[0][i]+1 , Abc_ObjName(pPi) );
   }
}

Abc_Obj_t*
SsatSolver::ntkCreateSelDef( Abc_Ntk_t * pNtkCube , Vec_Ptr_t * vMapVars ) const
{
   Abc_Obj_t * pObj , * pObjDef;
   vec<Lit> uLits;
   char name[1024];
   int * pfCompl = new int[_rootVars[0].size()];
   pObjDef = NULL;

   for ( int i = 0 ; i < _s1->nClauses() ; ++i ) {
      Clause & c  = _s1->ca[_s1->clauses[i]];
      uLits.clear();
      for ( int j = 0 ; j < c.size() ; ++j )
         if ( isAVar(var(c[j])) || isRVar(var(c[j])) ) uLits.push(c[j]);
      if ( uLits.size() > _rootVars[0].size() ) {
         Abc_Print( -1 , "Clause %d has %d rand lits, more than total(%d)\n" , i , uLits.size() , _rootVars[0].size() );
         exit(1);
      }
      if ( uLits.size() > 1 ) { // create gate for selection var
         pObj = Abc_NtkCreateNode( pNtkCube );
         sprintf( name , "s%d" , i );
         Abc_ObjAssignName( pObj , name , "" );
         for ( int j = 0 ; j < uLits.size() ; ++j ) {
            Abc_ObjAddFanin( pObj , (Abc_Obj_t*)Vec_PtrEntry( vMapVars , var(uLits[j]) ) );
            pfCompl[j] = sign(uLits[j]) ? 0 : 1;
         }
         Abc_ObjSetData( pObj , Abc_SopCreateAnd( (Mem_Flex_t*)pNtkCube->pManFunc , Abc_ObjFaninNum(pObj) , pfCompl ) );
         if ( Vec_PtrEntry( vMapVars , var(_claLits[i]) ) ) {
           Abc_Print( -1 , "Selection var for clause %d is redefined\n" , i );
           exit(1); 
         }
         Vec_PtrWriteEntry( vMapVars , var(_claLits[i]) , pObj );
         pObjDef = Ssat_SopAnd2Obj( pObjDef , pObj );
      }
   }
   delete[] pfCompl;
   return pObjDef;
}

Abc_Obj_t*
SsatSolver::ntkCreateNode( Abc_Ntk_t * pNtkCube , Vec_Ptr_t * vMapVars ) const
{
   Abc_Obj_t * pObj , * pObjCube;
   char name[1024];
   int * pfCompl = new int[Abc_NtkObjNumMax(pNtkCube)];

   pObjCube = NULL;
   for ( int i = _learntClause.size()-1 ; i > -1 ; --i ) {
      printf( "  > %3d-th learnt clause , type = %s\n" , i , _learntType[i] ? "SAT" : "UNSAT" );
      dumpCla( _learntClause[i] );
      pObj = Abc_NtkCreateNode( pNtkCube );
      sprintf( name , "c%d_%s" , i , _learntType[i] ? "SAT" : "UNSAT" );
      Abc_ObjAssignName( pObj , name , "" );
      for ( int j = 0 ; j < _learntClause[i].size() ; ++j ) {
         Abc_ObjAddFanin( pObj , (Abc_Obj_t*)Vec_PtrEntry( vMapVars , var(_learntClause[i][j]) ) );
         pfCompl[j] = sign(_learntClause[i][j]) ^ _learntType[i] ? 1 : 0;
      }
      if ( _learntType[i] ) { // SAT blocking clause
         Abc_ObjSetData( pObj , Abc_SopCreateAnd( (Mem_Flex_t*)pNtkCube->pManFunc , Abc_ObjFaninNum(pObj) , pfCompl ) );
         pObjCube = Ssat_SopOr2Obj( pObjCube , pObj );
      }
      else { // UNSAT conflict clause
         Abc_ObjSetData( pObj , Abc_SopCreateOr( (Mem_Flex_t*)pNtkCube->pManFunc , Abc_ObjFaninNum(pObj) , pfCompl ) );
         pObjCube = Ssat_SopAnd2Obj( pObjCube , pObj );
      }
   }
   delete[] pfCompl;
   return pObjCube;
}

void
SsatSolver::ntkCreatePoCheck( Abc_Ntk_t * pNtkCube , Abc_Obj_t * pObjDef , Abc_Obj_t * pObjCube ) const
{
   Abc_Obj_t * pPo;
   Abc_ObjAddFanin( pPo = Abc_NtkCreatePo( pNtkCube ) , Ssat_SopAnd2Obj( pObjDef , pObjCube ) );
   Abc_ObjAssignName( pPo , "cube_network_Po" , "" );
   if ( !Abc_NtkCheck( pNtkCube ) ) {
      Abc_Print( -1 , "Something wrong with cubes to network ...\n" );
      Abc_NtkDelete( pNtkCube );
      exit(1);
   }
   Ssat_DumpCubeNtk( pNtkCube );
   //Abc_NtkShow( pNtkCube , 0 , 0 , 1 );
}

void
SsatSolver::ntkWriteWcnf() const
{
}

/**Function*************************************************************

  Synopsis    [Helper functions to manipulate SOP gates.]

  Description [Return pObj2 if pObj1 == NULL.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

Abc_Obj_t*
Ssat_SopAnd2Obj( Abc_Obj_t * pObj1 , Abc_Obj_t * pObj2 )
{
   assert( pObj2 );
   if ( !pObj1 ) return pObj2;
   assert( Abc_ObjNtk(pObj1) == Abc_ObjNtk(pObj2) );

   Abc_Obj_t * pObjAnd;
   Vec_Ptr_t * vFanins = Vec_PtrStart( 2 );
   Vec_PtrWriteEntry( vFanins , 0 , pObj1 );
   Vec_PtrWriteEntry( vFanins , 1 , pObj2 );
   pObjAnd = Abc_NtkCreateNodeAnd( Abc_ObjNtk(pObj1) , vFanins );
   Vec_PtrFree( vFanins );
   return pObjAnd;
}

Abc_Obj_t*
Ssat_SopOr2Obj( Abc_Obj_t * pObj1 , Abc_Obj_t * pObj2 )
{
   assert( pObj2 );
   if ( !pObj1 ) return pObj2;
   assert( Abc_ObjNtk(pObj1) == Abc_ObjNtk(pObj2) );

   Abc_Obj_t * pObjOr;
   Vec_Ptr_t * vFanins = Vec_PtrStart( 2 );
   Vec_PtrWriteEntry( vFanins , 0 , pObj1 );
   Vec_PtrWriteEntry( vFanins , 1 , pObj2 );
   pObjOr = Abc_NtkCreateNodeOr( Abc_ObjNtk(pObj1) , vFanins );
   Vec_PtrFree( vFanins );
   return pObjOr;
}

/**Function*************************************************************

  Synopsis    [Dump constructed circuits for debug.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void
Ssat_DumpCubeNtk( Abc_Ntk_t * pNtk )
{
   Abc_Obj_t * pObj , * pFanin;
   int i , j;
   printf( "\n  > print nodes in the cube network\n" );
   Abc_NtkForEachNode( pNtk , pObj , i )
   {
      printf( ".names" );
      Abc_ObjForEachFanin( pObj , pFanin , j )
         printf( " %s" , Abc_ObjName( pFanin ) );
      printf( " %s\n%s" , Abc_ObjName(pObj), (char*)Abc_ObjData(pObj) );
   }
   printf( "\n" );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
