# slowION

slowION is a benchmark tool that can simulate data rates of a nanopore sequencer (e.g., PromethION) in chunks and see if a simple strategy coupled with a simple binary format like BLOW5 could meet the real-time writing requirement (plus reading back to cater real-time basecalling).

There is misconception that writing data from 144,000 channels in parallel cannot be done with simple binary binary formats like BLOW5 and instead requires fancy (potentially over-engineered) solutions. My personal opinion is, don't bring a chain saw when a hand saw would suffice. If you do, it looks fancy, but unanticipated issues will start appearing.

This is a quick benchmark tool that I wrote within a couple of days that demonstrate how BLOW5 can handle chunks, inspired by a [strategy proposed by @lh3](https://twitter.com/lh3lh3/status/1481042134442594306). This basic implementation that deliberately simulates a worse case scenario for BLOW5, even without doing any optimisations, could handle 96,000 channels on my laptop (~2000 AUD) and 216,000 channels on a gaming work station (~10,000 AUD). The actual limit could be even much higher - I/O capacity never reached the maximum, instead, overheads in simulating the aquisition (random number generation, memory buffer copying, etc.) started becoming the bottleneck. Nevertheless, this is adequate to demonstrate the point.

# Building

You will need to install zlib and zstd 1.3 or higher development libraries. You will need gcc that supports c99.

```
sudo apt-get install zlib1g-dev libzstd1-dev #in latest Ubuntu: libzstd-dev
git clone --recursive https://github.com/hasindu2008/slowION && cd slowION
make
./slowION
```

On CentOS use `sudo yum install libzstd-devel`. On Mac, use `brew install zstd`. On Mac M1, if zstd.h is still not found, give the location to make as `LDFLAGS=-L/opt/homebrew/lib/ CPPFLAGS=-I/opt/homebrew/include/ make`.

# Running

When you run example commands below, if many yellow colour WARNING are printed, that means there is a lag.

```
# Minion (1 position, 512 channels per position)
./slowION -p 1 -c 512

# PromethION P24 (24 positions, 3000 channels per position)
./slowION -p 24 -c 3000

# PromethION P48 (48 positions, 3000 channels per position)
./slowION -p 48 -c 3000
```

Because of the naive way I implemented for this proof of concept benchmark, you may get an error like "too many files open" on some systems. The easiest trick to get it working will be:

```
sudo su
./slowION -p 24 -c 3000
```

# Results

See the preprint at https://doi.org/10.1101/2025.06.30.662478 for thorough benchmarks with 5KHz sample rate.

The following tables shows some early benchmarks done with 4KHz sample rate. This table shows how many sequencing positions (3000 channels per position) and total channels each system could keep up.

| system                   | CPU (cores/threads)                  | RAM    | O/S       | Disk System | File system | positions | total channels |
|--------------------------|--------------------------------------|--------|-----------|-------------|-------------|-----------|----------------|
| laptop (Dell Inspiron)                  | Intel 11th Gen i7-11800H (8/16)      | 32 GB  | Ubuntu 18   | SSD         | EXT4        | 32        | 96000          |
| laptop (Dell XPS)                  | Intel 11th Gen i7-11800H (8/16)      | 32 GB  | Pop OS 22    | SSD         | EXT4        |   48      |   144000        |
| gaming desktop           | AMD Ryzen Threadripper 3970X (32/64) | 128 GB | Ubuntu 18 | SSD         | EXT4        | 72        | 216000         |
| NVIDIA Jetson Xavier AGX | Armv8.2 (8/8)                        | 16 GB  | Ubuntu 18 | SSD         | EXT4        | 14        | 42000          |
| server with HDD          | Intel Xeon Gold 6154 (36/72)         | 377 GB | Ubuntu 18 | HDD (12 disks RAID 6)!   | EXT4        | 72        | 216000         |
| server with network mount    | Intel Xeon Silver 4114 CPU (20/40)   | 377 GB | Ubuntu 18 | An HDD NAS (12 disks RAID 10)!     | EXT4 over NFS        | 32        | 96000          |
| Mac M1 mini   | ARM M1 (8/8)   | 8 GB | macOS 12.1 | SSD     | APFS        |   10     |  30000        |
| MacBook M2   | ARM M2 (8/8)   | 24 GB | macOS 12.1 | SSD     | APFS        |   24     |  72000        |

Note: The benchmark was run for an hour on the gaming desktop and the servers, while only for 5 minutes on others due to limited free storage space.

# Methods

Please refer to the preprint: https://doi.org/10.1101/2025.06.30.662478.

# Options

*  `-p INT`: number of positions [1]
*  `-c INT`: channels per position [512]
*  `-T INT`: simulation time in seconds [300]
*  `-r INT`: mean read length (num bases) [10000]
*  `-b INT`: average translocation speed (bases per second) [400]
*  `-f INT`: sample rate [4000]
*  `-d DIR`: output directory [./output]
*  `--verbose INT`: verbosity level [4]

# Notes

- Documentation and error checking are minimal as it takes too much time. For clarification you can use GitHub issues.
- Not optimised or feature rich - so perhaps underestimate the capability of a binary format.
- mean read length (-r) is not the mean value of all the reads that is modelled by gamma distribution. It is NOT the max read length.
- This is not an API. A chunk based writing API is a matter of implementation.
- Only tested on Linux and Mac with limited gcc and clang versions Getting these working on Windows is just a simple implementation matter.
- There could be bugs or mistakes, feel free to point them out.
- This is NOT a signal simulator intended for basecalling. See [Squigulator](https://github.com/hasindu2008/squigulator) instead.
- Opening many files was only for easy implementation.
- Multiple threads can be used for writing to a single SLOW5 file, though not implemented here.
- The 2-pass concept is inherently suitable for using expensive SSD as a cache before writing to cheap HDD or even a NAS (not implemented).
- Again, I would like to emphasise that this was a very quick implementation done in 2 days. So, don't mix implementation limitations with the limitations in the actual concept. For instance, some think BLOW5 signal records cannot be stored as a series of separately compressed chunks - probably from making their mind up before reading the format specification.




