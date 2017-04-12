#include "transcoder_core.h"

/*x264 library lock*/
pthread_mutex_t x264_t_lock = PTHREAD_MUTEX_INITIALIZER;

vector<trans_ctx_t *> t_ctxs;
extern core_t cores;
thread_pool_t thr_pool;

int init_task(json_object *jobj)
{
    printf("init task____\n");
    struct timeval start, end;
    gettimeofday(&start, NULL);
    init();

    pthread_mutex_init(&thr_pool.threads_map_mutex, NULL);
    pthread_barrier_init(&thr_pool.pool_barrier, NULL, cores.core_cnt + 1);

    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * cores.core_cnt);
    for (int j = 0; j < cores.core_cnt; j++)
        pthread_create(&threads[j], NULL, (void * (*)(void *))launch_thread, NULL);

    pthread_barrier_wait(&thr_pool.pool_barrier);
    cout << json_object_get_string(json_object_object_get(jobj,"command")) << endl;
    /* get the input conf file */
    string ipt_name;
    int task_num;
    int opt_num;
   
    trans_ctx_t *tctx = new trans_ctx_t;
    tctx->ipt.filename = json_object_get_string(json_object_object_get(jobj,"videoname"));
    opt_num = json_object_get_int(json_object_object_get(jobj,"num"));
        
    json_object *jvalue;
    json_object *jarray = json_object_object_get(jobj,"targetList");
    for (int i = 0; i < opt_num; i++)
    {
        jvalue = json_object_array_get_idx(jarray, i);
        output_t *opt = new output_t;
        opt->filename = json_object_get_string(json_object_object_get(jvalue,"filename"));
        opt->fmt = json_object_get_string(json_object_object_get(jvalue,"fmt"));;
        opt->width = json_object_get_int(json_object_object_get(jvalue,"width"));;
        opt->height = json_object_get_int(json_object_object_get(jvalue,"height"));
        opt->bitrate = json_object_get_int(json_object_object_get(jvalue,"bitrate"));

        opt->pix_fmt = AV_PIX_FMT_YUV420P;
        opt->tmp_dir = "." + opt->filename + to_string(time(nullptr));

        mkdir(opt->tmp_dir.c_str(), ACCESSPERMS);
        tctx->opt.push_back(opt);

        if (tctx->reso_opts_map.find(opt->width * opt->height) == tctx->reso_opts_map.end())
        {
            list<output_t *> opts;
            opts.push_back(opt);
            tctx->reso_opts_map[opt->width * opt->height] = opts;
        } else {
            tctx->reso_opts_map[opt->width * opt->height].push_back(opt);
        }
    }

        cout << "filename " << tctx->ipt.filename << endl; 
        cout << "map size " << tctx->reso_opts_map.size() << endl;

        vector<pthread_t> tmp;
        init_trans_ctx(tctx, 40);
        add_tctx(tctx, tmp);
        /*t_ctxs.push_back(tctx);*/
        /*clip_video(tctx);*/
      

    /*alarm(1);*/
    /*signal(SIGALRM, print_fps);*/

    for (int j = 0; j < cores.core_cnt; j++)
        pthread_join(threads[j], NULL);

    return 0;
}
