# Laurel

Timing opt tool base on OpenSTA and Eden/Resizer
Use Flow Algorithm to optimize timing cost or violations

## To do list

- [ ] add internal tcl cmd (now it is parasitic in openroad.tcl)
- [ ] add cost functions for PPA
- [ ] Add a more accurate delay calculation system

- add more testcases (long term)

## CMD

use `laurel` as the most top interface to call setup or hold optimize system.

for example if you wanna fix hold violations with a slack margin 0.1

```tcl
laurel -hold -slack_margin 0.1
```

if you wanna optimize setup delay with a default slack margin 0

```tcl
laurel -setup
```
