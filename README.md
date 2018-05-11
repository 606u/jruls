# jruls

`jruls` is a top-alike utility to display FreeBSD jails resource usage.


## Usage

Just run it; same as with `top`.

    # ./jruls -d1
    jid  name                  cpu%     mem    r/s    read    w/s  writtn
      1  www                      0   8936K      0       0      0       0
      2  tm                       0   14.0M      0       0      0       0
      3  pms                      0  164.5M      0       0      0       0
      4  imap                     0   11.6M      0       0      0       0
      5  homebridge               0   5324K      0       0      0       0
      6  fever                    0   79.0M      0       0      0       0
      7  develop                177   56.0M     10   1280K      1      81
      8  chitanka                 0  101.6M      0       0      0       0
      9  cdns                     0   68.7M      0       0      0       0
    #


## Compilation

Type `make`.
