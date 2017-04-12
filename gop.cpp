#include "transcoder_core.h"

void add_frame_to_pgb(AVFrame *frame, trans_ctx_t *tctx)
{
    int pict_idx = av_rescale_q_rnd(frame->pkt_pts - tctx->ipt.video_start_pts,
            tctx->ipt.stream_timebase, tctx->ipt.timebase,
            static_cast<enum AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

    /* int pict_idx = av_rescale_q(frame->pkt_pts - tctx->ipt.video_start_pts, */
            /* tctx->ipt.stream_timebase, tctx->ipt.timebase); */


    /* int pict_idx = (frame->pkt_pts - tctx->ipt.video_start_pts)/frame->pkt_duration; */
    int gop_size = tctx->gbt.gop_size;
    int gop_id = pict_idx / gop_size;
    int frame_id = pict_idx % gop_size;

    /* STEP 1: find one gop which the frame should be stored */
    Gop *g;

    pthread_mutex_lock(&tctx->gbt.pgb_mutex);
    if (tctx->gbt.proc_gop_buf.find(gop_id) != tctx->gbt.proc_gop_buf.end())
        g = tctx->gbt.proc_gop_buf[gop_id];
    else{
        /*printf("no gop \n");*/
        cout << gop_id << " " << endl;
        g = NULL;
        exit(0);
    }
    pthread_mutex_unlock(&tctx->gbt.pgb_mutex);
    //while (true) 
    //{
    //    g = get_gop_from_pgb(gop_id, tctx);
    //    if (g != NULL)
    //        break;

    //    cout << "EEEEE" << endl;
    //    scale_encode(tctx);
    //    cout << "DDDDD" << endl;
    //}

    /* STEP 2: put the frame into the gop */
    g->gop_id = gop_id;
    av_image_copy(g->frames[frame_id]->data, g->frames[frame_id]->linesize,
            (const uint8_t **)frame->data, frame->linesize,
            tctx->ipt.pix_fmt, tctx->ipt.width, tctx->ipt.height);

    g->valid[frame_id] = 1; 

    /* cout << "frame id " << frame_id << endl; */

    pthread_mutex_lock(&tctx->ipt.file_size_mutex);
    tctx->ipt.file_size += frame->pkt_size;
    pthread_mutex_unlock(&tctx->ipt.file_size_mutex);

    pthread_mutex_lock(&g->frame_cnt_mutex);
    g->pkts_size += frame->pkt_size;
    g->frame_cnt++;
    pthread_mutex_unlock(&g->frame_cnt_mutex);

    if (g->frame_cnt == gop_size)
    {
        rm_gop_from_pgb(gop_id, tctx);
        insert_gop_to_fgb(g, tctx);
        insert_gop_to_reso_fgb(g, tctx);
    }

    return;
}

Gop_wrapper *get_gop_from_reso_fgb(trans_ctx_t *tctx)
{
    Gop_wrapper *gw = NULL;

    pthread_mutex_lock(&tctx->gbt.rfgb_mutex);
    //for (auto iter = tctx->gbt.reso_full_gop_buf.begin(); iter != tctx->gbt.reso_full_gop_buf.end(); iter++)
    //{
    //    cout << "second size" << iter->second.size() << endl;
    //}

    for (auto iter = tctx->gbt.reso_full_gop_buf.begin(); iter != tctx->gbt.reso_full_gop_buf.end(); iter++)
    {
        if (iter->second.size() == 0)
            continue;

        gw = new Gop_wrapper;

        Gop *g = iter->second.back();
        iter->second.pop_back();
        gw->g = g;

        list<output_t *> &opt_list = g->reso_opts_lists.back();
        for(auto opt_iter = opt_list.begin(); opt_iter != opt_list.end(); opt_iter++)
            gw->opts.push_back(*opt_iter);

        g->reso_opts_lists.pop_back();

        rm_gop_from_fgb(g, tctx);

        break;
    }
    pthread_mutex_unlock(&tctx->gbt.rfgb_mutex);

    return gw;
}

void rm_gop_from_fgb(Gop *g, trans_ctx_t *tctx)
{
    pthread_mutex_lock(&tctx->gbt.fgb_mutex);

    g->fetch_cnt++;
    if (g->fetch_cnt == tctx->reso_opts_map.size())
    {
        tctx->gbt.full_gop_buf.erase(g);
        /*cout << "remove full gop buffer" << to_string(tctx->gbt.full_gop_buf.size()) << endl;*/
    }

    pthread_mutex_unlock(&tctx->gbt.fgb_mutex);
}

void insert_gop_to_fgb(Gop *g, trans_ctx_t *tctx)
{
    pthread_mutex_lock(&tctx->gbt.fgb_mutex);
    tctx->gbt.full_gop_buf.insert(g);
        /*printf("put into full gop buffer %d\n", tctx->gbt.full_gop_buf.size()); */
    pthread_mutex_unlock(&tctx->gbt.fgb_mutex);
}

void insert_gop_to_reso_fgb(Gop *g, trans_ctx_t *tctx)
{
    pthread_mutex_lock(&tctx->gbt.rfgb_mutex);
    for (auto iter = tctx->gbt.reso_full_gop_buf.begin();
            iter != tctx->gbt.reso_full_gop_buf.end(); iter++)
    {
        iter->second.push_back(g);
    }
    pthread_mutex_unlock(&tctx->gbt.rfgb_mutex);
}

Gop *get_gop_from_igb(trans_ctx_t *tctx)
{

    pthread_mutex_lock(&tctx->gbt.igb_mutex);
    if (tctx->gbt.idle_gop_buf.size() == 0)
    {
        pthread_mutex_unlock(&tctx->gbt.igb_mutex);
        return NULL; 
    }

    Gop *g = tctx->gbt.idle_gop_buf.back();
    /* cout << "get an igb, igb size " << tctx->gbt.idle_gop_buf.size() << "g " << g << endl; */
    tctx->gbt.idle_gop_buf.pop_back();
    pthread_mutex_unlock(&tctx->gbt.igb_mutex);

    return g;
}

void insert_gop_to_igb(Gop *g, trans_ctx_t *tctx)
{
    pthread_mutex_lock(&tctx->gbt.igb_mutex);
    g->ret_cnt++;
    if (g->ret_cnt == (int)tctx->reso_opts_map.size())
    {
        reset_gop(g, tctx);
        tctx->gbt.idle_gop_buf.push_back(g);
        /*sem_post(&tctx->gbt.igb_sem);*/
    }
    pthread_mutex_unlock(&tctx->gbt.igb_mutex);

    /* printf("return to idle\n"); */
}

void rm_gop_from_pgb(int gop_id, trans_ctx_t *tctx)
{
    pthread_mutex_lock(&tctx->gbt.pgb_mutex);
    tctx->gbt.proc_gop_buf.erase(gop_id);
    pthread_mutex_unlock(&tctx->gbt.pgb_mutex);
}

int prefetch_gops_from_pgb(vector<int>& gop_ids, trans_ctx_t *tctx)
{
    /*cout << "begin pretecth" << endl;*/
    vector<int> absent_gops;
    vector<Gop *> avail_idle_gops;

    /*whichs gops are not already in the processing gop buffer*/
    pthread_mutex_lock(&tctx->gbt.pgb_mutex);
    for (int i = 0; i < (int)gop_ids.size(); i++)
    {
        if (tctx->gbt.proc_gop_buf.find(gop_ids[i]) == tctx->gbt.proc_gop_buf.end())
            absent_gops.push_back(gop_ids[i]);
    }

    if (absent_gops.size() == 0)
        goto all_in_pgb;

    /*fetch the gops from idle gop buffer*/
    pthread_mutex_lock(&tctx->gbt.igb_mutex);
    if (tctx->gbt.idle_gop_buf.size() < absent_gops.size())
    {
        pthread_mutex_unlock(&tctx->gbt.igb_mutex);
        goto lack_idle_gop;
    }

    for (int i = 0; i < (int)absent_gops.size(); i++)
    {
        /*Gop *g = get_gop_from_igb(tctx); */
        /*sem_wait(&tctx->gbt.igb_sem);*/
        Gop *g = tctx->gbt.idle_gop_buf.back();
        tctx->gbt.idle_gop_buf.pop_back();
        avail_idle_gops.push_back(g);
    }
    pthread_mutex_unlock(&tctx->gbt.igb_mutex);

    /*put the gops into processing gop buffer*/
    for (int i = 0; i < (int)avail_idle_gops.size(); i++)
    {
        avail_idle_gops[i]->gop_id = absent_gops[i];
        tctx->gbt.proc_gop_buf[absent_gops[i]] = avail_idle_gops[i];
    }

    /*cout << "end pretecth" << endl;*/
all_in_pgb:
    pthread_mutex_unlock(&tctx->gbt.pgb_mutex);
    return 0;

lack_idle_gop:
    pthread_mutex_unlock(&tctx->gbt.pgb_mutex);
    return -1;
}

Gop *get_gop_from_pgb(int gop_id, trans_ctx_t *tctx)
{
    Gop *g = NULL;

    pthread_mutex_lock(&tctx->gbt.pgb_mutex);
    if (tctx->gbt.proc_gop_buf.find(gop_id) != tctx->gbt.proc_gop_buf.end())
        g = tctx->gbt.proc_gop_buf[gop_id];
    else
        cout << "can not find a gop" << endl;
    pthread_mutex_unlock(&tctx->gbt.pgb_mutex);

    if (g != NULL)
        return g;

    g = get_gop_from_igb(tctx);

    if (g == NULL)
        return g;

    pthread_mutex_lock(&tctx->gbt.pgb_mutex);
    tctx->gbt.proc_gop_buf[gop_id] = g;
    pthread_mutex_unlock(&tctx->gbt.pgb_mutex);

    return g;
}

void reset_gop(Gop *g, trans_ctx_t *tctx)
{
    g->gop_id = -1;
    g->frame_cnt = 0;
    g->ret_cnt = 0;
    g->fetch_cnt = 0;
    g->pkts_size = 0;

    for(auto iter = tctx->reso_opts_map.begin(); iter != tctx->reso_opts_map.end(); iter++)
        g->reso_opts_lists.push_back(iter->second);

    /* for (int i = 0; i < tctx->gbt.gop_size; i++) */
        /* av_free(g->frames[i]); */

    /* memset(g->frames, 0, sizeof(AVFrame *) * tctx->gbt.gop_size); */
    memset(g->valid, 0, sizeof(uint8_t) * tctx->gbt.gop_size);
}
