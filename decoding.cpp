#include "transcoder_core.h"
#include <list>

using namespace std;

extern thread_pool_t thr_pool;
void open_decoder (const char *filename, decode_t *dec_t)
{
    dec_t->fmt_ctx = NULL;

    /* open input file, and allocate format context */
    if (avformat_open_input(&dec_t->fmt_ctx, filename, NULL, NULL) < 0)
    {
        cout << "could not open input file " << filename << endl;
        exit(1);
	}
    if (avformat_find_stream_info(dec_t->fmt_ctx, NULL) < 0)
    {
        printf("could not open stream info\n");
        exit(1);
    }

    /* find the video stream */
    dec_t->video_dec_opened = false;
	int vst_idx = av_find_best_stream(dec_t->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (vst_idx >= 0)
    {
	    dec_t->streams[0] = dec_t->fmt_ctx->streams[vst_idx];
	    dec_t->ctx[0] = dec_t->streams[0]->codec;
	    dec_t->codec[0] = avcodec_find_decoder(dec_t->ctx[0]->codec_id);
	    dec_t->ctx[0]->thread_count = 1;

        if (dec_t->codec[0] && avcodec_open2(dec_t->ctx[0], dec_t->codec[0], NULL) >= 0)
            dec_t->video_dec_opened = true;
    }

    int ast_idx = av_find_best_stream(dec_t->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (ast_idx >= 0)
    {
        dec_t->has_audio = true;
    }
}

void launch_transcoding(trans_ctx_t *tctx)
{
    //trans_ctx_t *tctx = (trans_ctx_t *)arg;

    /* cout << "gop_id " << tctx->gbt.idle_gop_buf.front()->gop_id << endl; */

    int opt_num = tctx->opt.size();
    pthread_t tid = pthread_self();

    /* open decoder for every thread */
    decode_t *dec_t = (decode_t *)malloc(sizeof(decode_t));
    open_decoder(tctx->ipt.filename.c_str(), dec_t);

    if (!dec_t->video_dec_opened)
    {
        cout << "video codec is not opened" << endl;
        exit(0);
    }

    /* decoding ... */
    pthread_mutex_lock(&tctx->clip_status_mutex);
    if (tctx->clip_status == CLIP_NOT_START_YET)
    {
        tctx->clip_status = CLIPPING;
        pthread_mutex_unlock(&tctx->clip_status_mutex);

        clip_video(tctx);
        decode_period(tctx);
    }else
    {
        pthread_mutex_unlock(&tctx->clip_status_mutex);
        decode_period(tctx);
    }
}

void launch_thread(void *arg)
{
    bind_core();

    thread_inst_t *thr = new thread_inst_t;
    sem_init(&thr->tctx_sem, 0, 0);

    pthread_mutex_lock(&thr_pool.threads_map_mutex);
    thr_pool.threads_map[pthread_self()] = thr; 
    pthread_mutex_unlock(&thr_pool.threads_map_mutex);

    pthread_barrier_wait(&thr_pool.pool_barrier);

transcoding:
    sem_wait(&thr->tctx_sem);

    trans_ctx_t *tctx = thr->tctx_queue.front();
    thr->tctx_queue.pop();
    thr->schedule = false;

    launch_transcoding(tctx);
    goto transcoding;
}

void add_tctx(trans_ctx_t *tctx, vector<pthread_t>& thr_vec)
{
    for (int i = 0; i < thr_vec.size(); i++)
    {
        thread_inst_t *thr = thr_pool.threads_map[thr_vec[i]];
        thr->tctx_queue.push(tctx);
        sem_post(&thr->tctx_sem);
    }
//    for(auto iter = thr_pool.threads_map.begin();
//            iter != thr_pool.threads_map.end(); iter++)
//    {
//        thread_inst_t *thr = thr_pool.threads_map[iter->first];
//        thr->tctx_queue.push(tctx);
//        sem_post(&thr->tctx_sem);
//    }
}

period_t *get_period(trans_ctx_t *tctx)
{
    period_t *period = NULL;

    pthread_mutex_lock(&tctx->periods_mutex);

wake_up:
    if (tctx->periods.size() > 0)
    {
        period = tctx->periods.front();
        tctx->periods.pop_front();
        pthread_mutex_unlock(&tctx->periods_mutex);

        return period;
    } else {
        
        if (tctx->dec_status != EXIST_PEROID)
        {
            pthread_mutex_unlock(&tctx->periods_mutex);
            return NULL;
        }

        pthread_cond_wait(&tctx->periods_avail, &tctx->periods_mutex);
        goto wake_up;
    }
}

int clip_video(trans_ctx_t *tctx)
{
    decode_t *dec_t = (decode_t *)malloc(sizeof(decode_t));
    open_decoder(tctx->ipt.filename.c_str(), dec_t);

    cout << "clip video" << endl;
    /* cut the video file into multiple period for parallel decoding */
    AVPacket pkt;
    AVFrame *frame = av_frame_alloc();
    int got_frame;
    
    int video_stream_idx = dec_t->streams[0]->index;

    long pkt_duration = 0;
    long start_pts = -1;
    long end_pts = 0;

    while (av_seek_frame(dec_t->fmt_ctx, video_stream_idx, end_pts, AVSEEK_FLAG_BACKWARD) >= 0)
    {
        while (av_read_frame(dec_t->fmt_ctx, &pkt) >= 0)
        {
            if (pkt.stream_index == video_stream_idx &&
                    avcodec_decode_video2(dec_t->ctx[0], frame, &got_frame, &pkt) >= 0) 
            {
                if (!got_frame)
                    continue;

                if (start_pts == -1)
                {
                    tctx->ipt.video_start_pts = frame->pkt_pts;
                    pkt_duration = frame->pkt_duration;
                    tctx->ipt.pkt_duration = pkt_duration;
                    start_pts = frame->pkt_pts;
                    end_pts = frame->pkt_pts + tctx->clip_step * pkt_duration;
                    break;
                }

                end_pts = frame->pkt_pts - 1;

                bool clip_finished = end_pts <= start_pts;
                period_t *period = gen_period(start_pts, clip_finished ? LONG_MAX : end_pts, 1);

                pthread_mutex_lock(&tctx->periods_mutex);
                tctx->periods.push_back(period);
                tctx->periods_cnt += 1;
                tctx->clip_status = clip_finished ? CLIP_FINISHED : CLIPPING;
                pthread_mutex_unlock(&tctx->periods_mutex);

                pthread_cond_signal(&tctx->periods_avail);
                
                if (clip_finished)
                    goto finish_cut;

                /*printf("start %ld end %ld\n", period->start, period->end); */

                start_pts = frame->pkt_pts;
                end_pts = frame->pkt_pts + tctx->clip_step * pkt_duration;
  
                /* flush the decoder */
                pkt.data = NULL;
                pkt.size = 0;
  
                do {
                    got_frame = 0;
                    avcodec_decode_video2(dec_t->ctx[0], frame, &got_frame, &pkt);
                } while (got_frame);

                break;
            }
        }
    }

finish_cut:
    printf("finishing cutting the video to period\n");
    printf("period size %d\n", tctx->periods.size());


	return 0;
}

void decode_flush(decode_t *dec_t, period_t *p, trans_ctx_t *tctx)
{
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    
    int got_frame = 0;
    int ret;

    while (true)
    {
        AVFrame *frame = av_frame_alloc();

        ret = avcodec_decode_video2(dec_t->ctx[0], frame, &got_frame, &pkt);

        if (ret < 0 || !got_frame)
            return;
        
        if (p == NULL || (frame->pkt_pts >= p->start && frame->pkt_pts <= p->end))
        {
            add_frame_to_pgb(frame, tctx);
            /* printf("decode a frame %ld\n", frame->pkt_pts); */
        }

        av_frame_unref(frame);
    }
}

void decode_period(trans_ctx_t *tctx)
{
    /* get one period and decode it, put the decoded frame to process buf */
    period_t *p;
    while ((p = get_period(tctx))) 
    {
        /*cout << "successfully get a period" << endl;*/
        /*
         *now we have successfully get the valid period
         *we need to know whether will it will schedule to encoding because it can
         *not get a gop
         */
        int start_frame_idx = av_rescale_q_rnd(p->start - tctx->ipt.video_start_pts,
            tctx->ipt.stream_timebase, tctx->ipt.timebase,
            static_cast<enum AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

        int start_gop_id = start_frame_idx / tctx->gbt.gop_size;

        int end_gop_id = 0;
        if (p->end == LONG_MAX)
            end_gop_id = tctx->gop_cnt - 1;
        else{
            int end_frame_idx = av_rescale_q_rnd(p->end - tctx->ipt.video_start_pts,
                tctx->ipt.stream_timebase, tctx->ipt.timebase,
                static_cast<enum AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

            end_gop_id = end_frame_idx / tctx->gbt.gop_size;
        }

        vector<int> gop_ids;
        for (int i = start_gop_id; i <= end_gop_id; i++)
            gop_ids.push_back(i);

        if (prefetch_gops_from_pgb(gop_ids, tctx) < 0)
        {
            pthread_mutex_lock(&tctx->periods_mutex);
            tctx->periods.push_front(p);
            pthread_mutex_unlock(&tctx->periods_mutex);

            pthread_cond_signal(&tctx->periods_avail);
            
            /*can be schedule out*/
            thread_inst_t *thr = thr_pool.threads_map[pthread_self()];
            if (thr->schedule)
            {
                cout << "schedule" << endl;
                sem_wait(&thr->tctx_sem);
                tctx = thr->tctx_queue.front();
                thr->tctx_queue.pop();
                thr->schedule = false;
            }

            multi_scale_encode(tctx);
            /*can be schedule out*/
            thr = thr_pool.threads_map[pthread_self()];
            if (thr->schedule)
            {
                cout << "schedule" << endl;
                sem_wait(&thr->tctx_sem);
                tctx = thr->tctx_queue.front();
                thr->tctx_queue.pop();
                thr->schedule = false;
            }

            continue;
        }
            
        atomic(&tctx->fetch_periods_cnt_mutex, tctx->fetch_periods_cnt += 1);

        if (tctx->clip_status == CLIP_FINISHED && tctx->fetch_periods_cnt == tctx->periods_cnt)
        {
            tctx->dec_status = NO_PERIOD;
            pthread_cond_broadcast(&tctx->periods_avail);
        }

        /*decode now*/
         /*printf("get a period period size %ld %ld\n", p->start, p->end); */
        decode_t *dec_t = (decode_t *)malloc(sizeof(decode_t));

        open_decoder(tctx->ipt.filename.c_str(), dec_t);
        if (!dec_t->video_dec_opened)
        {
            printf("video codec not opened\n");
            exit(0); 
        }

        int video_stream_idx = dec_t->streams[0]->index;
        AVPacket pkt;
        int got_frame;

        if(av_seek_frame(dec_t->fmt_ctx, video_stream_idx, p->start, AVSEEK_FLAG_BACKWARD) < 0)
        {
            printf("couln't seek to start frame %ld\n", p->start);
            exit(0);
        }

        while ((av_read_frame(dec_t->fmt_ctx, &pkt) >= 0))
        {
            AVFrame *frame = av_frame_alloc();

            if (pkt.stream_index == video_stream_idx &&
                avcodec_decode_video2(dec_t->ctx[0], frame, &got_frame, &pkt) >= 0)
            {
                if (!got_frame)
                    continue;
                
                if (frame->pkt_pts >= p->start && frame->pkt_pts <= p->end)
                {
                    /* printf("decode a frame %d %d\n", frame->display_picture_number, frame->coded_picture_number); */

                    add_frame_to_pgb(frame, tctx);
                }
                /* fucking this condition */
                else if (frame->pkt_pts > p->end)
                    break;
            }

            av_frame_unref(frame);
        }

        decode_flush(dec_t, p, tctx);

        /*free_decoder*/
        free(dec_t);

        /*cout << "decode finished" << endl;*/
        /*whether decode finished?*/
        pthread_mutex_lock(&tctx->dec_periods_cnt_mutex);
        tctx->dec_periods_cnt += 1;
        pthread_mutex_unlock(&tctx->dec_periods_cnt_mutex);

        if (tctx->clip_status == CLIP_FINISHED && tctx->dec_periods_cnt == tctx->periods_cnt)
        {
            for (auto iter = tctx->gbt.proc_gop_buf.begin(); iter != tctx->gbt.proc_gop_buf.end(); iter++)
            {
                Gop *g = iter->second;
                insert_gop_to_fgb(g, tctx);
                insert_gop_to_reso_fgb(g, tctx);

                for (auto reso = tctx->gbt.reso_full_gop_buf.begin(); reso != tctx->gbt.reso_full_gop_buf.end(); reso++)
                {
                    cout << reso->first << "  " << reso->second.size() << endl;
                }
            }
            tctx->gbt.proc_gop_buf.clear();

            tctx->dec_status = DEC_FINISHED;
            cout << "dec finished " << tctx->gbt.full_gop_buf.size() << endl;
        }
        /*can be schedule out*/
        thread_inst_t *thr = thr_pool.threads_map[pthread_self()];
        if (thr->schedule)
        {
            cout << "schedule" << endl;
            sem_wait(&thr->tctx_sem);
            tctx = thr->tctx_queue.front();
            thr->tctx_queue.pop();
            thr->schedule = false;

            launch_transcoding(tctx);
        }
    }

    /* help encoder thread do encoding task */
    while (true)
    {
        if (tctx->dec_status == DEC_FINISHED)
        {
            multi_scale_encode(tctx);
            return;
        }


        /*can be schedule out*/
        //if (tctx->dec_status == DEC_FINISHED)
        //{
        //    return;
        //}
        /*can be schedule out*/

        //if (tctx->dec_status == DEC_FINISHED)
        //{
        //    cout << "I return" << endl;
        //    return;
        //}
    }
}

period_t *gen_period(long start, long end, int valid)
{
    /* generate a new period and insert it into periods */
    period_t *period = (period_t *)malloc(sizeof(period_t)); 
    period->valid = valid;
    period->start = start;
    period->end = end;

    return period;
}
