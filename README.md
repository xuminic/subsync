# Adjust Subtitle Timing with Subsync
Subsync is a command-line tool used to adjust the timeline of 
subtitle files so that audio and subtitles stay synchronized. 
It can:
- Shift subtitle display times forward or backward by 
  a specified offset
- Scale subtitle timing proportionally to adjust drift
- Adjust subtitles only within a specified time range
- Support `.srt`, `.ass`, and `.ssa` subtitle formats
- A filtering program written in C — simple and fast
- Rely only on the standard C runtime library, 
  making it portable to all operating systems
- A command-line tool, making it easy to integrate into scripts

# Why Subsync Is Needed
I have some old TV series and `.srt` subtitle files found online, 
but many of them are out of sync.
Since the structure of `.srt` is very simple, in theory you only need 
to extract the timestamps, adjust them slightly, and write them back.

Surprisingly, all I could find were large GUI programs, 
not the lightweight command-line tool I wanted.
Even worse, I couldn’t find any tool that could scale the subtitle 
timeline. Some videos run at 30 fps, while the matching subtitles 
were created for 25 fps. At the beginning, the video and subtitles 
are in sync, but the further you go, the larger the drift becomes.
A simple time offset won’t fix this. So I wrote this small program 
to fill that gap.

# Build and Install
You can fetch the source code from GitHub and compile it:
```
git clone https://github.com/xuminic/subsync.git
cd subsync
make
```
If everything goes well, it should produce the subsync executable.
You can move it anywhere on your system path.

To build the Windows version, you can compile directly on Windows 
using `MinGW64` or `Cygwin`, or cross-compile on Linux using MinGW, 
for example:
```
apt install mingw-w64
```
Then, inside the subsync directory:
```
make allwin
```
make will automatically download `libiconv-1.18.tar.gz`, compile it,
and generate both Win32 and Win64 versions of the subsync executable.
- You can place `libiconv-1.18.tar.gz` in the subsync directory 
  in advance to allow offline compilation
- The Windows executables are named as: 
  `subsync_i686.exe` and `subsync_x86_64.exe`
- Prebuilt Windows binaries are available directly in the GitHub 
  Releases section
- There is no installer for Windows — simply copy and use the program


# Command Line Options
- If no filename is specified, `subsync` reads from `stdin` and 
  writes to `stdout`, for example:
  ```
  subsync +12000 < source.ass > target.ass
  ```

- Any argument that is not a command-line option is treated as 
  an input filename. You mau use the `-w` option to specify the 
  output filename. For example:
  ```
  subsync +12000 -w target.ass source.ass
  ```
  This command is equivalent to the previous one.

- You may specify multiple filenames in a single command, 
  for example:
  ```
  subsync +12000 source1.ass source2.ass source3.ass
  ```
  However, all output will be combined and written to `stdout`.
  The same applies when using `-w`:
  ```
  subsync +12000 -w target.ass source1.ass source2.ass source3.ass
  ```
  All output will be combined into `target.ass`.

- To overwrite files directly, use the `-o` or `--overwrite` option:
  ```
  subsync +12000 -o source1.ass source2.ass source3.ass
  ```
  The output files will therefore be `source1.ass`, `source2.ass`, 
  and `source3.ass`.

- There is a slight difference between `--overwrite` and `-o`:
  `--overwrite` creates a backup file:
  ```
  subsync +12000 --overwrite source1.ass source2.ass source3.ass
  ```
  The modified content overwrites `source1.ass`, `source2.ass`, 
  and `source3.ass`, while backup files `source1.ass.bak`, 
  `source2.ass.bak`, and `source3.ass.bak` are created. 
  If something goes wrong, you can restore them.

- Time-offset option: `-/+OFFSET` is used to shift subtitle timing 
  forward or backward.
  - `+` increases timestamps, meaning subtitles appear later.
  - `-` decreases timestamps, meaning subtitles appear earlier.
  - `OFFSET` is the amount of shift applied to subtitle timestamps. 
    Supported formats include:
    - Milliseconds, e.g. 19700
    - `.srt` format timestamps, e.g. `0:0:10,190`
    - `.ass/.ssa` format timestamps, e.g. `0:0:10.19`
    - Timestamp arithmetic, e.g. `01:44:31,660-01:44:36,290`
    - See the [HOWTO: Time Offset](#howto:-time-offset) section below.

- Time-scaling option: `-SCALE` scales subtitle timestamps by a factor. 
  It may be:
  - A floating-point number, such as `1.1988`
  - Predefined constant `N-P` (equals `1.1988`)
  - Predefined constant `P-N` (equals `0.83417`)
  - Predefined constant `N-C` (equals `1.25`)
  - Predefined constant `C-N` (equals `0.8`)
  - Predefined constant `P-C` (equals `1.04271`)
  - Predefined constant `C-P` (equals `0.95904`)

  This option cannot be confused with `-OFFSET`, since scaling 
  must be a floating-point value.
  See the [HOWTO: Time Scale](#howto:-time-scale) section below.

- To delete subtitles within a specific range, 
  use `-c N:M` or `--chop N:M`. 
  
  `N:M` refers to subtitle sequence numbers for `.srt` files.

- To reorder `.srt` sequence numbers, 
  use `-r [NUM]` or `--reorder [NUM]`. 
  
  This is used to tidy up subtitle numbering, especially after 
  splitting or merging subtitles, which often results in messy 
  numbering. 
  
  Although this does not affect playback, it looks untidy.
  The `-r` option will renumber subtitles starting from 1.
  If `NUM` is provided, numbering will start from `NUM`.

- To specify a timestamp range, 
  use `-s TIME` or `--span TIME`.

  If you only want to modify subtitle timestamps within a certain 
  range rather than the whole file, use this option.
  A range looks like this: `-s 00:00:52,570 0:11:00,140`.
  If only the start time is given, e.g. `-s 00:00:52,570`,
  the default end time is the end of the file.

- Specify an output filename using `-w FILENAME` or `--write FILENAME`.

  If no output filename is provided, output goes to `stdout`,
  unless `-o` or `--overwrite` is used to modify files directly.

# Time Formats
`subsync` supports the following time formats:
- Integer value, in milliseconds, e.g. `19700`
- `.srt` timestamp format, e.g. `0:0:10,190`
  - The fields separated by colons represent hours, minutes, and seconds.
  - They may be omitted from the left, e.g. 1:20 means 1 minute 20 seconds.
  - The part after the comma is milliseconds: 190 means 190 ms.

- `.ass/.ssa` timestamp format, e.g. `0:0:10.19`
   - The fields separated by colons represent hours, minutes, and seconds,
     and may also be shortened the same way.
   - The part after the dot represents centiseconds: 
     `.19` means 19 centiseconds = 190 ms.

- Timestamp arithmetic, used for offsetting timestamps, 
  e.g. `01:44:31,660-01:44:36,290`. 
  `subsync` converts both timestamps to milliseconds and subtracts them. 
  The result may be positive or negative:
  -  A negative result shifts subtitles earlier
  - A positive result shifts subtitles later
  - Formats may be mixed, e.g. `01:44:31,660-12700`
  - This helps compute differences between timestamps from subtitles 
    in different languages

- Timestamp ratio, used for scaling timestamps, 
  e.g. `01:44:30,290/01:44:31,660`. 
  `subsync` converts both timestamps to milliseconds and divides them,
  producing a floating-point scaling factor:
  - If the result is less than 1, subtitle intervals become shorter
  - If the result is greater than 1, subtitle intervals become longer
  - Formats may be mixed, e.g. `01:44:31,660/12700`
  - Dividing the last subtitle’s time by the corresponding audio time 
    is a quick way to obtain a scale factor

- Note: When omitting hour/minute fields, be careful with 
  seconds vs. milliseconds.

  For example, 20 means 20 milliseconds, not 20 seconds.
  `20,0` or `20.0` means 20 seconds.

- Note: The value `20.0` is ambiguous and matches a scaling ratio, 
  not a timestamp. Scaling has higher priority than offset. 
  For example:
  ```
  subsync -20.0 -w target.ass source.ass
  ```
  This will enlarge the subtitle timing by 20×, not shift it earlier 
  by 20 seconds.

  To shift by 20 seconds, use any of the following:
  ```
  subsync -20,0 -w target.ass source.ass
  subsync -20000 -w target.ass source.ass
  subsync -0:20 -w target.ass source.ass
  subsync -0:20.0 -w target.ass source.ass
  ```

# HOWTO: Time Offset
Shifting timestamps is the most common operation: 
adding or subtracting a fixed value from subtitle timestamps 
causes the subtitles to appear later or earlier.
Most subtitle misalignment issues occur because some content 
was added to or removed from the opening of the video.
A simple timestamp offset can effectively correct this problem.

For example:
```
subsync +12000 < source.ass > target.ass
```
This increases all subtitle timestamps in `source.ass` by 12,000 
milliseconds, causing every subtitle to appear 12 seconds later.
In the parameter `+12000`, the `+` indicates an increase. 
Conversely, `-` indicates a decrease:
```
subsync -12000 < source.ass > target.ass
```
This decreases all timestamps by 12,000 milliseconds, causing every 
subtitle to appear 12 seconds earlier.

The time format may be an integer (milliseconds), 
or a standard `HH:MM:SS` format. Note that milliseconds 
in `HH:MM:SS` format appear in two different formats:
- `.srt`: `HH:MM:SS,mmm`
- `.ass/.ssa`: `HH:MM:SS.nn`
See the earlier [Time Formats](#time-formats) section for details.

To simplify calculations, `subsync` supports timestamp arithmetic 
as the offset parameter.
For example, you can pick any misaligned subtitle in the subtitle 
file, then find the correct line’s start time in the video, and 
subtract the subtitle timestamp from the correct video timestamp:
```
subsync +00:00:52,570-0:11:00,140 source.ass > target.ass
```
In this offset parameter `+00:00:52,570-0:11:00,140`:
- The leading `+` is ignored; using `-` makes no difference
- `00:00:52,570` is the correct start time of the dialogue in the video
- `0:11:00,140` is the incorrect start time in the subtitle file
- `subsync` calculates the difference automatically, 
  saving you the manual calculation
- The calculation formula is: `expected time − misaligned time`
- `expected time` and `misaligned time` can use different time formats.

# HOWTO: Time Scale
Some subtitles cannot be corrected by simply shifting timestamps. 
In these cases, the key symptom is that subtitles are synchronized 
at the beginning, but gradually drift as the video progresses,
sometimes appearing later, sometimes earlier.
This type of mismatch is usually caused by frame rate differences: 
for example, the video may be from an NTSC source while the subtitles
were created for a PAL source, creating a 30 fps vs. 25 fps mismatch.
Some films even use the original 24 fps cinema master, causing
desynchronization with both PAL and NTSC subtitles.

The solution is subsync’s timestamp scaling feature. 
Timestamp scaling multiplies every subtitle timestamp by a correction
factor to compensate for the drift. For example:
```
subsync -1.000955 source.ass > target.ass
```
If the scaling factor is greater than 1, subtitles drift later over time.
If the factor is less than 1, subtitles drift earlier over time.

The plus/minus sign does not affect scaling, `-1.000955` and 
`+1.000955` are equivalent.

Since scaling parameters are floating-point numbers, they rarely conflict
with offset parameters. If a conflict does occur, scaling takes precedence
over offset.

The scaling coefficient is computed as:
```
expected_time / misaligned_time
```
For example, suppose the subtitles are synchronized at the beginning,
but the last subtitle appears at `1:35:26,690`, while the actual 
spoken line occurs at `1:35:32,160`. 
This means the subtitles gradually drift earlier.
- Expected time: `1:35:32,160 = 5,732,160 ms`
- Misaligned time: `1:35:26,690 = 5,726,690 ms`
- Scaling factor: `5732160 / 5726690 ≈ 1.000955`

To simplify the calculation, subsync supports timestamp division
expressions directly as scaling parameters.

Using the example above:
```
subsync -1:35:32,160/1:35:26,690 source.ass > target.ass
```
In this scaling parameter `-01:35:32,160/1:35:26,690`:
- The `-` sign is ignored; `+` works the same
- `01:35:32,160` is the correct dialogue start time in the video
- `1:35:26,690` is the incorrect subtitle start time
- subsync computes the ratio automatically, replacing manual calculation
- The formula used is: `expected_time / misaligned_time`
- The timestamps may mix with different formats: 
  `-1:35:32,160/5726690` works the same


# HOWTO: Non-linear Editing
`subsync` supports basic non-linear editing, which is useful for 
fixing local subtitle offsets or gradual drift.
When working with TV series, once you determine the correct adjustment
formula for the first episode, you can often reuse the same parameters
for other episodes, greatly improving efficiency.

Use the `-s` option to specify the subtitle time range to process:
```
-s start-time-stamp [end-time-stamp]
```
- `start-time-stamp`: the starting point
- `end-time-stamp`: the ending point (optional; 
  if omitted, processing continues to the end of the file)

Example
```
subsync -s 0:01:15.00 1:23:34.00 -00:01:38,880-0:03:02.50 source.ass > target.ass
```
- The processing range is `0:01:15.00` to `1:23:34.00`
- The adjustment value comes from the expression `-00:01:38,880-0:03:02.50`
- This expression evaluates to `–83.62` seconds, meaning the subtitles
  in the selected range are shifted `83.62` seconds `earlier`.


# HOWTO: Batch Process
`subsync` is easy to integrate into shell scripts. For example:
```
for i in *.srt; do subsync +12000 "$i" > "$i.new"; done
```
You can also use it directly. Use the `-o` option to overwrite 
the original file:
```
subsync -o +12000 *.srt
```
Or use `--overwrite` to overwrite the original file while keeping a backup:
```
subsync --overwrite +12000 *.srt
```


# Case Study
## EVA3.3 Theatrical Edition
The video length of `EVA3.3 Theatrical Edition` is `1:32:52`. 
The subtitle file from Internet is a mess.
Opening the subtitle file `00002.v1.11_FINAL.ass` we find the last
dialogue line is:
```
Dialogue: 0,1:45:42.51,1:45:46.48,Comment,,0,0,0,,♫Peace in time we've never had it so good\N安享和平 生活从未如此美好
```
The subtitles extend about 10 minutes beyond the video; 
it likely contains scenes from the 巨神兵 segment.

Anyway, we locate the first line of speech, which occurs at `00:00:52,570`. The English subtitle is accurate, giving us a precise timestamp:
```
00:00:52,570  Tracking team, report current Eva unit positions.
```
The corresponding Chinese subtitle is:
```
Dialogue: 0,0:11:00.14,0:11:02.98,Default,,0,0,0,,追踪班 报告两机体现在的位置
```
We use subsync’s helper to compute the time difference:
```
$ subsync --help-sub 00:00:52,570 0:11:00.14
Time difference is -00:10:07,570 (-607570 ms)
```

First, compensate that ~10-minute offset with a timestamp shift:
```
$ subsync -00:10:07,570 00002.v1.11_FINAL.ass > 001.ass
```
Testing `001.ass`: the front is now synchronized, but drift appears later;
so frame rate adjustment is also needed.

Jumping to the end of the video, we find the last line of dialogue at:
```
01:35:32,160  The Wunder streaks through the sky
```
The matching Chinese line is:
```
Dialogue: 0,1:35:26.69,1:35:28.07,Default,,0,0,0,,划破天际的Wunder
```
Using subsync’s helper to compute the scale factor:
```
$ subsync --help-div 01:35:32,160 1:35:26.69
Time scale factor is 1.000955
```
This ratio is close to 24 / 23.976, which likely explains the drift. 
Apply the scale factor to the previously produced 001.ass:
```
$ subsync -1.000955 001.ass > 002.ass
```

Playing the video with `002.ass` now shows correct synchronization.

As a final check, the combined automatic command also produces 
the same result:
```
subsync +00:00:52,570-0:11:00,140 -01:35:32,160/1:35:26,690 00002.v1.11_FINAL.ass > 002.ass
```
The verification shows the automatic calculation matches the manual steps.

## 不可思议的海之娜蒂亚
This series has 39 episodes, and all of the subtitles are out of sync.
The video files are in MKV format and include embedded English subtitles.

First, extract the English subtitle track using `ffmpeg` for reference:
```
ffmpeg -i "Nadia Ep 01.mkv" -map 0:s:0 subs.srt
```
We can see the subtitles are correctly synchronized at the beginning.
In English subtitle track `subs.srt`:
```
1
00:00:03,200 --> 00:00:05,900
<font face="InfoDispBoldTf" size="46" color="#fffde1"><i>Are you adventurers,</i></font>
```
The corresponding Chinese subtitle:
```
Dialogue: 0,0:00:03.50,0:00:05.50,*Default,,0000,0000,0000,,你是一位冒险家吗
```
However, starting from subtitle line 18, the sync breaks:
```
18
00:01:09,630 --> 00:01:12,460
as the threat of a world war loomed ever closer.

19
00:01:38,880 --> 00:01:41,040
<font face="InfoDispBoldTf" size="46" color="#fffde1">Paris... Paris...</font>
```

The corresponding Chinese subtitles:
```
Dialogue: 0,0:01:08.00,0:01:11.80,*Default,,0000,0000,0000,,但是人们生活在 即将来临的世界阴影中
Dialogue: 0,0:03:02.50,0:03:04.90,*Default,,0000,0000,0000,,巴黎…巴黎…
```

The video interval is only 30 seconds, but the subtitle interval is 
over 2 minutes, which strongly suggests the Chinese subtitles were 
created from a version with a different opening sequence.

We can try repairing this section using `subsync`:
```
subsync -s 0:01:15.00 -00:01:38,880-0:03:02.50 "Nadia Ep 01.ass" > 01.ass
```

The result displays correctly; no timeline scaling is required.

However, Episode 1 is different from later episodes: it contains 
an opening monologue that does not appear again, but the Chinese 
subtitles still include those lines.

Therefore, starting from Episode 2, we manually delete the first 
18 subtitle lines in a text editor, then fix the timeline with `subsync`:
```
mv 01.ass 01.bak
subsync -00:00:01,710-00:01:25,510 -o  *.ass
mv 01.bak 01.ass
```

This corrects the timing for all episodes except the first, which uses 
its own specific adjustment.


## 工作细胞 Black
I did a quick preview and felt the subtitles were about 1 second early, 
so I first adjusted them with:
```
$ subsync +1000 -o *.ass
```

After closer inspection I found the first ~10 minutes were synced, 
but later the subtitles were about 6 seconds early. I located a 6-second
black cutaround at 10:38, so I applied a localized fix:
```
$ subsync -s 10:38 +6000 --overwrite 'Hataraku Saibou Black_-_01.ass'
```

Re-checking the video, the subtitles were then fully synchronized.

The annoying part is that, although the black-screen segment is the
same duration in every episode, its position differs per episode; 
so you must locate it episode by episode. The procedure is:
- Play the episode and jump roughly to the middle.
- If subtitles are in sync there, search forward; 
  if they are out of sync, search backward.
- Find the start time of the black-screen segment and note its time.
- Run `subsync -s <black-screen-start> +6000` on that episode.

So the command-line pattern looks like this:
```
$ subsync -s 10:38 +6000 --overwrite 'Hataraku Saibou Black_-_01.ass'
$ subsync -s 10:44 +6000 -o 'Hataraku Saibou Black_-_02.ass'
$ subsync -s 11:20 +6000 -o 'Hataraku Saibou Black_-_03.ass'
$ subsync -s 11:32 +6000 -o 'Hataraku Saibou Black_-_04.ass'
```

## NaNa (2005)/娜娜/世上的另一个我
For `Nana`, the first 10 minutes of subtitles are correct, 
but in the last 10 minutes they lag by 3 to 4 seconds.
Two aspects make this case more troublesome:
- There is no transition cut; it seems to have been removed entirely.
- The delay amount is inconsistent, anywhere from 3 to 5 seconds.

In this situation, you must manually locate where the subtitle 
delay starts, and then fine-tune the offset.

For example, in Episode 3, the subtitles are still in sync at `12:57`, 
but at `13:17` they already lag by more than 2 seconds. 
After checking back and forth, I found a scene cut at `13:09`, 
confirming this is the deletion point.

First, I tried shifting subtitles 3 seconds earlier:
```
$ subsync -s 13:10 -3000 -o 'S01E03-Nana and Shoji, Love'\''s Whereabouts [030F3BD0].ass'
```
The result was slightly ahead of the video, so I tried delaying 
by 0.5 second at the same position:
```
$ subsync -s 13:10 +500 -o 'S01E03-Nana and Shoji, Love'\''s Whereabouts [030F3BD0].ass'
```
This time the subtitles appeared properly synchronized. 
In general, a timing error within 0.5s is acceptable.

The first three episodes take more effort because you must adjust
repeatedly. Once you understand the pattern, later episodes become 
much faster to process.

Here is the actual command sequence I used:
```
$ subsync -s 8:20 -3500 -o S01E04-*.ass
$ subsync -s 10:43 -3500 -o S01E05-*.ass
$ subsync -s 11:13 -3500 -o S01E06-*.ass
$ subsync -s 11:13 -500 -o S01E06-*.ass
$ subsync -s 10:36 -4000 -o S01E07-*.ass
$ subsync -s 10:36 +500 -o S01E07-*.ass
$ subsync -s 10:36 +500 -o S01E07-*.ass
$ subsync -s 11:20 -3500 -o S01E08-*.ass
$ subsync -s 11:00 -3000 -o S01E09-*.ass
$ subsync -s 11:00 +500 -o S01E09-*.ass
$ subsync -s 12:16 -2500 -o S01E10-*.ass
$ subsync -s 13:04 -3500 -o S01E11-*.ass
$ subsync -s 13:04 +500 -o S01E11-*.ass
$ subsync -s 10:12 -3500 -o S01E12-*.ass
$ subsync -s 10:12 +500 -o S01E12-*.ass
```


## 再造人卡辛 (Casshern Sins)
One characteristic of Casshern Sins is that each episode begins with 
an opening segment of varying length (around a dozen seconds), followed 
by the OP, and then the main content.

Obviously, the OP in the Blu-ray version does not match the subtitled
version. If you delay the timeline after the OP by 1 minute and 30 seconds, 
the main content will be synchronized.

The problem is that the opening segment is of different lengths in each 
episode, so every subtitle file must be handled individually.
A simple approach is to open each original subtitle file one by one and
examine the beginning portion:

```
Dialogue: 0,0:01:42.93,0:01:51.20,staff,NTP,0000,0000,0000,,{\a6\fad(100,200)}翻譯 十六夜剎那 後期 秋月 暮葉 OPED歌詞協力 灰羽
Dialogue: 0,0:00:02.94,0:00:03.81,*Default,NTP,0000,0000,0000,,你是？
Dialogue: 0,0:00:04.56,0:00:05.98,*Default,NTP,0000,0000,0000,,我是卡辛
Dialogue: 0,0:00:08.12,0:00:11.20,*Default,NTP,0000,0000,0000,,露娜 殺了妳
Dialogue: 0,0:00:00.00,0:00:00.00,*Default,NTP,0000,0000,0000,,//-------------------OP-------------------
Dialogue: 0,0:01:01.98,0:01:06.00,*Default,NTP,0000,0000,0000,,從那天開始的 毀滅
Dialogue: 0,0:01:07.39,0:01:09.92,*Default,NTP,0000,0000,0000,,從殺死露娜的那一瞬間開始
```

The last line of dialogue before the OP starts at 0:00:08.12 and ends at 0:00:11.20. The next line begins at 0:01:01.98. Therefore, the interval from 0:00:12 to 0:01:01 is a possible adjustment range:
```
subsync -o -s 0:0:12 +0:1:30 S01E01*.ass
```

The complete processing workflow looks like this:
```
subsync -o -s 0:0:12 +0:1:30 S01E02*.ass
subsync -o -s 0:0:12 +0:1:30 S01E03*.ass
subsync -o -s 0:0:12 +0:1:30 S01E04*.ass
subsync -o -s 0:0:24 +0:1:30 S01E05*.ass
```





