# Descript patches on top of FFmpeg 9.1

Patches carried on `descript-9.1`, on top of upstream `release/9.1`. When a series merged
through an upstream PR, we carry the whole merged PR (cherry-picked with `-x`), not our
earlier version of it. When an upstream PR merges, replace our commits with the merged
ones and re-run the checks listed in the last column.

| Our commits | Upstream | Status | When it lands in our base |
|---|---|---|---|
| `tests/fate/mov: seek in a fragmented file with a chapter track`, `avformat/mov: ignore discarded streams when placing the fragment position on seek`, `avformat/mov: keep the current sample when a fragment is inserted at it` | [#25695](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/25695) (fMP4 seek with a chapter track) | merged on master after the 9.1 cut; the whole merged PR is carried | Disappears on the next rebase onto a release that contains it. |
| `tests/fate: check colour tags with unknown or reserved values`, `avcodec/proresdec, avformat/mov: ...`, `avcodec/mpeg12dec: ...`, `avcodec/mpeg4videodec: ...`, `avformat/mov: report reserved vpcC colour tags as unspecified` | [#25140](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/25140) (colour tags) | merged on master after the 9.1 cut; the whole merged PR is carried | Disappears on the next rebase onto a release that contains it. |
| `Reapply "avformat/seek: leave the pre-roll before the target of avformat_seek_file()"`, `Reapply "avformat: discard the priming or the seek pre-roll after a seek"` | [#25102](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/25102) (seek vs initial trimming) | 9.1 contains the PR but reverts these two commits (`a252eeb358`, `fce4e9821c`), because an always-on pre-roll breaks stream copy from a concat inpoint (#25672). We revert the reverts, as the base of the opt-in below. | Stays until a release carries the opt-in pre-roll. |
| `tests/fate/preroll: check a concat inpoint on a video keyframe`, `avformat: add a seek_flags option`, `avformat: add AVSEEK_FLAG_PREROLL`, `avformat/seek: with AVSEEK_FLAG_PREROLL set, seek back by the pre-roll`, `fftools/ffplay: seek with the audio pre-roll` | [#25842](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/25842) (opt-in pre-roll) | open; carried from the PR head `bc74cce352` | Replace with the merged commits; the flag and option names may change in review. **Callers that decode after a seek must set `seek_flags=preroll` or pass `AVSEEK_FLAG_PREROLL`** (`pkg-rust/libav/core/src/demux.rs` in the monorepo), otherwise the pre-roll and `seek_preroll_mode` have no effect. |
| `avcodec/mpegaudiodec: set the sample rate before allocating the frame`, `tests/fate/libavcodec: add a test for MPEG audio sample rate changes` | no upstream PR recorded | ours | — |
| `avcodec/bsf/side_data: set the fields of skip samples` | follow-up to [#25101](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/25101), which 9.1 contains without it | ours, ported onto the merged filter | The planner flag `side_data=skip_samples.discard=0` lives in `services/media-evals-worker/src/activities/replay.ts`. Do not replace it with `remove=skip_samples`, which also drops the priming. |
| `avformat/mov: discard the trimmed samples of an Opus packet`, `avcodec/aacenc: attach the discard padding of the last frame`, `avcodec/mpegaudiodec: export the seek pre-roll`, `avformat/matroskadec: rebuild exact audio timestamps with AVFMT_FLAG_EXACT_TS`, `avformat/matroskaenc: pick a clock finer than a sample with AVFMT_FLAG_EXACT_TS`, `avcodec: add seek_preroll_mode, for a pre-roll that matches a continuous decode`, `avformat/mov: rebuild exact audio timestamps with AVFMT_FLAG_EXACT_TS` | sample-exact seeks, upstream PRs to come | ours, carried ahead of upstream review | Replace with the merged commits, which may rename the flag, the option or the enum. `AVFMT_FLAG_EXACT_TS` is `0x800000` here (it was `0x400000` on 9.0, which 9.1 uses for `AVFMT_FLAG_LEGACY_ID3V2_COMM_KEYS`). `AVCodecContext` gains a trailing field, so regenerate the Rust bindings in `pkg-rust/libav/sys`. |

Versions: lavf 63.16.104, lavc 63.24.101 (9.1's minor, our micro).

FATE was not completed on this stack. Known open items: the `gapless-mp3` and
`mov-mp4-opus-trim` references differ, and our seeking tests (`opus-seek-preroll-exact`)
still need `-seek_flags preroll`.

## Dropped when moving from 9.0, because 9.1 contains them

The ID3v2 COMM/USLT series, the Ogg Vorbis trimming rework, the `trim` BSF, the AAC missing
channel element fix (#24641), the `side_data` BSF (#25101, in its merged form), and the rest
of #25102. Our removal of the legacy ID3v2 COMM keys is not carried: 9.1 keeps them behind
the deprecated `legacy_id3v2_comm_keys` flag, off by default, until the next major version.

## Why each one exists

- **fMP4 seek with a chapter track (#25695).** With `use_mfra_for`, a seek in a fragmented
  MP4 that carries a chapter track resumed reading from the first fragment. MTS seeks into
  such files decoded from the file start and timed out or returned the wrong segment.
- **Colour tags (#25140).** swscale rejects frames with reserved or invalid colour tags
  (`ENOTSUP`); FFmpeg 7.1 ignored them. ProRes, H.264 in mov and VP9 in mov files carry such
  values in production.
- **Pre-roll (#25102, #25842).** A decode after a seek needs the audio pre-roll before the
  target. Upstream made it opt-in so that stream copy lands where it asked.
- **MPEG audio sample rate.** The decoder allocated the frame with the previous sample rate
  when the rate changes mid-stream.
- **`side_data` skip samples.** The mov demuxer injects `discard` on the last packet, so
  audio ending at the end of a file is one frame shorter than FFmpeg 7. Our parity planner
  drops it with `side_data=skip_samples.discard=0`.
- **Sample-exact seeks.** Container clocks coarser than a sample round packet timestamps, so
  a seek lands up to half a tick away from the sample it names. With `AVFMT_FLAG_EXACT_TS`
  the demuxers rebuild audio timestamps from the packet lengths, and
  `seek_preroll_mode=exact` asks a decoder for the pre-roll after which its output equals a
  continuous decode.

## After an upstream merge

1. Rebase onto the release that contains the change, or replace our commits with the whole
   merged PR, and drop the commits that became redundant.
2. Update this table.
3. Rebuild through `descriptinc/ffmpeg-mts-build`, bump the pinned build in
   `pkg-rust/libav/sys` (monorepo), regenerate the bindings, and re-run the 7 vs 9
   comparison before deploying.
