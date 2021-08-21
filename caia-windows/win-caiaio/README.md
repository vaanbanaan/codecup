# win-caiaio
CodeCup caiaio Windows port  

### Features:
- Accepts all CodeCup caia project commands (including firsterror)
- Support for batch files

### Differences:
- Player program stderr writes are processed during player's turn (APC).  
  This is done to prevent the stderr pipe buffer getting exhausted, blocking the player's program, resulting in a time-out.  
  Player's max move time may decrease significantly when writing large quantities of data to stderr (not recommended anyway)

### Limitations:
- Time measurement is done in milliseconds

### Bugs:
- Probably plenty  
- Graceful quit win-caiaio when pressing CTRL-C sort of works, but not always\
  This can leave the manager, referee and / or players active in a suspended state\
  You'll have to end them manually in the tastmanager
- The command console switches to VT terminal mode after win-caiaio has quit\
  The referee and players are compiled by the CodeCup team for Cygwin\
  Somehow this leaves the console in the wrong mode when run in a command console\
  Cursor arrows / Escape keys won't work anymore (are displayed as terminal sequences)
  - Workaround
    Run win-caiaia in a Powershell console
