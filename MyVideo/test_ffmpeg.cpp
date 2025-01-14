#include <iostream>
#include <string>
#include <ctime>
#include <thread>
#include <queue>
extern "C"{
    #include<libavcodec/avcodec.h>
    #include<libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
    #include <libavfilter/avfilter.h>
    #include <libavutil/avutil.h>
    #include<libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libswresample/swresample.h>
    #include <SDL.h>
}
using namespace std;

#define MYEVENT (SDL_USEREVENT+1)
#define MYEVENT_BREAK (SDL_USEREVENT+2)

int mutex = 0, mutex2 = 0; // 线程同步信号量
int pause = 0; // 控制视频暂停
double ver = 25; // 控制帧率
int thd_exit = 0; // 控制程序退出标志
chrono::steady_clock::time_point timer; // 时间同步计时
double sub_timer = 1000*1.0/ver; // 每一帧时间

const char* filename = "test\\input\\sence.mp4";
// 准备ffmpeg相关变量
AVFormatContext* pAvformatcontext = 0;
AVFormatContext* pAvformatcontext_audio = 0;
AVInputFormat* pAvinputformat = 0;
AVInputFormat* pAvinputformat_audio = 0;
AVDictionary* pAvdictionary = 0;
AVCodecContext* pAvcodeccontext = 0;
AVCodecContext* pAVcodecContextAudio = 0;
const AVCodec* pAvcodec = 0;
const AVCodec* pAvcodec_audio = 0;
AVPacket* pAvpacket = 0;
AVPacket* pAvpacket_audio = 0;
AVFrame* pAvframe = 0;  
AVFrame* pAVframeYUV = 0;
AVFrame* pAVframePCM = 0;
int ret = 0; // 记录函数执行结果
int ret_audio = 0;
uint8_t* outbuffer = 0;
uint8_t* out_buffer = 0;
SwsContext* pSwsContext = 0;
SwrContext* pSwrContext_audio = 0;

// 音频参数
unsigned int audioLen = 0;
uint8_t* audioPos = nullptr;// 当前音频读取位置
int sz = 0;// 单音频帧字节大小

// 视频解码
queue<AVFrame*> pix;
int q_mutex = 0, q_max = 10;
int video_exit = 0;
void thd_video_tsk(int av_stream_idx){
    while(1){
        if(q_mutex<q_max){
            if(av_read_frame(pAvformatcontext,pAvpacket)>=0){
                if(pAvpacket->stream_index == av_stream_idx){
                    // 解码一帧视频流
                    avcodec_send_packet(pAvcodeccontext,pAvpacket);
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

    // 解析完毕
    video_exit = 1;
}

// 音频解析
void thd_audio_tsk(int av_stream_idx_audio){
    while (av_read_frame(pAvformatcontext_audio,pAvpacket_audio)>=0)
    {
        if(pAvpacket_audio->stream_index == av_stream_idx_audio){
            avcodec_send_packet(pAVcodecContextAudio,pAvpacket_audio);
            while (true)
            {
                ret_audio = avcodec_receive_frame(pAVcodecContextAudio,pAVframePCM);
                cout<<"audio "<<ret_audio<<endl;
                if(ret_audio == AVERROR(EAGAIN) || ret_audio == AVERROR_EOF){
                    break;
                } 
                else if(ret_audio < 0) {
                    cout<<"fail decode audio packet"<<endl;
                    return ;
                }
                // sz = 4096(2(声道数)*1024(单声道样本数)*2(AV_SAMPLE_FMT_S16为2个字节))每一帧音频数据字节大小
                sz = av_samples_get_buffer_size(nullptr, pAVcodecContextAudio->ch_layout.nb_channels,pAVframePCM->nb_samples,AV_SAMPLE_FMT_S16,0);
                cout<<sz<<endl;
                // out_buff = (uint8_t*)av_malloc(sz);
                // thd_buffer = (uint8_t)av_malloc(sz*10);
                swr_convert(pSwrContext_audio, &out_buffer, pAVframePCM->nb_samples,(const uint8_t**)pAVframePCM->data,pAVframePCM->nb_samples);
                while(audioLen>0) SDL_Delay(1);
                
                cout<<" haha "<<endl;

                // 同步数据
                audioLen = sz;
                audioPos = out_buffer;
            }
        }

        // 释放packet资源
        av_packet_unref(pAvpacket_audio);
    }
    cout << "over"<<endl;
}

// sdl音频回调函数
void audio_callback(void* userdata, uint8_t *stream, int len){
    cout<<" get "<<endl;
    cout<<"have "<<audioLen<<endl;
    if(audioLen){
        memset(stream, 0, len);
        len = min(len, static_cast<int>(audioLen));
        cout<<" len "<<len<<endl;
        SDL_MixAudio(stream, audioPos, len, SDL_MIX_MAXVOLUME);
        // memcpy(stream, audioPos, len);
        audioPos+=len;
        audioLen-=len;
    }
}

#undef main

int main()
{

    // sdl相关变量
    int screen_w, screen_h;
    SDL_Window* window;
    SDL_Renderer* sdlRenDerer;
    SDL_Texture* sdlTexture;
    SDL_Rect sdlRect;
    SDL_Event sdlEevent;
    SDL_AudioSpec audio_spec;

    // 分配空间
    pAvformatcontext = avformat_alloc_context();
    pAvformatcontext_audio = avformat_alloc_context();
    pAvpacket_audio = av_packet_alloc();
    pAvpacket = av_packet_alloc();
    pAvframe = av_frame_alloc();
    pAVframePCM = av_frame_alloc();
    pAVframeYUV = av_frame_alloc();

    // 在ffmpeg4.0后不用显示调用组件注册，ffmpeg初始化时自动注册
    // av_register_all();
    avformat_network_init();

    // 打开文件
    ret = avformat_open_input(&pAvformatcontext,filename,pAvinputformat,0);
    if(ret){
        cout<<"fail open file"<<endl;
        return 0;
    }
    ret = avformat_open_input(&pAvformatcontext_audio,filename,pAvinputformat_audio,0);
    if(ret){
        cout<<"fail open file"<<endl;
        return 0;
    }

    // 查找视频流信息
    ret = avformat_find_stream_info(pAvformatcontext,NULL);
    if(ret){
        cout<<"fail find"<<endl;
        return 0;
    }

    // 查找解码器
    int av_stream_idx = -1;
    int av_stream_idx_audio = -1;
    for(int i=0;i<pAvformatcontext->nb_streams;i++){
        // 找到视频流
        if(pAvformatcontext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            av_stream_idx = i;
            break;
        }
    }
    if(av_stream_idx == -1) {
        cout<<"fail find stream"<<endl;
        return 0;
    }
    for(int i=0;i<pAvformatcontext_audio->nb_streams;i++){
        //   找到音频流
        if(pAvformatcontext_audio->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            av_stream_idx_audio = i;
        }
    }
    if(av_stream_idx_audio == -1){
        cout<<"fail find stream audio"<<endl;
        return 0;
    }


    // 获取音视频流解码器上下文信息并寻找解码器id
    pAvcodeccontext = avcodec_alloc_context3(nullptr);
    pAVcodecContextAudio = avcodec_alloc_context3(nullptr);
    ret = avcodec_parameters_to_context(pAvcodeccontext,pAvformatcontext->streams[av_stream_idx]->codecpar);
    if(ret) {
        cout<<"fail to get AVCodecontext"<<endl;
        return 0;
    }
    ret = avcodec_parameters_to_context(pAVcodecContextAudio,pAvformatcontext->streams[av_stream_idx_audio]->codecpar);
    if(ret){
        cout<<"fail to get audio codec context"<<endl;
        return 0;
    }
    pAvcodec = avcodec_find_decoder(pAvcodeccontext->codec_id);
    if(!pAvcodec){
        cout<<"fail find decoder"<<endl;
        return 0;
    }
    pAvcodec_audio = avcodec_find_decoder(pAVcodecContextAudio->codec_id);
    if(!pAvcodec_audio){
        cout<<"fail find audio decoder"<<endl;
        return 0;
    }

    // 打开解码器
    ret = avcodec_open2(pAvcodeccontext,pAvcodec,NULL);
    if(ret){
        cout<<"fail to open decodec"<<endl;
        return 0;
    }
    ret = avcodec_open2(pAVcodecContextAudio,pAvcodec_audio,NULL);
    if(ret){
        cout<<"fail to open audio decoder"<<endl;
        return 0;
    }

    // 申请一帧视频数据缓冲区
    outbuffer = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,pAvcodeccontext->width,pAvcodeccontext->height,1));
    // 将申请到的缓冲区fill到pAVframeYUV的data和linesize数组
    av_image_fill_arrays(pAVframeYUV->data,pAVframeYUV->linesize,outbuffer,AV_PIX_FMT_YUV420P,pAvcodeccontext->width,pAvcodeccontext->height,1);
    pSwsContext = sws_getContext(pAvcodeccontext->width,pAvcodeccontext->height,pAvcodeccontext->pix_fmt,pAvcodeccontext->width,pAvcodeccontext->height,AV_PIX_FMT_YUV420P,SWS_BICUBIC,0,0,0);

    //   获取音频输入和输出通道布局
    AVChannelLayout in_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    if(pAVcodecContextAudio->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC){
        av_channel_layout_default(&in_ch_layout, 2);
    }else{
        av_channel_layout_default(&in_ch_layout,pAVcodecContextAudio->ch_layout.nb_channels);
    }

    // 初始化SwsContext用于重采样
    pSwrContext_audio = swr_alloc();
    av_opt_set_chlayout(pSwrContext_audio,"in_chlayout", &in_ch_layout,0); // 输入通道布局
    av_opt_set_chlayout(pSwrContext_audio,"out_chlayout", &out_ch_layout,0); // 输出通道布局
    av_opt_set_int(pSwrContext_audio,"in_sample_rate",pAVcodecContextAudio->sample_rate,0); //设置输入通道采样率
    av_opt_set_int(pSwrContext_audio,"out_sample_rate",44100,0); // 设置输出通道采样率
    av_opt_set_sample_fmt(pSwrContext_audio,"in_sample_fmt",pAVcodecContextAudio->sample_fmt,0); // 设置输入采样格式
    av_opt_set_sample_fmt(pSwrContext_audio,"out_sample_fmt",AV_SAMPLE_FMT_S16,0); // 设置输出采样格式
    swr_init(pSwrContext_audio);

    // 申请音频缓冲区
    // 设置缓冲区大小为一帧
    out_buffer = (uint8_t*)av_malloc(4096); // 缓存即将播放的一帧数据

// sdl初始化
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)){
        cout<<"fail sdl init"<<endl;
        return 0;
    }

    // sdl2渲染部分
    screen_w = pAvcodeccontext->width;
    screen_h = pAvcodeccontext->height;
    window = SDL_CreateWindow("my video player",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,screen_w,screen_h,SDL_WINDOW_OPENGL);
    sdlRenDerer = SDL_CreateRenderer(window,-1,0);
    sdlTexture = SDL_CreateTexture(sdlRenDerer,SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING,screen_w,screen_h);
    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = screen_w;
    sdlRect.h = screen_h;

    // 设置sdl音频参数
    SDL_Init(SDL_INIT_AUDIO); 
    audio_spec.freq = pAVcodecContextAudio->sample_rate;
    audio_spec.format = AUDIO_S16SYS;
    audio_spec.channels = 2;
    audio_spec.silence = 0; // 设置静音值
    audio_spec.samples = 1024;
    audio_spec.callback = audio_callback;
    audio_spec.userdata = NULL;

    // 打开播放器
    if(SDL_OpenAudio(&audio_spec,NULL) < 0) return 0;
    SDL_PauseAudio(0);

    pAvpacket = (AVPacket*)malloc(sizeof(AVPacket));
    pAvpacket_audio = (AVPacket*)malloc(sizeof(AVPacket));

    // 多线程解析音视频数据
    thread thd(thd_video_tsk, av_stream_idx);
    thd.detach();
    thread thd_audio(thd_audio_tsk, av_stream_idx_audio);
    thd_audio.detach();
    
    // 循环解码并渲染
    while(1){
        if(q_mutex){
            AVFrame* this_pix = pix.front(); pix.pop(); q_mutex--;

            // sdl 渲染
            SDL_UpdateTexture(sdlTexture,NULL,this_pix->data[0],this_pix->linesize[0]);
            SDL_RenderClear(sdlRenDerer);
            SDL_RenderCopy(sdlRenDerer,sdlTexture,NULL,NULL);
            SDL_RenderPresent(sdlRenDerer);
            SDL_Delay(sub_timer);
        }else if(video_exit && !q_mutex) break;
    }

    // 资源退出与释放
    sws_freeContext(pSwsContext);

    SDL_Quit();
    av_frame_free(&pAvframe);
    av_frame_free(&pAVframePCM);
    av_frame_free(&pAVframeYUV);
    avcodec_close(pAvcodeccontext);
    avcodec_close(pAVcodecContextAudio);
    avformat_close_input(&pAvformatcontext);
    avformat_close_input(&pAvformatcontext_audio);

    return 0;
}


