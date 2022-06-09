# slowION

slowION is a benchmark tool that can simulate data rates of a nanopore sequencer (e.g., PromethION) in chunks and see if a simple strategy coupled associated with a simple binary format like BLOW5 could meet the real-time writing requirement (plus reading back to cater real-time basecalling).

There seem to be a bit of a misconception that writing data from 144,000 channels in parallel cannot be done with simple binary binary formats like BLOW5 and instead requires fancy (potentially over-engineered) solutions. My biased opinion is, don't bring a chain saw when a hand saw would suffice. If you do, would look fancy, but unanticipated issues will start appearing.

This is a quick benchmark tool that I wrote within a couple of days that demonstrate how BLOW5 can handle chunks, inspired by a [strategy proposed by @lh3](https://twitter.com/lh3lh3/status/1481042134442594306). This basic implementation that deliberately simulates a worse case scenario for BLOW5, even without doing any optimisations, seem to be able to handle 96,000 channels on my laptop (~2000 AUD) and 216,000 channels on a gaming work station (~10,000 AUD). The actual limit could be even much higher - I/O capacity never reached the maximum, instead, overheads in simulating the aquisition (random number generation, memory buffer copying, etc.) started becoming the bottleneck. Nevertheless, this is adequate to demonstrate the point.

Again, I would like to emphasise that this was a very quick implementation done in 2 days. So, don't mix implementation limitations with the limitations in the actual concept. For instance, some think BLOW5 signal records cannot be stored as a series of separately compressed chunks - that is because they never read the specification carefully or just assumed by prejudice.


# Building

You will need to install zlib and zstd 1.3 or higher development libraries. You will need gcc that supports c99.

```
sudo apt-get install zlib1g-dev libzstd1-dev #in latest Ubuntu: libzstd-dev
git clone --recursive https://github.com/hasindu2008/slowION
make
./slowION
```


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

- laptop with a 11th Gen i7-11800H, 32 GB RAM, SSD storage (EXT4 file system), running Ubuntu can keep up with 32 positions with 3000 channels in each (96000 channels in total).
- a gaming desktop with a AMD Ryzen Threadripper 3970X, 128 GB RAM, SSD storage (EXT4 file system), running Ubuntu can keep up with 72 positions with 3000 channels in each (216000 channel in total).

# Methods

Do not have time to write this at the moment. One may ask for clarification on GitHub issues, as the code lacks comments and would be a bit hard to follow.


# Options

*  `-p INT`: number of positions [1]
*  `-c INT`: channels per position [512]
*  `-T INT`: simulation time in seconds [300]
*  `-r INT`: mean read length (num bases) [10000]
*  `-b INT`: average translocation speed (bases per second) [400]
*  `-f INT`: sample rate [4000]
*  `-d DIR`: output directory [./output]
*  `--verbose INT`: verbosity level [4]

Note that only the default parameters and values in the above examples in running section have been tested.

# Notes

- Documentation and error checking are minimal as it takes too much time. For clarification you can use GitHub issues.
- Not optimised or feature rich - so perhaps underestimate the capability of a binary format.
- mean read length (-r) is not the mean value of all the reads that is modelled by gamma distribution. It is NOT the max read length.
- This is not an API. A chunk based writing API is a matter of implementation.
- Only tested on Linux with limited gcc versions. May work on Mac. Getting these working on Windows is just a simple implementation matter.
-
- There could be bugs or mistakes, feel free to point them out.