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
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <pthread.h>

#include <slow5/slow5.h>

#include "slowion.h"
#include "error.h"
#include "misc.h"
#include "rand.h"


extern opt_t *opt;

void cal_opt(opt_t *opt){

    opt->mean_slen = opt->mean_rlen*opt->freq/opt->bps;
    assert(opt->mean_slen*2>opt->freq);

    opt->cz = ((opt->mean_slen*2));
    opt->ct = (opt->cz/opt->freq);
    assert(opt->sim_time>opt->ct);

    opt->iterations = (opt->sim_time/opt->ct);
    assert(opt->ct>0);
    assert(opt->sim_time>opt->ct);
    assert(opt->iterations>0);
}


opt_t *init_opt(){

    opt_t *opt = (opt_t *)malloc(sizeof(opt_t));

    opt->bps = 400; //in bases per second (translocation speed)
    opt->mean_rlen = 10000; //average read length in bases
    opt->sim_time = 300; //total simulation time in seconds
    opt->npos = 1; //number of sequencing positions
    opt->nchan = 512; //number of channels
    opt->freq = 4000; //sampling frequency in Hz
    opt->dir = "./output/"; //output directory
    opt->seed = 5; //seed for random number generator

    cal_opt(opt);

    return opt;
}


void free_opt(opt_t *opt){
    free(opt);
}


static void set_record_primary_fields(slow5_rec_t *slow5_record, slow5_file_t *sp, uint64_t len_raw_signal, int16_t *raw_signal, int pos, int chan, int32_t read_number){

    char read_id[4096];
    sprintf(read_id, "read_%d_%d_%d", pos, chan, read_number);
    slow5_record -> read_id = strdup(read_id);
    if(slow5_record->read_id == NULL){
        fprintf(stderr,"Could not do strdup.");
        exit(EXIT_FAILURE);
    }
    slow5_record-> read_id_len = strlen(slow5_record -> read_id);
    slow5_record -> read_group = 0;
    slow5_record -> digitisation = 2048.0;
    slow5_record -> offset = 3.0;
    slow5_record -> range = 10.0;
    slow5_record -> sampling_rate = opt->freq;
    slow5_record -> len_raw_signal = len_raw_signal;
    slow5_record -> raw_signal = (int16_t *)malloc(len_raw_signal * sizeof(int16_t));
    MALLOC_CHK(slow5_record -> raw_signal);
    memcpy(slow5_record -> raw_signal, raw_signal, len_raw_signal * sizeof(int16_t));

}

static void set_record_aux_fields(slow5_rec_t *slow5_record, slow5_file_t *sp, int chan, int32_t read_number){

    char channel_number[4096];
    sprintf(channel_number, "%d", chan);
    double median_before = 0.1;
    uint8_t start_mux = read_number;
    uint64_t start_time = 100;

    if(slow5_aux_set_string(slow5_record, "channel_number", channel_number, sp->header) < 0){
        fprintf(stderr,"Error setting channel_number auxilliary field\n");
        exit(EXIT_FAILURE);
    }
    if(slow5_aux_set(slow5_record, "median_before", &median_before, sp->header) < 0){
        fprintf(stderr,"Error setting median_before auxilliary field\n");
        exit(EXIT_FAILURE);
    }
    if(slow5_aux_set(slow5_record, "read_number", &read_number, sp->header) < 0){
        fprintf(stderr,"Error setting read_number auxilliary field\n");
        exit(EXIT_FAILURE);
    }
    if(slow5_aux_set(slow5_record, "start_mux", &start_mux, sp->header) < 0){
        fprintf(stderr,"Error setting start_mux auxilliary field\n");
        exit(EXIT_FAILURE);
    }
    if(slow5_aux_set(slow5_record, "start_time", &start_time, sp->header) < 0){
        fprintf(stderr,"Error setting start_time auxilliary field\n");
        exit(EXIT_FAILURE);
    }
}

prom_t *init_prom(){

    struct stat st = {0};

    if (stat(opt->dir, &st) == -1) {
        int ret = mkdir(opt->dir, 0755);
        if (ret == -1) {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
    } else{
        ERROR("Directory %s already exists. Delete that first.", opt->dir);
        exit(EXIT_FAILURE);
    }

    prom_t *prom = (prom_t *)malloc(sizeof(prom_t));
    MALLOC_CHK(prom);

    prom->npos = opt->npos;
    prom->pos = (pos_t **)malloc(prom->npos * sizeof(pos_t*));
    MALLOC_CHK(prom->pos);

    for(int i=0; i < prom->npos; i++){
        LOG_TRACE("Creating pos %d", i);
        prom->pos[i] = (pos_t *)malloc(sizeof(pos_t));
        MALLOC_CHK(prom->pos[i]);
        prom->pos[i]->nchan = opt->nchan;
        prom->pos[i]->c = (chan_t **)malloc(prom->pos[i]->nchan * sizeof(chan_t*));
        MALLOC_CHK(prom->pos[i]->c);

        char path[4096];
        sprintf(path, "%s/pos%d", opt->dir, i);
        //LOG_TRACE("Creating directory %s", path);
        int ret = mkdir(path, 0755);
        if (ret == -1) {
            ERROR("Could not create directory %s. %s", path, strerror(errno));
            exit(EXIT_FAILURE);
        }

        for(int j=0; j < prom->pos[i]->nchan; j++){
            prom->pos[i]->c[j] = (chan_t *)malloc(sizeof(chan_t));
            MALLOC_CHK(prom->pos[i]->c[j]);
            prom->pos[i]->c[j]->read_number = 0;
            prom->pos[i]->c[j]->aq = 0;
            prom->pos[i]->c[j]->c_islow5 = 0;
            prom->pos[i]->c[j]->c_s = 0;
            prom->pos[i]->c[j]->len_raw_signal = 0;
            prom->pos[i]->c[j]->raw_signal = (int16_t *)malloc(opt->cz * sizeof(int16_t));
            MALLOC_CHK(prom->pos[i]->c[j]->raw_signal);
            prom->pos[i]->c[j]->chunk_number = 0;
        }

        prom->pos[i]->c_direct = 0;
        prom->pos[i]->c_s = 0;
        prom->pos[i]->c_bd = 0;
        prom->pos[i]->c_bs = 0;
        prom->pos[i]->total_samples = 0;
        prom->pos[i]->aq_done = 0;
        prom->pos[i]->s_done = 0;
    }

    return prom;

}

void free_prom(prom_t *prom){

    for(int i=0; i < prom->npos; i++){
        for(int j=0; j < prom->pos[i]->nchan; j++){
            free(prom->pos[i]->c[j]->raw_signal);
            free(prom->pos[i]->c[j]);
        }
        free(prom->pos[i]->c);
        free(prom->pos[i]);
    }

    free(prom->pos);
    free(prom);
}

static void set_header_attributes(slow5_file_t *sp){

    slow5_hdr_t *header=sp->header; //pointer to the SLOW5 header

    //add a header group attribute called run_id
    if (slow5_hdr_add("run_id", header) < 0){
        ERROR("%s","Error adding run_id attribute");
        exit(EXIT_FAILURE);
    }
    //add another header group attribute called asic_id
    if (slow5_hdr_add("asic_id", header) < 0){
        ERROR("%s","Error adding asic_id attribute");
        exit(EXIT_FAILURE);
    }

    //set the run_id attribute to "run_0" for read group 0
    if (slow5_hdr_set("run_id", "run_0", 0, header) < 0){
        ERROR("%s","Error setting run_id attribute in read group 0");
        exit(EXIT_FAILURE);
    }
    //set the asic_id attribute to "asic_0" for read group 0
    if (slow5_hdr_set("asic_id", "asic_id_0", 0, header) < 0){
        ERROR("%s","Error setting asic_id attribute in read group 0");
        exit(EXIT_FAILURE);
    }

}

static void set_header_aux_fields(slow5_file_t *sp){

    //add auxilliary field: channel number
    if (slow5_aux_add("channel_number", SLOW5_STRING, sp->header) < 0){
        ERROR("%s","Error adding channel_number auxilliary field");
        exit(EXIT_FAILURE);
    }
    //add auxilliary field: median_before
    if (slow5_aux_add("median_before", SLOW5_DOUBLE, sp->header) < 0) {
        fprintf(stderr,"Error adding median_before auxilliary field");
        exit(EXIT_FAILURE);
    }
    //add auxilliary field: read_number
    if(slow5_aux_add("read_number", SLOW5_INT32_T, sp->header) < 0){
        ERROR("%s","Error adding read_number auxilliary field");
        exit(EXIT_FAILURE);
    }
    //add auxilliary field: start_mux
    if(slow5_aux_add("start_mux", SLOW5_UINT8_T, sp->header) < 0){
        ERROR("%s","Error adding start_mux auxilliary field");
        exit(EXIT_FAILURE);
    }
    //add auxilliary field: start_time
    if(slow5_aux_add("start_time", SLOW5_UINT64_T, sp->header) < 0){
        ERROR("%s","Error adding start_time auxilliary field");
        exit(EXIT_FAILURE);
    }
}

static void slow5fy(slow5_file_t *sp, uint64_t len_raw_signal, int16_t *raw_signal, int pos, int chan, int32_t read_number){
    slow5_rec_t *slow5_record = slow5_rec_init();
    if(slow5_record == NULL){
        ERROR("%s","Could not allocate space for a slow5 record.");
        exit(EXIT_FAILURE);
    }

    set_record_primary_fields(slow5_record, sp, len_raw_signal, raw_signal, pos, chan, read_number);
    set_record_aux_fields(slow5_record, sp, chan, read_number);

    //write to file
    if(slow5_write(slow5_record, sp) < 0){
        ERROR("%s","Error writing record!");
        exit(EXIT_FAILURE);
    }
    //free the slow5 record
    slow5_rec_free(slow5_record);
}

static void islow5_open(chan_t *chan, int mypos, int32_t channel){
    char path[4096];
    sprintf(path, "%s/pos%d/chan%d_%d.iblow5", opt->dir, mypos, channel, chan->c_islow5);
    chan->fp = fopen(path, "w");
    F_CHK(chan->fp, path);
    if(fwrite("ISLOW5\1", 1, 7, chan->fp) != 7){
        ERROR("Error in fwrite. %s",strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(fwrite(&chan->read_number, sizeof(int32_t), 1, chan->fp) != 1){
        ERROR("Error in fwrite. %s",strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void islow5_chunk_write(chan_t *chan, int64_t j){
    if(fwrite(&j, sizeof(int64_t), 1, chan->fp) != 1){
        ERROR("Error in fwrite. %s",strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(fwrite(chan->raw_signal, sizeof(int16_t), j, chan->fp) != (size_t) j){
        ERROR("Error in fwrite. %s",strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void islow5_to_slow5(slow5_file_t *sp, int mypos, int32_t channel, int32_t index){
    char path[4096];
    sprintf(path, "%s/pos%d/chan%d_%d.iblow5", opt->dir, mypos, channel, index);
    FILE *fp = fopen(path, "r");
    F_CHK(fp, path);
    char magic[7];
    if(fread(magic, 1, 7, fp) != 7){
        ERROR("Error reading magic number from %s. %s", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(strncmp(magic, "ISLOW5\1", 7) != 0){
        ERROR("Error: %s is not an islow5 file", path);
        exit(EXIT_FAILURE);
    }
    int32_t read_number;
    if(fread(&read_number, sizeof(int32_t), 1, fp) != 1){
        ERROR("Error read read_number from %s. %s", path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    int16_t *raw_signal = (int16_t *)malloc(sizeof(int16_t) * opt->cz);
    MALLOC_CHK(raw_signal);
    uint64_t len_raw_signal = 0;
    int64_t j;
    size_t off = 0;

    while(1){

        if(fread(&j, sizeof(int64_t), 1, fp) != 1){
            break; //todo check for EOF
        }
        len_raw_signal += j;

        raw_signal = (int16_t *)realloc(raw_signal,len_raw_signal*sizeof(int16_t));
        MALLOC_CHK(raw_signal);

        if(fread(raw_signal+off, sizeof(int16_t), j, fp) != (size_t) j){
            break;
        }
        off+=j;
    }

    slow5_rec_t *slow5_record = slow5_rec_init();
    if(slow5_record == NULL){
        ERROR("%s","Could not allocate space for a slow5 record.");
        exit(EXIT_FAILURE);
    }

    set_record_primary_fields(slow5_record, sp, len_raw_signal, raw_signal, mypos, channel, read_number);
    set_record_aux_fields(slow5_record, sp, channel, read_number);

    //write to file
    if(slow5_write(slow5_record, sp) < 0){
        ERROR("%s","Error writing record!");
        exit(EXIT_FAILURE);
    }
    //free the slow5 record
    slow5_rec_free(slow5_record);

    free(raw_signal);

    fclose(fp);

    if(remove(path) != 0){
        WARNING("Error deleting %s: %s", path, strerror(errno));
    }
}

static void islow5_close(chan_t *chan){
    fclose(chan->fp);
}

static slow5_file_t *slow5_initialise(int mypos, int type){
   //open the SLOW5 file for writing
    char path[4096];
    sprintf(path, "%s/pos%d_%d.blow5", opt->dir, mypos,type);
    slow5_file_t *sp = slow5_open(path, "w");
    if(sp==NULL){
        ERROR("%s","Error opening file!");
        exit(EXIT_FAILURE);
    }

    if(slow5_set_press(sp, SLOW5_COMPRESS_ZSTD, SLOW5_COMPRESS_SVB_ZD) < 0){
        ERROR("%s","Error setting compression method!");
        exit(EXIT_FAILURE);
    }

    set_header_attributes(sp);
    set_header_aux_fields(sp);

    if(slow5_hdr_write(sp) < 0){
        ERROR("%s","Error writing header!");
        exit(EXIT_FAILURE);
    }

    return sp;
}

void *seq_aq_w(void *ptarg){
    ptarg_t *arg = (ptarg_t*)ptarg;
    int mypos = arg->mypos;
    prom_t *prom = arg->prom;
    pos_t *pos = prom->pos[mypos];

    double realtime0 = realtime();
    fprintf(stderr,"[%.3f] starting aquisition on pos %d\n", realtime()-realtime0, mypos);

    int64_t ran = opt->seed;
    grng_t* generator=init_grng(opt->seed+1, 2.0, opt->mean_slen/2);

    slow5_file_t *sp = slow5_initialise(mypos, 0);
    int aq_done = 0;
    int slow5_done = 0;
    int islow5_done = 0;

    for(int it=0; it < opt->iterations; it++){

        double t0 = realtime();

        for(int i=0; i < pos->nchan; i++){
            chan_t *chan = pos->c[i];

            if(chan->len_raw_signal == 0){
                chan->len_raw_signal = (uint64_t)grng(generator);
                chan->aq = 0;
                chan->chunk_number=0;
                LOG_TRACE("channel %d pos %d: read %d (%ld samples) started", i, mypos, chan->read_number, chan->len_raw_signal);
            }

            if(chan->aq < chan->len_raw_signal){
                int16_t st = 500;
                int j = 0;
                for(j=0;j<opt->cz && chan->aq+j<chan->len_raw_signal ;j++){
                    chan->raw_signal[j] = st+round(rng(&ran)*1000-500);
                }
                chan->chunk_number++;
                if(chan->chunk_number==1){
                    if(chan->aq+j == chan->len_raw_signal){ //directly write to bLOW5 if the read is short and thus fits in one chunk

                        LOG_TRACE("channel %d pos %d: read %d (chunk %d, samples %ld/%ld) written to SLOW5", i, mypos, chan->read_number, chan->chunk_number-1, chan->aq+j, chan->len_raw_signal);
                        slow5fy(sp, chan->len_raw_signal, chan->raw_signal, mypos, i, chan->read_number);
                        slow5_done++;

                    } else { //if the read is long, write to an intermediate file (for now in a very inefficient - without even compressing the chunk)
                        LOG_TRACE("channel %d pos %d: read %d (chunk %d, samples %ld/%ld) written to ISLOW5", i, mypos, chan->read_number, chan->chunk_number, chan->aq+j, chan->len_raw_signal);
                        islow5_open(chan, mypos, i);
                        islow5_chunk_write(chan, j);
                    }

                } else {
                    LOG_TRACE("channel %d pos %d: read %d (chunk %d, samples %ld/%ld) written to ISLOW5", i, mypos, chan->read_number, chan->chunk_number, chan->aq+j, chan->len_raw_signal);
                    islow5_chunk_write(chan, j);
                }
                chan->aq += j;

            }
            if(chan->aq == chan->len_raw_signal){
                if(chan->chunk_number>1){
                    islow5_close(chan);
                    chan->c_islow5++;
                    islow5_done++;
                }
                LOG_TRACE("channel %d pos %d: read %d (samples %ld) done", i, mypos, chan->read_number, chan->len_raw_signal);
                pos->total_samples += chan->len_raw_signal;
                chan->len_raw_signal = 0;
                chan->read_number++;
                aq_done++;

            }

        }
        if(fflush(sp->fp) != 0){
            ERROR("%s","Error flushing slow5 file!\n");
            exit(EXIT_FAILURE);
        }

        double t1 = realtime();
        double elapsed = t1 - t0;
        double s = opt->ct-elapsed;
        if(s<0){
            WARNING("[%.3f] pos %d: aquisition+write is lagging: %f need to be %d", realtime()-realtime0, mypos, elapsed, opt->ct);
        }
        else{
            fprintf(stderr,"[%.3f] pos %d: reads done aquisition %d, dwrite %d, iwrite %d \n", realtime()-realtime0, mypos, aq_done, slow5_done, islow5_done);
            sleep(s);
        }
        pos->c_direct=slow5_done;
    }

    int half_done = 0;
    int sum_read_number = 0;
    for(int i=0; i < pos->nchan; i++){
        chan_t *chan = pos->c[i];
        if(chan->aq>0 && chan->aq < chan->len_raw_signal){
            islow5_close(chan);
            char path[4096];
            sprintf(path, "%s/pos%d/chan%d_%d.iblow5", opt->dir, mypos, i, chan->c_islow5);
            LOG_TRACE("Deleting half done temp file %s", path);
            if(remove(path) != 0){
                ERROR("Error deleting temp file %s", path);
                exit(EXIT_FAILURE);
            }
            half_done++;
        }
        sum_read_number += chan->read_number;
    }
    LOG_TRACE("Half done temp files deleted %d", half_done);
    assert(aq_done == slow5_done + islow5_done);
    assert(aq_done == sum_read_number);

    slow5_close(sp);
    free_grng(generator);
    pos->aq_done = 1;

    pthread_exit(0);
}

void *iwrite2dwrite(void *ptarg){
    ptarg_t *arg = (ptarg_t*)ptarg;
    int mypos = arg->mypos;
    prom_t *prom = arg->prom;
    pos_t *pos = prom->pos[mypos];

    double realtime0 = realtime();

    VERBOSE("Hi from slow5fier for pos %d", mypos);
    slow5_file_t *sp = slow5_initialise(mypos, 1);

    int done_s = 0;

    sleep(opt->ct+1);

    int cont = 2;

    while(cont>0){

        double t0 = realtime();

        for(int i=0; i < pos->nchan; i++){
            chan_t *chan = pos->c[i];

            int32_t aq_n = chan->c_islow5;
            int32_t s_n = chan->c_s;

            if (s_n < aq_n){
                for(int32_t j=s_n; j < aq_n; j++){
                    //serialise the intermediate binary file into BLOW5
                    //for now doing in an inefficient way (if the chunks in the intermediate format were already compressed,
                    //those chunks can be directly copied over without decompressing - yes, SLOW5 spec supports per-chunk compression)
                    islow5_to_slow5(sp, mypos, i, j);
                    done_s++;
                }
                chan->c_s = aq_n;
            }

        }

        if(fflush(sp->fp) != 0){
            ERROR("%s","Error flushing slow5 file!\n");
            exit(EXIT_FAILURE);
        }
        double t1 = realtime();
        double elapsed = t1 - t0;
        double s = opt->ct-elapsed;
        if(s<0){
            WARNING("[%.3f] pos %d: iwrite->dwrite is lagging: %f need to be %d", realtime() - realtime0, mypos, elapsed, opt->ct);
        }
        else{
            fprintf(stderr,"[%.3f] pos %d: iwrite->dwrite %d reads done\n", realtime() - realtime0, mypos, done_s);
            sleep(s);
        }

        if(pos->aq_done){
            cont--;
        }
        pos->c_s = done_s;

    }


    slow5_close(sp);

    pos->s_done = 1;

    char path[4096];
    sprintf(path, "%s/pos%d", opt->dir, mypos);
    int ret = remove(path);
    if (ret != 0) {
        WARNING("Error deleting temp dir %s. %s", path, strerror(errno));
    }

    pthread_exit(0);
}


void *pseudobasecaller(void *ptarg){
    ptarg_t *arg = (ptarg_t*)ptarg;
    int mypos = arg->mypos;
    prom_t *prom = arg->prom;
    pos_t *pos = prom->pos[mypos];

    double realtime0 = realtime();
    VERBOSE("Hi from pseudobasecaller for pos %d", mypos);

    int done_bd = 0;
    int done_bs = 0;

    sleep(opt->ct*2+1);
    int cont = 2;

    char path[4096];
    sprintf(path, "%s/pos%d_0.blow5", opt->dir, mypos);
    slow5_file_t *sp0 = slow5_open(path, "r");
    if(sp0==NULL){
        ERROR("%s","Error opening file!\n");
        exit(EXIT_FAILURE);
    }
    sprintf(path, "%s/pos%d_1.blow5", opt->dir, mypos);
    slow5_file_t *sp1 = slow5_open(path, "r");
    if(sp1==NULL){
        ERROR("%s","Error opening file!\n");
        exit(EXIT_FAILURE);
    }

    slow5_rec_t *rec0 = NULL;
    slow5_rec_t *rec1 = NULL;
    int ret=0;

    int64_t samples = 0;

    while(cont>0){

        double t0 = realtime();

        int64_t s_n = pos->c_direct;
        int64_t b_n = pos->c_bd;

        if (b_n < s_n){
            for(int32_t j=b_n; j < s_n; j++){
                ret = slow5_get_next(&rec0,sp0);
                if(ret<0){
                    ERROR("%s","Error reading slow5 file!\n");
                    exit(EXIT_FAILURE);
                }
                samples += rec0->len_raw_signal;
                done_bd++;
            }
            pos->c_bd = s_n;
        }

        s_n = pos->c_s;
        b_n = pos->c_bs;

        if (b_n < s_n){
            for(int32_t j=b_n; j < s_n; j++){
                ret = slow5_get_next(&rec1,sp1); //this is just to simulate the reading workload of basecalling. We read the whole record from disk, but do not actually do actual basecalling
                if(ret<0){
                    ERROR("%s","Error reading slow5 file!\n");
                    exit(EXIT_FAILURE);
                }
                samples += rec1->len_raw_signal;
                done_bs++;
            }
            pos->c_bs = s_n;
        }

        double t1 = realtime();
        double elapsed = t1 - t0;
        double s = opt->ct-elapsed;
        if(s<0){
            WARNING("[%.3f] pos %d: pseudobasecalling is lagging: %f need to be %d", realtime()-realtime0, mypos, elapsed, opt->ct);
        }
        else{
            fprintf(stderr,"[%.3f] pos %d: pseudobasecalled %d reads (%d+%d), %ld samples\n", realtime()-realtime0, mypos, done_bd+done_bs, done_bd, done_bs, samples);
            sleep(s);
        }

        if(pos->aq_done && pos->s_done){
            cont--;
        }

    }

    ret = slow5_get_next(&rec0,sp0);
    if(ret != SLOW5_ERR_EOF){  //check if proper end of file has been reached
        ERROR("EOF not properly reached. Return code %d\n",ret);
        exit(EXIT_FAILURE);
    }
    ret = slow5_get_next(&rec1,sp1);
    if(ret != SLOW5_ERR_EOF){  //check if proper end of file has been reached
        ERROR("EOF not properly reached. Return code %d\n",ret);
        exit(EXIT_FAILURE);
    }

    slow5_rec_free(rec0);
    slow5_rec_free(rec1);
    slow5_close(sp0);
    slow5_close(sp1);

    fprintf(stderr,"[%.3f] pos %d: total samples %ld, pseudobasecalled samples %ld\n",realtime()-realtime0, mypos, pos->total_samples, samples);
    assert(pos->total_samples == samples);

    pthread_exit(0);
}
