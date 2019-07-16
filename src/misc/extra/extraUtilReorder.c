/**CFile****************************************************************

   FileName    [extraUtilMult.c]

   SystemName  [ABC: Logic synthesis and verification system.]

   PackageName [extra]

   Synopsis    [Dynamic Variable Reordering for simple BDD package]

   Author      [Yukio Miyasaka]
  
   Affiliation [U Tokyo]

   Date        []

   Revision    []

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "extra.h"
#include "misc/vec/vec.h"
#include "aig/gia/gia.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

abctime timeForRef = 0;
abctime timeForUniSearch = 0;

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************
   
   Synopsis    []

   Description []
               
   SideEffects []

   SeeAlso     []

***********************************************************************/
void Abc_BddRef_rec( Abc_BddMan * p, unsigned i, int d )
{
  int j;
  //printf("ref %d(%d) by %d\n", i, Abc_BddRef( p, i ), d );
  if ( Abc_BddLitIsConst( i ) ) return;
  Abc_BddIncRef( p, i, d );
  Abc_BddRef_rec( p, Abc_BddElse( p, i ), d );
  Abc_BddRef_rec( p, Abc_BddThen( p, i ), d );
}
void Abc_BddDeref_rec( Abc_BddMan * p, unsigned i, int d )
{
  int j;
  //printf("deref %d(%d) by %d\n", i, Abc_BddRef( p, i ), d );
  if ( Abc_BddLitIsConst( i ) ) return;
  Abc_BddDecRef( p, i, d );
  Abc_BddDeref_rec( p, Abc_BddElse( p, i ), d );
  Abc_BddDeref_rec( p, Abc_BddThen( p, i ), d );
}

/**Function*************************************************************
   
   Synopsis    []

   Description []
               
   SideEffects []

   SeeAlso     []

***********************************************************************/
static inline void Abc_BddShiftBvar( Abc_BddMan * p, int i, int d )
{
  int Var = p->pVars[i];
  unsigned Then = p->pObjs[(unsigned)i + i];
  unsigned Else = p->pObjs[(unsigned)i + i + 1];
  unsigned hash;
  int * q, * head;
  int * next = p->pNexts + i;
  // remove
  
  abctime clk = Abc_Clock();
  hash = Abc_BddHash( Var, Then, Else ) & p->nUniqueMask;
  q = p->pUnique + hash;
  for ( ; *q; q = p->pNexts + *q )
    if ( *q == i )
      {
	*q = *next;
	break;
      }
  abctime clk2 = Abc_Clock();
  timeForUniSearch += clk2 - clk;

  // change
  Var = p->pVars[i] = p->pVars[i] + d;
  // register (overwrite. remove non-used node)
  hash = Abc_BddHash( Var, Then, Else ) & p->nUniqueMask;
  head = q = p->pUnique + hash;
  clk = Abc_Clock();  
  for ( ; *q; q = p->pNexts + *q )
    if ( (int)p->pVars[*q] == Var && p->pObjs[(unsigned)*q + *q] == Then && p->pObjs[(unsigned)*q + *q + 1] == Else )
      {
	*next = *(p->pNexts + *q);
	*(p->pNexts + *q) = 0;
	*q = i;
	return;
      }
  clk2 = Abc_Clock();
  timeForUniSearch += clk2 - clk;
  *next = *head;
  *head = i;
}
static inline void Abc_BddSwapBvar( Abc_BddMan * p, int i, int * nNew, int * nRemoved )
{
  int Var = p->pVars[i];
  unsigned Then = p->pObjs[(unsigned)i + i];
  unsigned Else = p->pObjs[(unsigned)i + i + 1];
  int Ref = p->pRefs[i];
  unsigned hash;
  int * q, * head;
  int *next = p->pNexts + i;
  unsigned f00, f01, f10, f11, n0, n1;
  // remove
  
  abctime clk = Abc_Clock();
  hash = Abc_BddHash( Var, Then, Else ) & p->nUniqueMask;
  q = p->pUnique + hash;
  for ( ; *q; q = p->pNexts + *q )
    if ( *q == i )
      {
	*q = *next;
	break;
      }
  abctime clk2 = Abc_Clock();
  timeForUniSearch += clk2 - clk;
  
  // new chlidren
  Abc_BddDeref_rec( p, Then, Ref );
  assert( Abc_BddVar( p, Then ) != Var + 1 );
  if ( Abc_BddVar( p, Then ) == Var )
    {
      f11 = Abc_BddThen( p, Then );
      f10 = Abc_BddElse( p, Then );
      if ( !Abc_BddRef( p, Then ) ) *nRemoved += 1;
    }
  else
    f11 = f10 = Then;
  Abc_BddDeref_rec( p, Else, Ref );
  assert( Abc_BddVar( p, Else ) != Var + 1 );
  if ( Abc_BddVar( p, Else ) == Var )
    {
      f01 = Abc_BddThen( p, Else );
      f00 = Abc_BddElse( p, Else );
      if ( !Abc_BddRef( p, Else ) ) *nRemoved += 1;
    }
  else
    f01 = f00 = Else;
  n1 = Abc_BddUniqueCreate( p, Var + 1, f11, f01 );
  n0 = Abc_BddUniqueCreate( p, Var + 1, f10, f00 );
  if ( Abc_BddVar( p, n1 ) == Var + 1 && !Abc_BddRef( p, n1 ) ) *nNew += 1;
  Abc_BddRef_rec( p, n1, Ref );
  if ( Abc_BddVar( p, n0 ) == Var + 1 && !Abc_BddRef( p, n0 ) ) *nNew += 1;
  Abc_BddRef_rec( p, n0, Ref );
  // change
  p->pObjs[(unsigned)i + i] = n1;
  p->pObjs[(unsigned)i + i + 1] = n0;
  // register
  hash = Abc_BddHash( Var, n1, n0 ) & p->nUniqueMask;
  head = q = p->pUnique + hash;
  clk = Abc_Clock();
  for ( ; *q; q = p->pNexts + *q )
    if ( (int)p->pVars[*q] == Var && p->pObjs[(unsigned)*q + *q] == n1 && p->pObjs[(unsigned)*q + *q + 1] == n0 )
      {
	*next = *(p->pNexts + *q);
	*(p->pNexts + *q) = 0;
	*q = i;
	return;
      }
  clk2 = Abc_Clock();
  timeForUniSearch += clk2 - clk;
  *next = *head;
  *head = i;
}
// swap x-th variable and (x+1)-th variable
int Abc_BddSwap( Abc_BddMan * p, int x, int fVerbose )
{
  int i, bvar, nNew = 0, nRemoved = 0;
  Vec_Int_t * pXthNodes = Vec_IntAlloc( 1 );
  for ( i = 1; i < p->nObjs; i++ )
    {
      if ( fVerbose ) printf("%d(<%d) %d %d(x=%d)\n", i, p->nObjs, p->pRefs[i], (int)p->pVars[i], x);
      if ( !p->pRefs[i] ) continue;
      if ( (int)p->pVars[i] == x + 1 )
	Abc_BddShiftBvar( p, i, -1 );
      else if ( (int)p->pVars[i] == x )
	{
	  if ( Abc_BddVar( p, p->pObjs[(unsigned)i + i]     ) == x     ||
	       Abc_BddVar( p, p->pObjs[(unsigned)i + i]     ) == x + 1 ||
	       Abc_BddVar( p, p->pObjs[(unsigned)i + i + 1] ) == x     ||
	       Abc_BddVar( p, p->pObjs[(unsigned)i + i + 1] ) == x + 1 )
	    Vec_IntPush( pXthNodes, i );
	  else
	    Abc_BddShiftBvar( p, i, 1 );
	}
    }
  Vec_IntForEachEntry( pXthNodes, bvar, i )
    Abc_BddSwapBvar( p, bvar, &nNew, &nRemoved );
  //  printf( "diff = %d  new = %d  removed = %d\n", nNew - nRemoved, nNew, nRemoved );
  Vec_IntFree( pXthNodes );
  return nNew - nRemoved;
}

/**Function*************************************************************
   
   Synopsis    []

   Description []
               
   SideEffects []

   SeeAlso     []

***********************************************************************/
static inline void Abc_BddSwapBvar2( Abc_BddMan * p, int i, int * nNew )
{
  int Var = p->pVars[i];
  unsigned Then = p->pObjs[(unsigned)i + i];
  unsigned Else = p->pObjs[(unsigned)i + i + 1];
  int Ref = p->pRefs[i];
  unsigned hash;
  int * q, * head;
  int *next = p->pNexts + i;
  unsigned f00, f01, f10, f11, n0, n1;
  // remove
  hash = Abc_BddHash( Var, Then, Else ) & p->nUniqueMask;
  q = p->pUnique + hash;
  for ( ; *q; q = p->pNexts + *q )
    if ( *q == i )
      {
	*q = *next;
	break;
      }
  // new chlidren
  if ( Abc_BddVar( p, Then ) == Var || Abc_BddVar( p, Then ) == Var + 1 )
    f11 = Abc_BddThen( p, Then ), f10 = Abc_BddElse( p, Then );
  else
    f11 = f10 = Then;
  if ( Abc_BddVar( p, Else ) == Var || Abc_BddVar( p, Else ) == Var + 1 )
    f01 = Abc_BddThen( p, Else ), f00 = Abc_BddElse( p, Else );
  else
    f01 = f00 = Else;
  n1 = Abc_BddUniqueCreate( p, Var + 1, f11, f01 );
  n0 = Abc_BddUniqueCreate( p, Var + 1, f10, f00 );
  if ( Abc_BddVar( p, n1 ) == Var + 1 && !Abc_BddRef( p, n1 ) ) *nNew += 1;
  if ( Abc_BddVar( p, n0 ) == Var + 1 && !Abc_BddRef( p, n0 ) ) *nNew += 1;
  Abc_BddRef_rec( p, n1, Ref );
  Abc_BddRef_rec( p, n0, Ref );
  // change
  p->pObjs[(unsigned)i + i] = n1;
  p->pObjs[(unsigned)i + i + 1] = n0;
  // register
  hash = Abc_BddHash( Var, n1, n0 ) & p->nUniqueMask;
  head = q = p->pUnique + hash;
  for ( ; *q; q = p->pNexts + *q )
    if ( (int)p->pVars[*q] == Var && p->pObjs[(unsigned)*q + *q] == n1 && p->pObjs[(unsigned)*q + *q + 1] == n0 )
      {
	*next = *(p->pNexts + *q);
	*(p->pNexts + *q) = 0;
	*q = i;
	return;
      }
  *next = *head;
  *head = i;
}
// swap x-th variable and (x+1)-th variable
int Abc_BddSwap2( Abc_BddMan * p, int x )
{
  int i, bvar, nNew = 0, nRemoved = 0;
  Vec_Int_t * pXthNodes = Vec_IntAlloc( 1 );
  Vec_Int_t * pX1thNodes = Vec_IntAlloc( 1 );
  // deref of children of x-level nodes can precede and save raising unnecessary x1-level nodes
  for ( i = 1; i < p->nObjs; i++ )
    {
      if ( !p->pRefs[i] ) continue;
      if ( (int)p->pVars[i] == x + 1 )
	Vec_IntPush( pX1thNodes, i );
      else if ( (int)p->pVars[i] == x )
	{
	  unsigned Then = p->pObjs[(unsigned)i + i];
	  unsigned Else = p->pObjs[(unsigned)i + i + 1];
	  if ( Abc_BddVar( p, Then ) == x + 1 || Abc_BddVar( p, Else ) == x + 1 )
	    {
	      int Ref = p->pRefs[i];
	      Vec_IntPush( pXthNodes, i );
	      Abc_BddDeref_rec( p, Then, Ref );
	      if ( Abc_BddVar( p, Then ) == x + 1 && !Abc_BddRef( p, Then ) ) nRemoved += 1;
	      Abc_BddDeref_rec( p, Else, Ref );
	      if ( Abc_BddVar( p, Else ) == x + 1 && !Abc_BddRef( p, Else ) ) nRemoved += 1;
	    }
	  else
	    Abc_BddShiftBvar( p, i, 1 );
	}
    }
  Vec_IntForEachEntry( pX1thNodes, bvar, i )
    if ( p->pRefs[bvar] ) Abc_BddShiftBvar( p, bvar, -1 );
  Vec_IntForEachEntry( pXthNodes, bvar, i )
    Abc_BddSwapBvar2( p, bvar, &nNew );
  //  printf( "diff = %d  new = %d  removed = %d\n", nNew - nRemoved, nNew, nRemoved );
  Vec_IntFree( pXthNodes );
  Vec_IntFree( pX1thNodes );
  return nNew - nRemoved;
}

/**Function*************************************************************
   
   Synopsis    []

   Description []
               
   SideEffects []

   SeeAlso     []

***********************************************************************/
static inline void Abc_BddSwapBvar3( Abc_BddMan * p, int i, int * nNew, int * nRemoved )
{
  int Var = p->pVars[i];
  unsigned Then = p->pObjs[(unsigned)i + i];
  unsigned Else = p->pObjs[(unsigned)i + i + 1];
  int Ref = p->pRefs[i];
  unsigned hash;
  int * q, * head;
  int *next = p->pNexts + i;
  unsigned f00, f01, f10, f11, n0, n1;
  // remove
  hash = Abc_BddHash( Var, Then, Else ) & p->nUniqueMask;
  q = p->pUnique + hash;
  for ( ; *q; q = p->pNexts + *q )
    if ( *q == i ) break;
  assert( *q );
  *q = *next;
  // new chlidren
  assert( Abc_BddVar( p, Then ) != Var + 1 );
  if ( Abc_BddVar( p, Then ) == Var )
    {
      Abc_BddDecRef( p, Then, Ref );
      f11 = Abc_BddThen( p, Then );
      f10 = Abc_BddElse( p, Then );
      if ( !Abc_BddRef( p, Then ) ) *nRemoved += 1;
    }
  else
    f11 = f10 = Then;
  assert( Abc_BddVar( p, Else ) != Var + 1 );
  if ( Abc_BddVar( p, Else ) == Var )
    {
      Abc_BddDecRef( p, Else, Ref );
      f01 = Abc_BddThen( p, Else );
      f00 = Abc_BddElse( p, Else );
      if ( !Abc_BddRef( p, Else ) ) *nRemoved += 1;
    }
  else
    f01 = f00 = Else;
  n1 = Abc_BddUniqueCreate( p, Var + 1, f11, f01 );
  n0 = Abc_BddUniqueCreate( p, Var + 1, f10, f00 );
  if ( Abc_BddVar( p, n1 ) == Var + 1 )
    {
      if ( !Abc_BddRef( p, n1 ) ) *nNew += 1;
      Abc_BddIncRef( p, n1, Ref );
    }
  if ( Abc_BddVar( p, n0 ) == Var + 1 )
    {
      if ( !Abc_BddRef( p, n0 ) ) *nNew += 1;
      Abc_BddIncRef( p, n0, Ref );
    }
  // change
  p->pObjs[(unsigned)i + i] = n1;
  p->pObjs[(unsigned)i + i + 1] = n0;
  // register
  hash = Abc_BddHash( Var, n1, n0 ) & p->nUniqueMask;
  head = q = p->pUnique + hash;
  for ( ; *q; q = p->pNexts + *q )
    if ( (int)p->pVars[*q] == Var && p->pObjs[(unsigned)*q + *q] == n1 && p->pObjs[(unsigned)*q + *q + 1] == n0 )
      {
	*next = *(p->pNexts + *q);
	*(p->pNexts + *q) = 0;
	*q = i;
	return;
      }
  *next = *head;
  *head = i;
}
// swap x-th variable and (x+1)-th variable
int Abc_BddSwap3( Abc_BddMan * p, int x )
{
  int i, bvar, nNew = 0, nRemoved = 0;
  Vec_Int_t * pXthNodes = Vec_IntAlloc( 1 );
  for ( i = 1; i < p->nObjs; i++ )
    {
      if ( !p->pRefs[i] ) continue;
      if ( (int)p->pVars[i] == x + 1 )
	Abc_BddShiftBvar( p, i, -1 );
      else if ( (int)p->pVars[i] == x )
	{
	  if ( Abc_BddVar( p, p->pObjs[(unsigned)i + i]     ) == x     ||
	       Abc_BddVar( p, p->pObjs[(unsigned)i + i]     ) == x + 1 ||
	       Abc_BddVar( p, p->pObjs[(unsigned)i + i + 1] ) == x     ||
	       Abc_BddVar( p, p->pObjs[(unsigned)i + i + 1] ) == x + 1 )
	    Vec_IntPush( pXthNodes, i );
	  else
	    Abc_BddShiftBvar( p, i, 1 );
	}
    }
  Vec_IntForEachEntry( pXthNodes, bvar, i )
    Abc_BddSwapBvar3( p, bvar, &nNew, &nRemoved );
  //  printf( "diff = %d  new = %d  removed = %d\n", nNew - nRemoved, nNew, nRemoved );
  Vec_IntFree( pXthNodes );
  return nNew - nRemoved;
}


/**Function*************************************************************
   
   Synopsis    []

   Description []
               
   SideEffects []

   SeeAlso     []

***********************************************************************/
static inline void Abc_BddShift( Abc_BddMan * p, int * pos, int * diff, int distance, int fUp, int * bestPos, int * bestDiff, int * old2new, int * new2old )
{
  int j, k;
  for ( j = 0; j < distance; j++ )
    {
      if ( fUp ) *pos -= 1;
      *diff += Abc_BddSwap2( p, *pos );
      ABC_SWAP( int, old2new[new2old[*pos]], old2new[new2old[*pos + 1]] );
      ABC_SWAP( int, new2old[*pos], new2old[*pos + 1] );
      if ( !fUp ) *pos += 1;
      if ( *diff < *bestDiff )
	{
	  *bestDiff = *diff;
	  *bestPos = *pos;
	}
      /* for ( k = 0; k < p->nVars; k++ ) */
      /* 	printf("%d,", new2old[k]); */
      /* printf("  cur pos %d  diff %d\n", *pos, *diff); */
    }
}
int Abc_BddReorder( Abc_BddMan * p, Vec_Int_t * pFunctions, int fVerbose )
{
  int i, j, k, best_i;
  int totalDiff = 0;
  unsigned a;
  p->pRefs = ABC_CALLOC( int, p->nObjsAlloc );
  abctime clk = Abc_Clock();
  Vec_IntForEachEntry( pFunctions, a, i )
    Abc_BddRef_rec( p, a, 1 );
  abctime clk2 = Abc_Clock();
  ABC_PRT( "init ref time", clk2 - clk );
  
  int * old2new = ABC_CALLOC( int, p->nVars );
  for ( i = 0; i < p->nVars; i++ )
    old2new[i] = i;
  int * new2old = ABC_CALLOC( int, p->nVars );
  for ( i = 0; i < p->nVars; i++ )
    new2old[i] = i;
  
  clk = Abc_Clock();
  int * numNodes = ABC_CALLOC( int, p->nVars );
  for ( i = 1; i < p->nObjs; i++ )
    if ( p->pRefs[i] )
      numNodes[(int)p->pVars[i]] += 1;
  clk2 = Abc_Clock();
  ABC_PRT( "count nodes time", clk2 - clk );
  
  int * descendingOrder = ABC_CALLOC( int, p->nVars );
  for ( i = 0; i < p->nVars; i++ )
    descendingOrder[i] = i;
  for ( i = 0; i < p->nVars - 1; i++ )
    {
      best_i = i;
      for ( j = i + 1; j < p->nVars; j++ )
	if ( numNodes[descendingOrder[j]] > numNodes[descendingOrder[best_i]])
	  best_i = j;
      ABC_SWAP( int, descendingOrder[i], descendingOrder[best_i] );
    }
  
  printf("num_nodes : ");
  for ( i = 0; i < p->nVars; i++ )
    printf("%d,", numNodes[i]);
  printf("\n");
  printf("indices (descending order) : ");
  for ( i = 0; i < p->nVars; i++ )
    printf("%d,", descendingOrder[i]);
  printf("\n");

  for ( i = 0; i < p->nVars; i++ )
    {
      int pos = old2new[descendingOrder[i]];
      int diff = 0;
      int bestPos = pos;
      int bestDiff = 0;
      int goUp = 0;
      int distance;
      if( pos < p->nVars >> 1 )
	{
	  goUp ^= 1;
	  distance = pos;
	}
      else distance = p->nVars - pos - 1;
      if ( fVerbose )
	{
	  printf("###############################\n");
	  printf("# begin shift %d\n", descendingOrder[i]);
	  printf("###############################\n");
	  printf("%d goes %s by %d\n", descendingOrder[i], goUp? "up": "down", distance);
	}
      Abc_BddShift( p, &pos, &diff, distance, goUp, &bestPos, &bestDiff, old2new, new2old );
      goUp ^= 1;
      distance = p->nVars - 1;
      if ( fVerbose ) printf("%d goes %s by %d\n", descendingOrder[i], goUp? "up": "down", distance);
      
      Abc_BddShift( p, &pos, &diff, distance, goUp, &bestPos, &bestDiff, old2new, new2old );


      goUp ^= 1;
      if ( goUp ) distance = p->nVars - bestPos - 1;
      else distance = bestPos;
      if ( fVerbose )
	{
	  printf("best %d\n", bestPos);
	  printf("%d goes %s by %d\n", descendingOrder[i], goUp? "up": "down", distance);
	}

      Abc_BddShift( p, &pos, &diff, distance, goUp, &bestPos, &bestDiff, old2new, new2old );
      totalDiff += bestDiff;
      if ( fVerbose )
	{
	  printf("###############################\n");
	  printf("# end shift %d\n", descendingOrder[i]);
	  printf("###############################\n");
	}
    }
  printf("gain %d\n", totalDiff);
  Vec_IntForEachEntry( pFunctions, a, i )
    Abc_BddDeref_rec( p, a, 1 );
  ABC_FREE( p->pRefs );

  ABC_PRT( "total ref time", timeForRef );
  ABC_PRT( "total unique search time", timeForUniSearch );
  return totalDiff;
}


////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

