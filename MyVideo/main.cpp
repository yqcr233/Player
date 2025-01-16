#include <iostream>
#include <string>
#include <ctime>
#include <thread>
#include <queue>
#include "ffm/ffm.hpp"
#include "render/sdl.hpp"
ffm player;
render renderer;

int ver = 25; // 控制帧率

void thd_video_task(){
    player.get_video_data();
}

void thd_audio_task(){
    player.get_audio_data(renderer.audioLen, &renderer.audioPos);
}

void audio_callback(void* userdata, uint8_t *stream, int len){
    cout<<" get "<<endl;
    cout<<"have "<<renderer.audioLen<<endl;
    if(renderer.audioLen){
        memset(stream, 0, len);
        len = min(len, static_cast<int>(renderer.audioLen));
        cout<<"len "<<len<<endl;
        SDL_MixAudio(stream, renderer.audioPos, len, SDL_MIX_MAXVOLUME);
        // memcpy(stream, audioPos, len);
        renderer.audioPos+=len;
        renderer.audioLen-=len;
    }
}

#undef main
int main(){
    const char* filename = "MyVideo\\input\\sence.mp4";
    int ret = player.open_video_file(filename);
    if(ret){
        cout<<"fail open file"<<endl;
        return 0;
    }

    player.open_audio_file(filename);

    player.find_info();

    int video_idx = player.find_video_decodec_info();
    if(video_idx == -1) {
        cout<<"fail find video decodec info"<<endl;
        return 0;
    }
    int audio_idx = player.find_audio_decodec_info();

    player.open_audio_decodec();
    ret = player.open_video_decodec();
    if(ret){
        cout<<"open video decodec fail"<<endl;
    }

    player.init_swscontext();

    player.set_audio_buff();

    player.set_video_buff();

    player.set_ch_layout();

    player.init_swrcontext();

    // 多线程解析
    thread thd_video(thd_video_task);
    thd_video.detach();
    thread thd_audio(thd_audio_task);
    thd_audio.detach();

    renderer.video_init(player.get_video_codec_context());
    renderer.audio_init(player.get_audio_codec_context());
    renderer.audio_spec.callback = audio_callback;

    // 打开播放器
    if(SDL_OpenAudio(&renderer.audio_spec,NULL) < 0) return 0;
    SDL_PauseAudio(0);

    while(1){
        if(player.q_mutex){
            AVFrame* this_pix = player.pop_first();

            // sdl 渲染
            SDL_UpdateTexture(renderer.sdlTexture,NULL,this_pix->data[0],this_pix->linesize[0]);
            SDL_RenderClear(renderer.sdlRenDerer);
            SDL_RenderCopy(renderer.sdlRenDerer,renderer.sdlTexture,NULL,NULL);
            SDL_RenderPresent(renderer.sdlRenDerer);
            SDL_Delay(1000/ver);
        }else if(player.video_exit && !player.q_mutex) break;
    }

    return 0;
}