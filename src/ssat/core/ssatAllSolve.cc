/**CFile****************************************************************

  FileName    [ssatAllSolve.cc]

  SystemName  [ssatQesto]

  Synopsis    [All-SAT enumeration solve]

  Author      [Nian-Ze Lee]
  
  Affiliation [NTU]

  Date        [10, Jan., 2017]

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

  Synopsis    [Solving process entrance]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

double
SsatSolver::aSolve( double range , int cLimit , bool fMini )
{
   _s2 = buildAllSelector();
   assert( isRVar( _rootVars[0][0] ) ); // only support 2SSAT
   return aSolve2SSAT( range , cLimit , fMini );
}

/**Function*************************************************************

  Synopsis    [Build _s2 (All-Sat Lv.1 vars selector)]

  Description [forall vars have exactly the same IDs as _s1]
               
  SideEffects [Initialize as tautology (no clause)]

  SeeAlso     []

***********************************************************************/

Solver*
SsatSolver::buildAllSelector()
{
   Solver * S = new Solver;
   vec<Lit> uLits;
   
   for ( int i = 0 ; i < _rootVars[0].size() ; ++i )
      while ( _rootVars[0][i] >= S->nVars() ) S->newVar();
   return S;
}

/**Function*************************************************************

  Synopsis    [All-Sat 2SSAT solving internal function]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

double
SsatSolver::aSolve2SSAT( double range , int cLimit , bool fMini )
{
   vec<Lit> rLits( _rootVars[0].size() ) , sBkCla;
   abctime clk = Abc_Clock();
   initCubeNetwork( cLimit , true );
   sBkCla.capacity(_rootVars[0].size());
   while ( 1.0 - _unsatPb - _satPb > range ) {
      if ( !_s2->solve() ) {
         _unsatPb = cubeToNetwork(false);
         return (_satPb = cubeToNetwork(true));
      }
      for ( int i = 0 ; i < _rootVars[0].size() ; ++i )
         rLits[i] = ( _s2->modelValue(_rootVars[0][i]) == l_True ) ? mkLit(_rootVars[0][i]) : ~mkLit(_rootVars[0][i]);
      if ( !_s1->solve(rLits) ) { // UNSAT case
         _unsatClause.push();
         if ( fMini ) {
            sBkCla.clear();
            miniUnsatCore( _s1->conflict , sBkCla );
            sBkCla.copyTo( _unsatClause.last() );
            _s2->addClause( sBkCla );
         }
         else {
            _s1->conflict.copyTo( _unsatClause.last() );
            _s2->addClause( _s1->conflict );
         }
         if ( unsatCubeListFull() ) {
            printf( "  > Collect %d UNSAT cubes, convert to network\n" , _cubeLimit );
            _unsatPb = cubeToNetwork(false);
            printf( "  > current unsat prob = %f\n" , _unsatPb );
            Abc_PrintTime( 1 , "  > current time" , Abc_Clock() - clk );
            fflush(stdout);
         }
      }
      else { // SAT case
         sBkCla.clear();
         miniHitSet( sBkCla );
         _satClause.push();
         sBkCla.copyTo( _satClause.last() );
         _s2->addClause( sBkCla );
         if ( satCubeListFull() ) {
            printf( "  > Collect %d SAT cubes, convert to network\n" , _cubeLimit );
            _satPb = cubeToNetwork(true);
            printf( "  > current sat prob = %f\n" , _satPb );
            Abc_PrintTime( 1 , "  > current time" , Abc_Clock() - clk );
            fflush(stdout);
         }
      }
   }
   return _satPb; // lower bound
}

/**Function*************************************************************

  Synopsis    [Minimum hitting set to generalize SAT solutions.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

void
SsatSolver::miniHitSet( vec<Lit> & sBkCla )
{
   vec<Lit> rLits , eLits , minterm;
   rLits.capacity(8);
   eLits.capacity(8);
   minterm.capacity(_rootVars[0].size());
   vec<bool> pick( _s1->nVars() , false );
   bool claSat , cnfSat;
   Lit lit;
   // step1: pick all one-hot true lits 
   for ( int i = 0 ; i < _s1->nClauses() ; ++i ) {
      Clause & c = _s1->ca[_s1->clauses[i]];
      short find = 0;
      claSat = false;
      for ( int j = 0 ; j < c.size() ; ++j ) {
         if ( _s1->modelValue(c[j]) == l_True ) {
            if ( pick[var(c[j])] ) {
               claSat = true;
               break;
            }
            ++find;
            if ( find == 1 ) lit  = c[j];
            else break;
         }
      }
      if ( claSat ) continue;
      if ( find == 1 ) {
         pick[var(lit)] = true;
         if ( isRVar(var(lit)) ) sBkCla.push( ~lit );
      }
   }
   // step2: collect minterm from clauses with more than 1 true lit
   for ( int i = 0 ; i < _s1->nClauses() ; ++i ) {
      Clause & c = _s1->ca[_s1->clauses[i]];
      claSat = false;
      rLits.clear();
      eLits.clear();
      for ( int j = 0 ; j < c.size() ; ++j ) {
         if ( _s1->modelValue(c[j]) == l_True ) {
            if ( pick[var(c[j])] ) {
               claSat = true;
               break;
            }
            isRVar(var(c[j])) ? rLits.push(c[j]) : eLits.push(c[j]);
         }
      }
      if ( !claSat ) {
         if ( eLits.size() ) {
            pick[var(eLits[0])] = true;
            continue;
         }
         assert( rLits.size() > 1 );
         for ( int j = 0 ; j < rLits.size() ; ++j ) {
            pick[var(rLits[j])] = true;
            minterm.push(rLits[j]);
         }
      }
   }
   // step3: try to drop lits in minterm
   if ( minterm.size() ) {
      for ( int i = 0 ; i < minterm.size() ; ++i ) {
         pick[var(minterm[i])] = false;
         cnfSat = true;
         for ( int j = 0 ; j < _s1->nClauses() ; ++j ) {
            Clause & c = _s1->ca[_s1->clauses[j]];
            claSat = false;
            for ( int k = 0 ; k < c.size() ; ++k ) {
               if ( pick[var(c[k])] && _s1->modelValue(c[k]) == l_True ) {
                  claSat = true;
                  break;
               }
            }
            if ( !claSat ) {
               cnfSat = false;
               break;
            }
         }
         if ( !cnfSat ) {
            pick[var(minterm[i])] = true;
            sBkCla.push( ~minterm[i] );
         }
      }
   }
   // sanity check: avoid duplicated lits --> invalid write!
   if ( sBkCla.size() > _rootVars[0].size() ) {
      Abc_Print( -1 , "Wrong hitting set!!!\n" );
      dumpCla(sBkCla);
      exit(1);
   }
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////