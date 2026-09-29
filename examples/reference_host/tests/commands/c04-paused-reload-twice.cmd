# case c04-paused-reload-twice: two reloads while paused keep the trace and the target frame
open
play
tick 2
pause
reload
reload
tick 3
play
tick 1
quit
