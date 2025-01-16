#include "sdl.hpp"
#undef main
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

render::render(){
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
    SDL_Init(SDL_INIT_AUDIO); 
}

render::~render(){
    SDL_Quit();
}

void render::video_init(AVCodecContext* pAvcodeccontext){
    screen_w = pAvcodeccontext->width;
    screen_h = pAvcodeccontext->height;
    window = SDL_CreateWindow("my video player",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,screen_w,screen_h,SDL_WINDOW_OPENGL);
    sdlRenDerer = SDL_CreateRenderer(window,-1,0);
    sdlTexture = SDL_CreateTexture(sdlRenDerer,SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING,screen_w,screen_h);
    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = screen_w;
    sdlRect.h = screen_h;   
}

void render::audio_callback(void* userdata, uint8_t *stream, int len){
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

void render::audio_init(AVCodecContext* pAVcodecContextAudio){
    audio_spec.freq = pAVcodecContextAudio->sample_rate;
    audio_spec.format = AUDIO_S16SYS;
    audio_spec.channels = 2;
    audio_spec.silence = 0; // 设置静音值
    audio_spec.samples = 1024;
    // audio_spec.callback = audio_callback;
    audio_spec.userdata = NULL;
}