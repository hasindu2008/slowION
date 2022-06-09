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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <pthread.h>

#include "slowion.h"
#include "misc.h"
#include "error.h"

opt_t *opt = NULL;

static struct option long_options[] = {
    {"positions", required_argument, 0, 'p'},      //0 number of posions
    {"channels", required_argument, 0, 'c'},       //1 chennels per possition
    {"verbose", required_argument, 0, 'v'},        //2 verbosity level
    {"help", no_argument, 0, 'h'},                 //3
    {"version", no_argument, 0, 'V'},              //4
    {"time", required_argument, 0, 'T'},           //5
    {"sample-rate", required_argument, 0, 'f'},    //6
    {"rlen", required_argument, 0, 'r'},           //7
    {"output", required_argument, 0, 'd'},         //8
    {0, 0, 0, 0}};


static inline void print_help_msg(FILE *fp_help, opt_t *opt){
    fprintf(fp_help,"Usage: slowion [OPTIONS]\n");
    fprintf(fp_help,"\nboptions:\n");
    fprintf(fp_help,"   -p INT                     number of positions [%d]\n",opt->npos);
    fprintf(fp_help,"   -c INT                     channels per position [%d]\n",opt->nchan);
    fprintf(fp_help,"   -T INT                     simulation time in seconds [%d]\n",opt->sim_time);
    fprintf(fp_help,"   -r INT                     mean read length (num bases) [%d]\n",opt->mean_rlen);
    fprintf(fp_help,"   -f INT                     sample rate [%d]\n",opt->freq);
    fprintf(fp_help,"   -b INT                     average translocation speed (bases per second) [%d]\n",opt->bps);
    fprintf(fp_help,"   -d DIR                     output directory [%s]\n",opt->dir);
    fprintf(fp_help,"   -h                         help\n");
    fprintf(fp_help,"   --verbose INT              verbosity level [%d]\n",(int)get_log_level());
    fprintf(fp_help,"   --version                  print version\n");

}

void set_max_open_files(){
    struct rlimit rlp;
    getrlimit(RLIMIT_NOFILE, &rlp);
    LOG_TRACE("max open files curr:%ld, max:%ld", rlp.rlim_cur, rlp.rlim_max);
    rlp.rlim_cur = rlp.rlim_max;
    setrlimit(RLIMIT_NOFILE, &rlp);
    getrlimit(RLIMIT_NOFILE, &rlp);
    LOG_TRACE("max open files curr:%ld, max:%ld ", rlp.rlim_cur, rlp.rlim_max);
}

int main(int argc, char* argv[]){

    double realtime0 = realtime();
    const char* optstring = "p:c:T:f:r:d:b:hVv";

    int longindex = 0;
    int32_t c = -1;
    FILE *fp_help = stderr;

    opt = init_opt();

    //parse the user args
    while ((c = getopt_long(argc, argv, optstring, long_options, &longindex)) >= 0) {

        if (c == 'p') {
            opt->npos = mm_parse_num(optarg);
            if(opt->npos<0 || opt->npos>100 ){
                WARNING("%s","Number of positions must be between 0 and 100. Continuing anyway. May crash.");
                exit(EXIT_FAILURE);
            }
        } else if (c == 'c') {
            opt->nchan = atoi(optarg);
            if(opt->nchan<0 || opt->nchan>3000 ){
                WARNING("%s","Number of channels must be between 0 and 3000. Continuing anyway. May crash.");
                exit(EXIT_FAILURE);
            }
        } else if (c=='v'){
            int v = atoi(optarg);
            set_log_level((enum log_level_opt)v);
        } else if (c=='V'){
            fprintf(stdout,"slowION %s\n",SLOWION_VERSION);
            exit(EXIT_SUCCESS);
        } else if (c=='h'){
            fp_help = stdout;
        } else if (c=='T'){
            opt->sim_time = atoi(optarg);
            assert(opt->npos>0);
        } else if (c=='f'){
            opt->freq = atoi(optarg);
            if(opt->freq<3000 || opt->freq>10000 ){
                WARNING("%s","sample rate must be between 1000 and 10000. Continuing anyway. May crash.");
            }
        } else if (c=='r'){
            opt->mean_rlen = atoi(optarg);
            if(opt->mean_rlen<3000 ){
                ERROR("%s","mean read length must be >=3000. For such libraries it is silly to write in chunks as we can just cache the whole thing in memory. This benchmark is not for such.");
                exit(EXIT_FAILURE);
            }
            if(opt->mean_rlen>50000 ){
                WARNING("%s","mean read length must be <=50000. Haven't seen a library with such long reads yet. Continuing anyway. May crash.");
            }
        } else if (c=='d'){
            opt->dir = optarg;
        } else if (c=='b'){
            opt->bps = atoi(optarg);
            if(opt->bps<50 || opt->bps>500 ){
                WARNING("%s","translocation speed must be between 50 and 500. Continuing anyway. May crash.");
            }
        }
        // } else if(c == 0 && longindex == 7){ //debug break
        //     opt.debug_break = atoi(optarg);
        // }
    }

    if (argc - optind > 0 || fp_help == stdout) {
        print_help_msg(fp_help, opt);
        if(fp_help == stdout){
            exit(EXIT_SUCCESS);
        }
        exit(EXIT_FAILURE);
    }

    cal_opt(opt);
    VERBOSE("positions: %d, channels: %d, sample_rate: %d Hz, avg speed: %d bases/s, avg readlen: %d bases", opt->npos, opt->nchan, opt->freq, opt->bps, opt->mean_rlen);
    VERBOSE("simulation time : %d seconds, ct: %d, cz: %d, iterations: %d, memreq %.2f GiB", opt->sim_time, opt->ct, opt->cz, opt->iterations, (double)opt->cz*opt->npos*opt->nchan*2.0/(1024*1024*1024));

    set_max_open_files();

    prom_t *prom = init_prom();

    pthread_t *wp = (pthread_t *)malloc(prom->npos * sizeof(pthread_t)); //sequence aquisition, dwrite and iwrite
    MALLOC_CHK(wp);
    ptarg_t *arg = (ptarg_t *)malloc(prom->npos * sizeof(ptarg_t));
    MALLOC_CHK(arg);

    for(int t=0; t<prom->npos; t++){
        arg[t].prom = prom;
        arg[t].mypos = t;
        int ret = pthread_create(&wp[t], NULL, seq_aq_w,(void*)(&arg[t]));
        NEG_CHK(ret);
    }

    pthread_t *sz = (pthread_t *)malloc(prom->npos * sizeof(pthread_t)); //iwrite->dwrite
    MALLOC_CHK(sz);
    for(int t=0; t<prom->npos; t++){
        int ret = pthread_create(&sz[t], NULL, iwrite2dwrite,(void*)(&arg[t]));
        NEG_CHK(ret);
    }

    pthread_t *b = (pthread_t *)malloc(prom->npos * sizeof(pthread_t));  //slow5 basecall
    MALLOC_CHK(b);
    for(int t=0; t<prom->npos; t++){
        int ret = pthread_create(&b[t], NULL, pseudobasecaller, (void*)(&arg[t]));
        NEG_CHK(ret);
    }

    for (int t = 0; t < prom->npos; t++) {
        int ret = pthread_join(b[t], NULL);
        NEG_CHK(ret);
    }

    for (int t = 0; t < prom->npos; t++) {
        int ret = pthread_join(sz[t], NULL);
        NEG_CHK(ret);
    }

    for (int t = 0; t < prom->npos; t++) {
        int ret = pthread_join(wp[t], NULL);
        NEG_CHK(ret);
    }


    free(wp);
    free(sz);
    free(b);
    free(arg);

    free_prom(prom);

    free_opt(opt);

    fprintf(stderr,"[%s] Version: %s\n", __func__, SLOWION_VERSION);
    fprintf(stderr, "[%s] CMD:", __func__);
    for (int i = 0; i < argc; ++i) fprintf(stderr, " %s", argv[i]);
    fprintf(stderr, "\n[%s] Real time: %.3f sec; CPU time: %.3f sec; Peak RAM: %.3f GB\n\n",
            __func__, realtime() - realtime0, cputime(),peakrss() / 1024.0 / 1024.0 / 1024.0);

    return 0;
}