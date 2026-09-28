# case n05f-seek-then-tick-overflow: a tick past the maximum chart time is refused, never wrapped
open
seek 9007199254740991
play
tick 1
quit
