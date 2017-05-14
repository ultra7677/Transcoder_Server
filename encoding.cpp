#include "transcoder_core.h"
#include <list>
#include <vector>

using namespace std;

extern unordered_map<trans_ctx_t *, list<pthread_t> > tctx_thrs_map;
extern thread_pool_t thr_pool;

void free_ctrl(x264_control_t *x_ctrl)
{
    return;
}
x264_control_t *get_x264_ctrl(int x_num)
{
    return NULL;
}
void set_injected_version(int v)
{

    return;
}

int open_encode_t(trans_ctx_t *tctx, encode_t *c, output_t *opt, Gop *g)
{
    pthread_t tid = pthread_self();

    c->opt = opt;
    /*x264 library*/
    if (tctx->inject)
    {
        pthread_mutex_lock(&tctx->x_ctrls_mutex);
        c->x264_ctrl = tctx->thr_xctrl_map[tid];
        pthread_mutex_unlock(&tctx->x_ctrls_mutex);
    }

    /* allocate format context */
    string guess_file = "0." + opt->fmt;
    int ret = avformat_alloc_output_context2(&c->fmt_ctx, NULL, NULL, guess_file.c_str());

    if (ret < 0)
    {
        printf("fail to alloc avformat context\n");
        exit(0);
    }

    /* find encoder */
    enum AVCodecID codec_id = av_guess_codec(c->fmt_ctx->oformat, NULL, guess_file.c_str(), NULL, AVMEDIA_TYPE_VIDEO);
    if (codec_id == AV_CODEC_ID_NONE)
    {
		printf(" %s Can not find encoder! \n", "scale and encode ");
		return -1;
    }

    c->codec = avcodec_find_encoder(codec_id);
    if (!c->codec)
    {
		printf(" %s Can not find encoder! \n", "scale and encode ");
		return -1;
    }

    /* new format stream */
    c->video_st = avformat_new_stream(c->fmt_ctx, 0);
	if (c->video_st==NULL){
		printf("Failed to new video stream! \n");
		return -1;
	}

    /* initialize AVCodecContext*/
    c->ctx = c->video_st->codec;
    c->ctx->codec_id = codec_id;
	c->ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	c->ctx->pix_fmt = opt->pix_fmt;
	c->ctx->width = opt->width;
	c->ctx->height = opt->height;
    c->ctx->time_base = tctx->ipt.timebase;
    c->ctx->bits_per_raw_sample = tctx->ipt.bits_per_raw_sample;
    c->ctx->qmin = -1;
    c->ctx->qmax = -1;
    c->ctx->trellis = -1;
    c->ctx->coder_type = 1;
	c->ctx->thread_count = 1;
    /*c->ctx->gop_size = -1;*/
    c->ctx->gop_size = 5; 
    c->ctx->me_method = -1;
    c->ctx->b_quant_factor = -1;
    c->ctx->b_frame_strategy = -1;
    c->ctx->i_quant_factor = -1;
    c->ctx->me_cmp = -1;
    c->ctx->me_subpel_quality = -1;
    c->ctx->me_range = -1;
    c->ctx->scenechange_threshold = -1;
    c->ctx->noise_reduction = -1;
    c->ctx->keyint_min = -1;
    c->ctx->refs = -1;
    c->ctx->chroma_sample_location = AVCHROMA_LOC_LEFT;
    c->ctx->request_sample_fmt = AV_SAMPLE_FMT_U8;
    c->ctx->qcompress = -1;
    c->ctx->qblur = -1;
    c->ctx->max_qdiff = -1;
    c->ctx->rc_initial_buffer_occupancy = -1;
    c->ctx->coder_type = -1;
    c->ctx->min_prediction_order = 0;
    c->ctx->max_prediction_order = 0;
    c->ctx->trellis = -1;
    c->ctx->me_cmp = -1;
    c->ctx->thread_type = 0;
    c->ctx->sub_text_format = 0;
    c->ctx->max_b_frames = -1;
    c->ctx->flags = AV_CODEC_FLAG_CLOSED_GOP;
    c->ctx->bit_rate = opt->bitrate;

    /* if (c->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) */
        /* c->ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; */

    int64_t size_per_second = g->pkts_size  * c->ctx->time_base.den /( c->ctx->time_base.num * g->frame_cnt);
    int64_t base_bit_rate = size_per_second * opt->bitrate / tctx->ipt.video_bit_rate;

    /* printf("bitrate %ld pkts_size %ld\n", base_bit_rate, g->pkts_size); */
    /* printf("bitrate %ld", c->ctx->bit_rate); */

    //if (opt->bitrate < 8000000)
    //{
    //    if (base_bit_rate * 8 > opt->bitrate * 1.5) {
    //        c->ctx->bit_rate = 12.5 * base_bit_rate;
    //        c->ctx->gop_size = -1;
    //    }
    //    else if (base_bit_rate * 8 < opt->bitrate * 0.4) {
    //        c->ctx->bit_rate = 8 * base_bit_rate;
    //        c->ctx->gop_size = -1;
    //    }
    //    else {
    //        c->ctx->bit_rate = opt->bitrate * 0.88;
    //    }
    //}
    //else
    //{
    //    c->ctx->bit_rate = opt->bitrate * 0.90;
    //    if (base_bit_rate * 8 > opt->bitrate * 1.5) {
    //        c->ctx->gop_size = -1;
    //        c->ctx->bit_rate = opt->bitrate;
    //    }
    //}
  
    /* printf("bit_rate %ld size %ld pkts_size %ld\n", c->ctx->bit_rate, size_per_second, g->pkts_size); */
    /*set related X264 parameters*/
    AVDictionary *param = NULL;
    if(c->ctx->codec_id == AV_CODEC_ID_H264)
    {
        /* av_dict_set(&param, "tune", "zerolatency", AV_DICT_APPEND); */
        //av_dict_set(&param, "force-cfr", "", AV_DICT_APPEND);
        /* av_dict_set(&param, "x264-params", "bframes=2", AV_DICT_APPEND); */
        /* av_dict_set(&param, "x264-params", "rc-lookahead", AV_DICT_APPEND); */
        //av_dict_set(&param, "sync-lookahead", "0", AV_DICT_APPEND);
        //av_dict_set(&param, "sliced-threads", "", AV_DICT_APPEND);
        //av_dict_set(&param, "rc-lookahead", "0", AV_DICT_APPEND);
        //av_dict_set(&param, "bframes", "0", AV_DICT_APPEND);
        /*av_dict_set(&param,"refs","3", AV_DICT_APPEND);*/
        /* av_dict_set(&param,"preset","medium",0); */
        /*av_dict_set(&param,"preset","veryslow",0);*/
    }
    else if(c->ctx->codec_id == AV_CODEC_ID_H265)
    {
        av_dict_set(&param, "tune", "zerolatency", AV_DICT_APPEND);
        av_dict_set(&param, "x265-params", "pools=none", AV_DICT_APPEND);
        av_dict_set(&param, "x265-params", "frame-threads=1", AV_DICT_APPEND);
    }
    else
    {
        printf("Encode format Error!\n");
        exit(1);
    }

    /*open encoder*/
    if (avcodec_open2(c->ctx, c->codec, &param) < 0)
    {
		printf("%s Failed to open encoder! \n", "scale and encode ");
        exit(1);
	}

    /* open output file */
    string gop_output = opt->tmp_dir + "/" + to_string(g->gop_id) + "." + opt->fmt;
    if (avio_open2(&c->fmt_ctx->pb, gop_output.c_str(), AVIO_FLAG_READ_WRITE, NULL, NULL) < 0)
    {
        printf("fail to open output file\n");
        exit(0);
    }
    if (avformat_write_header(c->fmt_ctx, NULL) < 0)
    {
        printf("fail to open output file header\n");
        exit(0);
    }

    /*create sws_scale context*/
	c->sws_ctx = sws_getContext(tctx->ipt.width, tctx->ipt.height, tctx->ipt.pix_fmt,
                                opt->width, opt->height, opt->pix_fmt,
			                    SWS_BILINEAR, NULL, NULL, NULL);

	if (!c->sws_ctx) {
        printf("fail to scale\n");
		exit(1);
	}

    /*fill the dst_frame with image buffer*/
	c->scaled_frame = av_frame_alloc();
	c->scaled_frame->width = opt->width;
	c->scaled_frame->height = opt->height;
	c->scaled_frame->format = opt->pix_fmt;

    int image_size = av_image_get_buffer_size(opt->pix_fmt, opt->width,
                                                opt->height, 1);

    uint8_t *image_buf = (uint8_t *) av_malloc(image_size);
    av_image_fill_arrays(c->scaled_frame->data,
                        c->scaled_frame->linesize,
                        image_buf,
                        opt->pix_fmt,
                        opt->width,
                        opt->height,
                        1);

    /*8. allocate a new AVPacket*/
    av_init_packet(&c->pkt);
    c->pkt.duration = tctx->ipt.pkt_duration;

    /*x264 library*/
    if (tctx->inject)
    {
        c->x264_ctrl->call_cnt++;
    }
    return 0;
}

int close_encode_t(trans_ctx_t *tctx, encode_t *c)
{
    av_write_trailer(c->fmt_ctx);
    av_packet_unref(&c->pkt);
    av_free(c->scaled_frame);
    sws_freeContext(c->sws_ctx);
    avcodec_close(c->ctx);
    avio_close(c->fmt_ctx->pb);
    avformat_free_context(c->fmt_ctx);

    /*x264 library*/
    if (tctx->inject)
    {
        c->x264_ctrl->call_cnt++;
    }

    return 0;
}

int flush_multi_encoder (trans_ctx_t *tctx, vector<encode_t *> multi_enc_t)
{
    int got_frame;
    int ret;
    unordered_map<int, bool> finish_map;
    int finish_cnt = 0;
    
    for (int i = 0; i < multi_enc_t.size(); i++)
        finish_map[i] = false;

    while (1)
    {
        for (int i = 0; i < multi_enc_t.size(); i++)
        {
            if (finish_map[i])
                continue;

            encode_t *enc_t = multi_enc_t[i];

            enc_t->pkt.data = NULL;
            enc_t->pkt.size = 0;

            if (tctx->inject)
            {
                enc_t->x264_ctrl->call_cnt = i;
                enc_t->x264_ctrl->skip_call = i;
            }

		    ret = avcodec_encode_video2 (enc_t->ctx, &enc_t->pkt, NULL, &got_frame);

            if (ret < 0)
            {
                printf("encode flush\n");
                exit(0);
            }

            if (!got_frame)
            {
                finish_map[i] = true;
                finish_cnt++;
                continue;
            }

            ret = av_write_frame(enc_t->fmt_ctx, &enc_t->pkt);
            atomic(&enc_t->opt->enc_frame_cnt_mutex, enc_t->opt->enc_frame_cnt += 1);

            if (ret < 0)
            {
                printf("write frame error\n");
                exit(0);
            }
        }

        if (finish_cnt == finish_map.size())
            break;
    }

    return ret;
}

int multi_scale_encode_gop(trans_ctx_t *tctx, Gop *g, vector<output_t *>& opts)
{
    pthread_t tid = pthread_self();
    int size = opts.size();
    
    /*x264 library*/
    if (tctx->inject)
    {
        pthread_mutex_lock(&tctx->x_ctrls_mutex);
        tctx->thr_xctrl_map[tid]->call_cnt = 0;
        pthread_mutex_unlock(&tctx->x_ctrls_mutex);
    }

    cout << "go to encode" << endl;

    vector<encode_t *> multi_enc_t;
    for (int i = 0; i < size; i++)
    {
        encode_t *enc_t = (encode_t *)malloc(sizeof(encode_t));
        output_t *opt = opts[i];

        open_encode_t(tctx, enc_t, opt, g);
        multi_enc_t.push_back(enc_t);
    }

    for (int i = 0; i < tctx->gbt.gop_size; i++)
    {
        if (g->valid[i] == 0) 
           continue;

        AVFrame *src_frame = g->frames[i];

        for (int j = 0; j < size; j++)
        {
            encode_t *enc_t = multi_enc_t[j];
            output_t *opt = opts[j];

            enc_t->pkt.data = NULL;
            enc_t->pkt.size = 0;

            if (tctx->inject)
            {
                enc_t->x264_ctrl->call_cnt = j;
                enc_t->x264_ctrl->skip_call = j;
            }

            AVFrame *dst_frame;
            if(opt->width != tctx->ipt.width || opt->height != tctx->ipt.height)
            {
                dst_frame = enc_t->scaled_frame;
                sws_scale(enc_t->sws_ctx,
                        (const uint8_t **)src_frame->data, src_frame->linesize,
                        0, tctx->ipt.height,
                        dst_frame->data, dst_frame->linesize);

            }
            else
            {
                dst_frame = src_frame;
            }

            dst_frame->pts = g->gop_id * tctx->gbt.gop_size + i;

            int got_picture;
            if (avcodec_encode_video2(enc_t->ctx, &enc_t->pkt, dst_frame, &got_picture) < 0)
            {
                printf("faile to encode frame \n");
            }

		    if (got_picture)
            {
                /*tctx->t_info.t_frames_map[tid]++;*/

		    	enc_t->pkt.stream_index = enc_t->video_st->index;
                av_packet_rescale_ts(&enc_t->pkt, enc_t->ctx->time_base, enc_t->video_st->time_base);
                av_write_frame(enc_t->fmt_ctx, &enc_t->pkt);

                atomic(&opt->enc_frame_cnt_mutex, opt->enc_frame_cnt += 1);
		    }
        }
    }

    flush_multi_encoder(tctx, multi_enc_t);

    if (tctx->inject)
    {
        pthread_mutex_lock(&tctx->x_ctrls_mutex);
        tctx->thr_xctrl_map[tid]->call_cnt = 0;
        pthread_mutex_unlock(&tctx->x_ctrls_mutex);
    }

    for (int i = 0; i < size; i++)
    {
        encode_t *enc_t = multi_enc_t[i];
        close_encode_t(tctx, enc_t);
        free(enc_t);
    }

    return 0;
}

int multi_scale_encode(void *arg)
{
    static int i = 0;
    trans_ctx_t *tctx = (trans_ctx_t *)arg;
    pthread_t tid = pthread_self();

    while (true)
    {
        /*Gop_wrapper *gw = get_gop_from_fgb(tctx);*/
        Gop_wrapper *gw = get_gop_from_reso_fgb(tctx);
        
        if (!gw)
            return 0;

        /*get the x264_ctrl*/
        if (tctx->inject)
        {
            x264_control_t *x_ctrl = get_x264_ctrl(gw->opts.size());
            if (x_ctrl == NULL)
            {
                cout << "fail to get x264_control_t" << endl;
                exit(0);
            }

            pthread_mutex_lock(&tctx->x_ctrls_mutex);
            tctx->thr_xctrl_map[tid] = x_ctrl;
            pthread_mutex_unlock(&tctx->x_ctrls_mutex);

            x_ctrl->x_num = gw->opts.size();
            x_ctrl->multi_version = gw->opts.size() - 1;
        }

        multi_scale_encode_gop(tctx, gw->g, gw->opts);

        insert_gop_to_igb(gw->g, tctx);

        for (auto iter = gw->opts.begin(); iter != gw->opts.end(); iter++)
        {
            output_t *opt = *iter;
            atomic(&opt->enc_gop_cnt_mutex, opt->enc_gop_cnt += 1);
        }

        if (tctx->inject)
        {
            pthread_mutex_lock(&tctx->x_ctrls_mutex);
            x264_control_t *x_ctrl = tctx->thr_xctrl_map[tid];
            pthread_mutex_unlock(&tctx->x_ctrls_mutex);

            free_ctrl(x_ctrl);
        }

        /*output video*/
        for (int i = 0; i < (int)tctx->opt.size(); i++)
        {
            /*cout << tctx->opt[i]->enc_gop_cnt << " ";*/
            if (tctx->opt[i]->enc_gop_cnt != tctx->gop_cnt)
                continue;

            pthread_mutex_lock(&tctx->opt[i]->enc_gop_cnt_mutex);
            cout << "output" << endl;
            tctx->opt[i]->enc_gop_cnt = 0;
            //output_video(tctx, tctx->opt[i]);
            pthread_mutex_unlock(&tctx->opt[i]->enc_gop_cnt_mutex);

            tctx_thrs_map.erase(tctx);
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

        /*cout << endl;*/
    }
}
