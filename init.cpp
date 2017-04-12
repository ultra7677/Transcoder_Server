#include "transcoder_core.h"
#include <list>

using namespace std;

void init()
{
    init_core_t();
    av_register_all();
}

void init_thread_pool()
{
}

void init_input(trans_ctx_t *tctx)
{
    decode_t dec_t;
    open_decoder(tctx->ipt.filename.c_str(), &dec_t);
    
    tctx->ipt.width = dec_t.ctx[0]->width;
    tctx->ipt.height = dec_t.ctx[0]->height;
    /* cout << dec_t->ctx[0]->pix_fmt << "pix " << AV_PIX_FMT_YUV420P << endl; */
    /* tctx->ipt.duration = dec_t.streams[0]->duration * dec_t.streams[0]->time_base.num / dec_t.streams[0]->time_base.den; */
    /* tctx->ipt.file_size = tctx->ipt.duration * dec_t.streams[0]->codec->bit_rate; */
    tctx->ipt.video_bit_rate = dec_t.streams[0]->codec->bit_rate;
    tctx->ipt.pix_fmt = dec_t.ctx[0]->pix_fmt;
    tctx->ipt.framerate = dec_t.ctx[0]->framerate;
    tctx->ipt.timebase = av_inv_q(dec_t.ctx[0]->framerate);
    tctx->ipt.stream_timebase = dec_t.streams[0]->time_base;
    /* tctx->ipt.dec_timebase = dec_t.ctx[0]->time_base; */
    /* tctx->ipt.ticks_per_frame = dec_t.ctx[0]->ticks_per_frame; */
    tctx->ipt.bits_per_raw_sample = dec_t.ctx[0]->bits_per_raw_sample;
    tctx->ipt.r_frame_rate = dec_t.streams[0]->r_frame_rate;
    tctx->ipt.avg_frame_rate = dec_t.streams[0]->avg_frame_rate;

    pthread_mutex_init(&tctx->ipt.file_size_mutex, NULL);
    tctx->ipt.file_size = 0;

    tctx->ipt.video_delay = av_rescale_q_rnd(dec_t.streams[0]->start_time,
                 dec_t.streams[0]->time_base,
                (AVRational){1,1000},
                static_cast<enum AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

    tctx->ipt.has_audio = (dec_t.has_audio) ? true : false;

    tctx->ipt.video_nb_frames = dec_t.streams[0]->nb_frames;

    return;
}

void init_output(trans_ctx_t *tctx, output_t *opt)
{
    /* new format context */
    string guess_file = "0." + opt->fmt;
    int ret = avformat_alloc_output_context2(&opt->fmt_ctx, NULL, NULL, guess_file.c_str());
    if (ret < 0)
    {
        printf("fail to alloc avformat context\n");
        exit(0);
    }

    /* find encoder */
    enum AVCodecID codec_id = av_guess_codec(opt->fmt_ctx->oformat, NULL, guess_file.c_str(), NULL, AVMEDIA_TYPE_VIDEO);
    if (codec_id == AV_CODEC_ID_NONE)
    {
		printf(" %s Can not find encoder! \n", "scale and encode ");
    }

    AVCodec *codec = avcodec_find_encoder(codec_id);
    if (!codec)
    {
		printf(" %s Can not find encoder! \n", "scale and encode ");
    }

    /* new format stream */
    opt->video_st = avformat_new_stream(opt->fmt_ctx, 0);
	if (opt->video_st==NULL){
		printf("Failed to new video stream! \n");
        exit(0);
	}

    AVCodecContext *ctx = opt->video_st->codec;
    opt->video_st->time_base = tctx->ipt.timebase;
    opt->video_st->codecpar->format = opt->pix_fmt;

    /* initialize AVCodecContext*/
    ctx->codec_id = codec_id;
	ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	ctx->pix_fmt = opt->pix_fmt;
	ctx->width = opt->width;
	ctx->height = opt->height;
    ctx->time_base = tctx->ipt.timebase;
    ctx->bits_per_raw_sample = tctx->ipt.bits_per_raw_sample;
    ctx->qmin = -1;
    ctx->qmax = -1;
    ctx->trellis = -1;
    ctx->coder_type = 1;
	ctx->thread_count = 1;
    /*ctx->gop_size = -1;*/
    ctx->gop_size = 5; 
    ctx->me_method = -1;
    ctx->b_quant_factor = -1;
    ctx->b_frame_strategy = -1;
    ctx->i_quant_factor = -1;
    ctx->me_cmp = -1;
    ctx->me_subpel_quality = -1;
    ctx->me_range = -1;
    ctx->scenechange_threshold = -1;
    ctx->noise_reduction = -1;
    ctx->keyint_min = -1;
    ctx->refs = -1;
    ctx->chroma_sample_location = AVCHROMA_LOC_LEFT;
    ctx->request_sample_fmt = AV_SAMPLE_FMT_U8;
    ctx->qcompress = -1;
    ctx->qblur = -1;
    ctx->max_qdiff = -1;
    ctx->rc_initial_buffer_occupancy = -1;
    ctx->coder_type = -1;
    ctx->min_prediction_order = 0;
    ctx->max_prediction_order = 0;
    ctx->trellis = -1;
    ctx->me_cmp = -1;
    ctx->thread_type = 0;
    ctx->sub_text_format = 0;
    /*ctx->max_b_frames = 0;*/
    ctx->max_b_frames = -1;
    /* c->ctx->pkt_timebase =  c->ctx->time_base; */
    ctx->flags = AV_CODEC_FLAG_CLOSED_GOP;

    if (opt->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    /*set related X264 parameters*/
    //AVDictionary *param = NULL;
    //if(ctx->codec_id == AV_CODEC_ID_H264)
    //{
    //    /* av_dict_set(&param, "tune", "zerolatency", AV_DICT_APPEND); */
    //    /*av_dict_set(&param,"refs","3", AV_DICT_APPEND);*/
    //    /* av_dict_set(&param,"preset","medium",0); */
    //    /*av_dict_set(&param,"preset","veryslow",0);*/
    //}
    //else if(ctx->codec_id == AV_CODEC_ID_H265)
    //{
    //    av_dict_set(&param, "tune", "zerolatency", AV_DICT_APPEND);
    //    av_dict_set(&param, "x265-params", "pools=none", AV_DICT_APPEND);
    //    av_dict_set(&param, "x265-params", "frame-threads=1", AV_DICT_APPEND);
    //}
    //else
    //{
    //    printf("Encode format Error!\n");
    //    exit(1);
    //}

    ///*open encoder*/
    //if (avcodec_open2(ctx, codec, &param) < 0)
    //{
	//	printf("%s Failed to open encoder! \n", "scale and encode ");
    //    exit(1);
	//}

    /* open output file */
    if (avio_open2(&opt->fmt_ctx->pb, opt->filename.c_str(), AVIO_FLAG_READ_WRITE, NULL, NULL) < 0)
    {
        printf("fail to open output file\n");
        exit(0);
    }

    cout << opt->filename.c_str() << endl;

    /* write header to this output stream */
    if (avformat_write_header(opt->fmt_ctx, NULL) < 0)
    {
        printf("fail to open output file header\n");
        exit(0);
    }
}

//void init_threads_info(trans_ctx_t *tctx, int t_num)
//{
//    pthread_mutex_init(&tctx->t_info.t_frames_mutx, NULL); 
//    pthread_mutex_init(&tctx->t_info.encoders_mutex, NULL); 
//    pthread_mutex_init(&tctx->t_info.decoder_mutex, NULL); 
//    pthread_mutex_init(&tctx->t_info.live_thr_cnt_mutx, NULL); 
//    pthread_mutex_init(&tctx->t_info.enc_running_flag_mutex, NULL); 
//
//    tctx->t_info.tcnt = t_num;
//    tctx->t_info.live_thr_cnt = 0;
//
//    return;
//}

void init_gop_buffer(trans_ctx_t *tctx)
{
    //tctx->gbt.buf_size = tctx->t_info.tcnt * 2;
    /*tctx->gbt.buf_size = tctx->t_info.tcnt;*/
    tctx->gbt.buf_size = 80;
    tctx->gbt.gop_size = 100;

    int buf_size = tctx->gbt.buf_size;
    int gop_size = tctx->gbt.gop_size;

    /*Gop *g = (Gop *)malloc(sizeof(Gop) * buf_size);*/
    
    pthread_mutex_init(&tctx->gbt.igb_mutex, NULL);
    pthread_mutex_init(&tctx->gbt.fgb_mutex, NULL);
    pthread_mutex_init(&tctx->gbt.pgb_mutex, NULL);
    pthread_mutex_init(&tctx->gbt.rfgb_mutex, NULL);

    for (auto iter = tctx->reso_opts_map.begin(); iter != tctx->reso_opts_map.end(); iter++)
    {
        list<Gop *> gop_list;
        tctx->gbt.reso_full_gop_buf[iter->first] = gop_list;
    }

    int image_size = av_image_get_buffer_size(tctx->ipt.pix_fmt, tctx->ipt.width,
                                                tctx->ipt.height, 1);

    for (int i = 0; i < buf_size; i++)
    {
        Gop *g = new Gop;
        g->gop_id = -1;
        g->frame_cnt = 0;
        g->capacity = gop_size;
        g->frames = (AVFrame **)malloc(sizeof(AVFrame *) * gop_size);
        g->pkts_size = 0;
        g->fetch_cnt = 0;
        g->ret_cnt = 0;
        
        for (int j = 0; j < gop_size; j++)
        {
            AVFrame *frame =av_frame_alloc();

            frame->width = tctx->ipt.width;
            frame->height = tctx->ipt.height;
            frame->format = tctx->ipt.pix_fmt;

            uint8_t *image_buf = (uint8_t *) av_malloc(image_size);
            av_image_fill_arrays(frame->data,
                        frame->linesize,
                        image_buf,
                        tctx->ipt.pix_fmt,
                        tctx->ipt.width,
                        tctx->ipt.height,
                        1);

            g->frames[j] = frame;
        }

	    g->valid = (uint8_t *)calloc(gop_size, sizeof(uint8_t));

        pthread_mutex_init(&g->frame_cnt_mutex, NULL);

        for (auto iter = tctx->reso_opts_map.begin(); iter != tctx->reso_opts_map.end(); iter++)
            g->reso_opts_lists.push_back(iter->second);

        tctx->gbt.idle_gop_buf.push_back(g);
    }
}

void init_trans_ctx(trans_ctx_t *tctx, int t_num)
{
    init_input(tctx);

    /* init output data */
    for (int i = 0; i < (int)tctx->opt.size(); i++)
    {
        pthread_mutex_init(&tctx->opt[i]->enc_gop_cnt_mutex, NULL);
        tctx->opt[i]->enc_gop_cnt = 0;
    }

    pthread_mutex_init(&tctx->gop_cnt_mutex, NULL);
    tctx->gop_cnt = 0;

    init_gop_buffer(tctx);

    pthread_mutex_init(&tctx->enc_state_mutex, NULL);
    tctx->enc_state = 0;
    tctx->before_stable_frame_cnt = 0;
    tctx->in_stable_frame_cnt = 0;
    tctx->before_stable_frame_cnt = 0;

    /* init clip related data */
    pthread_mutex_init(&tctx->periods_mutex, NULL);
    pthread_mutex_init(&tctx->clip_status_mutex, NULL);
    pthread_cond_init(&tctx->periods_avail, NULL);
    tctx->clip_status = CLIP_NOT_START_YET;
    tctx->clip_step = 300;
    tctx->periods_cnt = 0;

    /*decode related data*/
    pthread_mutex_init(&tctx->dec_periods_cnt_mutex, NULL);
    tctx->dec_periods_cnt = 0;
    pthread_mutex_init(&tctx->fetch_periods_cnt_mutex, NULL);
    tctx->fetch_periods_cnt = 0;
    tctx->dec_status = EXIST_PEROID;

    tctx->last_enc_frame_cnt = 0;
    tctx->enc_frame_cnt = 0;
    tctx->tick_cnt = 0;

    tctx->gop_cnt = tctx->ipt.video_nb_frames % tctx->gbt.gop_size == 0 ?
        tctx->ipt.video_nb_frames / tctx->gbt.gop_size :
        tctx->ipt.video_nb_frames / tctx->gbt.gop_size + 1;
    /* cout << "gop_id " << tctx->gbt.idle_gop_buf.front()->gop_id << endl; */

    /*x264 library*/
    tctx->inject = 0;
    /*tctx->inject = 1;*/
    /*set_injected_version(1);*/
    pthread_mutex_init(&tctx->x_ctrls_mutex, NULL);
}
