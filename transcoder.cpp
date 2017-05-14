#include "transcoder_core.h"

extern unordered_map<int, trans_ctx_t *>  id_tctx_map;
unordered_map<trans_ctx_t *, list<pthread_t> > tctx_thrs_map;
extern core_t cores;
extern thread_pool_t thr_pool;

int init_task(json_object *jobj)
{
    printf("init task____\n");
    cout << json_object_get_string(json_object_object_get(jobj,"command")) << endl;

    /* get the input conf file */
    string ipt_name;
    int task_num;
    int opt_num;
    int trans_id;
   
    trans_ctx_t *tctx = new trans_ctx_t;
    tctx->ipt.filename ="./videos/" + (string)json_object_get_string(json_object_object_get(jobj,"videoname"));
    opt_num = json_object_get_int(json_object_object_get(jobj,"num"));
    trans_id = json_object_get_int(json_object_object_get(jobj,"id"));
        
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

    init_trans_ctx(tctx, 40);
    id_tctx_map[trans_id] = tctx;
    vector<pthread_t> tmp;

    // thread scheduling
    if (tctx_thrs_map.size() == 0)
    {
        list<pthread_t> thr_list;
        for (auto iter = thr_pool.threads_map.begin(); iter != thr_pool.threads_map.end(); iter++)
        {
            thr_list.push_back(iter->first);
            tmp.push_back(iter->first);
        }

        tctx_thrs_map[tctx] = thr_list;  
    } else {

        int alive_tctx_cnt = tctx_thrs_map.size(); 

        int avg_thr_cnt = cores.core_cnt/(alive_tctx_cnt + 1);
        int left_thr_cnt = cores.core_cnt - avg_thr_cnt * alive_tctx_cnt;

        cout << "left_thr_cnt" << left_thr_cnt << endl;

        int i = 0;
        list<pthread_t> new_thr_list;
        while (i < left_thr_cnt)
        {
            for (auto iter = tctx_thrs_map.begin(); iter != tctx_thrs_map.end(); iter++)
            {
                list<pthread_t> &thr_list = iter->second;
                pthread_t tid = thr_list.front();
                tmp.push_back(tid);
                new_thr_list.push_back(tid);
                thr_list.pop_front();

                thr_pool.threads_map[tid]->schedule = true;

                if (++i == left_thr_cnt)
                    break;
            }
        }

        tctx_thrs_map[tctx] = new_thr_list;
    }

    add_tctx(tctx, tmp);

    /*t_ctxs.push_back(tctx);*/
    /*clip_video(tctx);*/
    /*alarm(1);*/
    /*signal(SIGALRM, print_fps);*/
    return 0;
}

