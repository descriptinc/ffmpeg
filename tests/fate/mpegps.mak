# This tests that a 16-bit pcm_dvd stream is correctly remuxed in mpegps
FATE_MPEGPS-$(call REMUX, MPEG1SYSTEM MPEGPS) += fate-mpegps-remuxed-pcm-demux
fate-mpegps-remuxed-pcm-demux: $(SAMPLES)/mpegps/pcm_aud.mpg
fate-mpegps-remuxed-pcm-demux: CMD = stream_remux "mpeg" "$(TARGET_SAMPLES)/mpegps/pcm_aud.mpg" "" "mpeg" "-map 0:a:0" "" "-codec copy"

FATE_SAMPLES_FFMPEG += $(FATE_MPEGPS-yes)
fate-mpegps: $(FATE_MPEGPS-yes)

# A sequence display extension can carry reserved colour values; they must not reach the frame
FATE_SAMPLES_FFMPEG_FFPROBE-$(call ALLYES, MPEGVIDEO_DEMUXER MPEG2VIDEO_MUXER MPEG2_METADATA_BSF MPEG2VIDEO_DECODER) += fate-mpeg2-reserved-color-tags
fate-mpeg2-reserved-color-tags: CMD = stream_remux mpegvideo $(TARGET_SAMPLES)/mpeg2/tcela-6.bits "" mpeg2video \
    "-map 0:v:0 -frames:v 3 -bsf:v mpeg2_metadata=colour_primaries=3:transfer_characteristics=3" "" "" \
    "-select_streams v -read_intervals %+\#1 -show_entries frame=color_primaries,color_transfer,color_space -of default=nw=1"
