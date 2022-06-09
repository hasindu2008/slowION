/******************************************************************************

MIT License

Copyright (c) 2023 Hasindu Gamaarachchi (hasindu@unsw.edu.au)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

******************************************************************************/

#ifndef SLOWION_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


#define SLOWION_VERSION "0.1.0"

typedef struct{
    int bps;
    int mean_rlen;
    int mean_slen;
    int sim_time;

    int npos;
    int nchan;

    int freq;
    int ct;
    int cz;
    int iterations;

    const char *dir;

    int64_t seed;

} opt_t;


typedef struct{
    FILE *fp;
    uint64_t len_raw_signal;
    int16_t *raw_signal;
    int32_t read_number;
    uint64_t aq; //how much sequenced
    int32_t chunk_number;

    int32_t c_islow5; //written to disk
    int32_t c_s; // iwrite2dwrited

} chan_t;


typedef struct{
    int nchan;
    chan_t **c;

    int64_t c_direct;  //written to disk
    int64_t c_s; // iwrite2dwrited

    int64_t c_bd; //direct ones current being basecalled
    int64_t c_bs; //iwrite2dwrited ones current being basecalled

    int64_t total_samples;
    int8_t aq_done;
    int8_t s_done;

} pos_t;

typedef struct{
    int npos;
    pos_t **pos;
} prom_t;

typedef struct{
    prom_t *prom;
    int mypos;
} ptarg_t;

void cal_opt(opt_t *opt);
opt_t *init_opt();
void free_opt(opt_t *opt);
prom_t *init_prom();
void free_prom(prom_t *prom);
void *seq_aq_w(void *ptarg);
void *iwrite2dwrite(void *ptarg);
void *pseudobasecaller(void *ptarg);

#endif