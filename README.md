# WinTime

A time-like utility for Windows/PowerShell.

## Use
- `-r` number of runs to average (deafults to 1)
- `-w` number of warmup runs (default to 0)
- `-p` "portable", outputs in a format like a Unix time utility (defaults to false)
- `-n` "no echo", prevents the program being timed to write to the console (defaults to false)

## Example output
```
PS \> wintime -r 1000 -n wintime
Real time:   0.005625s
User time:   0.001359s (24.2%)
Kernel time: 0.003297s (58.6%)
Unnacounted time: 0.000969s (17.2%)
Cycle time:  15637750 cycles
Page faults: 838.15
Peak pagefile usage: 677138.43 bytes
Peak paged pool usage: 24472.00 bytes
Peak non-paged pool usage: 4784.00 bytes
Peak working set size: 3271110.66 bytes

PS \> wintime -p -n wintime
real 0.006
user 0.000
sys 0.016
```