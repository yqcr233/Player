#include "ffm.hpp"
extern "C"{
    #include<libavcodec/avcodec.h>
    #include<libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
    #include <libavfilter/avfilter.h>
    #include <libavutil/avutil.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libswresample/swresample.h>
    #include <SDL.h>
}
ffm::ffm(){
    // 初始化空间结构
    pAvformatcontext = avformat_alloc_context();
    pAvformatcontext_audio = avformat_alloc_context();
    pAvpacket_audio = av_packet_alloc();
    pAvpacket = av_packet_alloc();
    pAvframe = av_frame_alloc();
    pAVframePCM = av_frame_alloc();
    pAVframeYUV = av_frame_alloc();
    pAvcodeccontext = avcodec_alloc_context3(nullptr);
    pAVcodecContextAudio = avcodec_alloc_context3(nullptr);
    pAvpacket_audio = (AVPacket*)malloc(sizeof(AVPacket));
    q_mutex = 0;
    q_max = 10;
    video_exit = 0;
}

ffm::~ffm(){
    sws_freeContext(pSwsContext);
    av_frame_free(&pAvframe);
    av_frame_free(&pAVframePCM);
    av_frame_free(&pAVframeYUV);
    avcodec_close(pAvcodeccontext);
    avcodec_close(pAVcodecContextAudio);
    avformat_close_input(&pAvformatcontext);
    avformat_close_input(&pAvformatcontext_audio);
    pAvcodeccontext = avcodec_alloc_context3(nullptr);
    pAVcodecContextAudio = avcodec_alloc_context3(nullptr);
}

int ffm::open_video_file(const char* filename){
    return avformat_open_input(&pAvformatcontext,filename,pAvinputformat,0);
}

int ffm::open_audio_file(const char* filename){
    return avformat_open_input(&pAvformatcontext_audio,filename,pAvinputformat_audio,0);
}

/**
 * avformat_find_stream_info 发现执行了avcodec_parameters_from_context函数，
 * 将AVCodecContext中的数据赋给AVCodecParameters中便于后续avcodec_parameters_to_context操作。
 */
int ffm::find_info(){
    int ret = avformat_find_stream_info(pAvformatcontext,NULL);
    ret = avformat_find_stream_info(pAvformatcontext_audio,NULL);
    return ret;
}

/**
 * 返回-1 表示操作失败
 * 成功返回video_idx
 */
int ffm::find_video_decodec_info(){
    for(int i=0;i<pAvformatcontext->nb_streams;i++){
        // 找到视频流
        if(pAvformatcontext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            video_idx = i;
            break;
        }
    }
    if(video_idx!=-1){
        cout<<"video idx "<<video_idx<<endl;
        cout<<"ffm pre "<<pAvcodeccontext->pix_fmt<<endl;
        int ret = avcodec_parameters_to_context(pAvcodeccontext,pAvformatcontext->streams[video_idx]->codecpar);
        cout<<"ffm "<<pAvcodeccontext->pix_fmt<<endl;
        cout<<ret<<endl;
        if(ret<0) return -1;
    }
    return video_idx;
}

/**
 * 返回-1 表示操作失败
 * 成功返回audio_idx
 */
int ffm::find_audio_decodec_info(){
    for(int i=0;i<pAvformatcontext_audio->nb_streams;i++){
        //   找到音频流
        if(pAvformatcontext_audio->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audio_idx = i;
        }
    }
    if(audio_idx!=-1){
        cout<<"audio idx "<<audio_idx<<endl;
        int ret = avcodec_parameters_to_context(pAVcodecContextAudio,pAvformatcontext_audio->streams[audio_idx]->codecpar);
        if(ret) return -1;
    }
    return audio_idx;
}

/**
 * 成功返回0
 * 反之返回-1
 */
int ffm::open_video_decodec(){
    pAvcodec = avcodec_find_decoder(pAvcodeccontext->codec_id);
    if(!pAvcodec) return -1;
    int ret = avcodec_open2(pAvcodeccontext,pAvcodec,NULL);
    cout<<"after open codec "<<pAvcodeccontext->pix_fmt<<endl;
    if(ret) return -1;
    return 0;
}

/**
 * 成功返回0
 * 反之返回-1
 */
int ffm::open_audio_decodec(){
    pAvcodec_audio = avcodec_find_decoder(pAVcodecContextAudio->codec_id);
    if(!pAvcodec_audio) return -1;
    int ret = avcodec_open2(pAVcodecContextAudio,pAvcodec_audio,NULL);
    if(ret) return -1;
    return 0;
}

void ffm::set_audio_buff(){
    // 设置缓冲区大小为一帧
    out_buff = (uint8_t*)av_malloc(5000); // 缓存即将播放的一帧音频数据
}

void ffm::set_video_buff(){
        // 申请一帧视频数据缓冲区
    out_buff_video = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,pAvcodeccontext->width,pAvcodeccontext->height,1));
    // 将申请到的缓冲区fill到pAVframeYUV的data和linesize数组
    av_image_fill_arrays(pAVframeYUV->data,pAVframeYUV->linesize,out_buff_video,AV_PIX_FMT_YUV420P,pAvcodeccontext->width,pAvcodeccontext->height,1);
 
}

void ffm::set_ch_layout(){
    //   获取音频输入和输出通道布局
    in_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    if(pAVcodecContextAudio->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC){
        av_channel_layout_default(&in_ch_layout, 2);
    }else{
        av_channel_layout_default(&in_ch_layout,pAVcodecContextAudio->ch_layout.nb_channels);
    }
}

void ffm::init_swrcontext(){
    // 初始化SwrContext用于重采样
    pSwrContext_audio = swr_alloc();
    av_opt_set_chlayout(pSwrContext_audio,"in_chlayout", &in_ch_layout,0); // 输入通道布局
    av_opt_set_chlayout(pSwrContext_audio,"out_chlayout", &out_ch_layout,0); // 输出通道布局
    av_opt_set_int(pSwrContext_audio,"in_sample_rate",pAVcodecContextAudio->sample_rate,0); //设置输入通道采样率
    av_opt_set_int(pSwrContext_audio,"out_sample_rate",44100,0); // 设置输出通道采样率
    av_opt_set_sample_fmt(pSwrContext_audio,"in_sample_fmt",pAVcodecContextAudio->sample_fmt,0); // 设置输入采样格式
    av_opt_set_sample_fmt(pSwrContext_audio,"out_sample_fmt",AV_SAMPLE_FMT_S16,0); // 设置输出采样格式
    swr_init(pSwrContext_audio);
}

void ffm::init_swscontext(){
    cout<<pAvcodeccontext->width<<" "<<pAvcodeccontext->height<<" "<<pAvcodeccontext->pix_fmt<<endl;
    pSwsContext = sws_getContext(pAvcodeccontext->width,pAvcodeccontext->height,pAvcodeccontext->pix_fmt,pAvcodeccontext->width,pAvcodeccontext->height,AV_PIX_FMT_YUV420P,SWS_BICUBIC,0,0,0);
}

void ffm::get_video_data(){
    pAvpacket = (AVPacket*)malloc(sizeof(AVPacket));
    while(1){
        if(q_mutex<q_max){
            if(av_read_frame(pAvformatcontext,pAvpacket)>=0){
                if(pAvpacket->stream_index == video_idx){
                    cout<<"geting video idx "<<video_idx<<endl;
                    // 解码一帧视频流
                    int ret = avcodec_send_packet(pAvcodeccontext,pAvpacket);
                    if(ret) {
                        cout<<"fail send packet"<<endl;
                    }
                    // 将解码后像素数据保存到frame
                    ret = avcodec_receive_frame(pAvcodeccontext,pAvframe);
                    cout<<"viding "<<ret<<endl;
                    if(ret == 0) {
                        // 进行缩放，剔除解码后YUV数据中的无效数据
                        sws_scale(pSwsContext,pAvframe->data,pAvframe->linesize,0,pAvcodeccontext->height,pAVframeYUV->data,pAVframeYUV->linesize);
                        pix.push(pAVframeYUV);
                        q_mutex++;
                    }
                }
                // 释放packet
                av_packet_unref(pAvpacket);
            }
            else break;
        }
    }

    // 渲染结束
    video_exit = 1;
}

void ffm::get_audio_data(unsigned int& audioLen, uint8_t** audioPos){
    while (av_read_frame(pAvformatcontext_audio,pAvpacket_audio)>=0)
    {
        if(pAvpacket_audio->stream_index == audio_idx){
            avcodec_send_packet(pAVcodecContextAudio,pAvpacket_audio);
            while (true)
            {
                int ret_audio = avcodec_receive_frame(pAVcodecContextAudio,pAVframePCM);
                cout<<"audio "<<ret_audio<<endl;
                if(ret_audio == AVERROR(EAGAIN) || ret_audio == AVERROR_EOF){
                    break;
                } 
                else if(ret_audio < 0) {
                    cout<<"fail decode audio packet"<<endl;
                    return ;
                }
                // sz = 4096(2(声道数)*1024(单声道样本数)*2(AV_SAMPLE_FMT_S16为2个字节))每一帧音频数据字节大小
                int sz = av_samples_get_buffer_size(nullptr, pAVcodecContextAudio->ch_layout.nb_channels,pAVframePCM->nb_samples,AV_SAMPLE_FMT_S16,0);
                cout<<"size "<<sz<<endl;
                // out_buff = (uint8_t*)av_malloc(sz);
                // thd_buffer = (uint8_t)av_malloc(sz*10);
                while(audioLen>0) SDL_Delay(1);
                swr_convert(pSwrContext_audio, &out_buff, pAVframePCM->nb_samples,(const uint8_t**)pAVframePCM->data,pAVframePCM->nb_samples);
                
                // 同步数据
                cout<<"callback "<<audioLen<<endl;
                *audioPos = out_buff;
                audioLen = sz;
                
            }
        }

        // 释放packet资源
        av_packet_unref(pAvpacket_audio);
    }
}

AVCodecContext* ffm::get_video_codec_context(){
    return pAvcodeccontext;
}

AVCodecContext* ffm::get_audio_codec_context(){
    return pAVcodecContextAudio;
}

AVFrame* ffm::pop_first(){
    AVFrame* this_pix = pix.front(); pix.pop(); q_mutex-=1;
    return this_pix;
}