# Subsync -- Synchronize your subtitles

Subsync is a command line filter to synchronize the timeline of subtitles.
It can shift, scale and non-linearly edit the timeline.
It's simple, fast and easy to be integrated to Shell scripts.
It supports .srt, .ass and .ssa formats. It's written in C and standard 
libraries so is able to be compiled in most of Posix systems like 
Linux, BSD, Cygwin, etc.

## Motivation

I had a couple of old TV shows and out-of-sync .srt files searched from websites. 
I knew the structure of .srt was quite simple so theoratically a small filter 
program can do the trick: filter out the time stamps, do some primary schooltime
mathematics, output back to the stream. 
To my surprise I couldn't find a command line tool to do it properly.
Something were either too fancy with a heavy GUI interfce, or too poor written 
by Shell scripts to do the basic calculation, especially to scale the timeline.
Finally I decided to write one for my own needs. It might be reinventing the 
wheels. But come on, it costs only a handful hundred lines and a Sunday morning.

## Installation

To install subsync, simply:

```
git clone https://github.com/xuminic/subsync.git
cd subsync
make
```

When successful, it will build the executable file `subsync`. 
You may have it moved to anywhere accessible for you.

## Command Line Options

If no file was specified, `subsync` will read from stdin and write to stdout,
such as:
```
subsync +12000 < source.ass > target.ass
```
Otherwise it reads from the first non-option argument. 
The output file can be specified by `-w` option. 
For example, the above command is same to

```
subsync +12000 -w target.ass source.ass
```

Other options are:

* -c, --chop N:M

chop off the specified number of subtitles. You may use Vi to do the same thing.

* -o, --overwrite

overwrite the original file. It's useful in batch processing, 
but be wisely backing up your files before doing so.

* -r, --reorder [NUM]

reorder the serial number from `NUM`. It can be tidy up a little bit 
when splitting or merging `.srt` files.

* -s, --span TIME

specifies the range of the time for processing. Used in non-linear editing.

* -w, --write FILENAME

specifies the output file.

* -/+OFFSET

specifies the offset of the timeline. 
The prefix `+` or `-` defines postpone or bring forward. 
It is defined by milliseconds like `+19700`, by HH:MM:SS.MS form, 
such as `-0:0:10,199`, or by subtraction statement like 
`+01:44:31,660-01:44:36,290`. 
See "HOWTO: Shift Timeline".

* -SCALE
 
specifies the scaling ratio of the timeline. 
It is defined by real number like `1.1988`, by predefined identifiers 
like `N-P`, or by dividing statement like `-01:44:30,290/01:44:31,660`.
The predefined identifiers are `N-P` which is same to 1.1988, `P-N`, 
same to 0.83417, `N-C`, same to 1.25, `C-N`, same to 0.8, `P-C`, same 
to 1.04271, and `C-P`, same to 0.95904.
See "HOWTO: Scale Timeline"

## HOWTO: Shift Timeline

Timeline shifting means a fixed piece of time will be added up to, 
or taken away from the original time stamp in the subtitles. 
Timeline shifting is commonly used because most of the out-of-sync 
scenario were caused by the prefaces of different video sources. 

For example, this command

```
subsync +12000 < source.ass > target.ass
```

will add 12000 milliseconds to all time stamps in source.ass, and pipe to the
new file `target.ass`, which is equal to delay all subtitles 12 seconds.
Whereas the `+` means add-up, the `-` would be read as taken-way, so

```
subsync -12000 < source.ass > target.ass
```

will bring forward all subtitles 12 seconds. The time stamp of offset can be
an integer, which is in unit of millisecond, or a more readable form HH:MM:SS.MS
which is also used in the subtitle files. For example:

```
subsync -00:10:07,570 source.ass > target.ass
```

To simplify the calculation, subsync supports subtraction statement
by the expected time stamp subtracting the misaligned time stamp. 
For example, you randomly pick up a line of subtitle in the subtitle file. 
The time stamp was written `0:11:00,140`. It's the misaligned time stamp.
The corresponding line in the video accutely sounds at `00:00:52,570`. 
It's the expected time stamp. You don't have to calculate the time difference
but simply using the formula `expected-time-stamp - misaligned-time-stamp`:

```
subsync +00:00:52,570-0:11:00,140 source.ass > target.ass
```

subsync will do the calculating itself. Nevermind the `+` character in the expression.
It's just an option switch. You may use

```
subsync -00:00:52,570-0:11:00,140 source.ass > target.ass
```

They are identical. 

Note that there is a minor difference of the time stamps between `.srt` and `.ass`.
The `.srt` uses like `00:10:07,570`, where the `570` after comma is milliseconds.
On the other hand the `.ass` is like `00:10:07.57`, where the `57` after dot is 
in unit of 10 milliseconds. Subsync accepts any of them.

## HOWTO: Scale Timeline

In some scenario shifting timeline won't have it synchronized. You may find some videos
had synchronized subtitle in the beginning but drifted away in the end. The offset can vary 
from a couple of seconds to 4 or more minutes. It mostly caused by different frame rates.
For example, your subtitle was ripped from a PAL based video but is wished to match to a 
NTSC video. Obviously the frame rate is 25 against 30. Sometimes it can be more subtle, 
like video had been ripped as 24 fps but subtitle was 23.976. They are both 24 fps but 
you may find some seconds delay, which is quite annoying. 

The cure is to scale the timeline, which is simply multiplying a ratio to every time stamps
in the subtitle file. For example:

```
subsync -1.000955 source.ass > target.ass
```

When the ratio is greater than 1, the timeline prolongs. 
When the ratio is less than 1, the timeline shortens. 
The ratio is always a positive real number so here the `-` is
just a switch; `+1.000955` do the same trick.

The ratio can be manually found by this formula: 
`expected-time-stamp  / misaligned-time-stamp`.
Assuming your video and subtitle are synchronized in the beginning, 
but the last subtitle line, for example, was found `1:35:26,690`. 
Yet the scene was actually `01:35:32,160` so the subtitle was drifting forwards. 
The expected time stamp is `01:35:32,160`, equal to `5732160 milliseconds`. 
The misaligned time stamp is `1:35:26,690`, equal to `5726690 milliseconds`.
Therefore the ratio is `5732160 / 5726690 = 1.00095517655050299562`, 
where we only take `1.000955`.

To simplify the calculation, subsync also supports dividing statement
by the formula `expected-time-stamp  / misaligned-time-stamp`. 
For example by the above arguments:

```
subsync -01:35:32,160/1:35:26,690 source.ass > target.ass
```

subsync will do the calculating itself. Nevermind the `-` character in the expression.
It's just an option switch. You may use

```
subsync +01:35:32,160/1:35:26,690 source.ass > target.ass
```

They are identical.


## HOWTO: Non-linear Editing

Subsync only supports limited non-linear editing. 
Sometimes the subtitle need both shifting and scaling. 
Sometimes only part of the subtitles need to be adjusted.
In extreme situation you may have to use a subtitle editor.
However if the situation is not that bad, subsync could speed up the process.
Especially like the TV drama, once you figured out the formula of the first episode,
it can be quickly applied on other episodes.

The timeline shifting and scaling can be combined in one command.
The process order is shifting first, then scaling.
Because the offset number is rather small so the error by multiplying
the ratio can be ignored. The command line may look like this:

```
subsync +00:00:52,570-0:11:00,140 -01:35:32,160/1:35:26,690 source.ass > target.ass
```

Subsync supports a range to process. The option is 

```
-s start-time-stamp [end-time-stamp]
```

The `end-time-stamp` is optional. The default is to the end of file if ignored.
For example:

```
subsync -s 0:01:15.00 1:23:34.00 -00:01:38,880-0:03:02.50 source.ass > target.ass
```

Which means starting from 1 minute 15 seconds, ending at 1 hour 23 minutes 34 seconds,
all subtitle time stamp will be brought forward 83.62 seconds.


## HOWTO: Batch Process

You may use Shell script to do the batch process, for example:

```
for i in *.srt; do subsync +12000 $i > $i.new; done
```

or by using `-o` option to override the original file so it looks neat:

```
subsync -o +12000 *.srt
```

Please keep in mind that backup your original files before the timeline
were totally steins-gated.

## Usage Examples

### EVA3.3 Theatrical Edition

The video length of EVA3.3 Theatrical Edition is 1:32:52. 
The subtitle file 00002.v1.11_FINAL.ass looked quite chaos.
Looked into the file, the last line is

```
Dialogue: 0,1:45:42.51,1:45:46.48,Comment,,0,0,0,,♫Peace in time we've never had it so good\N安享和平 生活从未如此美好
```

which was 10 minute longer than the video. 
By the way the 10 minute scene was about the story of 巨神兵.
Anyway found out the first meaningful talk at `00:00:52,570`, 
`Tracking team, report current Eva unit positions.`

The corresponding part in subtitle file is 

```
Dialogue: 0,0:11:00.14,0:11:02.98,Default,,0,0,0,,追踪班 报告两机体现在的位置
```

Using subsync to calculate the differece:

```
$ subsync --help-sub 00:00:52,570 0:11:00.14
Time difference is -00:10:07,570 (-607570 ms)
```

Manually delete the 巨神兵 scene in the subtitle file first. Then 

```
$ subsync -00:10:07,570 00002.v1.11_FINAL.ass > 001.ass
```

Tried this subtitle, it was sync-ed in the beginnging then drafted away.
Apparently it was also needed scaling. 
Went to the end part of the video, found the last talk at `01:35:32,160`, 
`The Wunder streaks through the sky`. The corresponding part in subtitle file is

```
Dialogue: 0,1:35:26.69,1:35:28.07,Default,,0,0,0,,划破天际的Wunder
```

Using subsync to calculate the scaling ratio:

```
$ subsync --help-div 01:35:32,160 1:35:26.69
Time scale factor is 1.000955
```

This ratio is very closed to 24/23.976, probably that's the reason of drifting.
Anyway using subsync process `001.ass` again:

```
$ subsync -1.000955 001.ass > 002.ass
```

It works. I'd also tried with one command. The result was same.

```
subsync +00:00:52,570-0:11:00,140 -01:35:32,160/1:35:26,690 00002.v1.11_FINAL.ass > 002.ass

```

### 不可思议的海之娜蒂亚

It's a 39 episode anime. All were out of sync. The video includes English subtitle
so it can be ripped out by `ffmepg` for reference:

```
ffmpeg -i "Nadia Ep 01.mkv" -map 0:s:0 subs.srt
```

The first subtitle was sync-ed:
```
1
00:00:03,200 --> 00:00:05,900
<font face="InfoDispBoldTf" size="46" color="#fffde1"><i>Are you adventurers,</i></font>
```
vs the ASS subtitle:
```
Dialogue: 0,0:00:03.50,0:00:05.50,*Default,,0000,0000,0000,,你是一位冒险家吗
```

However it went out of sync since No.18 lines:
```
18 00:01:09,630 --> 00:01:12,460 as the threat of a world war loomed ever closer.
19
00:01:38,880 --> 00:01:41,040
<font face="InfoDispBoldTf" size="46" color="#fffde1">Paris... Paris...</font>
```
vs the ASS subtitle:
```
Dialogue: 0,0:01:08.00,0:01:11.80,*Default,,0000,0000,0000,,但是人们生活在 即将来临的世界阴影中
Dialogue: 0,0:03:02.50,0:03:04.90,*Default,,0000,0000,0000,,巴黎…巴黎…
```

The video had only 30 seconds gap between No.18 and No.19. 
Yet the subtitle file had 2 minutes. Most likely contributed by different theme song.
So tried it with subsync

```
subsync -s 0:01:15.00 -00:01:38,880-0:03:02.50 "Nadia Ep 01.ass" > 01.ass
```

The test was good, no need to scaling. However the first episode is different to
other episodes. The first scene only appeared in this episode so I had to process
another episode manually. From episode 2 the first 47 lines, which were 18 subtitle
lines, must be removed. The time difference was `-00:00:01,710-00:01:25,510`
so it can be done by

```
subsync -00:00:01,710-00:01:25,510 -r -o  *.srt
```

The `-r` option was used to regenerate the serial number of SRT file.


