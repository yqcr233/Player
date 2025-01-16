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
}
using namespace std;
class ffm{
public:
    ffm();
    ~ffm();
    int open_video_file(const char* filename);
    int open_audio_file(const char* filename);
    int find_info();
    int find_video_decodec_info();
    int find_audio_decodec_info();
    int open_video_decodec();
    int open_audio_decodec();
    void set_audio_buff();
    void set_video_buff();
    void set_ch_layout();
    void init_swrcontext();
    void init_swscontext();
    void get_video_data();
    void get_audio_data(unsigned int& audioLen, uint8_t** audioPos);
    AVCodecContext* get_video_codec_context();
    AVCodecContext* get_audio_codec_context();
    AVFrame* pop_first();
    int q_mutex, q_max;
    int video_exit;

private:
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
    AVChannelLayout in_ch_layout;
    AVChannelLayout out_ch_layout;
    SwsContext* pSwsContext = 0;
    SwrContext* pSwrContext_audio = 0;
    queue<AVFrame*> pix;
    uint8_t* out_buff;
    uint8_t* out_buff_video;
    int video_idx = -1, audio_idx = -1;
};