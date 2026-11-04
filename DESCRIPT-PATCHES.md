# Descript patches on top of FFmpeg 9.0

Patches carried on `descript-9.0` that are also proposed upstream. When an upstream PR
merges, replace our commits with the merged ones (the merged version often changes in
review), and re-run the checks listed in the last column.

| Our commits | Upstream | Status | When it merges |
|---|---|---|---|
| `avcodec/codec_par: do not turn the container's priming into decoder delay`, `avcodec/aac: export the seek pre-roll`, `avformat/seek: keep the target inside the range when rescaling it`, `avformat/seek: leave the pre-roll before the target of avformat_seek_file()`, `avcodec/decode: discard the padding when it overlaps the skipped samples`, `avformat: discard the priming or the seek pre-roll after a seek` | [#25102](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/25102) (seek vs initial trimming) | merged upstream, the six library commits are carried; the PR's test and tool commits (ffprobe `seek_preroll`, `libavformat/tests/seek` columns, `tests/api`, `tests/fate/preroll.mak`) are not, their refs do not apply to 9.0 | Disappears on the next rebase onto a release that contains it. `tests/ref/seek/mkv-codec-delay` was regenerated on 9.0 and matches master's without the skip columns. Re-run the 7 vs 9 AAC seek comparison before deploying. |
| `avcodec/packet: look up side data types by handle`, `tests/fate: check that every packet side data type has a handle`, `avcodec/bsf: add the side_data bitstream filter`, `avcodec/bsf/side_data: set the fields of skip samples` | [#25101](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/25101) (`side_data` BSF) | open, review ongoing (a frame-filter-style selection mode was suggested) | Re-check the option names (`remove=a+b`, `skip_samples.discard`), the handle names, and the packet side data handle lookup. The planner flag `side_data=skip_samples.discard=0` lives in `services/media-evals-worker/src/activities/replay.ts` (`keepTrailingSamples`) and the pinned build in `pkg-rust/libav/sys/versions-ffmpeg9.txt` (monorepo); change them together with the new build. |
| `tests/fate: check colour tags with unknown or reserved values`, `avcodec/proresdec, avformat/mov: report reserved colour tags as unspecified`, `avcodec/mpeg12dec: ...`, `avcodec/mpeg4videodec: ...`, `avformat/mov: report reserved vpcC colour tags as unspecified` | [#25140](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/25140) | open | Drop our five commits for the merged ones. Our `tests/fate/mov.mak` only keeps the two colour tag tests, the rest of the PR's hunks conflict with unrelated mov tests missing here. Three of its tests need samples that are not in the fate-suite yet (`mpeg4/reserved-color-tags.m4v`, `prores/color-tags-fallback.mov`, `mov/vpcc-reserved-color-tags.mp4`). Re-run a pair that failed with swscale `ENOTSUP` (see below). |
| `avcodec/aac: zero the output of a missing channel element` | [#24641](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/24641) | merged upstream | Disappears on the next rebase onto a release that contains it. |
| `tests/fate/mov: seek in a fragmented file with a chapter track`, `avformat/mov: ignore discarded streams when placing the fragment position on seek`, `avformat/mov: keep the current sample when a fragment is inserted at it` | [#25695](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/25695) (fMP4 seek with a chapter track) | merged upstream, the merged commits are the ones carried here | Disappears on the next rebase onto a release that contains it. The `memmove` size in `mov_read_trun` keeps 9.0's `sc->tts_count` where master uses `sti->nb_index_entries`. |
| HLS `EXT-X-START` parsing | [#25121](https://code.ffmpeg.org/FFmpeg/FFmpeg/pulls/25121) | open, not yet carried here | Add when the source branch is known. |

## Why each one exists

- **Priming / seek (#25102).** FFmpeg 9 turned the container's priming into decoder delay,
  so a seek into AAC-in-MP4 dropped real samples (824 late on mono 44.1 kHz). Our MTS
  comparison against FFmpeg 7 failed on ~15% of audio pairs until this was fixed.
- **`side_data` BSF (#25101).** FFmpeg 9's mov demuxer injects `discard` on the last packet,
  so audio ending at the end of a file is one frame shorter than FFmpeg 7. Our parity
  planner drops it with `side_data=skip_samples.discard=0` so the comparison is like for like.
- **Colour tags (#25140).** FFmpeg 9's swscale rejects frames with reserved or invalid colour
  tags (`Unsupported input`, `ENOTSUP`); FFmpeg 7.1 ignored them. ProRes, H.264 in mov
  (`colr`) and VP9 in mov (`vpcC`) files carry such values in production. The fix follows
  `c80002aa81` upstream (`avcodec/h2645_vui`), which reports reserved values as unspecified.
- **fMP4 seek with a chapter track (#25695).** With `use_mfra_for`, a seek in a fragmented
  MP4 that carries a chapter track resumed reading from the first fragment, because the
  discarded chapter track's seek placed the fragment position, and a fragment inserted at a
  stream's current sample moved that stream back to the fragment's start. MTS seeks into such
  files decoded from the file start and timed out or returned the wrong segment (the
  2026-11-02 error-cache report).

## After an upstream merge

1. Rebase `descript-9.0` onto the release that contains the change, or replace our commits
   with the merged ones, and drop the row's commits that became redundant.
2. Update this table and remove rows that are no longer carried.
3. Rebuild through `descriptinc/ffmpeg-mts-build`, bump the tag in
   `pkg-rust/libav/sys/versions-ffmpeg9.txt`, and re-run the 7 vs 9 comparison on the affected
   strata before deploying.
