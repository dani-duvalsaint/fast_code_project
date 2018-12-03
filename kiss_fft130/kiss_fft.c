/*
Copyright (c) 2003-2010, Mark Borgerding

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the author nor the names of any contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "_kiss_fft_guts.h"
/* The guts header contains all the multiplication and addition macros that are defined for
 fixed or floating point complex numbers.  It also delares the kf_ internal functions.
 */

static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

static void kf_bfly2(
        kiss_fft_cpx * Fout,
        const size_t fstride,
        const kiss_fft_cfg st,
        int m
        )
{
    kiss_fft_cpx * Fout2;
    kiss_fft_cpx * tw1 = st->twiddles;
    kiss_fft_cpx t;
    Fout2 = Fout + m;
    //do{
    C_FIXDIV(*Fout,4); //C_FIXDIV(*Fout2,2);

    C_MUL (t,  *Fout2 , *tw1);
    //tw1 += fstride;
    C_SUB( *Fout2 ,  *Fout , t );
    C_ADDTO( *Fout ,  t );
    //++Fout2;
    //++Fout;
        
    //}while (--m);
}

static void kf_bfly4(
        kiss_fft_cpx * Fout,
        const size_t fstride,
        const kiss_fft_cfg st,
        const size_t m
        )
{
    kiss_fft_cpx *tw1,*tw2,*tw3;
    kiss_fft_cpx scratch0;
    kiss_fft_cpx scratch1;
    kiss_fft_cpx scratch2;
    kiss_fft_cpx scratch3;
    kiss_fft_cpx scratch4;
    kiss_fft_cpx scratch5;

    size_t k=m;
    const size_t m2=2*m;
    const size_t m3=3*m;

    tw3 = tw2 = tw1 = st->twiddles;

    if (st->inverse) { //ifft
      for (k=0; k<m; k+=2) {
	C_FIXDIV(*Fout,4);
	C_FIXDIV(Fout[m],4);
	C_FIXDIV(Fout[m2],4);
	C_FIXDIV(Fout[m3],4);

	C_MUL( scratch0 , Fout[m]  , *tw1 );
	C_MUL( scratch1 , Fout[m2] , *tw2 );
	C_MUL( scratch2 , Fout[m3] , *tw3 );

	C_ADD( scratch3 , scratch0 , scratch2 );
	C_SUB( scratch4 , scratch0 , scratch2 );
	C_SUB( scratch5 , *Fout    , scratch1 );
	C_SUB( Fout[m2] , *Fout    , scratch3 );
	
	C_ADDTO( *Fout , scratch1 );
	C_ADDTO( *Fout , scratch3 );

	Fout[m].r  = scratch5.r - scratch4.i;
	Fout[m].i  = scratch5.i + scratch4.r;
	Fout[m3].r = scratch5.r + scratch4.i;
	Fout[m3].i = scratch5.i - scratch4.r;

	tw1 += fstride;
	tw2 += fstride*2;
	tw3 += fstride*3;
	
	++Fout;
      }
    }
    else { // fft
      for (k=0; k<m; k++) {
	C_FIXDIV(*Fout,4);
	C_FIXDIV(Fout[m],4);
	C_FIXDIV(Fout[m2],4);
	C_FIXDIV(Fout[m3],4);

	C_MUL( scratch0 , Fout[m]  , *tw1 );
	C_MUL( scratch1 , Fout[m2] , *tw2 );
	C_MUL( scratch2 , Fout[m3] , *tw3 );

	C_ADD( scratch3 , scratch0 , scratch2 );
	C_SUB( scratch4 , scratch0 , scratch2 );
	C_SUB( scratch5 , *Fout    , scratch1 );

	C_SUB( Fout[m2] , *Fout    , scratch3 );


	C_ADDTO( *Fout , scratch1 );
	C_ADDTO( *Fout , scratch3 );

	Fout[m].r  = scratch5.r + scratch4.i;
	Fout[m].i  = scratch5.i - scratch4.r;
	Fout[m3].r = scratch5.r - scratch4.i;
	Fout[m3].i = scratch5.i + scratch4.r;

	tw1 += fstride;
	tw2 += fstride*2;
	tw3 += fstride*3;

	++Fout;
      }
    }

}

static void kf_bfly_generic(
        kiss_fft_cpx * Fout,
        const size_t fstride,
        const kiss_fft_cfg st,
        int m,
        int p
        )
{
    int u,k,q1,q;
    kiss_fft_cpx * twiddles = st->twiddles;
    kiss_fft_cpx t;
    int Norig = st->nfft;

    kiss_fft_cpx * scratch = (kiss_fft_cpx*)KISS_FFT_TMP_ALLOC(sizeof(kiss_fft_cpx)*p);

    for ( u=0; u<m; ++u ) {
        k=u;
        for ( q1=0 ; q1<p ; ++q1 ) {
            scratch[q1] = Fout[ k  ];
            C_FIXDIV(scratch[q1],p);
            k += m;
        }

        k=u;
        for ( q1=0 ; q1<p ; ++q1 ) {
            int twidx=0;
            Fout[ k ] = scratch[0];
            for (q=1;q<p;++q ) {
                twidx += fstride * k;
                if (twidx>=Norig) twidx-=Norig;
                C_MUL(t,scratch[q] , twiddles[twidx] );
                C_ADDTO( Fout[ k ] ,t);
            }
            k += m;
        }
    }
    KISS_FFT_TMP_FREE(scratch);
}

static
void kf_work(
        kiss_fft_cpx * Fout,
        const kiss_fft_cpx * f,
        const size_t fstride,
        int in_stride,
        int * factors,
        const kiss_fft_cfg st
        )
{
    kiss_fft_cpx * Fout_beg=Fout;
    const int p=*factors++; /* the radix  */
    const int m=*factors++; /* stage's fft length/p */
    const kiss_fft_cpx * Fout_end = Fout + p*m;

    if (m==1) {   
      do {
	*Fout = *f;
	f += fstride*in_stride;
      } while(++Fout != Fout_end );
    }else{
      do {
	// recursive call:
	// DFT of size m*p performed by doing
	// p instances of smaller DFTs of size m, 
	// each one takes a decimated version of the input
	kf_work( Fout , f, fstride*p, in_stride, factors,st);
	f += fstride*in_stride;
      } while( (Fout += m) != Fout_end );
    }

    Fout=Fout_beg;

    // recombine the p smaller DFTs 
    //printf("switch statement with p = %i, m = %i\n", p, m);
    
    switch (p) {
        case 2: ;
	  //printf("switch statement with p = %i, m = %i\n", p, m);
	  //kiss_fft_cpx * Fout2;
	  //kf_bfly2(Fout,fstride,st,m); 
	  kiss_fft_cpx * Fout2;
	  Fout2 = Fout + m;
	  kiss_fft_cpx * tw1 = st->twiddles;
	  kiss_fft_cpx t;
	  //do{
	  C_FIXDIV(*Fout2,2);
	  C_FIXDIV(*Fout,2);
	  C_MUL (t,  *Fout2 , *tw1);
	  //tw1 += fstride;
	  C_SUB( *Fout2 ,  *Fout , t );
	  C_ADDTO( *Fout ,  t );
	  //++Fout2;
	  break; 
        case 4: kf_bfly4(Fout,fstride,st,m); break;
        default: kf_bfly_generic(Fout,fstride,st,m,p); break;
    }
}

/*  facbuf is populated by p1,m1,p2,m2, ...
    where 
    p[i] * m[i] = m[i-1]
    m0 = n                  */
static 
void kf_factor(int n,int * facbuf)
{
    int p=4;
    double floor_sqrt;
    floor_sqrt = floor( sqrt((double)n) );

    /*factor out powers of 4, powers of 2, then any remaining primes */
    do {
        while (n % p) {
            switch (p) {
                case 4: p = 2; break;
                case 2: p = 4; break;
                default: p += 2; break;
            }
            if (p > floor_sqrt)
                p = n;          /* no more factors, skip to end */
        }
        n /= p;
        *facbuf++ = p;
        *facbuf++ = n;
    } while (n > 1);
}

/*
 *
 * User-callable function to allocate all necessary storage space for the fft.
 *
 * The return value is a contiguous block of memory, allocated with malloc.  As such,
 * It can be freed with free(), rather than a kiss_fft-specific function.
 * */
kiss_fft_cfg kiss_fft_alloc(int nfft,int inverse_fft,void * mem,size_t * lenmem )
{
    kiss_fft_cfg st=NULL;
    size_t memneeded = sizeof(struct kiss_fft_state)
        + sizeof(kiss_fft_cpx)*(nfft-1); /* twiddle factors*/
    const double pi=3.141592653589793238462643383279502884197169399375105820974944;

    if ( lenmem==NULL ) {
        st = ( kiss_fft_cfg)KISS_FFT_MALLOC( memneeded );
    }else{
        if (mem != NULL && *lenmem >= memneeded)
            st = (kiss_fft_cfg)mem;
        *lenmem = memneeded;
    }
    if (st) {
        int i;
        st->nfft=nfft;
        st->inverse = inverse_fft;
        for (i=0;i<nfft;++i) {
            double phase = -2*pi*i / nfft;
            if (st->inverse)
                phase *= -1;
            kf_cexp(st->twiddles+i, phase );
        }

        kf_factor(nfft,st->factors);
    }
    return st;
}


void kiss_fft_stride(kiss_fft_cfg st,const kiss_fft_cpx *fin,kiss_fft_cpx *fout,int in_stride)
{
    unsigned long long st_1, et_1;
    if (fin == fout) {
        //NOTE: this is not really an in-place FFT algorithm.
        //It just performs an out-of-place FFT into a temp buffer
        kiss_fft_cpx * tmpbuf = (kiss_fft_cpx*)KISS_FFT_TMP_ALLOC( sizeof(kiss_fft_cpx)*st->nfft);
        
        
        st_1 = rdtsc();
        kf_work(tmpbuf,fin,1,in_stride, st->factors,st);
        et_1 = rdtsc();
        printf ("time to take FFT: %llu\n", (et_1-st_1));

        memcpy(fout,tmpbuf,sizeof(kiss_fft_cpx)*st->nfft);
        KISS_FFT_TMP_FREE(tmpbuf);
    }else{
        st_1 = rdtsc();
        kf_work( fout, fin, 1,in_stride, st->factors,st );
        et_1 = rdtsc();
        printf ("time to take FFT: %llu\n", (et_1-st_1));
    }
}

void kiss_fft(kiss_fft_cfg cfg,const kiss_fft_cpx *fin,kiss_fft_cpx *fout)
{
    kiss_fft_stride(cfg,fin,fout,1);
}


void kiss_fft_cleanup(void)
{
    // nothing needed any more
}
