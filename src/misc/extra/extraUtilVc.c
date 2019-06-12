/**CFile****************************************************************

  FileName    [extraUtilVc.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [extra]

  Synopsis    [vector compose and its application using the simple BDD pakcage]

  Author      [Yukio Miyasaka]
  
  Affiliation [The University of Tokyo]

  Date        []

  Revision    []

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "extra.h"
#include "misc/vec/vec.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline unsigned Abc_BddIteCacheLookup( Abc_BddMan * p, unsigned Arg1, unsigned Arg2, unsigned Arg3 )
{
  unsigned * pEnt = p->pCache + 4 * (long long)( Abc_BddHash( Arg1, Arg2, Arg3 ) & p->nCacheMask );
  p->nCacheLookups++;
  return ( pEnt[0] == Arg1 && pEnt[1] == Arg2 && pEnt[2] == Arg3 ) ? pEnt[3] : Abc_BddInvalidLit();
}
static inline unsigned Abc_BddIteCacheInsert( Abc_BddMan * p, unsigned Arg1, unsigned Arg2, unsigned Arg3, unsigned Res )
{
  unsigned * pEnt = p->pCache + 4 * (long long)( Abc_BddHash( Arg1, Arg2, Arg3 ) & p->nCacheMask );
  pEnt[0] = Arg1; pEnt[1] = Arg2; pEnt[2] = Arg3; pEnt[3] = Res;
  p->nCacheMisses++;
  return Res;
}
unsigned Abc_BddIte( Abc_BddMan * p, unsigned c, unsigned d1, unsigned d0 )
{
  /*
  unsigned r1 = Abc_BddAnd( p, c, d1 );
  if ( Abc_BddLitIsInvalid( r1 ) ) return Abc_BddInvalidLit();
  unsigned r0 = Abc_BddAnd( p, Abc_BddLitNot( c ), d0 );
  if ( Abc_BddLitIsInvalid( r0 ) ) return Abc_BddInvalidLit();
  return Abc_BddOr( p, r1, r0 );
  */
  if ( c == 0 ) return d0;
  if ( c == 1 ) return d1;
  if ( Abc_BddLitIsCompl( c ) ) return Abc_BddIte( p, Abc_BddLitNot( c ), d0, d1 );
  unsigned r, r0, r1;
  r = Abc_BddIteCacheLookup( p, c, d1, d0 );
  if ( !Abc_BddLitIsInvalid( r ) ) return r;
  int cV = Abc_BddVar( p, c );
  int d1V = Abc_BddVar( p, d1 );
  int d0V = Abc_BddVar( p, d0 );
  int minV = cV;
  if ( minV > d1V ) minV = d1V;
  if ( minV > d0V ) minV = d0V;
  unsigned cThen = ( cV == minV ) ? Abc_BddThen( p, c ): c;
  unsigned cElse = ( cV == minV ) ? Abc_BddElse( p, c ): c;
  unsigned d1Then = ( d1V == minV ) ? Abc_BddThen( p, d1 ): d1;
  unsigned d1Else = ( d1V == minV ) ? Abc_BddElse( p, d1 ): d1;
  unsigned d0Then = ( d0V == minV ) ? Abc_BddThen( p, d0 ): d0;
  unsigned d0Else = ( d0V == minV ) ? Abc_BddElse( p, d0 ): d0;
  r0 = Abc_BddIte( p, cElse, d1Else, d0Else );
  if ( Abc_BddLitIsInvalid( r0 ) ) return Abc_BddInvalidLit();
  r1 = Abc_BddIte( p, cThen, d1Then, d0Then );
  if ( Abc_BddLitIsInvalid( r1 ) ) return Abc_BddInvalidLit();
  r = Abc_BddUniqueCreate( p, minV, r1, r0 );
  if ( Abc_BddLitIsInvalid( r ) ) return Abc_BddInvalidLit();
  return Abc_BddIteCacheInsert( p, c, d1, d0, r );
}
unsigned Abc_BddVectorCompose( Abc_BddMan * p, unsigned F,  Vec_Int_t * Vars, unsigned * cache )
{
  if ( F == 0 )  return 0;
  if ( F == 1 )  return 1;
  if ( cache[Abc_BddLit2Var( F )] != 0 ) return Abc_BddLitNotCond( cache[Abc_BddLit2Var( F )] - 1, Abc_BddLitIsCompl( F ) );
  int Index = Abc_BddVar( p, F );
  unsigned Then = Abc_BddThen( p, F );
  unsigned Else = Abc_BddElse( p, F );
  unsigned rThen = Abc_BddVectorCompose( p, Then, Vars, cache );
  if ( Abc_BddLitIsInvalid( rThen ) ) return Abc_BddInvalidLit();
  unsigned rElse = Abc_BddVectorCompose( p, Else, Vars, cache );
  if ( Abc_BddLitIsInvalid( rElse ) ) return Abc_BddInvalidLit();
  unsigned IndexFunc = Vec_IntEntry( Vars, Index );
  unsigned Result = Abc_BddIte( p, IndexFunc, rThen, rElse );   // ITE( c, d1, d0 ) = (c & d1) | (!c & d0)
  if ( Abc_BddLitIsInvalid( Result ) ) return Abc_BddInvalidLit();
  cache[Abc_BddLit2Var( F )] = Abc_BddLitNotCond( Result, Abc_BddLitIsCompl( F ) ) + 1;
  return Result;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_WriteMultiAdderTree( FILE * pFile, int nVars )
{
    extern void Abc_WriteAdder( FILE * pFile, int nVars );
    int i, k, nDigits = Abc_Base10Log( nVars ), nDigits2 = Abc_Base10Log( 2*nVars );

    assert( nVars > 0 );
    fprintf( pFile, ".model Multi%d\n", nVars );

    fprintf( pFile, ".inputs" );
    //    for ( i = 0; i < 2 * nVars; i++ )
    //        for ( k = 0; k < nVars; k++ )
    for ( i = 2 * nVars - 1; i >= 0; i-- )
        for ( k = nVars - 1; k >= 0; k-- )
	    if ( i >= k && k > i - nVars )
	        fprintf( pFile, " y%0*d_%0*d", nDigits, k, nDigits2, i );
    fprintf( pFile, "\n" );

    fprintf( pFile, ".outputs" );
    for ( i = 0; i < 2*nVars; i++ )
        fprintf( pFile, " m%0*d", nDigits2, i );
    fprintf( pFile, "\n" );

    for ( i = 0; i < 2*nVars; i++ )
        fprintf( pFile, ".names x%0*d_%0*d\n", nDigits, 0, nDigits2, i );
    for ( k = 0; k < nVars; k++ )
    {
        for ( i = 0; i < 2 * nVars; i++ )
            if ( i < k || i >= k + nVars )
                fprintf( pFile, ".names y%0*d_%0*d\n", nDigits, k, nDigits2, i );
        fprintf( pFile, ".subckt ADD%d", 2*nVars );
        for ( i = 0; i < 2*nVars; i++ )
            fprintf( pFile, " a%0*d=x%0*d_%0*d", nDigits2, i, nDigits, k, nDigits2, i );
        for ( i = 0; i < 2*nVars; i++ )
            fprintf( pFile, " b%0*d=y%0*d_%0*d", nDigits2, i, nDigits, k, nDigits2, i );
        for ( i = 0; i <= 2*nVars; i++ )
            fprintf( pFile, " s%0*d=x%0*d_%0*d", nDigits2, i, nDigits, k+1, nDigits2, i );
        fprintf( pFile, "\n" );
    }
    for ( i = 0; i < 2 * nVars; i++ )
        fprintf( pFile, ".names x%0*d_%0*d m%0*d\n1 1\n", nDigits, k, nDigits2, i, nDigits2, i );
    fprintf( pFile, ".end\n" ); 
    fprintf( pFile, "\n" );
    Abc_WriteAdder( pFile, 2*nVars );
}
void Abc_GenMultiAdderTree( char * pFileName, int nVars )
{
    FILE * pFile;
    assert( nVars > 0 );
    pFile = fopen( pFileName, "w" );
    fprintf( pFile, "# %d-bit multiplier generated by ABC on %s\n", nVars, Extra_TimeStamp() );
    Abc_WriteMultiAdderTree( pFile, nVars );
    fclose( pFile );
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_BddMulti( Gia_Man_t * pGia, int fVerbose, int nMem, int nJump, int nSize )
{
  abctime clk = Abc_Clock();
  Abc_BddMan * p;
  Vec_Int_t * vNodes;
  Gia_Obj_t * pObj;
  int i, k;
  unsigned nObjsAllocInit = 1 << nMem;
  while ( nObjsAllocInit < Gia_ManCiNum( pGia ) + nSize + nSize + 2 )
    {
      nObjsAllocInit = nObjsAllocInit << 1;
      assert( nObjsAllocInit != 0 );
    }
  if ( fVerbose ) printf( "Allocate nodes by 2^%d\n", Abc_Base2Log( nObjsAllocInit ) );
  p = Abc_BddManAlloc( Gia_ManCiNum( pGia ) + nSize + nSize, nObjsAllocInit, fVerbose );
  Abc_BddGia( pGia, fVerbose, nJump, p );
  Vec_Int_t * products = Vec_IntAlloc( nSize * nSize );
  //  for ( i = 0; i < nSize + nSize; i++ )
  //    for ( k = 0; k < nSize; k++ )
  for ( i = nSize + nSize - 1; i >= 0; i-- )
    for ( k = nSize - 1; k >= 0; k-- )
      if ( i >= k && k > i - nSize )
	{
	  unsigned a = Abc_BddIthVar( Gia_ManCiNum( pGia ) + 2 * ( nSize - k - 1 ) );
	  //	  printf( "%d, ", 2 * ( nSize - k - 1 ) );
	  unsigned b = Abc_BddIthVar( Gia_ManCiNum( pGia ) + 2 * ( nSize - i + k - 1 ) + 1);
	  //	  printf( "%d\n", 2 * ( nSize - i + k - 1 ) + 1 );
	  unsigned product = Abc_BddAnd( p, a, b );
	  assert( !Abc_BddLitIsInvalid( product ) );
	  Vec_IntPush( products, product );
	}
  unsigned * cache = ABC_CALLOC( unsigned, (long long)p->nObjsAlloc );
  p->nCacheMask = ( 1 << Abc_Base2Log( p->nObjsAlloc ) ) - 1;
  ABC_FREE( p->pCache );
  p->pCache = ABC_CALLOC( unsigned, 4 * (long long)( p->nCacheMask + 1 ) );
  assert( p->pCache );
  vNodes = Vec_IntAlloc( Gia_ManCoNum( pGia ) );
  Gia_ManForEachCo( pGia, pObj, i )
    {
      printf( "%u\n", pObj->Value );
      pObj->Value = Abc_BddVectorCompose( p, pObj->Value, products, cache );
      assert( !Abc_BddLitIsInvalid( pObj->Value ) );
      printf( "%u\n", pObj->Value );
      if ( Abc_BddLit2Var( pObj->Value ) > p->nVars )
	Vec_IntPush( vNodes, pObj->Value );
    }
  abctime clk2 = Abc_Clock();
  if( fVerbose ) printf( "\n" );
  ABC_PRT( "BDD construction time", clk2 - clk );
  printf( "Shared nodes = %d Allocated nodes = %u\n", Abc_BddCountNodesArray2( p, vNodes ), p->nObjsAlloc );
  Vec_IntFree( vNodes );
  ABC_FREE( cache );
  Vec_IntFree( products );
  Abc_BddManFree( p );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

