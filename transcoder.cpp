#include "transcoder_core.h"

int init_task(json_object *jobj)
{
    printf("init task____\n");
    cout << json_object_get_string(json_object_object_get(jobj,"command")) << endl;

    /* get the input conf file */
    string ipt_name;
    int task_num;
    int opt_num;
   
    trans_ctx_t *tctx = new trans_ctx_t;
    tctx->ipt.filename ="./videos/" + (string)json_object_get_string(json_object_object_get(jobj,"videoname"));
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
    return 0;
}

