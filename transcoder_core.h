extern "C" {
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <x264.h>
#include <signal.h>
#include <stdio.h>
#include <json-c/json.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
}


#include <unordered_set>
#include <unordered_map>
#include <list>
#include <vector>
#include <queue>
#include <string>
#include <iostream>
#include <fstream>

using namespace std;

#define MAX_OUTPUT_NUM 10
#define X264_CTRL_SIZE 100

//typedef struct x264_control
//{
//    int id;
//    int multi_version;
//    int used;
//    x264_t *h[MAX_OUTPUT_NUM];
//    int call_cnt;
//    int x_num;
//    int skip_call;
//    pthread_t tid;
//} x264_control_t;
//
//void free_ctrl(x264_control_t *x_ctrl);
//x264_control_t *get_x264_ctrl(int x_num);
//void set_injected_version(int v);

typedef struct x264_t x264_t;
typedef struct input_t
{
    string filename;
    int width;
    int height;
	int bitrate;
    AVPixelFormat pix_fmt;

    long video_start_pts;
    int video_bit_rate;
    int64_t duration;
    pthread_mutex_t file_size_mutex;
    int64_t file_size;

    AVRational stream_timebase;
    AVRational timebase;
    AVRational dec_timebase;
    AVRational framerate;
    int bits_per_raw_sample;
    int ticks_per_frame;
    int pkt_duration;

    int video_delay;
    bool has_audio;

    AVRational avg_frame_rate;
    AVRational r_frame_rate;
    int video_nb_frames;
} input_t;

typedef struct output_t
{
    string filename;
	int width;
	int height;
	int bitrate;

    AVFormatContext *fmt_ctx;
    AVStream *video_st;

	enum AVPixelFormat pix_fmt;

    string tmp_dir;
    string fmt;

    int enc_gop_cnt;
    pthread_mutex_t enc_gop_cnt_mutex;
}output_t;

typedef struct period_t
{
    int valid;
	long start;
	long end;
} period_t;

typedef struct Gop
{
    //id of this gop
	int gop_id;

    //number of frames that actually exists
	int64_t frame_cnt;
	pthread_mutex_t frame_cnt_mutex;

    //frame data of this gop
    AVFrame **frames;

	int capacity;

    //one dimension array which stores the validty of each frame
	uint8_t *valid;

    int64_t pkts_size;

    /* every gop may be used to generate multiple outputs */
    list<list<output_t *>> reso_opts_lists; 
    int fetch_cnt;
    int ret_cnt;
} Gop;

typedef struct Gop_wrapper
{
    Gop *g;
    vector<output_t *> opts;
} Gop_wrapper;

typedef struct gop_buffer_t
{
    unordered_map<int, Gop *> proc_gop_buf;
    unordered_map<long, list<Gop *>> reso_full_gop_buf;
    unordered_set<Gop *> full_gop_buf;
    list<Gop *> idle_gop_buf;

    pthread_mutex_t fgb_mutex;
    pthread_mutex_t pgb_mutex;
    pthread_mutex_t igb_mutex;
    pthread_mutex_t rfgb_mutex;

    int gop_size;
    int buf_size;

} gop_buffer_t;

typedef struct encode_t
{
    AVFormatContext *fmt_ctx;
    AVOutputFormat *out_fmt;
    AVStream *video_st;
    AVCodecContext *ctx;
    AVCodec *codec;
    struct SwsContext *sws_ctx;
    AVFrame *scaled_frame;
    AVPacket pkt;

    x264_control_t *x264_ctrl;

} encode_t;

typedef struct decode_t
{
    AVFormatContext *fmt_ctx;
	AVStream *streams[2];
	AVCodecContext *ctx[2];
    AVCodec *codec[2];

    bool video_dec_opened;
    bool audio_dec_opened;

    bool has_audio;
} decode_t;

typedef struct threads_info_t
{
    int tcnt;

    int live_thr_cnt;
    pthread_mutex_t live_thr_cnt_mutx;

    /* how many frames are encoded by each thread */
    pthread_mutex_t t_frames_mutx;
    unordered_map<pthread_t, int> t_frames_map;

    /* every thread may act as encoding role, so prepare encode_t for it */
    pthread_mutex_t encoders_mutex;
    unordered_map<pthread_t, vector<encode_t *>> encoders_map;

    /* every thread may act as decoding role, so prepare decode_t for it */
    pthread_mutex_t decoder_mutex;
    unordered_map<pthread_t, decode_t *> decoder_map;

    pthread_mutex_t enc_running_flag_mutex;
    unordered_map<pthread_t, bool> enc_running_flag;
} threads_info_t;

enum TRANS_STATUS {
    CLIP_NOT_START_YET,
    CLIPPING,
    CLIP_FINISHED,

    EXIST_PEROID,
    NO_PERIOD,
    DEC_FINISHED
};


typedef struct trans_ctx_t
{
    /* basic input*/
    input_t ipt;

    /* basic output */
    vector<output_t *> opt;
    unordered_map<long, list<output_t *>> reso_opts_map;
    pthread_mutex_t gop_cnt_mutex;
    int gop_cnt;

    /*clip video*/
    pthread_mutex_t periods_mutex;
    list<period_t *> periods;
    int periods_cnt;
    int clip_step;
    pthread_cond_t periods_avail;
    enum TRANS_STATUS clip_status;
    pthread_mutex_t clip_status_mutex;
    
    /* decoded ring buffer data */
    gop_buffer_t gbt;
    pthread_mutex_t fetch_periods_cnt_mutex;  
    int fetch_periods_cnt;
    pthread_mutex_t dec_periods_cnt_mutex;
    int dec_periods_cnt;
    enum TRANS_STATUS dec_status;
    
    /* infomation about threads */
    /*threads_info_t t_info;*/

    int frame_cnt;

    int last_enc_frame_cnt;
    int enc_frame_cnt;
    int tick_cnt;

    /* 0 means before stable, 1 means in stable, 2 means after stable */
    pthread_mutex_t enc_state_mutex;
    int enc_state;
    int before_stable_frame_cnt;
    int in_stable_frame_cnt;
    int after_stable_frame_cnt;

    /*x264 library*/
    int inject;
    pthread_mutex_t x_ctrls_mutex;
    unordered_map<pthread_t, x264_control_t *> thr_xctrl_map;

} trans_ctx_t;

typedef struct core_t
{
    int core_cnt;
    int next_core_id;
    pthread_mutex_t core_mutex;
} core_t;

typedef struct thread_inst_t
{
   queue<trans_ctx_t *> tctx_queue;
   sem_t tctx_sem;
} thread_inst_t;

typedef struct thread_pool_t
{
    unordered_map<pthread_t, thread_inst_t *> threads_map;
    pthread_mutex_t threads_map_mutex;
    pthread_barrier_t pool_barrier;
} thread_pool_t;

void init();
int init_task(json_object *jobj);
int init_gop_buffer(int buf_size, int gop_size);
void init_input(trans_ctx_t *tctx);
void init_output(trans_ctx_t *tctx, output_t *opt);
void init_threads_info(trans_ctx_t *tctx, int t_num);
void init_trans_ctx(trans_ctx_t *tctx, int t_num);

void launch_thread(void *arg);
void launch_transcoding(trans_ctx_t *tctx);
void add_tctx(trans_ctx_t *tctx, vector<pthread_t>& thr_vec);

//encoding function declaration
int init_encode_t(trans_ctx_t *tctx, encode_t *c, output_t *opt);
int destroy_encode_t(encode_t *c);
int open_encoder(encode_t *c);
int close_encoder(encode_t *c);
int open_output_gopfile (Gop *g, output_t *opt, encode_t *c);
int close_output_gopfile (encode_t *c);
int flush_multi_encoder (trans_ctx_t *tctx, vector<encode_t *> multi_enc_t);
int multi_scale_encode (void *arg);
int multi_scale_encode_gop(trans_ctx_t *tctx, Gop *g, vector<encode_t *>& encoders);
int scale_encode(void *arg);
int scale_encode_gop(trans_ctx_t *tctx, Gop *g, int opt_idx);

//decoding function declaration
void open_decoder (const char *filname, decode_t *dec_t);
int clip_video(trans_ctx_t *tctx);
void decode_flush(decode_t *dec_t, period_t *p, trans_ctx_t *tctx);
period_t *get_period(trans_ctx_t *tctx);
period_t *gen_period(long start, long end, int valid);
void decode_period(trans_ctx_t *tctx);

//gop related function declartion
void init_gop_buffer(trans_ctx_t *tctx);
void reset_gop(Gop *g, trans_ctx_t *tctx);
void add_frame_to_pgb(AVFrame *frame, trans_ctx_t *tctx);
Gop_wrapper *get_gop_from_reso_fgb(trans_ctx_t *tctx);
void insert_gop_to_reso_fgb(Gop *g, trans_ctx_t *tctx);
void insert_gop_to_fgb(Gop *g, trans_ctx_t *tctx);
void rm_gop_from_fgb(Gop *g, trans_ctx_t *tctx);
int prefetch_gops_from_pgb(vector<int>& gop_ids, trans_ctx_t *tctx);
Gop *get_gop_from_igb(trans_ctx_t *tctx);
void insert_gop_to_igb(Gop *g, trans_ctx_t *tctx);
void rm_gop_from_pgb(int gop_id, trans_ctx_t *tctx);
Gop *get_gop_from_pgb(int gop_id, trans_ctx_t *tctx);

//core binding function declaration
void init_core_t();
void bind_core();

//output function declaration
void print_average_fps();
void save_frame (AVFrame *frame, int width, int height, int iframe);
void atomic_inc(int *var, int m, sem_t *lock);
void atomic_dec(int *var, int m, sem_t *lock);
/* void output_video(trans_ctx_t *tctx); */
void output_video(trans_ctx_t *tctx, output_t *opt);
//x264 function declaration
// x264_control_t *get_x264_ctrl(int x_num);
#define SUCCESS_CALL(msg, func, ...)    \
    do {                                \
        int ret = func;                 \
        if (ret < 0) {                   \
            printf(msg);                \
            return ret;                 \
        }                               \
    } while(0);

#define atomic(mutex, statement)        \
    pthread_mutex_lock((mutex));        \
    do {                                \
        statement;                      \
    } while(0);                         \
    pthread_mutex_unlock((mutex)); 

