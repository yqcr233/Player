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
#undef main
class render{
public:
    render();
    ~render();
    void video_init(AVCodecContext* pAvcodeccontext);
    void audio_callback(void* userdata, uint8_t *stream, int len);
    void audio_init(AVCodecContext* pAVcodecContextAudio);

    SDL_Window* window;
    SDL_Renderer* sdlRenDerer;
    SDL_Texture* sdlTexture;
    SDL_Rect sdlRect;
    SDL_Event sdlEevent;
    SDL_AudioSpec audio_spec;
    int screen_w, screen_h;
    unsigned int audioLen = 0;
    uint8_t* audioPos = nullptr;
};