# 🎄 Advent of Code 2023 🎄

This repository contains my solutions for [Advent of Code
2023](https://adventofcode.com/2023), my third year of participation.

## What Is Advent of Code?

[Advent of Code](https://adventofcode.com/) is a series of small programming
puzzles created by [Eric Wastl](http://was.tl/). Every day from December 1st to
25th, a puzzle is released alongside an engaging fictional Christmas story. Each
puzzle consists of two parts, the second of which usually contains some
interesting twist or changing requirements and is only unlocked after completing
the first one. The objective is to solve all parts and collect fifty stars ⭐
until December 25th to save Christmas.

Many users compete on the [global
leaderboard](https://adventofcode.com/2023/leaderboard) by solving the puzzles
in an unbelievably fast way in order to get some extra points. Personally, I see
Advent of Code as a fun exercise to do during the Advent season while waiting
for Christmas. I often use it to learn a new programming language (like I did in
2021 with `C#`) or some advanced programming concepts. I can only encourage you
to participate as well – of course in a way that you find fun. Just get started
and learn more about Advent of Code [here](https://adventofcode.com/2023/about).

## About This Project

For this year of Advent of Code, I decided to use `C` (instead of `C#` as in the
previous two years). This not only gave me the opportunity to try out the new
`C23` standard in practice, but also allowed to improve my systems programming
skills (memory management, pointers, alignment, bit manipulation, etc.). I
developed a small library called [`SCU`](https://github.com/Piwimau/scu)
alongside, which contains various utilities for things like assertions, error
handling, I/O, math, string manipulation, and more. These utilities proved to be
very useful when solving the puzzles and will likely be as well in any of my
future projects.

For this project and in general when developing software, I strive to produce
readable and well-documented source code. However, I also enjoy benchmarking and
optimizing my code, which is why I sometimes implement a less idiomatic, yet
more efficient solution at the expense of readability. In such situations, I try
to document my design choices with analogies, possible alternative solutions and
sometimes little sketches to better illustrate the way a piece of code works.

The general structure of this project is as follows:

```plaintext
day-01-trebuchet/
  resources/
    .gitkeep
  src/
    main.c
day-02-cube-conundrum/
  resources/
    .gitkeep
  src/
    main.c
...
day-25-snowverload/
  ...
subprojects/
  ...
.clang-format
.gitignore
LICENSE
meson.build
meson.options
README.md
```

The repository contains 25 standalone projects for the days of the Advent
calendar, organized into separate directories. Each one provides a `src`
directory with a `main.c` file in which the execution begins. In addition, there
is a `resources` directory which contains the puzzle description and my personal
input for that day. However, [as requested](https://adventofcode.com/2023/about)
by the creator of Advent of Code, these are only present in my own private copy
of the repository and therefore not publicly available.

> If you're posting a code repository somewhere, please don't include parts of
> Advent of Code like the puzzle text or your inputs.

As a consequence, you will have to provide your own inputs for the days, as
described in more detail in the following section.

## Dependencies and Usage

If you want to try out one of my solutions, simply follow these steps below:

1. Ensure you have a compiler supporting the latest `C23` standard (such as
   `GCC`) installed on your machine.

2. Clone the repository (or download the source code) to a directory of your
   choice.

   ```shell
   git clone https://github.com/Piwimau/advent-of-code-2023 ./advent-of-code-2023
   cd ./advent-of-code-2023
   ```

3. [As explained above](#about-this-project), the solutions depend on my small
   utility library [`SCU`](https://github.com/Piwimau/scu). You can either
   download, build and install it yourself, or simply let Meson handle it for
   you, which automatically includes it as a subproject locally if not found on
   your system.

4. Once you have downloaded the source code and optionally installed `SCU` on
   your system, you can configure the build process:

   ```shell
   meson setup build
   ```

   By default, this will configure the build process to produce unoptimized
   debug executables. To build optimized release executables instead, specify
   the `--buildtype=release` option:

   ```shell
   meson setup build --buildtype=release
   ```

   To build the solution for a specific day, run the following command, where
   `<day>` corresponds to the directory of the day (e.g., `day-01-trebuchet`):

   ```shell
   meson compile -C build <day>
   ```

5. To run the solution for a specific day, run the following command:

   ```shell
   meson compile -C build run-<day>
   ```

   Again, `<day>` corresponds to the directory of the day (e.g.,
   `day-01-trebuchet`). Note that the solutions read the puzzle input from the
   standard input stream. The `run-<day>` target expects a file called
   `input.txt` in the `resources` directory of the selected day, which is used
   to redirect the standard input stream. [As explained
   above](#about-this-project), my input files are not included in the
   repository, so you'll have to create them yourself and paste your puzzle
   input into them. You can find your input for each day
   [here](https://adventofcode.com/2023) if you haven't downloaded it already.

## Timings

Finally, here are some simple (non-scientific) timings I created using
[`SCU`](https://github.com/Piwimau/scu) and my main machine (Intel Core
i9-13900HX, 32GB DDR5-5600 RAM) running Windows 11 25H2. All used
`--buildtype=release` and `-Dnative=true` to take advantage of
(machine-specific) optimizations. The reported times are the result of ten runs
and represent the (real) wall time, including the time spent for parsing the
input, as well as printing the puzzle results. CPU times were usually slightly
lower, but quite similar in general.

| Day                                     |        Min |        Max |       Mean |     Median | Standard Deviation |
|-----------------------------------------|-----------:|-----------:|-----------:|-----------:|-------------------:|
| Day 1 – Trebuchet                       |   0.671 ms |   0.923 ms |   0.795 ms |   0.798 ms |           0.068 ms |
| Day 2 – Cube Conundrum                  |   0.204 ms |   0.555 ms |   0.363 ms |   0.356 ms |           0.094 ms |
| Day 3 – Gear Ratios                     |   0.577 ms |   0.698 ms |   0.629 ms |   0.611 ms |           0.047 ms |
| Day 4 – Scratchcards                    |   0.835 ms |   1.248 ms |   0.950 ms |   0.888 ms |           0.130 ms |
| Day 5 – If You Give A Seed A Fertilizer |   0.168 ms |   0.527 ms |   0.290 ms |   0.271 ms |           0.109 ms |
| Day 6 – Wait For It                     |   0.090 ms |   0.237 ms |   0.182 ms |   0.190 ms |           0.051 ms |
| Day 7 – Camel Cards                     |   0.468 ms |   0.693 ms |   0.535 ms |   0.508 ms |           0.072 ms |
| Day 8 – Haunted Wasteland               |   2.239 ms |   2.654 ms |   2.368 ms |   2.360 ms |           0.124 ms |
| Day 9 – Mirage Maintenance              |   0.665 ms |   0.934 ms |   0.757 ms |   0.756 ms |           0.090 ms |
| Day 10 – Pipe Maze                      |   1.860 ms |   2.239 ms |   2.009 ms |   1.994 ms |           0.114 ms |
| Day 11 – Cosmic Expansion               |   0.567 ms |   0.799 ms |   0.650 ms |   0.628 ms |           0.075 ms |
| Day 12 – Hot Springs                    | 234.053 ms | 242.009 ms | 236.478 ms | 235.682 ms |           2.519 ms |
| Day 13 – Point of Incidence             |   0.395 ms |   0.775 ms |   0.573 ms |   0.588 ms |           0.119 ms |
| Day 14 – Parabolic Reflector Dish       |  28.712 ms |  29.258 ms |  28.990 ms |  28.978 ms |           0.148 ms |
| Day 15 – Lens Library                   |   1.004 ms |   1.440 ms |   1.130 ms |   1.103 ms |           0.128 ms |
| Day 16 – The Floor Will Be Lava         |  33.441 ms |  35.787 ms |  34.694 ms |  34.822 ms |           0.790 ms |
| Day 17 – Clumsy Crucible                | 278.827 ms | 296.070 ms | 286.835 ms | 286.840 ms |           6.047 ms |
| Day 18 – Lavaduct Lagoon                |   0.328 ms |   0.530 ms |   0.392 ms |   0.353 ms |           0.067 ms |
| Day 19 – Aplenty                        |   0.622 ms |   1.008 ms |   0.775 ms |   0.741 ms |           0.128 ms |
| Day 20 – Pulse Propagation              |   7.384 ms |   7.989 ms |   7.704 ms |   7.723 ms |           0.184 ms |
| Day 21 – Step Counter                   |  13.120 ms |  15.744 ms |  14.291 ms |  14.043 ms |           0.827 ms |
| Day 22 – Sand Slabs                     |   9.938 ms |  11.405 ms |  10.500 ms |  10.360 ms |           0.523 ms |
| Day 23 – A Long Walk                    | 181.746 ms | 195.721 ms | 188.118 ms | 189.021 ms |           4.447 ms |
| Day 24 – Never Tell Me The Odds         |   0.689 ms |   0.968 ms |   0.837 ms |   0.851 ms |           0.090 ms |
| Day 25 – Snowverload                    |  14.609 ms |  16.359 ms |  15.395 ms |  15.357 ms |           0.508 ms |
| Total                                   | 813.212 ms | 866.570 ms | 836.240 ms | 835.822 ms |          17.499 ms |

> [!NOTE]
>
> The timings shown above represent actual computation times, not total process
> lifetimes. Startup and shutdown of processes usually introduces an overhead of
> a few milliseconds, but since this is not relevant for the performance of the
> solutions themselves, I decided to ignore it here.

## License

This project is licensed under the [MIT License](LICENSE). Feel free to
experiment with the code, adapt it to your own preferences, and share it with
others.