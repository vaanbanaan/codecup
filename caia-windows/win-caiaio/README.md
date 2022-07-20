# win-caiaio
CodeCup caiaio Windows port  

### Features:
- Accepts all CodeCup caia project commands (including firsterror)
- Support for batch files

### Limitations:
- Time measurement is done in milliseconds
- Player's program stderr is read from invisible consoles\
  stderr output size has to be limited (max ~8kB per move) to prevent information loss\
  \
  This is a trade-off as Windows sets stderr to full buffered if redireced to anything other\
  than a FILE_TYPE_CHAR handle (e.g. pipe)\
  The C99 standard doesn't mandate stderr flushing, so assume external programs don't either\
  Therefore the firsterror function would fail if stderr is redirected to a pipe
 
### Bugs:
- Probably plenty
- Graceful quit win-caiaio when pressing CTRL-C sort of works, but not always\
  This can leave the manager, referee and / or players programs active in a suspended state\
  You'll have to end them manually in the taskmanager