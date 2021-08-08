# win_caiaio
CodeCup caiaio Windows port  

### Features:
- Accepts all CodeCup caia project commands (including firsterror)
- Support for batch files

### Differences:
- Player program stderr writes are processed during player's turn.  
  This is done to prevent the pipe buffer getting exhausted, blocking the player program, resulting in a time-out.  
  Player's max move time might decrease significantly when writing large quantities of data to stderr (not recommended anyway)

### Limitations:
- Time measurement is done in milliseconds

### Bugs:
- Probably plenty  
  This was my first project with Windows pipes and processes.  
  Yrying to port a long existing project was a challenge to say the least.  
  Most of the non i/o original code is left unchanged.
  
### Todo:
- Graceful quit win_caiaio when pressing CTRL-C  
  If win_caiaio is interrupted (CTRL-C) the program will quit immediately  
  Other programs (manager / referee / players) may be in suspended state and stay active forever  
  You have to end them manually in the Windows task manager
